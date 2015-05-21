#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluescript.h"

/* variable control */
BSVariable *bsVarCreate(void) {
  BSVariable *v;
  v = bsAllocObj(BSVariable);
  v->type = BS_V_UNSET;
  return v;
}

void bsVarClear(BSVariable *v) {
  if (v) {
    if (v->o) {
      bsObjDelete(v->o);
      v->o = NULL;
    }
    v->type = BS_V_UNSET;
    v->link = NULL;
  }
}

void bsVarFree(BSVariable *v) {
  if (v) {
    if (v->o)
      bsObjDelete(v->o);
    bsFree(v);
  }
}

static BSVariable *find_slot(BSContext *ctx, char *name, int force) {
  BSContext *c;
  BSVariable *v;

  /* search to the left */
  for (c = ctx; c; c = c ->left) {
    if (c->vars && (v = (BSVariable *)bsHashLookup(c->vars,name)))
      return v; /* found it */
  }
  if (force) {
    /* create in given context */
    v = bsVarCreate();
    bsHashInsert(ctx->vars,name,v);
    return v;
  }
  return NULL;
}

/* make a link to another variable... */
BSVariable *bsVarLink(BSContext *ctx, char *name) {
  BSVariable *v, *link;
  
  /* find the link */
  link = bsGetVar(NULL,ctx,name,1);
  assert(link != NULL);
  v = bsVarCreate();
  v->type = BS_V_LINK;
  v->link = link;
  return v;
}

BSVariable *bsResolveVar(BSInterp *i, BSVariable *v) {
  while (v && (v->type == BS_V_LINK))
    v = v->link;
  return v;
}

/* "force" -- means force return of non-null slot */
BSVariable *bsGetVar(BSInterp *i, BSContext *ctx, char *name, int force) {
  BSVariable *v;
  if (!ctx) {
    if (!i)
      return NULL;
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  if (!(v = find_slot(ctx,name,force))) {
    bsClearResult(i);
    bsAppendResult(i,"cannot find variable ",name,NULL);
    return NULL;
  }
  return v;
}

/* Since other bits of code may reference this slot, do not change
   the address of the slot, but do change its contents... */
int bsSetVar(BSInterp *i, BSContext *ctx, char *name, BSVariable *var) {
  BSVariable *v;
  if (!ctx) {
    if (!i)
      return BS_ERROR;
    assert(i->stack != NULL);
    ctx = i->stack;
  }
  v = find_slot(ctx,name,1);
  assert(v != NULL);
  if (!var) {
    v->type = BS_V_UNSET;
    if (v->o) {
      bsObjDelete(v->o);
      v->o = NULL;
    }
    v->link = NULL;
  } else {
    v->type = var->type;
    if (var->o)
      bsObjSetCopy(v->o,var->o);
    else {
      bsObjDelete(v->o);
      v->o = NULL;
    }
    v->link = var->link;
  }
  return BS_OK;
}

int bsSet(BSInterp *i, BSContext *ctx, char *name, BSObject *value) {
  BSVariable *v;
  if (!(v = bsGetVar(i,ctx,name,1)))
    return BS_ERROR; /* something failed - let lower error report it */
  /* !!! There is a danger of an infinite loop here if we create a
     circular link of variable links !!! */
  while (v->type == BS_V_LINK) {
    assert(v->link != NULL);
    v = v->link;
  }
  assert(v->type != BS_V_LINK);
  switch (v->type) {
  case BS_V_LOCAL:
    assert(v->o != NULL);
    if (!value) {
      v->type = BS_V_UNSET;
      bsObjDelete(v->o);
      v->o = NULL;
    } else
      bsObjSetCopy(v->o,value);
    break;

  case BS_V_UNSET:
    assert(v->o == NULL);
    if (value) { /* otherwise nothing to do */
      v->type = BS_V_LOCAL;
      v->o = bsObjCopy(value);
    }
    break;

  default:
    BS_FATAL("bsSet: variable has invalid type");
  }
  return BS_OK;
}

int bsUnset(BSInterp *i, BSContext *ctx, char *name) {
  return bsSet(i,ctx,name,NULL);
}

BSObject *bsGet(BSInterp *i, BSContext *ctx, char *name, int force) {
  BSVariable *v;

  if (!(v = bsGetVar(i,ctx,name,force)))
    return NULL; /* error? */

  while (v->type == BS_V_LINK) {
    assert(v->link != NULL);
    v = v->link;
  }
  assert(v->type != BS_V_LINK);

  if (v->type == BS_V_UNSET) {
    assert(v->o == NULL);
    if (!force)
      return NULL;
    v->type = BS_V_LOCAL;
    v->o = bsObjNew();
  }

  assert(v->type != BS_V_LINK && v->type != BS_V_UNSET);
  
  switch (v->type) {
  case BS_V_LOCAL:
    assert(v->o != NULL);
    return v->o;

  default:
    BS_FATAL("bsGet: variable has invalid type");
  }
  
  BS_FATAL("bsGet: unreachable");
  return NULL; /* should never be executed */
}

/* place-holders - we may use these to cache variable slots in the
   future */
int bsSetObj(BSInterp *i, BSContext *ctx, BSObject *o, 
		   BSObject *value) {
  return (bsSet(i,ctx,bsObjGetStringPtr(o),value));
}

BSObject *bsGetObj(BSInterp *i, BSContext *ctx, BSObject *o, int force) {
  return (bsGet(i,ctx,bsObjGetStringPtr(o),force));
}
