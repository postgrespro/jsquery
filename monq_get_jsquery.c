#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "monq.h"


static void getExpression(StringInfo strInfo, Expression * expression);
static void getClause(StringInfo strInfo, Clause *clause);
static void getExpressionClause(StringInfo strInfo, ExpressionClause* expClause);
static void getTextClause(StringInfo strInfo, TextClause* textClause);
static void getLeafClause(StringInfo strInfo, LeafClause *leafClause);
static void getLeafClauseValue(StringInfo strInfo, char *key, MValue *value);
static void getLeafValueEq(StringInfo strInfo, char *key, LeafValue *leafValue);
static void getOperatorObject(StringInfo strInfo, char *key, OperatorObject *opObject);
static void getOperator(StringInfo strInfo, char *key, Operator *operator);
static void getNotOperator(StringInfo strInfo, char *key, NotOperator *notOperator);
static void getElemMatchOperator(StringInfo strInfo, char *key, ElemMatchOperator *elemMatchOperator);
static void getArrayOperator(StringInfo strInfo, char *key, ArrayOperator *arOperator);
static void getArraySequence(StringInfo strInfo, MArray *marray);
static void getLeafValue(StringInfo strInfo, LeafValue *value);
static void getValueOperator(StringInfo strInfo, char *key, ValueOperator *valOperator);
static void getValueOperatorType(StringInfo strInfo, ValueOperatorType type);
static void getValueType(StringInfo strInfo, char *type);

static void
getValueOperatorType(StringInfo strInfo, ValueOperatorType type)
{
    switch(type)
    {
        case _LESS :
            appendStringInfo(strInfo, "<");
            break;
        case _EQ :
        case _NOTEQ :
            appendStringInfo(strInfo, "=");
            break;
        case _LESSEQ :
            appendStringInfo(strInfo, "<=");
            break;
        case _GREAT :
            appendStringInfo(strInfo, ">");
            break;
        case _GREATEQ :
            appendStringInfo(strInfo, ">=");
            break;
        case _TYPE :
            break;
        case _SIZE :
            appendStringInfo(strInfo, ".@# =");
            break;
        case _EXISTS :
            appendStringInfo(strInfo, "= *");
            break;
        default :
            elog(ERROR,"This value operator is not supported");
            break;
    }
}

/*
 * Return type in jsquery format
 */
static void
getValueType(StringInfo strInfo, char *type)
{
    if(strcmp(type,"\"string\"") == 0)              appendStringInfo(strInfo, " IS STRING");
    else if(
                strcmp(type, "\"double\"") == 0 || 
                strcmp(type, "\"int\"") == 0 || 
                strcmp(type, "\"long\"") == 0 || 
                strcmp(type, "\"decimal\"") == 0
            )                                       appendStringInfo(strInfo, " IS NUMERIC");
                             
    else if(strcmp(type, "\"array\"") == 0)         appendStringInfo(strInfo, " IS ARRAY");
    else if(strcmp(type, "\"object\"") == 0)        appendStringInfo(strInfo, " IS OBJECT");
    else if(strcmp(type, "\"bool\"") == 0)          appendStringInfo(strInfo, " IS BOOLEAN");
    else                                                  
        elog(ERROR, "Jsquery is not supported MongoDB %s value type", type);
}

static void
getValueOperator(StringInfo strInfo, char *key, ValueOperator *valOperator)
{
    if(valOperator->value_op == _EXISTS)
    {
        if(valOperator->value->b)
        {
            appendStringInfo(strInfo, "%s", key);
            appendStringInfo(strInfo, " ");
            getValueOperatorType(strInfo, valOperator->value_op);
        }
        else
        {
            appendStringInfo(strInfo, "NOT (%s ", key);
            getValueOperatorType(strInfo, valOperator->value_op);
            appendStringInfo(strInfo, ")");
        }
    }
    else if(valOperator->value_op == _TYPE)
    {
        appendStringInfo(strInfo, "%s ", key);
        getValueType(strInfo, valOperator->value->str);
    }
    else if(valOperator->value_op == _NOTEQ)
    {
        appendStringInfo(strInfo, "NOT (%s ", key);
        getValueOperatorType(strInfo, valOperator->value_op);
        appendStringInfo(strInfo, " ");
        getLeafValue(strInfo, valOperator->value);
        appendStringInfo(strInfo, ")");
    }
    else
    {
        appendStringInfo(strInfo, "%s ", key);
        getValueOperatorType(strInfo, valOperator->value_op);
        appendStringInfo(strInfo, " ");
        getLeafValue(strInfo, valOperator->value);
    }
}

static void
getElemMatchOperator(StringInfo strInfo, char *key, ElemMatchOperator *elemMatchOperator)
{   
    appendStringInfo(strInfo, "%s.#:(", key);
    switch(elemMatchOperator->typeOfValue)
    {
        case E_EXPRESSION:
            getExpression(strInfo, elemMatchOperator->expression);
            break;
        case E_OP_OBJECT:
            getOperatorObject(strInfo, "$",elemMatchOperator->operatorOpbject);
            break;
    }
    appendStringInfo(strInfo, ")");
}

static void
getOperator(StringInfo strInfo, char *key, Operator *operator)
{
    switch(operator->type)
    {
        case NOP :
            getNotOperator(strInfo, key, (NotOperator*) operator);
            break;
        case MOP :
            elog(ERROR, "MongoDB module operator is not supported by jsquery");
        case AOP :
            getArrayOperator(strInfo, key, (ArrayOperator*) operator);
            break;
        case VOP :
            getValueOperator(strInfo, key, (ValueOperator*) operator);
            break;
        case EOP :
            getElemMatchOperator(strInfo, key, (ElemMatchOperator*) operator);
            break;
        default  :
            elog(ERROR, "This mongoDB operator is not supported by jsquery");
    }
}

static void
getNotOperator(StringInfo strInfo, char *key, NotOperator *notOperator)
{
    appendStringInfo(strInfo, "NOT ("); 
    getOperator(strInfo, key,notOperator->op);
    appendStringInfo(strInfo, ")");
}

static void
getOperatorObject(StringInfo strInfo, char *key, OperatorObject *opObject)
{
    ListCell    *cell;
    bool         first = true;

    foreach(cell, opObject->operatorList)
    {
        if(first)
        {
            appendStringInfo(strInfo, "("); 
            getOperator(strInfo, key, (Operator *)lfirst(cell));
            appendStringInfo(strInfo, ")");
            first = false;
        }
        else 
        {          
            appendStringInfo(strInfo, " AND ("); 
            getOperator(strInfo, key, ((Operator *)lfirst(cell)));
            appendStringInfo(strInfo, ")"); 
        }
    }
}

static void
getLeafValueEq(StringInfo strInfo, char *key, LeafValue *leafValue)
{
    appendStringInfo(strInfo, "%s = ", key);
    getLeafValue(strInfo, leafValue);
}

static void
getLeafClauseValue(StringInfo strInfo, char *key, MValue *value)
{ 
    if(value->type) 
        getOperatorObject(strInfo, key, value->oob);
    else
        getLeafValueEq(strInfo, key, value->lv);
}

static void
getLeafClause(StringInfo strInfo, LeafClause *leafClause)
{
    getLeafClauseValue(strInfo, leafClause->key, leafClause->vl);
}

static void
getExpressionClause(StringInfo strInfo, ExpressionClause* expClause)
{
    ListCell    *cell;
    bool         first = true;
    char		*expOperator = NULL;

    switch(expClause->op)
    {
        case _AND :
            expOperator = "AND";
            break;
        case _OR :
            expOperator = "OR";
            break;
        case _NOR :
            expOperator = "OR NOT";
            break;
    }

    foreach(cell, expClause->expressionList)
    {
        if(first)
        {
            if(expClause->op == _NOR)   appendStringInfo(strInfo, "NOT ");

            appendStringInfo(strInfo, "(");
            getExpression(strInfo, (Expression *)lfirst(cell));
            appendStringInfo(strInfo, ") ");
        
            first = false;
        }
        else
        {
            appendStringInfo(strInfo, "%s (", expOperator);
            getExpression(strInfo, (Expression *)lfirst(cell));
            appendStringInfo(strInfo, ") ");
        }
    }   
}

static void
getTextClause(StringInfo strInfo, TextClause *textClause)
{
    appendStringInfo(strInfo, "* = %s", textClause->search_str);
}

static void
getClause(StringInfo strInfo, Clause *clause)
{  
    switch(clause->type)
    {
        case LEAF :
            getLeafClause(strInfo, (LeafClause*) clause);
            break;
        case COMMENT :
            elog(ERROR, "MongoDB comment clause is not supported by jsquery");
        case TEXT :
            getTextClause(strInfo, (TextClause*) clause);
            break;
        case WHERE :
            elog(ERROR, "MongoDB where clause is not supported by jsquery");
        case EXPRESSION :
            getExpressionClause(strInfo, (ExpressionClause*) clause);
            break;
        default:
            break;
    }
}

static void
getExpression(StringInfo strInfo, Expression *expression)
{
    ListCell    *cell;
    bool         first = true;
    
    foreach(cell, expression->clauseList)
    {
        if (first)
        {
            getClause(strInfo, (Clause *)lfirst(cell));
            first = false;
        }
        else
        {
            appendStringInfo(strInfo, " AND ");
            getClause(strInfo, (Clause *)lfirst(cell));
        }
    }
}

static void
getLeafValue(StringInfo strInfo, LeafValue *value)
{
    switch(value->type)
    {
        case S :
            appendStringInfo(strInfo, "%s", value->str);
            break;
        case I :
            appendStringInfo(strInfo, "%s", value->i);
            break;
        case A :
            appendStringInfo(strInfo, "[");
            getArraySequence(strInfo, value->ar);
            appendStringInfo(strInfo, "]");
            break;
        case B :
            appendStringInfo(strInfo, "%s", (value->b ? "true" : "false"));
            break;
        case D :
            appendStringInfo(strInfo, "%s", value->d);
            break;
        default :
            break;
    }
}

static void
getArraySequence(StringInfo strInfo, MArray *marray)
{ 
    ListCell    *cell;
    bool         first = true;
   
    foreach(cell, marray->arrayList)
    {
        if(first)
        {
            getLeafValue(strInfo, (LeafValue *)lfirst(cell));
            first = false;
        }
        else
        {
            appendStringInfo(strInfo, ", ");
            getLeafValue(strInfo, (LeafValue *)lfirst(cell));
        }
    }
}

static void
getArrayOperator(StringInfo strInfo, char *key, ArrayOperator *arOperator)
{        
    switch(arOperator->array_op)
    {
        case _IN :
            appendStringInfo(strInfo, "%s IN (", key);
            getArraySequence(strInfo, arOperator->ar);
            appendStringInfo(strInfo, ")");
            break;
        case _NIN:
            appendStringInfo(strInfo, "NOT (%s IN (", key);
            getArraySequence(strInfo, arOperator->ar);
            appendStringInfo(strInfo, "))");
            break;
        case _ALL:
            appendStringInfo(strInfo, "%s @> [", key);
            getArraySequence(strInfo, arOperator->ar);
            appendStringInfo(strInfo, "]");
            break;
        default  :
            break;
    }
}

char *
getJsquery(MQuery *qu)
{
    StringInfoData strInfo;
    initStringInfo(&strInfo);
    getExpression(&strInfo, qu->exp);
    deleteMquery(qu);
    return strInfo.data;
}