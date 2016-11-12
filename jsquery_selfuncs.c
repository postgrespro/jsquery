/*-------------------------------------------------------------------------
 *
 * jsquery_selfuncs.c
 *     Functions for selectivity estimation of jsquery operators
 *
 * Copyright (c) 2016, PostgreSQL Global Development Group
 * Author: Nikita Glukhov
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_selfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "catalog/pg_operator.h"
#include "utils/json_selfuncs.h"
#include "utils/selfuncs.h"

#include "jsquery.h"

#include <math.h>

/* Default selectivity constant for "@@" operator */
#define DEFAULT_JSQUERY_SEL 0.005

static Selectivity jsqSelectivity(JsonPathStats stats,
								  JsQueryItem *jsq,
								  JsQueryItem *jsqLeftArg);

static JsonbValue *
JsQueryItemToJsonbValue(JsQueryItem *jsq, JsonbValue *jbv)
{
	switch (jsq->type)
	{
		case jqiNull:
			jbv->type = jbvNull;
			break;

		case jqiString:
			jbv->type = jbvString;
			jbv->val.string.val = jsqGetString(jsq, &jbv->val.string.len);
			break;

		case jqiBool:
			jbv->type = jbvBool;
			jbv->val.boolean = jsqGetBool(jsq);
			break;

		case jqiNumeric:
			jbv->type = jbvNumeric;
			jbv->val.numeric = jsqGetNumeric(jsq);
			break;

		default:
			elog(ERROR, "Invalid jsquery scalar type %d", jsq->type);
			break;
	}

	return jbv;
}

static Selectivity
jsqSelectivityScalarCmp(JsonPathStats stats, JsQueryItem *jsq, int32 op)
{
	JsonbValue	jbv;
	Oid			opr;

	if (jsq->type != jqiNumeric) /* TODO other types support */
		return 0.0;

	if (jsq->type == jqiAny)
		return jsonPathStatsGetFreq(stats, DEFAULT_JSQUERY_SEL);

	JsQueryItemToJsonbValue(jsq, &jbv);

	switch (op)
	{
		case jqiEqual:
			opr = JsonbEqOperator;
			break;
		case jqiLess:
			opr = JsonbLtOperator;
			break;
		case jqiGreater:
			opr = JsonbGtOperator;
			break;
		case jqiLessOrEqual:
			opr = JsonbLeOperator;
			break;
		case jqiGreaterOrEqual:
			opr = JsonbGeOperator;
			break;
		default:
			elog(ERROR, "Unknown jsquery operation %d", op);
	}

	return jsonSelectivity(stats, JsonbGetDatum(JsonbValueToJsonb(&jbv)), opr);
}

static Selectivity
jsqSelectivityScalarEq(JsonPathStats stats, JsQueryItem *jsq)
{
	JsonbValue	jbv;
	Datum		jsonb;

	if (jsq->type == jqiAny)
		return jsonPathStatsGetFreq(stats, DEFAULT_JSQUERY_SEL);

	jsonb = JsonbGetDatum(JsonbValueToJsonb(
										JsQueryItemToJsonbValue(jsq, &jbv)));

	return jsonSelectivity(stats, jsonb, JsonbEqOperator);
}

static Selectivity
jsqSelectivityArrayEq(JsonPathStats stats, JsQueryItem *jsq)
{
	JsonbParseState	   *ps = NULL;
	JsQueryItem			elem;
	JsonbValue		   *jbv;

	if (jsq->type != jqiArray)
		return 0.0;

	pushJsonbValue(&ps, WJB_BEGIN_ARRAY, NULL);

	for (jsqIterateInit(jsq); jsqIterateArray(jsq, &elem);)
	{
		JsonbValue	v;

		if (elem.type == jqiAny)
			continue;

		pushJsonbValue(&ps, WJB_ELEM, JsQueryItemToJsonbValue(&elem, &v));
	}

	jbv = pushJsonbValue(&ps, WJB_END_ARRAY, NULL);

	return jsonSelectivity(stats, JsonbGetDatum(JsonbValueToJsonb(jbv)),
						   JsonbEqOperator);
}

static Selectivity
jsqSelectivityScalarIn(JsonPathStats stats, JsQueryItem *jsq)
{
	JsQueryItem	elem;
	Selectivity	res = 1.0;

	if (jsq->type != jqiArray)
		return 0.0;

	for (jsqIterateInit(jsq); res > 0.0 && jsqIterateArray(jsq, &elem);)
		res *= 1.0 - jsqSelectivityScalarEq(stats, &elem);

	return 1.0 - res;
}

static Selectivity
jsqSelectivityArrayAnyAll(JsonPathStats stats, JsQueryItem *jsq, bool any)
{
	JsonPathStats	arrstats = jsonPathStatsGetSubpath(stats, NULL, 0);
	float4 			arrfreq  = jsonPathStatsGetTypeFreq(stats, jbvArray, 0.001);
	float4 			arrsize;
	float4			arrsize0freq;
	Selectivity		res;

	if (!arrstats || arrfreq <= 0.0)
		return 0.0;

	arrsize = jsonPathStatsGetAvgArraySize(arrstats);

	if (arrsize == 0.0)
		return any ? 0.0 : arrfreq;

	res = jsqSelectivity(arrstats, jsq, NULL);

	res /= arrfreq;
	CLAMP_PROBABILITY(res);

	if (any)
		res = 1 - res;

	/* TODO use other array size stats */
	arrsize0freq = jsonPathStatsGetArrayIndexSelectivity(arrstats, 0) / arrfreq;

	res = pow(res, arrsize / arrsize0freq);

	if (any)
		res = 1 - res;

	res *= arrsize0freq;

	if (!any)
		res += 1.0 - arrsize0freq;

	return arrfreq * res;
}


static Selectivity
jsqSelectivityKeyAnyAll(JsonPathStats stats, JsQueryItem *jsq, bool any)
{
/*	float4 			objfreq  = jsonPathStatsGetTypeFreq(stats, jbvObject, 0.0); */
	JsonPathStats	keystats = NULL;
	Selectivity		res = 1.0;

	while (res > 0.0 && jsonPathStatsGetNextKeyStats(stats, &keystats, true))
	{
		Selectivity	sel = jsqSelectivity(keystats, jsq, NULL);
		res *= any ? (1.0 - sel) : sel;
	}

	if (any)
		res = 1.0 - res;

	return res;
}

static Selectivity
jsqSelectivityAnyAll(JsonPathStats stats, JsQueryItem *jsq, bool any)
{
	JsQueryItem	elem;
	Selectivity	sel;
	Selectivity	res;
	Selectivity freq = jsonPathStatsGetFreq(stats, 0.0);

	if (freq <= 0.0)
		return 0.0;

	jsqGetNext(jsq, &elem);

	sel = jsqSelectivity(stats, &elem, NULL) / freq;
	CLAMP_PROBABILITY(sel);
	//res = any ? (1.0 - sel) : sel;
	res = sel;

	if (res > 0.0 && jsonPathStatsGetTypeFreq(stats, jbvArray, 0.0) > 0.0)
	{
		sel = jsqSelectivityArrayAnyAll(stats, jsq, any) / freq;
		CLAMP_PROBABILITY(sel);
		//res *= any ? (1.0 - sel) : sel;
		res = any ?  res + sel : res * sel;
	}

	if (res > 0.0 && jsonPathStatsGetTypeFreq(stats, jbvObject, 0.0) > 0.0)
	{
		sel = jsqSelectivityKeyAnyAll(stats, jsq, any) / freq;
		CLAMP_PROBABILITY(sel);
		//res *= any ? (1.0 - sel) : sel;
		res = any ?  res + sel : res * sel;
	}

	//if (any)
	//	res = 1.0 - res;
	CLAMP_PROBABILITY(res);

	return res * freq;
}

static Selectivity
jsqSelectivityArrayOp(JsonPathStats stats, JsQueryItem *op, JsQueryItem *arg)
{
	JsQueryItem		elem;

	if (arg->type != jqiArray)
		return 0.0;

	if (op->type == jqiContains || op->type == jqiOverlap)
	{
		JsQueryItem		eq;
		Selectivity 	res = 1.0;
		bool			contains = op->type == jqiContains;

		eq.type = jqiEqual;
		eq.base = arg->base;

		for (jsqIterateInit(arg); res > 0.0 && jsqIterateArray(arg, &elem);)
		{
			Selectivity sel;
			eq.arg = arg->array.arrayPtr[arg->array.current - 1]; /* FIXME */
			sel = jsqSelectivityArrayAnyAll(stats, &eq, true);
			res *= contains ? sel : (1.0 - sel);
		}

		return contains ? res : (1.0 - res);
	}
	else
	{
		JsQueryItem	in;

		in.type = jqiIn;
		in.base = op->base;
		in.arg = op->arg;

		return jsqSelectivityArrayAnyAll(stats, &in, false);
	}
}

static Selectivity
jsqSelectivityExpr(JsonPathStats stats, JsQueryItem *expr, 
				   JsQueryItem *jsqLeftArg)
{
	JsQueryItem arg;

	jsqGetArg(expr, &arg);
	
	Assert(!jsqGetNext(&arg, NULL));
	Assert(arg.type == jqiAny || arg.type == jqiString ||
		   arg.type == jqiNumeric || arg.type == jqiNull ||
		   arg.type == jqiBool || arg.type == jqiArray);

	switch (expr->type)
	{
		case jqiEqual:
			return arg.type == jqiArray ? jsqSelectivityArrayEq(stats, &arg)
										: jsqSelectivityScalarEq(stats, &arg);

		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
			return jsqSelectivityScalarCmp(stats, &arg, expr->type);

		case jqiIn:
			return jsqSelectivityScalarIn(stats, &arg);

		case jqiOverlap:
		case jqiContains:
		case jqiContained:
			return jsqSelectivityArrayOp(stats, expr, &arg);

		default:
			elog(ERROR, "Unknown operation");
			return DEFAULT_JSQUERY_SEL;
	}
}

static Selectivity
jsqSelectivity(JsonPathStats stats, JsQueryItem *jsq, JsQueryItem *jsqLeftArg)
{
	JsQueryItem		elem;
	Selectivity		res = DEFAULT_JSQUERY_SEL;

	check_stack_depth();

	switch (jsq->type)
	{
		case jqiAnd:
			jsqGetLeftArg(jsq, &elem);
			res = jsqSelectivity(stats, &elem, jsqLeftArg);
			if (res > 0.0)
			{
				jsqGetRightArg(jsq, &elem);
				res *= jsqSelectivity(stats, &elem, jsqLeftArg);
			}
			break;

		case jqiOr:
			jsqGetLeftArg(jsq, &elem);
			res = jsqSelectivity(stats, &elem, jsqLeftArg);
			if (res < 1.0)
			{
				jsqGetRightArg(jsq, &elem);
				res = 1.0 - (1.0 - res) *
					(1.0 - jsqSelectivity(stats, &elem, jsqLeftArg));
			}
			break;

		case jqiNot:
			jsqGetArg(jsq, &elem);
			res = 1.0 - jsqSelectivity(stats, &elem, jsqLeftArg);
			break;

		case jqiKey:
		{
			int				keylen;
			char		   *key = jsqGetString(jsq, &keylen);
			JsonPathStats	keystats = jsonPathStatsGetSubpath(stats,
															   key, keylen);

			if (keystats)
			{
				jsqGetNext(jsq, &elem);
				res = jsqSelectivity(keystats, &elem, NULL);
			}
			else
				res = 0.0;

			break;
		}

		case jqiCurrent:
			jsqGetNext(jsq, &elem);
			res = jsqSelectivity(stats, &elem, jsqLeftArg);
			break;

		case jqiAnyKey:
		case jqiAllKey:
			jsqGetNext(jsq, &elem);
			res = jsqSelectivityKeyAnyAll(stats, &elem, jsq->type == jqiAnyKey);
			break;

		case jqiAnyArray:
		case jqiAllArray:
			jsqGetNext(jsq, &elem);
			res = jsqSelectivityArrayAnyAll(stats, &elem,
											jsq->type == jqiAnyArray);
			break;

		case jqiAny:
		case jqiAll:
			res = jsqSelectivityAnyAll(stats, jsq, jsq->type == jqiAny);
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
			res = jsqSelectivityExpr(stats, jsq, jsqLeftArg);
			break;

		case jqiLength:
			jsqGetNext(jsq, &elem);
			stats = jsonPathStatsGetLengthStats(stats);
			res = stats ? jsqSelectivity(stats, &elem, jsq) : 0.0;
			break;

		case jqiIs:
			res = jsonPathStatsGetTypeFreq(stats, jsqGetIsType(jsq), 0.0);
			break;

		default:
			elog(ERROR,"Wrong state: %d", jsq->type);
	}

	CLAMP_PROBABILITY(res);

	return res;
}

PG_FUNCTION_INFO_V1(jsquery_sel);

Datum
jsquery_sel(PG_FUNCTION_ARGS)
{
	PlannerInfo	   *root = (PlannerInfo *) PG_GETARG_POINTER(0);
/*	Oid				operator = PG_GETARG_OID(1); */
	List		   *args = (List *) PG_GETARG_POINTER(2);
	int				varRelid = PG_GETARG_INT32(3);
	Selectivity		sel = DEFAULT_JSQUERY_SEL;
	VariableStatData vardata;
	JsonStatData	jsdata;
	JsonPathStats	pstats;
	Node		   *other;
	bool			varonleft;
	JsQuery		   *jq;
	JsQueryItem		jsq;

	if (!get_restriction_variable(root, args, varRelid,
								  &vardata, &other, &varonleft))
		PG_RETURN_FLOAT8((float8) sel);

	if (!IsA(other, Const) || !varonleft /* FIXME l/r operator */)
		goto out;

	if (((Const *) other)->constisnull)
	{
		sel = 0.0;
		goto out;
	}

	jq = DatumGetJsQueryP(((Const *) other)->constvalue);
	jsqInit(&jsq, jq);

	if (!jsonStatsInit(&jsdata, &vardata))
		goto out;

	pstats = jsonStatsGetPathStatsStr(&jsdata, "$", 1);

	sel = pstats ? jsqSelectivity(pstats, &jsq, NULL) : 0.0;

out:
	ReleaseVariableStats(vardata);

	CLAMP_PROBABILITY(sel);

	PG_RETURN_FLOAT8((float8) sel);
}
