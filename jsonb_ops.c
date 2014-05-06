#include "postgres.h"

#include "access/hash.h"
#include "access/gin.h"
#include "access/skey.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

#include "jsquery.h"

typedef struct PathHashStack
{
	uint32	hash;
	struct PathHashStack *parent;
}	PathHashStack;

typedef struct
{
	int32		vl_len_;
	uint32		hash;
	uint8		type;
	char		data[1];
} GINKey;

#define GINKEYLEN offsetof(GINKey, data)

#define GINKeyLenString(len) (offsetof(GINKey, data) + len)
#define GINKeyLenNumeric(len) (INTALIGN(offsetof(GINKey, data)) + len)
#define GINKeyStringLen(key) (VARSIZE(key) - offsetof(GINKey, data))
#define GINKeyDataString(key) ((key)->data)
#define GINKeyDataNumeric(key) ((Pointer)key + INTALIGN(offsetof(GINKey, data)))
#define GINKeyType(key) ((key)->type & 0x7F)
#define GINKeyIsTrue(key) ((key)->type & 0x80)
#define	GINKeyTrue 0x80

#define BLOOM_BITS 2
#define JsonbNestedContainsStrategyNumber	13
#define JsQueryMatchStrategyNumber			14

typedef enum
{
	eAnd,
	eOr,
	eNot,
	eScalar
} ExtractedNodeType;

typedef struct ExtractedNode ExtractedNode;

struct ExtractedNode
{
	ExtractedNodeType	type;
	union
	{
		struct
		{
			ExtractedNode **items;
			int				count;
		} args;
		int	entryNum;
	};
};

typedef struct
{
	Datum *entries;
	Pointer *extra_data;
	bool	*partial_match;
	int count, total;
} Entries;

typedef struct
{
	ExtractedNode  *root;
	uint32			hash;
} KeyExtra;

static uint32 get_bloom_value(uint32 hash);
static uint32 get_path_bloom(PathHashStack *stack);
static GINKey *make_gin_key(JsonbValue *v, uint32 hash);
static GINKey *make_gin_query_key(char *jqBase, int32 jqPos, int32 type, uint32 hash);
static int32 compare_gin_key_value(GINKey *arg1, GINKey *arg2);
static int addEntry(Entries *e, Datum key, Pointer extra, bool pmatch);
static ExtractedNode *recursiveExtract(char *jqBase, int32 jqPos, uint32 hash, Entries *e, bool lossy);

PG_FUNCTION_INFO_V1(gin_compare_jsonb_bloom_value);
PG_FUNCTION_INFO_V1(gin_compare_partial_jsonb_bloom_value);
PG_FUNCTION_INFO_V1(gin_extract_jsonb_bloom_value);
PG_FUNCTION_INFO_V1(gin_extract_jsonb_query_bloom_value);
PG_FUNCTION_INFO_V1(gin_consistent_jsonb_bloom_value);
PG_FUNCTION_INFO_V1(gin_triconsistent_jsonb_bloom_value);

Datum gin_compare_jsonb_bloom_value(PG_FUNCTION_ARGS);
Datum gin_compare_partial_jsonb_bloom_value(PG_FUNCTION_ARGS);
Datum gin_extract_jsonb_bloom_value(PG_FUNCTION_ARGS);
Datum gin_extract_jsonb_query_bloom_value(PG_FUNCTION_ARGS);
Datum gin_consistent_jsonb_bloom_value(PG_FUNCTION_ARGS);
Datum gin_triconsistent_jsonb_bloom_value(PG_FUNCTION_ARGS);

static int
addEntry(Entries *e, Datum key, Pointer extra, bool pmatch)
{
	int entryNum;
	if (!e->entries)
	{
		e->total = 16;
		e->entries = (Datum *)palloc(e->total * sizeof(Datum));
		e->extra_data = (Pointer *)palloc(e->total * sizeof(Pointer));
		e->partial_match = (bool *)palloc(e->total * sizeof(bool));
	}
	if (e->count + 1 > e->total)
	{
		e->total *= 2;
		e->entries = (Datum *)repalloc(e->entries, e->total * sizeof(Datum));
		e->extra_data = (Pointer *)repalloc(e->entries, e->total * sizeof(Pointer));
		e->partial_match = (bool *)repalloc(e->entries, e->total * sizeof(bool));
	}
	entryNum = e->count;
	e->count++;
	e->entries[entryNum] = key;
	e->extra_data[entryNum] = extra;
	e->partial_match[entryNum] = pmatch;
	return entryNum;
}

static ExtractedNode *
recursiveExtract(char *jqBase, int32 jqPos, uint32 hash, Entries *e, bool lossy)
{
	int32	type;
	int32	nextPos;
	int32	left, right, arg;
	ExtractedNode *leftNode, *rightNode, *result;
	int32	len;
	KeyExtra	*extra;

	check_stack_depth();

	jqPos = readJsQueryHeader(jqBase, jqPos, &type, &nextPos);

	switch(type) {
		case jqiAnd:
		case jqiOr:
			read_int32(left, jqBase, jqPos);
			read_int32(right, jqBase, jqPos);
			Assert(nextPos == 0);
			leftNode = recursiveExtract(jqBase, left, hash, e, lossy);
			rightNode = recursiveExtract(jqBase, right, hash, e, lossy);
			if (leftNode)
			{
				if (rightNode)
				{
					result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
					if (type == jqiAnd)
						result->type = eAnd;
					else if (type == jqiOr)
						result->type = eOr;
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
			leftNode = recursiveExtract(jqBase, arg, hash, e, lossy);
			if (leftNode)
			{
				result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
				result->type = eNot;
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
			return recursiveExtract(jqBase, nextPos,
				hash | get_bloom_value(hash_any((unsigned char *)jqBase + jqPos, len)), e, lossy);
		case jqiAny:
			Assert(nextPos != 0);
			return recursiveExtract(jqBase, nextPos, hash, e, true);
		case jqiAnyArray:
			Assert(nextPos != 0);
			return recursiveExtract(jqBase, nextPos, hash, e, lossy);
		case jqiEqual:
			read_int32(arg, jqBase, jqPos);
			result = (ExtractedNode *)palloc(sizeof(ExtractedNode));
			extra = (KeyExtra *)palloc(sizeof(KeyExtra));
			extra->hash = hash;
			result->type = eScalar;
			arg = readJsQueryHeader(jqBase, arg, &type, &nextPos);
			result->entryNum = addEntry(e,
					PointerGetDatum(make_gin_query_key(jqBase, arg, type, lossy ? 0 : hash)),
					(Pointer)extra, lossy);
			return result;
		case jqiIn:
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
		case jqiContains:
		case jqiContained:
		case jqiOverlap:
			return NULL;
		default:
			elog(ERROR,"Wrong state: %d", type);
	}

	return NULL;
}

static uint32
get_bloom_value(uint32 hash)
{
	int i, j, vals[BLOOM_BITS], val, tmp;
	uint32 res = 0;
	for (i = 0; i < BLOOM_BITS; i++)
	{
		val = hash % (32 - i) + i;
		vals[i] = val;

		j = i;
		while (j > 0 && vals[j] <= vals[j - 1])
		{
			tmp = vals[j] - 1;
			vals[j] = vals[j - 1];
			vals[j - 1] = tmp;
			j--;
		}
	}
	for (i = 0; i < BLOOM_BITS; i++)
	{
		res |= (1 << vals[i]);
	}
	return res;
}

static uint32
get_path_bloom(PathHashStack *stack)
{
	int	i = 0;
	uint32 res = 0, val;

	while (stack)
	{
		uint32 hash = stack->hash;

		val = get_bloom_value(hash);

		res |= val;
		i++;
		stack = stack->parent;
	}
	return res;
}

static GINKey *
make_gin_key(JsonbValue *v, uint32 hash)
{
	GINKey *key;

	if (v->type == jbvNull)
	{
		key = (GINKey *)palloc(GINKEYLEN);
		key->type = v->type;
		SET_VARSIZE(key, GINKEYLEN);
	}
	else if (v->type == jbvBool)
	{
		key = (GINKey *)palloc(GINKEYLEN);
		key->type = v->type | (v->val.boolean ? GINKeyTrue : 0);
		SET_VARSIZE(key, GINKEYLEN);
	}
	else if (v->type == jbvNumeric)
	{
		key = (GINKey *)palloc(GINKeyLenNumeric(VARSIZE_ANY(v->val.numeric)));
		key->type = v->type;
		memcpy(GINKeyDataNumeric(key), v->val.numeric, VARSIZE_ANY(v->val.numeric));
		SET_VARSIZE(key, GINKeyLenNumeric(VARSIZE_ANY(v->val.numeric)));
	}
	else if (v->type == jbvString)
	{
		key = (GINKey *)palloc(GINKeyLenString(v->val.string.len));
		key->type = v->type;
		memcpy(GINKeyDataString(key), v->val.string.val, v->val.string.len);
		SET_VARSIZE(key, GINKeyLenString(v->val.string.len));
	}
	else
	{
		elog(ERROR, "GINKey must be scalar");
	}
	key->hash = hash;
	return key;
}

static GINKey *
make_gin_query_key(char *jqBase, int32 jqPos, int32 type, uint32 hash)
{
	GINKey *key;
	int		len;
	Numeric	numeric;

	switch(type)
	{
		case jqiNull:
			key = (GINKey *)palloc(GINKEYLEN);
			key->type = jbvNull;
			SET_VARSIZE(key, GINKEYLEN);
			break;
		case jqiString:
			read_int32(len, jqBase, jqPos);
			key = (GINKey *)palloc(GINKeyLenString(len));
			key->type = jbvString;
			memcpy(GINKeyDataString(key), jqBase + jqPos, len);
			SET_VARSIZE(key, GINKeyLenString(len));
			break;
		case jqiBool:
			read_byte(len, jqBase, jqPos);
			key = (GINKey *)palloc(GINKEYLEN);
			key->type = jbvBool | (len ? GINKeyTrue : 0);
			SET_VARSIZE(key, GINKEYLEN);
			break;
		case jqiNumeric:
			numeric = (Numeric)(jqBase + jqPos);
			key = (GINKey *)palloc(GINKeyLenNumeric(VARSIZE_ANY(numeric)));
			key->type = jbvNumeric;
			memcpy(GINKeyDataNumeric(key), numeric, VARSIZE_ANY(numeric));
			SET_VARSIZE(key, GINKeyLenNumeric(VARSIZE_ANY(numeric)));
			break;
		default:
			elog(ERROR,"Wrong state");
	}
	key->hash = hash;
	return key;
}

static int32
compare_gin_key_value(GINKey *arg1, GINKey *arg2)
{
	if (arg1->type != arg2->type)
	{
		return (arg1->type > arg2->type) ? 1 : -1;
	}
	else
	{
		switch(arg1->type)
		{
			case jbvNull:
				return 0;
			case jbvBool:
				if (GINKeyIsTrue(arg1) == GINKeyIsTrue(arg2))
					return 0;
				else if (GINKeyIsTrue(arg1) > GINKeyIsTrue(arg2))
					return 1;
				else
					return -1;
			case jbvNumeric:
				return DatumGetInt32(DirectFunctionCall2(numeric_cmp,
							 PointerGetDatum(GINKeyDataNumeric(arg1)),
							 PointerGetDatum(GINKeyDataNumeric(arg2))));
			case jbvString:
				return varstr_cmp(GINKeyDataString(arg1), GINKeyStringLen(arg1),
						GINKeyDataString(arg2), GINKeyStringLen(arg2), C_COLLATION_OID);
			default:
				elog(ERROR, "GINKey must be scalar");
				return 0;
		}
	}
}

Datum
gin_compare_jsonb_bloom_value(PG_FUNCTION_ARGS)
{
	GINKey	   *arg1 = (GINKey *)PG_GETARG_VARLENA_P(0);
	GINKey	   *arg2 = (GINKey *)PG_GETARG_VARLENA_P(1);
	int32		result = 0;

	result = compare_gin_key_value(arg1, arg2);
	if (result == 0 && arg1->hash != arg2->hash)
	{
		result = (arg1->hash > arg2->hash) ? 1 : -1;
	}
	PG_RETURN_INT32(result);
}

Datum
gin_compare_partial_jsonb_bloom_value(PG_FUNCTION_ARGS)
{
	GINKey	   *partial_key = (GINKey *)PG_GETARG_VARLENA_P(0);
	GINKey	   *key = (GINKey *)PG_GETARG_VARLENA_P(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	int32		result;
	uint32 		bloom;

	if (strategy == JsQueryMatchStrategyNumber)
	{
		GINKey *extra_data = (GINKey *)PG_GETARG_POINTER(3);
		bloom = extra_data->hash;
	}
	else
	{
		uint32 *extra_data = (uint32 *)PG_GETARG_POINTER(3);
		bloom = *extra_data;
	}

	result = compare_gin_key_value(key, partial_key);

	if (result == 0)
	{
		if ((key->hash & bloom) != bloom)
			result = -1;
	}

	PG_RETURN_INT32(result);
}

static Datum *
gin_extract_jsonb_bloom_value_internal(Jsonb *jb, int32 *nentries, uint32 **bloom)
{
	int			total = 2 * JB_ROOT_COUNT(jb);
	JsonbIterator *it;
	JsonbValue	v;
	PathHashStack *stack;
	int			i = 0,
				r;
	Datum	   *entries = NULL;
	uint32		hash;

	if (total == 0)
	{
		*nentries = 0;
		return NULL;
	}

	entries = (Datum *) palloc(sizeof(Datum) * total);
	if (bloom)
		(*bloom) = (uint32 *) palloc(sizeof(uint32) * total);

	it = JsonbIteratorInit(VARDATA(jb));

	stack = NULL;

	while ((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
	{
		PathHashStack  *tmp;

		if (i >= total)
		{
			total *= 2;
			entries = (Datum *) repalloc(entries, sizeof(Datum) * total);
			if (bloom)
				(*bloom) = (uint32 *) repalloc(*bloom, sizeof(uint32) * total);
		}

		switch (r)
		{
			case WJB_BEGIN_OBJECT:
				tmp = stack;
				stack = (PathHashStack *) palloc(sizeof(PathHashStack));
				stack->parent = tmp;
				break;
			case WJB_KEY:
				stack->hash = 0;
				JsonbHashScalarValue(&v, &stack->hash);
				break;
			case WJB_ELEM:
			case WJB_VALUE:
				if (bloom)
				{
					(*bloom)[i] = get_path_bloom(stack);
					hash = 0;
				}
				else
				{
					hash = get_path_bloom(stack);
				}
				entries[i++] =  PointerGetDatum(make_gin_key(&v, hash));
				break;
			case WJB_END_OBJECT:
				/* Pop the stack */
				tmp = stack->parent;
				pfree(stack);
				stack = tmp;
				break;
			case WJB_END_ARRAY:
			case WJB_BEGIN_ARRAY:
				break;
			default:
				elog(ERROR, "invalid JsonbIteratorNext rc: %d", r);
		}
	}

	*nentries = i;

	return entries;
}

Datum
gin_extract_jsonb_bloom_value(PG_FUNCTION_ARGS)
{
	Jsonb	   *jb = PG_GETARG_JSONB(0);
	int32	   *nentries = (int32 *) PG_GETARG_POINTER(1);

	PG_RETURN_POINTER(gin_extract_jsonb_bloom_value_internal(jb, nentries, NULL));
}

Datum
gin_extract_jsonb_query_bloom_value(PG_FUNCTION_ARGS)
{
	Jsonb	   *jb;
	int32	   *nentries = (int32 *) PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool	  **pmatch = (bool **) PG_GETARG_POINTER(3);
	Pointer	  **extra_data = (Pointer **) PG_GETARG_POINTER(4);
	int32	   *searchMode = (int32 *) PG_GETARG_POINTER(6);
	Datum	   *entries = NULL;
	int			i, n;
	uint32	   *bloom;
	Entries		e = {0};
	JsQuery	   *jq;
	ExtractedNode *root;

	switch(strategy)
	{
		case JsonbContainsStrategyNumber:
			jb = PG_GETARG_JSONB(0);
			entries = gin_extract_jsonb_bloom_value_internal(jb, nentries, NULL);
			break;

		case JsonbNestedContainsStrategyNumber:
			jb = PG_GETARG_JSONB(0);
			entries = gin_extract_jsonb_bloom_value_internal(jb, nentries, &bloom);

			n = *nentries;
			*pmatch = (bool *) palloc(sizeof(bool) * n);
			for (i = 0; i < n; i++)
				(*pmatch)[i] = true;

			*extra_data = (Pointer *) palloc(sizeof(Pointer) * n);
			for (i = 0; i < n; i++)
				(*extra_data)[i] = (Pointer)&bloom[i];
			break;

		case JsQueryMatchStrategyNumber:
			jq = PG_GETARG_JSQUERY(0);
			root = recursiveExtract(VARDATA(jq), 0, 0, &e, false);

			*nentries = e.count;
			entries = e.entries;
			*pmatch = e.partial_match;
			*extra_data = e.extra_data;
			for (i = 0; i < e.count; i++)
				((KeyExtra *)e.extra_data[i])->root = root;
			break;

		default:
			elog(ERROR, "unrecognized strategy number: %d", strategy);
			break;
	}

	/* ...although "contains {}" requires a full index scan */
	if (entries == NULL)
		*searchMode = GIN_SEARCH_MODE_ALL;

	PG_RETURN_POINTER(entries);
}

static bool
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

static bool
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

Datum
gin_consistent_jsonb_bloom_value(PG_FUNCTION_ARGS)
{
	bool	   *check = (bool *) PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);
	/* Jsonb	   *query = PG_GETARG_JSONB(2); */
	int32		nkeys = PG_GETARG_INT32(3);
	Pointer	   *extra_data = (Pointer *) PG_GETARG_POINTER(4);
	bool	   *recheck = (bool *) PG_GETARG_POINTER(5);
	bool		res = true;
	int32		i;

	*recheck = true;
	switch (strategy)
	{
		case JsonbContainsStrategyNumber:
		case JsonbNestedContainsStrategyNumber:
			for (i = 0; i < nkeys; i++)
			{
				if (!check[i])
				{
					res = false;
					break;
				}
			}
			break;

		case JsQueryMatchStrategyNumber:
			if (nkeys == 0)
				res = true;
			else
				res = execRecursive(((KeyExtra *)extra_data[0])->root, check);
			break;

		default:
			elog(ERROR, "unrecognized strategy number: %d", strategy);
			break;
	}

	PG_RETURN_BOOL(res);
}

Datum
gin_triconsistent_jsonb_bloom_value(PG_FUNCTION_ARGS)
{
	GinTernaryValue   *check = (GinTernaryValue *) PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);
	/* Jsonb	   *query = PG_GETARG_JSONB(2); */
	int32		nkeys = PG_GETARG_INT32(3);
	Pointer	   *extra_data = (Pointer *) PG_GETARG_POINTER(4);
	GinTernaryValue	res = GIN_TRUE;
	int32			i;
	bool			has_maybe = false;

	switch (strategy)
	{
		case JsonbContainsStrategyNumber:
		case JsonbNestedContainsStrategyNumber:
			/*
			 * All extracted keys must be present.  A combination of GIN_MAYBE and
			 * GIN_TRUE induces a GIN_MAYBE result, because then all keys may be
			 * present.
			 */
			for (i = 0; i < nkeys; i++)
			{
				if (check[i] == GIN_FALSE)
				{
					res = GIN_FALSE;
					break;
				}
				if (check[i] == GIN_MAYBE)
				{
					res = GIN_MAYBE;
					has_maybe = true;
				}
			}

			/*
			 * jsonb_hash_ops index doesn't have information about correspondence of
			 * Jsonb keys and values (as distinct from GIN keys, which for this opclass
			 * are a hash of a pair, or a hash of just an element), so invariably we
			 * recheck.  This is also reflected in how GIN_MAYBE is given in response
			 * to there being no GIN_MAYBE input.
			 */
			if (!has_maybe && res == GIN_TRUE)
				res = GIN_MAYBE;
			break;

		case JsQueryMatchStrategyNumber:
			if (nkeys == 0)
				res = GIN_TRUE;
			else
				res = execRecursiveTristate(((KeyExtra *)extra_data[0])->root, check);
			break;

		default:
			elog(ERROR, "unrecognized strategy number: %d", strategy);
			break;
	}

	PG_RETURN_GIN_TERNARY_VALUE(res);
}
