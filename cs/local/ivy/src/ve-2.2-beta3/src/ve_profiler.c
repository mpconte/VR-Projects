#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <ve_debug.h>
#include <ve_error.h>
#include <ve_profiler.h>
#include <ve_thread.h>
#include <ve_clock.h>

#define MODULE "ve_profiler"

static VeThrMutex *ve_pf_mutex = NULL;
static FILE *ve_pf_dest = NULL;
static int ve_pf_recording = 0;
static int ve_pf_pausecnt = 0;

void vePfInit(void) {
  char *s;
  ve_pf_mutex = veThrMutexCreate();
  if (!ve_pf_dest)
    ve_pf_dest = stderr;
  if (s = getenv("VE_PF")) {
    if (vePfDest(s)) {
      veWarning(MODULE,"defaulting to stderr for events");
      (void) vePfDest("@stderr");
    }
    vePfRecord();
  }
}

int vePfDest(char *dest) {
  int res = 0;

  assert(ve_pf_mutex != NULL);

  veThrMutexLock(ve_pf_mutex);

  if (ve_pf_dest) {
    fflush(ve_pf_dest);
    if (ve_pf_dest != stderr && ve_pf_dest != stdout)
      fclose(ve_pf_dest);
    ve_pf_dest = NULL;
  }

  /* special cases */
  if (dest == NULL)
    dest = "@none";
  else if (dest[0] == '\0')
    dest = "@stderr";

  if (dest[0] == '@') {
    /* special */
    if (strcmp(dest,"@stdout") == 0)
      ve_pf_dest = stdout;
    else if (strcmp(dest,"@stderr") == 0)
      ve_pf_dest = stderr;
    else if (strcmp(dest,"@none") == 0)
      ve_pf_dest = NULL;
    else {
      veWarning(MODULE,"vePfDest: invalid special: %s",dest);
      res = -1;
    }
  } else {
    /* file */
    if (!(ve_pf_dest = fopen(dest,"w"))) {
      veWarning(MODULE,"vePfDest: could not open '%s': %s",
		dest, strerror(errno));
      res = -1;
    }
  }

  veThrMutexUnlock(ve_pf_mutex);
}

void vePfRecord(void) {
  assert(ve_pf_mutex != NULL);
  veThrMutexLock(ve_pf_mutex);
  ve_pf_recording = 1;
  veThrMutexUnlock(ve_pf_mutex);
}

void vePfFlush(void) {
  assert(ve_pf_mutex != NULL);
  veThrMutexLock(ve_pf_mutex);
  if (ve_pf_dest)
    fflush(ve_pf_dest);
  veThrMutexUnlock(ve_pf_mutex);
}

void vePfStop(void) {
  assert(ve_pf_mutex != NULL);
  veThrMutexLock(ve_pf_mutex);
  ve_pf_recording = 0;
  veThrMutexUnlock(ve_pf_mutex);
}

/* this will blow up on us if the number of recursive pauses exceeds
   the size of an int - unlikely enough that it probably doesn't warrant
   concern */
void vePfPause(void) {
  assert(ve_pf_mutex != NULL);
  veThrMutexLock(ve_pf_mutex);
  ve_pf_pausecnt++;
  veThrMutexUnlock(ve_pf_mutex);
}

void vePfResume(void) {
  assert(ve_pf_mutex != NULL);
  veThrMutexLock(ve_pf_mutex);
  ve_pf_pausecnt--;
  veThrMutexUnlock(ve_pf_mutex);
}

int vePfIsPaused(void) {
  /* don't care about locking here */
  return (ve_pf_pausecnt != 0);
}

int vePfIsActive(void) {
  /* don't care about locking here */
  return (ve_pf_recording != 0);
}

void vePfEvent(char *module, char *event, char *fmt, ...) {
  va_list ap;
  
  /* short-circuit for when we aren't recording */
  if (!ve_pf_recording || ve_pf_pausecnt || !ve_pf_dest)
    return;

  assert(ve_pf_mutex != NULL);

  veThrMutexLock(ve_pf_mutex);
  if (ve_pf_recording && !ve_pf_pausecnt && ve_pf_dest) {
    VeClockHR hr;
    hr = veClockHires();
    if (fmt) {
	fprintf(ve_pf_dest,"%lu %lu %s %s ", hr.secs, hr.nsecs,
		(module ? module : "<null>"),
		(event ? event : "<null>"));
	va_start(ap,fmt);
	vfprintf(ve_pf_dest,fmt,ap);
	va_end(ap);
	fputc('\n',ve_pf_dest);
    } else {
	fprintf(ve_pf_dest,"%lu %lu %s %s\n", hr.secs, hr.nsecs,
		(module ? module : "<null>"),
		(event ? event : "<null>"));
    }
  }
  veThrMutexUnlock(ve_pf_mutex);
}
