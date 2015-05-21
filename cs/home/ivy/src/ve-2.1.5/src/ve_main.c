/* The glue bits of VE - tying all the modules together */
/* Some programs may omit this when incorporating bits of VE under
   another app model */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <ve_clock.h>
#include <ve_driver.h>
#include <ve_render.h>
#include <ve_debug.h>
#include <ve_main.h>
#include <ve_env.h>
#include <ve_device.h>
#include <ve_util.h>
#include <ve_tlist.h>
#include <ve_thread.h>
#include <ve_stats.h>
#include <ve_mp.h>
#include <ve_core.h>
#include <ve_timer.h>
#include <ve_profiler.h>

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#include <libgen.h>
#include <sys/stat.h>
#endif /* __APPLE__ */

#define MODULE "ve_main"

static VeThrMutex *display_mutex = NULL;
static VeThrCond *display_post = NULL;
static VeStrMap runtime_options = NULL;
static int redisplay_posted = 0;
static VeIdleProc idle_proc = NULL;

/* default names to lookup */
#define DEFAULT_ENV "default.env"
#define DEFAULT_PROFILE "default.pro"
#define DEFAULT_MANIFEST "manifest"
#define DEFAULT_DEVICES  "devices"

/* search through known locations for the given file */
#define PATHSZ 2048
FILE *veFindFile(char *type, char *fname) {
  FILE *f;
  char path[2048];
  char *root;
  if (veIsPathAbsolute(fname))
    return fopen(fname,"r"); /* don't try anything fancy */
  else {
    /* try relative to cwd */
    if ((f = fopen(fname,"r")))
      return f;
    if ((root = getenv("VEROOT"))) {
      /* try paths relative to VEROOT/type and VEROOT */
      if (type) {
	veSnprintf(path,PATHSZ,"%s/%s/%s",root,type,fname);
	if ((f = fopen(path,"r")))
	  return f;
      }
      veSnprintf(path,PATHSZ,"%s/%s",root,fname);
      if ((f = fopen(path,"r")))
	return f;
    }
  }
  /* exhausted possible options... */
  return NULL;
}

void veSetIdleProc(VeIdleProc proc) {
  idle_proc = proc;
}

void veSetOption(char *opt, char *val) {
  char *s;

  assert(runtime_options != NULL);

  if ((s = veStrMapLookup(runtime_options,opt)))
    free(s);
  veStrMapInsert(runtime_options,opt,veDupString(val));
}

char *veGetOption(char *opt) {
  assert(runtime_options != NULL);
  return veStrMapLookup(runtime_options,opt);
}

static void extract_envvar(void) {
  char *s;

  if ((s = getenv("VEROOT")))
    veSetOption("root",s);
  if ((s = getenv("VEENV")))
    veSetOption("env",s);
  if ((s = getenv("VEUSER")) || (s = getenv("VEPROFILE")))
    veSetOption("user",s);
  if ((s = getenv("VEDEBUG")))
    veSetOption("debug",s);
  if ((s = getenv("VEMANIFEST")))
    veSetOption("manifest",s);
  if ((s = getenv("VEDEVICES")))
    veSetOption("devices",s);
}

static void extract_options(int *argc, char **argv) {
  /* parse command-line into runtime options */
  int i,j;
  char *opt, *val;
  char *valid[] = { 
    "env", "user", "debug", "manifest", "devices", "root", "profile", 
    "notices", NULL 
  };

  for(i = 1; i < *argc; i++) {
    if (strncmp(argv[i],"-ve_",4) == 0) {
      for(j = 0; valid[j]; j++) {
	if (strcmp(argv[i]+4,valid[j]) == 0) {
	  i++;
	  if (i >= *argc)
	    veFatalError(MODULE, "missing argument to -ve_%s option",valid[j]);
	  veSetOption(valid[j],argv[i]);
	  break;
	}
      }
      if (valid[j])
	continue;

      /* special cases... */
      if (strcmp(argv[i],"-ve_opt") == 0) {
	i++;
	if (i >= *argc)
	  veFatalError(MODULE, "missing argument to -ve_opt option");
	opt = argv[i];
	i++;
	if (i >= *argc)
	  veFatalError(MODULE, "missing second argument to -ve_opt option");
	val = argv[i];
	veSetOption(opt,val);
      } else {
	veFatalError(MODULE, "unrecognized VE option: %s", argv[i]);
      }
    } else
      break;
  }

  if (i > 1) {
    /* copy args over ones we read */
    for(j = 1; i < *argc; j++, i++)
      argv[j] = argv[i];
    *argc = j;
  }
}

static void process_options(void) {
  char *s;
  FILE *f;

  if ((s = veGetOption("debug")))
    veSetDebugLevel(atoi(s));

  if ((s = veGetOption("notices")))
    veShowNotices(atoi(s));

  if ((s = veGetOption("root"))) {
    char *senv;
    /* update VEROOT entry in environment */
    senv = malloc(strlen(s)+strlen("VEROOT=")+1);
    assert(senv != NULL);
    sprintf(senv,"VEROOT=%s",s);
    putenv(senv);
    /* explicitly set this as a root for the driver loading piece */
    veSetDriverRoot(s);
  }

  if ((s = veGetOption("env"))) {
    if (!(f = veFindFile("env",s)))
      veFatalError(MODULE, "cannot open env file %s: %s", s, strerror(errno));
    veSetEnv(veEnvRead(f,1));
    fclose(f);
    if (!getenv("VEENV")) {
      char *senv;
      senv = malloc(strlen(s)+strlen("VEENV=")+1);
      assert(senv != NULL);
      sprintf(senv,"VEENV=%s",s);
      putenv(senv);
    }
  } else {
    /* try to open default file if it exists */
    if ((f = veFindFile("env",DEFAULT_ENV))) {
      veSetEnv(veEnvRead(f,0));
      fclose(f);
    }
  }

  if ((s = veGetOption("user")) || (s = veGetOption("profile"))) {
    if (!(f = veFindFile("profile",s)))
      veFatalError(MODULE, "cannot open user profile %s: %s", s,
		   strerror(errno));
    veSetProfile(veProfileRead(f,1));
    fclose(f);
    if (!getenv("VEUSER")) {
      char *senv;
      senv = malloc(strlen(s)+strlen("VEUSER=")+1);
      assert(senv != NULL);
      sprintf(senv,"VEUSER=%s",s);
      putenv(senv);
    }
  } else {
    /* try to open default file if it exists */
    if ((f = veFindFile("profile",DEFAULT_PROFILE))) {
      veSetProfile(veProfileRead(f,0));
      fclose(f);
    }
  }

  /* Only try to load device information on the master node */
  if (veMPIsMaster()) {
    if ((s = veGetOption("manifest"))) {
      if (!(f = veFindFile("manifest",s)))
	veFatalError(MODULE, "cannot open manifest %s: %s", s, strerror(errno));
      if (veReadDeviceManifest(f,s))
	veFatalError(MODULE, "failed to read device manifest %s", s);
      fclose(f);
      if (!getenv("VEMANIFEST")) {
	char *senv;
	senv = malloc(strlen(s)+strlen("VEMANIFEST=")+1);
	assert(senv != NULL);
	sprintf(senv,"VEMANIFEST=%s",s);
	putenv(senv);
      }
    } else {
      /* try to open default file if it exists */
      if ((f = veFindFile("manifest",DEFAULT_MANIFEST))) {
	(void) veReadDeviceManifest(f,DEFAULT_MANIFEST);
	fclose(f);
      }
    }

    if ((s = veGetOption("devices"))) {
      if (!(f = veFindFile("devices",s)))
	veFatalError(MODULE, "cannot open devices %s: %s", s, strerror(errno));
      veReadDeviceManifest(f,s);
      fclose(f);
    } else {
      /* try to open default file if it exists */
      if ((f = veFindFile("devices",DEFAULT_DEVICES))) {
	veReadDeviceManifest(f,DEFAULT_DEVICES);
	fclose(f);
      }
    }
  }
}

#ifdef __APPLE__
/* This bit of code will make the necessary symlinks and re-exec the program
   too in order to convince the MacOSX that this is a real application,
   even if we aren't in a directory structure corresponding to a package. */
static void macosx_pkg_hack(int argc, char **argv) {
  struct stat buf;
  char *dir, *base, *p, lnk[256];
  if (strstr(argv[0],"Contents/MacOS")) {
    return;
  }
  /* try to re-exec argv[0] as a package-like path */
  dir = dirname(veDupString(argv[0]));
  base = basename(veDupString(argv[0]));
  p = malloc(strlen(argv[0])+256);
  assert(p != NULL);
  sprintf(p,"Contents");
  if (lstat(p,&buf) == 0) {
    if (!S_ISLNK(buf.st_mode)) {
      return; /* don't try to replace something that already
		 exists */
    }
    memset(lnk,0,256);
    if (readlink(p,lnk,255) == -1 || strcmp(lnk,".")) {
      return; /* wrong link - don't try to re-exec */
    }
  } else
    if (symlink(".","Contents")) {
      return; /* failed to create link */
    }
  sprintf(p,"MacOS");
  if (lstat(p,&buf) == 0) {
    if (!S_ISLNK(buf.st_mode))
      return;
    memset(lnk,0,256);
    if (readlink(p,lnk,255) == -1 || strcmp(lnk,"."))
      return; /* wrong link - don't try to re-exec */
  } else
    if (symlink(".","MacOS"))
      return; /* failed to create link */
  /* If we get this far, then either the links already exist, or
     we made them */
  /* Now re-exec with the "package-like" path */
  sprintf(p,"%s/Contents/MacOS/%s",dir,base);
  argv[0] = p;
  execv(argv[0],argv);
  veWarning(MODULE,"macos_pkg_hack:  failed to re-exec - continuing anyways");
}
#endif /* __APPLE__ */

void veInit(int *argc, char **argv) {
#ifdef __APPLE__
  macosx_pkg_hack(*argc,argv);
#endif /* __APPLE__ */

  /* read in options from environment and command-line */
  /* initialize other pieces */
  veMPSlaveInit(argc,argv); /* rip out the slave option if there is one */

  veThrInitDelayGate();  /* initialize delay gate for spawned threads */
  vePfInit();       /* initialize profiler */
  vePfEvent(MODULE,"ve-init",NULL);
  veStatInit();     /* initialize stats gathering */
  veCoreInit();     /* initialize math and frames of reference */
  veDeviceInit();   /* initialize device queue */
  veClockInit();    /* initialize system clock */
  veTimerInit();    /* initialize system timer queue */
  veRenderInit();   /* initialize the rendering system */
  veHeadTrackInit(); /* initialize the built-in head tracking filter */

  veDeviceBuiltinFilters();  /* register built-in filters */

  runtime_options = veStrMapCreate();
  extract_envvar(); /* extract environment variables into options */
  extract_options(argc,argv);
  process_options();

  display_mutex = veThrMutexCreate();
  display_post = veThrCondCreate();
}

void vePostRedisplay() {
  veThrMutexLock(display_mutex);
  redisplay_posted = 1;
  veThrCondBcast(display_post);
  veThrMutexUnlock(display_mutex);
}

static void wait_forever(void) {
  /* on slaves, we don't need a render thread... */
  VeThrMutex *m = veThrMutexCreate();
  VeThrCond *c = veThrCondCreate();
  veThrMutexLock(m);
  while (1)
    veThrCondWait(c,m);
  veThrMutexUnlock(m);
}

static void *render_thread(void *arg) {
  VeStatistic *frame_rate_stat, *render_time_stat;
  float frame_rate = 0.0, render_time = 0.0;
  int sample_frames = 10;
  int nframes = 0;
  long elapsed = veClock();
  float tm;

  render_time_stat = veNewStatistic(MODULE, "render_time", "secs");
  render_time_stat->type = VE_STAT_FLOAT;
  render_time_stat->data = &render_time;
  frame_rate_stat = veNewStatistic(MODULE, "frame_rate", "Hz");
  frame_rate_stat->type = VE_STAT_FLOAT;
  frame_rate_stat->data = &frame_rate;

  veAddStatistic(render_time_stat);
  veAddStatistic(frame_rate_stat);
  
  veThrMutexLock(display_mutex);
  while (1) {
    if (redisplay_posted) {
      redisplay_posted = 0;
      veThrMutexUnlock(display_mutex);
      veLockFrameExcl();
      vePfEvent(MODULE,"render-start",NULL);
      veRender();
      vePfEvent(MODULE,"render-end",NULL);
      veUnlockFrameExcl();
      veThrMutexLock(display_mutex);
      nframes++;
      if (nframes >= sample_frames) {
	tm = (veClock() - elapsed)/1000.0;
        frame_rate = nframes/tm;
        render_time = tm/(float)nframes;
	veThrMutexUnlock(display_mutex);
	veUpdateStatistic(render_time_stat);
	veUpdateStatistic(frame_rate_stat);
	veThrMutexLock(display_mutex);
	nframes = 0;
	elapsed = veClock();
      }
    }
    while (!redisplay_posted) {
      if (idle_proc) {
	veThrMutexUnlock(display_mutex);
	veLockCallbacks();
	idle_proc();
	veUnlockCallbacks();
	veThrMutexLock(display_mutex);
      } else
	veThrCondWait(display_post,display_mutex);
    }
  }
  veThrMutexUnlock(display_mutex);
}

static void *timer_thread(void *arg) {
  while (1) {
    while (!veTimerEventsPending())
      veTimerWaitForEvents();
    veLockFrame();
    while (veTimerEventsPending()) {
      veLockCallbacks();
      if (veTimerProcEvent())
	veFatalError(MODULE, "failed to handle timer events");
      veUnlockCallbacks();
    }
    veUnlockFrame();
  }
}

static void *input_thread(void *arg) {
  VeDeviceEvent *e;
  while (1) {
    while (!veDeviceEventPending())
      veDeviceWaitForEvents();
    if ((e = veDeviceNextEvent(VE_QUEUE_HEAD))) {
      veDeviceProcessEvent(e);
      veDeviceEventDestroy(e);
    }
  }
}

void veEventLoop() {
  if (veMPIsMaster()) {
    veThreadInit(NULL,input_thread,NULL,0,0);
    veThreadInit(NULL,timer_thread,NULL,0,0);
  }
#if defined(__APPLE__)
  veThreadInit(NULL,render_thread,NULL,0,0);
  RunApplicationEventLoop();
#else
  /* turn this thread into the rendering thread */
  if (veMPIsMaster())
    render_thread(NULL);
  else
    wait_forever();
#endif /* __APPLE__ */
}

void veRun() {
  /* pre-run checks... */
  if (!veGetEnv())
    veFatalError(MODULE,"no environment loaded");
  if (!veGetProfile())
    veFatalError(MODULE,"no user profile loaded");

  if (VE_ISDEBUG(1)) {
    veDeviceDumpFTable();
  }

  veMPInit(); /* set-up slave processes */
  veRenderRun(); /* let my renderers go... */
  veThrReleaseDelayed(); /* tell all held processing threads to 
			    get on with it */
  vePostRedisplay();   /* get everything to render at least once... */
  vePfEvent(MODULE,"event-loop",NULL);
  veEventLoop(); /* process VE events and render when necessary */
}
