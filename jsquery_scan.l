/*-------------------------------------------------------------------------
 *
 * jsquery_scan.l
 *     Lexical parser for jsquery datatype
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 * Author: Teodor Sigaev <teodor@sigaev.ru>
 *
 * IDENTIFICATION
 *    contrib/jsquery/jsquery_scan.l
 *
 *-------------------------------------------------------------------------
 */

%{
#include "mb/pg_wchar.h"

static string scanstring;

/* No reason to constrain amount of data slurped */
/* #define YY_READ_BUF_SIZE 16777216 */

/* Handles to the buffer that the lexer uses internally */
static YY_BUFFER_STATE scanbufhandle;
static char *scanbuf;
static int	scanbuflen;

static void addstring(bool init, char *s, int l);
static void addchar(bool init, char s);
static int checkSpecialVal(void); /* examine scanstring for the special value */

static void parseUnicode(char *s, int l);

%}

%option 8bit
%option never-interactive
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option warn
%option prefix="jsquery_yy"
%option bison-bridge

%x xQUOTED
%x xNONQUOTED

special		 [\%\$\.\[\]\(\)\|\&\!\=\<\>\@\#\,\*]
any			[^\%\$\.\[\]\(\)\|\&\!\=\<\>\@\#\,\* \t\n\r\f\\\"]
blank		[ \t\n\r\f]
unicode     \\u[0-9A-Fa-f]{4}

%%

<INITIAL>{special}				{ return *yytext; }

<INITIAL>{blank}+				{ /* ignore */ }

<INITIAL>[+-]?[0-9]+(\.[0-9]+)?[eE][+-]?[0-9]+  /* float */  {
									addstring(true, yytext, yyleng);
									addchar(false, '\0');
									yylval->str = scanstring;
									return NUMERIC_P;
								}

<INITIAL>[+-]?\.[0-9]+[eE][+-]?[0-9]+  /* float */  {
									addstring(true, yytext, yyleng);
									addchar(false, '\0');
									yylval->str = scanstring;
									return NUMERIC_P;
								}

<INITIAL>[+-]?([0-9]+)?\.[0-9]+ {
									addstring(true, yytext, yyleng);
									addchar(false, '\0');
									yylval->str = scanstring;
									return NUMERIC_P;
								}

<INITIAL>[+-]?[0-9]+ 			{
									addstring(true, yytext, yyleng);
									addchar(false, '\0');
									yylval->str = scanstring;
									return NUMERIC_P;
								}


<INITIAL>{any}+					{
									addstring(true, yytext, yyleng);
									BEGIN xNONQUOTED;
								}
									
<INITIAL>\" 					{
									addchar(true, '\0');
									BEGIN xQUOTED;
								}

<INITIAL>\\						{
									yyless(0);
									addstring(true, "", 0);
									BEGIN xNONQUOTED;
								}

<xNONQUOTED>{any}+				{ 
									addstring(false, yytext, yyleng); 
								}

<xNONQUOTED>{blank}+			{ 
									yylval->str = scanstring;
									BEGIN INITIAL;
									return checkSpecialVal();
								}


<xNONQUOTED>({special}|\")		{
									yylval->str = scanstring;
									yyless(0);
									BEGIN INITIAL;
									return checkSpecialVal();
								}

<xNONQUOTED><<EOF>>				{ 
									yylval->str = scanstring;
									BEGIN INITIAL;
									return checkSpecialVal();
								}

<xNONQUOTED,xQUOTED>\\[\"\\\/]	{ 
									addchar(false, yytext[1]); 
								}

<xNONQUOTED,xQUOTED>\\b 		{ addchar(false, '\b'); }

<xNONQUOTED,xQUOTED>\\f 		{ addchar(false, '\f'); }

<xNONQUOTED,xQUOTED>\\n 		{ addchar(false, '\n'); }

<xNONQUOTED,xQUOTED>\\r 		{ addchar(false, '\r'); }

<xNONQUOTED,xQUOTED>\\t 		{ addchar(false, '\t'); }

<xNONQUOTED,xQUOTED>{unicode}+	{ parseUnicode(yytext, yyleng); }

<xNONQUOTED,xQUOTED>\\u 		{ yyerror("Unicode sequence is invalid"); }

<xNONQUOTED,xQUOTED>\\. 		{ yyerror("Escape sequence is invalid"); }

<xNONQUOTED,xQUOTED>\\ 			{ yyerror("Unexpected end after backslesh"); }

<xQUOTED><<EOF>>				{ yyerror("Unexpected end of quoted string"); }

<xQUOTED>\"						{
									yylval->str = scanstring;
									BEGIN INITIAL;
									return STRING_P;
								}

<xQUOTED>[^\\\"]+   			{ addstring(false, yytext, yyleng); }

<INITIAL><<EOF>>				{ yyterminate(); }

%%

void
yyerror(const char *message)
{
	if (*yytext == YY_END_OF_BUFFER_CHAR)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("bad jsquery representation"),
				 /* translator: %s is typically "syntax error" */
				 errdetail("%s at end of input", message)));
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("bad jsquery representation"),
				 /* translator: first %s is typically "syntax error" */
				 errdetail("%s at or near \"%s\"", message, yytext)));
	}
}

static int
checkSpecialVal()
{
	int res = STRING_P;

	if (scanstring.len == 2)
	{
		if (pg_strncasecmp("in", scanstring.val, scanstring.len) == 0)
			return IN_P;
	}
	else if (scanstring.len == 4)
	{
		if (strncmp("null", scanstring.val, scanstring.len) == 0)
			res = NULL_P;
		else if (strncmp("true", scanstring.val, scanstring.len) == 0)
			res = TRUE_P;
	}
	else if (scanstring.len == 5)
	{
		if (strncmp("false", scanstring.val, scanstring.len) == 0)
			res = FALSE_P;
	}

	return res;
}
/*
 * Called before any actual parsing is done
 */
static void
jsquery_scanner_init(const char *str, int slen)
{
	if (slen <= 0)
		slen = strlen(str);

	/*
	 * Might be left over after ereport()
	 */
	if (YY_CURRENT_BUFFER)
		yy_delete_buffer(YY_CURRENT_BUFFER);

	/*
	 * Make a scan buffer with special termination needed by flex.
	 */

	scanbuflen = slen;
	scanbuf = palloc(slen + 2);
	memcpy(scanbuf, str, slen);
	scanbuf[slen] = scanbuf[slen + 1] = YY_END_OF_BUFFER_CHAR;
	scanbufhandle = yy_scan_buffer(scanbuf, slen + 2);

	BEGIN(INITIAL);
}


/*
 * Called after parsing is done to clean up after jsquery_scanner_init()
 */
static void
jsquery_scanner_finish(void)
{
	yy_delete_buffer(scanbufhandle);
	pfree(scanbuf);
}

static void
addstring(bool init, char *s, int l) {
	if (init) {
		scanstring.total = 32;
		scanstring.val = palloc(scanstring.total);
		scanstring.len = 0;
	}

	if (s && l) {
		while(scanstring.len + l + 1 >= scanstring.total) {
			scanstring.total *= 2;
			scanstring.val = repalloc(scanstring.val, scanstring.total);
		}

		memcpy(scanstring.val + scanstring.len, s, l);
		scanstring.len += l;
	}
}

static void
addchar(bool init, char s) {
	if (init)
	{
		scanstring.total = 32;
		scanstring.val = palloc(scanstring.total);
		scanstring.len = 0;
	}
	else if(scanstring.len + 1 >= scanstring.total)
	{
		scanstring.total *= 2;
		scanstring.val = repalloc(scanstring.val, scanstring.total);
	}

	scanstring.val[ scanstring.len ] = s;
	if (s != '\0')
		scanstring.len++;
}

JsQueryParseItem* 
parsejsquery(const char *str, int len) {
	JsQueryParseItem		*parseresult;

	jsquery_scanner_init(str, len);

	if (jsquery_yyparse((void*)&parseresult) != 0)
		jsquery_yyerror("bugus input");

	jsquery_scanner_finish();

	return parseresult;
}

static int
hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0xA;
	elog(ERROR, "invalid hexadecimal digit");
	return 0; /* not reached */
}

/*
 * parseUnicode was adopted from json_lex_string() in 
 * src/backend/utils/adt/json.c
 */
static void
parseUnicode(char *s, int l)
{
	int i, j;
	int ch = 0;
	int hi_surrogate = -1;

	Assert(l % 6 /* \uXXXX */ == 0);

	for(i = 0; i < l / 6; i++)
	{
		ch = 0;

		for(j=0; j<4; j++)
			ch = (ch << 4) | hexval(s[ i*6 + 2 + j]);

		if (ch >= 0xd800 && ch <= 0xdbff)
		{
			if (hi_surrogate != -1)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
						 errmsg("invalid input syntax for type jsquery"),
						 errdetail("Unicode high surrogate must not follow a high surrogate.")));
			hi_surrogate = (ch & 0x3ff) << 10;
			continue;
		}
		else if (ch >= 0xdc00 && ch <= 0xdfff)
		{
			if (hi_surrogate == -1)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
						 errmsg("invalid input syntax for type jsquery"),
						 errdetail("Unicode low surrogate must follow a high surrogate.")));
			ch = 0x10000 + hi_surrogate + (ch & 0x3ff);
			hi_surrogate = -1;
		}

		if (hi_surrogate != -1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("invalid input syntax for type jsquery"),
					 errdetail("Unicode low surrogate must follow a high surrogate.")));

		/*
		 * For UTF8, replace the escape sequence by the actual
		 * utf8 character in lex->strval. Do this also for other
		 * encodings if the escape designates an ASCII character,
		 * otherwise raise an error. We don't ever unescape a
		 * \u0000, since that would result in an impermissible nul
		 * byte.
		 */

		if (ch == 0)
		{
			addstring(false, "\\u0000", 6);
		}
		else if (GetDatabaseEncoding() == PG_UTF8)
		{
			char utf8str[5];
			int utf8len;

			unicode_to_utf8(ch, (unsigned char *) utf8str);
			utf8len = pg_utf_mblen((unsigned char *) utf8str);
			addstring(false, utf8str, utf8len);
		}
		else if (ch <= 0x007f)
		{
			/*
			 * This is the only way to designate things like a
			 * form feed character in JSON, so it's useful in all
			 * encodings.
			 */
			addchar(false, (char) ch);
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("invalid input syntax for type jsquery"),
					 errdetail("Unicode escape values cannot be used for code point values above 007F when the server encoding is not UTF8.")));
		}

		hi_surrogate = -1;
	}
}


