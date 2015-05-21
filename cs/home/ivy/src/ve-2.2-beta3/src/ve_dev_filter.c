/* Code for handling filters */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_alloc.h>
#include <ve_debug.h>
#include <ve_driver.h>
#include <ve_tlist.h>
#include <ve_error.h>
#include <ve_util.h>
#include <ve_device.h>

#define MODULE "ve_dev_filter"

VeDeviceFilter *veDeviceFilterCreate(VeDeviceEventProc proc,
				     void *cdata) {
  VeDeviceFilter *f;
  f = veAllocObj(VeDeviceFilter);
  f->proc = proc;
  f->cdata = cdata;
  return f;
}

/* filter table - the following is kept here rather than in
   the ve_device.h header since it is not needed outside of
   this module... */

static VeDeviceFTableEntry *ft_head = NULL, *ft_tail = NULL;

int veDeviceFilterAdd(VeDeviceSpec *spec, VeDeviceFilter *filter, int where) {
  VeDeviceFTableEntry *e;

  if (where != VE_FTABLE_HEAD && where != VE_FTABLE_TAIL) {
    veError(MODULE,"veDeviceFilterAdd: invalid 'where' argument: %d",
	    where);
    return -1;
  }

  e = veAllocObj(VeDeviceFTableEntry);
  e->spec = spec;
  e->filter = filter;
  e->next = NULL;

  if (where == VE_FTABLE_HEAD) {
    e->next = ft_head;
    ft_head = e;
    if (!ft_tail)
      ft_tail = ft_head;
  } else {
    assert(where == VE_FTABLE_TAIL);
    if (ft_tail)
      ft_tail->next = e;
    else
      ft_head = e;
    ft_tail = e;
  }
  return 0;
}

int veDeviceFilterProcess(VeDeviceEvent *e) {
  int status, ret_status = VE_FILT_CONTINUE;
  VeDeviceFTableEntry *f;

  if (!e)
    return ret_status; /* nothing to do */

  f = ft_head;
  while (f) {
    if (veDeviceMatchSpec(e,f->spec)) {
      if (f->filter->proc) {
	/* special case - filters that map to a particular index of a
	   vector */
	if (veDeviceEventType(e) == VE_ELEM_VECTOR &&
	    f->spec && f->spec->index >= 0 && e->index < 0) {
	  VeDeviceEvent *ve;
	  int mapped;

	  ve = veDeviceEventCreate(VE_ELEM_VALUATOR,0);
	  ve->timestamp = e->timestamp;
	  ve->device = e->device ? veDupString(e->device) : NULL;
	  ve->elem = e->elem ? veDupString(e->elem) : NULL;
	  ve->index = f->spec->index;
	  VE_EVENT_VALUATOR(ve)->min = VE_EVENT_VECTOR(e)->min[ve->index];
	  VE_EVENT_VALUATOR(ve)->max = VE_EVENT_VECTOR(e)->max[ve->index];
	  VE_EVENT_VALUATOR(ve)->value = VE_EVENT_VECTOR(e)->value[ve->index];

	  status = f->filter->proc(e,f->filter->cdata);
	  
	  mapped = strcmp(e->device,ve->device) || 
	    strcmp(e->elem, ve->elem) ||
	    ve->content->type != VE_ELEM_VALUATOR;

	  if (!mapped) {
	    VE_EVENT_VECTOR(e)->min[ve->index] = VE_EVENT_VALUATOR(ve)->min;
	    VE_EVENT_VECTOR(e)->max[ve->index] = VE_EVENT_VALUATOR(ve)->max;
	    VE_EVENT_VECTOR(e)->value[ve->index] = 
	      VE_EVENT_VALUATOR(ve)->value;
	  }
	  
	  /* deal with special status cases for "sub"-event */
	  switch (status) {
	  case VE_FILT_ERROR:
	    veError(MODULE,"veDeviceFilterProcess: filter returned error");
	    veDeviceEventDestroy(ve);
	    return -1;

	  case VE_FILT_CONTINUE:
	    if (mapped)
	      /* process this next */
	      veDevicePushEvent(ve,VE_QUEUE_HEAD,VE_FILT_CONTINUE);
	    else
	      veDeviceEventDestroy(ve);
	    break;

	  case VE_FILT_RESTART:
	    veDevicePushEvent(ve,VE_QUEUE_HEAD,VE_FILT_CONTINUE);
	    status = VE_FILT_CONTINUE;
	    break;

	  case VE_FILT_DISCARD:
	    if (mapped)
	      status = VE_FILT_CONTINUE; /* do not discard original */
	    veDeviceEventDestroy(ve);

	  case VE_FILT_DELIVER:
	    if (mapped) {
	      veDevicePushEvent(ve,VE_QUEUE_HEAD,VE_FILT_DELIVER);
	      status = VE_FILT_CONTINUE;
	    } else
	      veDeviceEventDestroy(ve);
	    break;

	  default:
	    veFatalError(MODULE,"veDeviceFilterProcess: invalid filter result: %d", status);
	  }
	} else
	  status = f->filter->proc(e,f->filter->cdata);

	switch(status) {
	case VE_FILT_ERROR:
	case VE_FILT_DISCARD:
	case VE_FILT_DELIVER:
	  return status; /* aborts processing... */
	case VE_FILT_RESTART:
	  f = ft_head;
	  ret_status = VE_FILT_CONTINUE;
	  continue; /* skip advance at end... */
	default:
	  ret_status = status; /* continue as usual */
	}
      }
    }
    f = f->next;
  }
  return ret_status;
}
