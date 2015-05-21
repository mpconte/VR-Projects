/* An implementation of the driver loading interfaces that uses the
   Darwin/MacOSX interfaces rather than the POSIX ones. */

/* Q: Need to determine the difference between "libraries" and
   "modules" in Darwin - there is apparently a difference. */
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ve_alloc.h>
#include <ve_debug.h>
#include <ve_driver.h>
#include <ve_util.h>
#include <mach-o/dyld.h>

#define MODULE "ve_drv_darwin"

#define ARB 1024

/* under Mac OS X we cannot currently unload modules, so we
   need to keep track of what handles we have open... */
static char *basename(char *s) {
  char *c;
  if (!s || !*s)
    return ".";
  c = &s[strlen(s)-1];
  if (*c == '/')
    *c-- = '\0';
  if (!(c = strrchr(s,'/')))
    return (*s) ? s : "/";
  else
    return c+1;
}

static char *getinitname(char *dllname) {
  static char ret[ARB];
  char *c, *p, str[ARB];
  strncpy(str,dllname,ARB);
  str[ARB-1] = '\0';
  c = basename(str);
  if (strcmp(c,".") == 0 || strcmp(c,"/") == 0)
    return NULL;
  if ((p = strrchr(c,'.')))
    *p = '\0';
  veSnprintf(ret, ARB, "_ve_%s_init", c);
  return ret;
}

static char *getprobename(char *dllname) {
  static char ret[ARB];
  char *c, *p, str[ARB];
  strncpy(str,dllname,ARB);
  str[ARB-1] = '\0';
  c = basename(str);
  if (strcmp(c,".") == 0 || strcmp(c,"/") == 0)
    return NULL;
  if ((p = strrchr(c,'.')))
    *p = '\0';
  veSnprintf(ret, ARB, "_ve_%s_probe", c);
  return ret;
}

static char *converr(NSObjectFileImageReturnCode rc) {
  switch (rc) {
  case NSObjectFileImageSuccess: return "success";
  case NSObjectFileImageFailure: return "failure";
  case NSObjectFileImageInappropriateFile: return "inappropriate file";
  case NSObjectFileImageArch: return "no arch support";
  case NSObjectFileImageFormat: return "invalid format";
  case NSObjectFileImageAccess: return "no access";
  default: return "<unknown>";
  }
}

VeDriverObj *veDrvImplProbe(char *path) {
  NSObjectFileImage fileImage;
  NSModule handle;
  NSSymbol sym;
  NSObjectFileImageReturnCode returnCode;
  char *pname;
  void (*probe)(void *);
  VeDriverObj *olist = NULL;

  if (!(pname = getprobename(path))) {
    veNotice(MODULE,"could not find probename for %s",path);
    return NULL; /* could not determine probe name */
  }

  returnCode = NSCreateObjectFileImageFromFile(path, &fileImage);
  if (returnCode != NSObjectFileImageSuccess) {
    veNotice(MODULE,"could not open shared object %s: %s",path,
	     converr(returnCode));
    return NULL;
  }
  handle = NSLinkModule(fileImage,path,
			NSLINKMODULE_OPTION_RETURN_ON_ERROR|
			NSLINKMODULE_OPTION_PRIVATE);
  NSDestroyObjectFileImage(fileImage);
  if (!handle) {
    veNotice(MODULE,"could not link shared object %s",path);
    return NULL;
  }
  if (!(sym = NSLookupSymbolInModule(handle, pname)) ||
      !(probe = (void *)NSAddressOfSymbol(sym))) {
    veNotice(MODULE,"could not find probe symbol %s in module %s",
	     pname, path);
    NSUnLinkModule(handle,0);
    return NULL;
  }
  probe((void *)&olist);
  NSUnLinkModule(handle,0);
  return olist;
}

int veDrvImplLoad(VeDriverInfo *di) {
  NSObjectFileImage fileImage;
  NSModule handle;
  NSSymbol sym;
  NSObjectFileImageReturnCode returnCode;
  void (*init)(void);
  char *iname;
  
  if (di->handle)
    return 0; /* already loaded */
  
  if (!(iname = getinitname(di->path))) {
    veError(MODULE, "could not determine initialization function for %s",
	    di->path);
    return -1;
  }

  returnCode = NSCreateObjectFileImageFromFile(di->path, &fileImage);
  if (returnCode != NSObjectFileImageSuccess) {
    veError(MODULE,"could not open shared object %s: %s",di->path,
	    converr(returnCode));
    return -1;
  }
  handle = NSLinkModule(fileImage,di->path,
			NSLINKMODULE_OPTION_RETURN_ON_ERROR);
  NSDestroyObjectFileImage(fileImage);
  if (!handle) {
    veError(MODULE,"could not link shared object %s",di->path);
    return -1;
  }
  if (!(sym = NSLookupSymbolInModule(handle, iname)) ||
      !(init = (void *)NSAddressOfSymbol(sym))) {
    veError(MODULE,"could not find init symbol %s in module %s",
	     iname, di->path);
    NSUnLinkModule(handle,0);
    return -1;
  }

  di->handle = (void *)handle;
  init();
  return 0;
}

int veDrvImplIsDrv(char *path) {
  int n;
  n = strlen(path);
  if ((n < 7) ||
      strcmp(".bundle",(path+n-7)) != 0)
    return 0; /* not a driver */
  return 1; /* a driver */
}

void *veDrvImplFind(VeDriverInfo *di, char *sym) {
  void *v = NULL;
  char *sym2;
  if (di && di->handle && sym) {
    NSSymbol nssym;
    sym2 = veAlloc(strlen(sym)+2,0);
    sprintf(sym2,"_%s",sym);
    if ((nssym = NSLookupSymbolInModule((NSModule)(di->handle),sym2)))
      v = (void *)NSAddressOfSymbol(nssym);
    veFree(sym2);
  }
  return v;
}

/* fake it - Mac OS X does not unload drivers right now */
void veDrvImplClose(VeDriverInfo *di) {
  if (di && di->handle) {
    NSUnLinkModule(di->handle,0);
    di->handle = NULL;
  }
}

