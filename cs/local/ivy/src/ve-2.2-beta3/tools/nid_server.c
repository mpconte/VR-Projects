/* A generic server framework for NID servers - meant to be run out of
   inetd, or something else that can spawn servers. */
/* Assumes a POSIX-like environment.  Other environments may be supportable
   by porting the scan.h interface. */
/* A simplifying assumption throughout this - there is a single connection
   per server process (a la inetd) so drivers do not need to manage individual
   connections. */

/* The set of available devices is defined by the set of drivers below */
/* When initialized, each driver searches for devices it knows */
/* In an imminent revision, there will be a configuration file, from which
   drivers can read specific bits of data (e.g. a device's serial port and
   speed) */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>

#include "nid_server.h"
#include "scan.h"

#ifdef linux
extern NidServerDriver linuxInputDriver;
#endif /* linux */

static NidServerDriver *drivers[] = {
#ifdef linux
  &linuxInputDriver, /* handle new-style joystick, mouse, keyboard inputs */
#endif /* linux */
  NULL
};

/* timeout for scan - we'll just loop around anyways */
#define SCAN_TIMEOUT 1000

int nid_server_fd = 0; /* this should be true for inetd-style connections */
int update_rate = 60; /* update rate in Hz - how often should we send
			  updates to the server */
int sent_events_avail = 0; /* have we told the client events are available? */

static NidServerDevice **devices = NULL;
static int device_alloc = 0;
static int device_max = 0;

static int id_to_index(int i) {
  int j;
  for(j = 0; j < device_max; j++)
    if (devices[j] && devices[j]->devid == i)
      return j;
  return -1;
}

static int index_to_id(int i) { 
  if (i >= 0 && i < device_max && devices[i])
    return devices[i]->devid;
  return -1;
}

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

static int compress = 0;
static int event_space = 0;
static int nevents = 0;
static NidElementState *events = NULL;

void nidServer_addEvent(NidElementState *s) {
  int i, last = -1;

  if (event_space == 0) {
    event_space = 32;
    events = malloc(sizeof(NidElementState)*event_space);
    assert(events != NULL);
  }

  /* compress valuators as much as possible */
  /* note that this violates some other properties (time-ordering) in
     favour of lower latency */
  if (compress) { 
    if (!sent_events_avail) {
      /* send available packet */
      NidPacket p;
      p.header.request = 0;
      p.header.type = NID_PKT_EVENTS_AVAIL;
      p.payload = NULL;
      nidTransmitPacket(nid_server_fd,&p);
      sent_events_avail = 1;
    }
    if (0 && s->type == NID_ELEM_VALUATOR) {
      for(i = 0; i < nevents; i++) {
	if (events[i].devid == s->devid &&
	    events[i].elemid == s->elemid)
	  last = i;
	if (events[i].type != NID_ELEM_VALUATOR)
	  last = -1;
      }
      if (last != -1) {
	/* there is already an event for this valuator - stuff the new value
	   here */
	events[last] = *s;
	return;
      }
    }
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
    
    pkt.header.type = NID_PKT_ELEMENT_EVENTS;
    pkt.header.request = req;
    pkt.payload = &sl;
    sl.num_states = nevents;
    sl.states = events;
    nidTransmitPacket(nid_server_fd,&pkt);
    nevents = 0;
    sent_events_avail = 0;
  }
}

int nidServer_uniqueId() {
  static int id = 0;
  id++;
  return id;
}

NidServerDevice *nidServer_findDevice(int devid) {
  int index;
  index = id_to_index(devid);
  if (index < 0 || index >= device_max)
    return NULL;
  return devices[index];
}

void nidServer_addDevice(NidServerDevice *dev) {
  int i;
  if (device_alloc == 0) {
    device_alloc = 16; /* default */
    devices = calloc(device_alloc,sizeof(NidServerDevice));
    assert(devices != NULL);
  }
  
  for(i = 0; i < device_max; i++)
    if (!(devices[i]))
      break;
  
  if (i >= device_max) {
    while (device_max >= device_alloc) {
      device_alloc *= 2;
      devices = realloc(devices,sizeof(NidServerDevice)*device_alloc);
      assert(devices != NULL);
    }
    device_max++;
  }

  devices[i] = dev;
}

void ack(NidPacket *p, int fd) {
  NidPacket pkt;
  pkt.header.type = NID_PKT_ACK;
  pkt.header.request = p->header.request;
  nidTransmitPacket(fd,&pkt);
}

void nak(NidPacket *p, int fd, int reason) {
  NidPacket pkt;
  NidNak nak;
  pkt.header.request = p->header.request;
  pkt.header.type = NID_PKT_NAK;
  pkt.payload = &nak;
  nak.reason = reason;
  nidTransmitPacket(fd,&pkt);
}

void proc_pkt(int fd, int arg) {
  NidPacket pkt, *p;
  NidServerDevice *d;

  if (!(p = nidReceivePacket(fd,-1))) {
    /* this failed - is the socket okay...? */
    logmsg("bad read from client");
    exit(-1);
  }

  switch(p->header.type) {
  case NID_PKT_ENUM_ELEMENTS:
    {
      NidEnumElements *e = (NidEnumElements *)(p->payload);
      NidElementList el;
      
      if (!(d = nidServer_findDevice(e->id))) {
	nak(p,fd,-1);
      } else {
	pkt.header.request = p->header.request;
	pkt.header.type = NID_PKT_ELEMENT_LIST;
	pkt.payload = &el;
	el.devid = e->id;
	el.elements = d->info;
	el.num_elems = d->nelems;
	nidTransmitPacket(fd,&pkt);
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
      nidTransmitPacket(fd,&pkt);
      free(sl.states);
    }
    break;
  case NID_PKT_SET_VALUE:
    {
      NidValue *value = (NidValue *)(p->payload);
      if (value->id != NID_VALUE_REFRESH_RATE)
	nak(p,fd,-1);
      else {
	update_rate = value->value;
	if (update_rate <= 0 || update_rate > 100)
	  update_rate = 100; /* base value */
	ack(p,fd);
      }
    }
    break;
  case NID_PKT_GET_VALUE:
    {
      NidValue *value = (NidValue *)(p->payload);
      if (value->id != NID_VALUE_REFRESH_RATE)
	nak(p,fd,-1);
      else {
	value->value = update_rate;
	nidTransmitPacket(nid_server_fd,p);
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

      for(i = 0; i < device_max; i++)
	if (devices[i]) {
	  if ((nw || (strncmp(f->name,devices[i]->name,nl) == 0)) &&
	      (tw || (strncmp(f->type,devices[i]->type,tl) == 0))) {
	    /* this device matches...add it to the return list... */
	    while (dl.num_devices >= dsize) {
	      dsize *= 2;
	      dl.devices = realloc(dl.devices,sizeof(NidDeviceInfo)*dsize);
	    }
	    dl.devices[dl.num_devices].devid = devices[i]->devid;
	    strncpy(dl.devices[dl.num_devices].name,devices[i]->name,
		    NID_DEVICE_STR_SZ);
	    dl.devices[dl.num_devices].name[NID_DEVICE_STR_SZ-1] = '\0';
	    strncpy(dl.devices[dl.num_devices].type,devices[i]->type,
		    NID_DEVICE_STR_SZ);
	    dl.devices[dl.num_devices].type[NID_DEVICE_STR_SZ-1] = '\0';
	    dl.num_devices++;
	  }
	}
      
      pkt.header.request = p->header.request;
      pkt.header.type = NID_PKT_DEVICE_LIST;
      pkt.payload = &dl;
      nidTransmitPacket(fd,&pkt);
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
	  d->driver->report(d,on,r->requests[i].elemid);
      ack(p,fd);
    }
    break;
  case NID_PKT_DEVICE_FUNC: /* pass through to device */
    {
      int devid;
      devid = ((NidDeviceFunc *)(p->payload))->devid;
      if (!(d = nidServer_findDevice(devid))) {
	logmsg("DEVICE_FUNC: invalid device id %d", devid);
	nak(p,fd,-1);
      } else {
	if (d->driver->func)
	  d->driver->func(d,p);
	else
	  nak(p,fd,-1);
      }
    }
    break;
  case NID_PKT_TIME_SYNCH:
    {
      NidTimeSynch *tm = (p->payload);
      veClockSetRef(tm->refpt,tm->absolute);
    }
    break;

  case NID_PKT_COMPRESS_EVENTS:
    compress = 1;
    sent_events_avail = 0;
    ack(p,fd);
    break;

  case NID_PKT_UNCOMPRESS_EVENTS:
    compress = 0;
    ack(p,fd);
    break;

  case NID_PKT_DUMP_EVENTS:
    if (!compress)
      nak(p,fd,-1);
    else
      nidServer_flushEvents(p->header.request);
    break;

  default:
    logmsg("unsupported packet type from client: %d",p->header.type);
    nak(p,fd,-1);
  }
  nidFreePacket(p);
}

int main(int argc, char **argv) {
  int i,fd,tmout = -1;

  openlog("nid",LOG_PID,LOG_DAEMON);
  nidLogToSyslog(1);

  /* redirect stderr */
  if ((fd = open("/tmp/nid_server.err",O_WRONLY|O_APPEND|O_CREAT,0644)) >= 0) {
    close(2);
    dup2(fd,2);
    close(fd);
  }

  if ((i = nidRegister(0)) < 0) {
    logmsg("bad incoming call");
    exit(1);
  }
  
  /* driver initialization should add itself to any loops */
  for(i = 0; drivers[i]; i++)
    drivers[i]->init(drivers[i]);

  addfd(0,proc_pkt,0); /* setup handler for incoming requests */

  /* process fd events from here on in */
  while (1) {
    /* calculate timeout in msecs */
    long tm1, tm2, basetmout;
    basetmout = (int)(1000/update_rate);
    if (tmout <= 0) {
      if (!compress)
	nidServer_flushEvents(0);
      tmout = basetmout;
      tm1 = now();
    }
    scan(tmout);
    tm2 = now();
    tmout = basetmout - (tm2 - tm1); /* subtract elapsed time */
  }

  exit(0);
}

