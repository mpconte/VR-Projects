#include "autocfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <ve_debug.h>
#include <ve_driver.h>
#include <ve_util.h>
#include <dlfcn.h>

#define MODULE "ve_drv_posix"

#define ARB 1024

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
  veSnprintf(ret, ARB, "ve_%s_init", c);
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
  veSnprintf(ret, ARB, "ve_%s_probe", c);
  return ret;
}

/* unix version... */
VeDriverObj *veDrvImplProbe(char *path) {
  VeDriverObj *olist = NULL;
  void *dll;
  void (*probe)(void *);
  char *pname;

  if (!(pname = getprobename(path))) {
    veNotice(MODULE,"could not find probename for %s",path);
    return NULL; /* could not determine probe name */
  }

  /* try to load driver */
  if (!(dll = dlopen(path,RTLD_LAZY))) {
    veNotice(MODULE,"could not open shared object %s: %s",
	     path, dlerror());
    return NULL;
  }
  /* if successful call probe f'n with pointer to our list */
  if (!(probe = (void (*)(void *))dlsym(dll,pname))) {
    veNotice(MODULE,"probe function %s not available for driver %s",
	     pname, path);
    dlclose(dll);
    return NULL;
  }
  probe((void *)&olist);
  /* unload driver */
  dlclose(dll);
  return olist;
}

int veDrvImplLoad(VeDriverInfo *di) {
  void (*init)(void);
  char *iname;

  if (di->handle)
    /* already loaded */
    return 0;

  if (!(di->handle = dlopen(di->path,RTLD_LAZY|RTLD_GLOBAL))) {
    veError(MODULE, "failed to load driver %s: %s",
	    di->path, dlerror());
    return -1;
  }

  if (!(iname = getinitname(di->path))) {
    veError(MODULE, "could not determine initialization function for %s",
	    di->path);
    return -1;
  }

  init = (void (*)(void))dlsym(di->handle, iname);
  if (init == NULL) {
    veError(MODULE, "%s: %s", iname, dlerror());
    return -1;
  }

  /* initialization function should register any drivers it supports */
  init();

  return 0;
}

int veDrvImplIsDrv(char *path) {
  int n;
  n = strlen(path);
  if ((n < 3) ||
      strcmp(".so",(path+n-3)) != 0)
    return 0; /* not a driver */
  return 1; /* a driver */
}

void *veDrvImplFind(VeDriverInfo *di, char *sym) {
  void *v;
  if (di && di->handle && sym && (v = dlsym(di->handle,sym)))
    return v;
  return NULL;
}

void veDrvImplClose(VeDriverInfo *di) {
  if (di && di->handle) {
    dlclose(di->handle);
    di->handle = NULL;
  }
}
