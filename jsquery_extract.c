#include "postgres.h"

#include "access/gin.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

#include "miscadmin.h"
#include "jsquery.h"

static ExtractedNode *recursiveExtract(JsQueryItem *jsq, bool indirect, PathItem *path);
static int coundChildren(ExtractedNode *node, ExtractedNodeType type, bool first, bool *found);
static void fillChildren(ExtractedNode *node, ExtractedNodeType type, bool first, ExtractedNode **items, int *i);
static void flatternTree(ExtractedNode *node);
static int comparePathItems(PathItem *i1, PathItem *i2);
static ExtractedNode *makeEntries(ExtractedNode *node, MakeEntryHandler handler, Pointer extra);
static int compareNodes(const void *a1, const void *a2);
static void processGroup(ExtractedNode *node, int start, int end);
static void simplifyRecursive(ExtractedNode *node);
static int compareJsQueryItem(JsQueryItem *v1, JsQueryItem *v2);

static ExtractedNode *
recursiveExtract(JsQueryItem *jsq,	bool indirect, PathItem *path)
{
	ExtractedNode 	*leftNode, *rightNode, *result;
	PathItem		*pathItem;
	JsQueryItem	elem, e;

	check_stack_depth();

	switch(jsq->type) {
		case jqiAnd:
		case jqiOr:
			jsqGetLeftArg(jsq, &elem);
			leftNode = recursiveExtract(&elem, false, path);
			jsqGetLeftArg(jsq, &elem);
			rightNode = recursiveExtract(&elem, false, path);
			if (leftNode)
			{
				if (rightNode)
				{
					result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
					result->type = (jsq->type == jqiAnd) ? eAnd : eOr;
					result->indirect = indirect;
					result->path = path;
					result->args.items = (ExtractedNode **)palloc(2 * sizeof(ExtractedNode *));
					result->args.items[0] = leftNode;
					result->args.items[1] = rightNode;
					result->args.count = 2;
					return result;
				}
				else
				{
					return leftNode;
				}
			}
			else
			{
				if (rightNode)
					return rightNode;
				else
					return NULL;
			}
		case jqiNot:
			jsqGetLeftArg(jsq, &elem);
			leftNode = recursiveExtract(&elem, false, path);
			if (leftNode)
			{
				result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
				result->type = eNot;
				result->path = path;
				result->indirect = indirect;
				result->args.items = (ExtractedNode **)palloc(sizeof(ExtractedNode *));
				result->args.items[0] = leftNode;
				result->args.count = 1;
				return result;
			}
			else
			{
				return NULL;
			}
		case jqiKey:
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iKey;
			pathItem->s = jsqGetString(jsq, &pathItem->len);
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, indirect, pathItem);
		case jqiAny:
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAny;
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, indirect, pathItem);
		case jqiAnyArray:
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAnyArray;
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, indirect, pathItem);
		case jqiAnyKey:
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAnyKey;
			pathItem->parent = path;
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, indirect, pathItem);
		case jqiCurrent:
			jsqGetNext(jsq, &elem);
			return recursiveExtract(&elem, indirect, path);
		case jqiEqual:
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = eScalar;
			result->path = path;
			result->indirect = indirect;
			result->bounds.inequality = false;
			result->bounds.exact = (JsQueryItem *)palloc(sizeof(JsQueryItem));
			jsqGetArg(jsq, result->bounds.exact);
			return result;
		case jqiIn:
		case jqiOverlap:
		case jqiContains:
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = (jsq->type == jqiContains) ? eAnd : eOr;
			jsqGetArg(jsq, &elem);
			Assert(elem.type == jqiArray);
			result->indirect = indirect;
			result->args.items = (ExtractedNode **)palloc(elem.array.nelems * sizeof(ExtractedNode *));
			result->args.count = 0;
			result->path = path;
			if (jsq->type == jqiContains || jsq->type == jqiOverlap)
			{
				pathItem = (PathItem *)palloc(sizeof(PathItem));
				pathItem->type = iAnyArray;
				pathItem->parent = path;
			}
			else
			{
				pathItem = path;
			}

			while(jsqIterateArray(&elem, &e))
			{
				ExtractedNode *item = (ExtractedNode *)palloc(sizeof(ExtractedNode));

				item->indirect = false;
				item->type = eScalar;
				item->path = pathItem;

				item->bounds.inequality = false;
				item->bounds.exact = (JsQueryItem *)palloc(sizeof(JsQueryItem));
				*item->bounds.exact = e;
				result->args.items[result->args.count] = item;
				result->args.count++;
			}
			return result;
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = eScalar;
			result->indirect = indirect;
			result->path = path;
			result->bounds.inequality = true;

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
		case jqiContained:
			return NULL;
		default:
			elog(ERROR,"Wrong state: %d", jsq->type);
	}
	return NULL;
}

static int
coundChildren(ExtractedNode *node, ExtractedNodeType type, bool first, bool *found)
{
	if ((node->indirect || node->type != type) && !first)
	{
		return 1;
	}
	else
	{
		int i, total = 0;
		*found = true;
		for (i = 0; i < node->args.count; i++)
			total += coundChildren(node->args.items[i], type, false, found);
		return total;
	}
}

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

static void
flatternTree(ExtractedNode *node)
{
	if (node->type == eAnd || node->type == eOr)
	{
		int count;
		bool found;

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
	if (node->type == eAnd || node->type == eOr || node->type == eNot)
	{
		int i;
		for (i = 0; i < node->args.count; i++)
			flatternTree(node->args.items[i]);
	}
}

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
	{
		return (n1->type < n2->type) ? -1 : 1;
	}

	if (n1->type == eScalar)
	{
		int cmp;
		cmp = comparePathItems(n1->path, n2->path);
		if (cmp) return cmp;
		if (n1->entryNum < n2->entryNum)
			return -1;
		else if (n1->entryNum == n2->entryNum)
			return 0;
		else
			return 1;
	}
	else
	{
		return 0;
	}
}

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
			elog(ERROR,"Wrong state");
	}

	return 0; /* make compiler happy */
}

static void
processGroup(ExtractedNode *node, int start, int end)
{
	int 			i;
	JsQueryItem 	*leftBound = NULL, *rightBound = NULL, *exact = NULL;
	bool 			leftInclusive = false, rightInclusive = false;
	ExtractedNode 	*child;

	if (end - start < 2)
		return;

	for (i = start; i < end; i++)
	{
		int cmp;

		child = node->args.items[i];

		if (!child->bounds.inequality)
		{
			if (child->bounds.exact->type == jqiAny)
			{
				continue;
			}
			else
			{
				exact = child->bounds.exact;
				break;
			}
		}

		if (child->bounds.leftBound)
		{
			if (!leftBound)
			{
				leftBound = child->bounds.leftBound;
				leftInclusive = child->bounds.leftInclusive;
			}
			cmp = compareJsQueryItem(child->bounds.leftBound, leftBound);
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
			cmp = compareJsQueryItem(child->bounds.rightBound, rightBound);
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
	}

	child = node->args.items[start];
	if (exact)
	{
		child->bounds.inequality = false;
		child->bounds.exact = exact;
	}
	else
	{
		child->bounds.inequality = true;
		child->bounds.leftBound = leftBound;
		child->bounds.leftInclusive = leftInclusive;
		child->bounds.rightBound = rightBound;
		child->bounds.rightInclusive = rightInclusive;
	}

	for (i = start + 1; i < end; i++)
		node->args.items[i] = NULL;
}

static void
simplifyRecursive(ExtractedNode *node)
{
	if (node->type == eAnd)
	{
		int 			i, groupStart = -1;
		ExtractedNode 	*child, *prevChild = NULL;

		qsort(node->args.items, node->args.count,
			  sizeof(ExtractedNode *), compareNodes);

		for (i = 0; i < node->args.count; i++)
		{
			child = node->args.items[i];
			if (child->indirect || child->type != eScalar)
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

	if (node->type == eAnd || node->type == eOr || node->type == eNot)
	{
		int i;
		for (i = 0; i < node->args.count; i++)
		{
			if (node->args.items[i])
				simplifyRecursive(node->args.items[i]);
		}
	}
}

static ExtractedNode *
makeEntries(ExtractedNode *node, MakeEntryHandler handler, Pointer extra)
{
	if (node->type == eAnd || node->type == eOr || node->type == eNot)
	{
		int i, j = 0;
		ExtractedNode *child;
		for (i = 0; i < node->args.count; i++)
		{
			child = node->args.items[i];
			if (child)
				child = makeEntries(child, handler, extra);
			if (child)
			{
				node->args.items[j] = child;
				j++;
			}
		}
		if (j == 1 && (node->type == eAnd || node->type == eOr))
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

static bool
queryHasPositive(ExtractedNode *node)
{
	int i;
	bool result;
	switch(node->type)
	{
		case eAnd:
			result = false;
			for (i = 0; i < node->args.count; i++)
			{
				if (queryHasPositive(node->args.items[i]))
				{
					result = true;
					break;
				}
			}
			return result;
		case eOr:
			result = true;
			for (i = 0; i < node->args.count; i++)
			{
				if (!queryHasPositive(node->args.items[i]))
				{
					result = false;
					break;
				}
			}
			return result;
		case eNot:
			return !queryHasPositive(node->args.items[0]);
		case eScalar:
			return true;
	}
}

static bool
needRecheckRecursive(ExtractedNode *node, bool not)
{
	int i;
	switch(node->type)
	{
		case eAnd:
		case eOr:
			if (node->type == eAnd && !not && node->indirect)
				return true;
			if (node->type == eOr && not && node->indirect)
				return true;
			for (i = 0; i < node->args.count; i++)
			{
				if (needRecheckRecursive(node->args.items[i], not))
					return true;
			}
			return false;
		case eNot:
			return !needRecheckRecursive(node->args.items[0], !not);
		case eScalar:
			return false;
	}
}

ExtractedNode *
extractJsQuery(JsQuery *jq, MakeEntryHandler handler, Pointer extra)
{
	ExtractedNode 	*root;
	JsQueryItem		jsq;

	jsqInit(&jsq, jq);
	root = recursiveExtract(&jsq, false, NULL);
	if (root)
	{
		flatternTree(root);
		simplifyRecursive(root);
		root = makeEntries(root, handler, extra);
	}
	if (root && !queryHasPositive(root))
		root = NULL;
	if (root)
		root->indirect = needRecheckRecursive(root, false);
	return root;
}

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
		case eNot:
			return !execRecursive(node->args.items[0], check);
		case eScalar:
			return check[node->entryNum];
	}
}

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
		case eNot:
			v = execRecursive(node->args.items[0], check);
			if (v == GIN_TRUE)
				return GIN_FALSE;
			else if (v == GIN_FALSE)
				return GIN_TRUE;
			else
				return GIN_MAYBE;
		case eScalar:
			return check[node->entryNum];
	}
}
