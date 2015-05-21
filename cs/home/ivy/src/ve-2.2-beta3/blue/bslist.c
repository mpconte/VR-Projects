#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <bluescript.h>

void bsListRecache(BSList *l) {
  /* assume objc is correct */
  BSObject *o;
  int k;
  if (!l)
    return;
#if BS_SMALL_LIST_SZ >= 2
  for (k = 0, o = l->head; 
       k < BS_SMALL_LIST_SZ-1 && k < l->objc && o; 
       k++, o = o->next) {
    l->objv[k] = o;
  }
  l->objv[k] = NULL; /* terminate list */
  if (k < BS_SMALL_LIST_SZ-1 && k < l->objc) {
    BS_WARN("bsListRecache correcting invalid objc");
    l->objc = k;
  }
#endif
}

void bsListRecalc(BSList *l) {
  BSObject *o;
  int n = 0;
  if (!l)
    return;
  o = l->head;
  while (o) {
    n++;
    o = o->next;
  }
  l->objc = n;
#if BS_SMALL_LIST_SZ >= 2
  bsListRecache(l);
#endif
}

void bsListPush(BSList *l, BSObject *o, int headtail) {
  if (!l || !o)
    return;

  if (!l->head) {
    /* empty list case */
    assert(l->tail == NULL);
    assert(l->objc == 0);
    l->head = l->tail = o;
    o->prev = o->next = NULL;
    l->objc = 1;
#if BS_SMALL_LIST_SZ >= 2
    l->objv[0] = o;
    l->objv[1] = NULL;
#endif
    return;
  }
  /* otherwise, list is not empty */
  assert(l->head != NULL && l->tail != NULL);
  switch (headtail) {
  case BS_HEAD:
    /* add to head */
    o->prev = NULL;
    o->next = l->head;
    l->head->prev = o;
    l->head = o;
    l->objc++;
    /* rebuilding list is enough work that we might as well
       recache */
    bsListRecache(l);
    break;
  case BS_TAIL:
    o->next = NULL;
    o->prev = l->tail;
    l->tail->next = o;
    l->tail = o;
#if BS_SMALL_LIST_SZ >= 2
    if (l->objc < BS_SMALL_LIST_SZ-1) {
      l->objv[l->objc] = o;
      l->objv[l->objc+1] = NULL;
    }
#endif
    l->objc++;
    break;
  default:
    BS_FATAL("bsListPush called with invalid headtail");
  }
}

BSObject *bsListPop(BSList *l, int headtail) {
  BSObject *o = NULL;
  if (!l)
    return NULL;
  if (!l->head) {
    assert(l->tail == NULL);
    assert(l->objc == 0);
    return NULL;
  }
  switch (headtail) {
  case BS_HEAD:
    o = l->head;
    if (l->head->next) {
      assert(l->objc > 1);
      l->head->next->prev = NULL;
      l->head = l->head->next;
    } else {
      assert(l->objc == 1);
      l->head = NULL;
      l->tail = NULL;
    }
    o->next = NULL;
    o->prev = NULL;
    l->objc--;
    bsListRecache(l);
    break;

  case BS_TAIL:
    o = l->tail;
    if (l->tail->prev) {
      assert(l->objc > 1);
      l->tail->prev->next = NULL;
      l->tail = l->tail->prev;
    } else {
      assert(l->objc == 1);
      l->head = NULL;
      l->tail = NULL;
    }
    o->next = NULL;
    o->prev = NULL;
    l->objc--;
#if BS_SMALL_LIST_SZ >= 2
    if (l->objc < BS_SMALL_LIST_SZ)
      l->objv[l->objc] = NULL;
#endif
    break;
  }
  return o;
}

void bsListClear(BSList *l) {
  BSObject *o, *ot;
  o = l->head;
  while (o) {
    ot = o->next;
    o->next = o->prev = NULL;
    bsObjDelete(o);
    o = ot;
  }
  l->head = l->tail = NULL;
  l->objc = 0;
  l->objv[0] = NULL;
}

void bsListInit(BSList *l) {
  l->head = l->tail = NULL;
  l->objc = 0;
  l->objv[0] = NULL;
#if BS_SMALL_LIST_SZ >= 2
  l->objv[BS_SMALL_LIST_SZ-1] = NULL;
#endif /* BS_SMALL_LIST_SZ */
}

int bsListSize(BSList *l) {
  if (!l)
    return 0;
  return l->objc;
}

void bsListAppend(BSList *l2, BSList *l) {
  BSObject *o;
  for (o = l->head; o; o = o->next)
    bsListPush(l2,bsObjCopy(o),BS_TAIL);
}

BSObject *bsListIndex(BSList *l, int i) {
  BSObject *o;
  if (!l || i < 0 || i >= l->objc)
    return NULL; /* index is out of range */
#if BS_SMALL_LIST_SZ >= 2
  if (i < BS_SMALL_LIST_SZ-1)
    return l->objv[i];
  /* this must mean that i belongs to an element
     outside of the small list rep which means that
     the list is longer than the small list rep */
  assert(l->objc > BS_SMALL_LIST_SZ-1);
  o = l->objv[BS_SMALL_LIST_SZ-2];
  i -= BS_SMALL_LIST_SZ-2;
#else
  o = l->head;
#endif /* BS_SMALL_LIST_SZ */
  while (o && i > 0) {
    o = o->next;
    i--;
  }
  if (!o)
    BS_WARN("bsListIndex received list with incorrect objc");
  return o;
}
