/* A driver for joysticks under Linux */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h> /* POSIX regular expressions */
#include <sys/ioctl.h>
#include <sys/types.h>

#include <linux/joystick.h>

#include <ve_debug.h>
#include <ve_device.h>
#include <ve_driver.h>

#define MODULE "driver:linuxjs"

struct driver_linuxjs_private {
  int fd;
};

/* implicit */
#define LINUX_JS_MAX 32767
#define MAX_JS 32
#define JS_AUTO_PATH "/dev/input/js%d"
/* can this be longer than a byte...? */
#define MAX_NAME_SZ 128

#define AXISNAME "axis%d"
#define AXISSPEC "axis%d valuator -1.0 1.0"
#define BUTTONNAME "button%d"
#define BUTTONSPEC "button%d switch"

static void push_js_event(VeDevice *d, struct js_event *e) {
  VeDeviceEvent *ve;
  int is_init;
  char ename[80];
  is_init = e->type & JS_EVENT_INIT;
  e->type &= ~JS_EVENT_INIT;
  switch (e->type) {
  case JS_EVENT_BUTTON:
    {
      VeDeviceE_Switch *sw;
      ve = veDeviceEventCreate(VE_ELEM_SWITCH,0);
      ve->device = veDupString(d->name);
      sprintf(ename,BUTTONNAME,e->number);
      ve->elem = veDupString(ename);
      ve->timestamp = veClock();
      sw = VE_EVENT_SWITCH(ve);
      sw->state = e->value ? 1 : 0;
      veDeviceApplyEventToModel(d->model,ve);
      if (!is_init)
	veDeviceInsertEvent(ve);
    }
    break;
  case JS_EVENT_AXIS:
    {
      VeDeviceE_Valuator *val;
      ve = veDeviceEventCreate(VE_ELEM_VALUATOR,0);
      ve->device = veDupString(d->name);
      sprintf(ename,AXISNAME,e->number);
      ve->elem = veDupString(ename);
      ve->timestamp = veClock();
      val = VE_EVENT_VALUATOR(ve);
      val->min = -1.0;
      val->max = 1.0;
      val->value = e->value/(float)LINUX_JS_MAX;
      veDeviceApplyEventToModel(d->model,ve);
      if (!is_init)
	veDeviceInsertEvent(ve);
    }
    break;
  }
}

static void *js_thread(void *v) {
  VeDevice *d;
  struct driver_linuxjs_private *priv;
  int fd, i;
  struct js_event e;

  d = (VeDevice *)v;
  priv = (struct driver_linuxjs_private *)(d->instance->idata);
  fd = priv->fd;

  while (1) {
    if ((i = read(fd,&e,sizeof(e))) != sizeof(e)) {
      veError(MODULE,"bad read from joystick (%d bytes): %s",
	      i,strerror(errno));
      close(fd);
      return NULL;
    }
    /* we have one event - block rendering while we catch up... */
    veLockFrame();
    push_js_event(d,&e);
    while (vePeekFd(fd) > 0) {
      if ((i = read(fd,&e,sizeof(e))) != sizeof(e)) {
	veUnlockFrame();
	veError(MODULE,"bad read from joystick (%d bytes): %s",
		i,strerror(errno));
	close(fd);
	return NULL;
      }
      push_js_event(d,&e);
    }
    veUnlockFrame();
  }
}

static VeDevice *new_linuxjs_driver(VeDeviceDriver *driver,
				    VeDeviceDesc *desc,
				    VeStrMap override) {
  struct driver_linuxjs_private *priv;
  VeDevice *d;
  VeDeviceInstance *i;
  char *path, name[MAX_NAME_SZ];
  int fd;
  unsigned char nbuttons, naxes; /* important - these are chars! */
  int k;

  assert(desc != NULL);
  assert(desc->name != NULL);
  assert(desc->type != NULL);

  VE_DEBUG(1,("linuxjs - creating new joystick driver"));

  i = veDeviceInstanceInit(driver,NULL,desc,override);

  if ((path = veDeviceInstOption(i,"path")) && strcmp(path,"auto")) {
    /* an explicit path and it is not "auto" */
    if ((fd = open(path,O_RDONLY)) < 0) {
      veError(MODULE, "device %s disabled: cannot open path %s: %s",
	      desc->name, path, strerror(errno));
      return NULL;
    }
  } else {
    /* search for the joystick in known locations */
    char *match, p[1024];
    regex_t re;
    if (match = veDeviceInstOption(i,"match")) {
      if (regcomp(&re,match,REG_EXTENDED|REG_NOSUB)) {
	veError(MODULE, "device %s disabled: match has bad regular expression: %s",
		desc->name, match);
	return NULL;
      }
    }
    fd = -1;
    for(k = 0; k < MAX_JS; k++) {
      sprintf(p,JS_AUTO_PATH,k);
      if ((fd = open(p,O_RDONLY)) >= 0) {
	/* a device exists at this path - is it any good? */
	if (!match || ((ioctl(fd,JSIOCGNAME(MAX_NAME_SZ),name) >= 0) &&
		       regexec(&re,name,0,NULL,0) == 0)) {
	  /* either we did not specify a name, or the regular expression
	     matched */
	  break;
	}
      }
    }
    if (fd < 0) {
      veError(MODULE, "device %s disabled: cannot find desired device",
	      desc->name);
      return NULL;
    }
  }

  /* we have successfully opened a device */
  assert(fd >= 0);

  if (ioctl(fd,JSIOCGBUTTONS,&nbuttons) < 0) {
    veError(MODULE,"disabling device %s: ioctl(JSIOCGBUTTONS) failed: %s",
	    desc->name, strerror(errno));
    return NULL;
  }
  if (ioctl(fd,JSIOCGAXES,&naxes) < 0) {
    veError(MODULE,"disabling device %s: ioctl(JSIOCGAXES) failed: %s",
	    desc->name, strerror(errno));
    return NULL;
  }

  VE_DEBUG(2,("linuxjs - joystick found: %d buttons, %d axes",nbuttons,
	      naxes));

  d = veDeviceCreate(desc->name);
  d->instance = i;
  priv = malloc(sizeof(struct driver_linuxjs_private));
  assert(priv != NULL);
  priv->fd = fd;
  i->idata = (void *)priv;
  d->model = veDeviceCreateModel();
  
  for(k = 0; k < nbuttons; k++) {
    char str[512];
    sprintf(str,BUTTONSPEC,k);
    veDeviceAddElemSpec(d->model,str);
  }
  for(k = 0; k < naxes; k++) {
    char str[512];
    sprintf(str,AXISSPEC,k);
    veDeviceAddElemSpec(d->model,str);
  }
  veThreadInitDelayed(js_thread,d,0,0);
  return d;
}

static VeDeviceDriver linuxjs_drv = {
  "linuxjs", new_linuxjs_driver
};

void VE_DRIVER_INIT(linuxjsdrv) (void) {
  veDeviceAddDriver(&linuxjs_drv);
}

