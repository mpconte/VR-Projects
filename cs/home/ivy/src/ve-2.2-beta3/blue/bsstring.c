#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluescript.h"

/* a string can only be in STATIC or
   DYNAMIC state */

void bsStringInit(BSString *s) {
  s->type = BS_S_STATIC;
  s->buf = NULL;
  s->spc = 0;
  s->use = 0;
}

void bsStringClear(BSString *s) {
  if (s->type == BS_S_STATIC) {
    s->buf = NULL;
    s->spc = 0;
    s->use = 0;
  } else {
    /* keep space allocated (if any) */
    if (s->buf)
      s->buf[0] = '\0';
    s->use = 0;
  }
}

#define BASEBUF 32

/* convert string to dynamic if not so */
static void mkdynamic(BSString *s) {
  if (s->type == BS_S_STATIC) {
    char *sv = s->buf;
    int msz = s->use;
    assert(msz >= 0);
    if (msz > 0) {
      assert(sv != NULL);
    }
    s->spc = msz + BASEBUF;
    s->buf = bsAlloc(s->spc,0);
    if (msz > 0)
      memcpy(s->buf,sv,msz);
    s->buf[msz] = '\0'; /* null terminate */
    s->type = BS_S_DYNAMIC; 
  }
}

/* make sure that we can safely append up to 'n' chars
   to the existing string */
static void grow(BSString *s, int n) {
  if (s->type != BS_S_DYNAMIC)
    mkdynamic(s);
  if ((s->spc-s->use-1) < n) {
    while ((s->spc-s->use-1) < n)
      s->spc *= 2;
    s->buf = bsRealloc(s->buf,s->spc);
  }
}

void bsStringSet(BSString *s, char *str, int sz, int type) {
  int trunc = 0;

  if (!str) {
    /* override */
    type = BS_S_STATIC;
    sz = 0;
  } else if (sz < 0)
    sz = strlen(str);
  
  if (str) {
    int n = strlen(str);
    if (sz > n)
	sz = n;
    else if (sz < n) {
      trunc = 1; /* need to truncate string */
      if (type == BS_S_STATIC)
	type = BS_S_VOLATILE; /* override */
    }
  }

  switch (type) {
  case BS_S_STATIC:
    bsStringFreeSpace(s);
    s->type = BS_S_STATIC;
    s->spc = 0;
    s->use = sz;
    s->buf = str;
    break;

  case BS_S_DYNAMIC:
    if (s->type == BS_S_STATIC) {
      /* reuse given buffer */
      s->spc = sz+1;
      s->buf = str;
      s->buf[sz] = '\0';
      s->type = BS_S_DYNAMIC;
      s->use = sz;
    } else {
      /* keep larger of the two buffers... */
      if (sz+1 < s->spc) {
	/* keep mine */
	assert(s->buf != NULL);
	memcpy(s->buf,str,sz);
	s->buf[sz] = '\0';
	s->use = sz;
	bsFree(str); /* since it's dynamic... */
      } else {
	/* take new one */
	bsFree(s->buf);
	s->buf = str;
	s->buf[sz] = '\0';
	s->spc = sz+1;
	s->use = sz;
      }
    }
    break;

  case BS_S_VOLATILE:
    bsStringClear(s);
    mkdynamic(s);
    if (s->spc < sz+1)
      grow(s,sz+1);
    memcpy(s->buf,str,sz);
    s->buf[sz] = '\0';
    s->use = sz;
    break;

  default:
    BS_WARN("bsStringSet called with invalid string type");
  }
}

void bsStringAppend(BSString *s, char *str, int sz) {
  if (!s || !str)
    return; /* nothing to do */
  if (sz < 0)
    sz = strlen(str);
  if (s->type != BS_S_DYNAMIC)
    mkdynamic(s);
  if (s->use+sz+1 >= s->spc)
    grow(s,sz);
  memcpy(s->buf+s->use,str,sz);
  s->use += sz;
  s->buf[s->use] = '\0';
}


void bsStringAppQuote(BSString *s, char *str, int sz) {
  char *p;
  int extra = 0;
  if (!s || !str)
    return; /* nothing to do */
  if (sz < 0)
    sz = strlen(str);
  if (s->type != BS_S_DYNAMIC)
    mkdynamic(s);

  for (p = str; *p; p++) {
    if (BS_C_ISSPECIAL(*p))
      extra++;
  }

  if (s->use+sz+extra+1 >= s->spc)
    grow(s,sz+extra);

  p = s->buf+s->use;
  if (extra == 0) {
    memcpy(s->buf+s->use,str,sz);
    s->use += sz;
  } else {
    while (sz > 0) {
      if (BS_C_ISSPECIAL(*str))
	*(p++) = BS_C_ESCAPE;
      *(p++) = *(str++);
      sz--;
    }
    s->use += sz + extra;
  }
  s->buf[s->use] = '\0';
}

void bsStringAppChr(BSString *s, int c) {
  if (!s)
    return;
  if (s->type != BS_S_DYNAMIC)
    mkdynamic(s);
  if (s->use+1 >= s->spc)
    grow(s,1);
  s->buf[s->use++] = c;
  s->buf[s->use] = '\0';
}

int bsStringLength(BSString *s) {
  return s->use;
}

void bsStringFreeSpace(BSString *s) {
  if (s->type == BS_S_STATIC) {
    s->buf = NULL;
    s->spc = 0;
    s->use = 0;
  } else {
    /* free space allocated (if any) */
    if (s->buf) {
      free(s->buf);
      s->buf = NULL;
      s->spc = 0;
    }
    s->use = 0;
  }
}

void bsStringCopy(BSString *s, BSString *orig) {
  if (!s || !orig)
    return;
  if (orig->type == BS_S_STATIC) {
    bsStringFreeSpace(s);
    memcpy(s,orig,sizeof(BSString));
  } else {
    /* dynamic */
    bsStringClear(s);
    mkdynamic(s);
    if (orig->use+1 >= s->spc)
      grow(s,orig->use+1);
    memcpy(s->buf,orig->buf,orig->use);
    s->use = orig->use;
    s->buf[s->use] = '\0';
  }
}
