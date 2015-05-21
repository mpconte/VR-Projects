/* Thread implementation using pthreads */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <ve_alloc.h>
#include <ve_error.h>
#include <ve_debug.h>
#include <ve_thread.h>

#define MODULE "ve_thread_posix"

#if defined(__sun) || defined(__sgi)
#define HAVE_RTCLOCK 1
#else
#undef HAVE_RTCLOCK
#endif /* __sun || __sgi */

VeThrBarrier *veThrBarrierCreate(int size) {
  VeThrBarrier *b;
  b = veAllocObj(VeThrBarrier);
  assert(b != NULL);
  pthread_mutex_init(&(b->mutex), NULL);
  pthread_cond_init(&(b->reached), NULL);
  pthread_cond_init(&(b->entry), NULL);
  b->count = 0;
  b->max = size;
  b->exiting = 0;
  return b;
}

void veThrBarrierDestroy(VeThrBarrier *b) {
  veFree(b);
}

void veThrBarrierEnter(VeThrBarrier *b)
{
  if (pthread_mutex_lock(&b->mutex))
    veFatalError(MODULE, "veThrBarrierEnter: pthread_mutex_lock: %s", strerror(errno));

  while (b->exiting) {
    /* some threads have not left the barrier yet */
    if (pthread_cond_wait(&b->entry, &b->mutex))
      veFatalError(MODULE, "veThrBarrierEnter: pthread_cond_wait: %s", strerror(errno));
  }
  
  b->count++;
  
  if (b->count >= b->max) {
    /* last thread - let everybody go */
    b->exiting = 1;
    if (pthread_cond_broadcast(&b->reached))
      veFatalError(MODULE, "veThrBarrierEnter: pthread_cond_broadcast: %s", strerror(errno));
  } else {
    while(!b->exiting) {
      /* not everybody is here yet */
      if (pthread_cond_wait(&b->reached, &b->mutex)) 
	veFatalError(MODULE, "veThrBarrierEnter: pthread_cond_wait: %s", strerror(errno));
    }
  }

  b->count--;

  if (b->count <= 0) {
    b->exiting = 0;
    /* allow any waiting threads to enter the barrier */
    if (pthread_cond_broadcast(&b->entry))
      veFatalError(MODULE, "veThrBarrierEnter: pthread_cond_broadcast: %s", strerror(errno));
  }

  if(pthread_mutex_unlock(&b->mutex))
    veFatalError(MODULE, "veThrBarrierEnter: pthread_mutex_unlock: %s", strerror(errno));
}

VeThread *veThreadCreate() {
  pthread_t *p;
  p = veAllocObj(pthread_t);
  return p;
}

/* choose scope for kernel-level threads */
/* on SGIs "BOUND" scope allows kernel-level thread effects without
   superuser privileges */
#if defined(PTHREAD_SCOPE_BOUND_NP)
#define VE_THR_KERNEL_SCOPE PTHREAD_SCOPE_BOUND_NP
#elif defined(PTHREAD_SCOPE_BOUND)
#define VE_THR_KERNEL_SCOPE PTHREAD_SCOPE_BOUND
#else
#define VE_THR_KERNEL_SCOPE PTHREAD_SCOPE_SYSTEM
#endif

int veThreadInit(VeThread *t, void *(*func)(void *), void *arg, int priority,
		 int flags) {
  pthread_t p;
  pthread_attr_t a;
#ifdef __sgi
  static int min_p, max_p;
  struct sched_param spar;
#endif
  static int init = 0;
  int st;

  if (!init) {
#ifdef __sgi
    min_p = sched_get_priority_min(SCHED_RR);
    max_p = sched_get_priority_max(SCHED_RR);
#endif /* __sgi */
    init = 1;
  }

  pthread_attr_init(&a);
  if (flags & VE_THR_KERNEL) {
    if (pthread_attr_setscope(&a,VE_THR_KERNEL_SCOPE))
      veWarning(MODULE, "veThreadInit: could not set scope of thread: %s",
		strerror(errno));
  }
  if (priority > 0) {
#ifdef __sgi
    spar.sched_priority = priority + min_p;
    if (spar.sched_priority > max_p)
      spar.sched_priority = max_p;
    pthread_attr_setschedparam(&a,&spar);
#endif /* __sgi */
  }
  st = pthread_create(&p,&a,func,arg);
  if (st && (flags & VE_THR_KERNEL)) {
    /* try to fall-back on user-level thread */
    pthread_attr_setscope(&a,PTHREAD_SCOPE_PROCESS);
    if ((st = pthread_create(&p,&a,func,arg)) == 0)
      veWarning(MODULE, "veThreadInit: could not create kernel thread - falling back on user thread");
  }
  if (st) {
    veError(MODULE, "veThreadInit: could not create thread: %s", 
	    strerror(errno));
    return -1;
  }
  if (t)
    *t = p;
  VE_DEBUGM(10,("[%x] thread created",(int)p));
  return 0;
}

void veThreadDestroy(VeThread *t) {
  veFree(t);
}

void veThreadKill(VeThread *t) {
  if (t)
    pthread_cancel((pthread_t)*t);
}

void veThreadWait(VeThread *t) {
  if (t)
    pthread_join((pthread_t)*t,NULL);
}

VeThrMutex *veThrMutexCreate() {
  pthread_mutex_t *m;
  
  m = veAllocObj(pthread_mutex_t);
  pthread_mutex_init(m,NULL);
  return (VeThrMutex *)m;
}

void veThrMutexDestroy(VeThrMutex *m) {
  veFree(m);
}

void veThrMutexQuietLock(VeThrMutex *m) {
  if (pthread_mutex_lock((pthread_mutex_t *)m))
    veFatalError(MODULE, "pthread_mutex_lock: %s", strerror(errno));
}

void veThrMutexLock(VeThrMutex *m) {
  VE_DEBUGM(10,("[%d] requesting lock on mutex 0x%x", 
	      (int) pthread_self(), (unsigned int)m));
  veThrMutexQuietLock(m);
  VE_DEBUGM(10,("[%d] acquired lock on mutex 0x%x", 
	       (int) pthread_self(), (unsigned int)m));
}

void veThrMutexQuietUnlock(VeThrMutex *m) {
  if (pthread_mutex_unlock((pthread_mutex_t *)m))
    veFatalError(MODULE, "pthread_mutex_unlock: %s", strerror(errno));
}

void veThrMutexUnlock(VeThrMutex *m) {
  VE_DEBUGM(10,("[%d] releasing lock on mutex 0x%x",
	       (int) pthread_self(), (unsigned int)m));
  veThrMutexQuietUnlock(m);
}

VeThrCond *veThrCondCreate() {
  pthread_cond_t *c;
  c = veAllocObj(pthread_cond_t);
  pthread_cond_init(c,NULL);
  return (VeThrCond *)c;
}

void veThrCondDestroy(VeThrCond *c) {
  veFree(c);
}

void veThrCondWait(VeThrCond *c, VeThrMutex *m) {
  VE_DEBUGM(10,("[%d] waiting on cond 0x%x mutex 0x%x",
	      (int) pthread_self(), (unsigned int) c, (unsigned int)m));
  if (pthread_cond_wait((pthread_cond_t *)c,(pthread_mutex_t *)m))
    veFatalError(MODULE, "pthread_cond_wait: %s", strerror(errno));
  VE_DEBUGM(10,("[%d] woken up from cond 0x%x mutex 0x%x",
	      (int) pthread_self(), (unsigned int) c, (unsigned int)m));
}

void veThrCondTimedWait(VeThrCond *c, VeThrMutex *m, long ms) {
  struct timespec ts;
  int i;

#ifdef HAVE_RTCLOCK
  if (clock_gettime(CLOCK_REALTIME,&ts))
    veFatalError(MODULE,"veThrCondTimedWait: could not get clock time: %s",
		 strerror(errno));
#else
  {
    struct timeval tv;
    if (gettimeofday(&tv,NULL))
      veFatalError(MODULE,"veThrCondTimedWait: coudl not get clock time: %s",
		   strerror(errno));
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec*1000;
  }
#endif /* HAVE_RTCLOCK */
  ts.tv_nsec += (ms%1000)*1000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec += ts.tv_nsec/1000000000;
    ts.tv_nsec %= 1000000000;
  }
  ts.tv_sec += (ms/1000);
  
  VE_DEBUGM(10,("[%d] timed wait on cond 0x%x mutex 0x%x",
	       (int) pthread_self(), (unsigned int) c, (unsigned int)m));
  if ((i = pthread_cond_timedwait(c,m,&ts)) && i != ETIMEDOUT)
    veFatalError(MODULE, "pthread_cond_timedwait: %s", strerror(errno));
  VE_DEBUGM(10,("[%d] woken up from cond 0x%x mutex 0x%x",
	       (int) pthread_self(), (unsigned int) c, (unsigned int)m));
}

void veThrCondSignal(VeThrCond *c) {
  VE_DEBUGM(10,("[%d] broadcasting on cond 0x%x",
	      (int) pthread_self(), (unsigned int) c));
  if (pthread_cond_signal(c))
    veFatalError(MODULE, "pthread_cond_broadcast: %s", strerror(errno));
}

void veThrCondBcast(VeThrCond *c) {
  VE_DEBUGM(10,("[%d] broadcasting on cond 0x%x",
	      (int) pthread_self(), (unsigned int) c));
  if (pthread_cond_broadcast(c))
    veFatalError(MODULE, "pthread_cond_broadcast: %s", strerror(errno));
}

int veThreadId(void) {
  return (int)pthread_self();
}

VeThrKey *veThrKeyCreate(VeThrDataFreeProc *p) {
  pthread_key_t *k;
  k = veAllocObj(pthread_key_t);
  pthread_key_create(k,p);
  return k;
}

void veThrKeyDestroy(VeThrKey *k) {
  pthread_key_t *kk = (pthread_key_t *)k;
  if (kk) {
    pthread_key_delete(*kk);
    veFree(kk);
  }
}

void *veThrDataGet(VeThrKey *k) {
  if (!k)
    return NULL;
  return pthread_getspecific(*k);
}

void veThrDataSet(VeThrKey *k, void *data) {
  if (!k)
    return;
  (void) pthread_setspecific(*k,data);
}


