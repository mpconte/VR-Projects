#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void (*oom_cback)(void) = (void (*)(void))NULL;

void bsOomCallback(void (*cback)(void)) {
  oom_cback = cback;
}

void *bsAlloc(int bytes, int zero) {
  void *v;
  if (bytes <= 0)
    return NULL;
  v = (zero ? calloc(1,bytes) : malloc(bytes));
  if (!v) {
    if (oom_cback) {
      oom_cback();
      /* if control returns from callback, assume that it is okay
	 to continue with a NULL pointer... */
      return NULL;
    } else {
      fprintf(stderr,"out of memory - allocation failure\n");
      abort();
    }
  }
  return v;
}

void *bsRealloc(void *v, int bytes) {
  void *v2;
  if (!(v2 = realloc(v,bytes))) {
    if (oom_cback) {
      oom_cback();
      return NULL;
    } else {
      fprintf(stderr,"out of memory - allocation failure\n");
      abort();
    }
  }
  return v2;
}

void bsFree(void *v) {
  if (v)
    free(v);
}

char *bsStrdup(char *s) {
  char *s2;
  if (!s)
    return NULL;
  if (!(s2 = bsAlloc(strlen(s)+1,0)))
    return NULL;
  strcpy(s2,s);
  return s2;
}
