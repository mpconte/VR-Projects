#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "ve_alloc.h"
#include "ve_debug.h"
#include "ve_util.h"
#include "ve_thread.h"

static char *ve_debug_prefix = NULL;
static int ve_debug_level = 0;  /* now the global debug level */
static int ve_debug_enabled = 0;  /* is any level turned on? */
static VeThrMutex *debug_mutex = NULL;
static char *ve_debug_module = NULL;

#define HASHSZ 31
static struct ve_debug_modlev {
  struct ve_debug_modlev *next;
  char *name;
  int level;
} *modinfo[HASHSZ];

/* borrowed from Oz's hashing code (which is public domain) */
#define HASHV 65587
#define HASHC (n = *s++ + HASHV * n)
static int hash(char *s) {
  unsigned n = 0;
  while (*s)
    HASHC;
  return (n%HASHSZ);
}

int veDebugEnabled(void) {
  return ve_debug_enabled;
}

void veSetDebug2Level(char *m, int i) {
  struct ve_debug_modlev *l;
  int h = hash(m);
  for(l = modinfo[h]; l; l = l->next) {
    if (strcmp(l->name,m) == 0) {
      l->level = i;
      if (i > 0)
	ve_debug_enabled = 1;
      return;
    }
  }
  /* new level */
  l = veAllocObj(struct ve_debug_modlev);
  l->name = veDupString(m);
  l->level = i;
  l->next = modinfo[h];
  modinfo[h] = l;
  if (i > 0)
    ve_debug_enabled = 1;
}

static void veSetDebugStrUnit(char *s) {
  char *x;
  unsigned l;
  if (!(x = strchr(s,'='))) {
    if (sscanf(s,"%u",&l) == 1)
      veSetDebugLevel(l);
    return;
  } else {
    if (sscanf(x+1,"%u",&l) == 1) {
      if (x == s) /* no name - global */
	veSetDebugLevel(l);
      else {
	char *z;
	z = veAlloc(x-s+1,0);
	strncpy(z,s,x-s);
	z[x-s] = '\0';
	veSetDebug2Level(z,l);
	veFree(z);
      }
    }
  }
}

static char *last_debug_str = NULL;

void veSetDebugStr(char *s) {
  char *copy, *x;
  last_debug_str = veDupString(s);
  copy = veDupString(s);
  x = strtok(copy,", \t\r\n");
  while (x != NULL) {
    veSetDebugStrUnit(x);
    x = strtok(NULL,", \t\r\n");
  }
  veFree(copy);
}

/* this is obviously bogus and needs to be fixed... */
char *veGetDebugStr(void) {
  return last_debug_str ? last_debug_str : "";
}

int veGetDebug2Level(char *m) {
  struct ve_debug_modlev *l;
  int h = hash(m);
  if (!ve_debug_enabled)
    return 0;
  for(l = modinfo[h]; l; l = l->next) {
    if (strcmp(l->name,m) == 0) 
      return l->level;
  }
  return ve_debug_level;
}

void veSetDebugModule(char *m) {
  ve_debug_module = m;
}

void veVDebug(int level, char *fmt, va_list ap) {
  /* race condition */
  if (!debug_mutex)
    debug_mutex = veThrMutexCreate();

  /* MutexLock can generate debug messages... */
  veThrMutexQuietLock(debug_mutex);
  if (level >= 0)
    fprintf(stderr,"(%d) ",level);
  if (ve_debug_prefix)
    fprintf(stderr,"%s ",ve_debug_prefix);
  fprintf(stderr,"[%8x] ",(unsigned)veThreadId());
  if (ve_debug_module)
    fprintf(stderr, "module %s: ",ve_debug_module);
  fprintf(stderr, "debug: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  fflush(stderr);
  veThrMutexQuietUnlock(debug_mutex);
}

void veDebug(char *fmt, ...) {
  va_list ap;
  va_start(ap,fmt);
  veVDebug(-1,fmt,ap);
  va_end(ap);
}

void veDebug2(int level, char *fmt, ...) {
  va_list ap;
  va_start(ap,fmt);
  veVDebug(level,fmt,ap);
  va_end(ap);
}

void veSetDebugLevel(int i) {
  ve_debug_level = i;
  if (i > 0)
    ve_debug_enabled = 1;
}

int veGetDebugLevel(void) {
  return ve_debug_level;
}

void veSetDebugPrefix(char *txt) {
  if (ve_debug_prefix) {
    veFree(ve_debug_prefix);
    ve_debug_prefix = NULL;
  }
  if (txt)
    ve_debug_prefix = veDupString(txt);
}
