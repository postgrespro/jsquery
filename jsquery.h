/*-------------------------------------------------------------------------
 *
 * jsquery.h
 *	Definitions of jsquery datatype
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *	contrib/jsquery/jsquery.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef __JSQUERY_H__
#define __JSQUERY_H__

#include "access/gin.h"
#include "fmgr.h"
#include "utils/numeric.h"
#include "utils/jsonb.h"
#include "utils/jsonpath.h"

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
		jqiAnd,
		jqiOr,
		jqiNot,
		jqiEqual,
		jqiLess,
		jqiGreater,
		jqiLessOrEqual,
		jqiGreaterOrEqual,
		jqiContains,
		jqiContained,
		jqiOverlap,
		jqiAny,
		jqiAnyArray,
		jqiAnyKey,
		jqiAll,
		jqiAllArray,
		jqiAllKey,
		jqiKey,
		jqiCurrent,
		jqiLength,
		jqiIn,
		jqiIs,
		jqiIndexArray,
		jqiFilter
} JsQueryItemType;

/*
 * JsQueryHint is stored in the same byte as JsQueryItemType so
 * JsQueryItemType should not use three high bits
 */
typedef enum JsQueryHint {
		jsqIndexDefault = 0x00,
		jsqForceIndex = 0x80,
		jsqNoIndex = 0x40
} JsQueryHint;

#define JSQ_HINT_MASK	(jsqIndexDefault | jsqForceIndex | jsqNoIndex)

/*
 * Support functions to parse/construct binary value.
 * Unlike many other representation of expression the first/main
 * node is not an operation but left operand of expression. That
 * allows to implement cheep follow-path descending in jsonb
 * structure and then execute operator with right operand which
 * is always a constant.
 */

typedef struct JsQueryItem {
	JsQueryItemType	type;
	JsQueryHint		hint;
	uint32			nextPos;
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

		uint32		arrayIndex;
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
extern int32	jsqGetIsType(JsQueryItem *v);
extern char * jsqGetString(JsQueryItem *v, int32 *len);
extern void jsqIterateInit(JsQueryItem *v);
extern bool jsqIterateArray(JsQueryItem *v, JsQueryItem *e);
extern void jsqIterateDestroy(JsQueryItem *v);

void alignStringInfoInt(StringInfo buf);

/*
 * Parsing
 */

typedef struct JsQueryParseItem JsQueryParseItem;

struct JsQueryParseItem {
	JsQueryItemType	type;
	JsQueryHint		hint;
	JsQueryParseItem	*next; /* next in path */
	bool			filter;

	union {
		struct {
			JsQueryParseItem	*left;
			JsQueryParseItem	*right;
		} args;

		JsQueryParseItem	*arg;
		int8		isType; /* jbv* values */

		Numeric		numeric;
		bool		boolean;
		struct {
			uint32		len;
			char		*val; /* could not be not null-terminated */
		} string;

		struct {
			int					nelems;
			JsQueryParseItem	**elems;
		} array;

		uint32		arrayIndex;
	};
};

extern JsQueryParseItem* parsejsquery(const char *str, int len);

/* jsquery_extract.c */

typedef enum
{
	iAny		= jqiAny,
	iAnyArray	= jqiAnyArray,
	iKey		= jqiKey,
	iAnyKey		= jqiAnyKey,
	iIndexArray = jqiIndexArray
} PathItemType;

typedef struct PathItem PathItem;
struct PathItem
{
	PathItemType	type;
	int				len;
	int				arrayIndex;
	char		   *s;
	PathItem	   *parent;
};

typedef enum
{
	eExactValue = 1,
	eEmptyArray,
	eInequality,
	eIs,
	eAny,
	eAnd		= jqiAnd,
	eOr			= jqiOr,
} ExtractedNodeType;

typedef enum
{
	sEqual = 1,
	sRange,
	sInequal,
	sIs,
	sAny
} SelectivityClass;

typedef struct ExtractedNode ExtractedNode;
struct ExtractedNode
{
	ExtractedNodeType	type;
	JsQueryHint			hint;
	PathItem		   *path;
	bool				indirect;
	SelectivityClass	sClass;
	bool				forceIndex;
	int					number;
	int					entryNum;
	union
	{
		struct
		{
			ExtractedNode **items;
			int				count;
		} args;
		struct
		{
			bool			leftInclusive;
			bool			rightInclusive;
			JsQueryItem	   *leftBound;
			JsQueryItem	   *rightBound;
		} bounds;
		JsQueryItem		   *exactValue;
		int32				isType;
	};
};

typedef int (*MakeEntryHandler)(ExtractedNode *node, Pointer extra);
typedef bool (*CheckEntryHandler)(ExtractedNode *node, Pointer extra);
bool isLogicalNodeType(ExtractedNodeType type);

ExtractedNode *extractJsQuery(JsQuery *jq, MakeEntryHandler makeHandler,
								CheckEntryHandler checkHandler, Pointer extra);
#ifndef NO_JSONPATH
ExtractedNode *extractJsonPath(JsonPath *jp, bool exists, bool arrayPathItems,
							   MakeEntryHandler makeHandler,
							   CheckEntryHandler checkHandler, Pointer extra);
#endif
char *debugJsQuery(JsQuery *jq, MakeEntryHandler makeHandler,
								CheckEntryHandler checkHandler, Pointer extra);
bool queryNeedRecheck(ExtractedNode *node);
bool execRecursive(ExtractedNode *node, bool *check);
bool execRecursiveTristate(ExtractedNode *node, GinTernaryValue *check);

#ifndef PG_RETURN_JSONB_P
#define PG_RETURN_JSONB_P(x)	PG_RETURN_JSONB(x)
#endif

#ifndef PG_GETARG_JSONB_P
#define PG_GETARG_JSONB_P(x)	PG_GETARG_JSONB(x)
#endif

#endif
