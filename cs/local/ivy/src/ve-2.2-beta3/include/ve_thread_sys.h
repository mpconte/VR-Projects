#ifndef VE_THREAD_SYS_H
#define VE_THREAD_SYS_H

#include <ve_version.h>

/** misc
    <p>
    The ve_thread_sys.h include file defines the platform-specific
    thread types.  These are:
    </p>
    <ul>
    <li><b>VeThread</b></li> - a thread
    <li><b>VeThrMutex</b></li> - a mutex
    <li><b>VeThrCond</b></li> - a condition variable
    <li><b>VeThrBarrier</b></li> - an n-thread barrier
    </ul>
    <p>All types should be treated as though they were opaque struct-like 
    types.
    They are passed to/from function calls by reference, but applications
    should never assume anything about the members of the structures (or
    even that they actually are structures).  It should be assumed that
    these objects are not usable unless they have been initialized with
    a VE call as described in the thread interface.</p>
    <p>This header file is part of an abstraction layer so that threads
    can be defined in a relatively cross-platform manner to making porting
    of the entire VE library easier.</p>
*/

#if defined(_unix) || defined(unix) || defined(__APPLE__)
/* A Unix-like system - use POSIX threads */
#include <pthread.h>

typedef pthread_t VeThread;
typedef pthread_mutex_t VeThrMutex;
typedef pthread_cond_t VeThrCond;
typedef pthread_key_t VeThrKey;
    
typedef struct ve_thr_barrier {
  int count;
  int max;
  int exiting;
  pthread_mutex_t mutex;
  pthread_cond_t reached, entry;
} VeThrBarrier;
#else
/* Not every compiler understands the following, but it should halt those
   that don't */
#error "*** No ve_thread support for this platform"
#endif

#endif /* VE_THREAD_SYS_H */
