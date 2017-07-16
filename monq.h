#ifndef _MONQ_H_
#define _MONQ_H_

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"   

typedef enum TypeOfLeafValue 
{ 
    S, 
    I, 
    A, 
    D, 
    B 
} TypeOfLeafValue;

typedef enum TypeOfOperator 
{ 
    NOP, 
    MOP, 
    AOP, 
    VOP,
    EOP
} TypeOfOperator;

typedef enum ArrayOperatorType 
{ 
    _IN, 
    _NIN, 
    _ALL
} ArrayOperatorType;

typedef enum ExpressionOperatorType 
{ 
    _OR, 
    _NOR, 
    _AND 
} ExpressionOperatorType;

typedef enum ValueOperatorType
{ 
    _LESS, 
    _EQ,
    _NOTEQ,
    _LESSEQ,
    _GREAT,
    _GREATEQ,
    _TYPE,
    _SIZE,
    _EXISTS  
} ValueOperatorType;


typedef enum TypeOfClause 
{ 
    LEAF, 
    COMMENT, 
    TEXT, 
    WHERE, 
    EXPRESSION 
} TypeOfClause;

typedef enum TypeOfValue 
{ 
    LF_VALUE, 
    OP_OBJECT 
} TypeOfValue;

typedef enum TypeOfElemMatchValue 
{ 
    E_EXPRESSION, 
    E_OP_OBJECT 
} TypeOfElemMatchValue;

typedef enum TypeOfWhereClauseValue 
{ 
    STR 
} TypeOfWhereClauseValue;

typedef struct MArray
{
	List 	*arrayList;
} MArray;

typedef struct LeafValue
{
    TypeOfLeafValue type;
    union
    {
        char    *str;
        char    *i;
        MArray  *ar;
        bool     b;
        char    *d;
    };
} LeafValue;

/* Operators */
typedef struct ValueOperator
{
    TypeOfOperator       type;

    ValueOperatorType    value_op;
    LeafValue           *value;
} ValueOperator;

typedef struct NotOperator
{
    TypeOfOperator       type;

    struct Operator     *op;
} NotOperator;

typedef struct ArrayOperator
{
    TypeOfOperator       type;

    ArrayOperatorType    array_op;
    MArray              *ar;
} ArrayOperator;

typedef struct ModOperator
{
    TypeOfOperator   type;

    LeafValue       *divisor;
    LeafValue       *remainder;
} ModOperator;

typedef struct Operator
{
    TypeOfOperator type;
} Operator;

typedef struct OperatorObject
{
    List *operatorList;
} OperatorObject;

typedef struct MValue
{
    TypeOfValue             type;
    union
    {
        LeafValue          *lv;
        OperatorObject     *oob;
    };
} MValue;

typedef struct LeafClause
{
    TypeOfClause    type;

    char           *key;
    MValue         *vl;
} LeafClause;

typedef struct CommentClause
{
    TypeOfClause    type;

    char           *op;
    char           *str;
} CommentClause;

typedef struct WhereClauseValue
{
    TypeOfClause              type;

    TypeOfWhereClauseValue    val_type;
    char                     *str;
} WhereClauseValue;

typedef struct WhereClause
{
    TypeOfClause         type;

    WhereClauseValue    *wcv;
} WhereClause;

typedef struct TextClause
{
    TypeOfClause    type;

    char           *search_str;
    bool            lang_op;
    char           *lang_str;
    bool            case_sense;
    bool            diacr_sense;
} TextClause;

typedef struct Clause
{
    TypeOfClause type;
} Clause;

typedef struct Expression
{
    List *clauseList;
} Expression;

typedef struct ElemMatchOperator
{
    TypeOfOperator          type;

    TypeOfElemMatchValue    typeOfValue;

    union
    {
        Expression         *expression;
        OperatorObject     *operatorOpbject;
    };
} ElemMatchOperator;

typedef struct ExpressionClause
{
    TypeOfClause               type;

    ExpressionOperatorType     op;
    List             		  *expressionList;
} ExpressionClause;

typedef struct MQuery
{
    Expression *exp;
    Datum jsQuery;
} MQuery;


extern MArray *createNewArray(List *arrayList);
extern List *addArrayElement(LeafValue * value, List *arrayList);
extern Operator *createNotOperator(Operator *op);
extern Operator *createModOperator(LeafValue *divisor, LeafValue *remainder);
extern Operator *createArrayOperator(ArrayOperatorType op, MArray *ar);
extern Operator *createValueOperator(ValueOperatorType op, LeafValue *value);
extern Operator *createElemMatchOperatorExpression(Expression *expression);
extern Operator *createElemMatchOperatorOpObject(OperatorObject *oob);
extern LeafValue *createStringValue(char *str);
extern LeafValue *createDoubleValue(char* d);
extern LeafValue *createIntegerValue(char* i);
extern LeafValue *createArrayValue(MArray *ar);
extern LeafValue *createBooleanValue(bool b);
extern List *addOperator(Operator *op, List *operatorList);
extern OperatorObject *createOperatorObject(List *operatorList);
extern MValue *createOperatorObjectValue(OperatorObject *oob);
extern MValue *createLeafValueValue(LeafValue *lv);
extern Clause *createLeafClause(char* key, MValue *vl);
extern Clause *createCommentClause(char *op, char *str);
extern WhereClauseValue *stringToWhereClauseValue(char *str);
extern Clause *createWhereClause(WhereClauseValue *wcv);
extern List *addClause(Clause *clause, List *clauseList);
extern Expression *createExpression(List *clauseList);
extern List *addExpression(Expression *exp, List *expressionList);
extern Clause *createExpressionTreeClause(ExpressionOperatorType op, List *expressionList);
extern Clause *createTextClause(char* search_str, bool lang_op, char* lang_str, bool case_sense, bool diacr_sense);
extern MQuery *createQuery(Expression *exp);
extern void deleteMquery(MQuery *qu);
extern char *getJsquery(MQuery *query);
extern MQuery *parse(char *str);

#endif