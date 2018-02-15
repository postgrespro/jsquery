/*-------------------------------------------------------------------------
 *
 * jsquery_op.c
 *	Functions and operations over jsquery/jsonb datatypes
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *	contrib/jsquery/jsquery_op.c
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

typedef struct ResultAccum {
	StringInfo	buf;
	bool		missAppend;
	JsonbParseState	*jbArrayState;
} ResultAccum;


static bool recursiveExecute(JsQueryItem *jsq, JsonbValue *jb, JsQueryItem *jsqLeftArg,
							 ResultAccum *ra);

static void
appendResult(ResultAccum *ra, JsonbValue *jb)
{
	if (ra == NULL || ra->missAppend == true)
		return;

	if (ra->jbArrayState == NULL)
		pushJsonbValue(&ra->jbArrayState, WJB_BEGIN_ARRAY, NULL);

	pushJsonbValue(&ra->jbArrayState, WJB_ELEM, jb);
}

static void
concatResult(ResultAccum *ra, JsonbParseState *a, JsonbParseState *b)
{
	Jsonb			*value;
	JsonbIterator	*it;
	int32			r;
	JsonbValue		v;

	Assert(a);
	Assert(b);

	ra->jbArrayState = a;

	value = JsonbValueToJsonb(pushJsonbValue(&b, WJB_END_ARRAY, NULL));
	it = JsonbIteratorInit(&value->root);

	while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
		if (r == WJB_ELEM)
			pushJsonbValue(&ra->jbArrayState, WJB_ELEM, &v);
}

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

#define jbvScalar jbvBinary
static int
JsonbType(JsonbValue *jb)
{
	int type = jb->type;

	if (jb->type == jbvBinary)
	{
		JsonbContainer	*jbc = jb->val.binary.data;

		if (jbc->header & JB_FSCALAR)
			type = jbvScalar;
		else if (jbc->header & JB_FOBJECT)
			type = jbvObject;
		else if (jbc->header & JB_FARRAY)
			type = jbvArray;
		else
			elog(ERROR, "Unknown container type: 0x%08x", jbc->header);
	}

	return type;
}

static bool
recursiveAny(JsQueryItem *jsq, JsonbValue *jb, ResultAccum *ra)
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
			/*
			 * we don't need actually store result, we may need to store whole
			 * object/array
			 */
			res = recursiveExecute(jsq, &v, NULL, ra);

			if (res == false && v.type == jbvBinary)
				res = recursiveAny(jsq, &v, ra);
		}
	}

	return res;
}

static bool
recursiveAll(JsQueryItem *jsq, JsonbValue *jb, ResultAccum *ra)
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
			/*
			 * we don't need actually store result, we may need to store whole
			 * object/array
			 */
			if ((res = recursiveExecute(jsq, &v, NULL, ra)) == true)
			{
				if (v.type == jbvBinary)
					res = recursiveAll(jsq, &v, ra);
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

	if (!(jsq->type == jqiArray && JsonbType(jb) == jbvArray))
		return false;


	it = JsonbIteratorInit(jb->val.binary.data);
	r = JsonbIteratorNext(&it, &v, true);
	Assert(r == WJB_BEGIN_ARRAY);

	if (v.val.array.nElems != jsq->array.nelems)
		return false;

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
	int32			r = 0; /* keep static analyzer quiet */
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
executeExpr(JsQueryItem *jsq, int32 op, JsonbValue *jb, JsQueryItem *jsqLeftArg)
{
	bool res = false;
	/*
	 * read arg type
	 */
	Assert(jsqGetNext(jsq, NULL) == false);
	Assert(jsq->type == jqiAny || jsq->type == jqiString || jsq->type == jqiNumeric ||
		   jsq->type == jqiNull || jsq->type == jqiBool || jsq->type == jqiArray);

	if (jsqLeftArg && jsqLeftArg->type == jqiLength)
	{
		if (JsonbType(jb) == jbvArray || JsonbType(jb) == jbvObject)
		{
			int32	length;
			JsonbIterator	*it;
			JsonbValue		v;
			int				r;

			it = JsonbIteratorInit(jb->val.binary.data);
			r = JsonbIteratorNext(&it, &v, true);
			Assert(r == WJB_BEGIN_ARRAY || r == WJB_BEGIN_OBJECT);

			length = (r == WJB_BEGIN_ARRAY) ? v.val.array.nElems : v.val.object.nPairs;

			v.type = jbvNumeric;
			v.val.numeric = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(length)));

			switch(op)
			{
				case jqiEqual:
				case jqiLess:
				case jqiGreater:
				case jqiLessOrEqual:
				case jqiGreaterOrEqual:
					res = makeCompare(jsq, op, &v);
					break;
				case jqiIn:
					res = checkScalarIn(jsq, &v);
					break;
				case jqiOverlap:
				case jqiContains:
				case jqiContained:
					break;
				default:
					elog(ERROR, "Unknown operation");
			}
		}
	}
	else
	{
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
	}

	return res;
}

static bool
recursiveExecute(JsQueryItem *jsq, JsonbValue *jb, JsQueryItem *jsqLeftArg,
				 ResultAccum *ra)
{
	JsQueryItem		elem;
	bool			res = false;

	check_stack_depth();

	switch(jsq->type) {
		case jqiAnd:
			{
				JsonbParseState *saveJbArrayState = NULL;

				jsqGetLeftArg(jsq, &elem);
				if (ra && ra->missAppend == false)
				{
					saveJbArrayState = ra->jbArrayState;
					ra->jbArrayState = NULL;
				}

				res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
				if (res == true)
				{
					jsqGetRightArg(jsq, &elem);
					res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
				}

				if (ra && ra->missAppend == false)
				{
					if (res == true)
					{
						if (saveJbArrayState != NULL)
							/* append args lists to current */
							concatResult(ra, saveJbArrayState, ra->jbArrayState);
					}
					else
						ra->jbArrayState = saveJbArrayState;
				}

				break;
			}
		case jqiOr:
			jsqGetLeftArg(jsq, &elem);
			res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
			if (res == false)
			{
				jsqGetRightArg(jsq, &elem);
				res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
			}
			break;
		case jqiNot:
			{
				bool saveMissAppend = (ra) ? ra->missAppend : true;

				jsqGetArg(jsq, &elem);
				if (ra)
					ra->missAppend = true;
				res = !recursiveExecute(&elem, jb, jsqLeftArg, ra);
				if (ra)
					ra->missAppend = saveMissAppend;

				break;
			}
		case jqiKey:
			if (JsonbType(jb) == jbvObject) {
				JsonbValue	*v, key;

				key.type = jbvString;
				key.val.string.val = jsqGetString(jsq, &key.val.string.len);

				v = findJsonbValueFromContainer(jb->val.binary.data, JB_FOBJECT, &key);

				if (v != NULL)
				{
					if (jsqGetNext(jsq, &elem) == false)
					{
						appendResult(ra, v);
						res = true;
					}
					else
						res = recursiveExecute(&elem, v, NULL, ra);
					pfree(v);
				}
			}
			break;
		case jqiCurrent:
			if (jsqGetNext(jsq, &elem) == false)
			{
				appendResult(ra, jb);
				res = true;
			}
			else if (JsonbType(jb) == jbvScalar)
			{
				JsonbIterator	*it;
				int32			r;
				JsonbValue		v;

				it = JsonbIteratorInit(jb->val.binary.data);

				r = JsonbIteratorNext(&it, &v, true);
				Assert(r == WJB_BEGIN_ARRAY);
				Assert(v.val.array.rawScalar == 1);
				Assert(v.val.array.nElems == 1);

				r = JsonbIteratorNext(&it, &v, true);
				Assert(r == WJB_ELEM);

				res = recursiveExecute(&elem, &v, jsqLeftArg, ra);
			}
			else
			{
				res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
			}
			break;
		case jqiAny:
			if (jsqGetNext(jsq, &elem) == false)
			{
				res = true;
				appendResult(ra, jb);
			}
			else if (recursiveExecute(&elem, jb, NULL, ra))
				res = true;
			else if (jb->type == jbvBinary)
				res = recursiveAny(&elem, jb, ra);
			break;
		case jqiAll:
			if (jsqGetNext(jsq, &elem) == false)
			{
				res = true;
				appendResult(ra, jb);
			}
			if ((res = recursiveExecute(&elem, jb, NULL, ra)) == true)
			{
				if (jb->type == jbvBinary)
					res = recursiveAll(&elem, jb, ra);
			}
			break;
		case jqiAnyArray:
		case jqiAllArray:
			if (JsonbType(jb) == jbvArray)
			{
				JsonbIterator	*it;
				int32			r;
				JsonbValue		v;
				bool			anyres = false;
				bool			hasNext;

				hasNext = jsqGetNext(jsq, &elem);
				it = JsonbIteratorInit(jb->val.binary.data);

				if (hasNext == false)
				{
					res = true;

					while(ra && (r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
						if (r == WJB_ELEM)
							appendResult(ra, &v);

					break;
				}

				if (jsq->type == jqiAllArray)
					res = true;

				while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
				{
					if (r == WJB_ELEM)
					{
						res = recursiveExecute(&elem, &v, NULL, ra);

						if (jsq->type == jqiAnyArray)
						{
							anyres |= res;
							if (res == true &&
								(ra == NULL || ra->missAppend == true))
								break;
						}
						else if (jsq->type == jqiAllArray)
						{
							if (res == false)
								break;
						}
					}
				}

				if (jsq->type == jqiAnyArray)
					res = anyres;
			}
			break;
		case jqiIndexArray:
			if (JsonbType(jb) == jbvArray)
			{
				JsonbValue		*v;

				v = getIthJsonbValueFromContainer(jb->val.binary.data,
												  jsq->arrayIndex);

				if (v)
				{
					if (jsqGetNext(jsq, &elem) == false)
					{
						res = true;
						appendResult(ra, v);
					}
					else
						res = recursiveExecute(&elem, v, NULL, ra);
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
				bool			anyres = false;
				bool			hasNext;

				hasNext = jsqGetNext(jsq, &elem);
				it = JsonbIteratorInit(jb->val.binary.data);

				if (hasNext == false)
				{
					res = true;

					while(ra && (r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
						if (r == WJB_VALUE)
							appendResult(ra, &v);

					break;
				}

				if (jsq->type == jqiAllKey)
					res = true;

				while((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
				{
					if (r == WJB_VALUE)
					{
						res = recursiveExecute(&elem, &v, NULL, ra);

						if (jsq->type == jqiAnyKey)
						{
							anyres |= res;
							if (res == true &&
								(ra == NULL || ra->missAppend == true))
								break;
						}
						else if (jsq->type == jqiAllKey)
						{
							if (res == false)
								break;
						}
					}
				}

				if (jsq->type == jqiAnyKey)
					res = anyres;
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
			res = executeExpr(&elem, jsq->type, jb, jsqLeftArg);
			break;
		case jqiLength:
			jsqGetNext(jsq, &elem);
			res = recursiveExecute(&elem, jb, jsq, ra);
			break;
		case jqiIs:
			if (JsonbType(jb) == jbvScalar)
			{
				JsonbIterator	*it;
				int32			r;
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
		case jqiFilter:
			{
				bool saveMissAppend = (ra) ? ra->missAppend : true;

				jsqGetArg(jsq, &elem);
				if (ra)
					ra->missAppend = true;
				res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
				if (ra)
					ra->missAppend = saveMissAppend;
				if (res) {
					if (jsqGetNext(jsq, &elem) == false)
					{
						appendResult(ra, jb);
						res = true;
					}
					else
						res = recursiveExecute(&elem, jb, jsqLeftArg, ra);
				}
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
	JsQueryItem		jsq;

	jbv.type = jbvBinary;
	jbv.val.binary.data = &jb->root;
	jbv.val.binary.len = VARSIZE_ANY_EXHDR(jb);

	jsqInit(&jsq, jq);

	res = recursiveExecute(&jsq, &jbv, NULL, NULL);

	PG_FREE_IF_COPY(jq, 0);
	PG_FREE_IF_COPY(jb, 1);

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

	jbv.type = jbvBinary;
	jbv.val.binary.data = &jb->root;
	jbv.val.binary.len = VARSIZE_ANY_EXHDR(jb);

	jsqInit(&jsq, jq);

	res = recursiveExecute(&jsq, &jbv, NULL, NULL);

	PG_FREE_IF_COPY(jb, 0);
	PG_FREE_IF_COPY(jq, 1);

	PG_RETURN_BOOL(res);
}

PG_FUNCTION_INFO_V1(json_jsquery_filter);
Datum
json_jsquery_filter(PG_FUNCTION_ARGS)
{
	Jsonb			*jb = PG_GETARG_JSONB(0);
	JsQuery			*jq = PG_GETARG_JSQUERY(1);
	Jsonb			*res = NULL;
	JsonbValue		jbv;
	JsQueryItem		jsq;
	ResultAccum		ra;

	jbv.type = jbvBinary;
	jbv.val.binary.data = &jb->root;
	jbv.val.binary.len = VARSIZE_ANY_EXHDR(jb);

	jsqInit(&jsq, jq);
	memset(&ra, 0, sizeof(ra));

	recursiveExecute(&jsq, &jbv, NULL, &ra);

	if (ra.jbArrayState)
	{
		res = JsonbValueToJsonb(
				pushJsonbValue(&ra.jbArrayState, WJB_END_ARRAY, NULL)
		);
	}

	PG_FREE_IF_COPY(jb, 0);
	PG_FREE_IF_COPY(jq, 1);

	if (res)
		PG_RETURN_JSONB(res);
	else
		PG_RETURN_NULL();
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
		case jqiFilter:
			break;
		case jqiIndexArray:
			if (v1->arrayIndex != v2->arrayIndex)
				res = (v1->arrayIndex > v2->arrayIndex) ? 1 : -1;
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
		case jqiFilter:
			break;
		case jqiIndexArray:
			COMP_CRC32(*crc, &v->arrayIndex, sizeof(v->arrayIndex));
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

