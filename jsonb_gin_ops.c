/*-------------------------------------------------------------------------
 *
 * jsonb_gin_ops.c
 *     Support GIN over jsonb with jsquery operation
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Alexander Korotkov <aekorotkov@gmail.com>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsonb_gin_ops.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
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

#define	GINKeyTrue 0x80
#define	GINKeyMinusInf 0x80
#define	GINKeyEmptyArray 0x80
#define GINKeyLenString (INTALIGN(offsetof(GINKey, data)) + sizeof(uint32))
#define GINKeyLenNumeric(len) (INTALIGN(offsetof(GINKey, data)) + len)
#define GINKeyDataString(key) (*(uint32 *)((Pointer)key + INTALIGN(offsetof(GINKey, data))))
#define GINKeyDataNumeric(key) ((Pointer)key + INTALIGN(offsetof(GINKey, data)))
#define GINKeyType(key) ((key)->type & 0x7F)
#define GINKeyIsTrue(key) ((key)->type & GINKeyTrue)
#define GINKeyIsMinusInf(key) ((key)->type & GINKeyMinusInf)
#define GINKeyIsEmptyArray(key) ((key)->type & GINKeyEmptyArray)

#define BLOOM_BITS 2
#define JsonbNestedContainsStrategyNumber	13
#define JsQueryMatchStrategyNumber			14

typedef struct
{
	Datum *entries;
	Pointer *extra_data;
	bool	*partial_match;
	int		*map;
	int count, total;
} Entries;

typedef struct
{
	ExtractedNode  *root;
	ExtractedNode  *node;
	uint32			hash;
	bool			lossyHash;
	GINKey		   *rightBound;
} KeyExtra;

static uint32 get_bloom_value(uint32 hash);
static uint32 get_path_bloom(PathHashStack *stack);
static GINKey *make_gin_key(JsonbValue *v, uint32 hash);
static GINKey *make_gin_key_string(uint32 hash);
static GINKey *make_gin_query_value_key(JsQueryItem *value, uint32 hash);
static GINKey *make_gin_query_key(ExtractedNode *node, bool *partialMatch, uint32 hash, KeyExtra *keyExtra);
static GINKey *make_gin_query_key_minus_inf(uint32 hash);
static int32 compare_gin_key_value(GINKey *arg1, GINKey *arg2);
static int add_entry(Entries *e, Datum key, Pointer extra, bool pmatch);

PG_FUNCTION_INFO_V1(gin_compare_jsonb_value_path);
PG_FUNCTION_INFO_V1(gin_compare_partial_jsonb_value_path);
PG_FUNCTION_INFO_V1(gin_extract_jsonb_value_path);
PG_FUNCTION_INFO_V1(gin_extract_jsonb_query_value_path);
PG_FUNCTION_INFO_V1(gin_consistent_jsonb_value_path);
PG_FUNCTION_INFO_V1(gin_triconsistent_jsonb_value_path);
PG_FUNCTION_INFO_V1(gin_debug_query_value_path);

Datum gin_compare_jsonb_value_path(PG_FUNCTION_ARGS);
Datum gin_compare_partial_jsonb_value_path(PG_FUNCTION_ARGS);
Datum gin_extract_jsonb_value_path(PG_FUNCTION_ARGS);
Datum gin_extract_jsonb_query_value_path(PG_FUNCTION_ARGS);
Datum gin_consistent_jsonb_value_path(PG_FUNCTION_ARGS);
Datum gin_triconsistent_jsonb_value_path(PG_FUNCTION_ARGS);
Datum gin_debug_query_value_path(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gin_compare_jsonb_path_value);
PG_FUNCTION_INFO_V1(gin_compare_partial_jsonb_path_value);
PG_FUNCTION_INFO_V1(gin_extract_jsonb_path_value);
PG_FUNCTION_INFO_V1(gin_extract_jsonb_query_path_value);
PG_FUNCTION_INFO_V1(gin_consistent_jsonb_path_value);
PG_FUNCTION_INFO_V1(gin_triconsistent_jsonb_path_value);
PG_FUNCTION_INFO_V1(gin_debug_query_path_value);

Datum gin_compare_jsonb_path_value(PG_FUNCTION_ARGS);
Datum gin_compare_partial_jsonb_path_value(PG_FUNCTION_ARGS);
Datum gin_extract_jsonb_path_value(PG_FUNCTION_ARGS);
Datum gin_extract_jsonb_query_path_value(PG_FUNCTION_ARGS);
Datum gin_consistent_jsonb_path_value(PG_FUNCTION_ARGS);
Datum gin_triconsistent_jsonb_path_value(PG_FUNCTION_ARGS);
Datum gin_debug_query_path_value(PG_FUNCTION_ARGS);

static int
add_entry(Entries *e, Datum key, Pointer extra, bool pmatch)
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
		e->extra_data = (Pointer *)repalloc(e->extra_data, e->total * sizeof(Pointer));
		e->partial_match = (bool *)repalloc(e->partial_match, e->total * sizeof(bool));
	}
	entryNum = e->count;
	e->count++;
	e->entries[entryNum] = key;
	e->extra_data[entryNum] = extra;
	e->partial_match[entryNum] = pmatch;
	return entryNum;
}

static uint32
get_bloom_value(uint32 hash)
{
	int i, j, vals[BLOOM_BITS], val, tmp;
	uint32 res = 0;
	for (i = 0; i < BLOOM_BITS; i++)
	{
		val = hash % (32 - i) + i;
		hash /= (32 - i);
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

static uint32
get_query_path_bloom(PathItem *pathItem, bool *lossy)
{
	uint32 res = 0, val;

	*lossy = false;
	while (pathItem)
	{
		uint32 hash;

		if (pathItem->type == iKey)
		{
			hash = hash_any((unsigned char *)pathItem->s, pathItem->len);
			val = get_bloom_value(hash);
			res |= val;
		}
		else if (pathItem->type == iAny || pathItem->type == iAnyKey)
		{
			*lossy = true;
		}

		pathItem = pathItem->parent;
	}
	return res;
}

#ifdef NOT_USED
static void
log_gin_key(GINKey *key)
{
	if (GINKeyType(key) == jbvNull)
	{
		elog(NOTICE, "hash = %X, NULL", key->hash);
	}
	else if (GINKeyType(key) == jbvBool)
	{
		if (GINKeyIsTrue(key))
			elog(NOTICE, "hash = %X, true", key->hash);
		else
			elog(NOTICE, "hash = %X, false", key->hash);
	}
	else if (GINKeyType(key) == jbvNumeric)
	{
		if (GINKeyIsMinusInf(key))
		{
			elog(NOTICE, "hash = %X, -inf", key->hash);
		}
		else
		{
			char *s;
			s = DatumGetCString(DirectFunctionCall1(numeric_out, PointerGetDatum(GINKeyDataNumeric(key))));
			elog(NOTICE, "hash = %X, \"%s\"", key->hash, s);
		}
	}
	else if (GINKeyType(key) == jbvString)
	{
		elog(NOTICE, "hash = %X, %X", key->hash, GINKeyDataString(key));
	}
	else
	{
		elog(ERROR, "GINKey must be scalar");
	}
}
#endif

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
	else if (v->type == jbvArray)
	{
		key = (GINKey *)palloc(GINKEYLEN);
		key->type = v->type;
		if (v->val.array.nElems == 0)
			key->type |= GINKeyEmptyArray;
		SET_VARSIZE(key, GINKEYLEN);
	}
	else if (v->type == jbvObject)
	{
		key = (GINKey *)palloc(GINKEYLEN);
		key->type = v->type;
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
		key = (GINKey *)palloc(GINKeyLenString);
		key->type = v->type;
		GINKeyDataString(key) = hash_any((unsigned char *)v->val.string.val,
														  v->val.string.len);
		SET_VARSIZE(key, GINKeyLenString);
	}
	else
	{
		elog(ERROR, "GINKey must be scalar");
	}
	key->hash = hash;
	return key;
}

static GINKey *
make_gin_key_string(uint32 hash)
{
	GINKey *key;

	key = (GINKey *)palloc(GINKeyLenString);
	key->type = jbvString;
	GINKeyDataString(key) = 0;
	SET_VARSIZE(key, GINKeyLenString);
	key->hash = hash;
	return key;
}

static GINKey *
make_gin_query_value_key(JsQueryItem *value, uint32 hash)
{
	GINKey *key;
	int32	len;
	char	*s;
	Numeric	numeric;

	switch(value->type)
	{
		case jqiNull:
			key = (GINKey *)palloc(GINKEYLEN);
			key->type = jbvNull;
			SET_VARSIZE(key, GINKEYLEN);
			break;
		case jqiString:
			key = (GINKey *)palloc(GINKeyLenString);
			key->type = jbvString;
			s = jsqGetString(value, &len);
			GINKeyDataString(key) = hash_any((unsigned char *)s, len);
			SET_VARSIZE(key, GINKeyLenString);
			break;
		case jqiBool:
			key = (GINKey *)palloc(GINKEYLEN);
			key->type = jbvBool | (jsqGetBool(value) ? GINKeyTrue : 0);
			SET_VARSIZE(key, GINKEYLEN);
			break;
		case jqiNumeric:
			numeric = jsqGetNumeric(value);
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

static GINKey *
make_gin_query_key(ExtractedNode *node, bool *partialMatch, uint32 hash, KeyExtra *keyExtra)
{
	JsonbValue	v;
	GINKey	   *key;

	switch (node->type)
	{
		case eExactValue:
			key = make_gin_query_value_key(node->exactValue, hash);
			break;
		case eEmptyArray:
			v.type = jbvArray;
			v.val.array.nElems = 0;
			key = make_gin_key(&v, hash);
			break;
		case eInequality:
			*partialMatch = true;
			if (node->bounds.leftBound)
				key = make_gin_query_value_key(node->bounds.leftBound, hash);
			else
				key = make_gin_query_key_minus_inf(hash);
			if (node->bounds.rightBound)
				keyExtra->rightBound = make_gin_query_value_key(node->bounds.rightBound, hash);
			else
				keyExtra->rightBound = NULL;
			break;
		case eIs:
			switch (node->isType)
			{
				case jbvArray:
					*partialMatch = true;
					v.type = jbvArray;
					v.val.array.nElems = 1;
					key = make_gin_key(&v, hash);
					break;
				case jbvObject:
					*partialMatch = true;
					v.type = jbvObject;
					key = make_gin_key(&v, hash);
					break;
				case jbvString:
					*partialMatch = true;
					key = make_gin_key_string(hash);
					break;
				case jbvNumeric:
					*partialMatch = true;
					key = make_gin_query_key_minus_inf(hash);
					break;
				case jbvBool:
					*partialMatch = true;
					v.type = jbvBool;
					v.val.boolean = false;
					key = make_gin_key(&v, hash);
					break;
				case jbvNull:
					v.type = jbvNull;
					key = make_gin_key(&v, hash);
					break;
				default:
					elog(ERROR,"Wrong type");
					return NULL;
			}
			break;
		case eAny:
			v.type = jbvNull;
			key = make_gin_key(&v, hash);
			*partialMatch = true;
			break;
		default:
			elog(ERROR, "Wrong type");
			break;
	}
	return key;
}


static GINKey *
make_gin_query_key_minus_inf(uint32 hash)
{
	GINKey *key;

	key = (GINKey *)palloc(GINKEYLEN);
	key->type = jbvNumeric | GINKeyMinusInf;
	key->hash = hash;
	SET_VARSIZE(key, GINKEYLEN);
	return key;
}

static bool
check_value_path_entry_handler(ExtractedNode *node, Pointer extra)
{
	return true;
}

static int
make_value_path_entry_handler(ExtractedNode *node, Pointer extra)
{
	Entries	   *e = (Entries *)extra;
	uint32		hash;
	bool		lossy, partialMatch;
	GINKey	   *key;
	KeyExtra   *keyExtra;
	int			result;

	Assert(!isLogicalNodeType(node->type));

	hash = get_query_path_bloom(node->path, &lossy);
	keyExtra = (KeyExtra *)palloc(sizeof(KeyExtra));
	keyExtra->hash = hash;
	keyExtra->node = node;
	keyExtra->lossyHash = lossy;

	key = make_gin_query_key(node, &partialMatch, hash, keyExtra);

	result = add_entry(e, PointerGetDatum(key), (Pointer)keyExtra,
											lossy | partialMatch);
	return result;
}

static int32
compare_gin_key_value(GINKey *arg1, GINKey *arg2)
{
	if (GINKeyType(arg1) != GINKeyType(arg2))
	{
		return (GINKeyType(arg1) > GINKeyType(arg2)) ? 1 : -1;
	}
	else
	{
		switch(GINKeyType(arg1))
		{
			case jbvNull:
			case jbvArray:
				if (GINKeyIsEmptyArray(arg1) == GINKeyIsEmptyArray(arg2))
					return 0;
				else if (GINKeyIsEmptyArray(arg1) > GINKeyIsEmptyArray(arg2))
					return 1;
				else
					return -1;
			case jbvObject:
				return 0;
			case jbvBool:
				if (GINKeyIsTrue(arg1) == GINKeyIsTrue(arg2))
					return 0;
				else if (GINKeyIsTrue(arg1) > GINKeyIsTrue(arg2))
					return 1;
				else
					return -1;
			case jbvNumeric:
				if (GINKeyIsMinusInf(arg1))
				{
					if (GINKeyIsMinusInf(arg2))
						return 0;
					else
						return -1;
				}
				else
				{
					if (GINKeyIsMinusInf(arg2))
						return 1;
				}
				return DatumGetInt32(DirectFunctionCall2(numeric_cmp,
							 PointerGetDatum(GINKeyDataNumeric(arg1)),
							 PointerGetDatum(GINKeyDataNumeric(arg2))));
			case jbvString:
				if (GINKeyDataString(arg1) < GINKeyDataString(arg2))
					return -1;
				else if (GINKeyDataString(arg1) == GINKeyDataString(arg2))
					return 0;
				else
					return 1;
			default:
				elog(ERROR, "GINKey must be scalar");
				return 0;
		}
	}
}

Datum
gin_compare_jsonb_value_path(PG_FUNCTION_ARGS)
{
	GINKey	   *arg1 = (GINKey *)PG_GETARG_VARLENA_P(0);
	GINKey	   *arg2 = (GINKey *)PG_GETARG_VARLENA_P(1);
	int32		result = 0;

	result = compare_gin_key_value(arg1, arg2);
	if (result == 0 && arg1->hash != arg2->hash)
	{
		result = (arg1->hash > arg2->hash) ? 1 : -1;
	}
	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);
	PG_RETURN_INT32(result);
}

Datum
gin_compare_partial_jsonb_value_path(PG_FUNCTION_ARGS)
{
	GINKey	   *partial_key = (GINKey *)PG_GETARG_VARLENA_P(0);
	GINKey	   *key = (GINKey *)PG_GETARG_VARLENA_P(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	int32		result;

	if (strategy == JsQueryMatchStrategyNumber)
	{
		KeyExtra *extra = (KeyExtra *)PG_GETARG_POINTER(3);
		ExtractedNode *node = extra->node;

		switch (node->type)
		{
			case eExactValue:
			case eEmptyArray:
				result = compare_gin_key_value(key, partial_key);
				break;
			case eInequality:
				result = 0;
				if (!node->bounds.leftInclusive &&
						compare_gin_key_value(key, partial_key) <= 0)
				{
					result = -1;
				}
				if (result == 0 && extra->rightBound)
				{
					result = compare_gin_key_value(key,	extra->rightBound);
					if ((node->bounds.rightInclusive && result <= 0)
																|| result < 0)
						result = 0;
					else
						result = 1;
				}
				break;
			case eIs:
				if (node->isType == GINKeyType(key))
					result = 0;
				else
					result = (GINKeyType(key) > node->isType) ? 1 : -1;
				break;
			case eAny:
				result = 0;
				break;
			default:
				elog(ERROR, "Wrong type");
				break;
		}

		if (result == 0)
		{
			if (extra->lossyHash)
			{
				if ((key->hash & extra->hash) != extra->hash)
					result = -1;
			}
			else
			{
				if (key->hash != extra->hash)
					result = -1;
			}
		}
	}
	else
	{
		uint32 *extra_data = (uint32 *)PG_GETARG_POINTER(3);
		uint32	bloom = *extra_data;

		result = compare_gin_key_value(key, partial_key);

		if (result == 0)
		{
			if ((key->hash & bloom) != bloom)
				result = -1;
		}
	}

	PG_FREE_IF_COPY(partial_key, 0);
	PG_FREE_IF_COPY(key, 1);
	PG_RETURN_INT32(result);
}

static Datum *
gin_extract_jsonb_value_path_internal(Jsonb *jb, int32 *nentries, uint32 **bloom)
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

	it = JsonbIteratorInit(&jb->root);

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
			case WJB_BEGIN_ARRAY:
				entries[i++] = PointerGetDatum(make_gin_key(&v, get_path_bloom(stack)));
				break;
			case WJB_BEGIN_OBJECT:
				entries[i++] = PointerGetDatum(make_gin_key(&v, get_path_bloom(stack)));
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
				break;
			default:
				elog(ERROR, "invalid JsonbIteratorNext rc: %d", r);
		}
	}

	*nentries = i;

	return entries;
}

Datum
gin_extract_jsonb_value_path(PG_FUNCTION_ARGS)
{
	Jsonb	   *jb = PG_GETARG_JSONB(0);
	int32	   *nentries = (int32 *) PG_GETARG_POINTER(1);

	PG_RETURN_POINTER(gin_extract_jsonb_value_path_internal(jb, nentries, NULL));
}

Datum
gin_debug_query_value_path(PG_FUNCTION_ARGS)
{
	JsQuery	   *jq;
	Entries		e = {0};
	char	   *s;

	jq = PG_GETARG_JSQUERY(0);
	s = debugJsQuery(jq, make_value_path_entry_handler,
							check_value_path_entry_handler, (Pointer)&e);
	PG_RETURN_TEXT_P(cstring_to_text(s));
}

Datum
gin_extract_jsonb_query_value_path(PG_FUNCTION_ARGS)
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
			entries = gin_extract_jsonb_value_path_internal(jb, nentries, NULL);
			break;

		case JsonbNestedContainsStrategyNumber:
			jb = PG_GETARG_JSONB(0);
			entries = gin_extract_jsonb_value_path_internal(jb, nentries, &bloom);

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
			root = extractJsQuery(jq, make_value_path_entry_handler,
									check_value_path_entry_handler, (Pointer)&e);
			if (root)
			{
				*nentries = e.count;
				entries = e.entries;
				*pmatch = e.partial_match;
				*extra_data = e.extra_data;
				for (i = 0; i < e.count; i++)
					((KeyExtra *)e.extra_data[i])->root = root;
			}
			else
			{
				entries = NULL;
				*nentries = 0;
			}
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

Datum
gin_consistent_jsonb_value_path(PG_FUNCTION_ARGS)
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
gin_triconsistent_jsonb_value_path(PG_FUNCTION_ARGS)
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
				res = GIN_MAYBE;
			else
				res = execRecursiveTristate(((KeyExtra *)extra_data[0])->root, check);

			if (res == GIN_TRUE)
				res = GIN_MAYBE;

			break;

		default:
			elog(ERROR, "unrecognized strategy number: %d", strategy);
			break;
	}

	PG_RETURN_GIN_TERNARY_VALUE(res);
}

static bool
get_query_path_hash(PathItem *pathItem, uint32 *hash)
{
	check_stack_depth();

	if (!pathItem)
		return true;

	if (!get_query_path_hash(pathItem->parent, hash))
	{
		return false;
	}
	else
	{
		if (pathItem->type == iAny || pathItem->type == iAnyKey)
		{
			return false;
		}
		else
		{
			if (pathItem->type == iKey)
			{
				*hash = (*hash << 1) | (*hash >> 31);
				*hash ^= hash_any((unsigned char *)pathItem->s, pathItem->len);
			}
			else if (pathItem->type == iAnyArray || pathItem->type == iIndexArray)
			{
				*hash = (*hash << 1) | (*hash >> 31);
				*hash ^= JB_FARRAY;
			}
			return true;
		}
	}
}

static bool
check_path_value_entry_handler(ExtractedNode *node, Pointer extra)
{
	uint32		hash;
	hash = 0;
	if (!get_query_path_hash(node->path, &hash))
		return false;
	return true;
}

static int
make_path_value_entry_handler(ExtractedNode *node, Pointer extra)
{
	Entries	   *e = (Entries *)extra;
	uint32		hash;
	GINKey	   *key;
	KeyExtra   *keyExtra;
	int			result;
	bool		partialMatch = false;

	Assert(!isLogicalNodeType(node->type));

	hash = 0;
	if (!get_query_path_hash(node->path, &hash))
		return -1;

	keyExtra = (KeyExtra *)palloc(sizeof(KeyExtra));
	keyExtra->hash = hash;
	keyExtra->node = node;
	key = make_gin_query_key(node, &partialMatch, hash, keyExtra);

	result = add_entry(e, PointerGetDatum(key), (Pointer)keyExtra, partialMatch);
	return result;
}

Datum
gin_compare_jsonb_path_value(PG_FUNCTION_ARGS)
{
	GINKey	   *arg1 = (GINKey *)PG_GETARG_VARLENA_P(0);
	GINKey	   *arg2 = (GINKey *)PG_GETARG_VARLENA_P(1);
	int32		result = 0;

	if (arg1->hash != arg2->hash)
	{
		result = (arg1->hash > arg2->hash) ? 1 : -1;
	}
	else
	{
		result = compare_gin_key_value(arg1, arg2);
	}
	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);
	PG_RETURN_INT32(result);
}

Datum
gin_compare_partial_jsonb_path_value(PG_FUNCTION_ARGS)
{
	GINKey	   *partial_key = (GINKey *)PG_GETARG_VARLENA_P(0);
	GINKey	   *key = (GINKey *)PG_GETARG_VARLENA_P(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	int32		result;

	if (key->hash != partial_key->hash)
	{
		result = (key->hash > partial_key->hash) ? 1 : -1;
	}
	else if (strategy == JsQueryMatchStrategyNumber)
	{
		KeyExtra *extra = (KeyExtra *)PG_GETARG_POINTER(3);
		ExtractedNode *node = extra->node;

		switch (node->type)
		{
			case eInequality:
				result = 0;
				if (!node->bounds.leftInclusive &&
						compare_gin_key_value(key, partial_key) <= 0)
				{
					result = -1;
				}
				if (result == 0 && extra->rightBound)
				{
					result = compare_gin_key_value(key, extra->rightBound);
					if ((node->bounds.rightInclusive && result <= 0)
																|| result < 0)
						result = 0;
					else
						result = 1;
				}
				break;
			case eIs:
				if (node->isType == GINKeyType(key))
					result = 0;
				else
					result = (GINKeyType(key) > node->isType) ? 1 : -1;
				break;
			case eAny:
				result = 0;
				break;
			default:
				elog(ERROR, "Wrong type");
				break;
		}
	}
	else
	{
		result = compare_gin_key_value(key, partial_key);
	}

	PG_FREE_IF_COPY(partial_key, 0);
	PG_FREE_IF_COPY(key, 1);
	PG_RETURN_INT32(result);
}

static Datum *
gin_extract_jsonb_path_value_internal(Jsonb *jb, int32 *nentries)
{
	int			total = 2 * JB_ROOT_COUNT(jb);
	JsonbIterator *it;
	JsonbValue	v;
	PathHashStack tail;
	PathHashStack *stack;
	int			i = 0,
				r;
	Datum	   *entries = NULL;

	if (total == 0)
	{
		*nentries = 0;
		return NULL;
	}

	entries = (Datum *) palloc(sizeof(Datum) * total);

	it = JsonbIteratorInit(&jb->root);

	tail.parent = NULL;
	tail.hash = 0;
	stack = &tail;

	while ((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
	{
		PathHashStack  *tmp;

		if (i >= total)
		{
			total *= 2;
			entries = (Datum *) repalloc(entries, sizeof(Datum) * total);
		}

		switch (r)
		{
			case WJB_BEGIN_ARRAY:
				entries[i++] = PointerGetDatum(make_gin_key(&v, stack->hash));
				tmp = stack;
				stack = (PathHashStack *) palloc(sizeof(PathHashStack));
				stack->parent = tmp;
				stack->hash = stack->parent->hash;
				stack->hash = (stack->hash << 1) | (stack->hash >> 31);
				stack->hash ^= JB_FARRAY;
				break;
			case WJB_BEGIN_OBJECT:
				entries[i++] = PointerGetDatum(make_gin_key(&v, stack->hash));
				tmp = stack;
				stack = (PathHashStack *) palloc(sizeof(PathHashStack));
				stack->parent = tmp;
				stack->hash = stack->parent->hash;
				break;
			case WJB_KEY:
				/* Initialize hash from parent */
				stack->hash = stack->parent->hash;
				JsonbHashScalarValue(&v, &stack->hash);
				break;
			case WJB_ELEM:
			case WJB_VALUE:
				/* Element/value case */
				entries[i++] = PointerGetDatum(make_gin_key(&v, stack->hash));
				break;
			case WJB_END_ARRAY:
			case WJB_END_OBJECT:
				/* Pop the stack */
				tmp = stack->parent;
				pfree(stack);
				stack = tmp;
				break;
			default:
				elog(ERROR, "invalid JsonbIteratorNext rc: %d", r);
		}
	}

	*nentries = i;

	return entries;
}

Datum
gin_extract_jsonb_path_value(PG_FUNCTION_ARGS)
{
	Jsonb	   *jb = PG_GETARG_JSONB(0);
	int32	   *nentries = (int32 *) PG_GETARG_POINTER(1);

	PG_RETURN_POINTER(gin_extract_jsonb_path_value_internal(jb, nentries));
}

Datum
gin_debug_query_path_value(PG_FUNCTION_ARGS)
{
	JsQuery	   *jq;
	Entries		e = {0};
	char	   *s;

	jq = PG_GETARG_JSQUERY(0);
	s = debugJsQuery(jq, make_path_value_entry_handler,
										check_path_value_entry_handler, (Pointer)&e);
	PG_RETURN_TEXT_P(cstring_to_text(s));
}

Datum
gin_extract_jsonb_query_path_value(PG_FUNCTION_ARGS)
{
	Jsonb	   *jb;
	int32	   *nentries = (int32 *) PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool	  **pmatch = (bool **) PG_GETARG_POINTER(3);
	Pointer	  **extra_data = (Pointer **) PG_GETARG_POINTER(4);
	int32	   *searchMode = (int32 *) PG_GETARG_POINTER(6);
	Datum	   *entries = NULL;
	int			i;
	Entries		e = {0};
	JsQuery	   *jq;
	ExtractedNode *root;

	switch(strategy)
	{
		case JsonbContainsStrategyNumber:
			jb = PG_GETARG_JSONB(0);
			entries = gin_extract_jsonb_path_value_internal(jb, nentries);
			break;

		case JsQueryMatchStrategyNumber:
			jq = PG_GETARG_JSQUERY(0);
			root = extractJsQuery(jq, make_path_value_entry_handler,
										check_path_value_entry_handler, (Pointer)&e);
			if (root)
			{
				*nentries = e.count;
				entries = e.entries;
				*pmatch = e.partial_match;
				*extra_data = e.extra_data;
				for (i = 0; i < e.count; i++)
					((KeyExtra *)e.extra_data[i])->root = root;
			}
			else
			{
				entries = NULL;
				*nentries = 0;
			}
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

Datum
gin_consistent_jsonb_path_value(PG_FUNCTION_ARGS)
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
gin_triconsistent_jsonb_path_value(PG_FUNCTION_ARGS)
{
	GinTernaryValue *check = (GinTernaryValue *) PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);
	/* Jsonb	   *query = PG_GETARG_JSONB(2); */
	int32		nkeys = PG_GETARG_INT32(3);
	Pointer	   *extra_data = (Pointer *) PG_GETARG_POINTER(4);
	GinTernaryValue res = GIN_TRUE;
	int32			i;
	bool			has_maybe = false;


	switch (strategy)
	{
		case JsonbContainsStrategyNumber:
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
				res = GIN_MAYBE;
			else
				res = execRecursiveTristate(((KeyExtra *)extra_data[0])->root, check);

			if (res == GIN_TRUE)
				res = GIN_MAYBE;

			break;

		default:
			elog(ERROR, "unrecognized strategy number: %d", strategy);
			break;
	}

	PG_RETURN_GIN_TERNARY_VALUE(res);
}

