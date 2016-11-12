/*-------------------------------------------------------------------------
 *
 * jsquery_op.c
 *     Functions and operations over jsquery/jsonb datatypes
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_op.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_crc.h"
#if PG_VERSION_NUM >= 90500
/*
 * We have to keep same checksum algorithm as in pre-9.5 in order to be
 * pg_upgradeable.
 */
#define	INIT_CRC32	INIT_LEGACY_CRC32
#define	FIN_CRC32	FIN_LEGACY_CRC32
#define	COMP_CRC32	COMP_LEGACY_CRC32
#endif

#include "jsquery.h"

static bool recursiveExecute(JsQueryItem *jsq, JsonbValue *jb);

static int
compareNumeric(Numeric a, Numeric b)
{
	return	DatumGetInt32(
				DirectFunctionCall2(
					numeric_cmp,
					PointerGetDatum(a),
					PointerGetDatum(b)
				)
			);
}

static int
JsonbType(JsonbValue *jb)
{
	int type = jb->type;

	if (jb->type == jbvBinary)
	{
		JsonbContainer	*jbc = jb->val.binary.data;
		type = jbc->type;
		if (type == (jbvArray | jbvScalar))
			type = jbvScalar;
	}

	return type;
}

static JsonbValue *
JsonbSize(JsonbValue *jb, JsonbValue *size)
{
	JsonbValue			v;
	JsonbIterator	   *it;
	JsonbIteratorToken	r;
	int32				length;
	int					type = JsonbType(jb);

	if (type != jbvArray && type != jbvObject)
		return NULL;

	Assert(jb->type == jbvBinary);

	it = JsonbIteratorInit(jb->val.binary.data);
	r = JsonbIteratorNext(&it, &v, true);
	Assert(r == WJB_BEGIN_ARRAY || r == WJB_BEGIN_OBJECT);

	if (r == WJB_BEGIN_ARRAY)
	{
		length = v.val.array.nElems;
		if (length < 0)
			length = JsonGetArraySize(jb->val.binary.data);
	}
	else
	{
		length = v.val.object.nPairs;
		if (length < 0)
			length = JsonGetObjectSize(jb->val.binary.data);
	}

	size->type = jbvNumeric;
	size->val.numeric = DatumGetNumeric(DirectFunctionCall1(
										int4_numeric, Int32GetDatum(length)));
	return size;
}

static bool
recursiveAny(JsQueryItem *jsq, JsonbValue *jb)
{
	bool			res = false;
	JsonbIterator	*it;
	int32			r;
	JsonbValue		v;

	check_stack_depth();

	it = JsonbIteratorInit(jb->val.binary.data);

	while(res == false && (r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
	{
		if (r == WJB_KEY)
		{
			r = JsonbIteratorNext(&it, &v, true);
			Assert(r == WJB_VALUE);
		}

		if (r == WJB_VALUE || r == WJB_ELEM)
		{
			res = recursiveExecute(jsq, &v);

			if (res == false && v.type == jbvBinary)
				res = recursiveAny(jsq, &v);
		}
	}

	return res;
}

static bool
recursiveAll(JsQueryItem *jsq, JsonbValue *jb)
{
	bool			res = true;
	JsonbIterator	*it;
	int32			r;
	JsonbValue		v;

	check_stack_depth();

	it = JsonbIteratorInit(jb->val.binary.data);

	while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
	{
		if (r == WJB_KEY)
		{
			r = JsonbIteratorNext(&it, &v, true);
			Assert(r == WJB_VALUE);
		}

		if (r == WJB_VALUE || r == WJB_ELEM)
		{
			if ((res = recursiveExecute(jsq, &v)) == true)
			{
				if (v.type == jbvBinary)
					res = recursiveAll(jsq, &v);
			}

			if (res == false)
				break;
		}
	}

	return res;
}

static bool
checkScalarEquality(JsQueryItem *jsq,  JsonbValue *jb)
{
	int		len;
	char	*s;

	if (jsq->type == jqiAny)
		return true;

	if (jb->type == jbvBinary)
		return false;

	if ((int)jb->type != (int)jsq->type /* see enums */)
		return false;

	switch(jsq->type)
	{
		case jqiNull:
			return true;
		case jqiString:
			s = jsqGetString(jsq, &len);
			return (len == jb->val.string.len && memcmp(jb->val.string.val, s, len) == 0);
		case jqiBool:
			return (jb->val.boolean == jsqGetBool(jsq));
		case jqiNumeric:
			return (compareNumeric(jsqGetNumeric(jsq), jb->val.numeric) == 0);
		default:
			elog(ERROR,"Wrong state");
	}

	return false;
}

static bool
checkArrayEquality(JsQueryItem *jsq, JsonbValue *jb)
{
	int32			r;
	JsonbIterator	*it;
	JsonbValue		v;
	JsQueryItem	elem;
	int				nelems;

	if (!(jsq->type == jqiArray && JsonbType(jb) == jbvArray))
		return false;

	nelems = JsonContainerSize(jb->val.binary.data);
	if (nelems < 0)
		nelems = JsonGetArraySize(jb->val.binary.data);

	if (nelems != jsq->array.nelems)
		return false;

	it = JsonbIteratorInit(jb->val.binary.data);
	r = JsonbIteratorNext(&it, &v, true);
	Assert(r == WJB_BEGIN_ARRAY);

	while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
	{
		if (r != WJB_ELEM)
			continue;

		jsqIterateArray(jsq, &elem);

		if (checkScalarEquality(&elem, &v) == false)
			return false;
	}

	return true;
}

static bool
checkScalarIn(JsQueryItem *jsq, JsonbValue *jb)
{
	JsQueryItem	elem;

	if (jb->type == jbvBinary)
		return false;

	if (jsq->type != jqiArray)
		return false;

	while(jsqIterateArray(jsq, &elem))
		if (checkScalarEquality(&elem, jb))
			return true;

	return false;
}

static bool
executeArrayOp(JsQueryItem *jsq, int32 op, JsonbValue *jb)
{
	int32			r;
	JsonbIterator	*it;
	JsonbValue		v;
	JsQueryItem	elem;

	if (JsonbType(jb) != jbvArray)
		return false;
	if (jsq->type != jqiArray)
		return false;

	if (op == jqiContains)
	{
		while(jsqIterateArray(jsq, &elem))
		{
			bool res = false;

			it = JsonbIteratorInit(jb->val.binary.data);

			while(res == false && (r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
			{
				if (r == WJB_ELEM && checkScalarEquality(&elem, &v))
					res = true;
			}

			if (res == false)
				return false;
		}
	}
	else
	{
		it = JsonbIteratorInit(jb->val.binary.data);

		while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
		{
			if (r == WJB_ELEM)
			{
				bool res = false;

				jsqIterateInit(jsq);
				while(jsqIterateArray(jsq, &elem))
				{
					if (checkScalarEquality(&elem, &v))
					{
						if (op == jqiOverlap)
							return true;
						res = true;
						break;
					}
				}

				if (op == jqiContained && res == false)
					return false;
			}
		}

		if (op == jqiOverlap)
			return false;
	}

	return true;
}

static bool
makeCompare(JsQueryItem *jsq, int32 op, JsonbValue *jb)
{
	int	res;

	if (jb->type != jbvNumeric)
		return false;
	if (jsq->type != jqiNumeric)
		return false;

	res = compareNumeric(jb->val.numeric, jsqGetNumeric(jsq));

	switch(op)
	{
		case jqiEqual:
			return (res == 0);
		case jqiLess:
			return (res < 0);
		case jqiGreater:
			return (res > 0);
		case jqiLessOrEqual:
			return (res <= 0);
		case jqiGreaterOrEqual:
			return (res >= 0);
		default:
			elog(ERROR, "Unknown operation");
	}

	return false;
}

static bool
executeExpr(JsQueryItem *jsq, int32 op, JsonbValue *jb)
{
	bool res = false;
	/*
	 * read arg type
	 */
	Assert(jsqGetNext(jsq, NULL) == false);
	Assert(jsq->type == jqiAny || jsq->type == jqiString || jsq->type == jqiNumeric ||
		   jsq->type == jqiNull || jsq->type == jqiBool || jsq->type == jqiArray);

	switch(op)
	{
		case jqiEqual:
			if (JsonbType(jb) == jbvArray && jsq->type == jqiArray)
				res = checkArrayEquality(jsq, jb);
			else
				res = checkScalarEquality(jsq, jb);
			break;
		case jqiIn:
			res = checkScalarIn(jsq, jb);
			break;
		case jqiOverlap:
		case jqiContains:
		case jqiContained:
			res = executeArrayOp(jsq, op, jb);
			break;
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
			res = makeCompare(jsq, op, jb);
			break;
		default:
			elog(ERROR, "Unknown operation");
	}

	return res;
}

static bool
recursiveExecute(JsQueryItem *jsq, JsonbValue *jb)
{
	JsQueryItem		elem;
	bool			res = false;

	check_stack_depth();

	switch(jsq->type) {
		case jqiAnd:
			jsqGetLeftArg(jsq, &elem);
			res = recursiveExecute(&elem, jb);
			if (res == true)
			{
				jsqGetRightArg(jsq, &elem);
				res = recursiveExecute(&elem, jb);
			}
			break;
		case jqiOr:
			jsqGetLeftArg(jsq, &elem);
			res = recursiveExecute(&elem, jb);
			if (res == false)
			{
				jsqGetRightArg(jsq, &elem);
				res = recursiveExecute(&elem, jb);
			}
			break;
		case jqiNot:
			jsqGetArg(jsq, &elem);
			res = !recursiveExecute(&elem, jb);
			break;
		case jqiKey:
			if (JsonbType(jb) == jbvObject) {
				JsonbValue	*v, key;

				key.type = jbvString;
				key.val.string.val = jsqGetString(jsq, &key.val.string.len);

				v = findJsonbValueFromContainer(jb->val.binary.data, JB_FOBJECT, &key);

				if (v != NULL)
				{
					jsqGetNext(jsq, &elem);
					res = recursiveExecute(&elem, v);
					pfree(v);
				}
			}
			break;
		case jqiCurrent:
			jsqGetNext(jsq, &elem);
			if (JsonbType(jb) == jbvScalar)
			{
				JsonbIterator	*it;
				int32			r PG_USED_FOR_ASSERTS_ONLY;
				JsonbValue		v;

				it = JsonbIteratorInit(jb->val.binary.data);

				r = JsonbIteratorNext(&it, &v, true);
				Assert(r == WJB_BEGIN_ARRAY);
				Assert(v.val.array.rawScalar == 1);
				Assert(v.val.array.nElems == 1);

				r = JsonbIteratorNext(&it, &v, true);
				Assert(r == WJB_ELEM);

				res = recursiveExecute(&elem, &v);
			}
			else
			{
				res = recursiveExecute(&elem, jb);
			}
			break;
		case jqiAny:
			jsqGetNext(jsq, &elem);
			if (recursiveExecute(&elem, jb))
				res = true;
			else if (jb->type == jbvBinary)
				res = recursiveAny(&elem, jb);
			break;
		case jqiAll:
			jsqGetNext(jsq, &elem);
			if ((res = recursiveExecute(&elem, jb)) == true)
			{
				if (jb->type == jbvBinary)
					res = recursiveAll(&elem, jb);
			}
			break;
		case jqiAnyArray:
		case jqiAllArray:
			if (JsonbType(jb) == jbvArray)
			{
				JsonbIterator	*it;
				int32			r;
				JsonbValue		v;

				jsqGetNext(jsq, &elem);
				it = JsonbIteratorInit(jb->val.binary.data);

				if (jsq->type == jqiAllArray)
					res = true;

				while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
				{
					if (r == WJB_ELEM)
					{
						res = recursiveExecute(&elem, &v);

						if (jsq->type == jqiAnyArray)
						{
							if (res == true)
								break;
						}
						else if (jsq->type == jqiAllArray)
						{
							if (res == false)
								break;
						}
					}
				}
			}
			break;
		case jqiAnyKey:
		case jqiAllKey:
			if (JsonbType(jb) == jbvObject)
			{
				JsonbIterator	*it;
				int32			r;
				JsonbValue		v;

				jsqGetNext(jsq, &elem);
				it = JsonbIteratorInit(jb->val.binary.data);

				if (jsq->type == jqiAllKey)
					res = true;

				while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
				{
					if (r == WJB_VALUE)
					{
						res = recursiveExecute(&elem, &v);

						if (jsq->type == jqiAnyKey)
						{
							if (res == true)
								break;
						}
						else if (jsq->type == jqiAllKey)
						{
							if (res == false)
								break;
						}
					}
				}
			}
			break;
		case jqiEqual:
		case jqiIn:
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
		case jqiContains:
		case jqiContained:
		case jqiOverlap:
			jsqGetArg(jsq, &elem);
			res = executeExpr(&elem, jsq->type, jb);
			break;
		case jqiLength:
		{
			JsonbValue size;
			jsqGetNext(jsq, &elem);
			if (JsonbSize(jb, &size))
			{
				res = recursiveExecute(&elem, &size);
				pfree(size.val.numeric);
			}
			break;
		}
		case jqiIs:
			if (JsonbType(jb) == jbvScalar)
			{
				JsonbIterator	*it;
				int32			r PG_USED_FOR_ASSERTS_ONLY;
				JsonbValue		v;

				it = JsonbIteratorInit(jb->val.binary.data);

				r = JsonbIteratorNext(&it, &v, true);
				Assert(r == WJB_BEGIN_ARRAY);
				Assert(v.val.array.rawScalar == 1);
				Assert(v.val.array.nElems == 1);

				r = JsonbIteratorNext(&it, &v, true);
				Assert(r == WJB_ELEM);

				res = (jsqGetIsType(jsq) == JsonbType(&v));
			}
			else
			{
				res = (jsqGetIsType(jsq) == JsonbType(jb));
			}
			break;
		default:
			elog(ERROR,"Wrong state: %d", jsq->type);
	}

	return res;
}

PG_FUNCTION_INFO_V1(jsquery_json_exec);
Datum
jsquery_json_exec(PG_FUNCTION_ARGS)
{
	JsQuery			*jq = PG_GETARG_JSQUERY(0);
	Jsonb			*jb = PG_GETARG_JSONB(1);
	bool			res;
	JsonbValue		jbv;
	JsQueryItem	jsq;

	JsonValueInitBinary(&jbv, &jb->root);

	jsqInit(&jsq, jq);

	res = recursiveExecute(&jsq, &jbv);

	PG_FREE_IF_COPY(jq, 0);
	PG_FREE_IF_COPY_JSONB(jb, 1);

	PG_RETURN_BOOL(res);
}

PG_FUNCTION_INFO_V1(json_jsquery_exec);
Datum
json_jsquery_exec(PG_FUNCTION_ARGS)
{
	Jsonb			*jb = PG_GETARG_JSONB(0);
	JsQuery			*jq = PG_GETARG_JSQUERY(1);
	bool			res;
	JsonbValue		jbv;
	JsQueryItem	jsq;

	JsonValueInitBinary(&jbv, &jb->root);

	jsqInit(&jsq, jq);

	res = recursiveExecute(&jsq, &jbv);

	PG_FREE_IF_COPY_JSONB(jb, 0);
	PG_FREE_IF_COPY(jq, 1);

	PG_RETURN_BOOL(res);
}

static int
compareJsQuery(JsQueryItem *v1, JsQueryItem *v2)
{
	JsQueryItem	elem1, elem2;
	int32			res = 0;

	check_stack_depth();

	if (v1->type != v2->type)
		return (v1->type > v2->type) ? 1 : -1;

	switch(v1->type)
	{
		case jqiNull:
		case jqiCurrent:
		case jqiLength:
		case jqiAny:
		case jqiAnyArray:
		case jqiAnyKey:
		case jqiAll:
		case jqiAllArray:
		case jqiAllKey:
			break;
		case jqiKey:
		case jqiString:
			{
				int32 len1, len2;
				char *s1, *s2;

				s1 = jsqGetString(v1, &len1);
				s2 = jsqGetString(v2, &len2);

				if (len1 != len2)
					res = (len1 > len2) ? 1 : -1;
				else
					res = memcmp(s1, s2, len1);
			}
			break;
		case jqiNumeric:
			res = compareNumeric(jsqGetNumeric(v1), jsqGetNumeric(v2));
			break;
		case jqiBool:
			if (jsqGetBool(v1) != jsqGetBool(v2))
				res = (jsqGetBool(v1) > jsqGetBool(v2)) ? 1 : -1;
			break;
		case jqiArray:
			if (v1->array.nelems != v2->array.nelems)
				res = (v1->array.nelems > v2->array.nelems) ? 1 : -1;

			while(res == 0 && jsqIterateArray(v1, &elem1) && jsqIterateArray(v2, &elem2))
				res = compareJsQuery(&elem1, &elem2);
			break;
		case jqiAnd:
		case jqiOr:
			jsqGetLeftArg(v1, &elem1);
			jsqGetLeftArg(v2, &elem2);

			res = compareJsQuery(&elem1, &elem2);

			if (res == 0)
			{
				jsqGetRightArg(v1, &elem1);
				jsqGetRightArg(v2, &elem2);

				res = compareJsQuery(&elem1, &elem2);
			}
			break;
		case jqiEqual:
		case jqiIn:
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
		case jqiContains:
		case jqiContained:
		case jqiOverlap:
		case jqiNot:
			jsqGetArg(v1, &elem1);
			jsqGetArg(v2, &elem2);

			res = compareJsQuery(&elem1, &elem2);
			break;
		default:
			elog(ERROR, "Unknown JsQueryItem type: %d", v1->type);
	}

	if (res == 0)
	{
		if (jsqGetNext(v1, &elem1))
		{
			if (jsqGetNext(v2, &elem2))
				res = compareJsQuery(&elem1, &elem2);
			else
				res = 1;
		}
		else if (jsqGetNext(v2, &elem2))
		{
			res = -1;
		}
	}

	return res;
}

PG_FUNCTION_INFO_V1(jsquery_cmp);
Datum
jsquery_cmp(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_INT32(res);
}

PG_FUNCTION_INFO_V1(jsquery_lt);
Datum
jsquery_lt(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_BOOL(res < 0);
}

PG_FUNCTION_INFO_V1(jsquery_le);
Datum
jsquery_le(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_BOOL(res <= 0);
}

PG_FUNCTION_INFO_V1(jsquery_eq);
Datum
jsquery_eq(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_BOOL(res == 0);
}

PG_FUNCTION_INFO_V1(jsquery_ne);
Datum
jsquery_ne(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_BOOL(res != 0);
}

PG_FUNCTION_INFO_V1(jsquery_ge);
Datum
jsquery_ge(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_BOOL(res >= 0);
}

PG_FUNCTION_INFO_V1(jsquery_gt);
Datum
jsquery_gt(PG_FUNCTION_ARGS)
{
	JsQuery			*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery			*jq2 = PG_GETARG_JSQUERY(1);
	int32			res;
	JsQueryItem		v1, v2;

	jsqInit(&v1, jq1);
	jsqInit(&v2, jq2);

	res = compareJsQuery(&v1, &v2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_BOOL(res > 0);
}

static void
hashJsQuery(JsQueryItem *v, pg_crc32 *crc)
{
	JsQueryItem	elem;

	check_stack_depth();

	COMP_CRC32(*crc, &v->type, sizeof(v->type));

	switch(v->type)
	{
		case jqiNull:
			COMP_CRC32(*crc, "null", 5);
			break;
		case jqiKey:
		case jqiString:
			{
				int32	len;
				char	*s;

				s = jsqGetString(v, &len);

				if (v->type == jqiKey)
					len++; /* include trailing '\0' */
				COMP_CRC32(*crc, s, len);
			}
			break;
		case jqiNumeric:
			*crc ^= (pg_crc32)DatumGetInt32(DirectFunctionCall1(
												hash_numeric,
												PointerGetDatum(jsqGetNumeric(v))));
			break;
		case jqiBool:
			{
				bool	b = jsqGetBool(v);

				COMP_CRC32(*crc, &b, 1);
			}
			break;
		case jqiArray:
			COMP_CRC32(*crc, &v->array.nelems, sizeof(v->array.nelems));
			while(jsqIterateArray(v, &elem))
				hashJsQuery(&elem, crc);
			break;
		case jqiAnd:
		case jqiOr:
			jsqGetLeftArg(v, &elem);
			hashJsQuery(&elem, crc);
			jsqGetRightArg(v, &elem);
			hashJsQuery(&elem, crc);
			break;
		case jqiNot:
		case jqiEqual:
		case jqiIn:
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
		case jqiContains:
		case jqiContained:
		case jqiOverlap:
			jsqGetArg(v, &elem);
			hashJsQuery(&elem, crc);
			break;
		case jqiCurrent:
		case jqiLength:
		case jqiAny:
		case jqiAnyArray:
		case jqiAnyKey:
		case jqiAll:
		case jqiAllArray:
		case jqiAllKey:
			break;
		default:
			elog(ERROR, "Unknown JsQueryItem type: %d", v->type);
	}
}

PG_FUNCTION_INFO_V1(jsquery_hash);
Datum
jsquery_hash(PG_FUNCTION_ARGS)
{
	JsQuery			*jq = PG_GETARG_JSQUERY(0);
	JsQueryItem		v;
	pg_crc32		res;

	INIT_CRC32(res);
	jsqInit(&v, jq);
	hashJsQuery(&v, &res);
	FIN_CRC32(res);

	PG_FREE_IF_COPY(jq, 0);

	PG_RETURN_INT32(res);
}

