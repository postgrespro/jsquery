#include "postgres.h"

#include "access/gin.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

#include "miscadmin.h"
#include "jsquery.h"

static ExtractedNode *recursiveExtract(char *jqBase, int32 jqPos,
		bool indirect, PathItem *path);
static int coundChildren(ExtractedNode *node, ExtractedNodeType type, bool first, bool *found);
static void fillChildren(ExtractedNode *node, ExtractedNodeType type, bool first, ExtractedNode **items, int *i);
static void flatternTree(ExtractedNode *node);
static int comparePathItems(PathItem *i1, PathItem *i2);
static ExtractedNode *makeEntries(ExtractedNode *node, MakeEntryHandler handler, Pointer extra);
static int compareNodes(const void *a1, const void *a2);
static void processGroup(ExtractedNode *node, int start, int end);
static void simplifyRecursive(ExtractedNode *node);
static int compareJsQueryValue(JsQueryValue *v1, JsQueryValue *v2);

static ExtractedNode *
recursiveExtract(char *jqBase, int32 jqPos,	bool indirect, PathItem *path)
{
	int32	type, childType;
	int32	nextPos;
	int32	left, right, arg;
	ExtractedNode *leftNode, *rightNode, *result;
	int32	len, *arrayPos, nelems, i;
	PathItem	*pathItem;

	check_stack_depth();

	jqPos = readJsQueryHeader(jqBase, jqPos, &type, &nextPos);

	switch(type) {
		case jqiAnd:
		case jqiOr:
			read_int32(left, jqBase, jqPos);
			read_int32(right, jqBase, jqPos);
			Assert(nextPos == 0);
			leftNode = recursiveExtract(jqBase, left, false, path);
			rightNode = recursiveExtract(jqBase, right, false, path);
			if (leftNode)
			{
				if (rightNode)
				{
					result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
					if (type == jqiAnd)
						result->type = eAnd;
					else if (type == jqiOr)
						result->type = eOr;
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
			read_int32(arg, jqBase, jqPos);
			Assert(nextPos == 0);
			leftNode = recursiveExtract(jqBase, arg, false, path);
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
			read_int32(len, jqBase, jqPos);
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iKey;
			pathItem->s = jqBase + jqPos;
			pathItem->len = len;
			pathItem->parent = path;
			return recursiveExtract(jqBase, nextPos, indirect, pathItem);
		case jqiAny:
			Assert(nextPos != 0);
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAny;
			pathItem->parent = path;
			return recursiveExtract(jqBase, nextPos, true, pathItem);
		case jqiAnyArray:
			Assert(nextPos != 0);
			pathItem = (PathItem *)palloc(sizeof(PathItem));
			pathItem->type = iAnyArray;
			pathItem->parent = path;
			return recursiveExtract(jqBase, nextPos, true, pathItem);
		case jqiCurrent:
			return recursiveExtract(jqBase, nextPos, indirect, path);
		case jqiEqual:
			read_int32(arg, jqBase, jqPos);
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = eScalar;
			result->path = path;
			result->indirect = indirect;
			arg = readJsQueryHeader(jqBase, arg, &childType, &nextPos);
			result->bounds.inequality = false;
			result->bounds.exact = (JsQueryValue *)palloc(sizeof(JsQueryValue));
			result->bounds.exact->jqBase = jqBase;
			result->bounds.exact->jqPos = arg;
			result->bounds.exact->type = childType;
			return result;
		case jqiIn:
		case jqiOverlap:
		case jqiContains:
			read_int32(arg, jqBase, jqPos);
			arg = readJsQueryHeader(jqBase, arg, &childType, &nextPos);

			read_int32(nelems, jqBase, arg);
			arrayPos = (int32*)(jqBase + arg);

			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			if (type == jqiContains)
				result->type = eAnd;
			else
				result->type = eOr;
			result->indirect = indirect;
			result->args.items = (ExtractedNode **)palloc(nelems * sizeof(ExtractedNode *));
			result->args.count = 0;
			result->path = path;
			if (type == jqiContains || type == jqiOverlap)
			{
				pathItem = (PathItem *)palloc(sizeof(PathItem));
				pathItem->type = iAnyArray;
				pathItem->parent = path;
			}
			else
			{
				pathItem = path;
			}
			for (i = 0; i < nelems; i++)
			{
				ExtractedNode *item;
				item = (ExtractedNode *)palloc(sizeof(ExtractedNode));
				item->indirect = false;
				item->type = eScalar;
				item->path = pathItem;
				arg = readJsQueryHeader(jqBase, arrayPos[i], &childType, &nextPos);
				item->bounds.inequality = false;
				item->bounds.exact = (JsQueryValue *)palloc(sizeof(JsQueryValue));
				item->bounds.exact->jqBase = jqBase;
				item->bounds.exact->jqPos = arg;
				item->bounds.exact->type = childType;
				result->args.items[result->args.count] = item;
				result->args.count++;
			}
			return result;
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
			read_int32(arg, jqBase, jqPos);
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			result->type = eScalar;
			result->indirect = indirect;
			result->path = path;
			result->bounds.inequality = true;
			arg = readJsQueryHeader(jqBase, arg, &childType, &nextPos);
			if (type == jqiGreater || type == jqiGreaterOrEqual)
			{
				if (type == jqiGreaterOrEqual)
					result->bounds.leftInclusive = true;
				else
					result->bounds.leftInclusive = false;
				result->bounds.rightBound = NULL;
				result->bounds.leftBound = (JsQueryValue *)palloc(sizeof(JsQueryValue));
				result->bounds.leftBound->jqBase = jqBase;
				result->bounds.leftBound->jqPos = arg;
				result->bounds.leftBound->type = childType;
			}
			else
			{
				if (type == jqiLessOrEqual)
					result->bounds.rightInclusive = true;
				else
					result->bounds.rightInclusive = false;
				result->bounds.leftBound = NULL;
				result->bounds.rightBound = (JsQueryValue *)palloc(sizeof(JsQueryValue));
				result->bounds.rightBound->jqBase = jqBase;
				result->bounds.rightBound->jqPos = arg;
				result->bounds.rightBound->type = childType;
			}
			return result;
		case jqiContained:
			return NULL;
		default:
			elog(ERROR,"Wrong state: %d", type);
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
compareJsQueryValue(JsQueryValue *v1, JsQueryValue *v2)
{
	char   *jqBase1, *jqBase2;
	int		jqPos1, jqPos2, len1, len2, cmp;

	if (v1->type != v2->type)
		return (v1->type < v2->type) ? -1 : 1;

	jqBase1 = v1->jqBase;
	jqPos1 = v1->jqPos;
	jqBase2 = v2->jqBase;
	jqPos2 = v2->jqPos;

	switch(v1->type)
	{
		case jqiNull:
			return 0;
		case jqiString:
			read_int32(len1, jqBase1, jqPos1);
			read_int32(len2, jqBase2, jqPos2);
			cmp = memcmp(jqBase1 + jqPos1, jqBase2 + jqPos2, Min(len1, len2));
			if (cmp != 0 || len1 == len2)
				return cmp;
			return (len1 < len2) ? -1 : 1;
		case jqiBool:
			read_byte(len1, jqBase1, jqPos1);
			read_byte(len2, jqBase2, jqPos2);
			if (len1 < len2)
				return -1;
			else if (len1 == len2)
				return 0;
			else
				return 1;
		case jqiNumeric:
			return DatumGetInt32(DirectFunctionCall2(numeric_cmp,
					 PointerGetDatum(jqBase1 + jqPos1),
					 PointerGetDatum(jqBase2 + jqPos2)));
		default:
			elog(ERROR,"Wrong state");
	}
	return 0;
}

static void
processGroup(ExtractedNode *node, int start, int end)
{
	int i;
	JsQueryValue *leftBound = NULL, *rightBound = NULL, *exact = NULL;
	bool leftInclusive = false, rightInclusive = false;
	ExtractedNode *child;

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
			cmp = compareJsQueryValue(child->bounds.leftBound, leftBound);
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
			cmp = compareJsQueryValue(child->bounds.rightBound, rightBound);
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
		int i, groupStart = -1;
		ExtractedNode *child, *prevChild = NULL;
		qsort(node->args.items, node->args.count,
				sizeof(ExtractedNode *), compareNodes);
		for (i = 0; i < node->args.count; i++)
		{
			child = node->args.items[i];
			if (child->indirect || child->type != eScalar)
				break;
			if (!prevChild || comparePathItems(child->path, prevChild->path) != 0)
			{
				if (groupStart > 0)
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

ExtractedNode *
extractJsQuery(JsQuery *jq, MakeEntryHandler handler, Pointer extra)
{
	ExtractedNode *root;

	root = recursiveExtract(VARDATA(jq), 0, false, NULL);
	flatternTree(root);
	simplifyRecursive(root);
	root = makeEntries(root, handler, extra);
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
