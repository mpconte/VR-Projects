/* controllers */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_alloc.h>
#include <ve_debug.h>
#include <ve_util.h>
#include <ve_tlist.h>
#include <ve_device.h>

#define MODULE "ve_dev_ctrl"

VeDeviceCtrlDesc *veDeviceCtrlDescCreate(void) {
  VeDeviceCtrlDesc *d = veAllocObj(VeDeviceCtrlDesc);
  d->options = veStrMapCreate();
  return d;
}

void veDeviceCtrlDescDestroy(VeDeviceCtrlDesc *d) {
  if (d) {
    veFree(d->inputname);
    veFree(d->outputname);
    veFree(d->type);
    if (d->options)
      veStrMapDestroy(d->options,veFree);
    veFree(d);
  }
}

void veDeviceCtrlDescSetOption(VeDeviceCtrlDesc *d, char *name, char *value) {
  if (d && name) {
    char *s;
    if (!d->options)
      d->options = veStrMapCreate();
    veFree(veStrMapLookup(d->options,name));
    veStrMapInsert(d->options,name,veDupString(value));
  }
}

char *veDeviceCtrlDescGetOption(VeDeviceCtrlDesc *d, char *name) {
  if (!d || !d->options || !name)
    return NULL;
  return veStrMapLookup(d->options,name);
}

static VeStrMap ctrl_drivers = NULL;

void veDeviceCtrlAddDriver(VeDeviceCtrlDriver *drv) {
  if (!ctrl_drivers)
    ctrl_drivers = veStrMapCreate();
  if (drv && drv->type)
    veStrMapInsert(ctrl_drivers,drv->type,drv);
}

VeDeviceCtrlDriver *veDeviceCtrlFindDriver(char *type) {
  if (!ctrl_drivers || !veStrMapLookup(ctrl_drivers,type)) {
    /* try to get driver level to locate a driver for us */
    (void) veDriverRequire("control",type);
  }
  if (!ctrl_drivers)
    return NULL;
  return (VeDeviceCtrlDriver *)veStrMapLookup(ctrl_drivers,type);
}

static VeDeviceCtrl *ctrl_list_head = NULL, *ctrl_list_tail = NULL;

VeDeviceCtrl *veDeviceCtrlCreate(VeDeviceCtrlDesc *desc) {
  VeDeviceCtrlDriver *d;
  VeDeviceCtrl *c;

  if (!desc || !desc->type) {
    veError(MODULE,"veDeviceCtrlCreate: invalid description (no type)");
    return NULL;
  }
  
  if (!(d = veDeviceCtrlFindDriver(desc->type))) {
    veError(MODULE,"veDeviceCtrlCreate: cannot find driver for '%s'",
	    desc->type);
    return NULL;
  }

  if (!(d->init)) {
    veError(MODULE,"veDeviceCtrlCreate: driver for '%s' has no init function",
	    desc->type);
    return NULL;
  }
  
  c = veAllocObj(VeDeviceCtrl);
  c->inputname = veDupString(desc->inputname);
  c->outputname = veDupString(desc->outputname);
  c->type = veDupString(desc->type);
  c->driver = d;
  if (d->init(c,desc->options)) {
    veError(MODULE,"veDeviceCtrlCreate: failed to initialize control for %s",
	    desc->type);
    c->driver = NULL;
    veDeviceCtrlDestroy(c);
    return NULL;
  }

  /* add to head of list */
  c->next = ctrl_list_head;
  if (ctrl_list_head)
    ctrl_list_head->prev = c;
  else
    ctrl_list_tail = c;
  ctrl_list_head = c;

  return c;
}

static int names_match(char *n1, char *n2) {
  /* wildcards */
  if (n1 && strcmp(n1,"*") == 0)
    return 1;
  if (n2 && strcmp(n2,"*") == 0)
    return 1;
  /* nulls */
  if (!n1 && !n2)
    return 1;
  if (!n1 || !n2)
    return 0;
  /* regular strings */
  return (strcmp(n1,n2) == 0);
}

VeDeviceCtrl *veDeviceCtrlFind(char *inputname, char *outputname) {
  VeDeviceCtrl *c;
  for(c = ctrl_list_head; c; c = c->next) {
    if (names_match(c->inputname,inputname) &&
	names_match(c->outputname,outputname))
      return c;
  }
  return NULL;
}

void veDeviceCtrlDestroy(VeDeviceCtrl *c) {
  if (c) {
    /* detach from list */
    if (c->prev)
      c->prev->next = c->next;
    else if (c == ctrl_list_head)
      ctrl_list_head = c->next;
    if (c->next)
      c->next->prev = c->prev;
    else if (c == ctrl_list_tail)
      ctrl_list_tail = c->prev;
    c->next = NULL;
    c->prev = NULL;
    /* deinst */
    if (c->driver && c->driver->deinit)
      c->driver->deinit(c);
    veFree(c->inputname);
    veFree(c->outputname);
    veFree(c->type);
    veFree(c);
  }
}

/* 0 = controller handled event, 1 = nobody wants event,
   -1 = error */
int veDeviceCtrlEvent(VeDeviceEvent *e) {
  VeDeviceCtrl *c;
  int k;
  for(c = ctrl_list_head; c; c = c->next) {
    if (c->inputname && strcmp(c->inputname,e->device) == 0 &&
	c->driver && c->driver->event) {
      if ((k = c->driver->event(c,e)) == 0) {
	veDeviceEventDestroy(e);
	return 0; /* processed */
      } else if (k < 0) {
	veError(MODULE,"driver type %s failed to process event",
		c->type);
	return -1;
      }
    }
  }
  /* nobody wanted our event */
  return 1;
}
