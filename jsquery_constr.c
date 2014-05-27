/*-------------------------------------------------------------------------
 *
 * jsquery_manipulation.c
 *     Functions and operations to manipulate jsquery  
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_manipulation.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "utils/builtins.h"

#include "jsquery.h"

static int32
copyJsQuery(StringInfo buf, char *jqBase, int32 jqPos)
{
	int32 			resPos = buf->len - VARHDRSZ; /* position from begining of jsquery data */
	JsQueryItemType	type;
	int32			nextPos, chld, next;

	check_stack_depth();

	jqPos = readJsQueryHeader(jqBase, jqPos, &type, &nextPos);

	appendStringInfoChar(buf, (char)type);
	alignStringInfoInt(buf);

	next = (nextPos > 0) ? buf->len : 0;;
	appendBinaryStringInfo(buf, (char*)&next /* fake value */, sizeof(next));

	switch(type)
	{
		case jqiKey:
		case jqiString:
			{
				int32 len;

				read_int32(len, jqBase, jqPos);
				appendBinaryStringInfo(buf, (char*)&len, sizeof(len));
				appendBinaryStringInfo(buf, jqBase + jqPos, len + 1 /* \0 */);
			}
			break;
		case jqiNumeric:
			appendBinaryStringInfo(buf, jqBase + jqPos, VARSIZE(jqBase + jqPos));
			break;
		case jqiBool:
			appendBinaryStringInfo(buf, jqBase + jqPos, 1);
			break;
		case jqiArray:
			{
				int32 i, nelems, arrayStart, *arrayPosIn;

				read_int32(nelems, jqBase, jqPos);
				appendBinaryStringInfo(buf, (char*)&nelems /* fake value */, sizeof(nelems));

				arrayStart = buf->len;
				arrayPosIn = (int32*)(jqBase + jqPos);

				/* reserve place for "pointers" to array's elements */
				for(i=0; i<nelems; i++)
					appendBinaryStringInfo(buf, (char*)&i /* fake value */, sizeof(i));

				for(i=0; i<nelems; i++)
				{
					chld = copyJsQuery(buf, jqBase, arrayPosIn[i]);
					*(int32*)(buf->data + arrayStart + i * sizeof(i)) = chld;
				}
			}
			break;
		case jqiAnd:
		case jqiOr:
			{
				int32	leftIn, rightIn, leftOut, rightOut;

				leftOut = buf->len;
				appendBinaryStringInfo(buf, (char*)&leftOut /* fake value */, sizeof(leftOut));
				rightOut = buf->len;
				appendBinaryStringInfo(buf, (char*)&rightOut /* fake value */, sizeof(rightOut));

				read_int32(leftIn, jqBase, jqPos);
				chld = copyJsQuery(buf, jqBase, leftIn);
				*(int32*)(buf->data + leftOut) = chld;

				read_int32(rightIn, jqBase, jqPos);
				chld = copyJsQuery(buf, jqBase, rightIn);
				*(int32*)(buf->data + rightOut) = chld;
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
			{
				int32	argIn, argOut;

				argOut = buf->len;
				appendBinaryStringInfo(buf, (char*)&argOut /* fake value */, sizeof(argOut));

				read_int32(argIn, jqBase, jqPos);
				chld = copyJsQuery(buf, jqBase, argIn);
				*(int32*)(buf->data + argOut) = chld;
			}
			break;
		case jqiNull:
		case jqiAny:
		case jqiCurrent:
		case jqiAnyArray:
		case jqiAnyKey:
			break;
		default:
			elog(ERROR, "Unknown JsQueryItem type: %d", type);
	}

	if (nextPos)
		*(int32*)(buf->data + next) = copyJsQuery(buf, jqBase, nextPos);

	return resPos;
}

static JsQuery*
joinJsQuery(JsQueryItemType type, JsQuery *jq1, JsQuery *jq2)
{
	JsQuery			*out;
	StringInfoData	buf;
	int32			left, right, chld;

	initStringInfo(&buf);
	enlargeStringInfo(&buf, VARSIZE_ANY(jq1) + VARSIZE_ANY(jq2) + 4 * sizeof(int32) + VARHDRSZ);

	appendStringInfoSpaces(&buf, VARHDRSZ);

	appendStringInfoChar(&buf, (char)type);
	alignStringInfoInt(&buf);

	/* next */
	chld = 0;
	appendBinaryStringInfo(&buf, (char*)&chld, sizeof(chld));

	left = buf.len;
	appendBinaryStringInfo(&buf, (char*)&left /* fake value */, sizeof(left));
	right = buf.len;
	appendBinaryStringInfo(&buf, (char*)&right /* fake value */, sizeof(right));

	chld = copyJsQuery(&buf, VARDATA(jq1), 0);
	*(int32*)(buf.data + left) = chld;
	chld = copyJsQuery(&buf, VARDATA(jq2), 0);
	*(int32*)(buf.data + right) = chld;

	out = (JsQuery*)buf.data;
	SET_VARSIZE(out, buf.len);

	return out;
}

PG_FUNCTION_INFO_V1(jsquery_join_and);
Datum
jsquery_join_and(PG_FUNCTION_ARGS)
{
	JsQuery 		*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery 		*jq2 = PG_GETARG_JSQUERY(1);
	JsQuery			*out;

	out = joinJsQuery(jqiAnd, jq1, jq2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_JSQUERY(out);
}

PG_FUNCTION_INFO_V1(jsquery_join_or);
Datum
jsquery_join_or(PG_FUNCTION_ARGS)
{
	JsQuery 		*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery 		*jq2 = PG_GETARG_JSQUERY(1);
	JsQuery			*out;

	out = joinJsQuery(jqiOr, jq1, jq2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_JSQUERY(out);
}

PG_FUNCTION_INFO_V1(jsquery_not);
Datum
jsquery_not(PG_FUNCTION_ARGS)
{
	JsQuery			*jq = PG_GETARG_JSQUERY(0);
	JsQuery			*out;
	StringInfoData	buf;
	int32			arg, chld;

	initStringInfo(&buf);
	enlargeStringInfo(&buf, VARSIZE_ANY(jq) + 4 * sizeof(int32) + VARHDRSZ);

	appendStringInfoSpaces(&buf, VARHDRSZ);

	appendStringInfoChar(&buf, (char)jqiNot);
	alignStringInfoInt(&buf);

	/* next */
	chld = 0;
	appendBinaryStringInfo(&buf, (char*)&chld, sizeof(chld));

	arg = buf.len;
	appendBinaryStringInfo(&buf, (char*)&arg /* fake value */, sizeof(arg));

	chld = copyJsQuery(&buf, VARDATA(jq), 0);
	*(int32*)(buf.data + arg) = chld;

	out = (JsQuery*)buf.data;
	SET_VARSIZE(out, buf.len);

	PG_FREE_IF_COPY(jq, 0);

	PG_RETURN_JSQUERY(out);
}

