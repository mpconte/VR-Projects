/* This is a very UNIX-specific dynamic module loader */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#include <ve_debug.h>
#include <ve_alloc.h>
#include <ve_util.h>
#include <ve_error.h>
#include <ve_driver.h>

#define MODULE "ve_driver"

#define ARB 1024
#define WHTSPC " \r\t\n"

static char *driver_root = NULL;

/* Two-level map.  First level is by object "type" 
   and points to strmap,
   Second level is by object "name" and points to
   VeDriverObj.  Same VeDriverObj pointers as in
   the "drivers" map.
*/
static VeStrMap provides;

/* Map of drivers by driver path. (i.e. real files on disk) */
static VeStrMap drivers;

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

void veDriverObjFree(VeDriverObj *o) {
  while (o) {
    VeDriverObj *on = o->next;
    veFree(o->name);
    veFree(o->type);
    veFree(o);
    o = on;
  }
}

void veDriverProvide(void *phdl, char *type, char *name) {
  VeDriverObj **olist = (VeDriverObj **)phdl;
  VeDriverObj *o;

  if (!olist || !type || !name)
    return; /* bogus probe callback */
  
  o = veAllocObj(VeDriverObj);
  o->type = veDupString(type);
  o->name = veDupString(name);
  o->next = *olist;
  *olist = o;
}


static VeDriverInfo *make_driver_info(char *dir, char *name) {
  char path[ARB];
  VeDriverInfo *di;
  struct stat st;

  veSnprintf(path,ARB,"%s/%s",dir,name);

  if (stat(path,&st))
    return NULL; /* cannot access */

  di = veAllocObj(VeDriverInfo);
  di->next = NULL;
  di->name = veDupString(name);
  di->path = veDupString(path);
  di->handle = NULL;

  di->obj = veDrvImplProbe(path);

  return di;
}

static void free_driver_info_list(VeDriverInfo *di) {
  while (di) {
    VeDriverInfo *n = di->next;
    if (di->handle)
      veDrvImplClose(di);
    veFree(di->name);
    veFree(di->path);
    veDriverObjFree(di->obj);
    veFree(di);
    di = n;
  }
}

static void remove_cache_info(VeDriverInfo *di) {
  VeDriverObj *o;

  if (di) {
    VeStrMap sm;
    VeDriverInfo *i;
    if (provides) {
      for(o = di->obj; o; o = o->next) {
	if ((sm = (VeStrMap)veStrMapLookup(provides,o->type)) &&
	    ((i = (VeDriverInfo *)veStrMapLookup(sm,o->name)) == di)) {
	  veStrMapDelete(sm,o->name);
	}
      }
    }
    if (drivers && 
	((i = (VeDriverInfo *)veStrMapLookup(drivers,di->path)) == di)) {
      veStrMapDelete(drivers,di->path);
    }
  }
}

static int merge_driver_info(VeDriverInfo *dlist) {
  /* merge information in dlist with information in tables... */
  /* as we do this, we deliberately wipe out the "next" links
     so we do not stumble over them later (e.g. if a single
     driver is removed) */
  VeDriverInfo *di, *i, *dn;
  VeDriverObj *o;
  int ret = 0;

  di = dlist;

  while (di) {
    dn = di->next;
    di->next = NULL;

    /* driver map */
    if (drivers && (i = (VeDriverInfo *)veStrMapLookup(drivers,di->path))) {
      /* is the driver in use? */
      if (i->handle) {
	veWarning(MODULE,"not overriding cache info for loaded driver %s",
		  di->path);
	free_driver_info_list(di);
	di = dn;
	ret = -1; /* let upper level know we did not completely merge */
	continue;
      }
      /* override it */
      remove_cache_info(i);
      free_driver_info_list(i);
    }
    
    if (!drivers)
      drivers = veStrMapCreate();
    veStrMapInsert(drivers,di->path,di);

    /* provides */
    if (!provides)
      provides = veStrMapCreate();
    for(o = di->obj; o; o = o->next) {
      VeStrMap sm;
      if (!(sm = (VeStrMap)veStrMapLookup(provides,o->type))) {
	sm = veStrMapCreate();
	veStrMapInsert(provides,o->type,sm);
      }
      /* we do not care if an entry already exists for this particular
	 driver type - even if it does, that driver object will still
	 be referenced in the "drivers" table - if we are "recaching"
	 the same driver path, then we wiped out its information
	 above... */
      veStrMapInsert(sm,o->name,di);
    }

    di = dn;
  }
  return ret;
}

static int probe_driver_path(char *dpath) {
  VeDriverInfo *dlist = NULL, *di, *dp;
  VeDriverObj *olist;
  VeDir dir;
  char *de; /* directory entry */
  struct stat st;
  int cache_ok = 1; /* assume cache is correct */
  char path[ARB];

  if (!(dir = veOpenDir(dpath)))
    return -1; /* cannot read path... */

  /* read directory and modify cache information as necessary 
     (e.g. adding objects if they are not present in the cache)
   */
  while ((de = veReadDir(dir,0))) {
    if (veDrvImplIsDrv(de)) {
      if ((di = make_driver_info(dpath,de))) {
	di->next = dlist;
	dlist = di;
      }
    }
  }
  veCloseDir(dir);

  merge_driver_info(dlist);

  return 0;
}

static void dump_driver_info(void) {
  VeStrMapWalk w;
  if (!drivers)
    return;
  w = veStrMapWalkCreate(drivers);
  if (veStrMapWalkFirst(w) == 0) {
    do {
      VeDriverInfo *di;
      VeDriverObj *o;
      di = (VeDriverInfo *)(veStrMapWalkObj(w));
      fprintf(stderr, "Driver %s %s\n",
	      di->name ? di->name : "<null>",
	      di->path ? di->path : "<null>");
      for (o = di->obj; o; o = o->next)
	fprintf(stderr, "\tprovides %s %s\n",
		o->type ? o->type : "<null>",
		o->name ? o->name : "<null>");
    } while (veStrMapWalkNext(w) == 0);
  }
  veStrMapWalkDestroy(w);
}

/* removed references to "/usr" paths - unused, unintuitive and
   un-something-else */
static char *dllsearch[] = {
  ".",
  NULL
};

static char *verootsearch[] = {
  "",
  "lib/ve",
  "lib/ve/drivers",
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
  userdllsearch[i] = veDupString(p);
  return 0;
}

char *veFindDriver(char *name) {
  static char path[ARB], *c;
  int i;

  if (strchr(name,'/'))
    return name; /* name is specified */
  for(i = 0; i < MAX_USER_SEARCH && userdllsearch[i]; i++) {
    sprintf(path, "%s/%s", dllsearch[i], name);
    if (access(path,R_OK) == 0)
      return path;
  }
  if ((c = driver_root)) {
    for(i = 0; verootsearch[i]; i++) {
      sprintf(path, "%s/%s/%s", c, verootsearch[i], name);
      if (access(path,R_OK) == 0)
	return path;
    }
  }
  for(i = 0; dllsearch[i]; i++) {
    sprintf(path, "%s/%s", dllsearch[i], name);
    if (access(path,R_OK) == 0)
      return path;
  }
  return NULL;
}

void veSetDriverRoot(char *veroot) {
  veFree(driver_root);
  if (veroot)
    driver_root = veDupString(veroot);
  else
    driver_root = NULL;
}

int veLoadDriver(char *path) {
  VeDriverInfo *di;

  /* do we have cache information for this driver? */
  if (!drivers || !(di = veStrMapLookup(drivers,path))) {
    /* try to probe driver */
    if (!(di = make_driver_info((path[0] == '/' ? "" : "."),path))) {
      veError(MODULE,"cannot probe driver %s",path);
      return -1;
    }
    merge_driver_info(di);
  }

  assert(di != NULL);

  return veDrvImplLoad(di);
}

void *veFindDynFunc(char *nm) {
  /* look-up a dynamically loaded function - needed to safely
     access driver functions but still link safely */
  VeDriverInfo *di;
  VeStrMapWalk w;
  void *v;

  if (!drivers)
    return NULL;

  w = veStrMapWalkCreate(drivers);
  if (veStrMapWalkFirst(w) == 0) {
    do {
      di = (VeDriverInfo *)(veStrMapWalkObj(w));
      if ((v = veDrvImplFind(di,nm))) {
	veStrMapWalkDestroy(w);
	return v;
      }
    } while (veStrMapWalkNext(w) == 0);
  }
  veStrMapWalkDestroy(w);
  return NULL;
}

int veDriverRequire(char *type, char *name) {
  VeStrMap sm;
  VeDriverInfo *di;
  
  if (!type || !name) {
    veWarning(MODULE,"veDriverRequire: null type or name");
    return -1;
  }

  if (!provides) {
    veNotice(MODULE,"veDriverRequire: no module information loaded");
    return -1;
  }

  if (!(sm = (VeStrMap)veStrMapLookup(provides,type))) {
    veNotice(MODULE,"veDriverRequire: cannot find table for type %s",type);
    return -1;
  }
   
  /* deal with '*' wildcard...? */
  if (!(di = (VeDriverInfo *)veStrMapLookup(sm,name))) {
    if (strcmp(name,"*") == 0) {
      VeStrMapWalk w = veStrMapWalkCreate(sm);
      if (veStrMapWalkFirst(w) == 0) {
	do {
	  di = (VeDriverInfo *)veStrMapWalkObj(w);
	  if (veDrvImplLoad(di) == 0) {
	    veStrMapWalkDestroy(w);
	    return 0;
	  }
	} while (veStrMapWalkNext(w) == 0);
      }
      veStrMapWalkDestroy(w);
      veNotice(MODULE,"veDriverRequire: no drivers for type %s (name = '*')",
	       type);
      return -1;
    }

    veNotice(MODULE,"veDriverRequire: cannot find driver for %s/%s",type,name);
    return -1;
  }
  
  return veDrvImplLoad(di);
}

/* probe for drivers in VE root */
void veProbeRootDrivers(void) {
  char rpath[ARB], *c;
  int i;

  if ((c = driver_root)) {
    for(i = 0; verootsearch[i]; i++) {
      sprintf(rpath, "%s/%s", c, verootsearch[i]);
      probe_driver_path(rpath);
    }
  }
  if (VE_ISDEBUG2(MODULE,0)) {
    dump_driver_info();
  }
}
