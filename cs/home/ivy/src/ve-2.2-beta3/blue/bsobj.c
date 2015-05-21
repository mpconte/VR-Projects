#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluescript.h"

#ifdef _WIN32
#define snprintf _snprintf
#endif /* _WIN32 */

BSObject *bsObjNew(void) {
  BSObject *o;
  o = bsAllocObj(BSObject);
  bsListInit(&(o->listRep));
  bsStringInit(&(o->stringRep));
  return o;
}

BSObject *bsObjString(char *str, int len, int type) {
  BSObject *o;
  o = bsObjNew();
  bsObjSetString(o,str,len,type);
  return o;
}

BSObject *bsObjList(int objc, BSObject *objv[]) {
  BSObject *o;
  o = bsObjNew();
  bsObjSetList(o,objc,objv);
  return o;
}

BSObject *bsObjInt(int i) {
  BSObject *o;
  o = bsObjNew();
  bsObjSetInt(o,i);
  return o;
}

BSObject *bsObjFloat(float f) {
  BSObject *o;
  o = bsObjNew();
  bsObjSetFloat(o,f);
  return o;
}

BSObject *bsObjOpaque(BSOpaque *p) {
  BSObject *o;
  o = bsObjNew();
  bsObjSetOpaque(o,p);
  return o;
}

BSObject *bsObjCopy(BSObject *orig) {
  BSObject *o;
  o = bsObjNew();
  bsObjSetCopy(o,orig);
  return o;
}

void bsObjClearCache(BSObject *o) {
  BSCachedRep *r, *rn;
  if (o) {
    r = o->cachedRep;
    while (r) {
      rn = r->next;
      if (r->drv && r->drv->freeproc) {
	r->drv->freeproc(r->priv,r->drv->cdata);
      }
      bsFree(r);
      r = rn;
    }
    o->cachedRep = NULL;
  }
}

void bsObjDelete(BSObject *o) {
  if (o) {
    bsObjInvalidate(o,0);
    /* string may still be allocated */
    bsStringFreeSpace(&(o->stringRep));
    bsFree(o);
  }
}

int bsObjIsNull(BSObject *o) {
  if (!o)
    return 1;
  /* a "null" object is one with no valid representations */
  return (o->repValid == 0);
}

/* means invalidate any representation which is *not* in the
   mask */
void bsObjInvalidate(BSObject *o, int mask) {
  int todel;
  if (!o)
    return; /* nothing to do? */
  todel = (o->repValid & ~mask);
  o->repValid &= mask;

  /* int, float reps do not have memory associated with them */
  /* for now, clear string rep but do not free space */
  if (todel & BS_T_STRING)
    bsStringClear(&(o->stringRep));

  if (todel & BS_T_LIST)
    bsListClear(&(o->listRep));

  if (todel & BS_T_OPAQUE) {
    assert(o->opaqueRep != NULL);
    assert(o->opaqueRep->refcnt > 0 || o->opaqueRep->linkcnt > 0);
    if (o->opaqueLink)
      o->opaqueRep->linkcnt--;
    else
      o->opaqueRep->refcnt--; /* lose a reference */
    if (o->opaqueRep->refcnt == 0 && bsObjIsGarbage(o)) {
      /* no more references - opaque object is isolated */
      if (o->opaqueRep->driver && o->opaqueRep->driver->destroy)
	o->opaqueRep->driver->destroy(o);
      bsFree(o->opaqueRep);
    } else {
      /* should still be references... */
      assert(o->opaqueRep->refcnt > 0 || o->opaqueRep->linkcnt > 0);
    }
    o->opaqueRep = NULL; /* even if we did not free it, we
			    gave up our reference to it */
    o->opaqueLink = 0;
  }

  /* invalidate the entire cache */
  bsObjClearCache(o);
}

void bsObjSetString(BSObject *o, char *str, int len, int type) {
  assert(o != NULL);
  bsObjInvalidate(o,BS_T_STRING);
  bsStringClear(&(o->stringRep));
  bsStringSet(&(o->stringRep),str,len,type);
  o->repValid |= BS_T_STRING;
}

void bsObjSetList(BSObject *o, int objc, BSObject *objv[]) {
  int i;
  assert(o != NULL);
  bsObjInvalidate(o,BS_T_LIST);
  bsListClear(&(o->listRep));
  for(i = 0; i < objc; i++)
    bsListPush(&(o->listRep),objv[i],BS_TAIL);
  o->repValid |= BS_T_LIST;
}

void bsObjSetInt(BSObject *o, int i) {
  assert(o != NULL);
  bsObjInvalidate(o,BS_T_INT);
  o->intRep = i;
  o->repValid |= BS_T_INT;
}

void bsObjSetFloat(BSObject *o, float f) {
  assert(o != NULL);
  bsObjInvalidate(o,BS_T_FLOAT);
  o->floatRep = f;
  o->repValid |= BS_T_FLOAT;
}

void bsObjSetOpaque(BSObject *o, BSOpaque *p) {
  assert(o != NULL);
  assert(p != NULL);
  bsObjInvalidate(o,BS_T_OPAQUE);
  assert(p->refcnt >= 0);
  o->opaqueRep = p;
  p->refcnt++;
  o->repValid |= BS_T_OPAQUE;
}

void bsObjSetCopy(BSObject *o, BSObject *orig) {
  assert(o != NULL);
  assert(orig != NULL);
  bsObjInvalidate(o,0); /* invalidate any existing representations */

  /* copy all valid representations */
  if (orig->repValid & BS_T_STRING) {
    bsStringCopy(&(o->stringRep),&(orig->stringRep));
    o->repValid |= BS_T_STRING;
  }

  if (orig->repValid & BS_T_INT) {
    o->intRep = orig->intRep;
    o->repValid |= BS_T_INT;
  }
  
  if (orig->repValid & BS_T_FLOAT) {
    o->floatRep = orig->floatRep;
    o->repValid |= BS_T_FLOAT;
  }

  /* list has been invalidated, so it should be clear */
  if (orig->repValid & BS_T_LIST) {
    bsListAppend(&(o->listRep),&(orig->listRep));
    o->repValid |= BS_T_LIST;
  }

  if (orig->repValid & BS_T_OPAQUE) {
    assert(orig->opaqueRep != NULL);
    assert(orig->opaqueRep->refcnt >= 1); /* or it should not be in an object */
    orig->opaqueRep->refcnt++;
    o->opaqueRep = orig->opaqueRep;
    o->repValid |= BS_T_OPAQUE;
  }

  /* copy cache */
  {
    BSCachedRep *r, *rn;
    void *np;
    for (r = orig->cachedRep; r; r = r->next) {
      if (r->drv && r->drv->copyproc &&
	  (np = r->drv->copyproc(r->priv,r->drv->cdata))) {
	rn = bsAllocObj(BSCachedRep);
	rn->drv = r->drv;
	rn->priv = np;
	rn->next = o->cachedRep;
	o->cachedRep = rn;
      }
    }
  }
}

/* general rules for converting:
   - don't convert unless desired representation is not available
   - convert as following:
      - if you don't want a string, but a string is available,
        use the string
      - otherwise use opaque
      - otherwise use list
      - otherwise use float
      - otherwise use int
*/

#define FSTRSZ 128
#define ISTRSZ 128

char *bsObjGetStringPtr(BSObject *o) {
  BSString *s;
  s = bsObjGetString(o);
  return (s ? bsStringPtr(s) : "");
}

BSString *bsObjGetString(BSObject *o) {
  int done = 0;

  /* shortcut if the representation is available */
  if (o->repValid & BS_T_STRING)
    return &(o->stringRep);

  /*
   * To think about:  should opaque be a special case here?
   *    That is, should we really try other representations here
   *    if we cannot convert the opaque version?
   */
  if (!(o->repValid & BS_T_STRING) && (o->repValid & BS_T_OPAQUE)) {
    assert(o->opaqueRep != NULL);
    if (o->opaqueRep->driver != NULL &&
	o->opaqueRep->driver->makerep != NULL) {
      if (o->opaqueRep->driver->makerep(o,BS_T_STRING) == 0) {
	/* check for a lying driver...  */
	assert(o->repValid & BS_T_STRING);
      }
    }
  }

  if (!(o->repValid & BS_T_STRING) && (o->repValid & BS_T_LIST)) {
    BSObject *lo;
    BSString *ss;
    bsStringClear(&(o->stringRep));
    lo = o->listRep.head;
    if (lo) {
      ss = bsObjGetString(lo);
      if (lo->repValid & BS_T_LIST) {
	/* quote the list... */
	bsStringAppChr(&(o->stringRep),'{');
	bsStringAppend(&(o->stringRep),bsStringPtr(ss),bsStringLength(ss));
	bsStringAppChr(&(o->stringRep),'}');
      } else {
	/* Note: should this be Append or AppQuote...? */
	/* AppQuote means that conversion back may not be accurate... */
	bsStringAppend(&(o->stringRep),bsStringPtr(ss),bsStringLength(ss));
      }
      lo = lo->next;
      while (lo) {
	bsStringAppChr(&(o->stringRep),' ');
	ss = bsObjGetString(lo);
	if (lo->repValid & BS_T_LIST) {
	  /* quote the list... */
	  bsStringAppChr(&(o->stringRep),'{');
	  bsStringAppend(&(o->stringRep),bsStringPtr(ss),bsStringLength(ss));
	  bsStringAppChr(&(o->stringRep),'}');
	} else {
	  /* See Append vs. AppQuote above... */
	  bsStringAppend(&(o->stringRep),bsStringPtr(ss),bsStringLength(ss));
	}
	lo = lo->next;
      }
    }
    o->repValid |= BS_T_STRING;
  }

  if (!(o->repValid & BS_T_STRING) && (o->repValid & BS_T_FLOAT)) {
    char fstr[FSTRSZ];
    snprintf(fstr,FSTRSZ,"%g",o->floatRep);
    bsStringSet(&(o->stringRep),fstr,-1,BS_S_VOLATILE);
    o->repValid |= BS_T_STRING;
  }

  if (!(o->repValid & BS_T_STRING) && (o->repValid & BS_T_INT)) {
    char istr[ISTRSZ];
    snprintf(istr,FSTRSZ,"%d",o->intRep);
    bsStringSet(&(o->stringRep),istr,-1,BS_S_VOLATILE);
    o->repValid |= BS_T_STRING;
  }

  if (!(o->repValid & BS_T_STRING)) {
    /* null object == empty string */
    bsStringClear(&(o->stringRep));
    o->repValid |= BS_T_STRING;
  }
  
  /* converting to a string should *never* fail */
  assert(o->repValid & BS_T_STRING);
  
  return &(o->stringRep);
}

BSList *bsObjGetList(BSInterp *i, BSObject *o) {
  BSParseSource sp;
  BSString *s;

  /* shortcut if the representation is available */
  if (o->repValid & BS_T_LIST)
    return (&(o->listRep));

  /* force conversion to string first... */
  s = bsObjGetString(o);
  assert(s != NULL);

  bsListClear(&(o->listRep));
  if (bsParseList(i,bsStringSource(&sp,bsStringPtr(s)),
		  &(o->listRep)))
    return NULL; /* parse-list sets error */
  o->repValid |= BS_T_LIST;

  return &(o->listRep);
}

int bsObjGetInt(BSInterp *ip, BSObject *o, BSInt *i) {
  BSString *s;
  char *chk;
  long l;

  /* shortcut if the representation is available */
  if (o->repValid & BS_T_INT) {
    if (i)
      *i = o->intRep;
    return BS_OK;
  }
  
  /* force conversion to string first... */
  s = bsObjGetString(o);
  assert(s != NULL);

  chk = bsStringPtr(s);
  if (*chk == '\0') {
    if (ip)
      bsSetStringResult(ip,"cannot convert blank string to integer",
			BS_S_STATIC);
    return BS_ERROR;
  }
    
  l = strtol(chk,&chk,0);
  if (*chk != '\0') {
    if (ip)
      bsSetStringResult(ip,"failed to convert string to integer",
			BS_S_STATIC);
    return BS_ERROR;
  }
  
  o->intRep = l;
  o->repValid |= BS_T_INT;
  if (i)
    *i = (BSInt)l;
  return BS_OK;
}

int bsObjGetFloat(BSInterp *ip, BSObject *o, BSFloat *f) {
  BSString *s;
  char *chk;
  double d;

  /* shortcut if the representation is available */
  if (o->repValid & BS_T_FLOAT) {
    if (f)
      *f = o->floatRep;
    return BS_OK;
  }
  
  /* force conversion to string first... */
  s = bsObjGetString(o);
  assert(s != NULL);

  chk = bsStringPtr(s);
  if (*chk == '\0') {
    if (ip)
      bsSetStringResult(ip,"cannot convert blank string to float",
			BS_S_STATIC);
    return BS_ERROR;
  }
    
  d = strtod(chk,&chk);
  if (*chk != '\0') {
    if (ip)
      bsSetStringResult(ip,"failed to convert string to float",
			BS_S_STATIC);
    return BS_ERROR;
  }
  
  o->floatRep = (float)d;
  o->repValid |= BS_T_FLOAT;
  if (f)
    *f = o->floatRep;
  return BS_OK;
}

void bsObjAddCache(BSObject *o, BSCacheDriver *drv, void *priv) {
  BSCachedRep *r;
  if (!o)
    BS_FATAL("bsObjAddCache: object is null");

  /* look for existing slot */
  if (drv) {
    for (r = o->cachedRep; r; r = r->next) {
      if (r->drv && drv && r->drv->id == drv->id) {
	if (r->drv->freeproc)
	  r->drv->freeproc(r->priv,r->drv->cdata);
	r->priv = priv;
	return;
      }
    }
  }
  
  /* create new slot */
  r = bsAllocObj(BSCachedRep);
  r->drv = drv;
  r->priv = priv;
  r->next = o->cachedRep;
  o->cachedRep = r;
}

int bsObjGetCache(BSObject *o, BSId id, void **priv_r) {
  BSCachedRep *r;
  if (!o)
    return -1; /* nope */
  for (r = o->cachedRep; r; r = r->next) {
    if (r->drv && r->drv->id == id) {
      if (priv_r)
	*priv_r = r->priv;
      return 0;
    }
  }
  return 1;
}

void bsObjDelCache(BSObject *o, BSId id) {
  BSCachedRep *r, *rp;
  if (!o)
    return; /* nope */
  for (r = o->cachedRep, rp = NULL; 
       r; 
       rp = r, r = r->next) {
    if (r->drv && r->drv->id == id) {
      if (rp)
	rp->next = r->next;
      else
	o->cachedRep = r->next;
      r->next = NULL;
      if (r->drv && r->drv->freeproc)
	r->drv->freeproc(r->priv,r->drv->cdata);
      bsFree(r);
    }
  }
}

/*
  Garbage collection theory:
  - we distinguish between references and links
     - a reference is stored in a variable or other unattached object
     - a link is stored in an object associated with an opaque
  - references can never lead to cycles (one cannot take a "reference"
    of variable)
  - if an object is not opaque, then it is garbage iff (refcnt == 0 && linkcnt == 0)
  - an object for which (refcnt == 0 && linkcnt > 0) is considered volatile
     - the "volatile" state is only interesting for opaques
  - when an opaque ("x") enters a "volatile" state we need to check to see
  if "x" is also _isolated_ (def'n: "x" is unreachable from any variable)
    (i)  pass 1:  search all reachable objects from "x", count the
                  number of their parents that are reachable from "x"
		  (O(e) where e is the number of edges in "x"'s dependency
		   graph)
    (ii) pass 2:  consider only nodes that are in "x"'s reachable set and
                  are part of a cycle "x" -> "x" - if, for all such nodes ("y")
		  the number of parents reachable from "x" is equiv to the
		  number of links to "y" and "y" is volatile then "x" is isolated
		  
  if "x" is isolated then "x" is garbage (i.e. "x" is part of isolated cycle)
*/

/* if this is an opaque object, convert it from a regular variable
   reference to an object link */
int bsObjMkRefLink(BSObject *o) {
  if (!o)
    return -1; /* invalid object */
  if (!(o->repValid & BS_T_OPAQUE))
    return -1; /* not opaque */
  assert(o->opaqueRep != NULL);
  if (o->opaqueLink)
    return -1; /* already a link */
  assert(o->opaqueRep->refcnt > 0); /* or you're bad */
  assert(o->opaqueRep->linkcnt >= 0); /* or something's screwed up */
  o->opaqueRep->refcnt--;
  o->opaqueRep->linkcnt++; /* hey - that was easy... */
  o->opaqueLink = 1;
  return 0;
}

/* 
   for the crumb trail:
   0  == unvisited,
   >0 == reachable count
*/

#define volatile(p) ((p)->refcnt == 0)

/* pass 1: walk tree and calculate reachability */
static void calc_reachable(BSOpaque *root, BSObject *n) {
  BSObject *o;
  BSOpaque *p;
  if (!n || !(n->repValid & BS_T_OPAQUE) || !(n->opaqueRep))
    return; /* nothing to do */
  p = n->opaqueRep;
  if (p->crumb > 0 || p == root) {
    p->crumb++;
    return; /* already visited */
  }
  p->crumb++;
  /* visit children */
  for (o = p->links.head; o; o = o->next)
    calc_reachable(root,o);
}

/* pass 2: check all cycles back to root, clean-up crumb trail as we go */
/* return: 1 if all possible non-backtracking paths from "n" to "root"
   have all links accounted for or if this is a leaf (trivial), 
   0 otherwise */
static int check_root_cycles(BSOpaque *root, BSObject *n) {
  BSObject *o;
  int res;
  if (!n || !(n->repValid & BS_T_OPAQUE) || !(n->opaqueRep) ||
      (n->opaqueRep->crumb == 0))
    return 1; /* leaf - or we've cleaned here already - or
	         we've never been here at all */
  /* since we are also cleaning our crumb trail, we need to 
     revisit all children no matter what, so do that first */
  res = 1;
  /* visit children */
  for (o = n->opaqueRep->links.head; o; o = o->next) {
    if (!check_root_cycles(root,o))
      res = 0; /* there's a path on which all links are not
		  accounted for */
  }
  /* now check me... */
  if (n->opaqueRep->crumb != n->opaqueRep->linkcnt ||
      n->opaqueRep->refcnt > 0)
    res = 0; /* I am not accounted for or I am volatile */
  /* clean up crumbs */
  n->opaqueRep->crumb = 0;
  return res;
}

/* Garbage collection for opaque objects (experimental) */
/* This function assumes that you have already removed any 
   known links to this object */
int bsObjIsGarbage(BSObject *o) {
  BSObject *c; /* child */
  BSOpaque *p;
  int garbage;

  if (!o)
    return 0; /* not garbage */
  if (!(o->repValid & BS_T_OPAQUE))
    return 1; /* not opaque - automatically garbage */
  p = o->opaqueRep;
  assert(p != NULL); /* ... or BS_T_OPAQUE should not be set */
  assert(p->refcnt >= 0);
  assert(p->linkcnt >= 0);
  if (p->refcnt > 0)
    return 0; /* not garbage - still reference counts */
  if (p->linkcnt == 0)
    return 1; /* no refcnt, no linkcnt - ditch it */
  /* okay - refcnt == 0, and linkcnt > 0, this means that no
     variables hold a reference to it, but that 1 or more opaques
     hold a link to it. */
  /* This may be the "final straw" that breaks our real linkage, so
     we need to search */

  /* calculate reachability */
  for (c = p->links.head; c; c = c->next)
    calc_reachable(p,c);

  garbage = 1;
  for (c = p->links.head; c; c = c->next)
    if (!check_root_cycles(p,c))
      garbage = 0;

  /* check/clean root */
  if (p->crumb != p->linkcnt)
    garbage = 0;
  p->crumb = 0;

  return garbage;
}

int bsOpaqueLink(BSOpaque *p, BSObject *o) {
  if (!o || !p)
    return -1;
  bsListPush(&(p->links),o,1);
  return 0;
}

int bsOpaqueUnlink(BSOpaque *p, BSObject *o) {
  if (!o || !p)
    return -1;
  if (o->prev == NULL) {
    assert(p->links.head == o);
    p->links.head = o->next;
  } else {
    o->prev->next = o->next;
  }
  if (o->next == NULL) {
    assert(p->links.tail == o);
    p->links.tail = o->prev;
  } else {
    o->next->prev = o->prev;
  }
  o->prev = o->next = NULL;
  return 0;
}

static void *constant_copyproc(void *priv, void *cdata) {
  return priv;
}

/* a simple cached type - converting a constant name to an index into 
   a table */
static BSCacheDriver constant_driver = {
  "constant",
  0,    /* id */
  NULL, /* freeproc */
  constant_copyproc, /* copyproc */
  NULL
};

BSCacheDriver *bsGetConstantDriver(void) {
  if (constant_driver.id == 0)
    constant_driver.id = bsUniqueId();
  return &constant_driver;
}

/* assume that the first element of the structure(s) pointed to by 
   table_v is the string */
int bsObjGetConstant(BSInterp *i, BSObject *o, void *table_v, int offset,
		     char *msg) {
  BSCacheDriver *d;
  char **table = (char **)table_v;
  void *v;
  
  d = bsGetConstantDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  if (bsObjGetCache(o,d->id,&v)) {
    char *s;
    int k = 0;
    /* look it up */
    s = bsObjGetStringPtr(o);
    while (*table != NULL) {
      if (strcmp(*table,s) == 0)
	break;
      k++;
      table = (char **)((char *)table + offset);
    }
    if (!*table) {
      bsClearResult(i);
      bsAppendResult(i,"unknown value for '",msg ? msg : "argument",
		     "': ",s,NULL);
      return -1;
    }
    v = (void *)k;
    if (!(i->opt & BS_OPT_MEMORY))
      bsObjAddCache(o,d,v);
  }
  
  return (int)v;
}
