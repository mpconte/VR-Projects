#ifndef VE_CLOCK_H
#define VE_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if 0
}
#endif /* */

/** misc
<p>The ve_clock module provides an abstraction for a general high-resolution
clock.  This clock is not guaranteed to be high-resolution.  It will be
limited by the available APIs and actual clocks on any platform.  In general,
the package uses <code>clock_gettime()</code> where available, and 
<code>gettimeofday()</code> on UNIX-like platforms where it is not available.
A single clock is provided.</p>
<p>The value of the clock is in reference to a zero-point.  This point can
be retrieved or set by providing a reference in a string format:
<blockquote>
<i>Y M D h:m:s X u</i>
</blockquote>
where Y is the year, M is the month, D is the day of the month, h is the
hour (in 24-hour format), m is minutes, s is seconds, X is 1 if DST is in
effect, 0 otherwise, and u is milliseconds.  All values are numerical, e.g.
<blockquote>
2001 11 4 12:23:41 0 534
</blockquote>
The default is for the zero point to be declared whenever the clock is
initialized.  You should only need to change the zero point when you
want to synchronize the clock to another.  Note that no method is
provided for synchronizing the <i>rate</i> of the clock.  This is beyond
the scope of this library.
</p>
**/

/* the following are platform-specific */
/** function veClockInit
    Initializes the clock.  This function must be called before any other
    function in this module.
 **/
void veClockInit();
/** function veClockGetZeroRef
    Returns a string describing the current zero reference point.
    This function is thread-safe in that many threads can retrieve
    the string (which should be treated as read-only).  However, the
    data returned may become invalid after another thread resets the
    zero reference point.

    @returns
    A pointer to a string describing the zero reference point.
 **/
char *veClockGetZeroRef(); /* gets zero time as an absolute string */
/** function veClockGetRef
    Returns a string describing the reference point for an arbitrary
    point in the clock's absolute time.  This is useful for determining
    the wall clock time of the clock's value.  Note that this function is
    current <i>not</i> thread-safe.  If you only need the zero reference
    point, it is preferred to use <code>veClockGetZeroRef()</code> as it
    is safer.

    @param ref
    The point in time for which to return the real-world time.
    
    @returns
    A string describing the real-world time at absolute clock time <i>ref</i>.
**/
char *veClockGetRef(long ref); 
/** function veClockSetRef
    Provides a reference point for the clock.  The real-world time is given
    in the string.  The absolute clock time to which this equates should be
    given as the first argument.  Calling this function will change the
    zero reference point.
    
    @param ref
    The clock time to which the real-world time equates.
    
    @param str
    The real-world time in string format.
**/
void veClockSetRef(long ref, char *str);
/** function veClock
    The general function for retrieving the current value of the clock.

    @returns 
    The current value of the clock in milliseconds since the
    zero reference point.
**/
long veClock();         /* returns current clock value in milliseconds */

/** function veClockConvUnix
    Converts a "Unix" time (seconds since Jan. 1, 1970) to veClock()
    units.  This function is only guaranteed to work on Unix systems
    and may generate an error on other systems when called.
 */
long veClockConvUnix(long);

/** function veClockConvTimeval
    Coverts a Unix-like timeval structure to veClock() units.  This
    function is only guaranteed to work on Unix systems and may
    generate an error on other systems when called.
 */
long veClockConvTimeval(long tv_secs, long tv_usecs);

/** function veClockConvTimespec
    Converts a Unix-like timespec structure to veClock() units. This
    function is only guaranteed to work on Unix systems and may
    generate an error on other systems when called.
 */
long veClockConvTimespec(long tv_secs, long tv_nsecs);

/** function veClockNano
    An alternate, potentially higher-resolution function returning a clock
    time in nanoseconds, instead of milliseconds.  The effective resolution of
    this clock will vary from system to system.  Its value is not guaranteed
    to be relative to the zero reference point and will not be on most
    systems to avoid overhead in measuring the time.  Thus this clock should
    only be used for measuring intervals, not absolute points in time.

    @returns
    A clock value in nanoseconds.
**/
long veClockNano();     /* returns hi-res clock in nanoseconds - not necess.
			   referenced to zero point */

/** type VeClockHR
    A structure describing a hi-res clock value.
 */
typedef struct {
  unsigned long secs;
  unsigned long nsecs;
} VeClockHR;

/** function veClockHires
    Returns a high resolution clock value.  To accomodate large periods
    of time, the clock value is returned as two parts - a "seconds" value
    and a "nanoseconds" value.  The absolute value of this clock should
    not be counted upon (i.e. what "0" means) but instead relative 
    measurements between clock values should be considered.  The 
    time value in seconds is equivelent to <code>secs + nsecs*1.0e-9</code>.
 */
VeClockHR veClockHires();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_CLOCK_H */
