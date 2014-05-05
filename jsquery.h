/*-------------------------------------------------------------------------
 *
 * jsquery.h
 *     Definitions of jsquery datatype
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef __JSQUERY_H__
#define __JSQUERY_H__

#include "fmgr.h"
#include "utils/numeric.h"
#include "utils/jsonb.h"

typedef struct
{
	int32	vl_len_;	/* varlena header (do not touch directly!) */
} JsQuery;

#define DatumGetJsQueryP(d)	((JsQuery*)DatumGetPointer(PG_DETOAST_DATUM(d)))
#define PG_GETARG_JSQUERY(x)	DatumGetJsQueryP(PG_GETARG_DATUM(x))
#define PG_RETURN_JSQUERY(p)	PG_RETURN_POINTER(p)


typedef struct JsQueryItem JsQueryItem;

struct JsQueryItem {
	enum {
		jqiNull = jbvNull, 
		jqiString = jbvString, 
		jqiNumeric = jbvNumeric, 
		jqiBool = jbvBool, 
		jqiArray = jbvArray, 
		jqiAnd = '&', 
		jqiOr = '|', 
		jqiNot = '!',
		jqiEqual = '=',
		jqiLess  = '<',
		jqiGreater  = '>',
		jqiLessOrEqual = '{',
		jqiGreaterOrEqual = '}',
		jqiContains = '@',
		jqiContained = '$',
		jqiOverlap = '%',
		jqiAny = '*',
		jqiAnyArray = '#',
		jqiKey = 'K',
		jqiIn = 'I'
	} type;

	union {
		struct {
			JsQueryItem	*left;
			JsQueryItem	*right;
		} args;

		JsQueryItem	*arg;

		Numeric		numeric;
		bool		boolean;
		struct {
			uint32      len;
			char        *val; /* could be not null-terminated */
		} string;

		struct {
			int			nelems;
			JsQueryItem	**elems;
		} array;
	};

	JsQueryItem	*next; /* next in path */
};

/*
 * support
 */

extern JsQueryItem* parsejsquery(const char *str, int len);

int32 readJsQueryHeader(char *base, int32 pos, int32 *type, int32 *nextPos);

#define read_byte(v, b, p) do {     \
	(v) = *(int8*)((b) + (p));      \
	(p) += 1;                       \
} while(0)                          \

#define read_int32(v, b, p) do {    \
	(v) = *(int32*)((b) + (p));     \
	(p) += sizeof(int32);           \
} while(0)                          \

#endif

