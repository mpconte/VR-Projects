#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef HAS_POSIXSELECT
#include <sys/select.h>
#endif
#ifdef HAS_POSIXPOLL
#include <sys/poll.h>
#endif
#include "ve_alloc.h"
#include "ve_debug.h"
#include "ve_util.h"

#define MODULE "ve_util"

int veIsFile(char *path) {
  struct stat st;
  if (stat(path,&st))
    return 0;
  return (S_ISREG(st.st_mode));
}

int veIsDir(char *path) {
  struct stat st;
  if (stat(path,&st))
    return 0;
  return (S_ISDIR(st.st_mode));
}

char *veSysError(void) {
  return strerror(errno);
}

void veVsnprintf(char *s, int n, char *format, va_list ap) {
#ifdef HAS_VSNPRINTF
  vsnprintf(s,n,format,ap);
#else
  vsprintf(s,format,ap);
#endif /* HAVE_VSNPRINTF */
}

void veSnprintf(char *s, int n, char *format, ...) {
  va_list ap;
  va_start(ap,format);
  veVsnprintf(s,n,format,ap);
  va_end(ap);
}

int veWaitForData(int fd) {
#if defined(HAS_POSIXSELECT)
  /* wait for data to become available */
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  /* should be more judicious about checking this... */
  select(fd+1,&fds,NULL,&fds,NULL);
  return 0;
#elif defined(HAS_POSIXPOLL)
  veFatalError(MODULE,"FIXME: POSIX poll() interface for veWaitForData");
#else
  /* fake this? */
  veFatalError(MODULE,"No veWaitForData() support on this platform");
#endif
}

int vePeekFd(int fd) {
#if defined(HAS_POSIXSELECT)
  /* do this with select */
  fd_set fds;
  int i;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  tv.tv_sec = tv.tv_usec = 0;
  if ((i = select(fd+1,&fds,NULL,NULL,&tv)))
    return 1;
  return 0;
#elif defined(HAS_POSIXPOLL)
  veFatalError(MODULE,"FIXME: POSIX poll() interface for vePeekFd");
#else
  veFatalError(MODULE, "No vePeekFd() support on this platform");
#endif /* __sun || __sgi || __linux */
}

/** 
 *
 *   VeStrMap/VeStrMapWalk implementation 
 *
 **/

/* Store strings in a hash-table */
#define HASHSIZE    64
#define HASHSAMPLE  16

typedef struct ve_strmap_list {
  char *str;
  void *obj;
  struct ve_strmap_list *next;
} VeStrMapList;

typedef struct ve_stringmaprec {
  VeStrMapList *hash[HASHSIZE];
} VeStrMapRec;

typedef struct ve_stringmapwalkrec {
  VeStrMap map;
  int start; /* flag - indicates to restart the walk */
  int i; /* the current hash index */
  char *str; /* current string - invalid if we erase... */
  void *obj; /* current object we are referencing... */
  VeStrMapList *next; /* the next record to be returned (NULL if none left) */
} VeStrMapWalkRec;

/* convert a string to a hash value */
/* consider - first N characters and last N characters
   (to avoid common prefix problems...) */
/* this hash gives a pretty good spread for both random strings and
   common systematic strings (e.g. "name20933") */
static unsigned hashString(char *s) {
  char *e;
  unsigned h = 0;
  int i, n, nhash;

  n = strlen(s);
  if (n < HASHSAMPLE)
    nhash = n;
  else
    nhash = HASHSAMPLE;
  e = s+n;
  for(i = 0; i < nhash; i++)
    h = ((h<<1)%(2*HASHSIZE)) + (float)(*e--) + (float)(*s++);
  return (h % (2*HASHSIZE))>>1;
}

VeStrMap veStrMapCreate(void) {
  return veAllocObj(VeStrMapRec);
}

void veStrMapDestroy(VeStrMap m, VeStrMapFreeProc freeval) {
  VeStrMapList *l, *tmp;
  int i;

  if (!m)
    return;

  for(i = 0; i < HASHSIZE; i++) {
    l = m->hash[i];
    while(l) {
      tmp = l->next;
      if (freeval && l->obj)
	freeval(l->obj);
      veFree(l->str);
      veFree(l);
      l = tmp;
    }
  }
  veFree(m);
}

void *veStrMapLookup(VeStrMap m, char *str) {
  VeStrMapList *l;
  int h = hashString(str);
  assert(h >= 0 && h < HASHSIZE);
  l = m->hash[h];
  while(l) {
    if (strcmp(l->str,str) == 0)
      return l->obj;
    l = l->next;
  }
  return NULL;
}

/* like veStrMapLookup, but we return a flag indicating whether
   or not the entry is actually there */
int veStrMapExists(VeStrMap m, char *str) {
  VeStrMapList *l;
  int h = hashString(str);
  assert(h >= 0 && h < HASHSIZE);
  l = m->hash[h];
  while(l) {
    if (strcmp(l->str,str) == 0)
      return 1;
    l = l->next;
  }
  return 0;
}

/* replaces if it already exists... */
int veStrMapInsert(VeStrMap m, char *str, void *obj) {
  VeStrMapList *l, *nl, *pre;
  int h = hashString(str);
  assert(h >= 0 && h < HASHSIZE);
  l = m->hash[h];
  pre = NULL;
  while(l) {
    if (strcmp(l->str,str) == 0) {
      l->obj = obj;
      return 0;
    }
    pre = l;
    l = l->next;
  }
  nl = veAllocObj(VeStrMapList);
  nl->str = veDupString(str);
  nl->obj = obj;
  nl->next = NULL;
  if (pre)
    pre->next = nl;
  else
    m->hash[h] = nl;
  return 0;
}

int veStrMapDelete(VeStrMap m, char *str) {
  VeStrMapList *l, *pre;
  int h = hashString(str);
  assert(h >= 0 && h < HASHSIZE);
  l = m->hash[h];
  pre = NULL;
  while(l) {
    if (strcmp(l->str,str) == 0) {
      if (pre)
	pre->next = l->next;
      else
	m->hash[h] = l->next;
      veFree(l->str);
      veFree(l);
    }
    pre = l;
    l = l->next;
  }
  return 0;
}

VeStrMapWalk veStrMapWalkCreate(VeStrMap m) {
  VeStrMapWalk w = veAllocObj(VeStrMapWalkRec);
  w->map = m;
  w->str = NULL;
  w->obj = NULL;
  w->next = NULL;
  w->i = HASHSIZE;
  return w;
}

void veStrMapWalkDestroy(VeStrMapWalk w) {
  veFree(w);
}

int veStrMapWalkFirst(VeStrMapWalk w) {
  if (!w || !w->map)
    return -1; /* failure */
  w->start = 1;
  return veStrMapWalkNext(w);
}

int veStrMapWalkNext(VeStrMapWalk w) {
  if (!w || !w->map)
    return -1;

  if (w->start) {
    /* find first non-NULL item */
    w->start = 0;
    for(w->i = 0; w->i < HASHSIZE && w->map->hash[w->i] == NULL; (w->i)++)
      ;
    if (w->i >= HASHSIZE)
      w->next = NULL;
    else 
      w->next = w->map->hash[w->i];
  }

  if (!w->next)
    return 1; /* end of walk */

  w->obj = w->next->obj;
  w->str = w->next->str;
  /* find next element */
  if (w->next->next)
    w->next = w->next->next; /* move along path */
  else {
    (w->i)++;
    while (w->i < HASHSIZE && w->map->hash[w->i] == NULL)
      (w->i)++;
    if (w->i >= HASHSIZE)
      w->next = NULL;
    else
      w->next = w->map->hash[w->i];
  }
  return 0;
}

void *veStrMapWalkObj(VeStrMapWalk w) {
  if (w)
    return w->obj;
  else
    return NULL;
}

char *veStrMapWalkStr(VeStrMapWalk w) {
  if (w)
    return w->str;
  else
    return NULL;
}

void veStrMapStats(VeStrMap m) {
  int i,n;
  int min = -1, max = -1, tot = 0, above = 0, below = 0;
  float avg;
  VeStrMapList *l;

  fprintf(stderr, "VeStrMap stats:\n");
  for(i = 0; i < HASHSIZE; i++) {
    for(l = m->hash[i], n = 0; l; n++, l = l->next)
      ;
    if (min == -1 || min > n)
      min = n;
    if (max == -1 || max < n)
      max = n;
    tot += n;
  }
  avg = tot/(float)HASHSIZE;
  for(i = 0; i < HASHSIZE; i++) {
    for(l = m->hash[i], n = 0; l; n++, l = l->next)
      ;
    if (n < avg)
      below++;
    else
      above++;
    fprintf(stderr,"hash[%d] = %d\n", i, n);
  }
  fprintf(stderr, "Total elements: %d\n", tot);
  fprintf(stderr, "Smallest hash list:  %d\n", min);
  fprintf(stderr, "Largest hash list:  %d\n", max);
  fprintf(stderr, "Avg hash list size: %.2f (%d - %d)\n", avg, below, above);
}

void veMicroSleep(int usecs) {
  /* right now, this code is UNIX-centric, but has been isolated so
     that it can be easily tweaked for other platforms */
#if defined(HAS_USLEEP)
  if (usecs >= 1000000) {
    sleep(usecs/1000000);
    usecs %= 1000000;
  }
  usleep(usecs);
#elif defined(HAS_POSIXSELECT)
  struct timeval tv;
  tv.tv_sec = usecs/1000000;
  tv.tv_usec = usecs%1000000;
  (void) select(0,NULL,NULL,NULL,&tv);
  return 0;
#elif defined(HAS_POSIXPOLL)
  veFatalError(MODULE,"FIXME: POSIX poll() interface for veMicroSleep");
#else
  veFatalError(MODULE,"No veMicroSleep() support on this platform");
#endif
}

int veIsPathAbsolute(char *path) {
  /* right now, this code is UNIX-centric, but has been isolated so
     that it can be easily tweaked for other platforms */
  if (path[0] == '/')
    return 1;
  else
    return 0;
}

/* directory interface */
VeDir veOpenDir(char *dir) {
  DIR *dd;
  if (!(dd = opendir(dir)))
    return NULL;
  return dd;
}

void veCloseDir(VeDir d) {
  DIR *dd = (DIR *)d;
  if (dd)
    closedir(dd);
}

char *veReadDir(VeDir d, int flags) {
  DIR *dd = (DIR *)d;
  struct dirent *de;
  
  if (!dd)
    return NULL;
  while ((de = readdir(d))) {
    if (de->d_name[0] == '.' && !(flags & VE_DIR_HIDDEN))
      continue;
    return de->d_name;
  }
  return NULL;
}
