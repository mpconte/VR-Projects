// This is not a YACC file.  It is a Lemon file
// See http://www.hwaci.com/sw/lemon/lemon.html
// Lemon is re-entrant and better encapsulated than YACC...

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bluescript.h>
#include "bsexpr.h"  /* constants and expression types */

static BSExprNode *mknode(int type) {
  BSExprNode *n;
  n = bsAllocObj(BSExprNode);
  n->type = type;
  return n;
}

}

// Set prefix for all functions...
%name bsExprParser

%token_prefix BSEXPR_
%token_type {BSExprNode *}
%token_destructor{ bsExprFreeExpr($$); }

%left OP_AND.
%left OP_OR.
%right OP_EQ OP_NE.
%right OP_LT OP_LE OP_GT OP_GE.
%left OP_ADD OP_SUB.
%left OP_MULT OP_DIV OP_MOD.
%right OP_NOT OP_NEG.

%type expr {BSExprNode *}
%type arglist {BSExprNode *}
%extra_argument {BSExprParserResult *parse_r}

%destructor expr { bsExprFreeExpr($$); }
%destructor arglist { bsExprFreeExpr($$); }

%parse_accept {
  if (parse_r) {
    parse_r->failed = 0;
  }
}

%parse_failure {
  if (parse_r) {
    if (bsIsResultClear(parse_r->interp))
      bsSetStringResult(parse_r->interp,
			"syntax error while parsing expression",
			BS_S_STATIC);
    parse_r->failed = 1;
  }
}

// Break down for expressions
bsexpr ::= expr(A). {
  if (parse_r) {
    parse_r->root = A;
  }
}

// Binary operators
expr(A) ::= expr(B) OP_ADD expr(C). {
  A = mknode(BSEXPR_OP_ADD);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_SUB expr(C). {
  A = mknode(BSEXPR_OP_SUB);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_MULT expr(C). {
  A = mknode(BSEXPR_OP_MULT);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_DIV expr(C). {
  A = mknode(BSEXPR_OP_DIV);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_MOD expr(C). {
  A = mknode(BSEXPR_OP_MOD);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_AND expr(C). {
  A = mknode(BSEXPR_OP_AND);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_OR expr(C). {
  A = mknode(BSEXPR_OP_OR);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_EQ expr(C). {
  A = mknode(BSEXPR_OP_EQ);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_NE expr(C). {
  A = mknode(BSEXPR_OP_NE);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_LT expr(C). {
  A = mknode(BSEXPR_OP_LT);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_LE expr(C). {
  A = mknode(BSEXPR_OP_LE);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_GT expr(C). {
  A = mknode(BSEXPR_OP_GT);
  A->sub = B;
  B->next = C;
}
expr(A) ::= expr(B) OP_GE expr(C). {
  A = mknode(BSEXPR_OP_GE);
  A->sub = B;
  B->next = C;
}

// Unary operators
expr(A) ::= OP_SUB expr(B). [OP_NOT] {
  A = mknode(BSEXPR_OP_NEG);
  A->sub = B;
}
expr(A) ::= OP_NOT expr(B). {
  A = mknode(BSEXPR_OP_NOT);
  A->sub = B;
}


// Other constructs
expr(A) ::= L_LPAREN expr(B) L_RPAREN. {
  A = B;
}

expr(A) ::= L_STRING(B). { A = B; }
expr(A) ::= L_FLOAT(B). { A = B; }
expr(A) ::= L_INT(B). { A = B; }
expr(A) ::= L_VAR(B). { A = B; }
expr(A) ::= L_PROC(B). { A = B; }
expr(A) ::= L_FUNC(B) L_LPAREN L_RPAREN. {
  /* function w/o arguments */
  A = B;
}

expr(A) ::= L_FUNC(B) L_LPAREN arglist(C) L_RPAREN. {
  /* function w/arguments */
  BSExprNode *e;
  A = B;
  A->data.func.nargs = 0;
  e = C;
  while (e) {
    A->data.func.nargs++;
    e = e->next;
  }
  if (A->data.func.nargs > 0) {
    /* allocate buffer for evaluating */
    A->data.func.args = bsAlloc(A->data.func.nargs*
				sizeof(BSExprResult),0);
  }
  A->sub = C;
}

arglist(A) ::= expr(B). {
  A = B;
}

arglist(A) ::= expr(B) L_COMMA arglist(C). {
  B->next = C;
  A = B;
}

