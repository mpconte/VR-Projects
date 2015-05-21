#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bluescript.h>

/* my hash function */
static unsigned hashString(char *s) {
  char *e;
  unsigned h = 0;
  int i, nhash;

  nhash = strlen(s);
  e = s+nhash;
  for(i = 0; i < nhash; i++)
    h = ((h<<1)%(2*BS_HASHSIZE)) + (unsigned)(*e--) + (unsigned)(*s++);
  return (h % (2*BS_HASHSIZE))>>1;
}

#undef streq
#define streq(a,b) (*(a) == *(b) ? (strcmp(a,b) == 0 ? 1 : 0) : 0)

static BSHashList *lookup(BSHash *h, char *key) {
  BSHashList *l;
  unsigned k;
  if (!key)
    return NULL;
  k = hashString(key);
  assert(k< BS_HASHSIZE);
  for (l = h->hash[k]; l; l = l->next)
    if (streq(l->key,key))
      return l;
  return NULL;
}

BSHash *bsHashCreate(void) {
  return bsAllocObj(BSHash);
}

void bsHashClear(BSHash *h, void (*freeproc)(void *)) {
  BSHashList *l, *lt;
  unsigned k;
  if (!h)
    return;
  for (k = 0; k < BS_HASHSIZE; k++) {
    l = h->hash[k];
    while (l) {
      lt = l->next;
      if (freeproc)
	freeproc(l->obj);
      bsFree(l->key);
      bsFree(l);
      l = lt;
    }
    h->hash[k] = NULL;
  }
}

void bsHashDestroy(BSHash *h, void (*freeproc)(void *)) {
  if (!h)
    return;
  bsHashClear(h,freeproc);
  bsFree(h);
}

int bsHashExists(BSHash *h, char *key) {
  if (!h)
    return 0; /* does not exist */
  return (lookup(h,key) != NULL ? 1 : 0);
}

void *bsHashLookup(BSHash *h, char *key) {
  BSHashList *l;
  if (!h)
    return NULL; /* cannot lookup */
  l = lookup(h,key);
  if (!l)
    return NULL;
  return l->obj;
}

int bsHashInsert(BSHash *h, char *key, void *obj) {
  BSHashList *l;
  unsigned k;
  if (!key)
    return -1;
  k = hashString(key);
  assert(k < BS_HASHSIZE);
  for (l = h->hash[k]; l; l = l->next) {
    if (streq(l->key,key)) {
      l->obj = obj;
      return 0;
    }
  }
  /* create new entry */
  l = bsAllocObj(BSHashList);
  l->key = bsStrdup(key);
  l->obj = obj;
  l->next = h->hash[k];
  h->hash[k] = l;
  return 0;
}

void bsHashDelete(BSHash *h, char *key) {
  BSHashList *l, *lp;
  unsigned k;
  if (!key)
    return;
  k = hashString(key);
  assert(k < BS_HASHSIZE);
  for (l = h->hash[k], lp = NULL; l; lp = l, l = l->next) {
    if (streq(l->key,key)) {
      if (lp)
	lp->next = l->next;
      else
	h->hash[k] = l->next;
      bsFree(l->key);
      bsFree(l);
    }
  }
}

int bsHashWalk(BSHashWalk *w, BSHash *h) {
  if (!w)
    return -1;
  if (h)
    w->hash = h;
  if (!w->hash)
    return -1;
  w->start = 1;
  w->i = 0;
  w->key = NULL;
  w->obj = NULL;
  w->next = NULL;
  return 0;
}

int bsHashNext(BSHashWalk *w) {
  if (!w || !w->hash)
    return -1;
  if (w->start) {
    /* find first non-NULL item */
    w->start = 0;
    for (w->i = 0; w->i < BS_HASHSIZE && w->hash->hash[w->i] == NULL; (w->i)++)
      ;
    if (w->i >= BS_HASHSIZE)
      w->next = NULL;
    else
      w->next = w->hash->hash[w->i];
  }
	
  if (!w->next)
    return 1; /* end of walk */
  w->obj = w->next->obj;
  w->key = w->next->key;
  if (w->next->next)
    w->next = w->next->next;
  else {
    (w->i)++;
    while (w->i < BS_HASHSIZE && w->hash->hash[w->i] == NULL)
      (w->i)++;
    if (w->i >= BS_HASHSIZE)
      w->next = NULL;
    else
      w->next = w->hash->hash[w->i];
  }
  return 0;
}
