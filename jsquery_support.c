/*-------------------------------------------------------------------------
 *
 * jsquery_support.c
 *	Functions and operations to support jsquery
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *	contrib/jsquery/jsquery_support.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "jsquery.h"

#define read_byte(v, b, p) do {		\
	(v) = *(uint8*)((b) + (p));		\
	(p) += 1;						\
} while(0)							\

#define read_int32(v, b, p) do {	\
	(v) = *(uint32*)((b) + (p));	\
	(p) += sizeof(int32);			\
} while(0)							\

void
alignStringInfoInt(StringInfo buf)
{
	switch(INTALIGN(buf->len) - buf->len)
	{
		case 3:
			appendStringInfoCharMacro(buf, 0);
		case 2:
			appendStringInfoCharMacro(buf, 0);
		case 1:
			appendStringInfoCharMacro(buf, 0);
		default:
			break;
	}
}

void
jsqInit(JsQueryItem *v, JsQuery *js)
{
	jsqInitByBuffer(v, VARDATA(js), 0);
}

void
jsqInitByBuffer(JsQueryItem *v, char *base, int32 pos)
{
	v->base = base;

	read_byte(v->type, base, pos);

	v->hint = v->type & JSQ_HINT_MASK;
	v->type &= ~JSQ_HINT_MASK;

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
			read_int32(v->value.datalen, base, pos);
			/* follow next */
		case jqiNumeric:
		case jqiBool:
		case jqiIs:
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
			abort();
			elog(ERROR, "Unknown type: %d", v->type);
	}
}

void
jsqGetArg(JsQueryItem *v, JsQueryItem *a)
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
jsqGetNext(JsQueryItem *v, JsQueryItem *a)
{
	if (v->nextPos > 0)
	{
		Assert(
			v->type == jqiKey ||
			v->type == jqiAny ||
			v->type == jqiAnyArray ||
			v->type == jqiAnyKey ||
			v->type == jqiAll ||
			v->type == jqiAllArray ||
			v->type == jqiAllKey ||
			v->type == jqiCurrent ||
			v->type == jqiLength
		);

		if (a)
			jsqInitByBuffer(a, v->base, v->nextPos);
		return true;
	}

	return false;
}

void
jsqGetLeftArg(JsQueryItem *v, JsQueryItem *a)
{
	Assert(
		v->type == jqiAnd ||
		v->type == jqiOr
	);

	jsqInitByBuffer(a, v->base, v->args.left);
}

void
jsqGetRightArg(JsQueryItem *v, JsQueryItem *a)
{
	Assert(
		v->type == jqiAnd ||
		v->type == jqiOr
	);

	jsqInitByBuffer(a, v->base, v->args.right);
}

bool
jsqGetBool(JsQueryItem *v)
{
	Assert(v->type == jqiBool);

	return (bool)*v->value.data;
}

Numeric
jsqGetNumeric(JsQueryItem *v)
{
	Assert(v->type == jqiNumeric);

	return (Numeric)v->value.data;
}

int32
jsqGetIsType(JsQueryItem *v)
{
	Assert(v->type = jqiIs);

	return  (int32)*v->value.data;
}

char*
jsqGetString(JsQueryItem *v, int32 *len)
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
jsqIterateInit(JsQueryItem *v)
{
	Assert(v->type == jqiArray);

	v->array.current = 0;
}

bool
jsqIterateArray(JsQueryItem *v, JsQueryItem *e)
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

