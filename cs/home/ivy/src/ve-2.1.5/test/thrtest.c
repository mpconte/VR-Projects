/* Test program:  explore bug with barrier and kernel level threads on IRIX */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ve_thread.h>

/* #define DBGPRINTF fprintf */
#define DBGPRINTF nothing

void nothing(FILE *f, ...) {
  ;
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void *trigger(void *v) {
  DBGPRINTF(stderr, "%x: trigger start\n", (unsigned)pthread_self());
  while(1) {
    DBGPRINTF(stderr, "%x: pausing\n", (unsigned)pthread_self());
    sleep(2);
    DBGPRINTF(stderr, "%x: acquiring\n", (unsigned)pthread_self());
    veLockMutex(&mutex);
    DBGPRINTF(stderr, "%x: signalling\n", (unsigned)pthread_self());
    veCondBcast(&cond);
    DBGPRINTF(stderr, "%x: releasing\n", (unsigned)pthread_self());
    veUnlockMutex(&mutex);
  }
}

int count = 0;

void *sleeper(void *v) {
  DBGPRINTF(stderr, "%x: sleeper start\n", (unsigned)pthread_self());
  while(1) {
    DBGPRINTF(stderr, "%x: acquiring\n", (unsigned)pthread_self());
    veLockMutex(&mutex);
    count++;
    DBGPRINTF(stderr, "%x: sleeping (total = %d)\n", (unsigned)pthread_self(),
	      count);
    veCondWait(&cond,&mutex);
    count--;
    DBGPRINTF(stderr, "%x: releasing (rem = %d)\n", (unsigned)pthread_self(),
	      count);
    veUnlockMutex(&mutex);
  }
}

VeThrBarrier *B = NULL;

void *barrier(void *v) {
  int i = 0;
  while(1) {
    DBGPRINTF(stderr, "%x: entering\n", (unsigned)pthread_self());
    veThrBarrier(B);
    DBGPRINTF(stderr, "%x: exiting\n", (unsigned)pthread_self());
    i++;
    if (i > 10) {
      fprintf(stderr, "%x: alive\n", (unsigned)pthread_self());
      i = 0;
    }
  }
}

int main(int argc, char **argv) {
  int i, nsleep = 5;
  pthread_t p;
  
  if (argc > 1)
    nsleep = atoi(argv[1]);
  if (nsleep <= 0)
    nsleep = 1;

  B = veCreateThrBarrier(nsleep);
  for(i = 0; i < nsleep; i++)
    p = veStartThread(barrier,NULL,0,1);
  
  pthread_join(p,NULL);
}
