#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ve_debug.h"
#include "ve_util.h"
#include "ve_thread.h"

static char *ve_debug_prefix = NULL;
static int ve_debug_level = 0;
static VeThrMutex *debug_mutex = NULL;

void veDebug(char *fmt, ...) {
  va_list ap;
  /* race condition */
  if (!debug_mutex)
    debug_mutex = veThrMutexCreate();

  va_start(ap,fmt);
  /* MutexLock can generate debug messages... */
  veThrMutexQuietLock(debug_mutex);
  if (ve_debug_prefix)
    fprintf(stderr,"%s ",ve_debug_prefix);
  fprintf(stderr,"[%8x] ",(unsigned)veThreadId());
  fprintf(stderr, "debug: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  fflush(stderr);
  veThrMutexQuietUnlock(debug_mutex);
  va_end(ap);
}

void veSetDebugLevel(int i) {
  ve_debug_level = i;
}

int veGetDebugLevel(void) {
  return ve_debug_level;
}

void veSetDebugPrefix(char *txt) {
  if (ve_debug_prefix) {
    free(ve_debug_prefix);
    ve_debug_prefix = NULL;
  }
  if (txt)
    ve_debug_prefix = veDupString(txt);
}
