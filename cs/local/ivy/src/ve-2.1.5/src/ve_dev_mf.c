/* Device Manifest support */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_debug.h>
#include <ve_device.h>
#include <ve_util.h>
#include <ve_tlist.h> /* for parsing lists... */

#define MODULE "ve_dev_mf"

static VeStrMap devmanifest = NULL;
static VeStrMap devdrivers = NULL;
static VeStrMap filtdrivers = NULL;

char *veDeviceDescOption(VeDeviceDesc *desc, char *name) {
  if (desc && desc->options && name)
    return (char *)veStrMapLookup(desc->options,name);
  return NULL;
}

VeDeviceDesc *veDeviceDescCreate() {
  VeDeviceDesc *d;
  d = calloc(1,sizeof(VeDeviceDesc));
  assert(d != NULL);
  d->options = veStrMapCreate();
  return d;
}

void veDeviceDescDestroy(VeDeviceDesc *d) {
  if (d) {
    if (d->name)
      free(d->name);
    if (d->type)
      free(d->type);
    if (d->options)
      veStrMapDestroy(d->options,free);
    free(d);
  }
}

int veDeviceAddDriverRef(char *type, char *name, char *driverpath) {
  char *s;
  VeStrMap sm;
  if (!devdrivers)
    devdrivers = veStrMapCreate();
  if (!filtdrivers)
    filtdrivers = veStrMapCreate();

  if (strcmp(type,"filter") == 0)
    sm = filtdrivers;
  else if (strcmp(type,"device") == 0)
    sm = devdrivers;
  else {
    veError(MODULE, "veDeviceAddDriverRef: unknown driver type: %s", type);
    return -1;
  }

  if ((s = veStrMapLookup(sm,name)))
    free(s);
  if (driverpath)
    veStrMapInsert(sm,name,veDupString(driverpath));
  else
    veStrMapDelete(sm,name);
  return 0;
}

char *veDeviceFindDriverRef(char *type, char *name) {
  VeStrMap sm;

  if (strcmp(type,"filter") == 0)
    sm = filtdrivers;
  else if (strcmp(type,"device") == 0)
    sm = devdrivers;
  else {
    veError(MODULE, "veDeviceAddDriverRef: unknown driver type: %s", type);
    return NULL;
  }

  if (!sm)
    return NULL;
  return (char *)(veStrMapLookup(sm,name));
}

VeDeviceDesc *veFindDeviceDesc(char *name) {
  if (!devmanifest)
    return NULL;
  return (VeDeviceDesc *)(veStrMapLookup(devmanifest,name));
}

int veAddDeviceDesc(VeDeviceDesc *desc) {
  VeDeviceDesc *d;

  if (!desc || !desc->name)
    return 0;

  if (!devmanifest)
    devmanifest = veStrMapCreate();
  if ((d = veStrMapLookup(devmanifest,desc->name)))
    veDeviceDescDestroy(d);
  veStrMapInsert(devmanifest,desc->name,(void *)desc);
  return 0;
}

VeStrMap veGetDeviceManifest(void) {
  return devmanifest;
}

int veClearDeviceManifest() {
  /* clears the current device manifest */
  if (devmanifest)
    veStrMapDestroy(devmanifest,(VeStrMapFreeProc)veDeviceDescDestroy);
  devmanifest = veStrMapCreate();
  return 0;
} 

int veReadDeviceManifest(FILE *stream, char *mfname) {
  static char *_func = "veReadDeviceManifest";
  char *l, *s, *ftype, *fname, *fpath;
  VeDeviceDesc *d;
  int lineno = 0;

  VE_DEBUG(2,("%s: entered", _func));

  if (!mfname)
    mfname = _func;

  while ((l = veNextFScrElem(stream,&lineno))) {
    s = veNextLElem(&l);
    assert(s != NULL); /* should never be returned an empty string */

    /* determine type */
    if (strcmp(s,"driver") == 0) {
      if (!(ftype = veNextLElem(&l))) {
	veWarning(MODULE, "%s: line %d, missing type for driver ref", mfname, lineno);
	continue;
      }
      if (!(fname = veNextLElem(&l))) {
	veWarning(MODULE, "%s: line %d, missing name for driver ref",
		  mfname, lineno);
	continue;
      }
      if (!(fpath = veNextLElem(&l))) {
	veWarning(MODULE, "%s: line %d, missing path for driver ref",
		  mfname, lineno);
	continue;
      }
      VE_DEBUG(2,("%s: driver ref '%s' '%s' '%s'", mfname, ftype, fname, fpath));
      veDeviceAddDriverRef(ftype,fname,fpath);
    } else if (strcmp(s,"device") == 0) {
      d = calloc(1,sizeof(VeDeviceDesc));
      assert(d != NULL);
      if (!(s = veNextLElem(&l))) {
	veWarning(MODULE, "%s: line %d, missing name for device description",
		  mfname, lineno);
	veDeviceDescDestroy(d);
      }
      d->name = veDupString(s);
      if (!(s = veNextLElem(&l))) {
	/* error - missing type */
	veWarning(MODULE, "%s: line %d, missing type for device description", 
		  mfname, lineno);
	veDeviceDescDestroy(d);
	continue;
      }
      d->type = veDupString(s);
      if ((s = veNextLElem(&l))) {
	char *ln, *x, *del;
	d->options = veStrMapCreate();
	while ((ln = veNextScrElem(&s,NULL))) {
	  x = veNextLElem(&ln);
	  assert(x != NULL);
	  if ((del = veStrMapLookup(d->options,x)))
	    free(del);
	  veStrMapInsert(d->options,x,veDupString(ln ? ln : ""));
	}
      }
      VE_DEBUG(2,("%s: device %s %s", mfname, d->name, d->type));
      veAddDeviceDesc(d);
    } else if (strcmp(s,"use") == 0) {
      VeStrMap override = NULL;
      char *name, *devtype, *opts;
      if (!(name = veNextLElem(&l)))
	veFatalError(MODULE,"%s: line %d, missing device name for 'use'",
		     mfname, lineno);
      opts = NULL;
      devtype = veNextLElem(&l);
      if (devtype) {
	if (!(opts = veNextLElem(&l))) {
	  opts = devtype;
	  devtype = NULL;
	}
      }
      if (devtype) {
	/* define and use a device all in one shot */
	char *ln, *x, *del;
	d = calloc(1,sizeof(VeDeviceDesc));
	assert(d != NULL);
	d->name = veDupString(name);
	d->type = veDupString(devtype);
	d->options = veStrMapCreate();
	while ((ln = veNextScrElem(&opts,NULL))) {
	  x = veNextLElem(&ln);
	  assert(x != NULL);
	  if ((del = veStrMapLookup(d->options,x)))
	    free(del);
	  veStrMapInsert(d->options,x,veDupString(ln ? ln : ""));
	}
	veAddDeviceDesc(d);

	/* now use it */
	if (!veDeviceUse(name,NULL)) {
	  if (!veStrMapLookup(d->options,"optional"))
	    veFatalError(MODULE,"%s: line %d, could not use device %s",
			 mfname, lineno, name);
	}
      } else {
	if (opts) {
	  char *ln, *n, *v;
	  override = veStrMapCreate();
	  while ((ln = veNextScrElem(&opts,NULL))) {
	    n = veNextLElem(&ln);
	    v = veNextLElem(&ln);
	    if (n)
	      veStrMapInsert(override,n,v);
	  }
	}
	if (!veDeviceUse(name,override))
	  if (!override || !veStrMapLookup(override,"optional"))
	    veFatalError(MODULE,"%s: line %d, could not use device %s", 
			 mfname, lineno, name);
	if (override)
	  veStrMapDestroy(override,NULL);
      }
    } else if (strcmp(s,"filter") == 0) {
      if (veDeviceFTableAddDesc(l,VE_FTABLE_TAIL))
	veFatalError(MODULE,"%s: line %d, invalid filter desc: %s",
		     mfname, lineno, l);
    } else {
      veWarning(MODULE, "%s: line %d, unknown manifest type: %s",
		mfname, lineno, s);
      continue;
    }
  }
  return 0;
}
