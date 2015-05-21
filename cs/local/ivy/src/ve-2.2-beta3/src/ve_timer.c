/* Generic timer support - pieces that work everywhere */
#include <stdio.h>
#include <stdlib.h>

#include "ve_alloc.h"
#include "ve_debug.h"
#include "ve_thread.h"
#include "ve_timer.h"
#include "ve_stats.h"
#include "ve_main.h"
#include "ve_mp.h"

#define MODULE "ve_clock"

#define DEFAULT_TIMER_HEAP_SIZE 1024

typedef struct theap_entry {
  long msecs;
  VeTimerProc proc;
  void *arg;
} THeapEntry;

/* Note that for the convenience of the math, we don't use timerHeap[0] */
/* The root of the heap is at timerHeap[1] */
static THeapEntry *timerHeap = NULL;
static int theapAlloc = 0, theapPtr = 1;

static VeStatistic *timer_latency_stat = NULL;
static float timer_latency = 0.0;
static long timer_diff_acc = 0;
static int timer_samples = 0;
static int timer_sample_size = 20;

#ifndef TIMER_BUSYWAIT
#define TIMER_BUSYWAIT 0
#endif /* TIMER_BUSYWAIT */

#ifndef TIMER_BUSYLIMIT
#define TIMER_BUSYLIMIT 30
#endif /* TIEMR_BUSYLIMIT */

static int timer_busywait = TIMER_BUSYWAIT;
static int timer_busylimit = TIMER_BUSYLIMIT;

static void dump_heap() {
  int i;
  fprintf(stderr, "Timer heap:\n");
  for(i = 1; i < theapPtr; i++) {
    fprintf(stderr, "\t%ld %x %x\n", timerHeap[i].msecs, 
	    (unsigned)(timerHeap[i].proc),
	    (unsigned)(timerHeap[i].arg));
  }
}

static void veTimerResizeHeap(int sz) {
  /* only allow heap to grow via this call... */
  VE_DEBUGM(6,("veTimerResizeHeap(%d)",sz));

  if (sz < theapAlloc)
    veFatalError(MODULE, "veTimerResizeHeap: cannot shrink heap (current = %d, requested = %d\n", theapAlloc, sz);
  theapAlloc = sz < DEFAULT_TIMER_HEAP_SIZE ? DEFAULT_TIMER_HEAP_SIZE : sz;
  timerHeap = veRealloc(timerHeap,theapAlloc);
  if (timerHeap == NULL)
    veFatalError(MODULE, "veTimerResizeHeap: failed to reallocate timer heap (size = %d)\n", theapAlloc);
}

static int emptyHeap() {
  return (theapPtr <= 1);
}

static void upheap(int i) {
  int p;
  THeapEntry e;
  while(i > 1) {
    p = i/2;
    if (timerHeap[i].msecs < timerHeap[p].msecs) {
      /* swap */
      e = timerHeap[i];
      timerHeap[i] = timerHeap[p];
      timerHeap[p] = e;
    } else
      break; /* no swap - no need to check further */
  }
}

static void downheap(int i) {
  THeapEntry e;
  int l,r;
  
  while(i < theapPtr) {
    l = 2*i;
    r = 2*i+1;
    if (l < theapPtr) {
      /* some children */
      if (r < theapPtr) {
	/* two children */
	if (timerHeap[r].msecs < timerHeap[l].msecs)
	  l = r; /* use the "right" child */
      }
      if (timerHeap[l].msecs < timerHeap[i].msecs) {
	/* swap */
	e = timerHeap[l];
	timerHeap[l] = timerHeap[i];
	timerHeap[i] = e;
	i = l;
      } else {
	/* no swap - no need to continue */
	break;
      }
    } else {
      /* no children */
      break;
    }
  }
}

static void insertHeap(long msecs, VeTimerProc proc, void *arg) {
  if (theapPtr >= theapAlloc)
    /* try to grow the heap */
    veTimerResizeHeap(theapAlloc*2);
  
  timerHeap[theapPtr].msecs = msecs;
  timerHeap[theapPtr].proc = proc;
  timerHeap[theapPtr].arg = arg;
  theapPtr++;
  upheap(theapPtr-1);
}

static THeapEntry getNextTimer() {
  THeapEntry e;
  if (emptyHeap())
    veFatalError(MODULE, "internal: getNextTimer() called on empty heap");
  e = timerHeap[1];
  if (theapPtr > 2)
    timerHeap[1] = timerHeap[theapPtr-1];
  theapPtr--;
  downheap(1);
  return e;
}

static long peekNextTime() {
  if (emptyHeap())
    veFatalError(MODULE, "internal: peekNextTime() called on empty heap");
  return (timerHeap[1].msecs);
}

VeThrMutex *timer_mutex = NULL;
VeThrCond *timer_wait = NULL;

void veAddTimerProc(long msecs, VeTimerProc proc, void *arg) {
  if (veMPTestSlaveGuard())
    return;

  veThrMutexLock(timer_mutex);
  insertHeap(veClock()+msecs,proc,arg);
  if (VE_ISDEBUG(8))
    dump_heap();
  veThrCondBcast(timer_wait);
  veThrMutexUnlock(timer_mutex);
}

int veTimerEventsPending() {
  int res;
  veThrMutexLock(timer_mutex);
  res = (!emptyHeap()) && (veClock() >= peekNextTime());
  veThrMutexUnlock(timer_mutex);
  return res;    
}

void veTimerWaitForEvents() {
  long now, next;
  static int init = 0;
  if (!init) {
    char *c;
    if ((c = getenv("VE_TIMER_BUSY")))
      timer_busywait = atoi(c);
    else if ((c = veGetOption("timer_busy")))
      timer_busywait = atoi(c);
    if ((c = veGetOption("timer_limit")))
      timer_busylimit = atoi(c);
    else if ((c = getenv("VE_TIMER_LIMIT")))
      timer_busylimit = atoi(c);
    init = 1;
  }

  veThrMutexLock(timer_mutex);
  while (emptyHeap() || (now = veClock()) < (next = peekNextTime())) {
    if (emptyHeap())
      veThrCondWait(timer_wait,timer_mutex);
    else if (timer_busywait) {
      /* Some platforms have poor granularity on sleeps
	 (most Linux kernels seem to have a interval of 10ms, and 
	 the sleep code sleeps for a minimum of 2 intervals for non-zero
	 sleep times).  If you have computing power to spare on these
	 platforms, it is a good idea to set either the VE_TIMER_BUSY
	 environment variable, or the timer_busy option.  The VE_TIMER_LIMIT
	 or timer_limit values change the default granularity.  Any wait
	 smaller than VE_TIMER_LIMIT will be "busy-waited" with sleep(0)'s
	 for yields.
      */
      if (next-now < timer_busylimit) {
	/* busy wait */
	while (veClock() < peekNextTime()) {
	  veThrMutexUnlock(timer_mutex);
	  veMicroSleep(0);
	  veThrMutexLock(timer_mutex);
	}
      } else
	veThrCondTimedWait(timer_wait,timer_mutex,(next-now)/2);
    } else
      /* when busywait is not set, let the OS notify us */
      veThrCondTimedWait(timer_wait,timer_mutex,next-now);
  }
  veThrMutexUnlock(timer_mutex);
}

int veTimerProcEvent() {
  /* fire off one pertinent timer (if any)... */
  long now;
  veThrMutexLock(timer_mutex);
  if (!emptyHeap() && (now = veClock()) >= peekNextTime()) {
    THeapEntry e = getNextTimer();
    /* update latency stats */
    if (timer_latency_stat) {
      timer_diff_acc += now-e.msecs;
      timer_samples++;
      if (timer_samples >= timer_sample_size) {
	timer_latency = (float)timer_diff_acc/timer_samples;
	timer_diff_acc = 0;
	timer_samples = 0;
	veUpdateStatistic(timer_latency_stat);
      }
    }

    veThrMutexUnlock(timer_mutex); /* don't need it right now,
				      and we need to avoid deadlocks */
    VE_DEBUGM(6,("timer firing off handler"));
    e.proc(e.arg);
    VE_DEBUGM(6,("timer handler finished"));
    veThrMutexLock(timer_mutex);
  }
  veThrMutexUnlock(timer_mutex);
  return 0;
}

void veTimerInit() {
  timer_mutex = veThrMutexCreate();
  timer_wait = veThrCondCreate();
  timer_latency_stat = veNewStatistic(MODULE, "timer_latency", "msecs");
  timer_latency_stat->type = VE_STAT_FLOAT;
  timer_latency_stat->data = &timer_latency;
  veAddStatistic(timer_latency_stat);
}
