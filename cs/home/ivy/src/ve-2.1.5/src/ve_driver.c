/* This is a very UNIX-specific dynamic module loader */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#else
/* other Unix platforms */
#include <dlfcn.h>
#endif /* */

#include <ve_util.h>
#include <ve_error.h>
#include <ve_driver.h>

#define MODULE "ve_driver"

static char *driver_root = NULL;

typedef struct ve_dyn_driver {
  void *handle;
  char *dllname;
  char *path;
} VeDynDriver;

static VeStrMap dyn_modules;

#define newstruct(x) mallocstruct(sizeof (x))
static void *mallocstruct(int n) {
  void *x = calloc(1,n);
  assert(x != NULL);
  return x;
}

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
  static char ret[1024];
  char *c, *p, str[1024];
  strcpy(str,dllname);
  c = basename(str);
  if (strcmp(c,".") == 0 || strcmp(c,"/") == 0)
    return NULL;
  if ((p = strrchr(c,'.')))
    *p = '\0';
  sprintf(ret, "ve_%s_init", c);
  return ret;
}

/* removed references to "/usr" paths - unused, unintuitive and
   un-something-else-bad */
static char *dllsearch[] = {
  ".",
  NULL
};

static char *verootsearch[] = {
  "",
  "lib",
  "lib/drivers",
  NULL
};

#define MAX_USER_SEARCH 32
static char *userdllsearch[MAX_USER_SEARCH];

int veAddDynDriverPath(char *p) {
  int i;
  for(i = 0; i < MAX_USER_SEARCH; i++)
    if (!userdllsearch[i])
      break;
  if (i >= MAX_USER_SEARCH) {
    veError(MODULE, "veAddDynDriverPath: internal path list limit exceeded (max = %d)", MAX_USER_SEARCH);
    return -1;
  }
  userdllsearch[i] = strdup(p);
  return 0;
}

static char *find_dll(char *dllname) {
  static char path[1024], *c;
  int i;

  if (strchr(dllname,'/'))
    return dllname; /* name is specified */
  for(i = 0; i < MAX_USER_SEARCH && userdllsearch[i]; i++) {
    sprintf(path, "%s/%s", dllsearch[i], dllname);
    if (access(path,R_OK) == 0)
      return path;
  }
  if ((c = driver_root) || (c = getenv("VEROOT"))) {
    for(i = 0; verootsearch[i]; i++) {
      sprintf(path, "%s/%s/%s", c, verootsearch[i], dllname);
      if (access(path,R_OK) == 0)
	return path;
    }
  }
  for(i = 0; dllsearch[i]; i++) {
    sprintf(path, "%s/%s", dllsearch[i], dllname);
    if (access(path,R_OK) == 0)
      return path;
  }
  return NULL;
}

void veSetDriverRoot(char *veroot) {
  if (driver_root)
    free(driver_root);
  if (veroot)
    driver_root = veDupString(veroot);
  else
    driver_root = NULL;
}

int veRegDynamicDriver(char *dllname) {
  void *dll;
  void (*init)(void);
  char *iname, *dname;
  VeDynDriver *current_dyn_driver;

  dname = find_dll(dllname);
  if (dname == NULL) {
    veError(MODULE, "cannot find dynamic driver %s", dllname);
    return -1;
  }

  if (dyn_modules && veStrMapLookup(dyn_modules,dname))
    return 0; /* driver is already loaded */

#ifdef __APPLE__
  {
    CFragConnectionID cid;
    Ptr addr;
    Str255 emsg;
    if (GetSharedLibrary(dname,kPowerPCCFragArch,kFindCFrag,&cid,&addr,emsg) != noErr) {
      veError(MODULE, "failed to load driver %s: %s",
	      dllname, emsg);
      return -1;
    }
    dll = cid;
  }
#else
  dll = dlopen(dname,RTLD_LAZY|RTLD_GLOBAL);
  if (dll == NULL) {
    veError(MODULE, "failed to load driver %s: %s",
	    dllname, dlerror());
    return -1;
  }
#endif /* __APPLE__ */
  if (!(iname = getinitname(dllname))) {
    veError(MODULE, "could not determine initialization function for %s",
	    dllname);
    return -1;
  }
#ifdef __APPLE__
  {
    CFragSymbolClass symClass;
    if (FindSymbol((CFragConnectionID)dll,iname,(Ptr *)&init,&symClass) 
	!= noErr) {
      veError(MODULE, "%s: could not find symbol %s",dllname,iname);
      return -1;
    }
    if (symClass != kCodeCFragSymbol) {
      veError(MODULE, "%s: found initialization symbol %s but "
	      "it is not executable", dllname,iname);
      return -1;
    }
  }
#else
  init = (void (*)(void))dlsym(dll, iname);
  if (init == NULL) {
    veError(MODULE, "%s: %s", iname, dlerror());
    return -1;
  }
#endif /* __APPLE__ */

  current_dyn_driver = newstruct(VeDynDriver);

  /* initialization function should register any drivers it supports */
  init();

  current_dyn_driver->dllname = veDupString(dllname);
  current_dyn_driver->path = veDupString(dname);
  current_dyn_driver->handle = dll;
  if (!dyn_modules)
    dyn_modules = veStrMapCreate();
  veStrMapInsert(dyn_modules,dname,current_dyn_driver);

  return 0;
}

void *veFindDynFunc(char *nm) {
  /* look-up a dynamically loaded function - needed to safely
     access driver functions but still link safely */
  VeDynDriver *d;
  VeStrMapWalk w;
  void *v;

  if (!dyn_modules)
    return NULL;

  w = veStrMapWalkCreate(dyn_modules);
  if (veStrMapWalkFirst(w) == 0) {
    do {
      d = (VeDynDriver *)(veStrMapWalkObj(w));
#ifdef __APPLE__
      {
	CFragSymbolClass symClass;
	if (FindSymbol((CFragConnectionID)d->handle,nm,(Ptr *)&v,&symClass) != noErr) {
	  veStrMapWalkDestroy(w);
	  return v;
	}
      }
#else
      if ((v = dlsym(d->handle,nm))) {
	veStrMapWalkDestroy(w);
	return v;
      }
#endif /* __APPLE__ */
    } while (veStrMapWalkNext(w) == 0);
  }
  veStrMapWalkDestroy(w);
  return NULL;
}

