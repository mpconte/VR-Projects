#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <bluescript.h>
#include "bsexpr.h"

#ifdef _WIN32
#define snprintf _snprintf
#endif /* _WIN32 */

/* parser interface */
extern void *bsExprParserAlloc(void *(*mallocProc)(size_t));
extern void bsExprParserFree(void *, void (*freeproc)(void *));
extern void bsExprParserTrace(FILE *, char *);
extern const char *bsExprParserTokenName(int);
extern void bsExprParser(void *, int, BSExprNode *, BSExprParserResult *);

#define streq(x,y) (((x)[0] == (y)[0]) && (strcmp((x),(y)) == 0))

/* Extracts a number from the source - could be integer, could be
   float - uses C syntax ... */
#undef isoctal
#define isoctal(c) ((c) >= '0' && c <= '7')

static BSExprNode *mknode(int type) {
  BSExprNode *n;
  n = bsAllocObj(BSExprNode);
  n->type = type;
  return n;
}

static void copyResult(BSExprResult *dst, BSExprResult *src) {
  dst->type = src->type;
  switch (dst->type) {
  case BS_T_STRING:
    bsStringInit(&(dst->data.stringValue));
    bsStringCopy(&(dst->data.stringValue),
		 &(src->data.stringValue));
    break;
  case BS_T_FLOAT:
    dst->data.floatValue = src->data.floatValue;
    break;
  case BS_T_INT:
    dst->data.intValue = src->data.intValue;
    break;
  }
}

static void initResult(BSExprResult *r) {
  r->type = 0;
}

static void clearResult(BSExprResult *r) {
  if (r->type == BS_T_STRING)
    bsStringFreeSpace(&(r->data.stringValue));
  r->type = 0;
}


static void setString(BSExprResult *r, char *s, int len) {
  if (len < 0)
    len = (s ? strlen(s) : 0);
  
  clearResult(r);
  bsStringInit(&(r->data.stringValue));
  bsStringSet(&(r->data.stringValue),s,len,BS_S_VOLATILE);
  r->type = BS_T_STRING;
}

static void setInt(BSExprResult *r, int x) {
  clearResult(r);
  r->data.intValue = x;
  r->type = BS_T_INT;
}

static void setFloat(BSExprResult *r, float x) {
  clearResult(r);
  r->data.floatValue = x;
  r->type = BS_T_FLOAT;
}

/* buffers for converting values */
#define VSZ 128

static int convertToString(BSExprResult *r) {
  char sbuf[VSZ];
  switch (r->type) {
  case BS_T_STRING:
    return 0; /* nothing to do */
  case BS_T_INT:
    snprintf(sbuf,VSZ,"%d",r->data.intValue);
    bsStringInit(&(r->data.stringValue));
    bsStringAppend(&(r->data.stringValue),sbuf,-1);
    r->type = BS_T_STRING;
    return 0;
  case BS_T_FLOAT:
    snprintf(sbuf,VSZ,"%g",r->data.intValue);
    bsStringInit(&(r->data.stringValue));
    bsStringAppend(&(r->data.stringValue),sbuf,-1);
    r->type = BS_T_STRING;
    return 0;
  case 0: /* special case - null */
    bsStringInit(&(r->data.stringValue));
    r->type = BS_T_STRING;
    return 0;

  default:
    BS_FATAL("convertToString - unexpected exprresult state");
  }
  BS_FATAL("convertToString - unreachable");
  return -1;
}

static int convertToFloat(BSInterp *i, BSExprResult *r) {
  switch (r->type) {
  case BS_T_STRING:
    {
      double d;
      char *chk;
      chk = bsStringPtr(&(r->data.stringValue));
      d = strtod(chk,&chk);
      if (*chk != '\0') {
	bsSetStringResult(i,"cannot convert string to float",
			  BS_S_STATIC);
	return -1;
      }
      bsStringFreeSpace(&(r->data.stringValue));
      r->data.floatValue = (BSFloat)d;
      r->type = BS_T_FLOAT;
    }
    return 0;

  case BS_T_FLOAT:
    return 0; /* nothing to do */

  case BS_T_INT:
    r->data.floatValue = (BSFloat)(r->data.intValue);
    r->type = BS_T_FLOAT;
    return 0;

  case 0: /* special case: null */
    bsSetStringResult(i,"cannot convert null value to float",
		      BS_S_STATIC);
    return -1;

  default:
    BS_FATAL("convertToFloat - unexpected exprresult state");
  }
  BS_FATAL("convertToFloat - unreachable");
  return -1;
}

static int convertToInt(BSInterp *i, BSExprResult *r) {
  switch (r->type) {
  case BS_T_STRING:
    {
      long d;
      char *chk;
      chk = bsStringPtr(&(r->data.stringValue));
      d = strtol(chk,&chk,0);
      if (*chk != '\0') {
	bsSetStringResult(i,"cannot convert string to integer",
			  BS_S_STATIC);
	return -1;
      }
      bsStringFreeSpace(&(r->data.stringValue));
      r->data.intValue = (BSInt)d;
      r->type = BS_T_INT;
    }
    return 0;

  case BS_T_INT:
    return 0; /* nothing to do */

  case BS_T_FLOAT:
    r->data.intValue = (BSInt)(r->data.floatValue);
    r->type = BS_T_INT;
    return 0;

  case 0: /* special case: null */
    bsSetStringResult(i,"cannot convert null value to int",
		      BS_S_STATIC);
    return -1;

  default:
    BS_FATAL("convertToFloat - unexpected exprresult state");
  }
  BS_FATAL("convertToFloat - unreachable");
  return -1;
}

static int getBoolean(BSExprResult *r) {
  /* prefer int */
  convertToInt(NULL,r) == 0 || convertToFloat(NULL,r) == 0;
  switch (r->type) {
  case BS_T_STRING:
    return (bsStringLength(&(r->data.stringValue)) > 0);
    
  case BS_T_INT:
    return (r->data.intValue ? 1 : 0);
    
  case BS_T_FLOAT:
    return (r->data.floatValue == 0.0 ? 0 : 1);

  case 0: /* special case - null value */
    return 0; /* always false */

  default:
    BS_FATAL("getBoolean: unexpected exprresult state");
  }
  
  BS_FATAL("getBoolean: unreachable");
  return 0;
}

/* functions */

static int to_int(BSInterp *i, int nargs, BSExprResult *args,
		  BSExprResult *result) {
  if (nargs != 1) {
    bsSetStringResult(i,"expected one arg to int()",BS_S_STATIC);
    return -1;
  }
  *result = args[0];
  return convertToInt(i,result);
}

static int to_float(BSInterp *i, int nargs, BSExprResult *args,
		  BSExprResult *result) {
  if (nargs != 1) {
    bsSetStringResult(i,"expected one arg to float()",BS_S_STATIC);
    return -1;
  }
  *result = args[0];
  return convertToFloat(i,result);
}

static int to_string(BSInterp *i, int nargs, BSExprResult *args,
		     BSExprResult *result) {
  if (nargs != 1) {
    bsSetStringResult(i,"expected one arg to string()",BS_S_STATIC);
    return -1;
  }
  *result = args[0];
  return convertToString(result);
}

static int to_boolean(BSInterp *i, int nargs, BSExprResult *args,
		      BSExprResult *result) {
  if (nargs != 1) {
    bsSetStringResult(i,"expected one arg to boolean()",BS_S_STATIC);
    return -1;
  }
  setInt(result,getBoolean(&(args[0])));
  return 0;
}

static struct {
  char *name;
  BSExprFunc func;
} expr_funcs[] = {
  { "int", to_int },
  { "float", to_float },
  { "string", to_string },
  { "boolean", to_boolean },
  { NULL, NULL }
};

static BSExprFunc find_func(char *name) {
  int k = 0;
  if (!name)
    return NULL;
  while (expr_funcs[k].name) {
    if (strcmp(name,expr_funcs[k].name) == 0)
      return expr_funcs[k].func;
    k++;
  }
  return NULL;
}

static int opNumArgs(int k) {
  switch (k) {
  case BSEXPR_OP_AND:
  case BSEXPR_OP_OR:
  case BSEXPR_OP_EQ:
  case BSEXPR_OP_NE:
  case BSEXPR_OP_LT:
  case BSEXPR_OP_LE:
  case BSEXPR_OP_GT:
  case BSEXPR_OP_GE:
  case BSEXPR_OP_ADD:
  case BSEXPR_OP_SUB:
  case BSEXPR_OP_MULT:
  case BSEXPR_OP_DIV:
  case BSEXPR_OP_MOD:
    return 2;

  case BSEXPR_OP_NOT:
  case BSEXPR_OP_NEG:
    return 1;
  }
  return 0;
}


static int extractNum(BSParseSource *p, BSString *buf, int *isfloat_r) {
  int c;
  int isfloat = 0;
  int seendigits = 0;
  c = bsGetc(p);

  if (c == '0') {
    bsStringAppChr(buf,c);
    c = bsGetc(p);
    if (c == 'x' || c == 'X') {
      /* hexadecimal (integer only) */
      bsStringAppChr(buf,c);
      c = bsGetc(p);
      while (isxdigit(c)) {
	bsStringAppChr(buf,c);
	c = bsGetc(p);
      }
      bsUngetc(p,c);
      isfloat = 0;
      goto cleanup;
    } else if (isoctal(c)) {
      /* octal - hexadecimal only */
      while (isoctal(c)) {
	bsStringAppChr(buf,c);
	c = bsGetc(p);
      }
      bsUngetc(p,c);
      isfloat = 0;
      goto cleanup;
    } else if (c == '.') {
      /* float - skip ahead... */
      seendigits = 1;
      goto fraction;
    } else if (c == 'e' || c == 'E') {
      /* float - skip ahead... */
      seendigits = 1;
      goto exponent;
    } else { /* ??? */
      bsUngetc(p,c);
      isfloat = 0;
      goto cleanup;
    }
  }

  if (isdigit(c)) {
    isfloat = 0;
    seendigits = 1;
    while (isdigit(c)) {
      bsStringAppChr(buf,c);
      c = bsGetc(p);
    }
  }

 fraction:
  if (c == '.') {
    bsStringAppChr(buf,c);
    isfloat = 1;
    c = bsGetc(p);
    if (isdigit(c)) {
      seendigits = 1;
      while (isdigit(c)) {
	bsStringAppChr(buf,c);
	c = bsGetc(p);
      }
    }
  }

 exponent:
  if (c == 'e' || c == 'E') {
    bsStringAppChr(buf,c);
    isfloat = 1;
    c = bsGetc(p);
    if (c == '+' || c == '-') {
      bsStringAppChr(buf,c);
      c = bsGetc(p);
    }
    if (!isdigit(c))
      /* uh-oh - bad floating-point format*/
      return 1;
    while (isdigit(c)) {
      bsStringAppChr(buf,c);
      c = bsGetc(p);
    }
  }

  if (!seendigits)
    return -1; /* no way */

  bsUngetc(p,c);

 cleanup:
  if (isfloat_r)
    *isfloat_r = isfloat;
  return 0;
}

/* extract next token from parse source */
/* 0 = EOF, -1 = error */
static int getToken(BSInterp *i, BSParseSource *p, BSExprNode **e_r) {
  int c;
  int op = 0; /* not a valid op... */

  bsClearResult(i);
  bsSkipSpace(p);

  *e_r = NULL;
  
  if ((c = bsGetc(p)) == BS_EOF)
    return 0; /* special case - 0 is never a valid token code */
  
  switch (c) {
  case '&':
    if ((c = bsGetc(p)) != '&') {
      bsSetStringResult(i,"missing second '&' in expression",BS_S_STATIC);
      return -1;
    }
    op = BSEXPR_OP_AND; goto finish;
    break;
  case '|':
    if ((c = bsGetc(p)) != '|') {
      bsSetStringResult(i,"missing second '|' in expression",BS_S_STATIC);
      return -1;
    }
    op = BSEXPR_OP_OR; goto finish;
    break;
  case '=':
    if ((c = bsGetc(p)) != '=') {
      bsSetStringResult(i,"missing second '=' in expression",BS_S_STATIC);
      return -1;
    }
    op = BSEXPR_OP_EQ; goto finish;
    break;
  case '!':
    if ((c = bsGetc(p)) == '=') {
      op = BSEXPR_OP_NE; goto finish;
    } else {
      bsUngetc(p,c);
      op = BSEXPR_OP_NOT; goto finish;
    }
    break;
  case '<':
    if ((c = bsGetc(p)) == '=') {
      op = BSEXPR_OP_LE; goto finish;
    } else {
      bsUngetc(p,c);
      op = BSEXPR_OP_LT; goto finish;
    }
    break;
  case '>':
    if ((c = bsGetc(p)) == '=') {
      op = BSEXPR_OP_GE; goto finish;
    } else {
      bsUngetc(p,c);
      op = BSEXPR_OP_GT; goto finish;
    }
    break;
  case '+':  op = BSEXPR_OP_ADD; goto finish;
  case '-':  op = BSEXPR_OP_SUB; goto finish;
  case '*':  op = BSEXPR_OP_MULT; goto finish;
  case '/':  op = BSEXPR_OP_DIV; goto finish;
  case '%':  op = BSEXPR_OP_MOD; goto finish;
  case '(':  op = BSEXPR_L_LPAREN; goto finish;
  case ')':  return BSEXPR_L_RPAREN; /* no node wanted */
  case ',':  return BSEXPR_L_COMMA; /* no node wanted */
  case '"': 
    {
      /* string value */
      BSObject *o;
      bsUngetc(p,c);
      if (!(o = bsParseString(i,p)))
	return -1; /* oops */
      *e_r = mknode(BSEXPR_L_STRING);
      (*e_r)->data.value.type = BS_T_STRING;
      bsStringInit(&((*e_r)->data.value.data.stringValue));
      bsStringCopy(&((*e_r)->data.value.data.stringValue),bsObjGetString(o));
      bsObjDelete(o);
      op = BSEXPR_L_STRING; 
      goto finish;
    }
    break;
  case '$':
    {
      /* variable reference */
      BSObject *o;
      bsUngetc(p,c);
      if (!(o = bsParseVariable(i,p)))
	return -1;
      *e_r = mknode(BSEXPR_L_VAR);
      (*e_r)->data.obj = o;
      op = BSEXPR_L_VAR; 
      goto finish;
    }
    break;
  case '[':
    {
      /* procedure call */
      BSObject *o;
      bsUngetc(p,c);
      if (!(o = bsParseEList(i,p)))
	return -1;
      *e_r = mknode(BSEXPR_L_PROC);
      (*e_r)->data.obj = o;
      op = BSEXPR_L_PROC; 
      goto finish;
    }
    break;

  default:
    /* possibilities: INT, FLOAT, or FUNC */
    {
      if (isdigit(c) || c == '.') {
	/* number? */
	int isfloat = 0, k;
	BSExprResult r;
	r.type = BS_T_STRING;
	bsStringInit(&(r.data.stringValue));
	bsUngetc(p,c);
	if (extractNum(p,&(r.data.stringValue),&isfloat)) {
	  bsStringFreeSpace(&(r.data.stringValue));
	  bsSetStringResult(i,"invalid number literal in expression",
			    BS_S_STATIC);
	  return -1;
	}
	/* try converting it... */
	if (isfloat)
	  k = convertToFloat(i,&r);
	else
	  k = convertToInt(i,&r);
	if (k) {
	  bsStringFreeSpace(&(r.data.stringValue));
	  return -1;
	}
	*e_r = mknode(isfloat ? BSEXPR_L_FLOAT : BSEXPR_L_INT);
	(*e_r)->data.value = r;
	return (*e_r)->type;
      }
      if (isalpha(c) || c == '_') {
	BSExprFunc func;
	char fname[BSEXPR_FUNCSZ];
	int k = 0;
	while (k < (BSEXPR_FUNCSZ-1) &&
	       (isalnum(c) || c == '_')) {
	  fname[k++] = c;
	  c = bsGetc(p);
	}
	fname[k] = '\0';
	if (isalnum(c) || c == '_') {
	  bsSetStringResult(i,"function name too long in expression",
			    BS_S_STATIC);
	  return -1;
	}
	bsUngetc(p,c);
	/* does function actually exist? */
	if (!(func = find_func(fname))) {
	  bsAppendResult(i,"unknown function in expression: ",
			 fname,NULL);
	  return -1;
	}
	*e_r = mknode(BSEXPR_L_FUNC);
	(*e_r)->data.func.func = func;
	/* leave args blank */
	(*e_r)->data.func.nargs = 0;
	return (*e_r)->type;
      }
      bsSetStringResult(i,"unrecognized character in expression",
			BS_S_STATIC);
      return -1;
    }
  }
  BS_FATAL("getToken: unreachable");
  return -1;

 finish:
  return op;
}

void bsExprFreeResult(BSExprResult *v) {
  if (v->type == BS_T_STRING)
    bsStringFreeSpace(&(v->data.stringValue));
}

void bsExprFreeExpr(BSExprNode *n) {
  BSExprNode *next;
  int k;
  while (n) {
    if (n->sub)
      bsExprFreeExpr(n->sub);
    next = n->next;
    switch (n->type) {
    case BSEXPR_L_VAR:
    case BSEXPR_L_PROC:
      bsObjDelete(n->data.obj);
      break;
    case BSEXPR_L_INT:
    case BSEXPR_L_FLOAT:
    case BSEXPR_L_STRING:
      bsExprFreeResult(&(n->data.value));
      break;
    }
    bsFree(n);
    n = next;
  }
}

static void *my_alloc(size_t s) {
  return bsAlloc(s,0);
}

static BSExprNode *parseExpr(BSInterp *i, BSParseSource *p) {
  void *parser;
  int id;
  BSExprNode *n;
  BSExprParserResult result;
  
  memset(&result,0,sizeof(result));
  result.interp = i;
  result.root = NULL;
  result.failed = 0;
  
  bsClearResult(i);
  parser = bsExprParserAlloc(my_alloc);
  while ((id = getToken(i,p,&n)) > 0) {
    bsExprParser(parser,id,n,&result);
    if (result.failed) {
      bsExprParserFree(parser,bsFree);
      bsExprFreeExpr(result.root);
      return NULL;
    }
  }
  bsExprParser(parser,0,NULL,&result);
  bsExprParserFree(parser,bsFree);

  if (result.failed) {
    bsExprFreeExpr(result.root);
    return NULL;
  }

  return result.root;
}

/* given two expr results, make them the same type */
/* if number is not 0, then make an effort to convert to
   a number first */
static void reconcile(BSExprResult *a, int number) {
  if (a[0].type == 0 || a[1].type == 0) {
    /* special case - force both to be strings */
    convertToString(&(a[0]));
    convertToString(&(a[1]));
    return;
  }

  if (a[0].type == a[1].type) {
    if (a[0].type == BS_T_STRING && a[1].type == BS_T_STRING && 
	number) {
      /* try converting to numbers... */
      convertToInt(NULL,&(a[0])) == 0 || convertToFloat(NULL,&(a[0])) == 0;
      convertToInt(NULL,&(a[1])) == 0 || convertToFloat(NULL,&(a[1])) == 0;
    }
    if (a[0].type == a[1].type)
      return; /* nothing to do - already the same */
  }

  if (a[0].type == BS_T_STRING) {
    /* try to string to other */
    switch (a[1].type) {
    case BS_T_FLOAT:
      if (convertToFloat(NULL,&(a[0])) == 0)
	return;
      break;
    case BS_T_INT:
      if (convertToInt(NULL,&(a[0])) == 0)
	return;
      if (convertToFloat(NULL,&(a[0])) == 0) {
	/* convert the int to float too... */
	convertToFloat(NULL,&(a[1]));
	return;
      }
      break;
    }
    /* otherwise, convert other to string */
    convertToString(&(a[1]));
    return;
  }
  /* repeat of previous case with 0/1 inverted */
  if (a[1].type == BS_T_STRING) {
    /* try to string to other */
    switch (a[0].type) {
    case BS_T_FLOAT:
      if (convertToFloat(NULL,&(a[1])) == 0)
	return;
      break;
    case BS_T_INT:
      if (convertToInt(NULL,&(a[1])) == 0)
	return;
      if (convertToFloat(NULL,&(a[1])) == 0) {
	/* convert the int to float too */
	convertToFloat(NULL,&(a[0]));
	return;
      }
      break;
    }
    /* otherwise, convert other to string */
    convertToString(&(a[0]));
    return;
  }
  /* if we get here then neither one is string and
     they are not the same, so both are numbers, and
     one is float and the other is int */
  assert((a[0].type == BS_T_INT && a[1].type == BS_T_FLOAT) ||
	 (a[0].type == BS_T_FLOAT && a[1].type == BS_T_INT));
  /* in this case, promote the int to a float */
  convertToFloat(NULL,(a[0].type == BS_T_INT ?
		       &(a[0]) : &(a[1])));
  return;
}

static int evalExpr(BSInterp *i, BSExprNode *n, BSExprResult *r) {
  BSExprResult a[2];
  a[0].type = 0;
  a[1].type = 0;

  initResult(r);
  if (!n)
    return 0; /* ??? */

  /* process sub-nodes for common cases */
  if (n->type != BSEXPR_OP_AND && n->type != BSEXPR_OP_OR) {
    int k;
    k = opNumArgs(n->type);
    switch (k) {
    case 2:
      if (evalExpr(i,n->sub->next,&(a[1])))
	goto cleanup;
      /* fall-through */
    case 1:
      if (evalExpr(i,n->sub,&(a[0])))
	goto cleanup;
      break;
    }
  }


  switch (n->type) {
  case BSEXPR_OP_AND:
    if (evalExpr(i,n->sub,&(a[0])))
      goto cleanup;
    if (!getBoolean(&(a[0]))) {
      setInt(r,0);
      return 0;
    }
    if (evalExpr(i,n->sub->next,&(a[1])))
      goto cleanup;
    setInt(r,getBoolean(&(a[1])));
    return 0;

  case BSEXPR_OP_OR:
    if (evalExpr(i,n->sub,&(a[0])))
      goto cleanup;
    if (getBoolean(&(a[0]))) {
      setInt(r,1);
      return 0;
    }
    if (evalExpr(i,n->sub->next,&(a[1])))
      goto cleanup;
    setInt(r,getBoolean(&(a[1])));
    return 0;

  case BSEXPR_OP_EQ:
    reconcile(a,0);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      setInt(r,streq(bsStringPtr(&(a[0].data.stringValue)),
		     bsStringPtr(&(a[1].data.stringValue))));
      break;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue == a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setInt(r,(a[0].data.floatValue == a[1].data.floatValue));
      break;
    default:
      BS_FATAL("OP_EQ: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_NE:
    reconcile(a,0);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      setInt(r,!streq(bsStringPtr(&(a[0].data.stringValue)),
		     bsStringPtr(&(a[1].data.stringValue))));
      break;
    case BS_T_INT:
      setInt(r,!(a[0].data.intValue == a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setInt(r,!(a[0].data.floatValue == a[1].data.floatValue));
      break;
    default:
      BS_FATAL("OP_NE: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_LT:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      setInt(r,(strcmp(bsStringPtr(&(a[0].data.stringValue)),
		       bsStringPtr(&(a[1].data.stringValue))) < 0));
      break;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue < a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setInt(r,(a[0].data.floatValue < a[1].data.floatValue));
      break;
    default:
      BS_FATAL("OP_LT: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_LE:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      setInt(r,(strcmp(bsStringPtr(&(a[0].data.stringValue)),
		       bsStringPtr(&(a[1].data.stringValue))) <= 0));
      break;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue <= a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setInt(r,(a[0].data.floatValue <= a[1].data.floatValue));
      break;
    default:
      BS_FATAL("OP_LE: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_GT:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      setInt(r,(strcmp(bsStringPtr(&(a[0].data.stringValue)),
		       bsStringPtr(&(a[1].data.stringValue))) > 0));
      break;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue > a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setInt(r,(a[0].data.floatValue > a[1].data.floatValue));
      break;
    default:
      BS_FATAL("OP_GT: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_GE:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      setInt(r,(strcmp(bsStringPtr(&(a[0].data.stringValue)),
		       bsStringPtr(&(a[1].data.stringValue))) >= 0));
      break;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue >= a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setInt(r,(a[0].data.floatValue >= a[1].data.floatValue));
      break;
    default:
      BS_FATAL("OP_GE: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_ADD:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      bsSetStringResult(i,"cannot add strings",BS_S_STATIC);
      goto cleanup;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue + a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setFloat(r,a[0].data.floatValue + a[1].data.floatValue);
      break;
    default:
      BS_FATAL("OP_ADD: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_SUB:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      bsSetStringResult(i,"cannot subtract strings",BS_S_STATIC);
      goto cleanup;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue - a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setFloat(r,a[0].data.floatValue - a[1].data.floatValue);
      break;
    default:
      BS_FATAL("OP_SUB: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_MULT:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      bsSetStringResult(i,"cannot multiply strings",BS_S_STATIC);
      goto cleanup;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue * a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setFloat(r,a[0].data.floatValue * a[1].data.floatValue);
      break;
    default:
      BS_FATAL("OP_MULT: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_DIV:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      bsSetStringResult(i,"cannot divide strings",BS_S_STATIC);
      goto cleanup;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue / a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      setFloat(r,a[0].data.floatValue / a[1].data.floatValue);
      break;
    default:
      BS_FATAL("OP_DIV: invalid exprresult type");
    }
    break;
    
  case BSEXPR_OP_MOD:
    reconcile(a,1);
    assert(a[0].type == a[1].type);
    switch (a[0].type) {
    case BS_T_STRING:
      bsSetStringResult(i,"cannot determine modulus of strings",BS_S_STATIC);
      goto cleanup;
    case BS_T_INT:
      setInt(r,(a[0].data.intValue % a[1].data.intValue));
      break;
    case BS_T_FLOAT:
      bsSetStringResult(i,"cannot determine modulus of floats",BS_S_STATIC);
      break;
    default:
      BS_FATAL("OP_MOD: invalid exprresult type");
    }
    break;

  case BSEXPR_OP_NOT:
    setInt(r,!getBoolean(&(a[0])));
    break;

  case BSEXPR_OP_NEG:
    /* attempt a string to number conversion first */
    if (a[0].type == BS_T_STRING)
      convertToInt(i,a) == 0 || convertToFloat(i,a) == 0;
    switch (a[0].type) {
    case BS_T_STRING:
      bsSetStringResult(i,"cannot negate string",BS_S_STATIC);
      goto cleanup;
    case BS_T_INT:
      setInt(r,-a[0].data.intValue);
      break;
    case BS_T_FLOAT:
      setFloat(r,-a[0].data.floatValue);
      break;
    default:
      BS_FATAL("OP_NEG: invalid exprresult type");
    }
    break;

  case BSEXPR_L_LPAREN:
    return evalExpr(i,n->sub,r);

  case BSEXPR_L_STRING:
  case BSEXPR_L_FLOAT:
  case BSEXPR_L_INT:
    copyResult(r,&(n->data.value));
    break;

  case BSEXPR_L_PROC:
    /* make sub-call */
    {
      BSList *scall;
      int scode;
      if (!(scall = bsObjGetList(i,n->data.obj)))
	goto cleanup;
      bsClearResult(i);
      if ((scode = bsProcessLine(i,scall)) != BS_OK) {
	if (scode != BS_ERROR)
	  bsAppendResult(i,"invalid code from sub-call: ",
			 bsCodeToString(scode),NULL);
	goto cleanup;
      }
      setString(r,bsObjGetStringPtr(i->result),-1);
      bsClearResult(i);
    }
    break;

  case BSEXPR_L_VAR:
    /* lookup variable - member value is it "obj" or "var"? */
    {
      BSObject *o;
      if (!(o = bsGetObj(i,NULL,n->data.obj,0)))
	goto cleanup;
      /* treat as string for now... */
      setString(r,bsObjGetStringPtr(o),-1);
    }
    break;

  case BSEXPR_L_FUNC:
    {
      BSExprNode *a;
      int k;
      /* ... calculate subvalues and stow in slots ... */
      for (k = 0, a = n->sub; 
	   k < n->data.func.nargs && a; 
	   k++, a = a->next) {
	if (evalExpr(i,a,&(n->data.func.args[k])))
	  goto cleanup;
      }
      /* both walks should end at the same time... */
      assert(k == n->data.func.nargs && a == NULL);
      return n->data.func.func(i,n->data.func.nargs,
			       n->data.func.args,r);
    }
    break;

  default:
    BS_FATAL("evalExpr: unexpected node type");
  }
  bsExprFreeResult(&(a[0]));
  bsExprFreeResult(&(a[1]));
  return 0;

 cleanup:
  bsExprFreeResult(&(a[0]));
  bsExprFreeResult(&(a[1]));
  return -1;
}

void bsExprFree(BSParsedExpr *e) {
  BSExprNode *n = (BSExprNode *)e;
  bsExprFreeExpr(n);
}

BSParsedExpr *bsExprParse(BSInterp *i, BSString *s) {
  BSParseSource sp;
  BSExprNode *n;
  bsStringSource(&sp,bsStringPtr(s));
  bsClearResult(i);
  if ((n = parseExpr(i,&sp))) {
    if (!bsIsResultClear(i)) {
      bsExprFreeExpr(n);
      return NULL;
    }
  }
  return (BSParsedExpr *)n;
}

int bsExprEval(BSInterp *i, BSParsedExpr *e, BSExprResult *r) {
  BSExprNode *n;
  int k;
  n = (BSExprNode *)e;
  bsClearResult(i);
  k = evalExpr(i,n,r);
  return k;
}

static void expr_freeproc(void *priv, void *cdata) {
  BSExprNode *n = (BSExprNode *)priv;
  if (n)
    bsExprFreeExpr(n);
}

static BSExprNode *copyNode(BSExprNode *n) {
  BSExprNode *n2, *tail;

  if (!n)
    return NULL;

  n2 = bsAllocObj(BSExprNode);
  n2->type = n->type;
  /* copy necessary value pieces */
  switch (n2->type) {
  case BSEXPR_L_VAR:
  case BSEXPR_L_PROC:
    n2->data.obj = bsObjCopy(n->data.obj);
    break;
  case BSEXPR_L_STRING:
  case BSEXPR_L_FLOAT:
  case BSEXPR_L_INT:
    copyResult(&(n2->data.value),&(n->data.value));
    break;
  case BSEXPR_L_FUNC:
    n2->data.func.func = n->data.func.func;
    n2->data.func.nargs = n->data.func.nargs;
    if (n2->data.func.nargs > 0) {
      n2->data.func.args = bsAlloc(n2->data.func.nargs*
				   sizeof(BSExprResult),1);
    } else {
      n2->data.func.args = NULL;
    }
    break;
  }

  n2->sub = copyNode(n->next);
  n2->next = copyNode(n->next);
  return n2;
}

static void *expr_copyproc(void *priv, void *cdata) {
  return (void *)copyNode((BSExprNode *)priv);
}

static BSCacheDriver expr_driver = {
  "expr",               /* name */
  0,                    /* fill in later */
  expr_freeproc,
  expr_copyproc,
  NULL                  /* cdata */
};

BSCacheDriver *bsGetExprDriver(void) {
  if (expr_driver.id == 0) {
    expr_driver.id = bsUniqueId();
  }
  return &expr_driver;
}

int bsExprEvalObj(BSInterp *i, BSObject *o, BSExprResult *r) {
  BSCacheDriver *d;
  BSExprNode *n;
  int k, free_expr = 0;
  
  d = bsGetExprDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  
  if (bsObjGetCache(o,d->id,(void **)&n)) {
    if (!(n = (BSExprNode *)bsExprParse(i,bsObjGetString(o)))
	&& !bsIsResultClear(i))
      return -1;
    if (i->opt & BS_OPT_MEMORY)
      free_expr = 1;
    else
      bsObjAddCache(o,d,(void *)n);
  }
  
  k = bsExprEval(i,(BSParsedExpr *)n,r);
  
  if (free_expr)
    bsExprFreeExpr(n);

  return k;
}

int bsExprBoolean(BSExprResult *r) {
  int k;
  if (!r)
    return 0;
  return getBoolean(r);
}

char *bsExprString(BSExprResult *r) {
  if (!r)
    return "";
  convertToString(r);
  return bsStringPtr(&(r->data.stringValue));
}

int bsExprFloat(BSInterp *i, BSExprResult *r, BSFloat *f) {
  bsClearResult(i);
  if (!r) {
    bsSetStringResult(i,"cannot convert null object to float",
		      BS_S_STATIC);
    return -1;
  }
  if (convertToFloat(i,r))
    return -1;
  if (f)
    *f = r->data.floatValue;
  return 0;
}

int bsExprInt(BSInterp *i, BSExprResult *r, BSInt *x) {
  bsClearResult(i);
  if (!r) {
    bsSetStringResult(i,"cannot convert null object to int",
		      BS_S_STATIC);
    return -1;
  }
  if (convertToInt(i,r))
    return -1;
  if (x)
    *x = r->data.intValue;
  return 0;
}
