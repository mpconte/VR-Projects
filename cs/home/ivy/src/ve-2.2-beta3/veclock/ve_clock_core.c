/* Generic clock support that relies on a platform-dependent implementation
   of:
   veClockHires();
*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "ve_clock.h"

#define MODULE "ve_clock"

/* everybody should have timespec... */
static VeClockHR zero_time;
static char zero_time_str[1024] = "";

/* veClockGetZeroRef() is better for multi-threaded programs */
/* It is only updated when the zero reference is changed */
char *veClockGetZeroRef() {
  return zero_time_str;
}

/* veClockGetRef() is dangerous in multi-threaded programs */
char *veClockGetRef(long refpt) {
  VeClockHR hr;
  static char str[1024];
  time_t secs;
  int msecs;
  struct tm *tm;

  hr.nsecs = zero_time.nsecs + (refpt%1000)*1000000;
  secs = zero_time.secs + refpt/1000 + hr.nsecs/1000000000;
  hr.nsecs %= 1000000000;

  return veClockMakeRef(hr);
}

void veClockSetRef(long refpt, char *tmstr) {
  zero_time = veClockParseRef(refpt,tmstr);
  strcpy(zero_time_str,veClockGetRef(0));
}

void veClockInit() {
  /* declare this to be zero time */
  zero_time = veClockHires();
}

long veClockConvUnix(long tv_secs) {
  return (long)(tv_secs - zero_time.secs);
}

long veClockConvTimeval(long tv_secs, long tv_usecs) {
  return (long)(tv_secs - zero_time.secs)*1000 +
    (long)(tv_usecs - (zero_time.nsecs/1000))/1000;
}

long veClockConvTimespec(long tv_secs, long tv_nsecs) {
  return (long)(tv_secs - zero_time.secs)*1000 +
    (long)(tv_nsecs - zero_time.nsecs)/1000000;
}

long veClock() {
  VeClockHR hr;
  hr = veClockHires();
  return (long)(hr.secs - zero_time.secs)*1000 + 
    (long)(hr.nsecs - zero_time.nsecs)/1000000;
}

long veClockNano() {
  VeClockHR hr;
  hr = veClockHires();
  /* this is likely to overflow - let it... */
  return (long)hr.secs*1000000000 + (long)hr.nsecs;
}
