#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <ve_alloc.h>
#include <ve_clock.h>
#include <ve_debug.h>
#include <ve_driver.h>
#include <ve_tlist.h>
#include <ve_device.h>
#include <ve_keysym.h>
#include <ve_stats.h>
#include <ve_mp.h>

#define MODULE "ve_device"

static VeStrMap active_devices = NULL;

VeDevice *veDeviceCreate(char *name) {
  VeDevice *d = veAllocObj(VeDevice);
  d->name = veDupString(name);
  d->instance = NULL;
  d->model = NULL;
  return d;
}

VeDevice *veDeviceCreateVirtual(char *name, VeDeviceModel *model) {
  VeDevice *d;
  
  if (!active_devices)
    active_devices = veStrMapCreate();
  if (!name)
    return NULL;
  if (veStrMapLookup(active_devices,name))
    return NULL;
  d = veDeviceCreate(name);
  d->model = model;
  veStrMapInsert(active_devices,name,d);
  return d;
}

VeDevice *veDeviceUseByDesc(VeDeviceDesc *desc, VeStrMap override) {
  VeDevice *d;
  VeDeviceDriver *drv;
  char *dpathlist, *cpy;
  char *s;

  if (veMPTestSlaveGuard())
    return NULL;

  VE_DEBUGM(2,("veDeviceUseByDesc: enter"));

  if (!desc) {
    VE_DEBUGM(2,("veDeviceUseByDesc: no description"));
    return NULL;
  }
  if (!active_devices)
    active_devices = veStrMapCreate();
  
  if (desc->name && (d = (VeDevice *)veStrMapLookup(active_devices,desc->name)))
    return d; /* device was previously "used" */

  /* try to load any drivers referenced in the device desc */
  if (desc->options && (dpathlist = veStrMapLookup(desc->options,"driver"))) {
    cpy = veDupString(dpathlist);
    dpathlist = cpy;
    while ((s = veNextLElem(&dpathlist)))
      veLoadDriver(s);
  }
  if (!(drv = veDeviceFindDriver(desc->type))) {
    VE_DEBUGM(2,("failed to find driver for device type %s", desc->type));
    return NULL;
  }
  d = drv->inst(drv,desc,override);
  if (d && desc->name) {
    veStrMapInsert(active_devices,desc->name,d);
  }
  VE_DEBUGM(2,("veDeviceUseByDesc: finished (d = 0x%x)",(unsigned)d));
  return d;
}

VeDevice *veDeviceUse(char *name, VeStrMap override) {
  VeDeviceDesc *desc;

  if (veMPTestSlaveGuard())
    return NULL;

  VE_DEBUGM(2,("veDeviceUse(%s) enter", name ? name : "<null>"));
  if (!(desc = veFindDeviceDesc(name))) {
    VE_DEBUGM(2,("veDeviceUse(%s) no desc", name ? name : "<null>"));
    return NULL;
  }
  return veDeviceUseByDesc(desc,override);
}

int veDeviceIsOptional(char *name, VeStrMap override) {
  VeDeviceDesc *desc;
  char *v;

  if (override && (v = veStrMapLookup(override,"optional")))
    return (atoi(v) ? 1 : 0);
  if ((desc = veFindDeviceDesc(name)) &&
      desc->options && (v = veStrMapLookup(desc->options,"optional")))
    return (atoi(v) ? 1 : 0);
  return 0; /* assume not */
}

VeDevice *veDeviceFind(char *name) {
  if (!active_devices || !name)
    return NULL;
  return veStrMapLookup(active_devices,name);
}

VeDeviceInstance *veDeviceInstanceInit(VeDeviceDriver *driver, void *idata,
				       VeDeviceDesc *desc,
				       VeStrMap override) {
  VeStrMapWalk w;
  VeDeviceInstance *i;

  i = veAllocObj(VeDeviceInstance);
  i->driver = driver;
  i->idata = idata;
  i->options = veStrMapCreate();
  if (desc->options) {
    w = veStrMapWalkCreate(desc->options);
    if (veStrMapWalkFirst(w) == 0) {
      do {
	veStrMapInsert(i->options,veStrMapWalkStr(w),
		       veDupString(veStrMapWalkObj(w)));
      } while (veStrMapWalkNext(w) == 0);
    }
    veStrMapWalkDestroy(w);
  }
  if (override) {
    w = veStrMapWalkCreate(override);
    if (veStrMapWalkFirst(w) == 0) {
      void *v;
      do {
	veFree(veStrMapLookup(i->options,veStrMapWalkStr(w)));
	veStrMapInsert(i->options,veStrMapWalkStr(w),
		       veDupString(veStrMapWalkObj(w)));
      } while (veStrMapWalkNext(w) == 0);
      veStrMapWalkDestroy(w);
    }
  }
  return i;
}

char *veDeviceInstOption(VeDeviceInstance *i, char *name) {
  if (i && i->options && name)
    return (char *)veStrMapLookup(i->options,name);
  return NULL;
}

/* This is here to ensure that it is linked into any binary that uses
   devices so that it is available for drivers to use. */
char *veKeysymToString(int ks) {
  static char chr[2];
  switch (ks) {
  case VEK_BackSpace:  return "BackSpace";
  case VEK_Tab:  return "Tab";
  case VEK_Linefeed:  return "Linefeed";
  case VEK_Clear:  return "Clear";
  case VEK_Return:  return "Return";
  case VEK_Pause:  return "Pause";
  case VEK_Scroll_Lock:  return "Scroll_Lock";
  case VEK_Sys_Req:  return "Sys_Req";
  case VEK_Escape:  return "Escape";
  case VEK_Delete:  return "Delete";
  case VEK_Home:  return "Home";
  case VEK_Left:  return "Left";
  case VEK_Up:  return "Up";
  case VEK_Right:  return "Right";
  case VEK_Down:  return "Down";
  case VEK_Page_Up:  return "Page_Up";
  case VEK_Page_Down:  return "Page_Down";
  case VEK_End:  return "End";
  case VEK_Begin:  return "Begin";
  case VEK_Select:  return "Select";
  case VEK_Print:  return "Print";
  case VEK_Execute:  return "Execute";
  case VEK_Insert:  return "Insert";
  case VEK_Undo:  return "Undo";
  case VEK_Redo:  return "Redo";
  case VEK_Menu:  return "Menu";
  case VEK_Find:  return "Find";
  case VEK_Cancel:  return "Cancel";
  case VEK_Help:  return "Help";
  case VEK_Break:  return "Break";
  case VEK_Mode_switch:  return "Mode_switch";
  case VEK_Num_Lock:  return "Num_Lock";
  case VEK_F1:  return "F1";
  case VEK_F2:  return "F2";
  case VEK_F3:  return "F3";
  case VEK_F4:  return "F4";
  case VEK_F5:  return "F5";
  case VEK_F6:  return "F6";
  case VEK_F7:  return "F7";
  case VEK_F8:  return "F8";
  case VEK_F9:  return "F9";
  case VEK_F10:  return "F10";
  case VEK_F11:  return "F11";
  case VEK_F12:  return "F12";
  case VEK_F13:  return "F13";
  case VEK_F14:  return "F14";
  case VEK_F15:  return "F15";
  case VEK_F16:  return "F16";
  case VEK_F17:  return "F17";
  case VEK_F18:  return "F18";
  case VEK_F19:  return "F19";
  case VEK_F20:  return "F20";
  case VEK_Shift_L:  return "Shift_L";
  case VEK_Shift_R:  return "Shift_R";
  case VEK_Control_L:  return "Control_L";
  case VEK_Control_R:  return "Control_R";
  case VEK_Caps_Lock:  return "Caps_Lock";
  case VEK_Shift_Lock:  return "Shift_Lock";
  case VEK_Meta_L:  return "Meta_L";
  case VEK_Meta_R:  return "Meta_R";
  case VEK_Alt_L:  return "Alt_L";
  case VEK_Alt_R:  return "Alt_R";
  case VEK_Super_L:  return "Super_L";
  case VEK_Super_R:  return "Super_R";
  case VEK_Hyper_L:  return "Hyper_L";
  case VEK_Hyper_R:  return "Hyper_R";
  default:
    if (isprint(ks)) {
      chr[0] = (char)ks;
      chr[1] = '\0';
      return chr;
    }
    return NULL; /* did our best... */
  }
}

int veStringToKeysym(char *str) {
  static VeStrMap sm = NULL;
  int key;

  /* One-time initialization */
  if (!sm) {
    sm = veStrMapCreate();
    veStrMapInsert(sm,"BackSpace",(void *)VEK_BackSpace);
    veStrMapInsert(sm,"Tab",(void *)VEK_Tab);
    veStrMapInsert(sm,"Linefeed",(void *)VEK_Linefeed);
    veStrMapInsert(sm,"Clear",(void *)VEK_Clear);
    veStrMapInsert(sm,"Return",(void *)VEK_Return);
    veStrMapInsert(sm,"Pause",(void *)VEK_Pause);
    veStrMapInsert(sm,"Scroll_Lock",(void *)VEK_Scroll_Lock);
    veStrMapInsert(sm,"Sys_Req",(void *)VEK_Sys_Req);
    veStrMapInsert(sm,"Escape",(void *)VEK_Escape);
    veStrMapInsert(sm,"Delete",(void *)VEK_Delete);
    veStrMapInsert(sm,"Home",(void *)VEK_Home);
    veStrMapInsert(sm,"Left",(void *)VEK_Left);
    veStrMapInsert(sm,"Up",(void *)VEK_Up);
    veStrMapInsert(sm,"Right",(void *)VEK_Right);
    veStrMapInsert(sm,"Down",(void *)VEK_Down);
    veStrMapInsert(sm,"Page_Up",(void *)VEK_Page_Up);
    veStrMapInsert(sm,"Page_Down",(void *)VEK_Page_Down);
    veStrMapInsert(sm,"End",(void *)VEK_End);
    veStrMapInsert(sm,"Begin",(void *)VEK_Begin);
    veStrMapInsert(sm,"Select",(void *)VEK_Select);
    veStrMapInsert(sm,"Print",(void *)VEK_Print);
    veStrMapInsert(sm,"Execute",(void *)VEK_Execute);
    veStrMapInsert(sm,"Insert",(void *)VEK_Insert);
    veStrMapInsert(sm,"Undo",(void *)VEK_Undo);
    veStrMapInsert(sm,"Redo",(void *)VEK_Redo);
    veStrMapInsert(sm,"Menu",(void *)VEK_Menu);
    veStrMapInsert(sm,"Find",(void *)VEK_Find);
    veStrMapInsert(sm,"Cancel",(void *)VEK_Cancel);
    veStrMapInsert(sm,"Help",(void *)VEK_Help);
    veStrMapInsert(sm,"Break",(void *)VEK_Break);
    veStrMapInsert(sm,"Mode_switch",(void *)VEK_Mode_switch);
    veStrMapInsert(sm,"Num_Lock",(void *)VEK_Num_Lock);
    veStrMapInsert(sm,"F1",(void *)VEK_F1);
    veStrMapInsert(sm,"F2",(void *)VEK_F2);
    veStrMapInsert(sm,"F3",(void *)VEK_F3);
    veStrMapInsert(sm,"F4",(void *)VEK_F4);
    veStrMapInsert(sm,"F5",(void *)VEK_F5);
    veStrMapInsert(sm,"F6",(void *)VEK_F6);
    veStrMapInsert(sm,"F7",(void *)VEK_F7);
    veStrMapInsert(sm,"F8",(void *)VEK_F8);
    veStrMapInsert(sm,"F9",(void *)VEK_F9);
    veStrMapInsert(sm,"F10",(void *)VEK_F10);
    veStrMapInsert(sm,"F11",(void *)VEK_F11);
    veStrMapInsert(sm,"F12",(void *)VEK_F12);
    veStrMapInsert(sm,"F13",(void *)VEK_F13);
    veStrMapInsert(sm,"F14",(void *)VEK_F14);
    veStrMapInsert(sm,"F15",(void *)VEK_F15);
    veStrMapInsert(sm,"F16",(void *)VEK_F16);
    veStrMapInsert(sm,"F17",(void *)VEK_F17);
    veStrMapInsert(sm,"F18",(void *)VEK_F18);
    veStrMapInsert(sm,"F19",(void *)VEK_F19);
    veStrMapInsert(sm,"F20",(void *)VEK_F20);
    veStrMapInsert(sm,"Shift_L",(void *)VEK_Shift_L);
    veStrMapInsert(sm,"Shift_R",(void *)VEK_Shift_R);
    veStrMapInsert(sm,"Control_L",(void *)VEK_Control_L);
    veStrMapInsert(sm,"Control_R",(void *)VEK_Control_R);
    veStrMapInsert(sm,"Caps_Lock",(void *)VEK_Caps_Lock);
    veStrMapInsert(sm,"Shift_Lock",(void *)VEK_Shift_Lock);
    veStrMapInsert(sm,"Meta_L",(void *)VEK_Meta_L);
    veStrMapInsert(sm,"Meta_R",(void *)VEK_Meta_R);
    veStrMapInsert(sm,"Alt_L",(void *)VEK_Alt_L);
    veStrMapInsert(sm,"Alt_R",(void *)VEK_Alt_R);
    veStrMapInsert(sm,"Super_L",(void *)VEK_Super_L);
    veStrMapInsert(sm,"Super_R",(void *)VEK_Super_R);
    veStrMapInsert(sm,"Hyper_L",(void *)VEK_Hyper_L);
    veStrMapInsert(sm,"Hyper_R",(void *)VEK_Hyper_R);
  }

  if ((key = (int)veStrMapLookup(sm,str)))
    return key;
  if (strlen(str) == 1 && isprint(*str))
    return (int)(*str);
  return VEK_UNKNOWN;
}

int veDeviceFunc(VeDevice *device, char *func, char *args,
		 char *resp_r, int rsz) {
  if (!device)
    return -1;
  if (!device->instance)
    return -1; /* cannot pass functions to virtual devices */
  if (!device->instance->driver)
    return -1; /* no driver for this device (huh?) */
  if (!device->instance->driver->devfunc)
    return -1; /* driver but no devfunc support */
  return device->instance->driver->devfunc(device,func,args,resp_r,rsz);
}

/* This is a special device and callback which is used to measure latency in the 
   event system */
static VeStatistic *event_latency_stat = NULL;
static int event_latency = 0;

#define LATENCY_DEV "_latency"
#define LATENCY_ELEM "_test"

static int latency_cback(VeDeviceEvent *e, void *arg) {
  int now;
  now = veClock();
  event_latency = now-(e->timestamp);
  veUpdateStatistic(event_latency_stat);
  VE_DEBUGM(2,("latency_cback: received event"));
  return VE_FILT_CONTINUE;
}

static void *latency_thread(void *v) {
  VeDevice *d = (VeDevice *)v;
  VeDeviceEvent *e;
  int interval = 500000; /* update latency once per second */
  while (1) {
    veMicroSleep(interval);
    VE_DEBUGM(2,("latency_thread: creating event"));
    e = veDeviceEventCreate(VE_ELEM_TRIGGER,0);
    e->device = veDupString(d->name);
    e->elem = veDupString(LATENCY_ELEM);
    e->timestamp = veClock();
    veDeviceInsertEvent(e);
  }
}

static VeDevice *new_latency_dev(VeDeviceDriver *driver, VeDeviceDesc *desc,
				 VeStrMap override) {
  VeDevice *d;

  assert(desc != NULL);
  assert(desc->name != NULL);
  
  VE_DEBUGM(1,("initializing event latency measurement driver"));
  d = veDeviceCreate(desc->name);
  d->instance = veDeviceInstanceInit(driver,NULL,desc,override);
  d->model = NULL;

  if (!event_latency_stat) {
    event_latency_stat = veNewStatistic(MODULE, "event_latency", "msecs");
    event_latency_stat->type = VE_STAT_INT;
    event_latency_stat->data = &event_latency;
    veAddStatistic(event_latency_stat);
  }

  /* add a callback */
  veDeviceAddCallback(latency_cback,NULL,LATENCY_DEV "." LATENCY_ELEM);

  veThreadInitDelayed(latency_thread,d,0,0);

  return d;
}

static VeDeviceDriver latency_drv = {
  "_latencydriver", new_latency_dev, NULL
};

static VeDeviceDesc latency_desc = {
  "_latency", "_latencydriver", NULL
};

/* This initialization function sets up the device so it can be used, 
   but does not set it running by default.  To generate latency
   statistics:

   use _latency

   in your devices file, or

   veDeviceUse("_latency")

   in your program.
*/
void veDeviceLatencyInit() {
  veDeviceAddDriver(&latency_drv);
  veAddDeviceDesc(&latency_desc);
}
