#ifndef BSEXPR_H
#define BSEXPR_H

#include <bluescript.h>

#include "bsexprp.h" /* constants... */

typedef int (*BSExprFunc)(BSInterp *i, int nargs, BSExprResult *args,
			  BSExprResult *result);
typedef struct bs_expr_func_call {
  BSExprFunc func;
  int nargs;
  BSExprResult *args; /* buffer for args */
} BSExprFuncCall;

typedef struct bs_expr_node {
  int type;
  union {
    BSExprResult value;  /* defined in bluescript.h */
    BSExprFuncCall func; 
    BSObject *obj;
  } data;
  struct bs_expr_node *sub;  /* first child */
  struct bs_expr_node *next; /* next sibling */
} BSExprNode;

typedef struct bs_parse_result {
  BSInterp *interp;
  BSExprNode *root;
  int failed;
} BSExprParserResult;

/* maximum size of a function name */
#define BSEXPR_FUNCSZ 80

void bsExprFreeExpr(BSExprNode *);

#endif /* BSEXPR_H */
