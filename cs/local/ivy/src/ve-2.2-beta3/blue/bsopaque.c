/*
  Some thoughts to consider later:
  - all inheritance is a "merge"
     - i.e. flatten namespace
     - easy to implement
     - may mean that inheriting breaks functionality in inherited
       classes (e.g. something you inherit blots out something else)
       - but that just means you need to know *what* you are
         inheriting
     - other relationships would be handled with explicit
       encapsulation (i.e. "inherited" object is just a field
       of the class)
  - belonging to a "class" really just means that you are prepared
    to provide certain methods (really more of an interface model)
*/

/* Script-based opaque objects */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluescript.h"

struct bs_opaque_model_ref;

/* place-holders for standards */
static BSProc _opaque_isa;
static BSProc _opaque_dup;

#define _OPAQUE_ISA (&opaque_isa)
#define _OPAQUE_DUP (&opauqe_dup)

static struct {
  char *name;
  int id;
  BSProc *p;
} std_names[] = {
  { "isa", BS_OPAQUE_ISA, _OPAQUE_ISA },
  { "dup", BS_OPAQUE_DUP, _OPAQUE_DUP },
  { NULL, -1 }
};

typedef struct bs_opaque_model {
  char *name;             /* name assigned */
  BSOpaqueModel *inherit; /* inheritance list */
  BSHash *methods;
} BSOpaqueModel;

/* mapping of names to models */
static BSHash *opaque_models = NULL;

typedef struct bs_opaque_script {
  struct bs_opaque_script *next;    /* next in inheritance list */
  struct bs_opaque_script *inherit; /* head of inheritance list */
  BSOpaqueModel *model; /* model for this level */
  BSHash *vars;         /* variable storage */
} BSOpaqueScript;

typedef struct bs_opaque_cache {
  int local; /* if non-zero, then this is a cached opaque call (to self)
		otherwise, it is a generic call */
  BSProc *p;
} BSOpaqueCache;


/* by default, we cannot convert to anything else
   (map method names?) */
static int opaque_makerep(BSObject *o, int rep) {
  return BS_ERROR;
}

static int opaque_destroy(BSObject *o) {
  
}

static int opaque_proc(BSObject *o, BSInterp *i, int objc, BSObject **objv) {
  BSOpaqueScript *s;
  BSOpaque *save;
  BSProc *p;
  int id;

  s = (BSOpaqueScript *)(o->opaqueRep->data);

  /* set context */
  if (!(p = bsObjGetProcFromHash(s->methods,objv[0]))) {
    /* try for "standard" functions */
    if (p = /* convert to std-name */(objv[0])) {
      bsObjCacheProc(objv[0],p);
    }
  }

  bsClearResult(i);
  if (!p) {
    bsAppendResult(i,"no such method: ",bsObjGetStringPtr(objv[0]),NULL);
    return BS_ERROR;
  }

  if (p == _OPAQUE_ISA) {
    static char *usage = "<o> isa <class>";
    char *c;
    /* special case */
    if (objc != 2) {
      bsSetStringResult(i,usage,BS_S_STATIC);
      return BS_ERROR;
    }
    c = bsObjGetStringPtr(objv[1]);
    assert(c != NULL);
    if (strcmp(s->name,c) == 0)
      bsSetIntResult(i,1); /* easy */
    else {
      BSOpaque
    }

  } else if (p == _OPAQUE_DUP) {
    /* another special case */
    /* ... */
  } else {
    save = bsInterpSetContext(i,o->opaqueRep);
    k = bsCallOpaqueProc(i,o->opaqueRep,p,objc,objv) :
    bsInterpSetContext(i,save);
    return k;
  }
  
}
