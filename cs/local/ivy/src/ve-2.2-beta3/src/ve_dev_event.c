/* Device events */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_alloc.h>
#include <ve_clock.h>
#include <ve_debug.h>
#include <ve_device.h>
#include <ve_util.h>
#include <ve_thread.h>
#include <ve_core.h>
#include <ve_mp.h>

#define MODULE "ve_dev_event"

VeDeviceEvent *veDeviceEventCreate(VeDeviceEType type, int vsize) {
  VeDeviceEvent *e;
  e = veAllocObj(VeDeviceEvent);
  e->content = veDeviceEContentCreate(type,vsize);
  e->index = -1;
  return e;
}

VeDeviceEvent *veDeviceEventInit(VeDeviceEType type, int vsize, 
				 char *device, char *elem) {
  VeDeviceEvent *e;
  e = veDeviceEventCreate(type,vsize);
  assert(e != NULL);
  if (device)
    e->device = veDupString(device);
  if (elem)
    e->elem = veDupString(elem);
  e->timestamp = veClock();
  return e;
}

VeDeviceEvent *veDeviceEventCopy(VeDeviceEvent *e) {
  VeDeviceEvent *ecopy;
  
  ecopy = veAllocObj(VeDeviceEvent);
  ecopy->timestamp = e->timestamp;
  ecopy->index = e->index;
  ecopy->device = e->device ? veDupString(e->device) : NULL;
  ecopy->elem = e->elem ? veDupString(e->elem) : NULL;
  ecopy->content = veDeviceEContentCopy(e->content);
  return ecopy;
}

VeDeviceEvent *veDeviceEventFromElem(char *device, VeDeviceElement *el) {
  VeDeviceEvent *e;
  if (!el)
    return NULL;
  /* need to do the initialization ourselves */
  e = veAllocObj(VeDeviceEvent);
  e->timestamp = veClock();
  e->index = 0;
  e->device = device ? veDupString(e->device) : NULL;
  e->elem = el->name ? veDupString(el->name) : NULL;
  e->content = veDeviceEContentCopy(el->content);
  return e;
}

void veDeviceEventDestroy(VeDeviceEvent *e) {
  if (e) {
    if (e->content)
      veDeviceEContentDestroy(e->content);
    veFree(e);
  }
}

struct ve_device_cback {
  VeDeviceEventProc proc;
  void *arg;
  VeDeviceSpec *spec;
  struct ve_device_cback *next;
} *cback_list = NULL;

int veDeviceAddCallback(VeDeviceEventProc p, void *arg, char *spec) {
  VeDeviceSpec *s;
  if (veMPTestSlaveGuard())
    return 0; /* fake success */
  if (!(s = veDeviceParseSpec(spec)))
    return -1;
  return veDeviceAddCallbackSpec(p,arg,s);
}

int veDeviceAddCallbackSpec(VeDeviceEventProc p, void *arg, VeDeviceSpec *s) {
  struct ve_device_cback *c;

  c = veAllocObj(struct ve_device_cback);
  c->proc = p;
  c->arg = arg;
  c->spec = s;
  /* stick at head of callback list (overrides callbacks added before it) */
  c->next = cback_list;
  cback_list = c;
  return 0;
}

/* remove all callbacks to given proc */
int veDeviceRemoveCallback(VeDeviceEventProc p) {
  struct ve_device_cback *c, *pre, *tmp;

  for(pre = NULL, c = cback_list; c; pre = c, c = c->next) {
    if (c->proc == p) {
      tmp = c;
      c = c->next;
      veFree(tmp);
      /* fix up list */
      if (pre)
	pre->next = c;
      else
	cback_list = c; /* must have been head */
    }
  }
  return 0;
}

int veDeviceHandleCallback(VeDeviceEvent *e) {
  struct ve_device_cback *c;
  int res = -1;
  int cont = 0;

  /* first step: try controllers */
  if (veDeviceCtrlEvent(e) == 0)
    return 0; /* event consumed by controller */

  for(c = cback_list; c; c = c->next) {
    if (veDeviceMatchSpec(e,c->spec)) {
      if (e->content->type == VE_ELEM_VECTOR && 
	  c->spec && c->spec->index >= 0) {
	/* need to create a valuator for an index of this vector */
	VeDeviceE_Vector *vec;
	VeDeviceE_Valuator *val;
	VeDeviceEvent *ve;

	vec = (VeDeviceE_Vector *)(e->content);
	/* we should not match a spec that references an index outside of
	   our vector */
	assert(c->spec->index < vec->size);

	/* build a new event which is just the valuator portion... */
	ve = veDeviceEventCreate(VE_ELEM_VALUATOR,0);
	val = (VeDeviceE_Valuator *)(ve->content);
	ve->timestamp = e->timestamp;
	ve->device = e->device ? veDupString(e->device) : NULL;
	ve->elem = e->elem ? veDupString(e->elem) : NULL;
	ve->index = c->spec->index;
	val->min = vec->min[c->spec->index];
	val->max = vec->max[c->spec->index];
	val->value = vec->value[c->spec->index];
	veLockCallbacks();
	vePfEvent(MODULE,"callback","%s %s",ve->device,ve->elem);
	res = c->proc(ve,c->arg);
	veUnlockCallbacks();
	veDeviceEventDestroy(ve);
	cont = 1;
      } else {
	veLockCallbacks();
	vePfEvent(MODULE,"callback","%s %s",e->device,e->elem);
	res = c->proc(e,c->arg);
	veUnlockCallbacks();
	cont = 0;
      }
      if (!cont || res)
	return res;
    }
  }
  return res;
}

static struct ve_device_equeue {
  VeDeviceEvent *event;
  int disp;  /* filter code: 
		VE_FILT_CONTINUE:  normal
		VE_FILT_DELIVER:   skip filter processing
	     */
  struct ve_device_equeue *next, *prev;
} *equeue_head = NULL, *equeue_tail = NULL;

static VeThrMutex *equeue_mutex = NULL;
static VeThrMutex *einsert_mutex = NULL;
/* inserted event disposition */
static int event_disp = VE_DEVICE_NOBLOCK;

extern void veDeviceLatencyInit();

int veDeviceInit() {
  equeue_mutex = veThrMutexCreate();
  einsert_mutex = veThrMutexCreate();
  veDeviceLatencyInit();
  return 0;
}

int veDevicePushEvent(VeDeviceEvent *e, int where, int disp) {
  struct ve_device_equeue *eq;

  if (veMPTestSlaveGuard())
    return 0;

  if (disp == 0)
    disp = VE_FILT_CONTINUE;

  assert(equeue_mutex != NULL);

  if (where != VE_QUEUE_HEAD && where != VE_QUEUE_TAIL) {
    veError(MODULE,"veDevicePushEvent: invalid insertion point: %d\n", where);
    return -1;
  }

  veThrMutexLock(equeue_mutex);

  eq = veAllocObj(struct ve_device_equeue);
  eq->event = e;
  eq->disp = disp;
  if (!equeue_head) {
    equeue_head = equeue_tail = veAllocObj(struct ve_device_equeue);
    equeue_head->event = e;
  } else if (where == VE_QUEUE_HEAD) {
    eq->next = equeue_head;
    equeue_head->prev = eq;
    equeue_head = eq;
  } else {
    equeue_tail->next = eq;
    eq->prev = equeue_tail;
    equeue_tail = eq;
  }

  veThrMutexUnlock(equeue_mutex);

  return 0;
}

/* 1 if something is there */
int veDeviceEventPending() {
  return (equeue_head ? 1 : 0);
}

VeDeviceEvent *veDeviceNextEvent(int where, int *disp) {
  VeDeviceEvent *e;
  struct ve_device_equeue *eq;

  assert(equeue_mutex != NULL);

  if (where != VE_QUEUE_HEAD && where != VE_QUEUE_TAIL) {
    veError(MODULE,"veDeviceNextEvent: invalid removal point: %d\n", where);
    return NULL;
  }

  veThrMutexLock(equeue_mutex);
  if (!equeue_head)
    e = NULL;
  else {
    if (where == VE_QUEUE_HEAD) {
      eq = equeue_head;
      equeue_head = equeue_head->next;
      if (equeue_head)
	equeue_head->prev = NULL;
      else
	equeue_tail = NULL;
    } else {
      eq = equeue_tail;
      equeue_tail = equeue_tail->prev;
      if (equeue_tail)
	equeue_tail->next = NULL;
      else
	equeue_head = NULL;
    }
    if (disp)
      *disp = eq->disp;
    e = eq->event;
    veFree(eq);
  }
  veThrMutexUnlock(equeue_mutex);
  return e;
}

static int _doProcessEvent(VeDeviceEvent *e) {
  VeDeviceFTableEntry *te = NULL;
  int status;

  if (!e) {
    veError(MODULE,"_doProcessEvent: cannot process null event");
    return -1; /* cannot process null event */
  }

  vePfEvent(MODULE,"process-event","%s %s",e->device,e->elem);

  switch((status = veDeviceFilterProcess(e))) {
    case VE_FILT_ERROR:
      veError(MODULE,"_doProcessEvent: filter returned error");
      return -1;
      break;
	
    case VE_FILT_CONTINUE:
      break;

    case VE_FILT_RESTART:
      /* restart filter processing from the top of the list */
      veFatalError(MODULE,"veDeviceFilterProcess returned RESTART");
      break;

    case VE_FILT_DISCARD:
      return 0;

    case VE_FILT_DELIVER:
      break;

    default:
      veFatalError(MODULE,"_doProcessEvent: rogue filter returned bad status code: %d", status);
  }

  /* if we fall through then deliver it */
  veDeviceApplyEvent(e);
  veDeviceHandleCallback(e);
  return 0;
}

void veDeviceProcessQueueNolock(void) {
  VeDeviceEvent *e;
  int disp;
  
  while (e = veDeviceNextEvent(VE_QUEUE_HEAD,&disp)) {
    switch (disp) {
    case VE_FILT_CONTINUE:
      (void) _doProcessEvent(e);
      break;

    case VE_FILT_DELIVER:
      veDeviceApplyEvent(e);
      veDeviceHandleCallback(e);
      break;

    default:
      veError(MODULE,"veDeviceProcessQueue: unexpected disposition: %d",
	      disp);
}
    veDeviceEventDestroy(e);
  }
}

int veDeviceProcessEvent(VeDeviceEvent *e) {
  int res;
  if (veMPTestSlaveGuard())
    return 0;
  veThrMutexLock(einsert_mutex);
  res = _doProcessEvent(e);
  veThrMutexUnlock(einsert_mutex);
  return res;
}

void veDeviceProcessQueue(void) {
  VeDeviceEvent *e;
  int disp;
  if (veMPTestSlaveGuard())
    return;
  
  veThrMutexLock(einsert_mutex);
  veDeviceProcessQueueNolock();
  veThrMutexUnlock(einsert_mutex);
}

/* The following is like ProcessEvent() except that it is for device
   drivers to call, so that the decision whether or not to queue the
   event goes here or not */
/* In the interest of latency, queued events are processed in the order in
   which they are queued, but not necessarily before events that are 
   generated by devices after the event has been queued.
*/
/* We also process queued events immediately after finishing with an
   inserted event (if we are in "noblock" mode). */
int veDeviceInsertEvent(VeDeviceEvent *e) {
int res;
  if (veMPTestSlaveGuard())
    return 0;
  veThrMutexLock(einsert_mutex);
  VE_DEBUGM(2,("InsertEvent: (%s,%s)",e->device?e->device:"<NULL>",
	      e->elem?e->elem:"<NULL>"));
  vePfEvent(MODULE,"insert-event","%s %s",e->device,e->elem);
  switch (event_disp) {
  case VE_DEVICE_NOBLOCK:
    res = _doProcessEvent(e); /* process normally */
    veDeviceEventDestroy(e);
    /* process any back-logged events */
    if (veDeviceEventPending())
      veDeviceProcessQueueNolock();
    break;
  case VE_DEVICE_DISCARD: /* just destroy it */
    veDeviceEventDestroy(e);
    break;
  case VE_DEVICE_QUEUE: /* queue for later processing */
    veDevicePushEvent(e,VE_QUEUE_TAIL,0);
    break;
  default:
    veError(MODULE,"event_disp has illegal value: %d", event_disp);
    abort();
  }
  veThrMutexUnlock(einsert_mutex);
  return res;
}

void veDeviceApplyEvent(VeDeviceEvent *e) {
  VeDevice *d;
  if (veMPTestSlaveGuard())
    return;
  if (e && e->device && (d = veDeviceFind(e->device)) && d->model)
    veDeviceApplyEventToModel(d->model,e);
}

void veDeviceBlockEvents(int disp) {
  if (disp != VE_DEVICE_QUEUE && disp != VE_DEVICE_DISCARD)
    veFatalError(MODULE,"invalid disposition for events in call to veDeviceBlockEvents: %d",disp);
  VE_DEBUGM(2,("blocking events (%s)",disp == VE_DEVICE_QUEUE ? "queue" : "discard"));
  veThrMutexLock(einsert_mutex);
  event_disp = disp;
  veThrMutexUnlock(einsert_mutex);
}

void veDeviceUnblockEvents(void) {
  VE_DEBUGM(2,("unblocking events"));
  veThrMutexLock(einsert_mutex);
  event_disp = VE_DEVICE_NOBLOCK;
  /* push through back-logged events */
  if (veDeviceEventPending())
    veDeviceProcessQueueNolock();  /* nolock? */
  veThrMutexUnlock(einsert_mutex);
}

float veDeviceToValuator(VeDeviceEvent *e) {
  switch (VE_EVENT_TYPE(e)) {
  case VE_ELEM_VALUATOR:
    return VE_EVENT_VALUATOR(e)->value;
  case VE_ELEM_VECTOR:
    {
      VeDeviceE_Vector *v = VE_EVENT_VECTOR(e);
      if (v->size > 0)
	return v->value[0];
      else
	return 0.0;
    }
    break;
  case VE_ELEM_SWITCH:
    return VE_EVENT_SWITCH(e)->state ? 1.0 : 0.0;
  case VE_ELEM_KEYBOARD:
    return VE_EVENT_KEYBOARD(e)->state ? 1.0 : 0.0;
  case VE_ELEM_TRIGGER:
    return 1.0;
  default: 
    /* this space deliberately left blank */;
  }
  /* there shouldn't be any other cases... */
  return 0.0;
}

int veDeviceToSwitch(VeDeviceEvent *e) {
  switch (VE_EVENT_TYPE(e)) {
  case VE_ELEM_SWITCH:
    return VE_EVENT_SWITCH(e)->state;
  case VE_ELEM_KEYBOARD:
    return VE_EVENT_KEYBOARD(e)->state;
  case VE_ELEM_TRIGGER:
    return 1;
  case VE_ELEM_VALUATOR:
    {
      float t;
      VeDeviceE_Valuator *v = VE_EVENT_VALUATOR(e);
      if (v->min == 0.0 && v->max == 0.0)
	t = 0.5;
      else
	t = (v->min+v->max)/2.0;
      return v->value >= t ? 1 : 0;
    }
    break;
  case VE_ELEM_VECTOR:
    {
      float t;
      VeDeviceE_Vector *v = VE_EVENT_VECTOR(e);
      if (v->size < 1)
	return 0;
      if (v->min[0] == 0.0 && v->max[0] == 0.0)
	t = 0.5;
      else
	t = (v->min[0]+v->max[0])/2.0;
      return v->value[0] >= t ? 1 : 0;
    }
    break;
  default:
    /* this space deliberately left blank */;
  }
  /* in theory - this should never happen */
  veWarning(MODULE,"dropped through in veDeviceToSwitch");
  return 0;
}
