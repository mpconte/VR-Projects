/* Code for handling filters */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_debug.h>
#include <ve_driver.h>
#include <ve_tlist.h>
#include <ve_error.h>
#include <ve_util.h>
#include <ve_device.h>

#define MODULE "ve_dev_filter"

static VeStrMap filter_drivers = NULL;

int veDeviceAddFilter(VeDeviceFilter *filter) {
  if (!filter_drivers)
    filter_drivers = veStrMapCreate();
  
  if (!filter || !filter->name)
    return -1;

  VE_DEBUG(1,("%s: veDeviceAddFilter: adding %s",MODULE,filter->name?filter->name:"<null>"));
  /* just overwrite a previous filter entry as it may have been allocated
     statically */
  veStrMapInsert(filter_drivers,filter->name,filter);
  return 0;
}

VeDeviceFilter *veDeviceFindFilter(char *name) {
  if (!name) {
    VE_DEBUG(1,("%s: veDeviceFindFilter: null name",MODULE));
    return NULL;
  }
  VE_DEBUG(1,("%s: veDeviceFindFilter(%s)",MODULE,name));

  if (!filter_drivers || !(veStrMapLookup(filter_drivers,name))) {
    /* see if a reference exists in the manifest */
    char *path;
    VE_DEBUG(1,("%s: veDeviceFindFilter: no filter found - looking for driver",
		MODULE));
    if ((path = veDeviceFindDriverRef("filter",name)))
      veRegDynamicDriver(path); /* load the driver and try again */
  }
  
  if (!filter_drivers) {
    VE_DEBUG(1,("%s: veDeviceFindFilter: no filter found at all"));
    return NULL;
  }
  VE_DEBUG(1,("%s: veDeviceFindFilter: filter %s may exist in map",MODULE,name));
  return (VeDeviceFilter *)veStrMapLookup(filter_drivers,name);
}

VeDeviceFilterInst *veDeviceCreateFilterInst() {
  VeDeviceFilterInst *i;
  i = calloc(1,sizeof(VeDeviceFilterInst));
  assert(i != NULL);
  return i;
}

#define MAXFILTARGS 16
VeDeviceFilterInst *veDeviceParseFilterDesc(char *fdesc) {
  VeDeviceFilterInst *i;
  VeDeviceFilter *f;
  VeStrMap optargs;
  char *args[MAXFILTARGS+1];
  int nargs;
  char *s, *dup, *ptr, *name, *sep;

  /* work with a copy */
  dup = veDupString(fdesc);
  ptr = dup;

  name = veNextLElem(&ptr);
  if (!name) {
    veError(MODULE,"veDeviceParseFilterDesc: missing name in desc: %s", fdesc);
    free(dup);
    return NULL; /* blank line */
  }

  VE_DEBUG(2,("veDeviceParseFilterDesc: parsing filter %s", name));

  /* special cases */
  if (strcmp(name,"discard") == 0) {
    i = calloc(1,sizeof(VeDeviceFilterInst));
    assert(i != NULL);
    i->special = VE_FILT_DISCARD;
    free(dup);
    return i;
  } else if (strcmp(name,"deliver") == 0) {
    i = calloc(1,sizeof(VeDeviceFilterInst));
    assert(i != NULL);
    i->special = VE_FILT_DELIVER;
    free(dup);
    return i;
  } else if (strcmp(name,"restart") == 0) {
    i = calloc(1,sizeof(VeDeviceFilterInst));
    assert(i != NULL);
    i->special = VE_FILT_RESTART;
    free(dup);
    return i;
  } else if (strcmp(name,"exit") == 0) {
    i = calloc(1,sizeof(VeDeviceFilterInst));
    assert(i != NULL);
    i->special = VE_FILT_EXIT;
    free(dup);
    return i;
  }

  if (!(f = veDeviceFindFilter(name))) {
    veError(MODULE,"veDeviceParseFilterDesc: no such filter: %s", name);
    free(dup);
    return NULL;
  }

  optargs = veStrMapCreate();
  for(nargs = 0, s = veNextLElem(&ptr); s; s = veNextLElem(&ptr)) {
    if ((sep = strchr(s,'='))) {
      *sep = '\0';
      VE_DEBUG(2,("veDeviceParseFilterDesc: adding option %s = '%s'",s?s:"<null>",(sep+1)?(sep+1):"<null>"));
      veStrMapInsert(optargs,s,sep+1);
    } else {
      if (nargs >= MAXFILTARGS) {
	veError(MODULE,"veDeviceParseFilterDesc: too many fixed arguments in desc (max %d): %s", MAXFILTARGS, fdesc);
	free(dup);
	veStrMapDestroy(optargs,NULL);
	return NULL;
      }
      args[nargs++] = s;
    }
  }
  args[nargs] = NULL;

  if (f->inst)
    i = f->inst(f,args,nargs,optargs);
  else {
    if (nargs > 0)
      veWarning(MODULE,"veDeviceParseFilterDesc: filter takes no arguments: %s", fdesc);
    i = veDeviceCreateFilterInst();
    i->filter = f;
  }
  free(dup);
  veStrMapDestroy(optargs,NULL);
  return i;
}

void veDeviceDestroyFilterInst(VeDeviceFilterInst *inst) {
  if (inst) {
    if (inst->filter && inst->filter->deinst)
      inst->filter->deinst(inst);
    else
      free(inst);
  }
}

VeDeviceFTableEntry *veDeviceCreateFTableEntry() {
  VeDeviceFTableEntry *e;
  e = calloc(1,sizeof(VeDeviceFTableEntry));
  assert(e != NULL);
  return e;
}

VeDeviceFTableEntry *veDeviceParseFTableEntry(char *desc) {
  VeDeviceFTableEntry *e;
  VeDeviceFilterInst *h, *tail = NULL;
  char *dup, *ptr, *s, *flist;

  e = veDeviceCreateFTableEntry();
  dup = veDupString(desc);
  ptr = dup;
  
  if ((s = veNextLElem(&ptr)) && (strcmp(s,"filter") == 0))
    s = veNextLElem(&ptr); /* skip "filter" if it is the first element */
  
  if (!(e->spec = veDeviceParseSpec(s))) {
    veDeviceDestroyFTableEntry(e);
    free(dup);
    return NULL;
  }

  if ((flist = veNextLElem(&ptr))) {
    if ((s = veNextLElem(&ptr))) {
      veError(MODULE, "veDeviceParseFTableEntry: extra junk in filter description: %s", s);
      veDeviceDestroyFTableEntry(e);
      free(dup);
      return NULL;
    }

    ptr = flist;
    while ((s = veNextScrElem(&ptr,NULL))) {
      if (!(h = veDeviceParseFilterDesc(s))) {
	veDeviceDestroyFTableEntry(e);
	free(dup);
	return NULL;
      }
      h->next = NULL;
      if (!tail) {
	e->head = tail = h;
      } else {
	tail->next = h;
	tail = h;
      }
    }
  }
  return e;
}

void veDeviceDestroyFTableEntry(VeDeviceFTableEntry *e) {
  VeDeviceFilterInst *tmp;
  if (e) {
    if (e->spec)
      veDeviceSpecDestroy(e->spec);
    while(e->head) {
      tmp = e->head->next;
      veDeviceDestroyFilterInst(e->head);
      e->head = tmp;
    }
    free(e);
  }
}

VeDeviceFTable *veDeviceCreateFTable() {
  VeDeviceFTable *f;
  f = calloc(1,sizeof(VeDeviceFTable));
  assert(f != NULL);
  return f;
}

void veDeviceDestroyFTable(VeDeviceFTable *f) {
  VeDeviceFTableEntry *tmp;
  if (f) {
    while(f->head) {
      tmp = f->head->next;
      veDeviceDestroyFTableEntry(f->head);
      f->head = tmp;
    }
    free(f);
  }
}

static VeDeviceFTable *filter_table = NULL;

int veDeviceFTableAdd(VeDeviceFTableEntry *e, int where) {
  if (!e)
    return -1;

  if (!filter_table)
    filter_table = veDeviceCreateFTable();
  
  if (!filter_table->head) {
    e->next = NULL;
    filter_table->head = filter_table->tail = e;
  } else if (where == VE_FTABLE_TAIL) {
    filter_table->tail->next = e;
    filter_table->tail = e;
    e->next = NULL;
  } else if (where == VE_FTABLE_HEAD) {
    e->next = filter_table->head;
    filter_table->head = e;
  } else {
    veError(MODULE,"veDeviceFTableAdd: invalid insertion point: %d\n", where);
    return -1;
  }
  return 0;
}

int veDeviceFTableAddDesc(char *desc, int where) {
  VeDeviceFTableEntry *e;
  if (!(e = veDeviceParseFTableEntry(desc)))
    return -1;
  return veDeviceFTableAdd(e,where);
}

VeDeviceFTableEntry *veDeviceFTableFind(VeDeviceEvent *e, 
					VeDeviceFTableEntry *te) {
  if (!filter_table ||  !e)
    return NULL;

  if (te)
    te = te->next;
  else
    te = filter_table->head;
  while (te) {
    if (veDeviceMatchSpec(e,te->spec))
      return te;
    te = te->next;
  }
  return NULL;
}

/* for debugging */
void veDeviceDumpFTable(void) {
  /* filter_table */
  VeDeviceFTableEntry *e;
  fprintf(stderr, "filter table start\n");
  if (filter_table) {
    for(e = filter_table->head; e; e = e->next) {
      VeDeviceFilterInst *i;
      fprintf(stderr, "%s.%s", 
	      (e->spec && e->spec->device) ? e->spec->device : "*",
	      (e->spec && e->spec->elem) ? e->spec->elem : "*");
      if (e->spec->index >= 0)
	fprintf(stderr, ".%d\n",e->spec->index);
      else
	fprintf(stderr,"\n");
      for(i = e->head; i; i = i->next) {
	if (!i->filter) {
	  switch (i->special) {
	  case VE_FILT_ERROR:  fprintf(stderr, "\tERROR\n"); break;
	  case VE_FILT_CONTINUE:  fprintf(stderr, "\tCONTINUE\n"); break;
	  case VE_FILT_RESTART:  fprintf(stderr, "\tRESTART\n"); break;
	  case VE_FILT_DISCARD:  fprintf(stderr, "\tDISCARD\n"); break;
	  case VE_FILT_DELIVER:  fprintf(stderr, "\tDELIVER\n"); break;
	  default:
	    fprintf(stderr, "\t<unknown special>\n"); break;
	  }
	} else {
	  fprintf(stderr,"\t%s\n",i->filter->name ? i->filter->name : 
		  "<unknown filter>");
	}
      }
    }
  }
  fprintf(stderr, "filter table end\n");
}

int veDeviceRunFilterChain(VeDeviceFilterInst *chain, VeDeviceEvent *e) {
  int status, ret_status = VE_FILT_CONTINUE;
  while(chain) {
    if (!chain->filter)
      return chain->special;
    else if (chain->filter->proc) {
      status = chain->filter->proc(e,chain->arg);
      switch(status) {
      case VE_FILT_ERROR:
      case VE_FILT_DISCARD:
      case VE_FILT_DELIVER:
      case VE_FILT_RESTART:
	return status; /* abortive statii (?) */
      default:
	ret_status = status;
      }
    }
    chain = chain->next;
  }
  return ret_status;
}
