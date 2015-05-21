/* Device specs - expressions for specifying some set of device/elem pairs */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_device.h>
#include <ve_util.h>

#define MODULE "ve_dev_spec"

static char *wildcard = "*"; /* define here so we can identify it... */

static void *fzeromalloc(int sz) {
  void *v = calloc(1,sz);
  assert(v != NULL);
  return v;
}
#define newstruct(x) (fzeromalloc(sizeof(x)))

VeDeviceSpec *veDeviceSpecCreate() {
  VeDeviceSpec *s;
  s = newstruct(VeDeviceSpec);
  s->device = s->elem = wildcard;
  s->index = -1;
  return s;
}

void veDeviceSpecDestroy(VeDeviceSpec *s) {
  if (s) {
    if (s->device && s->device != wildcard)
      free(s->device);
    if (s->elem && s->elem != wildcard)
      free(s->elem);
    free(s);
  }
}

VeDeviceSpec *veDeviceParseSpec(char *str) {
  VeDeviceSpec *s;
  char *dup, *c, *str2;
  
  str2 = dup = veDupString(str); /* work with a copy */

  s = newstruct(VeDeviceSpec);
  s->device = wildcard;
  s->elem = wildcard;
  s->index = -1;
  
  c = strtok(str2,".");
  if (c) {
    /* c = device part */
    s->device = veDupString(c);
    if ((c = strtok(NULL,"."))) {
      /* c = elem part */
      s->elem = veDupString(c);
      if ((c = strtok(NULL,"."))) {
	/* c = index part */
	if (sscanf(c,"%d",&(s->index)) != 1) {
	  veError(MODULE, "device spec has invalid index part: %s", str);
	  veDeviceSpecDestroy(s);
	  free(dup);
	  return NULL;
	}
      }
    }
  }
  free(dup);
  return s;
}

static char *etype_to_str(VeDeviceEType type) {
  switch (type) {
  case VE_ELEM_TRIGGER:   return "trigger";
  case VE_ELEM_SWITCH:    return "switch";
  case VE_ELEM_VALUATOR:  return "valuator";
  case VE_ELEM_VECTOR:    return "vector";
  case VE_ELEM_KEYBOARD:  return "keyboard";
  default: return "undef";
  }
}

/* return 1 if they match */
int veDeviceMatchSpec(VeDeviceEvent *e, VeDeviceSpec *s) {
  /* match against info in dest */
  if (!e || !s)
    return 0; /* fail if either one does not exist */
  if (s->device && strcmp(s->device,"*")) {
    if (!e->device || strcmp(s->device,e->device))
      return 0; /* device spec does not match name or type */
  }

  if (s->elem && strcmp(s->elem,"*")) {
    /* a specific element is requested */
    if (!e->elem || (strcmp(s->elem,e->elem) && 
		     strcmp(s->elem,etype_to_str(e->content->type))))
      return 0;
  }

  /* how do I determine type? */
  if (s->index >= 0 && e->content->type == VE_ELEM_VECTOR) {
    VeDeviceE_Vector *v = (VeDeviceE_Vector *)(e->content);
    if (!v || s->index >= v->size)
      return 0;
  }

  /* all specified aspects matched */
  return 1;
}
