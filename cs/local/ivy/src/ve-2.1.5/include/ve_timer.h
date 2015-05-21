#ifndef VE_TIMER_H
#define VE_TIMER_H

#include <ve_clock.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if 0
}
#endif /* */

typedef void (*VeTimerProc)(void *arg);

/** misc
    The ve_timer module provides a simple timer callback model based upon
    ve_clock library.  It allows timer callbacks to be called at particular
    points in the future.  Timer callbacks have the following type:
    <blockquote><code>void (*VeTimerProc)(void *arg)</code></blockquote>
    The argument is provided as a hoook for the application to pass its
    own data to the timer callback.  The system using this library
    must provide a thread to process the events.  The thread should look
    something like:
<pre>
        while (1) {
                while (!veTimerEventsPending())
                        veTimerWaitForEvents();
                while (veTimerEventsPending())
                        veTimerProcEvent();
        }
</pre>
    The main VE library provides such a thread if you are using the
    <code>veInit()</code> and <code>veRun()</code> or <code>veEventLoop()</code>
    calls.
*/

/* following are across all platforms */
/** function veTimerInit
    Must be called to initialize the timer callback mechanism before
    calling any other timer functions.  Programs that use <code>veInit()</code>
    do not need to call this function.
*/
void veTimerInit();     /* initializes clock and timer */
 
/** function veAddTimerProc
    Adds a timer callback.  Note that after adding a timer callback, the
    reference point of the clock should not be changed.

    @param msecs
    The time that must elapse before the timer is called, in milliseconds.
    
    @param proc
    The procedure to call after <i>time</i> milliseconds pass.

    @param arg
    An argument to pass to the callback when it is called.
*/
void veAddTimerProc(long msecs, VeTimerProc proc, void *arg);

/** function veTimerEventsPending
    Checks to see if there are any timer callbacks that are ready to
    be serviced.
    
    @returns
    1 if there are timer callbacks which should be processed now
    and 0 otherwise.
*/
int veTimerEventsPending();

/** function veTimerProcEvent
    Processes a single timer callback if there are any to process.

    @returns
    0 if successful, non-zero otherwise.
*/
int veTimerProcEvent(); /* process an event */

/** function veTimerWaitForEvents
    Puts the current thread to sleep until there are timer events to
    be processed.
*/
void veTimerWaitForEvents();

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* VE_TIMER_H */
