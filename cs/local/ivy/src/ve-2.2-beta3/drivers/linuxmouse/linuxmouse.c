#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>

#include <ve_debug.h>
#include <ve_device.h>
#include <ve_driver.h>
#include <ve_keysym.h>
#include <ve_clock.h>

#define MODULE "driver:linuxmouse"

struct driver_linuxmouse_private {
  int fd;  /* file descriptor for event handle */
};

static char *key2name(int key) {
  switch (key) {
  case BTN_LEFT:  return "left";
  case BTN_RIGHT:  return "right";
  case BTN_MIDDLE: return "middle";
  }
  return NULL;
}

static char *axis2name(int axis) {
  switch (axis) {
  case REL_X:  return "x";
  case REL_Y:  return "y";
  case REL_Z:  return "z";
  case REL_HWHEEL:  return "hwheel";
  case REL_DIAL:    return "dial";
  case REL_WHEEL:   return "wheel";
  case REL_MISC:    return "misc";
  case REL_MAX:     return "max";
  }
  return NULL;
}

#define MAXDEVS 32
#define NAMESZ  80
static int find_devname(char *dspec) {
  int k, fd;
  char name[NAMESZ];
  char path[256];

  for(k = 0; k < MAXDEVS; k++) {
    sprintf(path,"/dev/input/event%d",k);
    if ((fd = open(path,O_RDONLY)) >= 0) {
      if (ioctl(fd,EVIOCGNAME(NAMESZ),name) < 0)
	veWarning(MODULE,"cannot query name of event device %s",path);
      else {
	/* convert name to lower-case */
	char *s;
	for(s = name; *s; s++)
	  *s = tolower(*s);
	if (strstr(s,dspec))
	  return fd; /* found */
      }
      close(fd);
    }
  }
  return -1;
}

static void *mouse_thread(void *v) {
  struct input_event ev;
  VeDeviceEvent *ve;
  VeDeviceE_Switch *sw;
  VeDeviceE_Valuator *val;
  VeDevice *d = (VeDevice *)v;
  struct driver_linuxmouse_private *p = (struct driver_linuxmouse_private *)
    (d->instance->idata);
  
  VE_DEBUG(1,("linuxmouse %s: thread started", d->name));
  
  while(read(p->fd,&ev,sizeof(ev)) == sizeof(ev)) {
    char *elem;
    switch (ev.type) {
    case EV_REL:  /* relative axis motion */
      if ((elem = axis2name(ev.code))) {
	ve = veDeviceEventCreate(VE_ELEM_VALUATOR,0);
	ve->device = veDupString(d->name);
	ve->elem = veDupString(elem);
	ve->timestamp = veClockConvTimespec(ev.time.tv_sec,ev.time.tv_usec);
	val = VE_EVENT_VALUATOR(ve);
	val->min = val->max = 0.0;
	val->value = (float)(*(int *)(&ev.value));
	VE_DEBUG(2,("linuxmouse %s: rel motion %s %f",d->name,elem,
		    val->value));
	veDeviceInsertEvent(ve);
      }
      break;
    case EV_KEY:  /* button */
      if ((elem = key2name(ev.code))) {
	ve = veDeviceEventCreate(VE_ELEM_SWITCH,0);
	ve->device = veDupString(d->name);
	ve->elem = veDupString(elem);
	ve->timestamp = veClockConvTimespec(ev.time.tv_sec,ev.time.tv_usec);
	sw = VE_EVENT_SWITCH(ve);
	sw->state = ev.value;
	VE_DEBUG(2,("linuxmouse %s: button %s %d",d->name,elem,ev.value));
	veDeviceInsertEvent(ve);
      }
      break;
    }
  }
  
  veError(MODULE,"bad read from event handle: %s",strerror(errno));
  return NULL;
}

static VeDevice *new_mouse_driver(VeDeviceDriver *driver,
				  VeDeviceDesc *desc,
				  VeStrMap override) {
  VeDevice *d;
  VeDeviceInstance *i;
  char *devname = "mouse";  /* by default look for something called "mouse" */
  char *devpath = NULL;
  char *s;
  struct driver_linuxmouse_private *p;
  
  assert(desc != NULL);
  assert(desc->name != NULL);
  assert(desc->type != NULL);

  VE_DEBUG(1,("linuxmouse - creating new mouse driver"));

  i = veDeviceInstanceInit(driver,NULL,desc,override);
  
  devpath = veDeviceInstOption(i,"path");
  if (s = veDeviceInstOption(i,"devname"))
    devname = s;

  p = malloc(sizeof(struct driver_linuxmouse_private));
  assert(p != NULL);

  if (devpath) {
    if ((p->fd = open(devpath,O_RDONLY)) < 0) {
      veError(MODULE,"device %s disabled: cannot open device %s",
	      desc->name, devpath);
      free(p);
      return NULL;
    }
  } else {
    if ((p->fd = find_devname(devname)) < 0) {
      veError(MODULE,"device %s disabled: cannot find device %s",
	      desc->name, devname);
      free(p);
      return NULL;
    }
  }

  d = veDeviceCreate(desc->name);
  i->idata = (void *)p;
  d->instance = i;
  d->model = veDeviceCreateModel();
  /* pretend that it is a 3 button mouse */
  veDeviceAddElemSpec(d->model,"raxes vector 2 {0.0 0.0} {0.0 0.0}");
  veDeviceAddElemSpec(d->model,"left switch");
  veDeviceAddElemSpec(d->model,"middle switch");
  veDeviceAddElemSpec(d->model,"right switch");
  veThreadInitDelayed(mouse_thread,d,0,0);
  return d;
}

static VeDeviceDriver mouse_drv = {
  "linuxmouse", new_mouse_driver
};

void VE_DRIVER_INIT(linuxmouse) (void) {
  veDeviceAddDriver(&mouse_drv);
}

void VE_DRIVER_PROBE(linuxmouse) (void *phdl) {
  veDriverProvide(phdl,"input","linuxmouse");
}
