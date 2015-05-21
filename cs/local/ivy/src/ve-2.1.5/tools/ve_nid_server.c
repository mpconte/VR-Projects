/* A NID server that uses VE device drivers */
/* Every argument on the command line is treated as a device manifest to load.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include <nid.h>
#include <ve.h>

/* If 0, then this is a "one-shot" inetd-style daemon (wait = no)
   else if non-zero then this is a "multiple-shot" inetd-style daemon
   (wait = yes).  This should probably be configurable somehow at run-time. */
int handle_accept = 0;

typedef struct ve_nid_server_device {
  char *name;
  char *type;
  VeDevice *device;   /* the VE device */
  int id;     /* the NID id for this device */
  int report; /* if non-zero then report events for this device */
  NidElementInfo *info;
  NidElementState *state;
  int nelems;
} VeNidSrvDevice;

VeNidSrvDevice **srv_devices = NULL;
int num_srv_devices = 0;
int srv_dev_space = 0;
int next_dev_id = 1;

void proc_accept(int,int);
void proc_pkt(int,int);

void logmsg(char *fmt, ...) {
  va_list args;
  va_start(args,fmt);
  (void) vsyslog(LOG_ERR,fmt,args);
  va_end(args);
}

static long now() {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (long)(tv.tv_sec*1000 + tv.tv_usec/1000);
}

int ve_to_nid(int type) {
  switch (type) {
  case VE_ELEM_TRIGGER: return NID_ELEM_TRIGGER;
  case VE_ELEM_SWITCH: return NID_ELEM_SWITCH;
  case VE_ELEM_VALUATOR: return NID_ELEM_VALUATOR;
  case VE_ELEM_VECTOR: return NID_ELEM_VECTOR;
  case VE_ELEM_KEYBOARD: return NID_ELEM_KEYBOARD;
  }
  logmsg("unknown ve element type: %d",type);
  return -1; /* unknown? */
}

int make_srv_elem(VeDeviceElement *elem, int devid, int id, NidElementInfo *info,
		  NidElementState *state) {
  int x;
  if (!elem)
    return -1;

  strncpy(info->name,elem->name,NID_DEVICE_STR_SZ);
  info->name[NID_DEVICE_STR_SZ-1] = '\0';
  info->elemid = id;
  state->timestamp = 0;
  state->devid = devid;
  state->elemid = id;
  if ((x = ve_to_nid(elem->content->type)) < 0)
    return -1;
  state->type = info->type = (unsigned)x;

  info->vsize = 0;
  switch (elem->content->type) {
  case VE_ELEM_VECTOR:
    {
      int k;
      VeDeviceE_Vector *v = (VeDeviceE_Vector *)(elem->content);
      info->vsize = v->size < NID_VECTOR_MAX_SZ ? v->size : 
	NID_VECTOR_MAX_SZ;
      for(k = 0; k < info->vsize; k++) {
	info->min[k] = v->min[k];
	info->max[k] = v->max[k];
      }
    }
    break;
  case VE_ELEM_VALUATOR:
    {
      VeDeviceE_Valuator *v = (VeDeviceE_Valuator *)(elem->content);
      info->min[0] = v->min;
      info->max[0] = v->max;
    }
    break;
  }
  return 0;
}

VeNidSrvDevice *make_srv_device(VeDevice *d, int id) {
  VeNidSrvDevice *sd = NULL;
  int esz = 0, eid = 1;

  if (!d)
    return NULL;
  sd = malloc(sizeof(VeNidSrvDevice));
  assert(sd != NULL);
  sd->device = d;
  sd->name = veDupString(d->name);
  if (d->instance && d->instance->driver && d->instance->driver->type)
    sd->type = veDupString(d->instance->driver->type);
  else
    sd->type = veDupString("none");
  sd->id = id;
  sd->report = 0;
  sd->nelems = 0;
  sd->info = NULL;
  sd->state = NULL;

  if (d->model && d->model->elems) {
    VeStrMapWalk w = veStrMapWalkCreate(d->model->elems);
    if (veStrMapWalkFirst(w) == 0) {
      do {
	if (esz == 0) {
	  esz = 5;
	  sd->info = malloc(sizeof(NidElementInfo)*esz);
	  assert(sd->info != NULL);
	  sd->state = malloc(sizeof(NidElementState)*esz);
	  assert(sd->state != NULL);
	} else if (sd->nelems >= esz) {
	  while (sd->nelems >= esz)
	    esz *= 2;
	  sd->info = realloc(sd->info,sizeof(NidElementInfo)*esz);
	  assert(sd->info != NULL);
	  sd->state = realloc(sd->state,sizeof(NidElementState)*esz);
	  assert(sd->state != NULL);
	}
	if (make_srv_elem((VeDeviceElement *)veStrMapWalkObj(w),id,eid,&(sd->info[sd->nelems]),
			  &(sd->state[sd->nelems])) == 0) {
	  eid++;
	  sd->nelems++;
	}
      } while (veStrMapWalkNext(w) == 0);
    }
    veStrMapWalkDestroy(w);
  }
  return sd;
}

int nid_server_fd = 0; /* this should be true for inetd-style connections */
int update_rate = 100; /* update rate in Hz - how often should we send
                          updates to the server */
int sent_events_avail = 0; /* have we told the client events are available? */
static int event_space = 0;
static int nevents = 0;
static NidElementState *events = NULL;

struct active_conn {
  struct active_conn *next;
  int conn;
} *conns = NULL;

int nidServer_newConn(int fd) {
  int conn;
  struct active_conn *c;
  if ((conn = nidRegister(fd)) < 0)
    return -1;
  c = malloc(sizeof(struct active_conn));
  assert(c != NULL);
  c->conn = conn;
  c->next = conns;
  conns = c;
  return conn;
}

void nidServer_closeConn(int conn) {
  struct active_conn *c, *pre;
  if (conn >= 0) {
    nidClose(conn);
    for(pre = NULL, c = conns; c && c->conn != conn; pre = c, c = c->next)
      ;
    if (c) {
      if (pre)
	pre->next = c->next;
      else
	conns = c->next;
      free(c);
    }
  }
  if (!conns)
    /* no more connections */
    exit(0);
}

void nidServer_addEvent(NidElementState *s) {
  int i, last = -1;

  if (event_space == 0) {
    event_space = 32;
    events = malloc(sizeof(NidElementState)*event_space);
    assert(events != NULL);
  }
  
  while(nevents >= event_space) {
    event_space *= 2;
    events = realloc(events,sizeof(NidElementState)*event_space);
  }
  events[nevents++] = *s;
}

void nidServer_flushEvents(int req) {
  if (nevents > 0) {
    NidPacket pkt;
    NidStateList sl;
    struct active_conn *c;
    
    _nidLogMsg("flushing events");

    VE_DEBUG(2,("flushing events"));

    pkt.header.type = NID_PKT_ELEMENT_EVENTS;
    pkt.header.request = req;
    pkt.payload = &sl;
    sl.num_states = nevents;
    sl.states = events;
    /* send event to all connections */
    for(c = conns; c; c = c->next)
      nidTransmitPacket(c->conn,&pkt);
    nevents = 0;
    sent_events_avail = 0;
  }
}

VeNidSrvDevice *nidServer_findDevice(int id) {
  int k;
  for(k = 0; k < num_srv_devices; k++)
    if (srv_devices[k] && (srv_devices[k]->id == id))
      return srv_devices[k];
  return NULL;
}

VeNidSrvDevice *nidServer_findDeviceName(char *name) {
  int k;
  if (!name)
    return NULL;
  for(k = 0; k < num_srv_devices; k++)
    if (srv_devices[k] && (strcmp(srv_devices[k]->name,name) == 0))
      return srv_devices[k];
  return NULL;
}

/* we only report events for devices with models */
int event_cback(VeDeviceEvent *e, void *arg) {
  VeNidSrvDevice *d;
  int k,j,vmax;
  if (d = nidServer_findDeviceName(e->device)) {
    for(k = 0; k < d->nelems; k++)
      if (strncmp(d->info[k].name,e->elem,NID_DEVICE_STR_SZ) == 0)
	break;
    if (k >= 0 && k < d->nelems && 
	ve_to_nid(e->content->type) == d->info[k].type) {
      /* update state information and generate nid event*/
      d->state[k].timestamp = e->timestamp;
      switch (e->content->type) {
      case VE_ELEM_TRIGGER: break; /* nothing to do */
      case VE_ELEM_SWITCH:
	d->state[k].data.switch_ = VE_EVENT_SWITCH(e)->state;
	break;
      case VE_ELEM_VALUATOR:
	d->state[k].data.valuator = VE_EVENT_VALUATOR(e)->value;
	break;
      case VE_ELEM_KEYBOARD:
	/* !!! DIRTY LITTLE SECRET !!! - I happen to know that 
	   NID keysyms and VE keysyms are equivalent.  Good design
	   should indicate that I have some kind of translation table
	   to save against the day that somebody changes one but not
	   the other.  On ther other hand, I am a lazy, lazy man. 

	   You have been warned.
	*/
	d->state[k].data.keyboard.code = VE_EVENT_KEYBOARD(e)->key;
	d->state[k].data.keyboard.state = VE_EVENT_KEYBOARD(e)->state;
	break;
      case VE_ELEM_VECTOR:
	vmax = VE_EVENT_VECTOR(e)->size;
	if (vmax > NID_VECTOR_MAX_SZ)
	  vmax = NID_VECTOR_MAX_SZ;
	d->state[k].data.vector.size = vmax;
	for(j = 0; j < vmax; j++)
	  d->state[k].data.vector.data[j] = VE_EVENT_VECTOR(e)->value[j];
	break;
      }
      if (d->report) {
	VE_DEBUG(2,("reporting event"));
	nidServer_addEvent(&(d->state[k]));
      } else {
	VE_DEBUG(2,("not reporting event"));
      }	
    }
  }
  return VE_FILT_CONTINUE;
}

void ack(int conn, NidPacket *p) {
  NidPacket pkt;
  pkt.header.type = NID_PKT_ACK;
  pkt.header.request = p->header.request;
  nidTransmitPacket(conn,&pkt);
}

void nak(int conn, NidPacket *p, int reason) {
  NidPacket pkt;
  NidNak nak;
  pkt.header.request = p->header.request;
  pkt.header.type = NID_PKT_NAK;
  pkt.payload = &nak;
  nak.reason = reason;
  nidTransmitPacket(conn,&pkt);
}

void proc_accept(int sfd, int arg) {
  int fd, conn;
  struct sockaddr_in addr;
  int alen;
  alen = sizeof(addr);
  if ((fd = accept(sfd,(struct sockaddr *)&addr,&alen)) < 0) {
    logmsg("bad accept: %s",strerror(errno));
    return;
  }
  if ((conn = nidServer_newConn(fd)) < 0) {
    logmsg("bad incoming call");
    return;
  }
  addfd(fd,proc_pkt,conn);
}

void proc_pkt(int fd, int conn) {
  NidPacket pkt, *p;
  VeNidSrvDevice *d;

  veLockCallbacks();

  if (!(p = nidReceivePacket(conn,-1))) {
    /* bad read or eof */
    remfd(fd);
    nidServer_closeConn(conn);
    veUnlockCallbacks();
    return;
  }

  switch (p->header.type) {
  case NID_PKT_ENUM_ELEMENTS:
    {
      NidEnumElements *e = (NidEnumElements *)(p->payload);
      NidElementList el;
      
      if (!(d = nidServer_findDevice(e->id))) {
        nak(conn,p,-1);
      } else {
        pkt.header.request = p->header.request;
        pkt.header.type = NID_PKT_ELEMENT_LIST;
        pkt.payload = &el;
        el.devid = e->id;
        el.elements = d->info;
        el.num_elems = d->nelems;
        nidTransmitPacket(conn,&pkt);
      }
    }
    break;

  case NID_PKT_QUERY_ELEMENTS:
    {
      NidElementRequests *er = (NidElementRequests *)(p->payload);
      NidStateList sl;
      int slsize;
      int i,j;
      
      slsize = 16;
      sl.states = malloc(sizeof(NidElementState)*slsize);
      sl.num_states = 0;
      
      for(i = 0; i < er->num_requests; i++) {
        if (d = nidServer_findDevice(er->requests[i].devid)) {
          for(j = 0; j < d->nelems; j++) {
            if (d->state[j].elemid == er->requests[i].elemid) {
              while (sl.num_states >= slsize) {
                slsize *= 2;
                sl.states = realloc(sl.states,sizeof(NidElementState)*slsize);
              }
              sl.states[j] = d->state[j];
              sl.num_states++;
            }
          }
        }
      }
      pkt.header.request = p->header.request;
      pkt.header.type = NID_PKT_ELEMENT_STATES;
      pkt.payload = &sl;
      nidTransmitPacket(conn,&pkt);
      free(sl.states);
    }
    break;

  case NID_PKT_SET_VALUE:
    {
      NidValue *value = (NidValue *)(p->payload);
      if (value->id != NID_VALUE_REFRESH_RATE)
        nak(conn,p,-1);
      else {
        update_rate = value->value;
        if (update_rate <= 0 || update_rate > 100)
          update_rate = 100; /* base value */
        ack(conn,p);
      }
    }
    break;
  case NID_PKT_GET_VALUE:
    {
      NidValue *value = (NidValue *)(p->payload);
      if (value->id != NID_VALUE_REFRESH_RATE)
        nak(conn,p,-1);
      else {
        value->value = update_rate;
        nidTransmitPacket(conn,p);
      }
    }
    break;

  case NID_PKT_ENUM_DEVICES: /* just FIND_DEVICE with wildcards */
  case NID_PKT_FIND_DEVICE:
    {
      int i, nl, tl, nw, tw;
      NidFindDevice *f = (NidFindDevice *)(p->payload);
      NidDeviceList dl;
      int dsize = 10;
      
      dl.devices = malloc(sizeof(NidDeviceInfo)*dsize);
      dl.num_devices = 0;
      
      if (p->header.type == NID_PKT_ENUM_DEVICES)
        nw = tw = 1;
      else {
        nl = tl = NID_DEVICE_STR_SZ; /* limit on significant characters */
        nw = (strcmp(f->name,"*") == 0);
        tw = (strcmp(f->type,"*") == 0);
      }

      for(i = 0; i < num_srv_devices; i++)
        if (srv_devices[i]) {
          if ((nw || (strncmp(f->name,srv_devices[i]->name,nl) == 0)) &&
              (tw || (strncmp(f->type,srv_devices[i]->type,tl) == 0))) {
            /* this device matches...add it to the return list... */
            while (dl.num_devices >= dsize) {
              dsize *= 2;
              dl.devices = realloc(dl.devices,sizeof(NidDeviceInfo)*dsize);
            }
            dl.devices[dl.num_devices].devid = srv_devices[i]->id;
            strncpy(dl.devices[dl.num_devices].name,srv_devices[i]->name,
                    NID_DEVICE_STR_SZ);
            dl.devices[dl.num_devices].name[NID_DEVICE_STR_SZ-1] = '\0';
            strncpy(dl.devices[dl.num_devices].type,srv_devices[i]->type,
                    NID_DEVICE_STR_SZ);
            dl.devices[dl.num_devices].type[NID_DEVICE_STR_SZ-1] = '\0';
            dl.num_devices++;
          }
        }
      
      pkt.header.request = p->header.request;
      pkt.header.type = NID_PKT_DEVICE_LIST;
      pkt.payload = &dl;
      nidTransmitPacket(conn,&pkt);
      free(dl.devices);
    }
    break;

  case NID_PKT_LISTEN_ELEMENTS:
  case NID_PKT_IGNORE_ELEMENTS:
    {
      int on = (p->header.type == NID_PKT_LISTEN_ELEMENTS);
      int i;
      NidElementRequests *r = (NidElementRequests *)(p->payload);

      for(i = 0; i < r->num_requests; i++)
        if (d = nidServer_findDevice(r->requests[i].devid))
          d->report = on;
      ack(conn,p);
    }
    break;

  case NID_PKT_DEVICE_FUNC:
    nak(conn,p,-1); /* not currently supported */
    break;

  case NID_PKT_TIME_SYNCH:
    {
      NidTimeSynch *tm = (p->payload);
      veClockSetRef(tm->refpt,tm->absolute);
    }
    break;

  case NID_PKT_COMPRESS_EVENTS:
    /* compress no longer supported */
    nak(conn,p,-1);
    break;

  case NID_PKT_UNCOMPRESS_EVENTS:
    nak(conn,p,-1);
    break;

  case NID_PKT_DUMP_EVENTS:
    nak(conn,p,-1);
    break;

  case NID_PKT_SET_EVENT_SINK:
    {
      NidEventSink *s = (NidEventSink *)(p->payload);
      if (nidProcessTransport(conn,s->sink))
	nak(conn,p,-1);
      else
	ack(conn,p);
    }
    break;

  default:
    logmsg("unsupported packet type from client: %d",p->header.type);
    nak(conn,p,-1);
  }
  nidFreePacket(p);
  veUnlockCallbacks();
}

void *server_thread(void *z) {
  int tmout = 0;
  addfd(0,proc_pkt,0);
    /* process fd events from here on in */
  while (1) {
    /* calculate timeout in msecs */
    long tm1, tm2, basetmout;
    VE_DEBUG(2,("server thread loop"));
    basetmout = (int)(1000/update_rate);
    if (tmout <= 0) {
      VE_DEBUG(2,("flushing events"));
      nidServer_flushEvents(0);
      tmout = basetmout;
      tm1 = now();
    }
    VE_DEBUG(2,("waiting for activity/timeout (tmout = %d)",tmout));
    scan(tmout);
    tm2 = now();
    tmout = basetmout - (tm2 - tm1); /* subtract elapsed time */
  }
  return NULL;
}

int main(int argc, char **argv) {
  FILE *f;
  int i,fd;

  openlog("venid",LOG_PID,LOG_DAEMON);
  nidLogToSyslog(1);

  /* redirect stderr */
  if ((fd = open("/tmp/nid_server.err",O_WRONLY|O_APPEND|O_CREAT,0644)) >= 0) {
    close(2);
    dup2(fd,2);
    close(fd);
  }

  veInit(&argc,argv);

  /* use all devices in the manifest and create
     server objects for them */
  srv_dev_space = 5;
  srv_devices = malloc(sizeof(VeNidSrvDevice *)*srv_dev_space);
  assert(srv_devices != NULL);

  {
    VeStrMap m;
    VeStrMapWalk w;
    
    if (m = veGetDeviceManifest()) {
      w = veStrMapWalkCreate(m);
      if (veStrMapWalkFirst(w) == 0) {
	do {
	  VeDevice *d;
	  VeNidSrvDevice *sd;
	  if ((d = veDeviceUse(veStrMapWalkStr(w),NULL)) &&
	      (sd = make_srv_device(d,next_dev_id))) {
	    next_dev_id++;
	    if (num_srv_devices >= srv_dev_space) {
	      while (num_srv_devices >= srv_dev_space)
		srv_dev_space *= 2;
	      srv_devices = realloc(srv_devices,sizeof(VeNidSrvDevice *)*srv_dev_space);
	      assert(srv_devices != NULL);
	    }
	    srv_devices[num_srv_devices++] = sd;
	  }
	} while (veStrMapWalkNext(w) == 0);
      }
      veStrMapWalkDestroy(w);
    }
  }

  /* setup a callback to trap all events */
  veDeviceAddCallback(event_cback,NULL,"*.*");

  if (handle_accept)
    addfd(0,proc_accept,0);
  else {
    int conn;
    if ((conn = nidServer_newConn(0)) < 0) {
      logmsg("bad incoming call");
      exit(1);
    }
    addfd(0,proc_pkt,conn);
  }

  /* create a thread for the server */
  veThreadInit(NULL,server_thread,NULL,0,0);
  
  /* start the server running... */
  veThrReleaseDelayed();
  veEventLoop();
}
