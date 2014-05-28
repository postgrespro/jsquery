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

#include "access/gin.h"
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

typedef enum JsQueryItemType {
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
		jqiContained = '^',
		jqiOverlap = 'O',
		jqiAny = '*',
		jqiAnyArray = '#',
		jqiAnyKey = '%',
		jqiKey = 'K',
		jqiCurrent = '$',
		jqiIn = 'I'
} JsQueryItemType;

typedef struct JsQueryItem {
	JsQueryItemType	type;
	int32			nextPos;
	char			*base;

	union {
		struct {
			char		*data;  /* for bool, numeric and string/key */
			int			datalen; /* filled only for string/key */
		} value; 

		struct {
			int32	left;
			int32	right;
		} args;

		int32		arg;

		struct {
			int		nelems;
			int		current;
			int32	*arrayPtr;
		} array;
	};


} JsQueryItem;

extern void jsqInit(JsQueryItem *v, JsQuery *js);
extern void jsqInitByBuffer(JsQueryItem *v, char *base, int32 pos);
extern bool jsqGetNext(JsQueryItem *v, JsQueryItem *a);
extern void jsqGetArg(JsQueryItem *v, JsQueryItem *a);
extern void jsqGetLeftArg(JsQueryItem *v, JsQueryItem *a);
extern void jsqGetRightArg(JsQueryItem *v, JsQueryItem *a);
extern Numeric	jsqGetNumeric(JsQueryItem *v);
extern bool		jsqGetBool(JsQueryItem *v);
extern char * jsqGetString(JsQueryItem *v, int32 *len);
extern void jsqIterateInit(JsQueryItem *v);
extern bool jsqIterateArray(JsQueryItem *v, JsQueryItem *e);

/*
 * Parsing
 */

typedef struct JsQueryParseItem JsQueryParseItem;

struct JsQueryParseItem {
	JsQueryItemType	type;
	JsQueryParseItem	*next; /* next in path */

	union {
		struct {
			JsQueryParseItem	*left;
			JsQueryParseItem	*right;
		} args;

		JsQueryParseItem	*arg;

		Numeric		numeric;
		bool		boolean;
		struct {
			uint32      len;
			char        *val; /* could not be not null-terminated */
		} string;

		struct {
			int					nelems;
			JsQueryParseItem	**elems;
		} array;
	};
};

extern JsQueryParseItem* parsejsquery(const char *str, int len);

void alignStringInfoInt(StringInfo buf);

/* jsquery_extract.c */

typedef enum
{
	iAny 		= jqiAny,
	iAnyArray 	= jqiAnyArray,
	iKey 		= jqiKey,
	iAnyKey 	= jqiAnyKey
} PathItemType;

typedef struct PathItem PathItem;
struct PathItem
{
	PathItemType	type;
	int				len;
	char		   *s;
	PathItem	   *parent;
};

typedef enum
{
	eScalar = 1,
	eAnd	= jqiAnd,
	eOr		= jqiOr,
	eNot 	= jqiNot
} ExtractedNodeType;

typedef struct ExtractedNode ExtractedNode;
struct ExtractedNode
{
	ExtractedNodeType	type;
	PathItem		   *path;
	bool				indirect;
	union
	{
		struct
		{
			ExtractedNode **items;
			int				count;
		} args;
		struct
		{
			bool			inequality;
			bool			leftInclusive;
			bool			rightInclusive;
			JsQueryItem		*exact;
			JsQueryItem		*leftBound;
			JsQueryItem		*rightBound;
		} bounds;
		int	entryNum;
	};
};

typedef int (*MakeEntryHandler)(ExtractedNode *node, Pointer extra);

ExtractedNode *extractJsQuery(JsQuery *jq, MakeEntryHandler handler, Pointer extra);
bool execRecursive(ExtractedNode *node, bool *check);
bool execRecursiveTristate(ExtractedNode *node, GinTernaryValue *check);

#endif
