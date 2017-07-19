%{
    #include <stdlib.h>
    #include <stdio.h>
 	#include "monq.h"
    #include "monq_gram.h"
    #include <string.h>

	extern int yylex();
    extern void yyerror(char *s);

    int position = 1;
    char *inputString;
    
    void 
    yyerror(char *s) 
    { 
        int val;
        position -= yyleng;
        val = position;
        position = 1;
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("bad MongoDB representation"),
                 /* translator: first %s is typically "syntax error" */
                 errdetail("%s \"%s\" on line %d position %d\n%s\n%*c", s, yytext, yylineno, val,inputString,val,'^')));

        exit(0);
    }
%}

%option yylineno
%option noyywrap
%option nounput
%option noinput

%%

[/][/].*\n      		; // comment

[0-9]+							{ yylval.strval=strdup(yytext); position += yyleng; return INT; }
[0-9]+\.[0-9]+					{ yylval.strval=strdup(yytext); position += yyleng; return DOUBLE; }
(true|false|TRUE|FALSE)			{ yylval.boolval=(strcmp(strdup(yytext),"true")==0); position += yyleng; return BOOLEAN; }

\{								{ position += yyleng; return LSCOPE; }
\}								{ position += yyleng; return RSCOPE; }
\[								{ position += yyleng; return LSQBRACKET; }
\]								{ position += yyleng; return RSQBRACKET; }

\,								{ position += yyleng; return COMMA; }

\:								{ position += yyleng; yylval.valop_type=_EQ; return EQ; }
\$(eq|EQ)						{ position += yyleng; yylval.valop_type=_EQ; return EQ; }
\$(lt|LT)						{ position += yyleng; yylval.valop_type=_LESS; return LESS; }
\$(lte|LTE)						{ position += yyleng; yylval.valop_type=_LESSEQ; return LESSEQ; }
\$(gt|GT)						{ position += yyleng; yylval.valop_type=_GREAT; return GREAT; }
\$(gte|GTE)						{ position += yyleng; yylval.valop_type=_GREATEQ; return GREATEQ; }
\$(ne|NE)						{ position += yyleng; yylval.valop_type=_NOTEQ; return NOTEQ; }
\$type							{ position += yyleng; yylval.valop_type=_TYPE; return TYPE; }
\$size							{ position += yyleng; yylval.valop_type=_SIZE; return SIZE; }
\$exists						{ position += yyleng; yylval.valop_type=_EXISTS; return EXISTS; }

\$in 							{ position += yyleng; yylval.aop_type=_IN; return IN; }
\$nin							{ position += yyleng; yylval.aop_type=_NIN; return NIN; }
\$all							{ position += yyleng; yylval.aop_type=_ALL; return ALL; }

\$not 							{ position += yyleng; return NOT;}

\$where							{ position += yyleng; return WHERE_OPERATOR; }

\$elemMatch						{ position += yyleng; return ELEMMATCH; }

\$or 							{ position += yyleng; yylval.exop_type=_OR; return OR; }
\$nor 							{ position += yyleng; yylval.exop_type=_NOR; return NOR; }
\$and							{ position += yyleng; yylval.exop_type=_AND; return AND; }

\$search						{ position += yyleng; return SEARCH_OPERATOR; }
\$text							{ position += yyleng; return TEXT_OPERATOR; }
\$language						{ position += yyleng; return LANGUAGE_OPERATOR; }
\$caseSensitive					{ position += yyleng; return CASE_SENSITIVE_OPERATOR; }
\$diacriticSensitive			{ position += yyleng; return DIACRITIC_SENSITIVE_OPERATOR; }

\$comment						{ position += yyleng; return COMMENT_OPERATOR; }

\$mod							{ position += yyleng; return MOD_OPERATOR; }

\"\"							{ position += yyleng; yylval.strval=strdup(yytext); return STRING; }

[0-9a-zA-Z]+                    { position += yyleng; yylval.strval=strdup(yytext); return KEY_STRING; }

\"[\.0-9a-zA-Z]*\"              { 
                                    char *str = strdup(yytext+1);
                                    str[yyleng-2] = '\0';
                                    yylval.strval = str;
                                    position += yyleng;
                                    return KEY_STRING;
                                }

\"[(\\\")(\\0)\;\!\@\#\$\%\^\&\*\(\)\.\, 0-9a-zA-Z]*\" { 
                                                            position += yyleng;
                                                            yylval.strval=strdup(yytext); 
                                                            return STRING; 
                                                        }

[ \t\r\n]      			; { position += yyleng; } // whitespace


%%