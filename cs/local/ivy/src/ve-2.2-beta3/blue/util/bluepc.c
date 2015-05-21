/* BlueScript parser compiler */
/* Provides a simple way to write a C-based parser of  BlueScript file
   without too much "drudgery" */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <bluescript.h>

typedef struct bpc_arg {
  char *name;
  char *defvalue;
  int catchall;
} bpc_arg_t;

typedef struct bpc_native {
  char *filename;  /* original filename (if any) */
  int linenum;     /* original line number */
  char *block;     /* actual block */
} bpc_native_t;

typedef struct bpc_elem {
  struct bpc_elem *next;
  char *name;
  bpc_arg_t *arglist;
  int nargs;
  bpc_native_t *n_action;   /* action when elem is discovered */
  int id;
} bpc_elem_t;

typedef struct bpc_block {
  struct bpc_block *next;
  char *name;
  bpc_arg_t *arglist; /* doesn't include body */
  int nargs;
  bpc_native_t *n_enter;
  bpc_native_t *n_error;
  bpc_native_t *n_exit;
  bpc_elem_t *elems;
  struct bpc_block *blocks;
  int id;
} bpc_block_t;

#define STACK_DEPTH 64

typedef struct bpc_parser {
  char *name;
  bpc_native_t *n_context;  /* native context */
  bpc_native_t *n_returntype;   /* return type */
  bpc_native_t *n_enter;    /* enter action */
  bpc_native_t *n_error;    /* error action */
  bpc_native_t *n_exit;     /* exit action */
  bpc_elem_t *elems;
  bpc_block_t *blocks;
  bpc_arg_t *arglist;       /* kept as a place-holder for now */
  int nargs;
  /* following is used while parsing */
  bpc_block_t *block_stack[STACK_DEPTH];
  int top; /* top of stack (+1) */
  int counter; /* unique id counter */
} bpc_parser_t;

FILE *out;

BSInterp *mknative_interp(bpc_arg_t *arglist, int nargs) {
  BSInterp *i;
  int k;

  i = bsInterpCreate();

  /* create substitution variables */
  for (k = 0; k < nargs; k++) {
    char str[80];
    BSObject *o;
    snprintf(str,80,"(bsObjGetStringPtr(objv[%d]))",k+1);
    bsSet(i,NULL,arglist[k].name,(o = bsObjString(str,-1,BS_S_VOLATILE)));
    bsObjDelete(o);
  }

  return i;
}

bpc_native_t *mknative(BSObject *o,
		       bpc_arg_t *arglist, int nargs) {
  /* use temporary interpreter! */
  bpc_native_t *nt;
  BSString str;

  bsStringInit(&str);
  BSInterp *i = mknative_interp(arglist,nargs);
  /* Hopefully our substitution engine won't mangle C too
     badly - otherwise we'll have to do a custom engine. */
  if (bsSubsObj(i,&str,o,BS_SUBS_NOESCAPES|BS_SUBS_NOPROCS) != BS_OK) {
    fprintf(stderr,"error: failed to parse native block: %s\n",
	    bsObjGetStringPtr(bsGetResult(i)));
    bsInterpDestroy(i);
    return NULL;
  }
  nt = bsAllocObj(bpc_native_t);
  nt->block = bsStrdup(bsStringPtr(&str));
  bsStringFreeSpace(&str);
  return nt;
}

/* mkarglist() has known memory leaks on error - since it is not
   meant to be an ongoing process, I have decided against going through
   the effort to get rid of them */
int mkarglist(BSInterp *i, BSObject *o_argl, int *arglistsz_r, 
		     bpc_arg_t **list_r) {
  int nargs;
  BSList *argl;
  int catchall = 0;
  bpc_arg_t *args = NULL;

    /* build arg list */
  if (!(argl = bsObjGetList(i,o_argl)))
    return BS_ERROR;

  nargs = bsListSize(argl);
  if (nargs > 0) {
    /* check for catch-all case */
    BSObject *o;
    o = bsListIndex(argl,nargs-1);
    if (strcmp(bsObjGetStringPtr(o),BS_PROC_CATCHALL_NAME) == 0) {
      catchall = 1;
      nargs--;
    }
  }

  if ((nargs+catchall) > 0) {
    int k;
    args = bsAlloc((nargs+catchall)*sizeof(bpc_arg_t),0);
    for (k = 0; k < nargs; k++) {
      BSObject *ao = NULL, *an = NULL, *ad = NULL;
      BSList *l = NULL;
      ao = bsListIndex(argl,k);
      if (!(l = bsObjGetList(i,ao)) || bsListSize(l) <= 1) {
	an = ao; /* simple case */
	ad = NULL;
      } else if (bsListSize(l) > 2) {
	bsSetStringResult(i,"argument element is too long",BS_S_STATIC);
	return BS_ERROR;
      } else {
	/* argument with default */
	an = bsListIndex(l,0);
	ad = bsListIndex(l,1);
      }
      args[k].name = bsStrdup(bsObjGetStringPtr(an));
      args[k].defvalue = (ad ? bsStrdup(bsObjGetStringPtr(ad)) : NULL);
      args[k].catchall = 0;
    }
    if (catchall) {
      args[nargs].name = bsStrdup(BS_PROC_CATCHALL_NAME);
      args[nargs].defvalue = NULL;
      args[nargs].catchall = 1;
      nargs++;
    }
  }

  if (arglistsz_r)
    *arglistsz_r = nargs;
  if (list_r)
    *list_r = args;

  return BS_OK;
}

void freearglist(bpc_arg_t *al, int n) {
  while (n > 0) {
    bsFree(al[n].name);
    bsFree(al[n].defvalue);
    n--;
  }
}

int proc_elem(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "elem <name> <arglist> { <body> }";
  bpc_elem_t *e;
  bpc_native_t *nt;
  bpc_arg_t *arglist;
  int nargs;
  bpc_parser_t *p = (bpc_parser_t *)cdata;

  bsClearResult(i);
  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (mkarglist(i,objv[2],&nargs,&arglist) != BS_OK)
    return BS_ERROR;
  if (!(nt = mknative(objv[3],arglist,nargs))) {
    bsSetStringResult(i,"failed to create native block",BS_S_STATIC);
    freearglist(arglist,nargs);
    return BS_ERROR;
  }
  
  e = bsAllocObj(bpc_elem_t);
  e->name = bsStrdup(bsObjGetStringPtr(objv[1]));
  e->arglist = arglist;
  e->nargs = nargs;
  e->n_action = nt;
  e->id = p->counter++;

  if (p->top > 0) {
    e->next = p->block_stack[p->top-1]->elems;
    p->block_stack[p->top-1]->elems = e;
  } else {
    e->next = p->elems;
    p->elems = e;
  }

  return BS_OK;
}

#define N_ENTER 0
#define N_EXIT  0
#define N_ERROR 0

int proc_native(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  int k;
  for (k = 1; k < objc; k++) {
    fputs(bsObjGetStringPtr(objv[k]),out);
    putc('\n',out);
  }
  return BS_OK;
}

int proc_mnative(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  bpc_native_t *nt;
  bpc_parser_t *p = (bpc_parser_t *)cdata;
  char *s;

  if (objc != 2) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",bsObjGetStringPtr(objv[0])," { <body> }",NULL);
    return BS_ERROR;
  }
  if (p->top > 0) {
    nt = mknative(objv[1],p->block_stack[p->top-1]->arglist,
		  p->block_stack[p->top-1]->nargs);
  } else {
    nt = mknative(objv[1],p->arglist,p->nargs);
  }
  if (!nt) {
    bsSetStringResult(i,"failed to create native block",BS_S_STATIC);
    return BS_ERROR;
  }
  s = bsObjGetStringPtr(objv[0]);
  if (strcmp(s,"_enter") == 0) {
    if (p->top > 0)
      p->block_stack[p->top-1]->n_enter = nt;
    else
      p->n_enter = nt;
  } else if (strcmp(s,"_exit") == 0) {
    if (p->top > 0)
      p->block_stack[p->top-1]->n_exit = nt;
    else
      p->n_exit = nt;
  } else if (strcmp(s,"_error") == 0) {
    if (p->top > 0)
      p->block_stack[p->top-1]->n_error = nt;
    else
      p->n_error = nt;
  } else {
    abort(); /* should never happen */
  }
  return BS_OK;
}

int proc_block(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "block <name> <arglist> { <body> }";
  bpc_block_t *b;
  bpc_native_t *nt;
  bpc_arg_t *arglist;
  int nargs;
  int code;
  bpc_parser_t *p = (bpc_parser_t *)cdata;

  bsClearResult(i);
  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (mkarglist(i,objv[2],&nargs,&arglist) != BS_OK)
    return BS_ERROR;

  b = bsAllocObj(bpc_block_t);
  b->name = bsStrdup(bsObjGetStringPtr(objv[1]));
  b->arglist = arglist;
  b->nargs = nargs;
  b->id = p->counter++;

  if (p->top >= STACK_DEPTH) {
    fprintf(stderr,"block stack depth exceeded (STACK_DEPTH = %d)\n",STACK_DEPTH);
    abort();
  }
  p->block_stack[p->top] = b;
  p->top++;

  bsInterpPush(i);
  code = bsEval(i,objv[3]);
  bsInterpPop(i);

  p->top--;
  b->next = p->blocks;
  p->blocks = b;

  return code;
}

int proc_context(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "context { <body> }";
  bpc_parser_t *p = (bpc_parser_t *)cdata;
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  p->n_context = bsAllocObj(bpc_native_t);
  p->n_context->block = bsStrdup(bsObjGetStringPtr(objv[1]));
  return BS_OK;
}

int proc_returntype(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "returntype { <body> }";
  bpc_parser_t *p = (bpc_parser_t *)cdata;
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  p->n_returntype = bsAllocObj(bpc_native_t);
  p->n_returntype->block = bsStrdup(bsObjGetStringPtr(objv[1]));
  return BS_OK;
}

int write_preamble(FILE *);
int write_parser(bpc_parser_t *p, FILE *);

int proc_parser(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "parser <name> { <body> }";
  bpc_parser_t *p;
  int code;
  
  bsClearResult(i);
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  
  p = bsAllocObj(bpc_parser_t);
  p->name = bsStrdup(bsObjGetStringPtr(objv[1]));
  
  bsInterpPush(i);

  /* add functions */
  bsSetExtProc(i,"block",proc_block,(void *)p);
  bsSetExtProc(i,"elem",proc_elem,(void *)p);
  bsSetExtProc(i,"_enter",proc_mnative,(void *)p);
  bsSetExtProc(i,"_exit",proc_mnative,(void *)p);
  bsSetExtProc(i,"_error",proc_mnative,(void *)p);
  /* other functions are context procs so they are only visible
     at this level */
  bsSetExtCProc(i,NULL,"context",proc_context,(void *)p);
  bsSetExtCProc(i,NULL,"returntype",proc_returntype,(void *)p);

  code = bsEval(i,objv[2]);

  bsInterpPop(i);

  if (code != BS_OK) {
    bsAppendResult(i,"\nin parser body",NULL);
    return BS_ERROR;
  }

  if (write_parser(p,out)) {
    bsClearResult(i);
    bsAppendResult(i,"failed to write parser: ",strerror(errno),NULL);
    return BS_ERROR;
  }

  return BS_OK;
}

#define PLEN 1024	

int main(int argc, char **argv) {
  char outfile[PLEN] = "";
  BSInterp *i;
  int c;
  extern int optind;
  extern char *optarg;
  
  while ((c = getopt(argc,argv,"o:h")) != -1) {
    switch (c) {
    case 'o':
      snprintf(outfile,PLEN,"%s",optarg);
      break;
    case 'h':
    default:
      fprintf(stderr,"usage later - sorry\n");
      exit(1);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 1) {
    fprintf(stderr,"must specify input file on command-line\n");
    exit(1);
  }

  if (outfile[0] == '\0') {
    /* automatically determine name */
    if (strrchr(argv[1],'.')) {
      snprintf(outfile,PLEN-20,"%s",argv[1]);
      *strrchr(outfile,'.') = '\0';
      strcat(outfile,".c");
      if (strcmp(outfile,argv[1]) == 0)
	strcat(outfile,".c");
    } else {
      snprintf(outfile,PLEN,"%s.c",argv[1]);
    }
  }
  
  if (!(out = fopen(outfile,"w"))) {
    fprintf(stderr,"could not open %s for writing: %s\n",
	    outfile, strerror(errno));
    exit(1);
  }

  

  i = bsInterpCreateStd();
  bsSetExtCProc(i,NULL,"parser",proc_parser,NULL);
  bsSetExtCProc(i,NULL,"native",proc_native,NULL);

  if (bsEvalFile(i,argv[1])) {
    fprintf(stderr,"failed to evaluate %s: %s",
	    argv[1], bsObjGetStringPtr(bsGetResult(i)));
    exit(1);
  }
  fclose(out);
  exit(0);
}


/** actual work of writing out a parser **/
static int header_written = 0;

static char *preamble = "\n\
#include <stdlib.h>\n\
#include <bluescript.h>\n\
\n\
struct _bpc_arg {\n\
	char *name;\n\
	char *defvalue;\n\
	int catchall;\n\
};\n\
\n\
static int _bpc_load_args(BSInterp *i, \n\
			  int objc, BSObject *objv[],\n\
			  int na, struct _bpc_arg *a) {\n\
	int k;\n\
	BSList *l;\n\
	for (k = 0; k < na; k++) {\n\
		if (a[k].catchall) {\n\
			BSObject *o;\n\
			o = bsObjList(objc-k,objv+k);\n\
			bsSet(i,NULL,a[k].namep,o);\n\
			bsObjDelete(o);\n\
			return BS_OK;\n\
		} else if (k < objc) {\n\
			bsSet(i,NULL,a[k].name,objv[k]);\n\
		} else if (a[k].defvalue) {\n\
			o = bsObjString(a[k].defvalue);\n\
			bsSet(i,NULL,a[k].name,o);\n\
			bsObjDelete(o);\n\
		} else {\n\
			bsSetStringResult(i,\"too few arguments\");\n\
			return BS_ERROR;\n\
		}\n\
	}	\n\
	if (k < objc) {\n\
		bsSetStringResult(i,\"too many arguments\");\n\
		return BS_ERROR;\n\
	}	\n\
	return BS_OK;\n\
}\n\
\n\
#define FAIL(x) { bsSetStringResult(i,(x),BS_S_VOLATILE); goto _fail; }\n\
";

int write_preamble(FILE *f) {
  fputs(preamble,f);
  return 0;
}

char *context_model = "struct _bpc_%s_context {\n\
    int _bpc_dummy;  /* place-holder */\n\
    %s\n\
};\n\
";

char *elem_proto = "static int _bpc_%s_e%d_%s(BSInterp *, int, BSObject *, void *);\n";


char *elem_model1 = "static int _bpc_%s_e%d_%s(\n\
BSInterp *_bpc_i,\n\
int _bpc_objc,\n\
BSObject *_bpc_objv[],\n\
void *_bpc_cdata) {\n\
    static _bpc_arg _bpc_arglist[] = {\n\
";

char *arglist_model = "    { \"%s\", %s%s%s, %d },\n";

char *elem_model2 = "};\n\
static int _bpc_arglistsz = %d;\n\
struct _bpc_%s_context *context = (struct _bpc_%s_context *)_bpc_cdata;\n\
\n\
bsInterpPush(_bpc_i);\n\
if (_bpc_load_args(_bpc_i,_bpc_objc-1,_bpc_objv+1,\n\
    _bpc_arglist,_bpc_arglistsz) != BS_OK) {\n\
    bsAppendResult(_bpc_i,\"\nin elem %s\",NULL);\n\
    return BS_ERROR;\n\
}\n{\n";

char *elem_model3 = "}\n\
bsInterpPop(_bpc_i);\n\
return BS_OK;\n\
\n\
_fail:\n\
bsInterpPop(_bpc_i);\n\
return BS_ERROR;\n\
}\n\n";    

char *block_proto = "static int _bpc_%s_b%d_%s(BSInterp *, int, BSObject *, void *);\n";

char *block_model1 = "static int _bpc_%s_b%d_%s(\n\
BSInterp *_bpc_i,\n\
int _bpc_objc,\n\
BSObject *_bpc_objv[],\n\
void *_bpc_cdata) {\n\
    static _bpc_arg _bpc_arglist[] = {\n\
";

char *block_model2 = "};\n\
static int _bpc_arglistsz = %d;\n\
struct _bpc_%s_context *context = (struct _bpc_%s_context *)_bpc_cdata;\n\
BSObject *_bpc_body = NULL;\n\
\n\
if (_bpc_objc > 1) {\n\
    _bpc_objc--;\n\
    _bpc_body = _bpc_objv[_bpc_objc];\n\
} else {\n\
    _bpc_body = NULL;\n\
}\n\
bsInterpPush(_bpc_i);\n\
if (_bpc_load_args(_bpc_i,_bpc_objc-1,_bpc_objv+1,\n\
    _bpc_arglist,_bpc_arglistsz) != BS_OK) {\n\
    bsAppendResult(_bpc_i,\"\nin block %s\",NULL);\n\
    return BS_ERROR;\n\
}\n{\n";

char *block_elem_cproc = "bsSetExtCProc(_bpc_i,NULL,\"%s\",_bpc_%s_e%d_%s,(void *)context);\n";
char *block_block_cproc = "bsSetExtCProc(_bpc_i,NULL,\"%s\",_bpc_%s_b%d_%s,(void *)context);\n";

char *block_model3 = "}\n\
if (_bpc_body && (bsEval(_bpc_i,_bpc_body) != BS_OK)) goto fail;\n\{\n";

char *block_model4 = "{\n\
bsInterpPop(_bpc_i);\n\
return BS_OK;\n\
\n\
_fail:\n\{\n";

char *block_model5 = "}\n\
bsInterpPop(_bpc_i);\n\
return BS_ERROR;\n\
}\n\n";    


static BSObject *mkargobj(bpc_arg_t *a) {
  BSObject *o;
  BSList *l;
  o = bsObjList(0,NULL);
  l = bsObjGetList(o);
  bsListPush(l,bsObjString(a->name),BS_TAIL);
  bsListPush(l,bsObjInt(a->catchall),BS_TAIL);
  if (a->defvalue)
    bsListPush(l,bsObjString(a->defvalue),BS_TAIL);
  return o;
}

stati BSObject *mkarglistobj(bpc_arg_t *a, int nargs) {
  BSObject *o;
  BSList *l;
  o = bsObjList(0,NULL);
  l = bsObjGetList(o);
  while (nargs > 0) {
    bsListPush(l,mkargobj(a),BS_TAIL);
    a++;
    nargs--;
  }
  return o;
}

static int elem_proc(BSObject *, BSInterp *i, int objc, BSObject **);

static BSOpaqueDriver elem_driver = {
  "elem",
  0,
  NULL,
  NULL,
  elem_proc
};

static BSObject *mkelemobj(bpc_elem_t *e) {
  BSOpaque *p;
  p = bsAllocObj(BSOpaque);
  if (event_driver.id == 0) {
    
  }
}

static int elem_proc(BSObject *o, BSInterp *i, int objc, BSObject **objv) {
  BSOpaque *p;
  bpc_elem_t *e;
  char *s;

  assert(o != NULL);
  assert(o->repValid & BS_T_OPAQUE);
  p = o->opaqueRep;
  assert(p != NULL);
  assert(p->driver == &elem_driver);
  e = (bpc_elem_t *)(p->data);
  assert(e != NULL);

  assert(objc > 0);
  s = bsObjGetStringPtr(objv[0]);
  
  if (strcmp(s,"name") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <elem> name",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetStringResult(i,e->name,BS_S_VOLATILE);
    return BS_OK;
  } else if (strcmp(s,"nargs") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <elem> nargs",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetIntResult(i,e->nargs);
    return BS_OK;
  } else if (strcmp(s,"arglist") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <elem> arglist",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetResult(i,mkarglistobj(e->arglist,e->nargs));
    return BS_OK;
  } else if (strcmp(s,"id") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <elem> id",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetIntResult(i,e->id);
    return BS_OK;
  } else if (strcmp(s,"action") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <elem> action",BS_S_STATIC);
      return BS_ERROR;
    }
    bsClearResult(i);
    if (e->n_action && e->n_action->block) {
      bsSetStringResult(i,e->n_action->block,BS_S_VOLATILE);
    }
    return BS_OK;
  } else {
    bsSetStringResult(i,"invalid elem method: %s",s);
    return BS_ERROR;
  }
}

static int block_proc(BSObject *, BSInterp *i, int objc, BSObject **);

static BSOpaqueDriver block_driver = {
  "block",
  0,
  NULL,
  NULL,
  block_proc
};

static int block_proc(BSObject *o, BSInterp *i, int objc, BSObject **objv) {
  BSOpaque *p;
  bpc_block_t *e;
  char *s;

  assert(o != NULL);
  assert(o->repValid & BS_T_OPAQUE);
  p = o->opaqueRep;
  assert(p != NULL);
  assert(p->driver == &block_driver);
  e = (bpc_block_t *)(p->data);
  assert(e != NULL);

  assert(objc > 0);
  s = bsObjGetStringPtr(objv[0]);
  
  if (strcmp(s,"name") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> name",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetStringResult(i,e->name,BS_S_VOLATILE);
    return BS_OK;
  } else if (strcmp(s,"nargs") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> nargs",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetIntResult(i,e->nargs);
    return BS_OK;
  } else if (strcmp(s,"arglist") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> arglist",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetResult(i,mkarglistobj(e->arglist,e->nargs));
    return BS_OK;
  } else if (strcmp(s,"id") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> id",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetIntResult(i,e->id);
    return BS_OK;
  } else if (strcmp(s,"enter") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> enter",BS_S_STATIC);
      return BS_ERROR;
    }
    bsClearResult(i);
    if (e->n_enter && e->n_enter->block) {
      bsSetStringResult(i,e->n_enter->block,BS_S_VOLATILE);
    }
    return BS_OK;
  } else if (strcmp(s,"exit") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> exit",BS_S_STATIC);
      return BS_ERROR;
    }
    bsClearResult(i);
    if (e->n_exit && e->n_exit->block) {
      bsSetStringResult(i,e->n_exit->block,BS_S_VOLATILE);
    }
    return BS_OK;
  } else if (strcmp(s,"error") == 0) {
    if (objc > 1) {
      bsSetStringResult(i,"usage: <block> error",BS_S_STATIC);
      return BS_ERROR;
    }
    bsClearResult(i);
    if (e->n_error && e->n_error->block) {
      bsSetStringResult(i,e->n_error->block,BS_S_VOLATILE);
    }
    return BS_OK;
  } else if (strcmp(s,"elems") == 0) {
  } else if (strcmp(s,"blocks") == 0) {
  } else {
    bsSetStringResult(i,"invalid block method: %s",s);
    return BS_ERROR;
  }
}


int write_parser(bpc_parser_t *p, FILE *f) {
  bpc_elem_t *e;
  bpc_block_t *b;
  int k;

  fprintf(f,"\n/* *** parser: %s *** */\n\n",p->name);

  /* context type */
  fprintf(f,context_model,p->name,
	  (p->n_context && p->n_context->block) ? 
	  p->n_context->block : "");
  
  fprintf(f,"\n/* *** prototypes *** */\n\n");
  for (e = p->elems; e; e = e->next) {
    fprintf(f,elem_proto,p->name,e->id,e->name);
  }
  for (b = p->blocks; b; b = b->next) {
    fprintf(f,block_proto,p->name,b->id,b->name);
  }

  fprintf(f,"\n/* *** element functions *** */\n\n");

  /* element functions */
  for (e = p->elems; e; e = e->next) {
    fprintf(f,elem_model1,p->name,e->id,e->name);
    for (k = 0; k < e->nargs; k++) {
      fprintf(f,arglist_model,e->arglist[k].name,
	      e->arglist[k].defvalue ? "\"" : "",
	      /* FIXME:  escape quotes */
	      e->arglist[k].defvalue ? e->arglist[k].defvalue : "NULL",
	      e->arglist[k].defvalue ? "\"" : "",
	      e->arglist[k].catchall);
    }
    fprintf(f,elem_model2,e->nargs,p->name,p->name,e->name);
    if (e->n_action && e->n_action->block)
      fputs(e->n_action->block,f);
    fprintf(f,elem_model3);
  }

  fprintf(f,"\n/* *** block functions *** */\n\n");
  
  for (b = p->blocks; b; b = b->next) {
    fprintf(f,block_model1,p->name,b->id,b->name);
    for (k = 0; k < b->nargs; k++) {
      fprintf(f,arglist_model,b->arglist[k].name,
	      b->arglist[k].defvalue ? "\"" : "",
	      /* FIXME:  escape quotes */
	      b->arglist[k].defvalue ? b->arglist[k].defvalue : "NULL",
	      b->arglist[k].defvalue ? "\"" : "",
	      b->arglist[k].catchall);
    }
    fprintf(f,block_model2,b->nargs,p->name,p->name,b->name);

    {
      bpc_elem_t *be;
      bpc_block_t *bb;
      fprintf(f,"{\n/* *** procedures *** */\n\n");
      for (be = b->elems; be; be = be->next)
	fprintf(f,block_elem_cproc,be->name,p->name,be->id,be->name);
      for (bb = b->blocks; bb; bb = bb->next)
	fprintf(f,block_block_cproc,bb->name,p->name,bb->id,bb->name);
      fprintf(f,"}\n");
    }

    if (b->n_enter && b->n_enter->block) {
      fprintf(f,"\n/* *** enter native start *** */\n\n");
      fputs(b->n_enter->block,f);
      fprintf(f,"\n/* *** enter native end *** */\n\n");
    }
    fprintf(f,block_model3);
    if (b->n_exit && b->n_exit->block) {
      fprintf(f,"\n/* *** exit native start *** */\n\n");
      fputs(b->n_exit->block,f);
      fprintf(f,"\n/* *** exit native end *** */\n\n");
    }
    fprintf(f,block_model4);
    if (b->n_error && b->n_error->block) {
      fprintf(f,"\n/* *** error native start *** */\n\n");
      fputs(b->n_error->block,f);
      fprintf(f,"\n/* *** error native end *** */\n\n");
    }
    fputs(block_model5,f);
  }
}
