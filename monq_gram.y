%{
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>

    #include "monq.h"

    MQuery *RET;

    typedef struct yy_buffer_state *YY_BUFFER_STATE;
    extern int yylex();
    extern void yyerror(char *s);
    extern int yyparse();
    extern YY_BUFFER_STATE yy_scan_string(char * str);
    extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

    void 
    yyerror(char *s) 
    { 
        elog(ERROR,"%s",s);
        exit(0);
    }

    MQuery*
    parse(char *str)
    {
        YY_BUFFER_STATE  buffer = yy_scan_string(str);
        
        yyparse();
        yy_delete_buffer(buffer);

        return RET;    
    }
%}

/* Types of query tree nodes and leafs */
%union 
{
    MQuery                  *mquery;
    Expression              *exp;
    Clause                  *cl;
    MValue                  *vl;
    LeafValue               *lv;
    List                    *list;
    WhereClauseValue        *wcv;
    char                    *strval;
    int                      intval;
    double                   dubval;
    MArray                  *arrval;
    bool                     boolval;
    ArrayOperatorType        aop_type;
    ExpressionOperatorType   exop_type;
    ValueOperatorType        valop_type;
    OperatorObject          *oob;
    Operator                *op;
    ElemMatchOperator       *elemMatchOp;
}

%type<mquery>     QUERY
%type<exp>        EXPRESSION
%type<cl>         CLAUSE TEXT_CLAUSE EXPRESSION_TREE_CLAUSE LEAF_CLAUSE COMMENT_CLAUSE WHERE_CLAUSE
%type<vl>         VALUE 

%type<strval>     KEY KEY_STRING
%type<op>         OPERATOR
%type<oob>        OPEARATOR_OBJECT

%type<strval>     LSCOPE RSCOPE COMMA
    
%type<list>       OPERATOR_LIST LEAF_VALUE_LIST EXPRESSION_LIST CLAUSE_LIST

%type<wcv>        WHERE_CLAUSE_VALUE
%type<strval>     WHERE_OPERATOR
%token            WHERE_OPERATOR

/* OPERATORS */

/* Tree clause */
%type<exop_type>    TREE_OPERATOR OR NOR AND
%token              OR NOR AND

/* Leaf value operator */
%type<valop_type>    EQ LESS GREAT LESSEQ GREATEQ NOTEQ TYPE SIZE EXISTS NOT VALUE_OPERATOR
%token               EQ NOTEQ LESS LESSEQ GREAT GREATEQ TYPE SIZE EXISTS NOT

/* Array operator */
%type<aop_type>   IN NIN ALL ARRAY_OPERATOR
%token            IN NIN ALL

/* Mod operator */
%type<strval>     MOD_OPERATOR 
%type<lv>         DIVISOR REMAINDER
%token            MOD_OPERATOR

/* ElemMatch operator */
%type<op>            ELEMMATCH_OPERATOR
%type<strval>        ELEMMATCH
%token               ELEMMATCH

/* Comment clause */
%type<strval>     COMMENT_OPERATOR
%token            COMMENT_OPERATOR

/* Text clause */
%type<strval>     DIACRITIC_SENSITIVE_OPERATOR CASE_SENSITIVE_OPERATOR LANGUAGE_OPERATOR SEARCH_OPERATOR TEXT_OPERATOR
%token            DIACRITIC_SENSITIVE_OPERATOR CASE_SENSITIVE_OPERATOR LANGUAGE_OPERATOR SEARCH_OPERATOR TEXT_OPERATOR

/* Type of values */
%type<lv> LEAF_VALUE
%type<strval> INT
%type<strval> STRING
%type<strval> DOUBLE
%type<arrval> ARRAY
%type<boolval> BOOLEAN    
%token INT STRING DOUBLE BOOLEAN KEY_STRING

/* Scope types  */
%token LSCOPE RSCOPE COMMA LSQBRACKET RSQBRACKET LRBRACKET RRBRACKET

%start QUERY

%%
QUERY                   : EXPRESSION {$$ = createQuery($1); RET=$$; }
                        ;

EXPRESSION              : LSCOPE CLAUSE_LIST RSCOPE { $$ = createExpression($2); }
                        ;

CLAUSE_LIST             : CLAUSE COMMA CLAUSE_LIST  { $$ = addClause($1, $3); }
                        | CLAUSE                    { $$ = lappend(NULL, $1); }
                        ;

CLAUSE                  : LEAF_CLAUSE             
                        | COMMENT_CLAUSE         
                        | WHERE_CLAUSE            
                        | EXPRESSION_TREE_CLAUSE  
                        | TEXT_CLAUSE             
                        ;


/* TEXT CLAUSE SECTION */

TEXT_CLAUSE             : LSCOPE TEXT_OPERATOR EQ LSCOPE SEARCH_OPERATOR EQ KEY 
                            RSCOPE RSCOPE { $$ = createTextClause($7, false, "", false, false); }
                        | LSCOPE TEXT_OPERATOR EQ LSCOPE SEARCH_OPERATOR EQ KEY COMMA
                            LANGUAGE_OPERATOR EQ STRING COMMA
                            CASE_SENSITIVE_OPERATOR EQ BOOLEAN COMMA
                            DIACRITIC_SENSITIVE_OPERATOR EQ BOOLEAN RSCOPE RSCOPE { $$ = createTextClause($7, false, $11, $15, $19); }
                        ;

/* END OF SECTION */

/*WHERE CLAUSE SECTION*/
                
WHERE_CLAUSE            : LSCOPE WHERE_OPERATOR EQ WHERE_CLAUSE_VALUE RSCOPE { $$ = createWhereClause($4); }
                        ;

WHERE_CLAUSE_VALUE      : KEY { $$ = stringToWhereClauseValue($1); }
                        ;
/* END OF SECTION */   

/*COMMENT CLAUSE SECTION*/
COMMENT_CLAUSE          : LSCOPE COMMENT_OPERATOR EQ STRING RSCOPE { $$ = createCommentClause($2, $4); }
                        ;
/* END OF SECTION */

/*TREE CLAUSE SECTION*/

EXPRESSION_TREE_CLAUSE  : TREE_OPERATOR EQ LSQBRACKET EXPRESSION_LIST RSQBRACKET { $$ = createExpressionTreeClause($1, $4); }
                        | LSCOPE EXPRESSION_TREE_CLAUSE RSCOPE                   { $$ = $2; }
                        ;

EXPRESSION_LIST         : EXPRESSION                       { $$ = lcons($1, NULL); }
                        | EXPRESSION COMMA EXPRESSION_LIST { $$ = addExpression($1, $3); }
                        ;

TREE_OPERATOR           : OR | AND | NOR ;

/* END OF SECTION */

/* LEAF CLAUSE SECTION */
LEAF_CLAUSE             : KEY EQ VALUE { $$ = createLeafClause($1, $3); }
                        ;

KEY                     : STRING
                        | KEY_STRING
                        ;

VALUE                   : LEAF_VALUE       { $$ = createLeafValueValue($1); }
                        | OPEARATOR_OBJECT { $$ = createOperatorObjectValue($1); }
                        ;

OPEARATOR_OBJECT        : LSCOPE OPERATOR_LIST RSCOPE { $$ = createOperatorObject($2); }
                        ;

OPERATOR_LIST           : OPERATOR                     { $$ = lappend(NULL, $1); }
                        | OPERATOR COMMA OPERATOR_LIST { $$ = addOperator($1, $3); }
                        ;

OPERATOR                : VALUE_OPERATOR EQ LEAF_VALUE                                  { $$ = createValueOperator($1, $3); }
                        | ARRAY_OPERATOR EQ ARRAY                                       { $$ = createArrayOperator($1, $3); }
                        | MOD_OPERATOR EQ LSQBRACKET DIVISOR COMMA REMAINDER RSQBRACKET { $$ = createModOperator($4, $6); }
                        | NOT EQ LSCOPE OPERATOR RSCOPE                                 { $$ = createNotOperator($4); }
                        | ELEMMATCH EQ ELEMMATCH_OPERATOR                               { $$ = $3; }
                        ;

VALUE_OPERATOR          : EQ | NOTEQ | LESS | LESSEQ | GREAT | GREATEQ | TYPE | SIZE | EXISTS 
                        ;

ELEMMATCH_OPERATOR      : OPEARATOR_OBJECT  { $$ = createElemMatchOperatorOpObject($1); }
                        | EXPRESSION        { $$ = createElemMatchOperatorExpression($1); }
                        ; 

DIVISOR                 : LEAF_VALUE
                        ;

REMAINDER               : LEAF_VALUE
                        ;

ARRAY                   : LSQBRACKET LEAF_VALUE_LIST RSQBRACKET {$$ = createNewArray($2); }
                        ;

ARRAY_OPERATOR          : IN | NIN | ALL
                        ;

LEAF_VALUE_LIST         : LEAF_VALUE                         { $$ = lcons($1, NULL); }
                        | LEAF_VALUE COMMA LEAF_VALUE_LIST   { $$ = addArrayElement($1, $3); }
                        ;

LEAF_VALUE              : INT         { $$ = createIntegerValue($1); }
                        | STRING      { $$ = createStringValue($1); }
                        | KEY_STRING  { 
                                            StringInfoData strf;
                                            initStringInfo(&strf);
                                            appendStringInfo(&strf, "\"%s\"", $1);
                                            $$ = createStringValue(strf.data); 
                                      }
                        | DOUBLE      { $$ = createDoubleValue($1); }
                        | ARRAY       { $$ = createArrayValue($1); }
                        | BOOLEAN     { $$ = createBooleanValue($1); }
                        ;

/* END OF SECTION */
%%