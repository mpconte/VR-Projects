/* A driver for HID-based joysticks, gamepads and the like under
   Mac OS X */
/* Only tested on 10.3 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "HID_Utilities_External.h"
#include <Kernel/IOKit/hid/IOHIDUsageTables.h>
#include <Kernel/IOKit/hid/IOHIDKeys.h>

#include <ve_alloc.h>
#include <ve_debug.h>
#include <ve_device.h>
#include <ve_driver.h>

#define MODULE "driver:macjs"

struct driver_macjs_private {
  pRecDevice d;
  int nelem;
  pRecElement *elements;
  char **enames; /* cached element names */
};

#define MAX_NAME_SZ 512

/* For some reason, the vast majority of axes are *miscellaneous*
   rather than *axis* */

static void push_macjs_event(VeDevice *d, IOHIDEventStruct *ev) {
  VeDeviceEvent *ve;
  pRecElement e;
  struct driver_macjs_private *p =
    (struct driver_macjs_private *)(d->instance->idata);
  int i;
  char *ename = NULL;

  VE_DEBUGM(5,("pushing event"));

  e = NULL;
  for (i = 0; i < p->nelem; i++) {
    if (p->elements[i]->cookie == (void *)ev->elementCookie) {
      e = p->elements[i];
      ename = p->enames[i];
      break;
    }
  }
  if (!e) {
    veError(MODULE,"device %s: received event with unidentifiable element",
	    d->name);
    return;
  }
  switch (e->type) {
  case kIOHIDElementTypeInput_Misc:
    /* actually an axis */
    {
      VeDeviceE_Valuator *val;
      ve = veDeviceEventCreate(VE_ELEM_VALUATOR,0);
      ve->device = veDupString(d->name);
      ve->elem = veDupString(ename);
      ve->timestamp = veClock();
      val = VE_EVENT_VALUATOR(ve);
      if (e->min < 0) {
	/* scale -1.0 to 1.0 (assume even split) */
	val->min = -1.0;
	val->max = 1.0;
	val->value = 2.0*(ev->value - e->min)/(float)(e->max - e->min) - 1.0;
      } else {
	/* scale from 0.0 to 1.0 */
	val->min = 0.0;
	val->max = 1.0;
	val->value = (ev->value - e->min)/(float)(e->max - e->min);
      }
      veDeviceApplyEventToModel(d->model,ve);
      veDeviceInsertEvent(ve);
    }
    break;

  case kIOHIDElementTypeInput_Button:
    {
      VeDeviceE_Switch *sw;
      ve = veDeviceEventCreate(VE_ELEM_SWITCH,0);
      ve->device = veDupString(d->name);
      ve->elem = veDupString(ename);
      ve->timestamp = veClock();
      sw = VE_EVENT_SWITCH(ve);
      sw->state = ev->value;
      veDeviceApplyEventToModel(d->model,ve);
      veDeviceInsertEvent(ve);
    }
    break;

  case kIOHIDElementTypeInput_Axis: /* wtf? */
    veError(MODULE,"got an 'axis' type element for an event");
    veError(MODULE,"I have never seen these before");
    veError(MODULE,"Let Matt know about this (and what device you're using)");
    return; /* ignore */

  case kIOHIDElementTypeInput_ScanCodes:
    return; /* silently ignore */

  default:
    veError(MODULE,"unexpected non-Input element in event for device");
    return; /* ignore */
  }
}

/* is there a better way to do this loop? */
static void *macjs_thread(void *z) {
  VeDevice *d;
  struct driver_macjs_private *p;
  IOHIDEventStruct ev;

  VE_DEBUGM(3,("macjs thread starting"));

  d = (VeDevice *)z;
  p = (struct driver_macjs_private *)(d->instance->idata);
  
  VE_DEBUGM(3,("queueing all elements on device"));
  HIDQueueDevice(p->d);

  VE_DEBUGM(3,("entering loop"));
  while (1) {
    if (HIDGetEvent(p->d,(void *)&ev)) {
      veLockFrame();
      VE_DEBUGM(4,("starting event processing"));
      do {
	push_macjs_event(d,&ev);
      } while (HIDGetEvent(p->d,(void *)&ev));
      VE_DEBUGM(4,("event processing finished"));
      veUnlockFrame();
    }
    veMicroSleep(1000);
  }
  
}

static VeDevice *new_macjs_driver(VeDeviceDriver *driver,
				  VeDeviceDesc *desc,
				  VeStrMap override) {
  struct driver_macjs_private *p;
  int compat_names = 0; /* use Mac names by default... */
  VeDevice *d;
  VeDeviceInstance *i;
  pRecDevice dev = NULL;
  char *s;

  if (strcmp(driver->type,"joystick") == 0)
    compat_names = 1; /* use compat names by default for generic
			 driver */
  
  assert(desc != NULL);
  assert(desc->name != NULL);
  assert(desc->type != NULL);

  VE_DEBUGM(1,("macjs - creating new joystick driver"));
  
  i = veDeviceInstanceInit(driver,NULL,desc,override);
 
  {
    static int init = 0;
    if (!init) {
  	HIDBuildDeviceList(0,0);
  	if (!HIDHaveDeviceList()) {
	    veError(MODULE,"device %s disabled: cannot build HID device list",
		    desc->name);
	    return NULL;
	}
	init = 1;
    }
  }

  /* find the device */
  if ((s = veDeviceInstOption(i,"match"))) {
    /* match name against a regular expression */
    int index = 0, count = 0;
    regex_t re;
    if ((s = veDeviceInstOption(i,"index")))
      index = atoi(s);
    if (regcomp(&re,s,REG_EXTENDED|REG_NOSUB)) {
      veError(MODULE, "device %s disabled: match has bad regular expression: %s",
	      desc->name, s);
      return NULL;
    }
    dev = HIDGetFirstDevice();
    while (dev) {
      char ustr[MAX_NAME_SZ];
      /* ignore mice and keyboards... */
      if (dev->usagePage == kHIDPage_GenericDesktop &&
	  dev->usage != kHIDUsage_GD_Keyboard &&
	  dev->usage != kHIDUsage_GD_Mouse) {
	VE_DEBUGM(2,("examining '%s'",dev->product));
	if (regexec(&re,dev->product,0,NULL,0) == 0) {
	  if (index == count)
	    break;
	  else {
	    VE_DEBUGM(2,("skipping '%s' index=%d count=%d",
			 dev->product, index, count));
	    count++;
	  }
	}
      }
      dev = HIDGetNextDevice(dev);
    }
    regfree(&re);
    if (!dev) {
      veError(MODULE,"device %s disabled: cannot find matching device",
	      desc->name);
      return NULL;
    }
  } else {
    /* take first joystick-like object */
    int index = 0, count = 0;
    dev = HIDGetFirstDevice();
    if ((s = veDeviceInstOption(i,"index")))
      index = atoi(s);
    while (dev) {
      char ustr[MAX_NAME_SZ];
      /* ignore mice and keyboards... */
      if (dev->usagePage == kHIDPage_GenericDesktop &&
	  dev->usage != kHIDUsage_GD_Keyboard &&
	  dev->usage != kHIDUsage_GD_Mouse) {
	VE_DEBUGM(2,("chose '%s'",dev->product));

	if (count == index)
	  break;

	VE_DEBUGM(2,("skipping '%s' index=%d count=%d",dev->product,
		     index,count));
	count++;
      }
      dev = HIDGetNextDevice(dev);
    }
    if (!dev) {
      veError(MODULE,"device %s disabled: no joysticks found",
	      desc->name);
      return NULL;
    }
  }

  if ((s = veDeviceInstOption(i,"compat")))
    compat_names = (atoi(s) ? 1 : 0);

  if (compat_names) {
    VE_DEBUGM(2,("using compatibility names"));
  }
  
  VE_DEBUGM(2,("picked device - creating device object"));

  d = veDeviceCreate(desc->name);
  d->instance = i;
  p = calloc(1,sizeof(struct driver_macjs_private));
  d->instance->idata = (void *)p;
  assert(p != NULL);

  VE_DEBUGM(2,("building element list"));

  /* build elements */
  {
    pRecElement e;
    int axis = 0;
    int button = 0;
    int k = 0;

    p->d = dev;
    p->nelem = HIDCountDeviceElements(p->d,kHIDElementTypeInput);
    p->elements = calloc(p->nelem,sizeof(pRecElement));
    assert(p->elements != NULL);
    p->enames = calloc(p->nelem,sizeof(char *));
    assert(p->enames != NULL);
    e = HIDGetFirstDeviceElement(p->d,kHIDElementTypeInput);
    while (e) {
      char estr[MAX_NAME_SZ];
      p->elements[k] = e;
      if (compat_names) {
	if (e->type == kIOHIDElementTypeInput_Button) {
	  sprintf(estr,"button%d",button);
	  button++;
	} else {
	  sprintf(estr,"axis%d",axis);
	  axis++;
	}
      } else {
	HIDGetUsageName(e->usagePage,e->usage,estr);
      }
      VE_DEBUGM(3,("adding element: '%s'",estr));
      p->enames[k] = veDupString(estr);
      k++;
      e = HIDGetNextDeviceElement(e,kHIDElementTypeInput);
    }
  }
  
  VE_DEBUGM(3,("creating delayed thread"));
  veThreadInitDelayed(macjs_thread,d,0,0);
  return d;
}

/* mac-specific */
static VeDeviceDriver macjs_drv = {
  "macjs", new_macjs_driver
};

/* generic */
static VeDeviceDriver joystick_drv = {
  "joystick", new_macjs_driver
};

void VE_DRIVER_INIT(macjs)(void) {
  veDeviceAddDriver(&macjs_drv);
  veDeviceAddDriver(&joystick_drv);
}

void VE_DRIVER_PROBE(macjs)(void *phdl) {
  veDriverProvide(phdl,"input","macjs");
  veDriverProvide(phdl,"input","joystick");
}

