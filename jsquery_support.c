/*-------------------------------------------------------------------------
 *
 * jsquery_support.c
 *     Functions and operations to support jsquery  
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_support.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "jsquery.h"

void
jsqInit(JsQueryItemR *v, JsQuery *js)
{
	jsqInitByBuffer(v, VARDATA(js), 0);
}

void
jsqInitByBuffer(JsQueryItemR *v, char *base, int32 pos)
{
	v->base = base;

	read_byte(v->type, base, pos);

	switch(INTALIGN(pos) - pos)
	{
		case 3: pos++;
		case 2: pos++;
		case 1: pos++;
		default: break;
	}

	read_int32(v->nextPos, base, pos);

	switch(v->type)
	{
		case jqiNull:
		case jqiAny:
		case jqiCurrent:
		case jqiAnyArray:
		case jqiAnyKey:
			break;
		case jqiKey:
		case jqiString:
			read_int32(v->value.datalen, base, pos);
			/* follow next */
		case jqiNumeric:
		case jqiBool:
			v->value.data = base + pos;
			break;
		case jqiArray:
			read_int32(v->array.nelems, base, pos);
			v->array.current = 0;
			v->array.arrayPtr = (int32*)(base + pos);
			break;
		case jqiAnd:
		case jqiOr:
			read_int32(v->args.left, base, pos);
			read_int32(v->args.right, base, pos);
			break;
		case jqiEqual:
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
		case jqiContains:
		case jqiContained:
		case jqiOverlap:
		case jqiIn:
		case jqiNot:
			read_int32(v->arg, base, pos);
			break;
		default:
			elog(ERROR, "Unknown JsQueryItem type: %d", v->type);
	}
}

void
jsqGetArg(JsQueryItemR *v, JsQueryItemR *a)
{
	Assert(
		v->type == jqiEqual ||
		v->type == jqiLess ||
		v->type == jqiGreater ||
		v->type == jqiLessOrEqual ||
		v->type == jqiGreaterOrEqual ||
		v->type == jqiContains ||
		v->type == jqiContained ||
		v->type == jqiOverlap ||
		v->type == jqiIn ||
		v->type == jqiNot
	);

	jsqInitByBuffer(a, v->base, v->arg);
}

bool
jsqGetNext(JsQueryItemR *v, JsQueryItemR *a)
{
	if (v->nextPos > 0)
	{
		Assert(
			v->type == jqiKey ||
			v->type == jqiAny ||
			v->type == jqiAnyArray ||
			v->type == jqiAnyKey ||
			v->type == jqiCurrent
		);

		if (a)
			jsqInitByBuffer(a, v->base, v->nextPos);
		return true;
	}

	return false;
}

void
jsqGetLeftArg(JsQueryItemR *v, JsQueryItemR *a)
{
	Assert(
		v->type == jqiAnd ||
		v->type == jqiOr
	);

	jsqInitByBuffer(a, v->base, v->args.left);
}

void
jsqGetRightArg(JsQueryItemR *v, JsQueryItemR *a)
{
	Assert(
		v->type == jqiAnd ||
		v->type == jqiOr
	);

	jsqInitByBuffer(a, v->base, v->args.right);
}

bool
jsqGetBool(JsQueryItemR *v)
{
	Assert(v->type == jqiBool);

	return (bool)*v->value.data;
}

Numeric
jsqGetNumeric(JsQueryItemR *v)
{
	Assert(v->type == jqiNumeric);

	return (Numeric)v->value.data;
}

char*
jsqGetString(JsQueryItemR *v, int32 *len)
{
	Assert(
		v->type == jqiKey ||
		v->type == jqiString
	);

	if (len)
		*len = v->value.datalen;
	return v->value.data;
}

void
jsqIterateInit(JsQueryItemR *v)
{
	Assert(v->type == jqiArray);

	v->array.current = 0;
}

bool
jsqIterateArray(JsQueryItemR *v, JsQueryItemR *e)
{
	Assert(v->type == jqiArray);

	if (v->array.current < v->array.nelems)
	{
		jsqInitByBuffer(e, v->base, v->array.arrayPtr[v->array.current]);
		v->array.current++;
		return true;
	}
	else
	{
		return false;
	}
}

