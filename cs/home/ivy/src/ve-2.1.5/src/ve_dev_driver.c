#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_driver.h>
#include <ve_device.h>
#include <ve_util.h>

static VeStrMap device_drivers = NULL;

int veDeviceAddDriver(VeDeviceDriver *d) {
  if (!d || !d->type)
    return -1; /* bad device driver spec */

  if (!device_drivers)
    device_drivers = veStrMapCreate();
  if (!device_drivers)
    return -1; /* failed to create string map */
  veStrMapInsert(device_drivers,d->type,d);
  return 0;
}

VeDeviceDriver *veDeviceFindDriver(char *type) {
  if (!type)
    return NULL;
  
  if (!device_drivers || !(veStrMapLookup(device_drivers,type))) {
    /* see if a reference exists in the manifest */
    char *path;
    if ((path = veDeviceFindDriverRef("device",type)))
      veRegDynamicDriver(path);
  }
    
  if (!device_drivers)
    return NULL;
  return (VeDeviceDriver *)veStrMapLookup(device_drivers,type);
}
