#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_device.h>
#include <ve_util.h>
#include <ve_tlist.h>

#define MODULE "ve_dev_model"

static void *fzeromalloc(int sz) {
  void *v = calloc(1,sz);
  assert(v != NULL);
  return v;
}
#define newstruct(x) (fzeromalloc(sizeof(x)))

VeDeviceEContent *veDeviceEContentCopy(VeDeviceEContent *content) {
  VeDeviceEContent *c_out;
  if (content->type != VE_ELEM_VECTOR) {
    int dsize = 0;
    c_out = veDeviceEContentCreate(content->type,0);
    /* note that it is safe not to consider VE_ELEM_VECTOR in this
       switch statement */
    switch(content->type) {
    case VE_ELEM_TRIGGER:  dsize = sizeof(VeDeviceE_Trigger); break;
    case VE_ELEM_SWITCH:   dsize = sizeof(VeDeviceE_Switch); break;
    case VE_ELEM_VALUATOR: dsize = sizeof(VeDeviceE_Valuator); break;
    case VE_ELEM_KEYBOARD: dsize = sizeof(VeDeviceE_Keyboard); break;
    default:
      /* this space deliberately left blank */;
    }
    if (dsize > 0)
      memcpy(c_out,content,dsize);
  } else {
    VeDeviceE_Vector *ev, *ecv;
    ev = (VeDeviceE_Vector *)(content);
    c_out = veDeviceEContentCreate(content->type,ev->size);
    ecv = (VeDeviceE_Vector *)(c_out);
    memcpy(ecv->min,ev->min,sizeof(float)*ev->size);
    memcpy(ecv->max,ev->max,sizeof(float)*ev->size);
    memcpy(ecv->value,ev->value,sizeof(float)*ev->size);
  }
  return c_out;
}

VeDeviceEContent *veDeviceEContentCreate(VeDeviceEType type, int vsize) {
  VeDeviceEContent *c = NULL;
  switch(type) {
  case VE_ELEM_TRIGGER:
    c = newstruct(VeDeviceE_Trigger);
    break;
  case VE_ELEM_SWITCH:
    c = newstruct(VeDeviceE_Switch);
    break;
  case VE_ELEM_VALUATOR:
    c = newstruct(VeDeviceE_Valuator);
    break;
  case VE_ELEM_VECTOR:
    c = newstruct(VeDeviceE_Vector);
    {
      VeDeviceE_Vector *vec = (VeDeviceE_Vector *)c;
      vec->size = vsize;
      if (vsize > 0) {
	vec->min = calloc(vsize,sizeof(float));
	assert(vec->min != NULL);
	vec->max = calloc(vsize,sizeof(float));
	assert(vec->max != NULL);
	vec->value = calloc(vsize,sizeof(float));
	assert(vec->value != NULL);
      }
    }
    break;
  case VE_ELEM_KEYBOARD:
    c = newstruct(VeDeviceE_Keyboard);
    break;
  default:
    /* this space intentionally left blank */;
  }
  if (c)
    c->type = type;
  return c;
}

void veDeviceEContentDestroy(VeDeviceEContent *c) {
  VeDeviceE_Vector *vec;
  if (c) {
    switch(c->type) {
    case VE_ELEM_VECTOR: /* only one with extra storage */
      vec = (VeDeviceE_Vector *)c;
      if (vec->max)
	free(vec->max);
      if (vec->min)
	free(vec->min);
      if (vec->value)
	free(vec->value);
      break;
    default:
      /* this space intentionally left blank */;
    }
    free(c);
  }
}

VeDeviceElement *veDeviceElementCreate(char *name, VeDeviceEType type,
				       int vsize) {
  VeDeviceElement *e;
  
  e = newstruct(VeDeviceElement);
  if (name)
    e->name = veDupString(name);
  else
    e->name = NULL;
  e->content = veDeviceEContentCreate(type,vsize);
  return e;
}

void veDeviceElementDestroy(VeDeviceElement *e) {
  if (e) {
    veDeviceEContentDestroy(e->content);
    if (e->name)
      free(e->name);
    free(e);
  }
}

VeDeviceModel *veDeviceCreateModel() {
  VeDeviceModel *m;
  m = newstruct(VeDeviceModel);
  m->elems = veStrMapCreate();
  return m;
}

int veDeviceAddElem(VeDeviceModel *model, VeDeviceElement *elem) {
  VeDeviceElement *old;
  if (!model || !elem || !elem->name)
    return 0;
  if ((old = veStrMapLookup(model->elems,elem->name)))
    veDeviceElementDestroy(old);
  veStrMapInsert(model->elems,elem->name,elem);
  return 0;
}

VeDeviceElement *veDeviceFindElem(VeDeviceModel *model, char *name) {
  if (!model || !name)
    return NULL;
  return veStrMapLookup(model->elems,name);
}

#define DELIM " \r\t\n"
static VeDeviceEContent *parse_trigger(char *ln) {
  return veDeviceEContentCreate(VE_ELEM_TRIGGER,0);
}

static VeDeviceEContent *parse_keyboard(char *ln) {
  return veDeviceEContentCreate(VE_ELEM_KEYBOARD,0);
}

static VeDeviceEContent *parse_switch(char *ln) {
  VeDeviceE_Switch *c;
  char *s;
  c = (VeDeviceE_Switch *)veDeviceEContentCreate(VE_ELEM_SWITCH,0);
  c->state = -1; /* default if nothing specified */
  if ((s = strtok(ln,DELIM)))
    c->state = atoi(s);
  return (VeDeviceEContent *)c;
}

static VeDeviceEContent *parse_valuator(char *ln) {
  VeDeviceE_Valuator *c;
  char *s;
  c = (VeDeviceE_Valuator *)veDeviceEContentCreate(VE_ELEM_VALUATOR,0);
  c->min = c->max = c->value = 0.0; /* defaults */
  if ((s = strtok(ln,DELIM))) {
    c->min = atof(s);
    if ((s = strtok(NULL,DELIM))) {
      c->max = atof(s);
      if ((s = strtok(NULL,DELIM))) {
	c->value = atof(s);
      }
    }
  }
  return (VeDeviceEContent *)c;
}

static VeDeviceEContent *parse_vector(char *ln) {
  VeDeviceE_Vector *c;
  char *s, *l;
  int k, sz;

  sz = 0;
  if ((s = strtok(ln,DELIM))) {
    sz = atoi(s);
    ln = strtok(NULL,"");
  }
  c = (VeDeviceE_Vector *)veDeviceEContentCreate(VE_ELEM_VECTOR,sz);
  for(k = 0; k < c->size; k++) {
    if (!(l = veNextLElem(&ln)))
      break;
    if ((s = strtok(l,DELIM))) {
      c->min[k] = atof(s);
      if ((s = strtok(NULL,DELIM))) {
	c->max[k] = atof(s);
	if ((s = strtok(NULL,DELIM)))
	  c->value[k] = atof(s);
      }
    }
  }
  return (VeDeviceEContent *)c;
}

VeDeviceElement *veDeviceParseElem(char *spec) {
  VeDeviceElement *e;
  VeDeviceEContent *content;
  char *s, *dup, *name, *type, *ptr;
  
  dup = veDupString(spec); /* save spec - may not be alterable... */
  ptr = dup;
  name = veNextLElem(&ptr);
  if (name && strcmp(name,"elem") == 0)
    name = veNextLElem(&ptr); /* skip leading "elem" if there */
  if (!name) {
    veError(MODULE,"veDeviceParseElem: missing name in spec: %s", spec);
    free(dup);
    return NULL;
  }
  if (!(type = veNextLElem(&ptr))) {
    veError(MODULE,"veDeviceParseElem: missing type in spec: %s", spec);
    free(dup);
    return NULL; /* type */
  }

  /* parse content based upon name */
  content = NULL;
  s = ptr ? ptr : "";
  if (strcmp(type,"trigger") == 0)
    content = parse_trigger(s);
  else if (strcmp(type,"switch") == 0)
    content = parse_switch(s);
  else if (strcmp(type,"valuator") == 0)
    content = parse_valuator(s);
  else if (strcmp(type,"vector") == 0)
    content = parse_vector(s);
  else if (strcmp(type,"keyboard") == 0)
    content = parse_keyboard(s);
  else
    veError(MODULE,"veDeviceParseElem: unknown elem type %s in spec: %s",
	    type, spec);

  if (!content) {
    free(dup);
    return NULL; /* parsing of content failed - don't continue */
  }

  e = newstruct(VeDeviceElement);
  e->name = veDupString(name);
  e->content = content;
  return e;
}

int veDeviceAddElemSpec(VeDeviceModel *model, char *spec) {
  VeDeviceElement *e;
  
  if (!(e = veDeviceParseElem(spec)))
    return -1;
  return veDeviceAddElem(model,e);
}

void veDeviceApplyEventToModel(VeDeviceModel *model, VeDeviceEvent *e) {
  VeDeviceElement *el;
  VeDeviceE_Vector *e_v, *el_v;
  int i;

  if (e && e->elem && (el = veDeviceFindElem(model,e->elem))) {
    if (el->content->type == e->content->type) {
      switch (e->content->type) {
      case VE_ELEM_SWITCH:
	VE_EVENT_SWITCH(el)->state = VE_EVENT_SWITCH(e)->state;
	break;
      case VE_ELEM_VALUATOR:
	VE_EVENT_VALUATOR(el)->value = VE_EVENT_VALUATOR(el)->value;
	break;
      case VE_ELEM_VECTOR:
	e_v = VE_EVENT_VECTOR(e);
	el_v = VE_EVENT_VECTOR(el);
	for(i = 0; i < e_v->size && i < el_v->size; i++)
	  el_v->value[i] = e_v->value[i];
	break;
      default:
	/* this space intentionally left blank */;
      }
    }
  }
}
