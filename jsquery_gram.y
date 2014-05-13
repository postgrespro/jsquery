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

static JsQueryItem*
makeJsQueryItemType(int type)
{
	JsQueryItem* v = palloc(sizeof(*v));

	v->type = type;
	v->next = NULL;

	return v;
}

static JsQueryItem*
makeJsQueryItemString(string *s)
{
	JsQueryItem *v;

	if (s == NULL)
	{
		v = makeJsQueryItemType(jqiNull);
	}
	else
	{
		v = makeJsQueryItemType(jqiString);
		v->string.val = s->val;
		v->string.len = s->len;
	}

	return v;
}

static JsQueryItem*
makeJsQueryItemNumeric(string *s)
{
	JsQueryItem		*v;

	v = makeJsQueryItemType(jqiNumeric);
	v->numeric = DatumGetNumeric(DirectFunctionCall3(numeric_in, CStringGetDatum(s->val), 0, -1));

	return v;
}

static JsQueryItem*
makeJsQueryItemBool(bool val) {
	JsQueryItem *v = makeJsQueryItemType(jqiBool);

	v->boolean = val;

	return v;
}

static JsQueryItem*
makeJsQueryItemArray(List *list)
{
	JsQueryItem	*v = makeJsQueryItemType(jqiArray);

	v->array.nelems = list_length(list);

	if (v->array.nelems > 0)
	{
		ListCell	*cell;
		int			i = 0;

		v->array.elems = palloc(sizeof(JsQueryItem) * v->array.nelems);

		foreach(cell, list)
			v->array.elems[i++] = (JsQueryItem*)lfirst(cell);
	}
	else
	{
		v->array.elems = NULL;
	}

	return v;
}

static JsQueryItem*
makeJsQueryItemBinary(int type, JsQueryItem* la, JsQueryItem *ra)
{
	JsQueryItem  *v = makeJsQueryItemType(type);

	v->args.left = la;
	v->args.right = ra;

	return v;
}

static JsQueryItem*
makeJsQueryItemUnary(int type, JsQueryItem* a)
{
	JsQueryItem  *v = makeJsQueryItemType(type);

	v->arg = a;

	return v;
}

static JsQueryItem*
makeJsQueryItemList(List *list) {
	JsQueryItem	*head, *end;
	ListCell	*cell;

	head = end = (JsQueryItem*)linitial(list);

	foreach(cell, list)
	{
		JsQueryItem	*c = (JsQueryItem*)lfirst(cell);

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
	string 			str;
	Numeric			numeric;
	List			*elems; 		/* list of JsQueryItem */

	JsQueryItem		*value;
}

%token	<str>			NULL_P STRING_P TRUE_P FALSE_P
						NUMERIC_P IN_P

%type	<value>			result scalar_value 
%type	<str>			key

%type	<elems>			path value_list

%type 	<value>			path_elem right_expr expr array

%left '|'
%left '&'
%left '!'

/* Grammar follows */
%%

result: 
	expr							{ *((JsQueryItem**)result) = $1; } 
	| /* EMPTY */					{ *((JsQueryItem**)result) = NULL; }
	;

array:
	'[' value_list ']'				{ $$ = makeJsQueryItemArray($2); }
	;

scalar_value:
	NULL_P							{ $$ = makeJsQueryItemString(NULL); }
	| STRING_P						{ $$ = makeJsQueryItemString(&$1); }
	| IN_P							{ $$ = makeJsQueryItemString(&$1); }
	| TRUE_P						{ $$ = makeJsQueryItemBool(true); }
	| FALSE_P						{ $$ = makeJsQueryItemBool(false); }
	| NUMERIC_P						{ $$ = makeJsQueryItemNumeric(&$1); }
	;

value_list:
	scalar_value 					{ $$ = lappend(NIL, $1); } 
	| value_list ',' scalar_value	{ $$ = lappend($1, $3); } 
	;

right_expr:
	'='	scalar_value				{ $$ = makeJsQueryItemUnary(jqiEqual, $2); }
	| IN_P '(' value_list ')'		{ $$ = makeJsQueryItemUnary(jqiIn, makeJsQueryItemArray($3)); }
	| '=' array						{ $$ = makeJsQueryItemUnary(jqiEqual, $2); }
	| '=' '*'						{ $$ = makeJsQueryItemUnary(jqiEqual, makeJsQueryItemType(jqiAny)); }
	| '<' NUMERIC_P					{ $$ = makeJsQueryItemUnary(jqiLess, makeJsQueryItemNumeric(&$2)); }
	| '>' NUMERIC_P					{ $$ = makeJsQueryItemUnary(jqiGreater, makeJsQueryItemNumeric(&$2)); }
	| '<' '=' NUMERIC_P				{ $$ = makeJsQueryItemUnary(jqiLessOrEqual, makeJsQueryItemNumeric(&$3)); }
	| '>' '=' NUMERIC_P				{ $$ = makeJsQueryItemUnary(jqiGreaterOrEqual, makeJsQueryItemNumeric(&$3)); }
	| '@' '>' array					{ $$ = makeJsQueryItemUnary(jqiContains, $3); } 
	| '<' '@' array					{ $$ = makeJsQueryItemUnary(jqiContained, $3); } 
	| '&' '&' array					{ $$ = makeJsQueryItemUnary(jqiOverlap, $3); }
	;

expr:
	path right_expr					{ $$ = makeJsQueryItemList(lappend($1, $2)); }
	| path '(' expr ')'				{ $$ = makeJsQueryItemList(lappend($1, $3)); }
	| '(' expr ')'					{ $$ = $2; }
	| '!' expr 						{ $$ = makeJsQueryItemUnary(jqiNot, $2); }
	| expr '&' expr					{ $$ = makeJsQueryItemBinary(jqiAnd, $1, $3); } 
	| expr '|' expr					{ $$ = makeJsQueryItemBinary(jqiOr, $1, $3); }
	;

/*
 * key is always a string, not a bool or numeric
 */
key:
	STRING_P						{ $$ = $1; }
	| TRUE_P						{ $$ = $1; }
	| FALSE_P						{ $$ = $1; }
	| NUMERIC_P						{ $$ = $1; }
	| NULL_P						{ $$ = $1; }
	| IN_P							{ $$ = $1; }
	;

path_elem:
	'*'								{ $$ = makeJsQueryItemType(jqiAny); }
	| '#'							{ $$ = makeJsQueryItemType(jqiAnyArray); }
	| '%'							{ $$ = makeJsQueryItemType(jqiAnyKey); }
	| '$'							{ $$ = makeJsQueryItemType(jqiCurrent); }
	| key							{ $$ = makeJsQueryItemString(&$1); $$->type = jqiKey; }
	;

path:
	path_elem						{ $$ = lappend(NIL, $1); }
	| path '.' path_elem			{ $$ = lappend($1, $3); }
	;

%%

#include "jsquery_scan.c"
