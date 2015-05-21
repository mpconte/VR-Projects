/* to be filled in later */
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve.h>

#define MODULE "ve_audio"

#define DEFAULT_NAME "default"

static int frame_sz = 500; /* for now... */

/* if 1, then zero all output before sending it... */
static int global_mute_flag = 0;

int veAudioSetFrameSize(int sz) {
  if (sz <= 0)
    return -1;
  frame_sz = sz;
  return 0;
}

int veAudioGetFrameSize(void) {
  return frame_sz;
}

static int samp_freq = 44100;

int veAudioSetSampFreq(int freq) {
  if (freq <= 0)
    return -1;
  samp_freq = freq;
  return 0;
}

int veAudioGetSampFreq(void) {
  return samp_freq;
}

void veAudioOutputFree(VeAudioOutput *o) {
  if (o) {
    veFree(o->name);
    veOptionFreeList(o->options);
    veFree(o);
  }
}

static VeStrMap channels = NULL;

VeAudioChannel *veAudioFindChannel(char *name) {
  if (!channels)
    return NULL;
  if (!name)
    name = DEFAULT_NAME;
  return (VeAudioChannel *)veStrMapLookup(channels,name);
}

void veAudioChannelReg(VeAudioChannel *ch) {
  VeAudioChannel *old;
  if (!ch->name)
    ch->name = veDupString(DEFAULT_NAME);
  if (!channels)
    channels = veStrMapCreate();
  if ((old = veStrMapLookup(channels,ch->name)))
    veAudioChannelFree(old);
  veStrMapInsert(channels,ch->name,ch);
}

void veAudioChannelFree(VeAudioChannel *ch) {
  if (ch) {
    VeAudioOutput *o, *on;
    veFree(ch->name);
    veFree(ch->slave.node);
    veFree(ch->slave.process);
    veFree(ch->slave.thread);
    o = ch->outputs;
    while (o) {
      on = o->next;
      veAudioOutputFree(o);
      o = on;
    }
    veOptionFreeList(ch->options);
    veFree(ch);
  }
}

int veAudioChannelInit(VeAudioChannel *ch) {
  if (!ch->engine)
    return 0;
  return veAudioEngineInit(ch->engine,ch,ch->options);
}

static VeStrMap audio_devices = NULL;

VeAudioDevice *veAudioDevCreate(char *name, char *driver, VeOption *options) {
  VeAudioDevice *d, *old;
  VeAudioDriver *drv;

  if (!(drv = veAudioDriverFind(driver))) {
    veError(MODULE,"cannot find audio driver '%s'",driver);
    return NULL;
  }

  if (!drv->inst) {
    veError(MODULE,"audio driver '%s' has no instantiation procedure",driver);
    return NULL;
  }

  if (!(d = drv->inst(drv,options))) {
    veError(MODULE,"failed to initialize device '%s' using driver '%s'",
	    name ? name : "<null>", driver);
    return NULL;
  }

  d->name = veDupString(name);
  if (!audio_devices)
    audio_devices = veStrMapCreate();
  veStrMapInsert(audio_devices,name,d);
  return d;
}

VeAudioDevice *veAudioDevFind(char *name) {
  if (!audio_devices)
    return NULL;
  return (VeAudioDevice *)veStrMapLookup(audio_devices,name);
}

void veAudioDevDestroy(char *name) {
  VeAudioDevice *d;
  if (!audio_devices || !(d = veStrMapLookup(audio_devices,name)))
    return;
  if (d->driver && d->driver->deinst)
    d->driver->deinst(d);
  veStrMapDelete(audio_devices,name);
}

int veAudioDevGetSub(VeAudioDevice *dev, char *name) {
  if (!dev || !name)
    return -1;
  if (!dev->driver || !dev->driver->getsub)
    return -1;
  return dev->driver->getsub(dev,name);
}

int veAudioDevBuffer(VeAudioDevice *dev, int sub, float *data, int dlen) {
  if (!dev || !dev->driver || !dev->driver->buffer)
    return -1;
  return dev->driver->buffer(dev,sub,data,dlen);
}

void veAudioDevFlush(VeAudioDevice *dev, int sub) {
  if (!dev || !dev->driver || !dev->driver->flush)
    return;
  dev->driver->flush(dev,sub);
}

int veAudioDevWait(VeAudioDevice *dev) {
  if (!dev || !dev->driver || !dev->driver->wait)
    return -1;
  return dev->driver->wait(dev);
}

static VeStrMap engine_map = NULL;

void veAudioEngineAdd(char *name, VeAudioEngine *e) {
  if (!name)
    name = e->name;
  if (!name)
    name = "default";
  if (!engine_map)
    engine_map = veStrMapCreate();
  veStrMapInsert(engine_map,name,e);
}

VeAudioEngine *veAudioEngineFind(char *name) {
  VeAudioEngine *e;
  if (engine_map && (e = (VeAudioEngine *)veStrMapLookup(engine_map,name)))
    return e;
  if (veDriverRequire("audioengine",name))
    return NULL;
  if (engine_map && (e = (VeAudioEngine *)veStrMapLookup(engine_map,name)))
    return e;
  return NULL;
}

int veAudioEngineInit(VeAudioEngine *e, struct ve_audio_channel *ch, 
		      VeOption *options) {
  if (!e)
    return -1;
  if (e->init)
    e->init(ch,options);
  return 0;
}

typedef struct ve_audio_ientry { 
  int inuse;  /* is this slot available */
  VeAudioInst inst;  /* instance info */
} VeAudioIEntry;

typedef struct ve_audio_itable {
  VeAudioIEntry *data;
  int spc, use;
  VeThrMutex *mutex;
} VeAudioITable;

/* grow the given table so that index 'n' can be accomodated */
static void grow_itable(VeAudioITable *it, int n) {
  int orig;
  if (it->spc > n)
    return; /* nothing to do */

  orig = it->spc;

  if (it->spc == 0)
    it->spc = n+1; /* default */
  else {
    while (it->spc <= n)
      it->spc *= 2; /* grow */
  }
  it->data = veRealloc(it->data,sizeof(VeAudioIEntry)*it->spc);
  memset(it->data+orig,0,(it->spc - orig)*sizeof(VeAudioIEntry));
}

static int get_itable_id(VeAudioITable *it) {
  int k;
  veThrMutexLock(it->mutex);
  for (k = 0; k < it->use; k++)
    if (!it->data[k].inuse) {
      it->data[k].inuse = 1;
      veThrMutexUnlock(it->mutex);
      return k;
    }
  grow_itable(it,k);
  it->data[k].inuse = 1;
  if (k >= it->use)
    it->use = k+1;
  veThrMutexUnlock(it->mutex);
  return k;
}

/* MP layer... */

/* message types */
#define M_CHANNEL (1)  /* M->S:  create channel */
#define M_NEW     (2)  /* M->S:  new instance */
#define M_UPDATE  (3)  /* M->S:  update parameters */
#define M_STOP    (4)  /* M->S:  stop an instance */
#define M_CLEAN   (5)  /* S->M:  instance is finished (cleaned) */
#define M_MUTE    (6)  /* M->S:  set mute flag */

/* Current model is 1 thread per-channel */
/* Might not be a great model for multi-output cards... */

typedef struct ve_audio_thread {
  VeThread *thr;                 /* the actual thread */
  VeAudioChannel *ch;            /* my channel */
  int chid;
  VeAudioITable itable;          /* instance table */
  VeThrMutex *mutex;
} VeAudioThread;

typedef struct ve_audio_slave {
  int nthrs;     /* all channels I am managing must be in the
		    range 0..nthrs-1 */
  int thrspc;    /* space allocated for thread array */
  VeAudioThread **thrs;    /* space for threads */
} VeAudioSlave;

/* message structures */
#define VE_CHNAME_SZ 128
typedef struct {
  char name[VE_CHNAME_SZ+1]; /* name of channel */
  int id;              /* numerical id to assign to channel */
} VeAudioChMsg;

/* sound message used for new instances */
typedef struct {
  int chid;   /* channel id */
  int itid;   /* table entry */
  char sndname[VE_SNDNAME_SZ];
  VeAudioParams params; /* parameters */
} VeAudioSndMsg;

/* message for instance updates */
typedef struct {
  int chid;
  int itid;
  VeAudioParams params;
} VeAudioUpdMsg;

/* simple id-only message (for stop/clean) */
typedef struct {
  int chid;
  int itid;
} VeAudioIdMsg;

typedef struct {
  int flag;
} VeAudioMuteMsg;

static void *channel_thread(void *x) {
  VeAudioThread *me = (VeAudioThread *)x;
  int k;
  VeAudioChannelBuffer *buf;
  VeAudioEngine *eng;

  assert(me->ch != NULL);
  buf = veAudioChannelBufferCreate(me->ch);
  if (!buf)
    veFatalError(MODULE,"failed to allocate channel buffer for channel %s",
		 me->ch->name ? me->ch->name : "<null>");
  eng = me->ch->engine;
  assert(eng != NULL);

  while (1) {
    veThrMutexLock(me->mutex);

    veAudioChannelBufferZero(buf);

    for (k = 0; k < me->itable.use; k++) {
      if (me->itable.data[k].inuse) {
	VeAudioInst *i = (VeAudioInst *)(&me->itable.data[k].inst);
	if (i->next_frame < 0)
	  i->clean[1] = 1; /* raise my cleaning flag */
	else {
	  /* pass frame to engine for rendering... */
	  /* engines should *mix* into the buffer... */
	  eng->process(me->ch,i,buf);
	}
	/* update instance flags */
	i->next_frame++;
	if (i->next_frame >= i->snd->nframes) {
	  if (i->params && i->params->loop) {
	    i->next_frame = 0;
	    if (i->params->loop > 0)
	      i->params->loop--;
	  } else {
	    i->next_frame = -1; /* signal end */
	    i->clean[1] = 1;
	  }
	}
      }
    }

    if (global_mute_flag)
      veAudioChannelBufferZero(buf);

    /* feed buffers to outputs */
    {
      VeAudioOutput *o;
      int fsize = veAudioGetFrameSize();
      for (k = 0, o = me->ch->outputs; o; k++, o = o->next)
	veAudioDevBuffer(o->device,o->devout_id,buf->buf[k],fsize);
    }
    
    /* now that we have buffered, send "clean" messages to master
       as needed and mark cleaned instances as no longer being in use */
    for (k = 0; k < me->itable.use; k++) {
      if (me->itable.data[k].inuse &&
	  me->itable.data[k].inst.clean[1]) {
	VeAudioIdMsg m;
	m.chid = me->chid;
	m.itid = k;
	veMPSendMsg(VE_MP_RELIABLE,VE_MPTARG_MASTER,
		    VE_MPMSG_AUDIO,M_CLEAN,&m,sizeof(m));
	if (me->itable.data[k].inst.params) {
	  veFree(me->itable.data[k].inst.params);
	  me->itable.data[k].inst.params = NULL;
	}
	me->itable.data[k].inuse = 0;
      }
    }
    
    veThrMutexUnlock(me->mutex);

    /* sleep on output until output is ready
       to queue another frame */
    if (me->ch->outputs) {
      /* Rely on outputs to tell us when they are ready for more */
      VeAudioOutput *o;
      for (o = me->ch->outputs; o; o = o->next)
	veAudioDevWait(o->device);
    } else {
      /* SPECIAL CASE: no outputs - fake it by sleeping */
      veMicroSleep((veAudioGetFrameSize()*1.0e6)/
		   veAudioGetSampFreq());
    }
  }
}

static void slave_audio_cback(int src, VeMPPkt *p) {
  static VeAudioSlave me;
  static int running = 1;

  if (p->msg != VE_MPMSG_AUDIO)
    return;

  if (!running)
    return; /* message after shutdown */

  VE_DEBUGM(4,("slave_audio_cback: message %d",p->tag));
  
  switch (p->tag) {
  case M_CHANNEL:
    {
      VeAudioChMsg m;
      VeAudioChannel *ch;
      
      if (p->dlen != sizeof(VeAudioChMsg))
	veFatalError(MODULE,"expected %d bytes for M_CHANNEL, received %d bytes",
		     sizeof(VeAudioChMsg), p->dlen);
      memcpy(&m,p->data,sizeof(m));
      if (!(ch = veAudioFindChannel(m.name)))
	veFatalError(MODULE,"slave_audio_cback:  could not locate channel %s",
		     m.name);
      
      /* is id already in use? */
      if (me.nthrs > m.id && me.thrs[m.id])
	veFatalError(MODULE,"slave_audio_cback:  id %d for channel %s is already"
		     "in use by channel %s", m.id, m.name,
		     me.thrs[m.id]->ch->name);

      /* open outputs... */
      if (veAudioChannelInit(ch))
	veFatalError(MODULE,"slave_audio_cback:  could not initialize audio channel %s",
		     ch->name);

      VE_DEBUGM(2,("handling channel %s as id %d",m.name,m.id));
      
      if (me.thrspc <= m.id) {
	/* grow array */
	me.thrspc = m.id+1;
	me.thrs = veRealloc(me.thrs,sizeof(VeAudioThread *)*me.thrspc);
	assert(me.thrs != NULL);
      }

      /* create the slave */
      me.thrs[m.id] = veAllocObj(VeAudioThread);
      assert(me.thrs[m.id] != NULL);

      ch->id = m.id; /* assign id */
      me.thrs[m.id]->ch = ch;
      me.thrs[m.id]->chid = m.id;
      me.thrs[m.id]->thr = veThreadCreate();
      me.thrs[m.id]->mutex = veThrMutexCreate();
      if (m.id <= me.nthrs) {
	int k;
	for (k = me.nthrs; k < m.id; k++)
	  me.thrs[k] = NULL;
	me.nthrs = m.id+1;
      }
      veThreadInit(me.thrs[m.id]->thr,channel_thread,
		   (void *)me.thrs[m.id],10,VE_THR_KERNEL);

      VE_DEBUGM(2,("channel %s successfully initialized",m.name));
    }
    break;

  case M_NEW:
    {
      VeAudioSndMsg m;
      VeSound *s;
      
      if (p->dlen != sizeof(VeAudioSndMsg))
	veFatalError(MODULE,"expected %d bytes for M_NEW, received %d bytes",
		     sizeof(VeAudioSndMsg), p->dlen);

      memcpy(&m,p->data,sizeof(VeAudioSndMsg));
      if (m.chid < 0 || m.chid >= me.nthrs || !me.thrs[m.chid])
	veFatalError(MODULE,"invalid M_NEW message for non-existent channel id %d",m.chid);
      if (m.itid < 0)
	veFatalError(MODULE,"instance id cannot be negative");
      if (!(s = veSoundFindName(m.sndname)))
	veFatalError(MODULE,"no such sound: %s",m.sndname);

      veThrMutexLock(me.thrs[m.chid]->mutex);
      if (m.itid >= 0 && m.itid < me.thrs[m.chid]->itable.use &&
	  me.thrs[m.chid]->itable.data[m.itid].inuse) {
	veWarning(MODULE,"ignoring M_NEW request for existing instance (%d) on channel %d",
		  m.itid, m.chid);
      }
      grow_itable(&me.thrs[m.chid]->itable,m.itid);
      if (me.thrs[m.chid]->itable.use <= m.itid)
	me.thrs[m.chid]->itable.use = m.itid+1;
      me.thrs[m.chid]->itable.data[m.itid].inuse = 1;
      {
	VeAudioInst *i = &(me.thrs[m.chid]->itable.data[m.itid].inst);
	if (!i->params) {
	  i->params = veAllocObj(VeAudioParams);
	  veAudioInitParams(i->params);
	}
	i->id = m.itid;
	i->sid = s->id;
	i->snd = s;
	*(i->params) = m.params;
	i->next_frame = 0;
	i->clean[0] = i->clean[1] = 0;
	i->waiting = 0;
      }
      veThrMutexUnlock(me.thrs[m.chid]->mutex);
    }
    break;

  case M_UPDATE:
    {
      VeAudioUpdMsg m;
      
      if (p->dlen != sizeof(VeAudioUpdMsg))
	veFatalError(MODULE,"expected %d bytes for M_UPDATE, received %d bytes",
		     sizeof(VeAudioUpdMsg), p->dlen);
      memcpy(&m,p->data,sizeof(VeAudioUpdMsg));
      if (m.chid < 0 || m.chid >= me.nthrs || !me.thrs[m.chid])
	veFatalError(MODULE,"invalid M_UPDATE message for non-existent channel id %d",m.chid);
      veThrMutexLock(me.thrs[m.chid]->mutex);
      if (m.itid >= 0 && m.itid < me.thrs[m.chid]->itable.use &&
	  me.thrs[m.chid]->itable.data[m.itid].inuse) {
	/* update instance id */
	VeAudioInst *i = &(me.thrs[m.chid]->itable.data[m.itid].inst);
	if (!i->params) {
	  i->params = veAllocObj(VeAudioParams);
	  veAudioInitParams(i->params);
	}
	*i->params = m.params;
      }
      veThrMutexUnlock(me.thrs[m.chid]->mutex);
    }
    break;

  case M_STOP:
    {
      VeAudioIdMsg m;
      
      if (p->dlen != sizeof(VeAudioIdMsg))
	veFatalError(MODULE,"expected %d bytes for M_STOP, received %d bytes",
		     sizeof(VeAudioIdMsg), p->dlen);
      memcpy(&m,p->data,sizeof(VeAudioIdMsg));
      if (m.chid < 0 || m.chid >= me.nthrs || !me.thrs[m.chid])
	veFatalError(MODULE,"invalid M_UPDATE message for non-existent channel id %d",m.chid);
      veThrMutexLock(me.thrs[m.chid]->mutex);
      if (m.itid >= 0 && m.itid < me.thrs[m.chid]->itable.use &&
	  me.thrs[m.chid]->itable.data[m.itid].inuse) {
	me.thrs[m.chid]->itable.data[m.itid].inst.next_frame = -1;
      }
      veThrMutexUnlock(me.thrs[m.chid]->mutex);
    }
    break;

  case M_MUTE:
    {
      VeAudioMuteMsg m;
      if (p->dlen != sizeof(VeAudioMuteMsg))
	veFatalError(MODULE,"expected %d bytes for M_MUTE, received %d bytes",
		     sizeof(VeAudioMuteMsg), p->dlen);
      memcpy(&m,p->data,sizeof(VeAudioMuteMsg));
      global_mute_flag = m.flag;
    }
    break;

  default:
    veError(MODULE,"unexpected audio message from master (tag = %d)",
	    p->tag);
  }
}

typedef struct ve_audio_slave_conn {
  int inuse;
  VeAudioITable itable;
  int mpid;
  int chid;
  VeAudioChannel *ch;
} VeAudioSlaveConn;

typedef struct ve_audio_master {
  VeAudioSlaveConn *slaves;
  int nslaves;
  VeThrMutex *mutex;
} VeAudioMaster;

static VeAudioMaster master;

static void audio_inst_clean(VeAudioInst *);

static void cleanup_check(VeAudioInst *i) {
  if (i->clean[0] && i->clean[1] && i->waiting == 0)
    audio_inst_clean(i);
}

static void master_audio_cback(int src, VeMPPkt *p) {
  if (p->msg != VE_MPMSG_AUDIO)
    return;

  VE_DEBUGM(4,("master_audio_cback: message %d",p->tag));
  
  switch (p->tag) {
  case M_CLEAN:
    veThrMutexLock(master.mutex);
    {
      /* find channel */
      VeAudioIdMsg m;
      if (p->dlen != sizeof(m))
	veFatalError(MODULE,"master got message of invalid size for M_CLEAN: expected %d, got %d",sizeof(m),p->dlen);
      memcpy(&m,p->data,sizeof(m));
      if (m.chid < 0 || m.chid >= master.nslaves || !master.slaves[m.chid].inuse)
	veFatalError(MODULE,"master got M_CLEAN message for invalid channel id: %d",m.chid);
      /* channel is valid */
      if (m.itid < 0 || m.itid >= master.slaves[m.chid].itable.use ||
	  !master.slaves[m.chid].itable.data[m.itid].inuse)
	veWarning(MODULE,"master got M_CLEAN message for invalid instance id: %d",m.itid);
      else {
	VeAudioInst *i = &(master.slaves[m.chid].itable.data[m.itid].inst);
	i->clean[1] = 1;
	veThrCondBcast(i->finished_cond);
	cleanup_check(i);
      }
    }
    veThrMutexUnlock(master.mutex);
    break;

  default:
    veFatalError(MODULE,"master received unexpected message: %d",p->tag);
  }
}

void veMPAudioRun(void) {
  if (!veMPIsMaster())
    return;

  assert(master.mutex != NULL);

  veThrMutexLock(master.mutex);
  master.nslaves = 0;

  if (channels) {
    VeStrMapWalk w = veStrMapWalkCreate(channels);
    /* count slaves */
    if (veStrMapWalkFirst(w) == 0) {
      do {
	master.nslaves++;
      } while (veStrMapWalkNext(w) == 0);
    }
    
    master.slaves = veAlloc(master.nslaves*sizeof(VeAudioSlaveConn),1);

    /* allocate channels to slaves */
    if (veStrMapWalkFirst(w) == 0) {
      int id = 0;
      do {
	VeAudioChannel *ch = (VeAudioChannel *)veStrMapWalkObj(w);
	VeAudioChMsg m;
	int k;
	if ((k = veMPGetSlave(ch->slave.node,ch->slave.process,1)) < 0) {
	  veWarning(MODULE,"ignoring failed connection for audio channel %s",
		    ch->name);
	  continue;
	}
	memset(&m,0,sizeof(m));
	strncpy(m.name,ch->name,VE_CHNAME_SZ);
	m.name[VE_CHNAME_SZ-1] = '\0';
	m.id = id;
	master.slaves[id].inuse = 1;
	master.slaves[id].mpid = k;
	master.slaves[id].chid = id;
	master.slaves[id].ch = ch;
	master.slaves[id].itable.mutex = veThrMutexCreate();
	veMPSendMsg(VE_MP_RELIABLE,master.slaves[id].mpid,
		    VE_MPMSG_AUDIO,M_CHANNEL,&m,sizeof(m));
	id++;
      } while (veStrMapWalkNext(w) == 0);
    }
  }
  veThrMutexUnlock(master.mutex);
}

/* maps a "global" audio instance onto a unique
   instance identifier */
typedef struct ve_audio_inst_map {
  /* -1 for any value implies that this entry is not in use */
  int chid; /* the channel id */
  int itid; /* the channel's table entry */
} VeAudioInstMap;

static VeAudioInstMap *instmap = NULL;
static int instmap_spc = 0, instmap_use = 0;
static VeThrMutex *instmap_mutex = NULL;

static int get_inst_id(void) {
  int k;
  
  veThrMutexLock(instmap_mutex);

  /* look for unused slot */
  for (k = 0; k < instmap_use; k++)
    if (instmap[k].chid < 0)
      goto end;
  
  if (instmap_use >= instmap_spc) {
    while (instmap_use >= instmap_spc) {
      if (instmap_spc == 0)
	instmap_spc = 64;
      else
	instmap_spc *= 2;
    }
    instmap = veRealloc(instmap,instmap_spc*sizeof(VeAudioInstMap));
  }
  k = instmap_use;
  instmap_use++;

 end:
  /* fill in with bogus values to keep this cell from being
     reallocated */
  instmap[k].chid = 0;
  instmap[k].itid = 0;
  veThrMutexUnlock(instmap_mutex);

  return k;
}


static VeAudioInst *find_inst(int instid) {
  int chid, itid;
  if (instid < 0 || instid >= instmap_use)
    return NULL;
  if (instmap[instid].chid < 0)
    return NULL;
  chid = instmap[instid].chid;
  itid = instmap[instid].itid;
  if (chid < 0 || chid >= master.nslaves || !master.slaves[chid].inuse)
    return NULL; /* invalid chid */
  if (itid < 0 || itid >= master.slaves[chid].itable.use || 
      !master.slaves[chid].itable.data[itid].inuse)
    return NULL; /* invalid itid */
  return &(master.slaves[chid].itable.data[itid].inst);
}

int veAudioInst(char *chname, VeSound *snd, VeAudioParams *p) {
  VeAudioChannel *ch;
  int chid;
  int itid;
  int instid;
  if (!snd) {
    veError(MODULE,"attempt to instantiate null sound");
    return -1;
  }
  if (!(ch = veAudioFindChannel(chname))) {
    veWarning(MODULE,"attempt to instantiate audio on non-existent channel %s",chname);
    return -1;
  }
  /* find matching channel id */
  for (chid = 0; chid < master.nslaves; chid++) {
    if (master.slaves[chid].ch == ch)
      break;
  }
  if (chid >= master.nslaves) {
    veWarning(MODULE,"channel '%s' exists, but cannot be mapped to identifier",chname);
    return -1;
  }
  veThrMutexLock(master.mutex);
  itid = get_itable_id(&master.slaves[chid].itable);
  instid = get_inst_id();
  instmap[instid].chid = chid;
  instmap[instid].itid = itid;
  /* now fill in instance info */
  {
    VeAudioInst *i = &(master.slaves[chid].itable.data[itid].inst);
    i->id = instid;
    i->sid = snd->id;
    i->chid = chid;
    i->itid = itid;
    i->snd = snd;
    i->params = veAllocObj(VeAudioParams);
    veAudioInitParams(i->params);
    if (p)
      *i->params = *p;
    i->next_frame = 0;
    i->clean[0] = i->clean[1] = 0;
    i->waiting = 0;
    if (!i->finished_cond)
      i->finished_cond = veThrCondCreate();
    i->edata = NULL;
  }
  /* notify slave */
  {
    VeAudioSndMsg m;
    m.chid = chid;
    m.itid = itid;
    strncpy(m.sndname,snd->name,VE_SNDNAME_SZ);
    m.sndname[VE_SNDNAME_SZ-1] = '\0';
    if (p)
      m.params = *p;
    else
      veAudioInitParams(&m.params);
    veMPSendMsg(VE_MP_RELIABLE,master.slaves[chid].mpid,
		VE_MPMSG_AUDIO,M_NEW,&m,sizeof(m));
  }
  veThrMutexUnlock(master.mutex);
  return instid;
}

int veAudioGetParams(int instid, VeAudioParams *params_r) {
  VeAudioInst *i;
  int res;
  veThrMutexLock(master.mutex);
  if (!(i = find_inst(instid)))
    res = -1;
  else {
    if (params_r) {
      if (i->params)
	*params_r = *i->params;
      else
	veAudioInitParams(params_r);
    }
    res = 0;
  }
  veThrMutexUnlock(master.mutex);
  return res;
}

int veAudioUpdate(int instid, VeAudioParams *params) {
  VeAudioInst *i;
  int res;
  veThrMutexLock(master.mutex);
  if (!(i = find_inst(instid)))
    res = -1;
  else {
    if (!i->params)
      i->params = veAllocObj(VeAudioParams);
    if (params)
      *i->params = *params;
    else
      veAudioInitParams(i->params);
    /* push update to slave */
    {
      VeAudioUpdMsg m;
      m.chid = i->chid;
      m.itid = i->itid;
      m.params = *i->params;
      veMPSendMsg(VE_MP_FAST,master.slaves[i->chid].mpid,
		  VE_MPMSG_AUDIO,M_UPDATE,&m,sizeof(m));
    }
    res = 0;
  }
  veThrMutexUnlock(master.mutex);
  return res;
}

int veAudioStop(int instid, int clean) {
  VeAudioInst *i;
  VeAudioIdMsg m;
  int res;

  veThrMutexLock(master.mutex);
  if (!(i = find_inst(instid)))
    res = -1;
  else {
    m.chid = i->chid;
    m.itid = i->itid;
    veMPSendMsg(VE_MP_RELIABLE,master.slaves[i->chid].mpid,
		VE_MPMSG_AUDIO,M_STOP,&m,sizeof(m));
    if (clean) {
      i->clean[0] = 1;
      cleanup_check(i);
    }
    res = 0;
  }
  veThrMutexUnlock(master.mutex);
  return res;
}

int veAudioIsDone(int instid) {
  VeAudioInst *i;
  int res;
  veThrMutexLock(master.mutex);
  if (!(i = find_inst(instid))) {
    veWarning(MODULE,"veAudioIsDone: no such instance");
    res = -1;
  } else if (i->clean[1]) /* check clean flag from process thread */
    res = 1;
  else
    res = 0;
  veThrMutexUnlock(master.mutex);
  return res;
}

int veAudioClean(int instid) {
  VeAudioInst *i;
  int res;
  veThrMutexLock(master.mutex);
  if (!(i = find_inst(instid))) {
    res = -1;
  } else {
    i->clean[0] = 1;
    cleanup_check(i);
    res = 0;
  }
  veThrMutexUnlock(master.mutex);
  return res;
}

int veAudioWait(int instid) {
  /* wait for it to finish */
  VeAudioInst *i;
  int res;
  veThrMutexLock(master.mutex);
  if (!(i = find_inst(instid))) {
    res = -1;
  } else if (i->clean[1]) {
    res = 0; /* already done... */
  } else {
    i->waiting++;
    while (!i->clean[1])
      veThrCondWait(i->finished_cond,master.mutex);
    i->waiting--;
    /* If you're the last one out, please turn off the lights... */
    cleanup_check(i);
  }
}

int veAudioMute(int flag) {
  VeAudioMuteMsg m;
  int k;

  veThrMutexLock(master.mutex);
  m.flag = (flag ? 1 : 0);
  for (k = 0; k < master.nslaves; k++) {
    veMPSendMsg(VE_MP_RELIABLE,master.slaves[k].mpid,VE_MPMSG_AUDIO,M_MUTE,
		&m,sizeof(m));
  }
  veThrMutexUnlock(master.mutex);
  return 0;
}


static void audio_inst_clean(VeAudioInst *i) {
  if (i) {
    /* free params structure */
    if (i->params) {
      veFree(i->params);
      i->params = NULL;
    }
    /* mark channel table entry as unused */
    if (!(i->chid < 0 || i->chid >= master.nslaves || 
	  !master.slaves[i->chid].inuse) &&
	!(i->itid < 0 || i->itid >= master.slaves[i->chid].itable.use || 
	  !master.slaves[i->chid].itable.data[i->itid].inuse)) {
      /* channel table is valid */
      master.slaves[i->chid].itable.data[i->itid].inuse = 0;
    }
    /* mark global instmap as not being in use */
    if (i->id >= 0 && i->id < instmap_use) {
      instmap[i->id].chid = -1;
      instmap[i->id].itid = -1;
    }
  }
}

static VeStrMap audio_drivers = NULL;

void veAudioDriverAdd(char *name, VeAudioDriver *d) {
  if (!audio_drivers)
    audio_drivers = veStrMapCreate();
  if (!name)
    name = d->name;
  if (!name)
    name = "default";
  veStrMapInsert(audio_drivers,name,d);
}

VeAudioDriver *veAudioDriverFind(char *name) {
  VeAudioDriver *d;
  if (audio_drivers && (d = veStrMapLookup(audio_drivers,name)))
    return d;
  if (veDriverRequire("audio",name))
    return NULL;
  if (audio_drivers && (d = veStrMapLookup(audio_drivers,name)))
    return d;
  return NULL;
}

/* setup defaults */

/* the "mix" engine */
static void mix_process(struct ve_audio_channel *ch, VeAudioInst *i,
			VeAudioChannelBuffer *buf) {
  int j,k;
  VeAudioOutput *o;
  int fsize = veAudioGetFrameSize();
  float *frame;

  if (!i->snd || i->next_frame < 0 || i->next_frame >= i->snd->nframes)
    return;

  frame = i->snd->data+(i->next_frame*fsize);

  for (o = ch->outputs, k = 0;
       o; 
       o = o->next, k++) {
    for (j = 0; j < fsize; j++)
      buf->buf[k][j] += frame[j];
  }
}

static VeAudioEngine mix_engine = {
  "mix",
  NULL, /* init */
  NULL, /* deinit */
  mix_process,
  NULL /* clean */
};

static VeAudioDevice *null_inst(VeAudioDriver *drv,
				VeOption *options) {
  VeAudioDevice *d = veAllocObj(VeAudioDevice);
  d->driver = drv;
  d->options = options;
  return d;
}

static void null_deinst(VeAudioDevice *d) {
  if (d) {
    veOptionFreeList(d->options);
    veFree(d->name);
    veFree(d);
  }
}

static VeAudioDriver null_driver = {
  "null",
  null_inst, /* inst */
  null_deinst, /* deinst */
  NULL, /* buffer */
  NULL, /* flush */
  NULL, /* wait */
};

/* setup defaults */
void veAudioInit(void) {
  /* initialize MP layer */
  if (veMPIsMaster()) {
    master.mutex = veThrMutexCreate();
    master.slaves = NULL;
    master.nslaves = 0;
    instmap_mutex = veThrMutexCreate();
  }
  veMPAddMasterHandler(VE_MPMSG_AUDIO,VE_DTAG_ANY,master_audio_cback);
  veMPAddSlaveHandler(VE_MPMSG_AUDIO,VE_DTAG_ANY,slave_audio_cback);
  /* defaults */
  veAudioEngineAdd(NULL,&mix_engine);
  veAudioEngineAdd("default",&mix_engine);
  veAudioDriverAdd(NULL,&null_driver);
  veAudioDriverAdd("default",&null_driver);
  {
    VeAudioChannel *ch = veAllocObj(VeAudioChannel);
    ch->name = veDupString("default");
    ch->engine = &mix_engine;
    ch->id = -1;
    veAudioChannelReg(ch);
  }
}

void veAudioInitParams(VeAudioParams *p) {
  p->loop = 0;
  p->volume = 1.0;
  veFrameIdentity(&(p->source));
  p->spread = 0.0;
}

void veAudioSetVolume(VeAudioParams *p, float v) {
  if (p)
    p->volume = v;
}

float veAudioGetVolume(VeAudioParams *p) {
  return (p ? p->volume : 1.0);
}

int veAudioGetLoop(VeAudioParams *p) {
  return (p ? p->loop : 0);
}

void veAudioSetLoop(VeAudioParams *p, int loop) {
  if (p)
    p->loop = loop;
}

VeFrame *veAudioGetSource(VeAudioParams *p) {
  static VeFrame dummy;
  return (p ? &(p->source) : &dummy);
}

float veAudioGetSpread(VeAudioParams *p) {
  return (p ? p->spread : 0.0);
}

void veAudioSetSpread(VeAudioParams *p, float spread) {
  if (p)
    p->spread = spread;
}

VeAudioChannelBuffer *veAudioChannelBufferCreate(VeAudioChannel *ch) {
  VeAudioChannelBuffer *b = veAllocObj(VeAudioChannelBuffer);
  VeAudioOutput *o;
  int nbuf = 0;
  
  if (!ch)
    return b;
  
  o = ch->outputs;
  while (o) {
    nbuf++;
    o = o->next;
  }

  b->ch = ch;
  b->buf = veAlloc(sizeof(float *)*(nbuf+1),1);
  while (nbuf > 0)
    b->buf[--nbuf] = veAlloc(sizeof(float)*veAudioGetFrameSize(),1);
  return b;
}

void veAudioChannelBufferZero(VeAudioChannelBuffer *b) {
  int fsize = veAudioGetFrameSize();
  if (b && b->buf) {
    float **buf = b->buf;
    while (*buf) {
      memset(*buf,0,sizeof(float)*fsize);
      buf++;
    }
  }
}

void veAudioChannelBufferDestroy(VeAudioChannelBuffer *b) {
  if (b) {
    if (b->buf) {
      float **buf = b->buf;
      while (*buf) {
	veFree(*buf);
	buf++;
      }
      veFree(buf);
    }
    veFree(b);
  }
}
