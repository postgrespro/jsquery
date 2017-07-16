%{
    #include <stdlib.h>
    #include <stdio.h>
 	#include "monq_structures.h"
    #include "monq_gram.h"
    #include <string.h>


	extern int yylex();

%}

%option yylineno
%option noyywrap
%option nounput
%option noinput

%%

[/][/].*\n      		; // comment

[0-9]+							{ yylval.strval=strdup(yytext); return INT; }
[0-9]+\.[0-9]+					{ yylval.strval=strdup(yytext); return DOUBLE; }
(true|false|TRUE|FALSE)			{ yylval.boolval=(strcmp(strdup(yytext),"true")==0); return BOOLEAN; }

\{								{ return LSCOPE; }
\}								{ return RSCOPE; }
\[								{ return LSQBRACKET; }
\]								{ return RSQBRACKET; }

\,								{ return COMMA; }

\:								{ yylval.valop_type=_EQ; return EQ; }
\$(eq|EQ)						{ yylval.valop_type=_EQ; return EQ; }
\$(lt|LT)						{ yylval.valop_type=_LESS; return LESS; }
\$(lte|LTE)						{ yylval.valop_type=_LESSEQ; return LESSEQ; }
\$(gt|GT)						{ yylval.valop_type=_GREAT; return GREAT; }
\$(gte|GTE)						{ yylval.valop_type=_GREATEQ; return GREATEQ; }
\$(ne|NE)						{ yylval.valop_type=_NOTEQ; return NOTEQ; }
\$type							{ yylval.valop_type=_TYPE; return TYPE; }
\$size							{ yylval.valop_type=_SIZE; return SIZE; }
\$exists						{ yylval.valop_type=_EXISTS; return EXISTS; }

\$in 							{ yylval.aop_type=_IN; return IN; }
\$nin							{ yylval.aop_type=_NIN; return NIN; }
\$all							{ yylval.aop_type=_ALL; return ALL; }

\$not 							{ return NOT;}

\$where							{ return WHERE_OPERATOR; }

\$elemMatch						{ return ELEMMATCH; }

\$or 							{ yylval.exop_type=_OR; return OR; }
\$nor 							{ yylval.exop_type=_NOR; return NOR; }
\$and							{ yylval.exop_type=_AND; return AND; }

\$search						{ return SEARCH_OPERATOR; }
\$text							{ return TEXT_OPERATOR; }
\$language						{ return LANGUAGE_OPERATOR; }
\$caseSensitive					{ return CASE_SENSITIVE_OPERATOR; }
\$diacriticSensitive			{ return DIACRITIC_SENSITIVE_OPERATOR; }

\$comment						{ return COMMENT_OPERATOR; }

\$mod							{ return MOD_OPERATOR; }

\"\"							{ yylval.strval=strdup(yytext); return STRING; }

[0-9a-zA-Z]+					{ yylval.strval=strdup(yytext); return KEY_STRING; }

\"[\.0-9a-zA-Z]*\"				{ 
									char *str = strdup(yytext+1);
									str[yyleng-2] = '\0';
									yylval.strval = str;
									return KEY_STRING;
								}

\"[(\\0)\.\, 0-9a-zA-Z]*\"		{ yylval.strval=strdup(yytext); return STRING; }

[ \t\r\n]      			; // whitespace


%%

/*MQuery*
parsemquery(const char *str, int len)
{
    YY_BUFFER_STATE  buffer = yy_scan_string(str);
    
    yyparse();
    yy_delete_buffer(buffer);

    return RET;    
}
*/
