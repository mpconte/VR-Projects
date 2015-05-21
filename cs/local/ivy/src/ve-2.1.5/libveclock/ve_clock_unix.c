/* Unix-specific clock support */
/* we need to provide the following:

   veClockGetRef();
   veClockSetRef();
   veClock();
   veClockNano();
   veClockInit();

*/

/* This implementation uses the clock_gettime() function for what is
   (hopefully) a fairly good real-time clock.  For this implementation,
   veClockGetZero() returns a reference to a POSIX timespec structure.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#include "ve_clock.h"

#define MODULE "ve_clock_unix"

/* System-specific stuff - this should generally be moved somewhere
   more central... */
#if defined(__sun)
/* possibly SunOS 4.x, but almost certainly Solaris nowadays */
#define HAVE_RTCLOCK 1
#define VE_ABS_CLOCKID   CLOCK_REALTIME
#define VE_HIRES_CLOCKID CLOCK_HIGHRES
#elif defined (__sgi)
/* IRIX 6.x */
#define HAVE_RTCLOCK 1
#define VE_ABS_CLOCKID   CLOCK_REALTIME
#define VE_HIRES_CLOCKID CLOCK_REALTIME
#elif defined (linux)
/* Most Linux distributions do not have clock_gettime() */
#undef HAVE_RTCLOCK
#undef VE_ABS_CLOCKID
#undef VE_HIRES_CLOCKID
#else 
/* go with bare-bones defaults */
#undef HAVE_RTCLOCK
#undef VE_ABS_CLOCKID
#undef VE_HIRES_CLOCKID
#endif /* os switch */

/* everybody should have timespec... */
static struct timespec zero_time;
static char zero_time_str[1024];

static void get_the_time(int hires, struct timespec *ts) {
#ifdef HAVE_RTCLOCK
  if (clock_gettime(hires ? VE_HIRES_CLOCKID : VE_ABS_CLOCKID,ts))
    fprintf(stderr,"could not read clock: %s", strerror(errno));
#else
  struct timeval tv;
  if (gettimeofday(&tv,NULL))
    fprintf(stderr,"could not read clock: %s", strerror(errno));
  ts->tv_sec = tv.tv_sec;
  ts->tv_nsec = tv.tv_usec*1000;
#endif
}

/* veClockGetZeroRef() is better for multi-threaded programs */
/* It is only updated when the zero reference is changed */
char *veClockGetZeroRef() {
  return zero_time_str;
}

/* veClockGetRef() is dangerous in multi-threaded programs */
char *veClockGetRef(long refpt) {
  static char str[1024];
  time_t secs;
  int msecs;
  struct tm *tm;
  
  msecs = (zero_time.tv_nsec/1000000) + (refpt%1000);
  secs = zero_time.tv_sec + refpt/1000 + msecs/1000;
  msecs %= 1000;

  tm = localtime(&secs);
  sprintf(str,"%d %d %d %d:%d:%d %d %d",
	  tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min,
	  tm->tm_sec, tm->tm_isdst, msecs);
  return str;
}

void veClockSetRef(long refpt, char *tmstr) {
  time_t t;
  struct tm tm;
  int msecs = 0;
  
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
    zero_time.tv_sec = t;
    zero_time.tv_nsec = msecs*1000000;
    strcpy(zero_time_str,veClockGetRef(0));
  }
}

void veClockInit() {
  /* declare this to be zero time */
  get_the_time(0,&zero_time);
}

long veClockConvUnix(long tv_secs) {
  return (long)(tv_secs - zero_time.tv_sec);
}

long veClockConvTimeval(long tv_secs, long tv_usecs) {
  return (long)(tv_secs - zero_time.tv_sec)*1000 +
    (long)(tv_usecs - (zero_time.tv_nsec/1000))/1000;
}

long veClockConvTimespec(long tv_secs, long tv_nsecs) {
  return (long)(tv_secs - zero_time.tv_sec)*1000 +
    (long)(tv_nsecs - zero_time.tv_nsec)/1000000;
}

long veClock() {
  struct timespec ts;
  
  get_the_time(0,&ts);
  return (long)(ts.tv_sec - zero_time.tv_sec)*1000 + 
    (long)(ts.tv_nsec - zero_time.tv_nsec)/1000000;
}



long veClockNano() {
  struct timespec ts;
  get_the_time(1,&ts);
  /* this is likely to overflow - let it... */
  return (long)ts.tv_sec*1000000000 + (long)ts.tv_nsec;
}

VeClockHR veClockHires() {
  VeClockHR hr;
  struct timespec ts;
  get_the_time(1,&ts);
  hr.secs = ts.tv_sec;
  hr.nsecs = ts.tv_nsec;
  return hr;
}
