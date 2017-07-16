#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "c.h"
#include "postgres.h"
#include "access/gin.h"
#include "utils/numeric.h"

#include "monq.h"


static void deleteValueOperator(ValueOperator *vop);
static void deleteOperator(Operator *op);
static void deleteNotOperator(NotOperator *op);
static void deleteElemMatchOperator(ElemMatchOperator *elemMatchOperator);
static void deleteOperatorObject(OperatorObject *op_object);
static void deleteLeafClauseValue(MValue *val);
static void deleteLeafClause(LeafClause *lc);
static void deleteExpressionClause(ExpressionClause* expClause);
static void deleteTextClause(TextClause *tClause);
static void deleteClause(Clause *cl);
static void deleteExpression(Expression * ex);
static void deleteLeafValue(LeafValue *value);
static void deleteArraySequence(MArray *ar);
static void deleteArrayOperator(ArrayOperator *aop);


static void
deleteValueOperator(ValueOperator *vop)
{   
    deleteLeafValue(vop->value);
    pfree(vop);
}

static void
deleteOperator(Operator *operator)
{
    switch(operator->type)
    {
        case NOP :
            deleteNotOperator((NotOperator*) operator );
            break;
        case AOP :
            deleteArrayOperator((ArrayOperator*) operator );
            break;
        case VOP :
            deleteValueOperator((ValueOperator*) operator );
            break;
        case EOP :
            deleteElemMatchOperator((ElemMatchOperator*) operator);
            break;
        case MOP :
            break;
    }
}

static void
deleteElemMatchOperator(ElemMatchOperator *elemMatchOperator)
{
    switch(elemMatchOperator->typeOfValue)
    {
        case E_EXPRESSION:
            deleteExpression(elemMatchOperator->expression);
            break;
        case E_OP_OBJECT:
            deleteOperatorObject(elemMatchOperator->operatorOpbject);
            break;
    }
}

static void 
deleteNotOperator(NotOperator *op)
{
    deleteOperator(op->op);
    pfree(op);
}

static void
deleteOperatorObject(OperatorObject *op_object)
{
    List        *operatorList;
    ListCell    *cell;
    
    operatorList = op_object->operatorList;

    foreach(cell, operatorList)
        deleteOperator((Operator *)lfirst(cell));
        
    pfree(op_object->operatorList);
    pfree(op_object);
}

static void
deleteLeafClauseValue(MValue *value)
{ 
    value->type ? deleteOperatorObject(value->oob) : deleteLeafValue(value->lv);
}

static void
deleteLeafClause(LeafClause *lc)
{
    deleteLeafClauseValue(lc->vl);
    pfree(lc);
}

static void
deleteExpressionClause(ExpressionClause* expClause)
{
    List        *expressionList;
    ListCell    *cell;

    expressionList = expClause->expressionList;

    foreach(cell, expressionList)
        deleteExpression((Expression *)lfirst(cell));
    
    pfree(expressionList);
    pfree(expClause);
}

static void
deleteTextClause(TextClause *textClause)
{
    pfree(textClause);
}

static void
deleteClause(Clause *clause)
{  
    switch(clause->type)
    {
        case LEAF :
            deleteLeafClause((LeafClause*) clause);
            break;
        case TEXT :
            deleteTextClause((TextClause*) clause);
            break;
        case EXPRESSION :
            deleteExpressionClause((ExpressionClause*) clause);
            break;
        default :
            break;
    }
}

static void
deleteExpression(Expression * expression)
{
    List        *clauseList = expression->clauseList;
    ListCell    *cell;
   
    foreach(cell, clauseList)
        deleteClause((Clause *)lfirst(cell));
    
    pfree(expression);
}

static void
deleteLeafValue(LeafValue *value)
{
    if(value->type == A)
        deleteArraySequence(value->ar);
    pfree(value);
}

static void
deleteArraySequence(MArray *marray)
{ 
    List        *arrayList = marray->arrayList;
    ListCell    *cell;
    
    foreach(cell, arrayList)
        deleteLeafValue((LeafValue *)lfirst(cell));

    pfree(arrayList);
    pfree(marray);
}

static void
deleteArrayOperator(ArrayOperator *arrayOperator)
{        
    deleteArraySequence(arrayOperator->ar);
    pfree(arrayOperator);  
}

void
deleteMquery(MQuery *query)
{
    deleteExpression(query->exp);
    pfree(query);
}