/*
   This is the generic MP renderer.  It deals with thread synchronization
   and dispatching work to the underlying library.
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ve.h>

#define MODULE "ve_mp_render"

/* message types */
#define M_RUN       0x1   /* open windows, start renderer threads running */
#define M_RENDER    0x2   /* start rendering a frame */
#define M_SWAP      0x3   /* display a frame */
#define M_LOC       0x4   /* location information */
#define M_EXIT      0x5   /* shutdown notification */
#define M_WINDOW    0x6   /* assign a window to a slave */

/* *** Master Node Structures *** */

/* information about a slave we are connected to */
typedef struct ve_render_conn {
  int mpid;     /* MP identifier */
  VeThrMutex *mutex;
  int async;  /* if non-zero then we do not wait for response */
  /* slave's current state as reported to master */
  unsigned long active_frame;
  int swapped;
  /* initialization flag - returned by slave when we first start */
  int init;
  int stopped;
} VeRenderConn;

static VeThrMutex *slaves_mutex = NULL;
static unsigned long slave_ready_frame = 0;
static int slave_ready_swapped = 1;
static VeThrCond *slaves_ready_cond = NULL;
static VeThrCond *slaves_stopped_cond = NULL;

static VeRenderConn *slaves = NULL;
static int slave_spc = 0; /* how much space has been allocated... */
static int slave_max = 0;

/* find a slave connection on the master */
static int find_slave(char *node, char *process, int async) {
  int id, k;
  if ((id = veMPGetSlave(node,process,async)) < 0)
    return -1;
  for (k = 0; k < slave_max; k++)
    if (slaves[k].mpid == id) {
      if (slaves[k].async && !async)
	slaves[k].async = 0; /* remove async property */
      return k; /* return offset... */
    }
  /* new slave */
  slave_max++;
  if (slave_max > slave_spc) {
    if (slave_spc == 0)
      slave_spc = slave_max; /* initialize */
    else {
      while (slave_max > slave_spc)
	slave_spc *= 2;
    }
    slaves = veRealloc(slaves,slave_spc*sizeof(VeRenderConn));
    assert(slaves != NULL);
  }
  /* initialize structure */
  slaves[k].mpid = id;
  slaves[k].mutex = veThrMutexCreate();
  slaves[k].async = async;
  slaves[k].active_frame = 0xffffUL;
  slaves[k].swapped = 1;
  slaves[k].init = 0;
  slaves[k].stopped = 0;
  return k;
}

/* *** Slave Node Data *** */

typedef struct ve_render_thread {
  VeThread *thr;                 /* thread pointer */
  struct ve_render_slave *slave; /* slave to which we belong */
  char *name;           /* my thread name (use to identify windows) */
  /* needed for initialization synchronization */
} VeRenderThread;

/* information kept by a rendering slave */
typedef struct ve_render_slave {
  int is_async;  /* if non-zero then slave is asynchronous */
  /* which node/process am I? */
  int nwins;  /* how many windows am I managing */
  int winspc; /* how much space is allocated for window array */
  VeWindow **wins;  /* array of window pointers */
  int nthrs; /* how many threads am I managing */
  VeRenderThread *thrs; /* array of threads */
  VeThrBarrier *b_start, *b_entry, *b_exit, *b_swap;
  /* state */
  unsigned long active_frame;
  int swapped;

  int running; /* am I running? */
} VeRenderSlave;

typedef struct {
  unsigned long frame;
} VeRenderMsg;

#define WNAMELEN 128
typedef struct {
  char name[WNAMELEN];
} VeRenderWinMsg;

typedef struct {
  VeFrame origin;
  VeFrame eye;
} VeRenderLocMsg;

/* condition here is a little complex, but we're testing
   to see if a slave is in the requested state, or a
   *later* state */
static int slave_is_ready(int k) {
  if (slaves[k].async) { /* an asynchronous slave is always ready for more */
    return 1;
  }
  if (slaves[k].active_frame == slave_ready_frame &&
      slaves[k].swapped == slave_ready_swapped) {
    /* slave is in the desired state */
    return 1;
  }
  /* this slave is not ready */
  return 0;
}

static int slaves_are_ready(void) {
  int k;
  for (k = 0; k < slave_max; k++)
    if (!slave_is_ready(k))
      return 0; /* this slave is not ready */
  return 1; /* all slaves ready */
}

static int slaves_are_init(void) {
  int k;
  for (k = 0; k < slave_max; k++)
    if (!slaves[k].init)
      return 0; /* not initialized */
  return 1; /* all initialized */
}

static int slaves_are_stopped(void) {
  int k;
  for (k = 0; k < slave_max; k++)
    if (!slaves[k].stopped)
      return 0; /* not initialized */
  return 1; /* all initialized */
}

static int slaves_send_msg(int mode, int tag, int msg,
			   void *data, int dlen) {
  int k;
  for (k = 0; k < slave_max; k++)
    if (veMPSendMsg(mode,slaves[k].mpid,tag,msg,data,dlen))
      return -1;
  return 0;
}

/* message handler on master */
static void master_render_cback(int k, VeMPPkt *p) {
  VeRenderMsg *msg;

  if (p->msg != VE_MPMSG_RENDER)
    return;

  VE_DEBUGM(4,("master_render_cback: message %d",p->tag));

  veThrMutexLock(slaves_mutex);

  switch (p->tag) {
  case M_EXIT:
    vePfEvent(MODULE,"M:EXIT","slave %d",k);
    slaves[k].stopped = 1;
    if (slaves_are_stopped()) {
      vePfEvent(MODULE,"M:STOPPED",NULL);
      veThrCondBcast(slaves_stopped_cond);
    }
    break;

  case M_RUN: /* indicates initialization complete */
    vePfEvent(MODULE,"M:RUN","slave %d",k);
    slaves[k].init = 1;
    if (slaves_are_init()) {
      vePfEvent(MODULE,"M:READY",NULL);
      veThrCondBcast(slaves_ready_cond);
    }
    break;

  case M_RENDER:
    if (p->dlen != sizeof(VeRenderMsg)) {
      veError(MODULE,"invalid render message (size = %d)",p->dlen);
      return;
    }
    msg = (VeRenderMsg *)(p->data);
    vePfEvent(MODULE,"M:RENDER","slave %d frame %d",k,msg->frame);
    if (msg->frame > slaves[k].active_frame || msg->frame == 0) {
      slaves[k].active_frame = msg->frame;
      slaves[k].swapped = 0;
      if (!slave_ready_swapped && slaves_are_ready()) {
	vePfEvent(MODULE,"M:READY",NULL);
	veThrCondBcast(slaves_ready_cond);
      }
    }
    break;

  case M_SWAP:
    if (p->dlen != sizeof(VeRenderMsg)) {
      veError(MODULE,"invalid render message (size = %d)",p->dlen);
      return;
    }
    msg = (VeRenderMsg *)(p->data);
    vePfEvent(MODULE,"M:SWAP","slave %d frame %d",k,msg->frame);
    /* is this a later state? */
    if ((msg->frame > slaves[k].active_frame || msg->frame == 0) ||
	(msg->frame == slaves[k].active_frame && !slaves[k].swapped)) {
	  slaves[k].active_frame = msg->frame;
	  slaves[k].swapped = 1;
	  if (slave_ready_swapped && slaves_are_ready())
	    veThrCondBcast(slaves_ready_cond);
    }
    break;

  default:
    veWarning(MODULE,"ignoring unexpected render message (%d) from slave %d",
	      p->tag, k);
  }

  veThrMutexUnlock(slaves_mutex);
}

#define SLV(x) ((x)?(x):"auto")

/* a thread that renders... */
static void *render_thread(void *v) {
  VeRenderThread *t = (VeRenderThread *)v;
  VeWindow **wins = NULL;
  int nwins = 0, j, k;

  /* count my windows */
  for(k = 0; k < t->slave->nwins; k++)
    if (strcmp(SLV(t->slave->wins[k]->slave.thread),
	       SLV(t->name)) == 0)
      nwins++;

  VE_DEBUGM(2,("slave picks up %d windows",nwins));

  /* set up window list */
  wins = veAlloc(nwins*sizeof(VeWindow *),1);
  assert(wins != NULL);
  j = 0;
  for(k = 0; k < t->slave->nwins; k++)
    if (strcmp(SLV(t->slave->wins[k]->slave.thread),
	       SLV(t->name)) == 0) {
      assert(j < nwins);
      wins[j] = t->slave->wins[k];
      j++;
    }
  
  VE_DEBUGM(2,("slave entering start barrier"));

  veThrBarrierEnter(t->slave->b_start);
  while (1) {
  VE_DEBUGM(2,("slave entry in"));
    veThrBarrierEnter(t->slave->b_entry);
  VE_DEBUGM(2,("slave entry out"));
    for(k = 0; k < nwins; k++) {
      VE_DEBUGM(4,("rendering window %s",wins[k]->name ?
		   wins[k]->name:"<null>"));
      veRenderWindow(wins[k]);
    }
    VE_DEBUGM(2,("slave exit in"));
    veThrBarrierEnter(t->slave->b_exit);
    VE_DEBUGM(2,("slave exit out"));
    VE_DEBUGM(2,("slave swap in"));
    veThrBarrierEnter(t->slave->b_swap);
    VE_DEBUGM(2,("slave swap out"));
    for(k = 0; k < nwins; k++) {
      VE_DEBUGM(4,("swapping window %s",wins[k]->name ?
		   wins[k]->name:"<null>"));
      veRenderSwap(wins[k]);
    }
  }

  /*NOTREACHED*/
}

/* message handler on slave */

static void slave_render_cback(int src, VeMPPkt *p) {
  static VeRenderSlave me; /* slave information */
  static int running = 1;

  if (p->msg != VE_MPMSG_RENDER)
    return;

  if (!running)
    return; /* message after shutdown */

  VE_DEBUGM(4,("slave_render_cback: message %d",p->tag));

  switch (p->tag) {
  case M_WINDOW:
    {
      /* we have been assigned a particular window */
      VeRenderWinMsg *msg;
      msg = (VeRenderWinMsg *)(p->data);
      /* add this window to my set */
      if (me.nwins >= me.winspc) {
	/* grow... */
	if (me.winspc == 0)
	  me.winspc = 4; /* default */
	else {
	  while (me.nwins >= me.winspc)
	    me.winspc *= 2;
	}
	me.wins = veRealloc(me.wins,sizeof(VeWindow *)*me.winspc);
	assert(me.wins != NULL);
      }
      /* find the window */
      if (!(me.wins[me.nwins] = veFindWindow(veGetEnv(),
					     msg->name)))
	veFatalError("slave_render_cback:  given unknown window '%s'",
		     msg->name);
      VE_DEBUGM(3,("slave_render_cback:  opening window '%s'",
		   me.wins[me.nwins]->name));
      if (veRenderOpenWindow(me.wins[me.nwins]))
	veFatalError("slave_render_cback:  failed to open window '%s'",
		     me.wins[me.nwins]->name);
      me.nwins++;
    }
    break;

  case M_EXIT:
    {
      int k;
      for(k = 0; k < me.nwins; k++)
	veRenderCloseWindow(me.wins[k]);
      running = 0; /* prevent any more updates from coming through */
      /* respond */
      veMPSendMsg(VE_MP_RELIABLE,VE_MPTARG_MASTER,VE_MPMSG_RENDER,
		  M_EXIT,NULL,0);
    }
    break;

  case M_RUN:
    {
      VeEnv *e;
      VeWall *wl;
      VeWindow *w;
      int j, k;
      char *node, *process;

      if (me.running)
	veFatalError(MODULE,"slave_render_cback: received duplicate M_RUN message");
      
      if (me.nwins == 0)
	veFatalError(MODULE,"slave_render_cback: running render thread with no windows");

      /* determine set of threads (and number) */
      /* must be <= nwins */
      me.thrs = veAlloc(me.nwins*sizeof(VeRenderThread),1);
      me.nthrs = 0;
      for(k = 0; k < me.nwins; k++) {
	/* does this thread name exist? */
	for(j = 0; j < me.nthrs; j++) {
	  if (strcmp(SLV(me.wins[k]->slave.thread),me.thrs[j].name) == 0)
	    break;
	}
	if (j >= me.nthrs) {
	  /* thread does not currently exist */
	  me.thrs[me.nthrs].thr = veThreadCreate();
	  me.thrs[me.nthrs].slave = &me;
	  me.thrs[me.nthrs].name = veDupString(SLV(me.wins[k]->slave.thread));
	  me.nthrs++;
	}
      }

      /* create barriers - remember to leave space for message thread */
      me.b_start = veThrBarrierCreate(me.nthrs+1);
      me.b_entry = veThrBarrierCreate(me.nthrs+1);
      me.b_exit = veThrBarrierCreate(me.nthrs+1);
      me.b_swap = veThrBarrierCreate(me.nthrs+1);
      me.swapped = 1;

      /* now start threads */
      /* individual rendering threads should always be high-priority,
	 kernel-level if at all possible */
      for(k = 0; k < me.nthrs; k++)
	veThreadInit(me.thrs[k].thr,render_thread,(void *)&(me.thrs[k]),
		     10,VE_THR_KERNEL);

      /* at this point rendering is ready to go */
      veThrBarrierEnter(me.b_start);
      
      VE_DEBUGM(2,("slave syncing with master at start..."));

      /* sync with master */
      {
	VeRenderMsg m;
	m.frame = 0UL;
	veMPSendMsg(VE_MP_RELIABLE,VE_MPTARG_MASTER,VE_MPMSG_RENDER,
		    M_SWAP,&m,sizeof(m));
	veMPSendMsg(VE_MP_RELIABLE,VE_MPTARG_MASTER,VE_MPMSG_RENDER,
		    M_RUN,NULL,0);
      }
    }
    break;

  case M_RENDER:
    {
      VeRenderMsg *msg;
      VE_DEBUGM(3,("slave_render_cback - rendering frame"));
      if (p->dlen != sizeof(VeRenderMsg)) {
	veError(MODULE,"invalid render message (size = %d)",p->dlen);
	return;
      }
      msg = (VeRenderMsg *)(p->data);
      if ((msg->frame == 0 && me.active_frame != 0)
	  || msg->frame > me.active_frame) {
	if (!me.swapped)
	  veThrBarrierEnter(me.b_swap);

	/* render another frame */
	VE_DEBUGM(2,("msg entry in"));
	veThrBarrierEnter(me.b_entry); /* let threads start */
	VE_DEBUGM(2,("msg entry out"));
	VE_DEBUGM(2,("msg exit in"));
	veThrBarrierEnter(me.b_exit); /* wait for them to finish */
	VE_DEBUGM(2,("msg exit out"));

	me.active_frame = msg->frame;
	me.swapped = 0;
      }
      /* respond with same frame number */
      veMPSendMsg(VE_MP_FAST,src,VE_MPMSG_RENDER,M_RENDER,
		  p->data,p->dlen);
    }
    break;

  case M_SWAP:
    {
      VeRenderMsg *msg;
      VE_DEBUGM(3,("slave_render_cback - swapping frame"));
      if (p->dlen != sizeof(VeRenderMsg)) {
	veError(MODULE,"invalid render message (size = %d)",p->dlen);
	return;
      }
      msg = (VeRenderMsg *)(p->data);
      if ((msg->frame == 0 && me.active_frame != 0) || 
	  msg->frame > me.active_frame ||
	  (msg->frame == me.active_frame && !me.swapped)) {
	if (!me.swapped)
	  veThrBarrierEnter(me.b_swap);
	me.active_frame = msg->frame;
	me.swapped = 1;
      }
      /* respond with same frame number */
      veMPSendMsg(VE_MP_FAST,src,VE_MPMSG_RENDER,M_SWAP,
		  p->data,p->dlen);
    }
    break;

    
  default:
    veError(MODULE,"unexpected render message from master (tag = %d)",
	    p->tag);
  }
}

#define UPD_INTERVAL 5 /* poke slaves every 5 ms */
void veMPRenderFrame(long frame) {
  int k;
  VeRenderMsg msg;

  vePfEvent(MODULE,"render-frame","%d",frame);

  veRenderCallPreCback();

  veThrMutexLock(slaves_mutex);

  if (frame < 0)
    frame = slave_ready_frame+1;
  if (frame < 0) /* possible on overflow... */
    frame = 0;

  msg.frame = frame;

  /* enter render state */
  slave_ready_frame = frame;
  slave_ready_swapped = 0;

  if (slaves_send_msg(VE_MP_FAST,VE_MPMSG_RENDER,M_RENDER,&msg,sizeof(msg)))
    veFatalError(MODULE,"veMPRenderFrame: failed to send RENDER to slaves");

  while (!slaves_are_ready()) {
    veThrCondTimedWait(slaves_ready_cond,slaves_mutex,UPD_INTERVAL);
    for(k = 0; k < slave_max; k++)
      if (!slave_is_ready(k)) {
	/* poke again */
	if (veMPSendMsg(VE_MP_FAST,k,VE_MPMSG_RENDER,M_RENDER,&msg,
			sizeof(msg)))
	  veFatalError(MODULE,"veMPRenderFrame: failed to re-send RENDER to slave %d",k);
      }
  }

  /* enter swapped state */
  slave_ready_swapped = 1;
  if (slaves_send_msg(VE_MP_FAST,VE_MPMSG_RENDER,M_SWAP,&msg,
		      sizeof(msg)))
    veFatalError(MODULE,"veMPRenderFrame: failed to send SWAP to slaves");

  while (!slaves_are_ready()) {
    veThrCondTimedWait(slaves_ready_cond,slaves_mutex,UPD_INTERVAL);
    for(k = 0; k < slave_max; k++)
      if (!slave_is_ready(k)) {
	/* poke again */
	if (veMPSendMsg(VE_MP_FAST,k,VE_MPMSG_RENDER,M_SWAP,&msg,
			sizeof(msg)))
	  veFatalError(MODULE,"veMPRenderFrame: failed to re-send SWAP to slave %d",k);
      }
  }

  veThrMutexUnlock(slaves_mutex);

  veRenderCallPostCback();
}

/* initialize MP rendering interface */
void veMPRenderInit(void) {
  /* on anything other than the master, veMPAddMasterHandler() is a no-op */
  if (veMPIsMaster()) {
    slaves_mutex = veThrMutexCreate();
    slaves_ready_cond = veThrCondCreate();
    slaves_stopped_cond = veThrCondCreate();
  }
  veMPAddMasterHandler(VE_MPMSG_RENDER,VE_DTAG_ANY,master_render_cback);
  veMPAddSlaveHandler(VE_MPMSG_RENDER,VE_DTAG_ANY,slave_render_cback);
}

void veMPRenderRun(void) {
  VeEnv *e;
  VeWall *wl;
  VeWindow *w;
  int k;

  if (!veMPIsMaster())
    return;

  veThrMutexLock(slaves_mutex);

  e = veGetEnv();

  VE_DEBUGM(2,("veMPRenderRun: assigning windows to slaves"));
  for (wl = e->walls; wl; wl = wl->next)
    for (w = wl->windows; w; w = w->next) {
      VeRenderWinMsg msg;
      VE_DEBUGM(3,("veMPRenderRun: finding slave for window %s (%s,%s,%d)",
		   w->name,SLV(w->slave.node),
		   SLV(w->slave.process),w->async));
      if ((k = find_slave(w->slave.node,w->slave.process,w->async)) < 0) {
	veWarning(MODULE,"ignoring failed connection for window %s (async = %d)",
		  w->name,w->async);
	continue;
      }
      assert(slaves != NULL);
      VE_DEBUGM(3,("veMPRenderRun: assigning window %s to slave %d",w->name,k));
      strncpy(msg.name,w->name,WNAMELEN);
      msg.name[WNAMELEN-1] = '\0';
      veMPSendMsg(VE_MP_RELIABLE,slaves[k].mpid,
		  VE_MPMSG_RENDER,M_WINDOW,&msg,sizeof(msg));
    }

  VE_DEBUGM(1,("veMPRenderRun: sending run to slaves"));
  slaves_send_msg(VE_MP_RELIABLE,VE_MPMSG_RENDER,M_RUN,NULL,0);
  VE_DEBUGM(1,("veMPRenderRun: waiting for slaves to be ready"));
  while (!slaves_are_init())
    veThrCondWait(slaves_ready_cond,slaves_mutex);
  VE_DEBUGM(1,("veMPRenderRun: and they're off..."));
  veThrMutexUnlock(slaves_mutex);
}

void veMPRenderExit(void) {
  if (slaves_mutex) {
    veThrMutexLock(slaves_mutex);
    VE_DEBUGM(2,("sending exit to slaves"));
    slaves_send_msg(VE_MP_RELIABLE,VE_MPMSG_RENDER,M_EXIT,NULL,0);
    while (!slaves_are_stopped())
      veThrCondWait(slaves_stopped_cond,slaves_mutex);
    VE_DEBUGM(2,("and they're done..."));
    veThrMutexUnlock(slaves_mutex);
  }
}
