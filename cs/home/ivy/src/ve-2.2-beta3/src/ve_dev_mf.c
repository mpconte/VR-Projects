/* Device Manifest support */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_alloc.h>
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
  VeDeviceDesc *d = veAllocObj(VeDeviceDesc);
  d->options = veStrMapCreate();
  return d;
}

void veDeviceDescDestroy(VeDeviceDesc *d) {
  if (d) {
    veFree(d->name);
    veFree(d->type);
    if (d->options)
      veStrMapDestroy(d->options,free);
    veFree(d);
  }
}

int veDeviceAddDriverRef(char *type, char *name, char *driverpath) {
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

  veFree(veStrMapLookup(sm,name));
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
