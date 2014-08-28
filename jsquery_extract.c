/*-------------------------------------------------------------------------
 *
 * jsquery_extract.c
 *     Functions and operations to support jsquery in indexes  
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Alexander Korotkov <aekorotkov@gmail.com>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_extract.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

#include "miscadmin.h"
#include "jsquery.h"

static ExtractedNode *recursiveExtract(JsQueryItem *jsq, bool not, bool indirect, PathItem *path);
static int coundChildren(ExtractedNode *node, ExtractedNodeType type, bool first, bool *found);
static void fillChildren(ExtractedNode *node, ExtractedNodeType type, bool first, ExtractedNode **items, int *i);
static void flatternTree(ExtractedNode *node);
static int comparePathItems(PathItem *i1, PathItem *i2);
static int compareNodes(const void *a1, const void *a2);
static int compareJsQueryItem(JsQueryItem *v1, JsQueryItem *v2);
static void processGroup(ExtractedNode *node, int start, int end);
static void simplifyRecursive(ExtractedNode *node);
static SelectivityClass getScalarSelectivityClass(ExtractedNode *node);
static ExtractedNode *makeEntries(ExtractedNode *node, MakeEntryHandler handler, Pointer extra);
static void setSelectivityClass(ExtractedNode *node, CheckEntryHandler checkHandler, Pointer extra);
static void debugPath(StringInfo buf, PathItem *path);
static void debugValue(StringInfo buf, JsQueryItem *v);
static void debugRecursive(StringInfo buf, ExtractedNode *node, int shift);

/*
 * Recursive function that turns jsquery into tree of ExtractedNode items.
 */
static ExtractedNode *
recursiveExtract(JsQueryItem *jsq, bool not, bool indirect, PathItem *path)
{
	ExtractedNode 	*leftNode, *rightNode, *result;
	PathItem		*pathItem;
	ExtractedNodeType type;
	JsQueryItem	elem, e;

	check_stack_depth();

	switch(jsq->type) {
		case jqiAnd:
		case jqiOr:
			type = ((jsq->type == jqiAnd) == not) ? eOr : eAnd;

			jsqGetLeftArg(jsq, &elem);
			leftNode = recursiveExtract(&elem, not, false, path);
			jsqGetRightArg(jsq, &elem);
			rightNode = recursiveExtract(&elem, not, false, path);

			if (!leftNode || !rightNode)
			{
				if (type == eOr || (!leftNode && !rightNode))
					return NULL;
				if (leftNode)
				{
					leftNode->indirect = leftNode->indirect || indirect;
					return leftNode;
				}
				else
				{
					rightNode->indirect = rightNode->indirect || indirect;
					return rightNode;
				}
			}

			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = type;
			result->path = path;
			result->indirect = indirect;
			result->args.items = (ExtractedNode **)palloc(2 * sizeof(ExtractedNode *));
			result->args.items[0] = leftNode;
			result->args.items[1] = rightNode;
			result->args.count = 2;
			return result;
		case jqiNot:
			jsqGetArg(jsq, &elem);
			return recursiveExtract(&elem, !not, indirect, path);
		case jqiKey:
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iKey;
			pathItem->s = jsqGetString(jsq, &pathItem->len);
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, not, indirect, pathItem);
		case jqiAny:
			if (not) return NULL;
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAny;
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, not, true, pathItem);
		case jqiAnyArray:
			if (not) return NULL;
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAnyArray;
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, not, true, pathItem);
		case jqiAnyKey:
			if (not) return NULL;
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAnyKey;
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, not, true, pathItem);
		case jqiCurrent:
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, not, indirect, path);
		case jqiEqual:
			if (not) return NULL;
			jsqGetArg(jsq, &e);
			if (e.type == jqiAny)
			{
				result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
				result->type = eAny;
				result->hint = jsq->hint;
				result->path = path;
				result->indirect = indirect;
				return result;
			}
			else if (e.type != jqiArray)
			{
				result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
				result->type = eExactValue;
				result->hint = jsq->hint;
				result->path = path;
				result->indirect = indirect;
				result->exactValue = (JsQueryItem *)palloc(sizeof(JsQueryItem));
				*result->exactValue = e;
				return result;
			}
			/* jqiEqual with jqiArray follows */
		case jqiIn:
		case jqiOverlap:
		case jqiContains:
		case jqiContained:
			if (not) return NULL;
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = (jsq->type == jqiContains || jsq->type == jqiEqual) ? eAnd : eOr;
			jsqGetArg(jsq, &elem);
			Assert(elem.type == jqiArray);
			result->path = path;
			result->indirect = indirect;
			result->args.items = (ExtractedNode **)palloc((elem.array.nelems + 1) * sizeof(ExtractedNode *));
			result->args.count = 0;
			if (jsq->type == jqiContains || jsq->type == jqiOverlap ||
				jsq->type == jqiContained || jsq->type == jqiEqual)
			{
				pathItem = (PathItem *)palloc(sizeof(PathItem));
				pathItem->type = iAnyArray;
				pathItem->parent = path;
			}
			else
			{
				pathItem = path;
			}

			if (jsq->type == jqiContained ||
					(jsq->type == jqiEqual && elem.array.nelems == 0))
			{
				ExtractedNode *item = (ExtractedNode *)palloc(sizeof(ExtractedNode));

				item->hint = e.hint;
				item->type = eEmptyArray;
				item->path = pathItem->parent;
				item->indirect = false;
				item->hint = jsq->hint;

				result->args.items[result->args.count] = item;
				result->args.count++;
			}

			while(jsqIterateArray(&elem, &e))
			{
				ExtractedNode *item = (ExtractedNode *)palloc(sizeof(ExtractedNode));

				item->hint = e.hint;
				item->type = eExactValue;
				item->path = pathItem;
				item->indirect = true;
				item->hint = jsq->hint;

				item->exactValue = (JsQueryItem *)palloc(sizeof(JsQueryItem));
				*item->exactValue = e;
				result->args.items[result->args.count] = item;
				result->args.count++;
			}
			return result;
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
			if (not) return NULL;
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = eInequality;
			result->hint = jsq->hint;
			result->path = path;
			result->indirect = indirect;

			if (jsq->type == jqiGreater || jsq->type == jqiGreaterOrEqual)
			{
				result->bounds.leftInclusive = (jsq->type == jqiGreaterOrEqual);
				result->bounds.rightBound = NULL;
				result->bounds.leftBound = (JsQueryItem *)palloc(sizeof(JsQueryItem));
				jsqGetArg(jsq, result->bounds.leftBound);
			}
			else
			{
				result->bounds.rightInclusive = (jsq->type == jqiLessOrEqual);
				result->bounds.leftBound = NULL;
				result->bounds.rightBound = (JsQueryItem *)palloc(sizeof(JsQueryItem));
				jsqGetArg(jsq, result->bounds.rightBound);
			}
			return result;
		case jqiIs:
			if (not) return NULL;
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = eIs;
			result->hint = jsq->hint;
			result->path = path;
			result->indirect = indirect;
			result->isType = jsqGetIsType(jsq);
			return result;
		default:
			elog(ERROR,"Wrong state: %d", jsq->type);
	}
	return NULL;
}

/*
 * Count number of children connected with nodes of same type.
 */
static int
coundChildren(ExtractedNode *node, ExtractedNodeType type,
													bool first, bool *found)
{
	if ((node->indirect || node->type != type) && !first)
	{
		return 1;
	}
	else
	{
		int i, total = 0;
		if (!first)
			*found = true;
		for (i = 0; i < node->args.count; i++)
			total += coundChildren(node->args.items[i], type, false, found);
		return total;
	}
}

/*
 * Fill array of children connected with nodes of same type.
 */
static void
fillChildren(ExtractedNode *node, ExtractedNodeType type, bool first,
												ExtractedNode **items, int *i)
{
	if ((node->indirect || node->type != type) && !first)
	{
		items[*i] = node;
		(*i)++;
	}
	else
	{
		int j;
		for (j = 0; j < node->args.count; j++)
			fillChildren(node->args.items[j], type, false, items, i);
	}
}

/*
 * Turn tree into "flat" form, turning nested binary AND/OR operators into
 * single n-ary AND/OR operators.
 */
static void
flatternTree(ExtractedNode *node)
{
	if (node->type == eAnd || node->type == eOr)
	{
		int count;
		bool found = false;

		count = coundChildren(node, node->type, true, &found);

		if (found)
		{
			int i = 0;
			ExtractedNode **items = (ExtractedNode **)palloc(count * sizeof(ExtractedNode *));
			fillChildren(node, node->type, true, items, &i);
			node->args.items = items;
			node->args.count = count;
		}
	}
	if (node->type == eAnd || node->type == eOr)
	{
		int i;
		for (i = 0; i < node->args.count; i++)
			flatternTree(node->args.items[i]);
	}
}

/*
 * Compare path items chains from child to parent.
 */
static int
comparePathItems(PathItem *i1, PathItem *i2)
{
	int cmp;

	if (i1 == i2)
		return 0;

	if (!i1)
		return -1;
	if (!i2)
		return 1;

	if (i1->type != i2->type)
	{
		return (i1->type < i2->type) ? -1 : 1;
	}

	if (i1->type == iKey)
		cmp = memcmp(i1->s, i2->s, Min(i1->len, i2->len));
	else
		cmp = 0;

	if (cmp == 0)
	{
		if (i1->len != i2->len)
			return (i1->len < i2->len) ? -1 : 1;
		return comparePathItems(i1->parent, i2->parent);
	}
	else
	{
		return cmp;
	}
}

bool
isLogicalNodeType(ExtractedNodeType type)
{
	if (type == eAnd || type == eOr)
		return true;
	else
		return false;
}

/*
 * Compare nodes in the order where conditions to the same fields are located
 * together.
 */
static int
compareNodes(const void *a1, const void *a2)
{
	ExtractedNode *n1 = *((ExtractedNode **)a1);
	ExtractedNode *n2 = *((ExtractedNode **)a2);

	if (n1->indirect != n2->indirect)
	{
		if (n1->indirect)
			return 1;
		if (n2->indirect)
			return -1;
	}

	if (n1->type != n2->type)
		return (n1->type < n2->type) ? -1 : 1;

	if (!isLogicalNodeType(n1->type))
	{
		int cmp = comparePathItems(n1->path, n2->path);
		if (cmp) return cmp;
	}

	if (n1->number != n2->number)
		return (n1->number < n2->number) ? -1 : 1;

	return 0;
}

/*
 * Compare json values represented by JsQueryItems.
 */
static int
compareJsQueryItem(JsQueryItem *v1, JsQueryItem *v2)
{
	char	*s1, *s2;
	int32	len1, len2, cmp;

	if (v1->type != v2->type)
		return (v1->type < v2->type) ? -1 : 1;

	switch(v1->type)
	{
		case jqiNull:
			return 0;
		case jqiString:
			s1 = jsqGetString(v1, &len1);
			s2 = jsqGetString(v2, &len2);
			cmp = memcmp(s1, s2, Min(len1, len2));
			if (cmp != 0 || len1 == len2)
				return cmp;
			return (len1 < len2) ? -1 : 1;
		case jqiBool:
			len1 = jsqGetBool(v1);
			len2 = jsqGetBool(v2);

			return (len1 - len2);
		case jqiNumeric:
			return DatumGetInt32(DirectFunctionCall2(numeric_cmp,
					 PointerGetDatum(jsqGetNumeric(v1)),
					 PointerGetDatum(jsqGetNumeric(v2))));
		default:
			elog(ERROR, "Wrong state");
	}

	return 0; /* make compiler happy */
}

/*
 * Process group of nodes representing conditions for the same field. After
 * processing group of nodes is replaced with one node.
 */
static void
processGroup(ExtractedNode *node, int start, int end)
{
	int 				i;
	JsQueryItem		   *leftBound = NULL,
					   *rightBound = NULL,
					   *exactValue = NULL;
	bool 				leftInclusive = false,
						rightInclusive = false,
						first = true;
	ExtractedNode	   *child;
	ExtractedNodeType	type = eAny;
	JsQueryItemType		isType = jbvNull;

	if (end - start < 2)
		return;

	for (i = start; i < end; i++)
	{
		int cmp;

		child = node->args.items[i];

		if (first || child->type <= type)
		{
			type = child->type;
			first = false;
			Assert(!isLogicalNodeType(type));
			switch(type)
			{
				case eAny:
				case eEmptyArray:
					break;
				case eIs:
					isType = child->isType;
					break;
				case eInequality:
					if (child->bounds.leftBound)
					{
						if (!leftBound)
						{
							leftBound = child->bounds.leftBound;
							leftInclusive = child->bounds.leftInclusive;
						}
						cmp = compareJsQueryItem(child->bounds.leftBound,
																	leftBound);
						if (cmp > 0)
						{
							leftBound = child->bounds.leftBound;
							leftInclusive = child->bounds.leftInclusive;
						}
						if (cmp == 0 && leftInclusive)
						{
							leftInclusive = child->bounds.leftInclusive;
						}
					}
					if (child->bounds.rightBound)
					{
						if (!rightBound)
						{
							rightBound = child->bounds.rightBound;
							rightInclusive = child->bounds.rightInclusive;
						}
						cmp = compareJsQueryItem(child->bounds.rightBound,
																	rightBound);
						if (cmp > 0)
						{
							rightBound = child->bounds.rightBound;
							rightInclusive = child->bounds.rightInclusive;
						}
						if (cmp == 0 && rightInclusive)
						{
							rightInclusive = child->bounds.rightInclusive;
						}
					}
					break;
				case eExactValue:
					exactValue = child->exactValue;
					break;
				default:
					elog(ERROR, "Wrong state");
					break;

			}
		}
	}

	child = node->args.items[start];
	child->type = type;

	switch(type)
	{
		case eAny:
		case eEmptyArray:
			break;
		case eIs:
			child->isType = isType;
			break;
		case eInequality:
			child->bounds.leftBound = leftBound;
			child->bounds.rightBound = rightBound;
			child->bounds.leftInclusive = leftInclusive;
			child->bounds.rightInclusive = rightInclusive;
			break;
		case eExactValue:
			child->exactValue = exactValue;
			break;
		default:
			elog(ERROR, "Wrong state");
			break;
	}

	for (i = start + 1; i < end; i++)
		node->args.items[i] = NULL;
}

/*
 * Reduce number of nodes in tree, by turning multiple conditions about
 * same field in same context into one node.
 */
static void
simplifyRecursive(ExtractedNode *node)
{
	if (node->type == eAnd)
	{
		int 			i, groupStart = -1;
		ExtractedNode 	*child, *prevChild = NULL;

		for (i = 0; i < node->args.count; i++)
			node->args.items[i]->number = i;

		pg_qsort(node->args.items, node->args.count,
				 sizeof(ExtractedNode *), compareNodes);

		for (i = 0; i < node->args.count; i++)
		{
			child = node->args.items[i];
			if (child->indirect || isLogicalNodeType(child->type))
				break;
			if (!prevChild || comparePathItems(child->path, prevChild->path) != 0)
			{
				if (groupStart >= 0)
					processGroup(node, groupStart, i);
				groupStart = i;
			}
			prevChild = child;
		}
		if (groupStart >= 0)
			processGroup(node, groupStart, i);
	}

	if (node->type == eAnd || node->type == eOr)
	{
		int i;
		for (i = 0; i < node->args.count; i++)
		{
			if (node->args.items[i])
				simplifyRecursive(node->args.items[i]);
		}
	}
}

/*
 * Get selectivity class of scalar node.
 */
static SelectivityClass
getScalarSelectivityClass(ExtractedNode *node)
{
	Assert(!isLogicalNodeType(node->type));
	switch(node->type)
	{
		case eAny:
			return sAny;
		case eIs:
			return sIs;
		case eInequality:
			if (node->bounds.leftBound && node->bounds.rightBound)
				return sRange;
			else
				return sInequal;
		case eEmptyArray:
		case eExactValue:
			return sEqual;
		default:
			elog(ERROR, "Wrong state");
			return sAny;
	}
}

/*
 * Make entries for all leaf tree nodes using user-provided handler.
 */
static ExtractedNode *
makeEntries(ExtractedNode *node, MakeEntryHandler handler, Pointer extra)
{
	if (node->type == eAnd || node->type == eOr)
	{
		int i, j = 0;
		ExtractedNode *child;
		for (i = 0; i < node->args.count; i++)
		{
			child = node->args.items[i];
			if (!child) continue;
			if (child->sClass > node->sClass && !child->forceIndex)
			{
				Assert(node->type != eOr);
				continue;
			}
			child = makeEntries(child, handler, extra);
			if (child)
			{
				node->args.items[j] = child;
				j++;
			}
			else if (node->type == eOr)
			{
				return NULL;
			}
		}
		if (j == 1)
		{
			return node->args.items[0];
		}
		if (j > 0)
		{
			node->args.count = j;
			return node;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		int entryNum;

		if (node->hint == jsqNoIndex)
			return NULL;

		entryNum = handler(node, extra);
		if (entryNum >= 0)
		{
			node->entryNum = entryNum;
			return node;
		}
		else
		{
			return NULL;
		}
	}
}

static void
setSelectivityClass(ExtractedNode *node, CheckEntryHandler checkHandler,
																Pointer extra)
{
	int					i;
	bool				first;
	ExtractedNode	   *child;

	switch(node->type)
	{
		case eAnd:
		case eOr:
			first = true;
			node->forceIndex = false;
			for (i = 0; i < node->args.count; i++)
			{
				child = node->args.items[i];

				if (!child)
					continue;

				if (!isLogicalNodeType(child->type))
				{
					if (child->hint == jsqNoIndex ||
							!checkHandler(child, extra))
						continue;
				}

				setSelectivityClass(child, checkHandler, extra);

				if (child->forceIndex)
					node->forceIndex = true;

				if (first)
				{
					node->sClass = child->sClass;
				}
				else
				{
					if (node->type == eAnd)
						node->sClass = Min(node->sClass, child->sClass);
					else
						node->sClass = Max(node->sClass, child->sClass);
				}
				first = false;
			}
			break;
		default:
			node->sClass = getScalarSelectivityClass(node);
			node->forceIndex = node->hint == jsqForceIndex;
			break;
	}
}

/*
 * Turn jsquery into tree of entries using user-provided handler.
 */
ExtractedNode *
extractJsQuery(JsQuery *jq, MakeEntryHandler makeHandler,
								CheckEntryHandler checkHandler, Pointer extra)
{
	ExtractedNode 	*root;
	JsQueryItem		jsq;

	jsqInit(&jsq, jq);
	root = recursiveExtract(&jsq, false, false, NULL);
	if (root)
	{
		flatternTree(root);
		simplifyRecursive(root);
		setSelectivityClass(root, checkHandler, extra);
		root = makeEntries(root, makeHandler, extra);
	}
	return root;
}

/*
 * Evaluate previously extracted tree.
 */
bool
execRecursive(ExtractedNode *node, bool *check)
{
	int i;
	switch(node->type)
	{
		case eAnd:
			for (i = 0; i < node->args.count; i++)
				if (!execRecursive(node->args.items[i], check))
					return false;
			return true;
		case eOr:
			for (i = 0; i < node->args.count; i++)
				if (execRecursive(node->args.items[i], check))
					return true;
			return false;
		default:
			return check[node->entryNum];
	}
}

/*
 * Evaluate previously extracted tree using tri-state logic.
 */
bool
execRecursiveTristate(ExtractedNode *node, GinTernaryValue *check)
{
	GinTernaryValue	res, v;
	int i;

	switch(node->type)
	{
		case eAnd:
			res = GIN_TRUE;
			for (i = 0; i < node->args.count; i++)
			{
				v = execRecursive(node->args.items[i], check);
				if (v == GIN_FALSE)
					return GIN_FALSE;
				else if (v == GIN_MAYBE)
					res = GIN_MAYBE;
			}
			return res;
		case eOr:
			res = GIN_FALSE;
			for (i = 0; i < node->args.count; i++)
			{
				v = execRecursive(node->args.items[i], check);
				if (v == GIN_TRUE)
					return GIN_TRUE;
				else if (v == GIN_MAYBE)
					res = GIN_MAYBE;
			}
			return res;
		default:
			return check[node->entryNum];
	}
}

/*
 * Debug print of variable path.
 */
static void
debugPath(StringInfo buf, PathItem *path)
{
	if (path->parent)
	{
		debugPath(buf, path->parent);
		appendStringInfo(buf, ".");
	}
	switch (path->type)
	{
		case jqiAny:
			appendStringInfoChar(buf, '*');
			break;
		case jqiAnyKey:
			appendStringInfoChar(buf, '%');
			break;
		case jqiAnyArray:
			appendStringInfoChar(buf, '#');
			break;
		case jqiKey:
			appendBinaryStringInfo(buf, path->s, path->len);
			break;
	}
}

/*
 * Debug print of jsquery value.
 */
static void
debugValue(StringInfo buf, JsQueryItem *v)
{
	char   *s;
	int		len;

	switch(v->type)
	{
		case jqiNull:
			appendStringInfo(buf, "null");
			break;
		case jqiString:
			s = jsqGetString(v, &len);
			appendStringInfo(buf, "\"");
			appendBinaryStringInfo(buf, s, len);
			appendStringInfo(buf, "\"");
			break;
		case jqiBool:
			appendStringInfo(buf, jsqGetBool(v) ? "true" : "false");
			break;
		case jqiNumeric:
			s = DatumGetCString(DirectFunctionCall1(numeric_out,
					 PointerGetDatum(jsqGetNumeric(v))));
			appendStringInfoString(buf, s);
			break;
		default:
			elog(ERROR,"Wrong type");
			break;
	}
}

static const char *
getTypeString(int32 type)
{
	switch (type)
	{
		case jbvArray:
			return "array";
		case jbvObject:
			return "object";
		case jbvString:
			return "string";
		case jbvNumeric:
			return "numeric";
		case jbvBool:
			return "boolean";
		case jbvNull:
			return "null";
		default:
			elog(ERROR,"Wrong type");
			return NULL;
	}
}


/*
 * Recursive worker of debug print of query processing.
 */
static void
debugRecursive(StringInfo buf, ExtractedNode *node, int shift)
{
	int i;

	appendStringInfoSpaces(buf, shift * 2);

	if (isLogicalNodeType(node->type))
	{
		appendStringInfo(buf, (node->type == eAnd) ? "AND\n" : "OR\n");
		for (i = 0; i < node->args.count; i++)
			debugRecursive(buf, node->args.items[i], shift + 1);
		return;
	}

	debugPath(buf, node->path);
	switch(node->type)
	{
		case eExactValue:
			appendStringInfo(buf, " = ");
			debugValue(buf, node->exactValue);
			appendStringInfo(buf, " ,");
			break;
		case eAny:
			appendStringInfo(buf, " = * ,");
			break;
		case eEmptyArray:
			appendStringInfo(buf, " = [] ,");
			break;
		case eIs:
			appendStringInfo(buf, " IS %s ,", getTypeString(node->isType));
			break;
		case eInequality:
			if (node->bounds.leftBound)
			{
				if (node->bounds.leftInclusive)
					appendStringInfo(buf, " >= ");
				else
					appendStringInfo(buf, " > ");
				debugValue(buf, node->bounds.leftBound);
				appendStringInfo(buf, " ,");
			}
			if (node->bounds.rightBound)
			{
				if (node->bounds.rightInclusive)
					appendStringInfo(buf, " <= ");
				else
					appendStringInfo(buf, " < ");
				debugValue(buf, node->bounds.rightBound);
				appendStringInfo(buf, " ,");
			}
			break;
		default:
			elog(ERROR,"Wrong type");
			break;
	}
	appendStringInfo(buf, " entry %d \n", node->entryNum);
}

/*
 * Debug print of query processing.
 */
char *
debugJsQuery(JsQuery *jq, MakeEntryHandler makeHandler,
								CheckEntryHandler checkHandler, Pointer extra)
{
	ExtractedNode  *root;
	StringInfoData	buf;

	root = extractJsQuery(jq, makeHandler, checkHandler, extra);
	if (!root)
		return "NULL\n";

	initStringInfo(&buf);
	debugRecursive(&buf, root, 0);
	appendStringInfoChar(&buf, '\0');
	return buf.data;
}
