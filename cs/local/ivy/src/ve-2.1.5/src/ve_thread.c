/* Thread implementation - higher-level functions which run on-top of
   the system-dependent stuff. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <ve_debug.h>
#include <ve_thread.h>

#define MODULE "ve_thread"

static void *fmalloc(int x) {
  void *v;
  v = malloc(x);
  assert(v != NULL);
  return v;
}

static VeThrCond *open_gate = NULL;
static VeThrMutex *gate_mutex = NULL;
static int gate_opened = 0;
static struct ve_thread_queue {
  void *(*func)(void *);
  void *arg;
  int priority;
  int flags;
  struct ve_thread_queue *next;
} *thread_queue = NULL;

void veThrInitDelayGate(void) {
  gate_opened = 0;
  if (!open_gate)
    open_gate = veThrCondCreate();
  if (!gate_mutex)
    gate_mutex = veThrMutexCreate();
}

void veThrDelayGate(void) {
  veThrMutexLock(gate_mutex);
  while (!gate_opened)
    veThrCondWait(open_gate,gate_mutex);
  veThrMutexUnlock(gate_mutex);
}

void veThrReleaseDelayed(void) {
  struct ve_thread_queue *q;
  VE_DEBUG(3,("releasing delayed threads"));
  veThrMutexLock(gate_mutex);
  gate_opened = 1;
  veThrCondBcast(open_gate);
  /* flush thread queue */
  while (thread_queue) {
    q = thread_queue;
    thread_queue = thread_queue->next;
    veThreadInit(NULL,q->func,q->arg,q->priority,q->flags);
    free(q);
  }
  veThrMutexUnlock(gate_mutex);
}

int veThreadInitDelayed(void *(*func)(void *), void *arg, 
			int priority, int flags) {
  struct ve_thread_queue *q;

  veThrMutexLock(gate_mutex);
  if (gate_opened) {
    veThrMutexUnlock(gate_mutex);
    return veThreadInit(NULL,func,arg,priority,flags);
  }
  VE_DEBUG(3,("creating delayed thread"));
  q = fmalloc(sizeof(struct ve_thread_queue));
  q->func = func;
  q->arg = arg;
  q->priority = priority;
  q->flags = flags;
  q->next = thread_queue;
  thread_queue = q;
  veThrMutexUnlock(gate_mutex);
  return 0;
}
