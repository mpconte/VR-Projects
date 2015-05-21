#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ve_env.h>
#include <ve_core.h>
#include <ve_error.h>
#include <ve_util.h>
#include <ve_debug.h>
#include <ve_thread.h>
#include <ve_render.h>
#include <ve_mp.h>

#define MODULE "ve_mp"

/* Note that we have our own internal debugging switch macro here
   This is because some of this code will get executed before we parse
   command-line options or inspect environment variables so the usual
   VE debugging macros don't work */
#define VEMP_DEBUG 0

/* the following variables are specific to this process */
/* since processes run in separate memory spaces, this is easy */
static int vemp_id = 0;
static int vemp_is_master = 1; /* by default I am the master... */
static int vemp_argc = 0;
static int vemp_is_async = 0; /* if I'm a slave, am I running 
				 in asynchronous mode? */
static char **vemp_argv = NULL;
static char **vemp_async_argv = NULL;

static char *vemp_slave_opt = "-vemp_slave";
static char *vemp_async_opt = "-vemp_async";
static char *vemp_unique_name = "unique";

/* slave values */
static int vemp_recv = -1, vemp_send = -1;
static VeStrMap vemp_nodes;

static void statevar_handler(int msg, int tag, void *data, int dlen); /* see below */

static struct vemp_data_handler {
  int tag;
  VeMPDataHandler handler;
  struct vemp_data_handler *next;
} *dhandlers = NULL, *dh_tail = NULL;

static struct vemp_int_handler {
  int msg;
  int tag;
  VeMPIntDataHandler handler;
  struct vemp_int_handler *next;
} *ihandlers = NULL, *ih_tail = NULL;

typedef struct vemp_loc_msg {
  VeFrame origin;
  VeFrame eye;
} VeMPLocMsg;

#define MAX_SLAVES 256
typedef struct vemp_slave {
  int inuse;
  int id;
  int recv_h, send_h;
  char *node;
  char *process;
  int async;  /* if non-zero then do not wait for response from
		 slave */
  /* the is just a reference into vemp_nodes so we should not
     allocate/deallocate it here */
  VeStrMap wtable;  /* table of all windows that are actually ours */
  VeThrMutex *mutex;
} VeMPSlave;
static VeMPSlave slaves[MAX_SLAVES];
static int slave_max = 0;

/* (1) Do I need these? and (2) If I do, where do they get called from? */
void veMPLockSlave(int k) {
  assert(k >= 0 && k < slave_max && slaves[k].inuse && slaves[k].mutex);
  veThrMutexLock(slaves[k].mutex);
}

void veMPUnlockSlave(int k) {
  assert(k >= 0 && k < slave_max && slaves[k].inuse && slaves[k].mutex);
  veThrMutexUnlock(slaves[k].mutex);
}

static void *msg_thread(void *x) {
  int msg, tag, dlen;
  void *data;
  struct vemp_data_handler *dh;
  struct vemp_int_handler *ih;

  VE_DEBUG(5,("msg_thread starting for process",veMPId()));
  
  while (veMP_OSRecv(vemp_recv,&msg,&tag,&data,&dlen) == 0) {
    VE_DEBUG(3,("received MP message (%d,%d)",veMPId(),msg,tag));

    /* look for a general handler first... */
    for(ih = ihandlers; ih; ih = ih->next)
      if ((ih->msg == msg || ih->msg == VE_DMSG_ANY) &&
	  (ih->tag == tag || ih->tag == VE_DTAG_ANY))
	break;
    if (ih) {
      VE_DEBUG(5,("msg_thread passing msg (%d,%d) to handler 0x%x",
		  msg, tag, (unsigned)(ih->handler)));
      ih->handler(msg,tag,data,dlen);
      if (data)
	free(data);
      continue;
    }

    VE_DEBUG(5,("msg_thread trying default handlers"));

    /* try default handlers... */
    switch (msg) {
    case VE_MPMSG_DATA:
      for(dh = dhandlers; dh; dh = dh->next)
	if (dh->tag == tag || dh->tag == VE_DTAG_ANY)
	  break;
      if (dh)
	dh->handler(tag,data,dlen);
      else 
	veError(MODULE,"unhandled data message tag: %d",tag);
      break;

    case VE_MPMSG_LOCATION:
      {
	VeMPLocMsg *msg = (VeMPLocMsg *)data;
	*(veGetOrigin()) = msg->origin;
	*(veGetDefaultEye()) = msg->eye;
      }
      break;

    case VE_MPMSG_ENV:
      {
	FILE *f = tmpfile();
	assert(f != NULL);
	fwrite(data,dlen,1,f);
	rewind(f);
	veSetEnv(veEnvRead(f,0));
	fclose(f);
      }
      break;
    case VE_MPMSG_PROFILE:
      {
	FILE *f = tmpfile();
	assert(f != NULL);
	fwrite(data,dlen,1,f);
	rewind(f);
	veSetProfile(veProfileRead(f,0));
	fclose(f);
      }
      break;
    case VE_MPMSG_RECONNECT:
      {
	veMP_OSReconnect(tag,data,dlen,&vemp_recv,&vemp_send);
      }
      break;
    case VE_MPMSG__SYSDEP:
      {
	veMP_OSSysdep(tag,data,dlen,vemp_recv,vemp_send);
      }
      break;
    default:
      veError(MODULE,"unhandled message type: %d",msg);
    }
    if (data)
      free(data);
  }
  VE_DEBUG(1,("[%2x] message thread finishing",veMPId()));
  veMP_OSClose(vemp_recv);
  vemp_recv = -1;
  return NULL;
}

/* callback from OS layer */
void veMPSetSlaveProp(int id, int recv_h, int send_h) {
  VE_DEBUG(3,("veMPSetSlaveProp(%d,%d,%d)",id,recv_h,send_h));
  vemp_id = id;
  vemp_recv = recv_h;
  vemp_send = send_h;
  if (veMPIsMaster())
    veThreadInit(NULL,msg_thread,NULL,0,0);
}

int veMPSlaveInit(int *argc, char **argv) {
  /* Look for the "-vemp_slave" option.  If it is there,
     it needs to be first. */
  int k;
#if VEMP_DEBUG
  fprintf(stderr,"veMPSlaveInit - called\n");
#endif

  if (*argc > 2 && (strcmp(argv[1],vemp_slave_opt) == 0)) {
#if VEMP_DEBUG
    fprintf(stderr,"veMPSlaveInit - not the master\n");
#endif
    if (sscanf(argv[2],"%d",&vemp_id) != 1)
      veFatalError(MODULE,"invalid argument to %s option: %s",
		   vemp_slave_opt, argv[2]);
#if VEMP_DEBUG
    fprintf(stderr,"veMPInit - I am slave %d\n",vemp_id);
#endif
    vemp_is_master = 0; /* we're not the master */
    {
      char debugstr[80];
      sprintf(debugstr,"[slave %2d]",vemp_id);
      veSetDebugPrefix(debugstr);
    }
    /* strip the '-vemp_slave x' options out of the set */
    (*argc) -= 2;
    for(k = 1; k < (*argc); k++)
      argv[k] = argv[k+2];
    argv[k] = NULL;
    /* now look for async option */
    if (*argc > 1 && (strcmp(argv[1],vemp_async_opt) == 0)) {
#if VEMP_DEBUG
      fprintf(stderr, "veMPSlaveInit - running asynchronous\n");
#endif
      vemp_is_async = 1;
      (*argc)--;
      for(k = 1; k < (*argc); k++)
	argv[k] = argv[k+1];
      argv[k] = NULL;
    } 

    /* set-up communication channels */
#if VEMP_DEBUG
    fprintf(stderr,"[%2x] setting up comm with master\n",veMPId());
#endif
    if (veMP_OSSlaveInit(argc,argv,&vemp_recv,&vemp_send))
      veFatalError(MODULE,"[%2d] failed to establish connection with master",
		   vemp_id);
#if VEMP_DEBUG
    fprintf(stderr,"[%2x] vemp_recv=%d vemp_send=%d\n",veMPId(),vemp_recv,vemp_send);
#endif
  } else {
    /* master - we'll setup our recv/send handles later */
    /* build argc/argv lists */
#if VEMP_DEBUG
    fprintf(stderr,"veMPSlaveInit - master\n");
#endif
    vemp_argc = (*argc)+2;
    vemp_argv = malloc((vemp_argc+1)*sizeof(char *));
    vemp_async_argv = malloc((vemp_argc+2)*sizeof(char *));
    assert(vemp_argv != NULL);
    vemp_argv[0] = veDupString(argv[0]);
    vemp_argv[1] = veDupString(vemp_slave_opt);
    vemp_argv[2] = NULL; /* fill in later when spawning slaves */
    for(k = 1; k < *argc; k++)
      vemp_argv[k+2] = veDupString(argv[k]);
    vemp_argv[vemp_argc] = NULL;

    vemp_async_argv[0] = veDupString(argv[0]);
    vemp_async_argv[1] = veDupString(vemp_slave_opt);
    vemp_async_argv[2] = NULL; /* fill in later when spawning slaves */
    vemp_async_argv[3] = veDupString(vemp_async_opt);
    for(k = 1; k < *argc; k++)
      vemp_async_argv[k+3] = veDupString(argv[k]);
    vemp_async_argv[vemp_argc+1] = NULL;
  }
  return 0;
}

int veMPInit(void) {
  VeStrMap nsm, psm, sm;
  VeStrMapWalk nw, pw, wk;
  VeWall *wl;
  VeWindow *w;
  char unq[80];
  int unqc = 0, k;
  VeEnv *env;
  static int called = 0;

  if (called) {
    VE_DEBUG(5,("veMPInit - short-circuit - multiple calls"));
    return 0;
  }

  /* set-up an exit callback so that if we blowup, connections get closed */
  atexit(veMPCleanup);

  VE_DEBUG(5,("veMPInit enter"));

  /* add internal handler for state variables */
  veMPAddIntHandler(VE_MPMSG_STATE,VE_DTAG_ANY,statevar_handler);

  if (!vemp_is_master) {
    /* set-up incoming message thread */
    VE_DEBUG(5,("veMPInit - slave message thread starting"));
    veThreadInit(NULL,msg_thread,NULL,0,0);
    called = 1; /* guard against second call */
    return 0;
  }

  /* The rest of this function is master-only... */

  /* try to fire up the slaves... */
  /* determine the size of the set of slaves... */
  VE_DEBUG(5,("veMPInit - calling RenderAllocMP"));
  veRenderAllocMP(); /* let the renderer work out what it can/wants */

  if (!(env = veGetEnv()))
    veFatalError(MODULE,"no environment defined");

  VE_DEBUG(5,("veMPInit - building node/process/window set"));
  vemp_nodes = veStrMapCreate();
  for(wl = env->walls; wl; wl = wl->next)
    for(w = wl->windows; w; w = w->next) {
      char *nd,*pc;
      nd = w->slave.node ? w->slave.node : "auto"; /* default */
      if (!(nsm = (VeStrMap)veStrMapLookup(vemp_nodes,nd))) {
	nsm = veStrMapCreate();
	veStrMapInsert(vemp_nodes,nd,nsm);
      }
      pc = w->slave.process ? w->slave.process : "auto"; /* default */
      if (!(psm = (VeStrMap)veStrMapLookup(nsm,pc))) {
	psm = veStrMapCreate();
	veStrMapInsert(nsm,pc,psm);
      }
      veStrMapInsert(psm,w->name,(void *)w);
    }
  /* Okay - nodes is a stringmap which maps node names to a stringmap
     containing the set of processes that should run on that node.
     The process maps in turn contain the set of windows that will run
     in that process.
     Beware of the "unique" name in each process map as it contains the
     set of windows that should run in a unique, but unspecified
     process */
  /* start by cleaning up any "unique" entries - we do this by taking
     all windows in the "unique" set and creating new processes for them
     with unique names */
  VE_DEBUG(5,("veMPInit - resolving unique names"));
  nw = veStrMapWalkCreate(vemp_nodes);
  if (veStrMapWalkFirst(nw) == 0) {
    do {
      nsm = (VeStrMap)veStrMapWalkObj(nw); /* get this node's strmap */
      if ((psm = (VeStrMap)veStrMapLookup(nsm,vemp_unique_name))) {
	/* psm is now the set of processes which should be unique */
	pw = veStrMapWalkCreate(psm);
	if (veStrMapWalkFirst(pw) == 0) {
	  do {
	    /* find a new unique name for this process */
	    sprintf(unq,"uniq%d",unqc);
	    while (veStrMapLookup(nsm,unq)) {
	      /* rely on sanity for this loop to end... */
	      unqc++;
	      sprintf(unq,"uniq%d",unqc);
	    }
	    unqc++;
	    sm = veStrMapCreate();
	    veStrMapInsert(sm,veStrMapWalkStr(pw),veStrMapWalkObj(pw));
	    veStrMapInsert(nsm,unq,sm);
	  } while (veStrMapWalkNext(pw) == 0);
	}
	veStrMapWalkDestroy(pw);
	veStrMapDestroy(psm,NULL);
	veStrMapDelete(nsm,vemp_unique_name);
      }
    } while (veStrMapWalkNext(nw) == 0);
  }
  veStrMapWalkDestroy(nw);

  /* okay - all "unique" entries have been resolved - we can now start
     putting together all of our nodes... */

  VE_DEBUG(5,("veMPInit - spawning slave processes"));
  /* treat "auto" as any other process - the special case 
     node=auto, process=auto is handled by the OS layer */
  nw = veStrMapWalkCreate(vemp_nodes);
  if (veStrMapWalkFirst(nw) == 0) {
    do {
      pw = veStrMapWalkCreate((VeStrMap)veStrMapWalkObj(nw));
      if (veStrMapWalkFirst(pw) == 0) {
	do {
	  char str[80];
	  int async = -1;
	  
	  /* find me a slave spot */
	  for(k = 0; k < MAX_SLAVES; k++)
	    if (!slaves[k].inuse)
	      break;
	  if (k >= MAX_SLAVES)
	    veFatalError(MODULE,"too many slaves - max = %d",MAX_SLAVES);

	  slaves[k].inuse = 1;
	  slaves[k].id = k;
	  slaves[k].wtable = (VeStrMap)veStrMapWalkObj(pw);
	  slaves[k].node = veDupString(veStrMapWalkStr(nw));
	  slaves[k].process = veDupString(veStrMapWalkStr(pw));

	  /* walk through all assigned windows - iff all are marked
	     async, then make the client async */
	  wk = veStrMapWalkCreate(slaves[k].wtable);
	  if (veStrMapWalkFirst(wk) == 0) {
	    do {
	      w = veStrMapWalkObj(wk);
	      if (w) {
		if (w->async && async < 0)
		  async = 1;
		else if (!w->async)
		  async = 0;
	      }
	    } while (veStrMapWalkNext(wk) == 0);
	  }
	  veStrMapWalkDestroy(wk);

	  slaves[k].mutex = veThrMutexCreate();
	  slaves[k].async = (async > 0) ? 1 : 0;
	  sprintf(str,"%d",slaves[k].id);
	  /* !!! note hard-coded offset here - if you fiddle with
	     veMPSlaveInit() fix this value !!! */
	  if (async) {
	    vemp_async_argv[2] = str;
	    VE_DEBUG(5,("veMPInit - spawning async slave %d (%s,%s)",k,
			slaves[k].node, slaves[k].process));
	  } else {
	    vemp_argv[2] = str;
	    VE_DEBUG(5,("veMPInit - spawning slave %d (%s,%s)",k,
			slaves[k].node, slaves[k].process));
	  }
	  if (k >= slave_max)
	    slave_max = k+1; /* for convenience later */

	  if (veMP_OSSlaveSpawn(k,NULL,slaves[k].node,slaves[k].process,
				(async ? vemp_argc+1 : vemp_argc),
				(async ? vemp_async_argv : vemp_argv),
				&(slaves[k].recv_h),
				&(slaves[k].send_h)))
	    veFatalError(MODULE,"failed to spawn node %s process %s",
			 slaves[k].node,slaves[k].process);

	  /* put off initialization for now to give multiple processes
	     a chance to catch up and initialize in parallel */
	} while (veStrMapWalkNext(pw) == 0);
      }
      veStrMapWalkDestroy(pw);
    } while (veStrMapWalkNext(nw) == 0);
  }
  veStrMapWalkDestroy(nw);

  VE_DEBUG(5,("veMPInit - initializing slaves"));

  for(k = 0; k < MAX_SLAVES; k++)
    if (slaves[k].inuse) {
      /* Now do some initialization... */
      /* tell it the windows that the process needs to take care
	 of and let the renderer create the appropriate threads */
      /* reconnect if necessary */
      VE_DEBUG(5,("veMPInit - preparing slave %d",k));
      if (veMP_OSSlavePrepare(k,NULL,slaves[k].node,slaves[k].process,
			      &(slaves[k].recv_h),&(slaves[k].send_h)))
	veFatalError(MODULE,"failed to prepare slave %d",k);
      VE_DEBUG(5,("veMPInit - initializing slave %d",k));
      wk = veStrMapWalkCreate(slaves[k].wtable);
      if (veStrMapWalkFirst(wk) == 0) {
	do {
	  veMPSendCtrl(k,VE_MPCTRL_WINDOW,veStrMapWalkStr(wk));
	} while (veStrMapWalkNext(wk) == 0);
      }
      veStrMapWalkDestroy(wk);
    }

  VE_DEBUG(5,("veMPInit complete"));
  called = 1; /* guard against a second call... */
  return 0;
}


int veMPDataAddHandler(int tag, VeMPDataHandler handler) {
  struct vemp_data_handler *h;
  h = malloc(sizeof(struct vemp_data_handler));
  assert(h != NULL);
  h->tag = tag;
  h->handler = handler;
  h->next = NULL;
  if (dh_tail) {
    dh_tail->next = h;
    dh_tail = h;
  } else {
    dhandlers = dh_tail = h;
  }
  return 0;
}

int veMPDataPush(int tag, void *data, int dlen) {
  if (veMPTestSlaveGuard())
    return 0;
  return veMPIntPush(VE_MPTARG_ALL,VE_MPMSG_DATA,tag,data,dlen);
}

int veMPLocationPush(void) {
  VeMPLocMsg m;
  if (veMPTestSlaveGuard())
    return 0;
  m.origin = *(veGetOrigin());
  m.eye = *(veGetDefaultEye());
  return veMPIntPush(VE_MPTARG_ALL,VE_MPMSG_LOCATION,0,&m,sizeof(m));
}

int veMPAddIntHandler(int msg, int tag, VeMPIntDataHandler handler) {
  struct vemp_int_handler *h;
  h = malloc(sizeof(struct vemp_int_handler));
  assert(h != NULL);
  h->msg = msg;
  h->tag = tag;
  h->handler = handler;
  h->next = NULL;
  if (ih_tail) {
    ih_tail->next = h;
    ih_tail = h;
  } else {
    ihandlers = ih_tail = h;
  }
  return 0;
}

int veMPIntPush(int target, int msg, int tag, void *data, int dlen) {
  if (target == VE_MPTARG_ALL) {
    int k;
    VE_DEBUG(5,("MPIntPush(all,%d,%d) %d bytes",msg,tag,dlen));
    for(k = 0; k < slave_max; k++) {
      if (slaves[k].inuse) {
	veThrMutexLock(slaves[k].mutex);
	if (veMP_OSSend(slaves[k].send_h,msg,tag,data,dlen)) {
	  veThrMutexUnlock(slaves[k].mutex);
	  veError(MODULE,"OSSend(target=%d,msg=%d,tag=%d) - %d (%s)",
		  k,msg,tag,errno,strerror(errno));
	  return -1;
	}
	veThrMutexUnlock(slaves[k].mutex);
      }
    }
    return 0;
  } else {
    int k;
    VE_DEBUG(5,("MPIntPush(%d,%d,%d) %d bytes",target,msg,tag,dlen));
    if (target >= 0 && target < slave_max && slaves[target].inuse) {
      veThrMutexLock(slaves[target].mutex);
      if ((k = veMP_OSSend(slaves[target].send_h,msg,tag,data,dlen))) {
	veError(MODULE,"OSSend(target=%d,msg=%d,tag=%d) - %d (%s)",
		k,msg,tag,errno,strerror(errno));
      } 
      veThrMutexUnlock(slaves[target].mutex);
      return k;
    } else {
      veWarning(MODULE,"veMPIntPush: target %d is not valid (inuse=%c)",
		target,
		(target >= 0 && target < slave_max) ? (slaves[target].inuse ? '1' : '0') : '?');
      return -1;
    }
  }
}

int veMPIsMaster(void) {
  return vemp_is_master;
}

int veMPNumProcesses(void) {
  return slave_max;
}

int veMPId(void) {
  return vemp_id;
}

int veMPIsAsync(void) {
  return vemp_is_async;
}

void veMPCleanup(void) {
  veMP_OSCleanup();
}

int veMPSendCtrl(int target, int tag, char *arg) {
  VeMPCtrlMsg msg;
  strncpy(msg.arg,(arg ? arg : ""),VE_MPCTRL__ARGSZ);
  msg.arg[VE_MPCTRL__ARGSZ-1] = '\0';
  VE_DEBUG(3,("veMPSendCtrl(%d,%d,'%s')",target,tag,msg.arg));
  return veMPIntPush(target,VE_MPMSG_CTRL,tag,(void *)&msg,sizeof(msg));
}

int veMPRespond(int result) {
  VeMPRespMsg msg;
  msg.result = result;
  veMP_OSSend(vemp_send,VE_MPMSG_RESPONSE,0,(void *)&msg,sizeof(msg));
  return 0;
}

int veMPGetResponse(int target) {
  int msg, tag, dlen;
  void *data;
  VeMPRespMsg *resp;
  int result = 0, k;

  if (veMPTestSlaveGuard())
    return 0;
  
  if (target == VE_MPTARG_ALL) {
    VE_DEBUG(5,("veMPGetResponse(all)"));
    for(k = 0; k < slave_max; k++) {
      if (slaves[k].inuse && !slaves[k].async) {
	if (veMP_OSRecv(slaves[k].recv_h,&msg,&tag,&data,&dlen))
	  veFatalError(MODULE,"failed to read response from slave %d",k);
	if (dlen != sizeof(VeMPRespMsg))
	  veFatalError(MODULE,"exepcted response (%d bytes) from slave %d - got %d bytes",sizeof(VeMPRespMsg),k,dlen);
	assert(data != NULL);
	resp = (VeMPRespMsg *)data;
	if (resp->result)
	  result = -1;
	free(data);
      }
    }
  } else {
    if (slaves[target].async) {
      VE_DEBUG(5,("veMPGetResponse(%d) [async-nop]",target));
      result = 0; /* silently succeed */
    } else {
      VE_DEBUG(5,("veMPGetResponse(%d)",target));
      if (veMP_OSRecv(slaves[target].recv_h,&msg,&tag,&data,&dlen))
	veFatalError(MODULE,"failed to read response from slave %d",target);
      if (dlen != sizeof(VeMPRespMsg))
	veFatalError(MODULE,"exepcted response (%d bytes) from slave %d - got %d bytes",sizeof(VeMPRespMsg),target,dlen);
      assert(data != NULL);
      result = ((VeMPRespMsg *)data)->result;
      free(data);
    }
  }
  return result;
}

/* slave guard - by default it is on */
static int slave_guard = 1;
int veMPGetSlaveGuard(void) {
  return slave_guard;
}

int veMPTestSlaveGuard(void) {
  return slave_guard && !veMPIsMaster();
}

void veMPSetSlaveGuard(int state) {
  slave_guard = (state ? 1 : 0);
}

/* state variables */
/* the number of state variables in an app should be small,
   so we'll just go with a linear list */
static struct vemp_statevar {
  int tag;
  void *var;
  int vlen;
  int flags;
  struct vemp_statevar *next;
} *statevars = NULL;

int veMPAddStateVar(int tag, void *var, int vlen, int flags) {
  struct vemp_statevar *v;
  if (tag < 0)
    return -1; /* tag must be >= 0 */
  if (!var || vlen <= 0)
    return -1; /* invalid args */
  for(v = statevars; v; v = v->next) {
    if (v->tag == tag) {
      /* update this one */
      v->var = var;
      v->vlen = vlen;
      v->flags = flags;
      return 0;
    }
  }
  /* add a new element to the list */
  v = malloc(sizeof(struct vemp_statevar));
  assert(v != NULL);
  v->tag = tag;
  v->var = var;
  v->vlen = vlen;
  v->flags = flags;
  v->next = statevars;
  statevars = v;
  return 0;
}

int veMPPushStateVar(int tag, int flags) {
  struct vemp_statevar *v;
  int isauto = (flags & VE_MP_AUTO);
  if (veMPTestSlaveGuard())
    return 0;
  for(v = statevars; v; v = v->next) {
    if ((tag == VE_DTAG_ANY || tag == v->tag) &&
	(!isauto || (v->flags & VE_MP_AUTO))) {
      veMPIntPush(VE_MPTARG_ALL,VE_MPMSG_STATE,v->tag,
		  v->var,v->vlen);
    }
  }
  return 0;
}

static void statevar_handler(int msg, int tag, void *data, int dlen) {
  if (msg == VE_MPMSG_STATE) {
    struct vemp_statevar *v;
    for(v = statevars; v; v = v->next) {
      if (tag == v->tag) {
	int min = (dlen < v->vlen ? dlen : v->vlen);
	assert(v->var != NULL);
	assert(min > 0); /* we should never send 0 bytes of state */
	memcpy(v->var,data,min);
	break;
      }
    }
  }
}
