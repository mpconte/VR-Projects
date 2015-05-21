#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include <bluescript.h>

BSId bsUniqueId(void) {
  static BSId id = 0;
  id++;
  return id;
}

/* share a stack */
BSContext *bsContextLink(BSContext *stack) {
  if (!stack)
    return NULL;
  BS_LOCK(stack);
  stack->refcnt++;
  BS_UNLOCK(stack);
  return stack;
}

void bsContextUnlink(BSContext *stack) {
  if (!stack)
    return;
  BS_LOCK(stack); /* we may never relinquish this */
  if (stack->refcnt <= 1) {
    /* free the stack - lock is not relinquished */
    if (stack->vars)
      bsHashDestroy(stack->vars,(void (*)(void *))bsVarFree);
    if (stack->cproctable)
      bsHashDestroy(stack->cproctable,
		    (void (*)(void *))bsProcDestroy);
    if (stack->unknown)
      bsProcDestroy(stack->unknown);
    bsFree(stack);
    return; /* IMPORTANT: return without trying to "unlock" */
  } else
    stack->refcnt--;
  BS_UNLOCK(stack);
}

/* variable stacks */
BSContext *bsContextPush(BSContext *stack) {
  BSContext *s;
  s = bsAllocObj(BSContext);
  s->vars = bsHashCreate();
  s->up = stack;
  BS_MUTEX_INIT(s);
  bsContextLink(s);
  return s;
}

BSContext *bsContextNest(BSContext *stack) {
  BSContext *s;
  s = bsAllocObj(BSContext);
  s->vars = bsHashCreate();
  s->left = stack;
  BS_MUTEX_INIT(s);
  bsContextLink(s);
  return s;
}

BSContext *bsContextUnnest(BSContext *stack) {
  BSContext *ret;

  if (!stack || !stack->left) {
    BS_FATAL("bsContextUnnest: context is not nested");
  }

  ret = stack->left;
  bsContextUnlink(stack);
  return ret;
}

BSContext *bsContextPop(BSContext *stack) {
  BSContext *ret;
  if (!stack)
    return NULL;

  /* go back to left as far as possible */
  while (stack->left) {
    BSContext *n;
    n = stack->left;
    bsContextUnlink(stack);
    stack = n;
  }

  ret = stack->up;
  bsContextUnlink(stack);
  return ret;
}

void bsInterpOptSet(BSInterp *i, int flags, int val) {
  if (i) {
    if (val)
      i->opt |= flags;
    else
      i->opt &= ~flags;
  }
}

BSInterp *bsInterpCreate(void) {
  BSInterp *i;
  i = bsAllocObj(BSInterp);
  i->stack = bsContextPush(NULL); /* create new stack */
  i->global = i->stack;
  bsContextLink(i->global); /* prevent global from vanishing */
  i->proctable = bsHashCreate();
  i->result = bsObjNew();
  return i;
}

void bsInterpDestroy(BSInterp *i) {
  if (i) {
    if (i->result) {
      bsObjDelete(i->result);
      i->result = NULL;
    }
    if (i->proctable) {
      bsHashDestroy(i->proctable,(void (*)(void *))bsProcDestroy);
      i->proctable = NULL;
    }
    /* i->globalVars points at context... */
    while (i->stack)
      i->stack = bsContextPop(i->stack);
    bsContextUnlink(i->global); /* relinquish hold on global level */
    bsFree(i);
  }
}

BSObject *bsGetResult(BSInterp *i) {
  if (i) {
    if (!i->result)
      i->result = bsObjNew();
    return i->result;
  }
  return NULL;
}

int bsIsResultClear(BSInterp *i) {
  if (!i || !i->result)
    return 1; /* yep - clear */
  return bsObjIsNull(i->result);
}

int bsSetObjResult(BSInterp *i, BSObject *o) {
  if (!i)
    return 0;
  if (i->result) {
    bsObjDelete(i->result);
    i->result = NULL;
  }
  i->result = (o ? o : bsObjNew());
  return 0;
}

int bsSetResult(BSInterp *i, BSObject *o) {
  if (!i)
    return 0; /* ignore cases where i is NULL */
  if (!o) {
    if (!i->result)
      i->result = bsObjNew();
    else
      bsObjInvalidate(i->result,0);
  } else {
    if (!i->result)
      i->result = bsObjCopy(o);
    else
      bsObjSetCopy(i->result,o);
  }
  return 0;
}

int bsSetStringResult(BSInterp *i, char *str, int type) {
  if (!i)
    return 0;
  if (i->result)
    bsObjSetString(i->result,str,-1,type);
  else
    i->result = bsObjString(str,-1,type);
  return 0;
}

int bsSetIntResult(BSInterp *i, BSInt x) {
  if (!i)
    return 0;
  if (i->result)
    bsObjSetInt(i->result,x);
  else
    i->result = bsObjInt(x);
  return 0;
}

int bsSetFloatResult(BSInterp *i, BSFloat x) {
  if (!i)
    return 0;
  if (i->result)
    bsObjSetFloat(i->result,x);
  else
    i->result = bsObjFloat(x);
  return 0;
}

int bsClearResult(BSInterp *i) {
  return bsSetResult(i,NULL);
}

int bsAppendResult(BSInterp *i, ...) {
  BSString *str;
  char *s;
  va_list ap;

  if (!i)
    return 0; /* ignore cases where i is NULL */

  if (!i->result)
    i->result = bsObjNew();
  str = bsObjGetString(i->result);
  va_start(ap,i);
  while ((s = va_arg(ap,char *)) != NULL)
    bsStringAppend(str,s,-1);
  va_end(ap);
  return 0;
}

int bsCallProc(BSInterp *i, BSProc *proc, int objc, BSObject **objv) {
  int unknown = 0;

  bsClearResult(i);

  if (!proc || proc->type == BS_PROC_NONE) {
    if (proc = bsGetUnknown(i,NULL))
      unknown = 1; /* continue with unknown handler */
    else {
      if (bsIsResultClear(i))
	bsAppendResult(i,"unknown procedure in call: ",
		       (objc > 0 ? bsObjGetStringPtr(objv[0]) : "<null>"),
		       NULL);
      return BS_ERROR;
    }
  }
  switch (proc->type) {
  case BS_PROC_NONE: /* empty slot */
    assert(unknown != 0); /* unknown slot should never be "none" */
    bsAppendResult(i,"no such procedure: ",bsObjGetStringPtr(objv[0]),NULL);
    return BS_ERROR;

  case BS_PROC_EXT:
    {
      int k;
      k = proc->data.ext.proc(i,objc,objv,proc->data.ext.cdata);
      return k;
    }

  case BS_PROC_SCRIPT:
    {
      int code, k;

      /* strip off "name" object if this is not an unknown handler */
      if (!unknown) {
	objv++;
	objc--;
      }

      /* check for a valid call before we do any work */
      if (objc < proc->data.script.nargs) {
	/* this is okay iff the unspecified arguments have default
	   values */
	for (k = objc; k < proc->data.script.nargs; k++)
	  if (!proc->data.script.args[k].defvalue) {
	    /* missing default, so we cannot satisfy this argument */
	    bsAppendResult(i,"too few arguments",NULL);
	    return BS_ERROR;
	  }
      } else if (objc > proc->data.script.nargs) {
	/* this is okay iff the CATCHALL flag is set */
	if (!(proc->data.script.flags & BS_PROC_CATCHALL)) {
	  bsAppendResult(i,"too many arguments",NULL);
	  return BS_ERROR;
	}
      }
      
      i->stack = bsContextPush(i->stack);
      for (k = 0; k < objc && k < proc->data.script.nargs; k++)
	bsSet(i,NULL,proc->data.script.args[k].name,objv[k]);
      for (  ; k < proc->data.script.nargs; k++) {
	assert(proc->data.script.args[k].defvalue != NULL);
	bsSet(i,NULL,proc->data.script.args[k].name,
	      proc->data.script.args[k].defvalue);
      }
      if (k < objc) {
	BSObject *o;
	BSList *l;
	/* force creation of variable if it does not exist */
	o = bsGet(i,NULL,BS_PROC_CATCHALL_NAME,1);
	bsObjSetList(o,0,NULL);
	l = bsObjGetList(NULL,o);
	assert(l != NULL);
	while (k < objc)
	  bsListPush(l,bsObjCopy(objv[k++]),BS_TAIL);
      }
      /* let's get running... */
      bsClearResult(i);
      code = bsEval(i,proc->data.script.body);
      /* Normalize the code */
      if (code == BS_RETURN)
	code = BS_OK;
      else if (code != BS_OK)
	code = BS_ERROR;

      i->stack = bsContextPop(i->stack);
      return code;
    }
    break;
    
  default:
    BS_FATAL("bsCallProc: proc has invalid type");
  }
  BS_FATAL("bsCallProc: unreachable");
}

/* this is useful debugging stuff - consider putting it
   somewhere real */
static void write_obj(BSObject *o) {
  if (!o)
    fprintf(stderr,"{}");
  switch (o->quote) {
  case BS_Q_LIST:
    putc('{',stderr);
    break;
  case BS_Q_ELIST:
    putc('[',stderr);
    break;
  case BS_Q_STRING:
    putc('"',stderr);
    break;
  case BS_Q_VARIABLE:
    putc('$',stderr);
    break;
  case BS_Q_QVARIABLE:
    putc('$',stderr);
    putc('{',stderr);
    break;
  }
  fputs(bsObjGetStringPtr(o),stderr);
  switch (o->quote) {
  case BS_Q_LIST:
    putc('}',stderr);
    break;
  case BS_Q_ELIST:
    putc(']',stderr);
    break;
  case BS_Q_STRING:
    putc('"',stderr);
    break;
  case BS_Q_QVARIABLE:
    putc('}',stderr);
    break;
  }
}

static void write_cmd(BSList *call) {
  int k, n;
  n = bsListSize(call);
  if (n > 0) {
    write_obj(bsListIndex(call,0));
    for(k = 1; k < n; k++) {
      putc(' ',stderr);
      write_obj(bsListIndex(call,k));
    }
  }
}

/* speed up things for small call cases */
#define RESLISTSZ 8
int bsProcessLine(BSInterp *i, BSList *call) {
  BSObject *reslist[RESLISTSZ+1];
  BSObject *resfreelist[RESLISTSZ+1];
  BSObject **dynlist = NULL;
  BSObject **dynfreelist = NULL;
  BSObject **objv, **freelist;
  BSObject *o;
  int k, n, freecnt = 0;
  int code = BS_ERROR;

  bsClearResult(i);
  if (!call) {
    bsSetStringResult(i,"list for callproc is null",BS_S_STATIC);
    return BS_ERROR;
  }
  n = bsListSize(call);
  if (n <= 0) {
    bsSetStringResult(i,"cannot callproc with empty list",BS_S_STATIC);
    return BS_ERROR;
  }
  /* do we need to allocate space? */
  if (n < RESLISTSZ) {
    /* no */
    objv = reslist;
    freelist = resfreelist;
  } else {
    /* yes */
    dynlist = bsAlloc(sizeof(BSObject *)*(n+1),0);
    dynfreelist = bsAlloc(sizeof(BSObject *)*(n+1),0);
    objv = dynlist;
    freelist = dynfreelist;
  }

  /* resolve all elements in the list */
  for (k = 0, o = call->head; 
       k < n && o; 
       k++, o = o->next) {
    switch (o->quote) {
    case BS_Q_NONE:
    case BS_Q_LIST:
      /* no special parsing... */
      objv[k] = o;
      break;

    case BS_Q_ELIST:
      /* process this line... */
      {
	BSList *scall;
	int scode;
	if (!(scall = bsObjGetList(i,o))) {
	  code = BS_ERROR;
	  goto cleanup;
	}
	bsClearResult(i);
	if ((scode = bsProcessLine(i,scall)) != BS_OK) {
	  if (scode != BS_ERROR)
	    bsAppendResult(i,"invalid code from sub-call: ",
			   bsCodeToString(scode),NULL);
	  code = BS_ERROR;
	  goto cleanup;
	}
	freelist[freecnt] = bsObjCopy(i->result);
	objv[k] = freelist[freecnt++];
      }
      break;

    case BS_Q_STRING:
      /* perform substitutions inside string constructor */
      {
	BSString *s;
	freelist[freecnt] = bsObjNew();
	s = bsObjGetString(freelist[freecnt]);
	if (bsSubsObj(i,s,o,0)
	    != BS_OK) { 
	  /* bsSubs sets error */
	  code = BS_ERROR;
	  goto cleanup;
	}
	objv[k] = freelist[freecnt++];
      }
      break;

    case BS_Q_VARIABLE:
    case BS_Q_QVARIABLE:
      /* look up the variable */
      {
	if (!(objv[k] = bsGet(i,NULL,bsObjGetStringPtr(o),0))) {
	  /* bsGet sets error */
	  code = BS_ERROR;
	  goto cleanup;
	}
      }
      break;

    default:
      BS_FATAL("bsProcessLine: object in argument list has invalid quoting");
    }
  }
  if (k < n)
    BS_FATAL("bsProcessLine: call list has incorrect size");
  objv[k] = NULL; /* terminate list */

  /* Locating procedures:
     If (objv[0]) is an opaque type, then pass this function to the
     opaque driver.
  */
  assert(objv[0] != NULL);
  if (objv[0]->repValid & BS_T_OPAQUE) {
    assert(objv[0]->opaqueRep != NULL);
    if (!objv[0]->opaqueRep->driver ||
	!objv[0]->opaqueRep->driver->proc) {
      bsSetStringResult(i,"opaque object does not support procedures",
			BS_S_STATIC);
      code = BS_ERROR;
      goto cleanup;
    }
    /* do not pass objv[0] as part of the command array */
    code = objv[0]->opaqueRep->driver->proc(objv[0],i,n-1,objv+1);
    /* normalize result */
    if (code == BS_RETURN)
      code = BS_OK;
    else if (code != BS_OK)
      code = BS_ERROR;
  } else {
    /* Okay - objv[0] is not opaque, so try to turn it into
       a procedure reference */
    code = bsCallProc(i,bsObjGetProc(i,objv[0]),k,objv);
  }
  /* fall-through */

 cleanup:
  for (k = 0; k < freecnt; k++)
    bsObjDelete(freelist[k]);
  bsFree(dynfreelist);
  bsFree(dynlist);
  return code;
}

BSProc *bsProcExt(BSProc *p, BSExtProc proc, void *cdata) {
  if (!p)
    p = bsProcAlloc();
  else
    bsProcClear(p);
  p->type = BS_PROC_EXT;
  p->data.ext.proc = proc;
  p->data.ext.cdata = cdata;
  return p;
}

BSProc *bsProcScript(BSProc *p, int nargs, BSProcArg *args, 
		     BSObject *body, int flags) {
  if (!p)
    p = bsProcAlloc();
  else
    bsProcClear(p);
  p->type = BS_PROC_SCRIPT;
  p->data.script.nargs = nargs;
  p->data.script.args = args;
  p->data.script.body = bsObjCopy(body);
  p->data.script.flags = flags;
  return p;
}

/* convert a procedure slot to an empty one */
void bsProcClear(BSProc *p) {
  if (p) {
    if (p->type == BS_PROC_SCRIPT) {
      int k;
      for (k = 0; k < p->data.script.nargs; k++) {
	bsFree(p->data.script.args[k].name);
	if (p->data.script.args[k].defvalue)
	  bsObjDelete(p->data.script.args[k].defvalue);
      }
      if (p->data.script.body)
	bsObjDelete(p->data.script.body);
    }
    memset(p,0,sizeof(BSProc));
    p->type = BS_PROC_NONE;
  }
}

BSProc *bsProcAlloc(void) {
  BSProc *p;
  p = bsAllocObj(BSProc);
  p->type = BS_PROC_NONE;
  return p;
}

void bsProcDestroy(BSProc *p) {
  if (p) {
    bsProcClear(p);
    bsFree(p);
  }
}

static void *proc_copyproc(void *priv, void *cdata) {
  return priv;
}

static BSCacheDriver proc_driver = {
  "proc",
  0,             /* initialize later */
  NULL,          /* freeproc */
  proc_copyproc, /* copyproc */
  NULL           /* cdata */
};

BSCacheDriver *bsGetProcDriver(void) {
  if (proc_driver.id == 0)
    proc_driver.id = bsUniqueId();
  return &proc_driver;
}

void bsObjCacheProc(BSObject *o, BSProc *p) {
  BSCacheDriver *d;
  d = bsGetProcDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  bsObjAddCache(o,d,p);
}

BSProc *bsObjGetProcFromHash(BSHash *h, BSObject *o, int force) {
  BSCacheDriver *d;
  BSString *s;
  BSProc *p = NULL;

  d = bsGetProcDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  
  if (bsObjGetCache(o,d->id,(void **)&p) == 0)
    return p;

  s = bsObjGetString(o);
  assert(s != NULL);
  
  if (!(p = bsGetProcFromHash(h,bsStringPtr(s),0)))
    return NULL;

  bsObjAddCache(o,d,(void *)p);

  return p;
}

/* Because of context procedures, I've needed to disable
   procedure caching for now.  There should be an effective
   way to cache it, but I need to know to invalidate the cache
   when procedures are created or destroyed. */
BSProc *bsObjGetProc(BSInterp *i, BSObject *o) {
#if 0
  BSCacheDriver *d;
#endif
  BSString *s;
  BSProc *p = NULL;

#if 0
  d = bsGetProcDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  
  if (bsObjGetCache(o,d->id,(void **)&p) == 0)
    return p;
#endif  

  s = bsObjGetString(o);
  assert(s != NULL);

  if (!(p = bsResolveProc(i,NULL,bsStringPtr(s))))
    return NULL; /* bsGetProc() sets error */

#if 0
  if (!(i->opt & BS_OPT_MEMORY))
    bsObjAddCache(o,d,(void *)p);
#endif

  return p;
}

BSProc *bsGetProcFromHash(BSHash *h, char *name, int force) {
  BSProc *p;
  if (!(p = (BSProc *)bsHashLookup(h,name))) {
    /* force it, by creating an empty slot */
    if (force) {
      p = bsProcAlloc();
      bsHashInsert(h,name,p);
    }
  }
  return p;
}

BSProc *bsGetProc(BSInterp *i, char *name, int force) {
  BSProc *p;
  if (!i)
    return NULL;
  /* search context */
  assert(i->proctable != NULL);
  return bsGetProcFromHash(i->proctable,name,force);
}

BSProc *bsResolveProc(BSInterp *i, BSContext *ctx, char *name) {
  BSProc *p;
  if (!i)
    return NULL;
  if (!ctx) {
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  while (ctx) {
    if ((p = bsGetCProc(i,ctx,name,0)))
      return p;
    ctx = ctx->left;
  }
  if ((p = bsGetProc(i,name,0)))
    return p;
  bsClearResult(i);
  bsAppendResult(i,"no such procedure: ",name,NULL);
  return NULL;
}

int bsSetExtProc(BSInterp *i, char *name, BSExtProc proc, void *cdata) {
  BSProc *p;
  p = bsGetProc(i,name,1);
  if (!p) {
    bsSetStringResult(i,"cannot acquire procedure slot",BS_S_STATIC);
    return BS_ERROR;
  }
  bsProcExt(p,proc,cdata);
  return BS_OK;
}

int bsSetScriptProc(BSInterp *i, char *name, int nargs, BSProcArg *args, 
		    BSObject *body, int flags) {
  BSProc *p;
  p = bsGetProc(i,name,1);
  if (!p) {
    bsSetStringResult(i,"cannot acquire procedure slot",BS_S_STATIC);
    return BS_ERROR;
  }
  bsProcScript(p,nargs,args,body,flags);
  return BS_OK;
}

int bsUnsetProc(BSInterp *i, char *name) {
  BSProc *p;
  
  if ((p = bsGetProc(i,name,0)))
    bsProcClear(p);
  return BS_OK;
}

int bsSetExtCProc(BSInterp *i, BSContext *ctx,
		  char *name, BSExtProc proc, void *cdata) {
  BSProc *p;
  p = bsGetCProc(i,ctx,name,1);
  if (!p) {
    bsSetStringResult(i,"cannot acquire procedure slot",BS_S_STATIC);
    return BS_ERROR;
  }
  bsProcExt(p,proc,cdata);
  return BS_OK;
}

int bsSetScriptCProc(BSInterp *i, BSContext *ctx,
		    char *name, int nargs, BSProcArg *args, 
		    BSObject *body, int flags) {
  BSProc *p;
  p = bsGetCProc(i,ctx,name,1);
  if (!p) {
    bsSetStringResult(i,"cannot acquire procedure slot",BS_S_STATIC);
    return BS_ERROR;
  }
  bsProcScript(p,nargs,args,body,flags);
  return BS_OK;
}

int bsUnsetCProc(BSInterp *i, BSContext *ctx, char *name) {
  BSProc *p;
  if ((p = bsGetCProc(i,ctx,name,0)))
    bsProcClear(p);
  return BS_OK;
}

BSProc *bsGetCProc(BSInterp *i, BSContext *ctx, char *name, int force) {
  BSProc *p;
  if (!i)
    return NULL;
  if (!ctx) {
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  if (!ctx->cproctable) {
    if (!force)
      return NULL; /* no proctable and no need to build one */
    ctx->cproctable = bsHashCreate();
  }
  return bsGetProcFromHash(ctx->cproctable,name,force);
}

BSProc *bsGetUnknown(BSInterp *i, BSContext *ctx) {
  if (!ctx) {
    if (!i)
      return NULL;
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  while (ctx) {
    if (ctx->unknown)
      return ctx->unknown;
    ctx = ctx->left;
  }
  /* try global handler */
  if (i && i->global)
    return i->global->unknown;
  return NULL;
}

int bsSetExtUnknown(BSInterp *i, BSContext *ctx, BSExtProc proc, void *cdata) {
  if (!ctx) {
    if (!i)
      return BS_ERROR;
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  if (ctx->unknown) {
    bsProcClear(ctx->unknown);
    bsProcExt(ctx->unknown,proc,cdata);
  } else {
    ctx->unknown = bsProcExt(NULL,proc,cdata);
  }
  return BS_OK;
}

int bsSetScriptUnknown(BSInterp *i, BSContext *ctx, int nargs, BSProcArg *args,
		       BSObject *body, int flags) {
  if (!ctx) {
    if (!i)
      return BS_ERROR;
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  if (ctx->unknown) {
    bsProcClear(ctx->unknown);
    bsProcScript(ctx->unknown,nargs,args,body,flags);
  } else {
    ctx->unknown = bsProcScript(NULL,nargs,args,body,flags);
  }
  return BS_OK;
}

void bsUnsetUnknown(BSInterp *i, BSContext *ctx) {
  if (!ctx) {
    if (!i)
      return;
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  if (ctx->unknown) {
    bsProcDestroy(ctx->unknown);
    ctx->unknown = NULL;
  }
}

/* l is a parsed list */
int bsEvalScript(BSInterp *i, BSList *l) {
  BSObject *o;

  bsClearResult(i);
  if (!l)
    return BS_OK;

  for (o = l->head; o; o = o->next) {
    int k;
    BSList *el;
    bsClearResult(i);
    if (!(el = bsObjGetList(i,o)))
      return BS_ERROR;
    if ((k = bsProcessLine(i,el)) != BS_OK) {
      /* includes error, break, continue, etc. */
      return k;
    }
  }
  return BS_OK;
}

static void script_freeproc(void *priv, void *cdata) {
  BSList *l = (BSList *)priv;
  if (l) {
    bsListClear(l);
    bsFree(l);
  }
}

static void *script_copyproc(void *priv, void *cdata) {
  BSList *l, *l2;
  if (!priv)
    return NULL; /* no copy */
  l = (BSList *)priv;
  l2 = bsAllocObj(BSList);
  bsListAppend(l2,l);
  return (void *)l2;
}

static BSCacheDriver script_driver = {
  "script",         /* name */
  0,                /* id - assign later */
  script_freeproc,  /* freeproc */
  script_copyproc,  /* copyproc */
  NULL              /* cdata */
};

BSCacheDriver *bsGetScriptDriver(void) {
  if (script_driver.id == 0)
    script_driver.id = bsUniqueId();
  return &script_driver;
}

BSList *bsGetScript(BSInterp *i, BSObject *o, int *mustfree_r) {
  BSParseSource sp;
  BSObject *so;
  BSList *l = NULL;
  BSCacheDriver *d;
  int free_list = 0;

  bsClearResult(i);

  if (!o)
    return BS_OK; /* nothing to do */

  d = bsGetScriptDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);

  if (bsObjGetCache(o,d->id,(void **)&l)) {
    /* try to create a script representation */
    l = bsAllocObj(BSList);
    bsListInit(l);
    if (bsParseScript(i,bsStringSource(&sp,bsObjGetStringPtr(o)),l)
	!= BS_OK) {
      bsListClear(l);
      bsFree(l);
      /* should I be clearer about *why* we're not executing? */
      return NULL;
    }
    if (i && (i->opt & BS_OPT_MEMORY))
      free_list = 1;
    else
      bsObjAddCache(o,d,(void *)l);
  }
  if (mustfree_r)
    *mustfree_r = free_list;
  return l;
}

int bsEval(BSInterp *i, BSObject *o) {
  BSList *l = NULL;
  int k, free_list;

  if (!i)
    BS_FATAL("bsEval called with NULL interpreter");

  bsClearResult(i);

  if (!o)
    return BS_OK; /* nothing to do */
  if (!(l = bsGetScript(i,o,&free_list)))
    return BS_ERROR;

  k = bsEvalScript(i,l);
  
  if (free_list) {
    bsListClear(l);
    bsFree(l);
  }

  return k;
}

int bsEvalSource(BSInterp *i, BSParseSource *sp) {
  BSList script;
  int code;

  bsListInit(&script);
  bsClearResult(i);

  if (!sp)
    return BS_OK;

  if (bsParseScript(i,sp,&script) != BS_OK) {
    bsListClear(&script);
    return BS_ERROR;
  }
  code = bsEvalScript(i,&script);
  bsListClear(&script);
  return code;
}

int bsEvalStream(BSInterp *i, FILE *f) {
  BSParseSource sp;
  if (!f) {
    bsClearResult(i);
    return BS_OK;
  }
  return bsEvalSource(i,bsFileSource(&sp,f));
}

int bsEvalString(BSInterp *i, char *s) {
  BSParseSource sp;
  if (!s) {
    bsClearResult(i);
    return BS_OK;
  }
  return bsEvalSource(i,bsStringSource(&sp,s));
}

int bsEvalFile(BSInterp *i, char *filename) {
  BSParseSource sp;
  FILE *f;
  int code;

  if (!filename) {
    bsClearResult(i);
    bsAppendResult(i,"filename is null",NULL);
    return BS_ERROR;
  }

  if (!(f = fopen(filename,"r"))) {
    bsClearResult(i);
    bsAppendResult(i,"could not read file ",filename,": ",
		   strerror(errno),NULL);
    return BS_ERROR;
  }

  code = bsEvalSource(i,bsFileSource(&sp,f));
  fclose(f);

  return code;
}

char *bsCodeToString(int code) {
  switch (code) {
  case BS_OK:        return "ok";
  case BS_ERROR:     return "error";
  case BS_CONTINUE:  return "continue";
  case BS_BREAK:     return "break";
  case BS_RETURN:    return "return";
  default: return "<unknown>";
  }
}

BSInterp *bsInterpCreateStd(void) {
  BSInterp *i;
  i = bsInterpCreate();
  if (i) {
    bsCoreProcs(i);
    bsStringProcs(i);
    bsListProcs(i);
    bsHashProcs(i);
  }
  return i;
}

/* context wrappers */
void bsInterpPush(BSInterp *i) {
  if (!i)
    return;
  i->stack = bsContextPush(i->stack);
}

void bsInterpPop(BSInterp *i) {
  if (!i)
    return;
  i->stack = bsContextPop(i->stack);
}

void bsInterpNest(BSInterp *i) {
  if (!i)
    return;
  i->stack = bsContextNest(i->stack);
}

void bsInterpUnnest(BSInterp *i) {
  if (!i)
    return;
  i->stack = bsContextUnnest(i->stack);
}
