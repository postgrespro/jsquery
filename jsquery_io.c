/*-------------------------------------------------------------------------
 *
 * jsquery_io.c
 *	I/O functions for jsquery datatype
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 2016-2024, Postgres Professional
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *	contrib/jsquery/jsquery_io.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/json.h"

#include "jsquery.h"

PG_MODULE_MAGIC;

static int
flattenJsQueryParseItem(StringInfo buf, JsQueryParseItem *item, bool onlyCurrentInPath)
{
	int32	pos = buf->len - VARHDRSZ; /* position from begining of jsquery data */
	int32	chld, next;

	check_stack_depth();

	Assert((item->type & item->hint) == 0);
	Assert((item->type & JSQ_HINT_MASK) == 0);

	appendStringInfoChar(buf, (char)(item->type | item->hint));
	alignStringInfoInt(buf);

	next = (item->next) ? buf->len : 0;
	appendBinaryStringInfo(buf, (char*)&next /* fake value */, sizeof(next));

	switch(item->type)
	{
		case jqiKey:
			if (onlyCurrentInPath)
				elog(ERROR,"Array length should be last in path");
			/* fall through */
		case jqiString:
			appendBinaryStringInfo(buf, (char*)&item->string.len, sizeof(item->string.len));
			appendBinaryStringInfo(buf, item->string.val, item->string.len);
			appendStringInfoChar(buf, '\0');
			break;
		case jqiNumeric:
			appendBinaryStringInfo(buf, (char*)item->numeric, VARSIZE(item->numeric));
			break;
		case jqiBool:
			appendBinaryStringInfo(buf, (char*)&item->boolean, sizeof(item->boolean));
			break;
		case jqiIs:
			appendBinaryStringInfo(buf, (char*)&item->isType, sizeof(item->isType));
			break;
		case jqiArray:
			{
				int32	i, arrayStart;

				appendBinaryStringInfo(buf, (char*)&item->array.nelems, sizeof(item->array.nelems));
				arrayStart = buf->len;

				/* reserve place for "pointers" to array's elements */
				for(i=0; i<item->array.nelems; i++)
					appendBinaryStringInfo(buf, (char*)&i /* fake value */, sizeof(i));

				for(i=0; i<item->array.nelems; i++)
				{
					chld = flattenJsQueryParseItem(buf, item->array.elems[i], onlyCurrentInPath);
					*(int32*)(buf->data + arrayStart + i * sizeof(i)) = chld;
				}

			}
			break;
		case jqiAnd:
		case jqiOr:
			{
				int32	left, right;

				left = buf->len;
				appendBinaryStringInfo(buf, (char*)&left /* fake value */, sizeof(left));
				right = buf->len;
				appendBinaryStringInfo(buf, (char*)&right /* fake value */, sizeof(right));

				chld = flattenJsQueryParseItem(buf, item->args.left, onlyCurrentInPath);
				*(int32*)(buf->data + left) = chld;
				chld = flattenJsQueryParseItem(buf, item->args.right, onlyCurrentInPath);
				*(int32*)(buf->data + right) = chld;
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
		case jqiFilter:
			{
				int32 arg;

				arg = buf->len;
				appendBinaryStringInfo(buf, (char*)&arg /* fake value */, sizeof(arg));

				chld = flattenJsQueryParseItem(buf, item->arg, onlyCurrentInPath);
				*(int32*)(buf->data + arg) = chld;
			}
			break;
		case jqiIndexArray:
			appendBinaryStringInfo(buf, (char*)&item->arrayIndex,
								   sizeof(item->arrayIndex));
			/* FALLTHROUGH */ /* keep svace quiet */
		case jqiAny:
		case jqiAnyArray:
		case jqiAnyKey:
		case jqiAll:
		case jqiAllArray:
		case jqiAllKey:
			if (onlyCurrentInPath)
				elog(ERROR,"Array length should be last in path");
		case jqiCurrent:
		case jqiNull:
			break;
		case jqiLength:
			onlyCurrentInPath = true;
			break;
		default:
			elog(ERROR, "Unknown type: %d", item->type);
	}

	if (item->next)
	{
		chld = flattenJsQueryParseItem(buf, item->next, onlyCurrentInPath);
		*(int32*)(buf->data + next) = chld;
	}

	return  pos;
}

PG_FUNCTION_INFO_V1(jsquery_in);
Datum
jsquery_in(PG_FUNCTION_ARGS)
{
	char				*in = PG_GETARG_CSTRING(0);
	int32				len = strlen(in);
	JsQueryParseItem	*jsquery = parsejsquery(in, len);
	JsQuery				*res;
	StringInfoData		buf;

	initStringInfo(&buf);
	enlargeStringInfo(&buf, 4 * len /* estimation */);

	appendStringInfoSpaces(&buf, VARHDRSZ);

	flattenJsQueryParseItem(&buf, jsquery, false);

	res = (JsQuery*)buf.data;
	SET_VARSIZE(res, buf.len);

	PG_RETURN_JSQUERY(res);
}

static void
printHint(StringInfo buf, JsQueryHint hint)
{
	switch(hint)
	{
		case jsqForceIndex:
			appendStringInfoString(buf, " /*-- index */ ");
			break;
		case jsqNoIndex:
			appendStringInfoString(buf, " /*-- noindex */ ");
			break;
		case jsqIndexDefault:
			break;
		default:
			elog(ERROR, "Unknown hint: %d", hint);
	}
}

static void
printOperation(StringInfo buf, JsQueryItemType type)
{
	switch(type)
	{
		case jqiAnd:
			appendBinaryStringInfo(buf, " AND ", 5); break;
		case jqiOr:
			appendBinaryStringInfo(buf, " OR ", 4); break;
		case jqiEqual:
			appendBinaryStringInfo(buf, " = ", 3); break;
		case jqiLess:
			appendBinaryStringInfo(buf, " < ", 3); break;
		case jqiGreater:
			appendBinaryStringInfo(buf, " > ", 3); break;
		case jqiLessOrEqual:
			appendBinaryStringInfo(buf, " <= ", 4); break;
		case jqiGreaterOrEqual:
			appendBinaryStringInfo(buf, " >= ", 4); break;
		case jqiContains:
			appendBinaryStringInfo(buf, " @> ", 4); break;
		case jqiContained:
			appendBinaryStringInfo(buf, " <@ ", 4); break;
		case jqiOverlap:
			appendBinaryStringInfo(buf, " && ", 4); break;
		default:
			elog(ERROR, "Unknown type: %d", type);
	}
}

static void
printJsQueryItem(StringInfo buf, JsQueryItem *v, bool inKey, bool printBracketes)
{
	JsQueryItem	elem;
	bool		first = true;

	check_stack_depth();

	printHint(buf, v->hint);

	switch(v->type)
	{
		case jqiNull:
			appendStringInfoString(buf, "null");
			break;
		case jqiKey:
			if (inKey)
				appendStringInfoChar(buf, '.');
			/* fall through */
			/* follow next */
		case jqiString:
			escape_json(buf, jsqGetString(v, NULL));
			break;
		case jqiNumeric:
			appendStringInfoString(buf,
									DatumGetCString(DirectFunctionCall1(numeric_out,
										PointerGetDatum(jsqGetNumeric(v)))));
			break;
		case jqiBool:
			if (jsqGetBool(v))
				appendBinaryStringInfo(buf, "true", 4);
			else
				appendBinaryStringInfo(buf, "false", 5);
			break;
		case jqiIs:
			appendBinaryStringInfo(buf, " IS ", 4);
			switch(jsqGetIsType(v))
			{
				case jbvString:
					appendBinaryStringInfo(buf, "STRING", 6);
					break;
				case jbvNumeric:
					appendBinaryStringInfo(buf, "NUMERIC", 7);
					break;
				case jbvBool:
					appendBinaryStringInfo(buf, "BOOLEAN", 7);
					break;
				case jbvArray:
					appendBinaryStringInfo(buf, "ARRAY", 5);
					break;
				case jbvObject:
					appendBinaryStringInfo(buf, "OBJECT", 6);
					break;
				default:
					elog(ERROR, "Unknown type for IS: %d", jsqGetIsType(v));
					break;
			}
			break;
		case jqiArray:
			if (printBracketes)
				appendStringInfoChar(buf, '[');

			while(jsqIterateArray(v, &elem))
			{
				if (first == false)
					appendBinaryStringInfo(buf, ", ", 2);
				else
					first = false;
				printJsQueryItem(buf, &elem, false, true);
			}

			if (printBracketes)
				appendStringInfoChar(buf, ']');
			break;
		case jqiAnd:
		case jqiOr:
			appendStringInfoChar(buf, '(');
			jsqGetLeftArg(v, &elem);
			printJsQueryItem(buf, &elem, false, true);
			printOperation(buf, v->type);
			jsqGetRightArg(v, &elem);
			printJsQueryItem(buf, &elem, false, true);
			appendStringInfoChar(buf, ')');
			break;
		case jqiEqual:
		case jqiLess:
		case jqiGreater:
		case jqiLessOrEqual:
		case jqiGreaterOrEqual:
		case jqiContains:
		case jqiContained:
		case jqiOverlap:
			printOperation(buf, v->type);
			jsqGetArg(v, &elem);
			printJsQueryItem(buf, &elem, false, true);
			break;
		case jqiIn:
			appendBinaryStringInfo(buf, " IN (", 5);
			jsqGetArg(v, &elem);
			printJsQueryItem(buf, &elem, false, false);
			appendStringInfoChar(buf, ')');
			break;
		case jqiNot:
			appendStringInfoChar(buf, '(');
			appendBinaryStringInfo(buf, "NOT ", 4);
			jsqGetArg(v, &elem);
			printJsQueryItem(buf, &elem, false, true);
			appendStringInfoChar(buf, ')');
			break;
		case jqiCurrent:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '$');
			break;
		case jqiLength:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '@');
			appendStringInfoChar(buf, '#');
			break;
		case jqiAny:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '*');
			break;
		case jqiAnyArray:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '#');
			break;
		case jqiAnyKey:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '%');
			break;
		case jqiAll:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '*');
			appendStringInfoChar(buf, ':');
			break;
		case jqiAllArray:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '#');
			appendStringInfoChar(buf, ':');
			break;
		case jqiAllKey:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfoChar(buf, '%');
			appendStringInfoChar(buf, ':');
			break;
		case jqiIndexArray:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendStringInfo(buf, "#%u", v->arrayIndex);
			break;
		case jqiFilter:
			if (inKey)
				appendStringInfoChar(buf, '.');
			appendBinaryStringInfo(buf, " ?(", 3);
			jsqGetArg(v, &elem);
			printJsQueryItem(buf, &elem, false, false);
			appendBinaryStringInfo(buf, ") ", 2);
			break;
		default:
			elog(ERROR, "Unknown JsQueryItem type: %d", v->type);
	}

	if (jsqGetNext(v, &elem))
		printJsQueryItem(buf, &elem, true, true);
}

PG_FUNCTION_INFO_V1(jsquery_out);
Datum
jsquery_out(PG_FUNCTION_ARGS)
{
	JsQuery			*in = PG_GETARG_JSQUERY(0);
	StringInfoData	buf;
	JsQueryItem		v;

	initStringInfo(&buf);
	enlargeStringInfo(&buf, VARSIZE(in) /* estimation */); 

	jsqInit(&v, in);
	printJsQueryItem(&buf, &v, false, true);

	PG_RETURN_CSTRING(buf.data);
}


