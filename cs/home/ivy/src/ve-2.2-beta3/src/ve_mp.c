#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <ve_alloc.h>
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

static void VEMP_DEBUGMSG(char *fmt, ...) {
#if VEMP_DEBUG
  va_list ap;
  va_start(ap,fmt);
  fprintf(stderr,"[MP DEBUG] ");
  vfprintf(stderr,fmt,ap);
  va_end(ap);
#endif /* VEMP_DEBUG */
}

/* the following variables are specific to this process */
/* since processes run in separate memory spaces, this is easy */
static int vemp_id = 0;
static int vemp_is_master = 1; /* by default I am the master... */
static int vemp_argc = 0;
/* the following are only filled in on a slave process... */
static char *vemp_slave_node = NULL;
static char *vemp_slave_process = NULL;
static char **vemp_argv = NULL;
static char *vemp_slave_opt = "-vemp_slave";
static char *vemp_unique_name = "unique";
#define VEMP_SLAVE_ARG 2

/* slave values */
static VeMPImplConn vemp_conn = NULL; /* my connection */
static VeStrMap vemp_nodes;

/* message for initializing slaves */
#define INIT_NAMESZ 128
typedef struct ve_mp_init_msg {
  char process[INIT_NAMESZ];
  char node[INIT_NAMESZ];
} VeMPInitMsg;

static void statevar_handler(int src, VeMPPkt *p);

static struct vemp_data_handler {
  int tag;
  VeMPDataHandler handler;
  struct vemp_data_handler *next;
} *dhandlers = NULL, *dh_tail = NULL;

static struct vemp_int_handler {
  int msg;
  int tag;
  VeMPPktHandler handler;
  struct vemp_int_handler *next;
} *shandlers = NULL, *sh_tail = NULL, *mhandlers = NULL, *mh_tail = NULL;

typedef struct vemp_loc_msg {
  VeFrame origin;
  VeFrame eye;
} VeMPLocMsg;

static char *mkunique(void) {
  static int cnt = 0;
  char str[128];
  sprintf(str,"_unique%d",cnt);
  cnt++;
  return veDupString(str);
}

/* An arbitrary number - likely already too big for any practical
   use.  Prove me wrong, kids - prove me wrong. */
#define MAX_SLAVES 256
typedef struct vemp_slave {
  int inuse;
  int id;
  VeMPImplConn conn;
  char *method;  /* type of slave to create for this one... */
  char *node;    /* NULL implies unique */
  char *process; /* NULL implies unique */

  /* the is just a reference into vemp_nodes so we should not
     allocate/deallocate it here */
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

static VeThrMutex *slave_msg_mutex = NULL; /* synchronize slave msg handlers */

/* handler for messages from one slave */
static void *slave_msg_thread(void *x) {
  /* handle messages returned from slave */
  VeMPImplConn c, c_res;
  VeMPPkt *p;
  int k = (int)x;

  /* create initial set */
  c = slaves[k].conn;
  
  while (veMPImplWait(&c,&c_res,1) == 0) {
    /* process all pending connections */
    if (c_res) {
      struct vemp_int_handler *mh = NULL;

      if (veMPImplRecv(c_res,&p,-1))
	veFatalError(MODULE,"failed to read message from slave %d",
		     slaves[k].id);
      /* look for a handler */
      for(mh = mhandlers; mh; mh = mh->next)
	if ((mh->msg == p->msg || mh->msg == VE_DMSG_ANY) &&
	    (mh->tag == p->tag || mh->tag == VE_DTAG_ANY))
	    break;
      if (mh) {
	VE_DEBUGM(5,("slave_msg_thread passing msg (%d,%d) to handler 0x%x",
		     p->msg, p->tag, (unsigned)(mh->handler)));
	mh->handler(k,p);
      } else {
	veWarning(MODULE,"unhandled message from slave %d (%d,%d)",
		  k,p->tag,p->msg);
      }
      veMPPktDestroy(p);
    }
  }
  veFatalError(MODULE,"failed to wait on connection %d: %s",k,veSysError());
  return NULL;
}

static void *msg_thread(void *x) {
  VeMPImplConn conn = (VeMPImplConn)x;
  VeMPPkt *p;
  struct vemp_data_handler *dh;
  struct vemp_int_handler *sh;

  VE_DEBUGM(5,("msg_thread starting for process",veMPId()));
  
  /* do not do anything with timeouts for now... */
  while (veMPImplRecv(conn,&p,-1) == 0) {
    VE_DEBUGM(3,("received MP message (%d,%d)",veMPId(),p->msg,p->tag));

    /* look for a general handler first... */
    for(sh = shandlers; sh; sh = sh->next)
      if ((sh->msg == p->msg || sh->msg == VE_DMSG_ANY) &&
	  (sh->tag == p->tag || sh->tag == VE_DTAG_ANY))
	break;
    if (sh) {
      VE_DEBUGM(5,("msg_thread passing msg (%d,%d) to handler 0x%x",
		  p->msg, p->tag, (unsigned)(sh->handler)));
      sh->handler(VE_MPTARG_MASTER,p);
      veMPPktDestroy(p);
      continue;
    }

    VE_DEBUGM(5,("msg_thread trying default handlers"));

    /* try default handlers... */
    switch (p->msg) {
    case VE_MPMSG_DATA:
      for(dh = dhandlers; dh; dh = dh->next)
	if (dh->tag == p->tag || dh->tag == VE_DTAG_ANY)
	  break;
      if (dh)
	dh->handler(p->tag,p->data,p->dlen);
      else 
	veError(MODULE,"unhandled data message tag: %d",p->tag);
      break;

    case VE_MPMSG_LOCATION:
      if (!veMPIsMaster()) {
	VeMPLocMsg *msg = (VeMPLocMsg *)(p->data);
	*(veGetOrigin()) = msg->origin;
	*(veGetDefaultEye()) = msg->eye;
      }
      break;

    case VE_MPMSG_ENV:
      if (!veMPIsMaster()) {
	FILE *f = tmpfile();
	assert(f != NULL);
	VE_DEBUGM(2,("received environment message"));
	fwrite(p->data,p->dlen,1,f);
	rewind(f);
	veBlueEvalStream(f);
	fclose(f);
	VE_DEBUGM(2,("finished environment message"));
      }
      break;
    case VE_MPMSG_PROFILE:
      if (!veMPIsMaster()) {
	FILE *f = tmpfile();
	assert(f != NULL);
	VE_DEBUGM(2,("received profile message"));
	fwrite(p->data,p->dlen,1,f);
	rewind(f);
	veBlueEvalStream(f);
	fclose(f);
	VE_DEBUGM(2,("finished profile message"));
      }
      break;
    case VE_MPMSG__SYSDEP:
      veMPImplSysdep(conn,p);
      break;
    default:
      veError(MODULE,"unhandled message type: %d",p->msg);
    }
    if (p)
      veMPPktDestroy(p);
  }
  VE_DEBUGM(1,("[%2x] message thread finishing",veMPId()));
  veMPImplDestroy(conn);
  if (conn == vemp_conn)
    vemp_conn = NULL;
  return NULL;
}

void veMPCreateSlave(VeMPImplConn c) {
  /* create a thread to listen to messages... */
  veThreadInit(NULL,msg_thread,c,0,0);
  if (!vemp_conn)
    vemp_conn = c;
}

int veMPSlaveInit(int *argc, char **argv) {
  /* Look for the "-vemp_slave" option.  If it is there,
     it needs to be first. */
  int k;

  VEMP_DEBUGMSG("veMPSlaveInit - called\n");

  if (*argc > 2 && (strcmp(argv[1],vemp_slave_opt) == 0)) {

    VEMP_DEBUGMSG("veMPSlaveInit - not the master\n");

    if (sscanf(argv[2],"%d",&vemp_id) != 1)
      veFatalError(MODULE,"invalid argument to %s option: %s",
		   vemp_slave_opt, argv[2]);
    VEMP_DEBUGMSG("veMPInit - I am slave %d\n",vemp_id);

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

    /* set-up communication channels */
    VEMP_DEBUGMSG("[%2x] setting up comm with master\n",veMPId());

    if (!(vemp_conn = veMPImplSlaveCreate(argc,argv)))
      veFatalError(MODULE,"[%2d] failed to establish connection with master",
		   vemp_id);

    /* first message we receive (at the upper layer) should be init */
    {
      VeMPInitMsg *m;
      VeMPPkt *p;
      if (veMPImplRecv(vemp_conn,&p,-1))
	veFatalError(MODULE,"[%2d] failed to receive initialization packed",
		     vemp_id);
      if (p->msg != VE_MPMSG_INIT)
	veFatalError(MODULE,"[%2d] expected init (0x%x) got 0x%x",
		     vemp_id, VE_MPMSG_INIT, p->msg);
      if (p->tag != 0)
	veFatalError(MODULE,"[%2d] expected tag 0 on init message, got %d",
		     vemp_id, p->tag);
      if (p->dlen != sizeof(VeMPInitMsg))
	veFatalError(MODULE,"[%2d] expected init msg (%d bytes) got %d bytes",
		     vemp_id, sizeof(VeMPInitMsg), p->dlen);
      m = (VeMPInitMsg *)(p->data);
      vemp_slave_process = veDupString(m->process);
      vemp_slave_node = veDupString(m->node);
      veMPPktDestroy(p);
      veNotice(MODULE,"slave initialized");
    }
  } else {
    /* master - we'll setup our recv/send handles later */

    VEMP_DEBUGMSG("veMPSlaveInit - master\n");
    
    /* build argc/argv lists */
    vemp_argc = (*argc)+2;
    vemp_argv = veAlloc((vemp_argc+1)*sizeof(char *),0);
    vemp_argv[0] = veDupString(argv[0]);
    vemp_argv[1] = veDupString(vemp_slave_opt);
    vemp_argv[2] = NULL; /* fill in later when spawning slaves */
    for(k = 1; k < *argc; k++)
      vemp_argv[k+2] = veDupString(argv[k]);
    vemp_argv[vemp_argc] = NULL;
    veMPImplEarlyInit();
  }
  return 0;
}

int veMPGetSlave(char *node, char *process, int allow_fail) {
  int k;
  
  if (!veMPIsMaster()) {
    /* in general this is a bad, bad error, but let's listen
       to "allow_fail" */
    if (allow_fail) {
      veWarning(MODULE,"veMPGetSlave called on slave");
      return -1; 
    } else
      veFatalError(MODULE,"veMPGetSlave called on slave");
  }

  node = node ? node : "auto";
  process = process ? process : "auto";

  if (strcmp(process,vemp_unique_name)) {
    /* look for existing connection */
    for (k = 0; k < slave_max; k++) {
      if (slaves[k].inuse && slaves[k].process &&
	  strcmp(node,slaves[k].node) == 0 &&
	  strcmp(process,slaves[k].process) == 0)
	return k; /* exists */
    }
  }
  /* does not exist - look for opening */
  for (k = 0; k < slave_max; k++) {
    if (!slaves[k].inuse)
      break;
  }
  if (k == slave_max) {
    /* increase table size */
    slave_max++;
    if (slave_max > MAX_SLAVES) {
      if (allow_fail) {
	/* not a normal error, but upper level claims it can handle it */
	slave_max--;
	/* since it is unusual, print out an error even though
	   allow_fail is true */
	veError(MODULE,"too many slaves (MAX_SLAVES = %d)",MAX_SLAVES);
	return -1;
      } else
	veFatalError(MODULE,"too many slaves (MAX_SLAVES = %d)",MAX_SLAVES);
    }
  }
  /* create slot */
  slaves[k].inuse = 1;
  slaves[k].id = k;
  slaves[k].node = veDupString(node);
  slaves[k].process = strcmp(process,vemp_unique_name) ? 
    veDupString(process) : mkunique();
  /* method? */
  if (strcmp(slaves[k].node,"auto") == 0) {
    if (strcmp(slaves[k].process,"auto"))
      slaves[k].method = VE_MP_LOCAL; /* local process */
    else
      slaves[k].method = VE_MP_THREAD; /* local thread */
  } else {
    slaves[k].method = VE_MP_REMOTE; /* some other node */
  }
  slaves[k].mutex = veThrMutexCreate();
  {
    /* create arg list */
    char str[80];
    sprintf(str,"%d",slaves[k].id);
    vemp_argv[VEMP_SLAVE_ARG] = str;
    VE_DEBUGM(1,("veMPGetSlave - spawing slave %d (%s,%s)",k,
		 slaves[k].node,slaves[k].process));
    if (!(slaves[k].conn = veMPImplCreate(slaves[k].method,
					  k, slaves[k].node,
					  vemp_argc, vemp_argv))) {
      if (allow_fail) {
	/* upper level says it can take it */
	veFree(slaves[k].node);
	veFree(slaves[k].process);
	veThrMutexDestroy(slaves[k].mutex);
	slaves[k].inuse = 0; /* allow slot to be reused */
	return -1;
      } else
	veFatalError(MODULE,"failed to spawn slave %d (%s,%s)",
		     k, slaves[k].node, slaves[k].process);
    }
  }
  
  /* new connection has been established */
  VE_DEBUGM(1,("veMPGetSlave - slave %d spawned",k));
  {
    /* initialize slave */
    VeMPInitMsg m;
    if (slaves[k].method == VE_MP_THREAD) {
      /* this is a local slave - we have no special connection to it */
      /* it is also *not* waiting for an init message,
	 so we just fake the effects */
      VE_DEBUGM(1,("veMPGetSlave - initializing local thread slave"));
      if (!vemp_slave_process && slaves[k].process)
	vemp_slave_process = veDupString(slaves[k].process);
      if (!vemp_slave_node)
	vemp_slave_node = veDupString(slaves[k].node);
    } else {
      /* an independent process (local or remote) has been created
	 and now we need to tell it "who" it is */
      VE_DEBUGM(1,("veMPGetSlave - initializing independent slave %d",k));
      strncpy(m.process,slaves[k].process,INIT_NAMESZ);
      m.process[INIT_NAMESZ-1] = '\0';
      strncpy(m.node,slaves[k].node,INIT_NAMESZ);
      m.node[INIT_NAMESZ-1] = '\0';
      if (veMPSendMsg(VE_MP_RELIABLE,k,VE_MPMSG_INIT,0,
		      &m,sizeof(m)))
	veFatalError(MODULE,"veMPGetSlave - failed to initialize slave %d",k);
      /* let implementation layer do its work, if it so desires
	 - e.g. the remote implementation may setup the "FAST"
	 channel */
      VE_DEBUGM(1,("veMPGetSlave - preparing slave %d",k));
      if (veMPImplPrepare(slaves[k].conn,0))
	veFatalError(MODULE,"veMPGetSlave - failed to prepare slave %d",k);
    }
  }
  
  VE_DEBUGM(1,("veMPGetSlave - initializing message thread for slave %d",k));
  /* setup handler for incoming slave messages */
  veThreadInit(NULL,slave_msg_thread,(void *)k,0,0);

  /* connection is ready to go */
  return k;
}

/* !!! This code needs some serious reworking - we would like to gather
   MP information from places other than just the environment !!! */
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
    VE_DEBUGM(2,("veMPInit - short-circuit - multiple calls"));
    return 0;
  }

  /* set-up an exit callback so that if we blowup, connections get closed */
  atexit(veMPCleanup);

  VE_DEBUGM(2,("veMPInit enter"));

  /* add internal handler for state variables */
  veMPAddSlaveHandler(VE_MPMSG_STATE,VE_DTAG_ANY,statevar_handler);

  if (!vemp_is_master) {
    /* set-up incoming message thread */
    VE_DEBUGM(2,("veMPInit - slave message thread starting"));
    veThreadInit(NULL,msg_thread,vemp_conn,0,0);
    called = 1; /* guard against second call */
    return 0;
  }

  /* The rest of this function is master-only... */

  VE_DEBUGM(2,("veMPInit complete"));
  called = 1; /* guard against a second call... */
  return 0;
}


int veMPDataAddHandler(int tag, VeMPDataHandler handler) {
  struct vemp_data_handler *h = veAllocObj(struct vemp_data_handler);
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
  return veMPIntPush(VE_MPTARG_ALL,VE_MP_FAST,VE_MPMSG_DATA,tag,data,dlen);
}

int veMPLocationPush(void) {
  VeMPLocMsg m;
  if (veMPTestSlaveGuard())
    return 0;
  m.origin = *(veGetOrigin());
  m.eye = *(veGetDefaultEye());
  return veMPIntPush(VE_MPTARG_ALL,VE_MP_FAST,VE_MPMSG_LOCATION,0,&m,sizeof(m));
}

int veMPEnvPush(void) {
  FILE *f;
  long sz;
  char *buf;
  int k;

  {
    /* Due to some subtle ordering reasons we do not do environment pushes
       as it will corrupt the slaves - to be fixed later only
       if absolutely necessary */
    static int init = 0;
    if (!init) {
      veNotice(MODULE,"veMPEnvPush:  not currently supported (only a placeholder)");
      init = 1;
    }
    return -1; /* signal error */
  }

  f = tmpfile();
  if (!f)
    veFatalError(MODULE,"veMPEnvPush: failed to create temporary file: %s",
		 veSysError());
  if (veEnvWrite(veGetEnv(),f))
    veFatalError(MODULE,"veMPEnvPush: failed to write to temporary file: %s",
		 veSysError());
  if (fseek(f,0,SEEK_END))
    veFatalError(MODULE,"veMPEnvPush: failed to seek to end of temporary file: %s",
		 veSysError());
  sz = ftell(f);
  buf = veAlloc(sz,0);
  rewind(f);
  if (fread(buf,sz,1,f) != 1)
    veFatalError(MODULE,"veMPEnvPush: failed to retrieve data from temporary file: %s",
		 veSysError());
  fclose(f);
  k = veMPIntPush(VE_MPTARG_ALL,VE_MP_RELIABLE,VE_MPMSG_ENV,0,buf,sz);
  veFree(buf);
  return k;
}

int veMPProfilePush(void) {
  FILE *f = tmpfile();
  long sz;
  char *buf;
  int k;
  if (!f)
    veFatalError(MODULE,"veMPProfilePush: failed to create temporary file: %s",
		 veSysError());
  if (veProfileWrite(veGetProfile(),f))
    veFatalError(MODULE,"veMPProfilePush: failed to write to temporary file: %s",
		 veSysError());
  if (fseek(f,0,SEEK_END))
    veFatalError(MODULE,"veMPProfilePush: failed to seek to end of temporary file: %s",
		 veSysError());
  sz = ftell(f);
  buf = veAlloc(sz,0);
  rewind(f);
  if (fread(buf,sz,1,f) != 1)
    veFatalError(MODULE,"veMPProfilePush: failed to retrieve data from temporary file: %s",
		 veSysError());
  fclose(f);
  k = veMPIntPush(VE_MPTARG_ALL,VE_MP_RELIABLE,VE_MPMSG_PROFILE,0,buf,sz);
  veFree(buf);
  return k;
}

int veMPAddSlaveHandler(int msg, int tag, VeMPPktHandler handler) {
  struct vemp_int_handler *h;
  h = veAllocObj(struct vemp_int_handler);
  h->msg = msg;
  h->tag = tag;
  h->handler = handler;
  h->next = NULL;
  if (sh_tail) {
    sh_tail->next = h;
    sh_tail = h;
  } else {
    shandlers = sh_tail = h;
  }
  return 0;
}

int veMPAddMasterHandler(int msg, int tag, VeMPPktHandler handler) {
  struct vemp_int_handler *h;
  if (!veMPIsMaster())
    return -1; /* can only add on master */
  h = veAllocObj(struct vemp_int_handler);
  h->msg = msg;
  h->tag = tag;
  h->handler = handler;
  h->next = NULL;
  if (mh_tail) {
    mh_tail->next = h;
    mh_tail = h;
  } else {
    mhandlers = mh_tail = h;
  }
  return 0;
}

int veMPIntPushToMaster(int ch, int msg, int tag, void *data, int dlen) {
  int k;
  VE_DEBUGM(5,("MPIntPushToMaster(%d,%d,%d) %d bytes",ch,msg,tag,dlen));
  if ((k = veMPImplSendv(vemp_conn,0,ch,msg,tag,data,dlen))) {
    veError(MODULE,"ImplSend(msg=%d,tag=%d) - %s",
	    msg,tag,veSysError());
    return k;
  }
  return 0;
}

/* ignore sequence numbers for now */
int veMPIntPush(int target, int ch, int msg, int tag, void *data, int dlen) {
  if (target == VE_MPTARG_ALL) {
    int k;
    VE_DEBUGM(5,("MPIntPush(all,%d,%d,%d) %d bytes",ch,msg,tag,dlen));
    for(k = 0; k < slave_max; k++) {
      if (slaves[k].inuse) {
	veThrMutexLock(slaves[k].mutex);
	if (veMPImplSendv(slaves[k].conn,0,ch,msg,tag,data,dlen)) {
	  veThrMutexUnlock(slaves[k].mutex);
	  veError(MODULE,"ImplSend(target=%d,msg=%d,tag=%d) - %s",
		  k,msg,tag,veSysError());
	  return -1;
	}
	veThrMutexUnlock(slaves[k].mutex);
      }
    }
    return 0;
  } else {
    int k;
    VE_DEBUGM(5,("MPIntPush(%d,%d,%d,%d) %d bytes",ch,target,msg,tag,dlen));
    if (target >= 0 && target < slave_max && slaves[target].inuse) {
      veThrMutexLock(slaves[target].mutex);
      if ((k = veMPImplSendv(slaves[target].conn,0,ch,msg,tag,data,dlen))) {
	veError(MODULE,"ImplSend(target=%d,msg=%d,tag=%d) - %s",
		k,msg,tag,veSysError());
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

char *veMPSlaveProcess(int k) {
  if (k == VE_MP_SELF)
    return vemp_slave_process;
  if (k >= 0 && k < slave_max)
    return slaves[k].process;
  else
    return NULL;
}

char *veMPSlaveNode(int k) {
  if (k == VE_MP_SELF)
    return vemp_slave_node;
  if (k >= 0 && k < slave_max)
    return slaves[k].node;
  else
    return NULL;
}

int veMPId(void) {
  return vemp_id;
}

void veMPCleanup(void) {
  veMPImplCleanup();
}

int veMPSendMsg(int ch, int target, int msg, int tag, void *data, int dlen) {
  if (target == VE_MPTARG_MASTER)
    /* target is the master */
    return veMPIntPushToMaster(ch,msg,tag,data,dlen);
  else
    return veMPIntPush(target,ch,msg,tag,data,dlen);
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
  v = veAllocObj(struct vemp_statevar);
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
      veMPIntPush(VE_MPTARG_ALL,VE_MP_FAST,VE_MPMSG_STATE,v->tag,
		  v->var,v->vlen);
    }
  }
  return 0;
}

/* ignore STATE messages received on the master... */
static void statevar_handler(int k, VeMPPkt *p) {
  if (p->msg == VE_MPMSG_STATE && !veMPIsMaster()) {
    struct vemp_statevar *v;
    for(v = statevars; v; v = v->next) {
      if (p->tag == v->tag) {
	int min = (p->dlen < v->vlen ? p->dlen : v->vlen);
	assert(v->var != NULL);
	assert(min > 0); /* we should never send 0 bytes of state */
	memcpy(v->var,p->data,min);
	break;
      }
    }
  }
}

/* packet functions */
VeMPPkt *veMPPktCreate(int dlen) {
  VeMPPkt *p;
  p = veAllocObj(VeMPPkt);
  if (dlen > 0) {
    p->dlen = dlen;
    p->data = veAlloc(dlen,0);
    assert(p->data != NULL);
  }
  return p;
}

void veMPPktDestroy(VeMPPkt *p) {
  if (p) {
    veFree(p->data);
    veFree(p);
  }
}
