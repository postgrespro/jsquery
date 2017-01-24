/*-------------------------------------------------------------------------
 *
 * jsquery_constr.c
 *	Functions and operations to manipulate jsquery
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *	contrib/jsquery/jsquery_constr.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "utils/builtins.h"

#include "jsquery.h"

static int32
copyJsQuery(StringInfo buf, JsQueryItem *jsq)
{
	JsQueryItem	elem;
	int32		next, chld;
	int32		resPos = buf->len - VARHDRSZ; /* position from begining of jsquery data */

	check_stack_depth();

	Assert((jsq->type & jsq->hint) == 0);
	Assert((jsq->type & JSQ_HINT_MASK) == 0);

	appendStringInfoChar(buf, (char)(jsq->type | jsq->hint));
	alignStringInfoInt(buf);

	next = (jsqGetNext(jsq, NULL)) ? buf->len : 0;
	appendBinaryStringInfo(buf, (char*)&next /* fake value */, sizeof(next));

	switch(jsq->type)
	{
		case jqiKey:
		case jqiString:
			{
				int32	len;
				char	*s;

				s = jsqGetString(jsq, &len);
				appendBinaryStringInfo(buf, (char*)&len, sizeof(len));
				appendBinaryStringInfo(buf, s, len + 1 /* \0 */);
			}
			break;
		case jqiNumeric:
			{
				Numeric n = jsqGetNumeric(jsq);

				appendBinaryStringInfo(buf, (char*)n, VARSIZE_ANY(n));
			}
			break;
		case jqiBool:
			{
				bool v = jsqGetBool(jsq);

				appendBinaryStringInfo(buf, (char*)&v, 1);
			}
			break;
		case jqiArray:
			{
				int32 i, arrayStart;

				appendBinaryStringInfo(buf, (char*)&jsq->array.nelems, 
									   sizeof(jsq->array.nelems));

				arrayStart = buf->len;

				/* reserve place for "pointers" to array's elements */
				for(i=0; i<jsq->array.nelems; i++)
					appendBinaryStringInfo(buf, (char*)&i /* fake value */, sizeof(i));

				while(jsqIterateArray(jsq, &elem))
				{
					chld = copyJsQuery(buf, &elem);
					*(int32*)(buf->data + arrayStart + i * sizeof(i)) = chld;
					i++;
				}
			}
			break;
		case jqiAnd:
		case jqiOr:
			{
				int32	leftOut, rightOut;

				leftOut = buf->len;
				appendBinaryStringInfo(buf, (char*)&leftOut /* fake value */, sizeof(leftOut));
				rightOut = buf->len;
				appendBinaryStringInfo(buf, (char*)&rightOut /* fake value */, sizeof(rightOut));

				jsqGetLeftArg(jsq, &elem);
				chld = copyJsQuery(buf, &elem);
				*(int32*)(buf->data + leftOut) = chld;

				jsqGetRightArg(jsq, &elem);
				chld = copyJsQuery(buf, &elem);
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
				int32	argOut = buf->len;

				appendBinaryStringInfo(buf, (char*)&argOut /* fake value */, sizeof(argOut));

				jsqGetArg(jsq, &elem);
				chld = copyJsQuery(buf, &elem);
				*(int32*)(buf->data + argOut) = chld;
			}
			break;
		case jqiIndexArray:
			appendBinaryStringInfo(buf, (char*)&jsq->arrayIndex,
								   sizeof(jsq->arrayIndex));
			break;
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
		default:
			elog(ERROR, "Unknown type: %d", jsq->type);
	}

	if (jsqGetNext(jsq, &elem))
		*(int32*)(buf->data + next) = copyJsQuery(buf, &elem);

	return resPos;
}

static JsQuery*
joinJsQuery(JsQueryItemType type, JsQuery *jq1, JsQuery *jq2)
{
	JsQuery			*out;
	StringInfoData	buf;
	int32			left, right, chld;
	JsQueryItem	v;

	initStringInfo(&buf);
	enlargeStringInfo(&buf, VARSIZE_ANY(jq1) + VARSIZE_ANY(jq2) + 4 * sizeof(int32) + VARHDRSZ);

	appendStringInfoSpaces(&buf, VARHDRSZ);

	/* form jqiAnd/jqiOr header */
	appendStringInfoChar(&buf, (char)type);
	alignStringInfoInt(&buf);

	/* nextPos field of header*/
	chld = 0; /* actual value, not a fake */
	appendBinaryStringInfo(&buf, (char*)&chld, sizeof(chld));

	left = buf.len;
	appendBinaryStringInfo(&buf, (char*)&left /* fake value */, sizeof(left));
	right = buf.len;
	appendBinaryStringInfo(&buf, (char*)&right /* fake value */, sizeof(right));

	/* dump left and right subtree */
	jsqInit(&v, jq1);
	chld = copyJsQuery(&buf, &v);
	*(int32*)(buf.data + left) = chld;
	jsqInit(&v, jq2);
	chld = copyJsQuery(&buf, &v);
	*(int32*)(buf.data + right) = chld;

	out = (JsQuery*)buf.data;
	SET_VARSIZE(out, buf.len);

	return out;
}

PG_FUNCTION_INFO_V1(jsquery_join_and);
Datum
jsquery_join_and(PG_FUNCTION_ARGS)
{
	JsQuery		*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery		*jq2 = PG_GETARG_JSQUERY(1);
	JsQuery		*out;

	out = joinJsQuery(jqiAnd, jq1, jq2);

	PG_FREE_IF_COPY(jq1, 0);
	PG_FREE_IF_COPY(jq2, 1);

	PG_RETURN_JSQUERY(out);
}

PG_FUNCTION_INFO_V1(jsquery_join_or);
Datum
jsquery_join_or(PG_FUNCTION_ARGS)
{
	JsQuery		*jq1 = PG_GETARG_JSQUERY(0);
	JsQuery		*jq2 = PG_GETARG_JSQUERY(1);
	JsQuery		*out;

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
	JsQueryItem		v;

	initStringInfo(&buf);
	enlargeStringInfo(&buf, VARSIZE_ANY(jq) + 4 * sizeof(int32) + VARHDRSZ);

	appendStringInfoSpaces(&buf, VARHDRSZ);

	/* form jsquery header */
	appendStringInfoChar(&buf, (char)jqiNot);
	alignStringInfoInt(&buf);

	/* nextPos field of header*/
	chld = 0; /* actual value, not a fake */
	appendBinaryStringInfo(&buf, (char*)&chld, sizeof(chld));

	arg = buf.len;
	appendBinaryStringInfo(&buf, (char*)&arg /* fake value */, sizeof(arg));

	jsqInit(&v, jq);
	chld = copyJsQuery(&buf, &v);
	*(int32*)(buf.data + arg) = chld;

	out = (JsQuery*)buf.data;
	SET_VARSIZE(out, buf.len);

	PG_FREE_IF_COPY(jq, 0);

	PG_RETURN_JSQUERY(out);
}

