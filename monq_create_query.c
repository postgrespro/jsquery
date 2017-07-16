#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "monq.h"


MArray *
createNewArray(List *arrayList) 
{
    MArray *new_ar = (MArray *) palloc(sizeof(MArray));
    new_ar->arrayList = arrayList;
    return new_ar;
}

List *
addArrayElement(LeafValue *value, List *arrayList) 
{
    return lappend(arrayList, value);
}

Operator *
createNotOperator(Operator *op) 
{
    NotOperator *new_op = (NotOperator *) palloc(sizeof(NotOperator));
    new_op->type = NOP;
    new_op->op = op;
    return (Operator *) new_op;
}

Operator *
createModOperator(LeafValue *divisor, LeafValue *remainder) 
{
    ModOperator *new_op = (ModOperator *) palloc(sizeof(ModOperator));
    new_op->type = MOP;
    new_op->divisor = divisor;
    new_op->remainder = remainder;
    return (Operator *) new_op;
}

Operator *
createArrayOperator(ArrayOperatorType op, MArray *ar) 
{
    ArrayOperator *new_op = (ArrayOperator *) palloc(sizeof(ArrayOperator));
    new_op->type = AOP;
    new_op->array_op = op;
    new_op->ar = ar;
    return (Operator*) new_op;
}

Operator *
createValueOperator(ValueOperatorType op, LeafValue * value) 
{
    ValueOperator *new_op = (ValueOperator *) palloc(sizeof(ValueOperator));
    new_op->type = VOP;  
    new_op->value_op = op;
    new_op->value = value;
    return (Operator *) new_op;
}

Operator *
createElemMatchOperatorOpObject(OperatorObject *oob)
{
    ElemMatchOperator *new_op = (ElemMatchOperator *) palloc(sizeof(ElemMatchOperator));
    new_op->type = EOP;  
    new_op->typeOfValue = E_OP_OBJECT;
    new_op->operatorOpbject = oob;
    return (Operator *) new_op;
}

Operator *
createElemMatchOperatorExpression(Expression *expression)
{
    ElemMatchOperator *new_op = (ElemMatchOperator *) palloc(sizeof(ElemMatchOperator));
    new_op->type = EOP;  
    new_op->typeOfValue = E_EXPRESSION;
    new_op->expression = expression;
    return (Operator *) new_op;
}

LeafValue *
createStringValue(char *str)
{
    LeafValue *lv = (LeafValue *) palloc(sizeof(LeafValue));
    lv->type = S;
    lv->str = str;
    return lv;
}

LeafValue *
createDoubleValue(char* d)
{
    LeafValue *lv = (LeafValue *) palloc(sizeof(LeafValue));
    lv->type = D;
    lv->d = d;
    return lv;
}

LeafValue *
createIntegerValue(char* i)
{
    LeafValue *lv = (LeafValue *) palloc(sizeof(LeafValue));
    lv->type = I;
    lv->i = i;
    return lv;
}

LeafValue *
createArrayValue(MArray *ar)
{
  LeafValue *lv = (LeafValue *) palloc(sizeof(LeafValue));
  lv->type = A;
  lv->ar = ar;
  return lv;
}

LeafValue *
createBooleanValue(bool b)
{
  LeafValue *lv = (LeafValue *) palloc(sizeof(LeafValue));
  lv->type = B;
  lv->b = b;
  return lv;
}

List *
addOperator(Operator *op, List *operatorList) 
{  
    return lcons(op, operatorList);
}

OperatorObject *
createOperatorObject(List *operatorList) 
{
    OperatorObject *new_oob = (OperatorObject *) palloc(sizeof(OperatorObject));
    new_oob->operatorList = operatorList;
    return new_oob;
}

MValue *
createOperatorObjectValue(OperatorObject *oob) 
{
    MValue *vl = (MValue *) palloc(sizeof(MValue));
    vl->type = OP_OBJECT;
    vl->oob = oob;
    return vl;
}

MValue *
createLeafValueValue(LeafValue *lv) 
{
    MValue *vl = (MValue *) palloc(sizeof(MValue));
    vl->type = LF_VALUE;
    vl->lv = lv;
    return vl;
}

Clause *
createLeafClause(char* key, MValue *vl) 
{
    LeafClause *new_lc = (LeafClause *) palloc(sizeof(LeafClause));
    new_lc->type = LEAF;
    new_lc->key = key;
    new_lc->vl = vl;
    return ( Clause* ) new_lc;
}

Clause *
createCommentClause(char *op, char *str) 
{
    CommentClause *new_com_cl = (CommentClause *) palloc(sizeof(CommentClause));
    new_com_cl->type = COMMENT;
    new_com_cl->op = op;
    new_com_cl->str = str;
    return ( Clause* ) new_com_cl;
}

WhereClauseValue *
stringToWhereClauseValue(char *str)
{
    WhereClauseValue *wcv = (WhereClauseValue *) palloc(sizeof(WhereClauseValue));
    wcv->str = str;   
    return wcv;
}

Clause *
createWhereClause(WhereClauseValue *wcv)
{
    WhereClause *wc = (WhereClause *) palloc(sizeof(WhereClause));
    wc->type = WHERE;
    wc->wcv = wcv;
    return (Clause *) wc;
}

List *
addClause(Clause *clause, List *clauseList)
{
    return lappend(clauseList, clause);
}

Expression *
createExpression(List *clauseList)
{
    Expression *exp = (Expression *) palloc(sizeof(Expression));
    exp->clauseList = clauseList;
    return exp;
}

List *
addExpression(Expression *exp, List *expressionList)
{
    return lcons(exp, expressionList);
}

Clause *
createExpressionTreeClause(ExpressionOperatorType opType, List *expressionList)
{
    ExpressionClause *exp_cl = (ExpressionClause *) palloc(sizeof(ExpressionClause));
    exp_cl->type = EXPRESSION;
    exp_cl->op = opType;
    exp_cl->expressionList = expressionList;
    return (Clause *) exp_cl;
}

Clause *
createTextClause(char *search_str, bool lang_op, char *lang_str, bool case_sense, bool diacr_sense)
{
    TextClause *text_cl = (TextClause *) palloc(sizeof(TextClause));
    text_cl->type = TEXT;
    text_cl->search_str = search_str;
    text_cl->lang_op = lang_op;
    text_cl->lang_str = lang_str;
    text_cl->case_sense = case_sense;
    text_cl->diacr_sense = diacr_sense;
    return (Clause *) text_cl;
}

MQuery *
createQuery(Expression *exp)
{
    MQuery *qu = (MQuery *) palloc(sizeof(MQuery));
    qu->exp = exp;
    return qu;
}