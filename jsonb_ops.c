#include "postgres.h"

#include "access/gin.h"
#include "access/skey.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

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
#define JsonbNestedContainsStrategyNumber 13

static uint32 get_bloom_value(uint32 hash);
static uint32 get_path_bloom(PathHashStack *stack);
static GINKey *make_gin_key(JsonbValue *v, uint32 hash);
static int32 compare_gin_key_value(GINKey *arg1, GINKey *arg2);

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

		hash = (hash << 1) | (hash >> 31);
		hash ^= i;

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
	uint32	   *extra_data = (uint32 *)PG_GETARG_POINTER(3);
	int32		result;
	uint32 		bloom = *extra_data;

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
	Jsonb	   *jb = PG_GETARG_JSONB(0);
	int32	   *nentries = (int32 *) PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool	  **pmatch = (bool **) PG_GETARG_POINTER(3);
	Pointer	  **extra_data = (Pointer **) PG_GETARG_POINTER(4);
	int32	   *searchMode = (int32 *) PG_GETARG_POINTER(6);
	Datum	   *entries = NULL;
	int			i, n;
	uint32	   *bloom;

	if (strategy == JsonbContainsStrategyNumber)
	{
		entries = gin_extract_jsonb_bloom_value_internal(jb, nentries, NULL);
	}
	else if (strategy == JsonbNestedContainsStrategyNumber)
	{
		entries = gin_extract_jsonb_bloom_value_internal(jb, nentries, &bloom);

		n = *nentries;
		*pmatch = (bool *) palloc(sizeof(bool) * n);
		for (i = 0; i < n; i++)
			(*pmatch)[i] = true;

		*extra_data = (Pointer *) palloc(sizeof(Pointer) * n);
		for (i = 0; i < n; i++)
			(*extra_data)[i] = (Pointer)&bloom[i];
	}
	else
	{
		elog(ERROR, "unrecognized strategy number: %d", strategy);
	}

	/* ...although "contains {}" requires a full index scan */
	if (entries == NULL)
		*searchMode = GIN_SEARCH_MODE_ALL;

	PG_RETURN_POINTER(entries);
}

Datum
gin_consistent_jsonb_bloom_value(PG_FUNCTION_ARGS)
{
	bool	   *check = (bool *) PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);
	/* Jsonb	   *query = PG_GETARG_JSONB(2); */
	int32		nkeys = PG_GETARG_INT32(3);
	/* Pointer	   *extra_data = (Pointer *) PG_GETARG_POINTER(4); */
	bool	   *recheck = (bool *) PG_GETARG_POINTER(5);
	bool		res = true;
	int32		i;

	if (strategy != JsonbContainsStrategyNumber &&
			strategy != JsonbNestedContainsStrategyNumber)
		elog(ERROR, "unrecognized strategy number: %d", strategy);

	/*
	 * jsonb_hash_ops index doesn't have information about correspondence
	 * of Jsonb keys and values (as distinct from GIN keys, which a
	 * key/value pair is stored as), so invariably we recheck.  Besides,
	 * there are some special rules around the containment of raw scalar
	 * arrays and regular arrays that are not represented here.  However,
	 * if all of the keys are not present, that's sufficient reason to
	 * return false and finish immediately.
	 */
	*recheck = true;
	for (i = 0; i < nkeys; i++)
	{
		if (!check[i])
		{
			res = false;
			break;
		}
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
	/* Pointer	   *extra_data = (Pointer *) PG_GETARG_POINTER(4); */
	GinTernaryValue	res = GIN_TRUE;
	int32			i;
	bool			has_maybe = false;

	if (strategy != JsonbContainsStrategyNumber &&
			strategy != JsonbNestedContainsStrategyNumber)
		elog(ERROR, "unrecognized strategy number: %d", strategy);

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

	PG_RETURN_GIN_TERNARY_VALUE(res);
}
