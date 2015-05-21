/* Unix-specific clock support */
/* we need to provide the following:

   veClockMakeRef();
   veClockParseRef();
   veClockHires();
*/

/* This implementation uses the clock_gettime() function for what is
   (hopefully) a fairly good real-time clock.  For this implementation,
   veClockGetZero() returns a reference to a POSIX timespec structure.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#include "ve_clock.h"

#define MODULE "ve_clock_unix"

char *veClockMakeRef(VeClockHR hr) {
  static char str[1024];
  struct tm *tm;

  tm = localtime(&hr.secs);
  snprintf(str,1024,"%d %d %d %d:%d:%d %d %d",
	   tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min,
	   tm->tm_sec, tm->tm_isdst, hr.nsecs /= 1000000);
  return str;
}

VeClockHR veClockParseRef(long refpt, char *tmstr) {
  time_t t;
  struct tm tm;
  int msecs = 0;
  VeClockHR zero_time = { 0, 0 };
  
  if (sscanf(tmstr,"%d %d %d %d:%d:%d %d %d",
	     &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, 
	     &tm.tm_min, &tm.tm_sec, &tm.tm_isdst, &msecs) >= 7) {
    t = mktime(&tm);
    msecs -= refpt%1000;
    if (msecs < 0) {
      msecs += 1000;
      t--;
    }
    t -= refpt/1000;
    zero_time.secs = t;
    zero_time.nsecs = msecs*1000000;
  }
  return zero_time;
}

VeClockHR veClockHires() {
  VeClockHR hr;
  struct timeval tv;
  if (gettimeofday(&tv,NULL)) {
    fprintf(stderr, MODULE ": gettimeofday failed: %s", strerror(errno));
    exit(1);
  }
  hr.secs = tv.tv_sec;
  hr.nsecs = tv.tv_usec*1000;
  return hr;
}
