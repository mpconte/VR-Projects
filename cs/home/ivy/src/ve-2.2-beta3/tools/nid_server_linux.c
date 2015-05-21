/* driver for Linux input */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/* should add mouse and keyboard here... */
#include <linux/joystick.h>

#include <nid_server.h>

/* implicit */
#define LINUX_JS_MAX   32767
#define LINUX_JS_AXIS_NOISE   900

struct linux_dev_private {
  char *path;
  int fd;
  int nbuttons, naxes;
  int report;
};

extern void nak(NidPacket *p, int fd, int reson);
extern void ack(NidPacket *p, int fd);

static void free_if(void *v) {
  if (v) free(v);
}

static void js_handler(int fd, void *d) {
  NidServerDevice *dev = d;
  struct linux_dev_private *priv = dev->priv;
  struct js_event e;
  float axisval;
  int is_init, i;
  
  if ((i = read(fd,&e,sizeof(e))) != sizeof(e)) {
    if (i == 0)
      logmsg("%s - connection closed?",priv->path);
    else
      logmsg("%s - short read - %s",priv->path,strerror(errno));
    remfd(fd);
    close(fd); /* no further updates */
  }

  is_init = e.type & JS_EVENT_INIT;
  e.type &= ~JS_EVENT_INIT;
  switch (e.type) {
  case JS_EVENT_BUTTON:
    i = priv->naxes+e.number;
    assert(i >= 0 && i < priv->nbuttons+priv->naxes);
    dev->state[i].timestamp = veClock();
    dev->state[i].data.switch_ = e.value ? 1 : 0;
    if (!is_init && priv->report)
      nidServer_addEvent(&(dev->state[i]));
    break;
  case JS_EVENT_AXIS:
    i = e.number;
    assert(i >= 0 && i < priv->nbuttons+priv->naxes);
    axisval = e.value/(float)LINUX_JS_MAX;
    if (fabs(axisval-dev->state[i].data.valuator) < (LINUX_JS_AXIS_NOISE/(float)LINUX_JS_MAX))
	break; /* too small a change... */
    dev->state[i].timestamp = veClock();
    dev->state[i].data.valuator = e.value/(float)LINUX_JS_MAX;
    /* clamp value to min,max */
    if (dev->state[i].data.valuator < dev->info[i].min[0])
      dev->state[i].data.valuator = dev->info[i].min[0];
    else if (dev->state[i].data.valuator > dev->info[i].max[0])
      dev->state[i].data.valuator = dev->info[i].max[0];
    if (!is_init && priv->report)
      nidServer_addEvent(&(dev->state[i]));
    break;
  }
}

static void init_js(NidServerDriver *d, char *path, int fd) {
  char naxes, nbuttons; /* the joystick ioctl's expect char's */
  int nelems, i;
  char str[NID_DEVICE_STR_SZ];
  NidServerDevice *dev;
  struct linux_dev_private *priv;

  if (ioctl(fd,JSIOCGBUTTONS,&nbuttons) < 0) {
    logmsg("%s - cannot query buttons", path);
    close(fd);
    return;
  }
  if (ioctl(fd,JSIOCGAXES,&naxes) < 0) {
    logmsg("%s - cannot query axes", path);
    close(fd);
    return;
  }
  nelems = naxes + nbuttons;

  dev = malloc(sizeof(NidServerDevice));
  assert(dev != NULL);
  dev->driver = d;
  dev->info = calloc(nelems,sizeof(NidElementInfo));
  assert(dev->info != NULL);
  dev->state = calloc(nelems,sizeof(NidElementState));
  assert(dev->state != NULL);
  dev->nelems = nelems;
  str[0] = '\0';
  dev->devid = nidServer_uniqueId();
  sprintf(str,"device%d",dev->devid);
  dev->name = strdup(str);
  /* use name of device as type - we may have multiple of the same device
     accessible here */
  if (ioctl(fd,JSIOCGNAME(NID_DEVICE_STR_SZ),str) >= 0 && str[0] != '\0')
    dev->type = strdup(str);
  else /* no specific type information available */
    dev->type = strdup("joystick");

  /* setup elements */
  for(i = 0; i < naxes; i++) {
    sprintf(dev->info[i].name,"axis%d",i);
    dev->info[i].elemid = i+1;
    dev->info[i].type = NID_ELEM_VALUATOR;
    dev->info[i].vsize = 0;
    dev->info[i].min[0] = -1.0;
    dev->info[i].max[0] = 1.0;
    /* create initial state info */
    dev->state[i].timestamp = veClock();
    dev->state[i].devid = dev->devid;
    dev->state[i].elemid = dev->info[i].elemid;
    dev->state[i].type = dev->info[i].type;
    dev->state[i].data.valuator = 0.0;
  }
  for(i = 0; i < nbuttons; i++) {
    sprintf(dev->info[naxes+i].name,"button%d",i);
    dev->info[naxes+i].elemid = naxes+i+1;
    dev->info[naxes+i].type = NID_ELEM_SWITCH;
    dev->info[naxes+i].vsize = 0;
    /* create initial state info */
    dev->state[naxes+i].timestamp = veClock();
    dev->state[naxes+i].devid = dev->devid;
    dev->state[naxes+i].elemid = dev->info[naxes+i].elemid;
    dev->state[naxes+i].type = dev->info[naxes+i].type;
    dev->state[naxes+i].data.switch_ = 0;
  }

  /* setup private structure */
  priv = malloc(sizeof(struct linux_dev_private));
  assert(priv != NULL);
  priv->path = strdup(path);
  priv->fd = fd;
  priv->nbuttons = nbuttons;
  priv->naxes = naxes;
  priv->report = 0; /* don't forward events unless asked */
  dev->priv = priv;

  /* add this device and its event handler */
  nidServer_addDevice(dev);
  addfd(fd,js_handler,(void *)dev);
}

static void probe_js(NidServerDriver *d) {
  static char *base[] = {
    "/dev/js%d",
    "/dev/input/js%d",
    NULL
  };
  static int jsmax = 32;
  char path[256];
  int i,j,fd;

  for(i = 0; base[i]; i++) {
    for(j = 0; j < jsmax; j++) {
      sprintf(path,base[i],j);
      if ((fd = open(path,O_RDONLY)) >= 0)
	init_js(d,path,fd);
    }
  }
}

static void linux_server_init(NidServerDriver *d) {
  /* probe all possible locations for devices */
  probe_js(d);
}

static void linux_server_report(NidServerDevice *d, int on, int elemid) {
  /* for individual devices, either report all or report none - storing
     the filtering is not worth the complexity - clients are required to
     be able to cope with extraneous events for just this reason */
  /* so, if anything is requested for a particular device, report everything */
  struct linux_dev_private *priv = d->priv;
  priv->report = on ? 1 : 0;
}

static void linux_server_destroy(NidServerDevice *d) {
  struct linux_dev_private *priv = d->priv;
  remfd(priv->fd);
  close(priv->fd);
  free_if(priv->path);
  free_if(priv);
  free_if(d->name);
  free_if(d->type);
  free_if(d->info);
  free_if(d->state);
  free_if(d);
}

static void linux_server_request(NidServerDevice *d, NidPacket *pkt) {
  /* handle non-API requests (DEVICE_FUNC) */
  struct linux_dev_private *priv = d->priv;
  nak(pkt,priv->fd,-1);
}

NidServerDriver linuxInputDriver = {
  "linuxInput",
  linux_server_init,
  linux_server_report,
  linux_server_destroy,
  linux_server_request
};
