#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE "ve_alloc"

void *veAlloc(unsigned nbytes, int zero) {
  void *v;
  if (nbytes == 0)
    veFatalError(MODULE,"veAlloc - attempt to allocate zero bytes");
  if (!(v = malloc(nbytes)))
    veFatalError(MODULE,"veAlloc - failed to allocated %u bytes",nbytes);
  if (zero)
    memset(v,0,nbytes);
  return v;
}

void *veRealloc(void *ptr, unsigned nbytes) {
  void *ptr2;
  if (nbytes == 0)
    veFatalError(MODULE,"veRealloc - attempt to reallocate to zero bytes");
  if (!(ptr2 = realloc(ptr,nbytes)))
    veFatalError(MODULE,"veRealloc - failed to reallocate 0x%p to %u bytes",
		 ptr,nbytes);
  return ptr2;
}

void veFree(void *ptr) {
  if (ptr)
    free(ptr);
}

char *veDupString(char *s) {
  char *s2;
  if (!s)
    return NULL;
  s2 = veAlloc(strlen(s)+1,0);
  strcpy(s2,s);
  return s2;
}
