/* Demonstrates bug with PTHREAD_SCOPE_BOUND/PTHREAD_SCOPE_SYSTEM threads
   and condition variables */
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t up = PTHREAD_COND_INITIALIZER, down = PTHREAD_COND_INITIALIZER;

#ifndef PTHREAD_SCOPE_BOUND
#define PTHREAD_SCOPE_BOUND PTHREAD_SCOPE_SYSTEM
#endif /* PTHREAD_SCOPE_BOUND */

int max = 3; /* number of threads to work with */
int count = 0;
int going_up = 1;

void *sleeper(void *v) {
  int i = 0;
  pthread_mutex_lock(&mutex);
  while(1) {
      fprintf(stderr, "%x: alive\n", (unsigned)pthread_self());

    count++;
    if (count >= max) {
      going_up = 0;
      pthread_cond_broadcast(&up);
    } else
      while(going_up)
	pthread_cond_wait(&up,&mutex);
    
    count--;
    if (count <= 0) {
      going_up = 1;
      pthread_cond_broadcast(&down);
    } else
      while(!going_up)
	pthread_cond_wait(&down,&mutex);

    i++;
    if (i > 3) { /* heartbeat so we know when it hangs */
      fprintf(stderr, "%x: alive\n", (unsigned)pthread_self());
      i = 0;
    }
  }
  pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv) {
  int i, err;
  pthread_t p;
  pthread_attr_t a;
  int scope = PTHREAD_SCOPE_BOUND; /* demonstrate bug by default */

  if (argc > 1) {
    switch(atoi(argv[1])) {
    case 0:
      scope = PTHREAD_SCOPE_PROCESS;
      break;
    case 1:
      scope = PTHREAD_SCOPE_BOUND;
      break;
    case 2:
      scope = PTHREAD_SCOPE_SYSTEM;
      break;
    }
  }
  
  for(i = 0; i < max; i++) {
    pthread_attr_init(&a);
    if (err = pthread_attr_setscope(&a,scope)) {
      fprintf(stderr, "pthread_attr_setscope: %s\n", strerror(err));
      exit(1);
    }
    pthread_create(&p,&a,sleeper,NULL);
  }

  pthread_join(p,NULL); /* join the last thread so we run forever */
}
