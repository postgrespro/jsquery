/*-------------------------------------------------------------------------
 *
 * jsquery_gram.y
 *     Grammar definitions for jsquery datatype
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_gram.y
 *
 *-------------------------------------------------------------------------
 */

%{
#define YYPARSE_PARAM result  /* need this to pass a pointer (void *) to yyparse */

#include "postgres.h"

#include "fmgr.h"
#include "utils/builtins.h"

#include "jsquery.h"

	/*
 * Bison doesn't allocate anything that needs to live across parser calls,
 * so we can easily have it use palloc instead of malloc.  This prevents
 * memory leaks if we error out during parsing.  Note this only works with
 * bison >= 2.0.  However, in bison 1.875 the default is to use alloca()
 * if possible, so there's not really much problem anyhow, at least if
 * you're building with gcc.
 */
#define YYMALLOC palloc
#define YYFREE   pfree

/* Avoid exit() on fatal scanner errors (a bit ugly -- see yy_fatal_error) */
#undef fprintf
#define fprintf(file, fmt, msg)  fprintf_to_ereport(fmt, msg)

static void
fprintf_to_ereport(const char *fmt, const char *msg)
{
	ereport(ERROR, (errmsg_internal("%s", msg)));
}

/* struct string is shared between scan and gram */
typedef struct string {
	char 	*val;
	int  	len;
	int		total;
} string;
#include <jsquery_gram.h>

/* flex 2.5.4 doesn't bother with a decl for this */
int jsquery_yylex(YYSTYPE * yylval_param);
int jsquery_yyparse(void *result);
void jsquery_yyerror(const char *message);

static JsQueryParseItem*
makeItemType(int type)
{
	JsQueryParseItem* v = palloc(sizeof(*v));

	v->type = type;
	v->hint = jsqIndexDefault;
	v->next = NULL;

	return v;
}

static JsQueryParseItem*
makeItemString(string *s)
{
	JsQueryParseItem *v;

	if (s == NULL)
	{
		v = makeItemType(jqiNull);
	}
	else
	{
		v = makeItemType(jqiString);
		v->string.val = s->val;
		v->string.len = s->len;
	}

	return v;
}

static JsQueryParseItem*
makeItemNumeric(string *s)
{
	JsQueryParseItem		*v;

	v = makeItemType(jqiNumeric);
	v->numeric = DatumGetNumeric(DirectFunctionCall3(numeric_in, CStringGetDatum(s->val), 0, -1));

	return v;
}

static JsQueryParseItem*
makeItemBool(bool val) {
	JsQueryParseItem *v = makeItemType(jqiBool);

	v->boolean = val;

	return v;
}

static JsQueryParseItem*
makeItemArray(List *list)
{
	JsQueryParseItem	*v = makeItemType(jqiArray);

	v->array.nelems = list_length(list);

	if (v->array.nelems > 0)
	{
		ListCell	*cell;
		int			i = 0;

		v->array.elems = palloc(sizeof(JsQueryParseItem) * v->array.nelems);

		foreach(cell, list)
			v->array.elems[i++] = (JsQueryParseItem*)lfirst(cell);
	}
	else
	{
		v->array.elems = NULL;
	}

	return v;
}

static JsQueryParseItem*
makeItemBinary(int type, JsQueryParseItem* la, JsQueryParseItem *ra)
{
	JsQueryParseItem  *v = makeItemType(type);

	v->args.left = la;
	v->args.right = ra;

	return v;
}

static JsQueryParseItem*
makeItemUnary(int type, JsQueryParseItem* a)
{
	JsQueryParseItem  *v = makeItemType(type);

	v->arg = a;

	return v;
}

static JsQueryParseItem*
makeItemIs(int isType)
{
	JsQueryParseItem  *v = makeItemType(jqiIs);

	v->isType = isType;

	return v;
}

static JsQueryParseItem*
makeItemList(List *list) {
	JsQueryParseItem	*head, *end;
	ListCell	*cell;

	head = end = (JsQueryParseItem*)linitial(list);

	foreach(cell, list)
	{
		JsQueryParseItem	*c = (JsQueryParseItem*)lfirst(cell);

		if (c == head)
			continue;

		end->next = c;
		end = c;
	}

	return head;
}

%}

/* BISON Declarations */
%pure-parser
%expect 0
%name-prefix="jsquery_yy"
%error-verbose

%union {
	string 				str;
	List				*elems; 		/* list of JsQueryParseItem */

	JsQueryParseItem	*value;
	JsQueryHint			hint;
}

%token	<str>		IN_P IS_P NULL_P TRUE_P ARRAY_T
					FALSE_P NUMERIC_T OBJECT_T STRING_T 
					BOOLEAN_T

%token	<str>		STRING_P NUMERIC_P

%type	<value>		result scalar_value 
%type	<str>		key

%type	<elems>		path value_list

%type 	<value>		path_elem right_expr expr array

%token	<hint>		HINT_P

%type	<hint>		opt_hint

%left '|'
%left '&'
%left '!'

/* Grammar follows */
%%

result: 
	expr							{ *((JsQueryParseItem**)result) = $1; } 
	| /* EMPTY */					{ *((JsQueryParseItem**)result) = NULL; }
	;

array:
	'[' value_list ']'				{ $$ = makeItemArray($2); }
	;

scalar_value:
	STRING_P						{ $$ = makeItemString(&$1); }
	| IN_P							{ $$ = makeItemString(&$1); }
	| IS_P							{ $$ = makeItemString(&$1); }
	| NULL_P						{ $$ = makeItemString(NULL); }
	| TRUE_P						{ $$ = makeItemBool(true); }
	| ARRAY_T						{ $$ = makeItemString(&$1); }
	| FALSE_P						{ $$ = makeItemBool(false); }
	| NUMERIC_T						{ $$ = makeItemString(&$1); }
	| OBJECT_T						{ $$ = makeItemString(&$1); }
	| STRING_T						{ $$ = makeItemString(&$1); }
	| BOOLEAN_T						{ $$ = makeItemString(&$1); }
	| NUMERIC_P						{ $$ = makeItemNumeric(&$1); }
	;

opt_hint:
	HINT_P							{ $$ = $1; }
	| /* EMPTY */					{ $$ = jsqIndexDefault; }
	;

value_list:
	scalar_value 					{ $$ = lappend(NIL, $1); } 
	| value_list ',' scalar_value	{ $$ = lappend($1, $3); } 
	;

right_expr:
	'='	scalar_value				{ $$ = makeItemUnary(jqiEqual, $2); }
	| IN_P '(' value_list ')'		{ $$ = makeItemUnary(jqiIn, makeItemArray($3)); }
	| '=' array						{ $$ = makeItemUnary(jqiEqual, $2); }
	| '=' '*'						{ $$ = makeItemUnary(jqiEqual, makeItemType(jqiAny)); }
	| '<' NUMERIC_P					{ $$ = makeItemUnary(jqiLess, makeItemNumeric(&$2)); }
	| '>' NUMERIC_P					{ $$ = makeItemUnary(jqiGreater, makeItemNumeric(&$2)); }
	| '<' '=' NUMERIC_P				{ $$ = makeItemUnary(jqiLessOrEqual, makeItemNumeric(&$3)); }
	| '>' '=' NUMERIC_P				{ $$ = makeItemUnary(jqiGreaterOrEqual, makeItemNumeric(&$3)); }
	| '@' '>' array					{ $$ = makeItemUnary(jqiContains, $3); } 
	| '<' '@' array					{ $$ = makeItemUnary(jqiContained, $3); } 
	| '&' '&' array					{ $$ = makeItemUnary(jqiOverlap, $3); }
	| IS_P ARRAY_T 					{ $$ = makeItemIs(jbvArray); }
	| IS_P NUMERIC_T 				{ $$ = makeItemIs(jbvNumeric); }
	| IS_P OBJECT_T 				{ $$ = makeItemIs(jbvObject); }
	| IS_P STRING_T 				{ $$ = makeItemIs(jbvString); }
	| IS_P BOOLEAN_T 				{ $$ = makeItemIs(jbvBool); }
	;

expr:
	path opt_hint right_expr		{ $3->hint = $2; $$ = makeItemList(lappend($1, $3)); }
	| path '(' expr ')'				{ $$ = makeItemList(lappend($1, $3)); }
	| '(' expr ')'					{ $$ = $2; }
	| '!' expr 						{ $$ = makeItemUnary(jqiNot, $2); }
	| expr '&' expr					{ $$ = makeItemBinary(jqiAnd, $1, $3); } 
	| expr '|' expr					{ $$ = makeItemBinary(jqiOr, $1, $3); }
	;

/*
 * key is always a string, not a bool or numeric
 */
key:
	STRING_P						{ $$ = $1; }
	| IN_P							{ $$ = $1; }
	| IS_P							{ $$ = $1; }
	| NULL_P						{ $$ = $1; }
	| TRUE_P						{ $$ = $1; }
	| ARRAY_T						{ $$ = $1; }
	| FALSE_P						{ $$ = $1; }
	| NUMERIC_T						{ $$ = $1; }
	| OBJECT_T						{ $$ = $1; }
	| STRING_T						{ $$ = $1; }
	| BOOLEAN_T						{ $$ = $1; }
	| NUMERIC_P						{ $$ = $1; }
	;

path_elem:
	'*'								{ $$ = makeItemType(jqiAny); }
	| '#'							{ $$ = makeItemType(jqiAnyArray); }
	| '%'							{ $$ = makeItemType(jqiAnyKey); }
	| '$'							{ $$ = makeItemType(jqiCurrent); }
	| key							{ $$ = makeItemString(&$1); $$->type = jqiKey; }
	;

path:
	path_elem						{ $$ = lappend(NIL, $1); }
	| path '.' path_elem			{ $$ = lappend($1, $3); }
	;

%%

#include "jsquery_scan.c"
