/* The glue bits of VE - tying all the modules together */
/* Some programs may omit this when incorporating bits of VE under
   another app model */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <ve_alloc.h>
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
#include <ve_audio.h>

#define MODULE "ve_main"

#define STRINGIFY(x) #x

static VeThrMutex *display_mutex = NULL;
static VeThrCond *display_post = NULL;
static VeStrMap runtime_options = NULL;
static int redisplay_posted = 0;
static VeIdleProc idle_proc = NULL;
static VeAnimProc anim_proc = NULL;

/* initialized by veRun() */
static VeClockHR global_t0, last_anim;
static int anim_init = 0;

typedef struct ve_file_list_elem {
  struct ve_file_list_elem *next;
  char *file;
} VeFileListElem;

typedef struct ve_file_list {
  VeFileListElem *head, *tail;
} VeFileList;

static VeFileList conf_list = { NULL, NULL };
static VeFileList script_list = { NULL, NULL };

/* inefficient, but it's only at startup, so who cares? */
void fileListAdd(VeFileList *l, char *f) {
  VeFileListElem *x = veAllocObj(VeFileListElem);
  x->next = NULL;
  x->file = veDupString(f);
  if (l->tail)
    l->tail->next = x;
  else
    l->head = x;
  l->tail = x;
}

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

static void call_anim_proc(void) {
  float t, dt;
  VeClockHR now;
  
  if (!anim_proc)
    return;

  now = veClockHires();
  VE_DEBUGM(2,("now = %ld, %ld",now.secs, now.nsecs));
  VE_DEBUGM(2,("global_t0 = %ld, %ld",global_t0.secs, global_t0.nsecs));
  t = (float)((long)now.secs - (long)global_t0.secs) +
    ((long)now.nsecs - (long)global_t0.nsecs)*1.0e-9;
  if (!anim_init) {
    dt = 0.0;
    anim_init = 1;
  } else {
    dt = ((long)now.secs - (long)last_anim.secs) +
      ((long)now.nsecs - (long)last_anim.nsecs)*1.0e-9;
  }
  VE_DEBUGM(2,("calling animation (%f,%f)",t,dt));
  anim_proc(t,dt);
  last_anim = now;
}

void veSetIdleProc(VeIdleProc proc) {
  idle_proc = proc;
}

void veSetAnimProc(VeAnimProc proc) {
  anim_proc = proc;
  if (anim_proc)
    anim_init = 0;
}

void veSetOption(char *opt, char *val) {
  char *s;

  assert(runtime_options != NULL);

  veFree(veStrMapLookup(runtime_options,opt));
  veStrMapInsert(runtime_options,opt,veDupString(val));
}

char *veGetOption(char *opt) {
  assert(runtime_options != NULL);
  return veStrMapLookup(runtime_options,opt);
}

char *veGetWinOption(VeWindow *w, char *name) {
  /* The following order (a little odd, but think about it):
     - VeWindow (forced by config for a window)
     - VeGetOption (forced by program)
     - VeWall (default for wall)
     - VeEnv (default for world)
     - NULL
     */
  char *s;
  if ((s = veEnvGetOption(w->options,name)))
    return s;
  if ((s = veGetOption(name)))
    return s;
  if ((s = veEnvGetOption(w->wall->options,name)))
    return s;
  if ((s = veEnvGetOption(w->wall->env->options,name)))
    return s;
  return NULL;
}

void veParseGeom(char *geom, int *x, int *y, int *w, int *h) {
  int i,n;
  char sx, sy;
  if (sscanf(geom,"%dx%d%n", w, h, &n) != 2)
    veFatalError(MODULE, "geometry \"%s\" is missing width/height", geom);
  *x = *y = 0;
  if ((i = sscanf(&(geom[n]),"%c%d%c%d",&sx,x,&sy,y)) != 4 && i != 0)
    veFatalError(MODULE, "malformed offset in geometry %s", geom);
  switch (sx) {
  case '-':  *x *= -1; break;
  case '+':  break;
  default:   veFatalError(MODULE, "malformed offset in geometry %s", geom);
  }
  switch (sy) {
  case '-':  *y *= -1; break;
  case '+':  break;
  default:   veFatalError(MODULE, "malformed offset in geometry %s", geom);
  }
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
      } else if (strcmp(argv[i],"-ve_conf") == 0) {
	/* "configuration" file - first-phase script */
	i++;
	if (i >= *argc)
	  veFatalError(MODULE, "missing argument to -ve_conf option");
	fileListAdd(&conf_list,argv[i]);
      } else if (strcmp(argv[i],"-ve_script") == 0) {
	/* second-phase script (after configuration) */
	i++;
	if (i >= *argc)
	  veFatalError(MODULE, "missing argument to -ve_script option");
	fileListAdd(&script_list,argv[i]);
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

/* options are now split into two groups:
   "pre" options that should be processed before we initialize
   anything (includes debug/notices/root)
   "post" options that should be processed after we initialize
   everything (env/profile/devices)
*/
static void process_pre_options(void) {
  char *s;
  FILE *f;

  if ((s = veGetOption("debug")))
    veSetDebugStr(s);

  if ((s = veGetOption("notices")))
    veShowNotices(atoi(s));

  if ((s = veGetOption("root"))) {
    char *senv;
    /* update VEROOT entry in environment */
    senv = veAlloc(strlen(s)+strlen("VEROOT=")+1,0);
    sprintf(senv,"VEROOT=%s",s);
    putenv(senv);
    /* explicitly set this as a root for the driver loading piece */
    veSetDriverRoot(s);
  }
#ifdef PREFIX
  /* allow for "second-chance" with compiled in path */
  else if (!getenv("VEROOT")) {
    char *senv;
    /* update VEROOT entry in environment with PREFIX */
    senv = veAlloc(strlen(STRINGIFY(PREFIX))+strlen("VEROOT=")+1,0);
    sprintf(senv,"VEROOT=%s",STRINGIFY(PREFIX));
    putenv(senv);
    /* explicitly set this as a root for the driver loading piece */
    veSetDriverRoot(STRINGIFY(PREFIX));
  }
#endif /* PREFIX */
}

static void evalstream(char *name, FILE *f) {
  if (veBlueEvalStream(f))
    veFatalError(MODULE, "failed to evaluate %s", name);
}

static void process_post_options(void) {
  FILE *f;
  char *s;

  /* run configuration scripts */
  {
    VeFileListElem *l;
    for (l = conf_list.head; l; l = l->next) {
      if ((f = veFindFile(NULL,l->file))) {
	evalstream(l->file,f);
	fclose(f);
      }
    }
  }

  if ((s = veGetOption("env"))) {
    if (!(f = veFindFile("env",s)))
      veFatalError(MODULE, "cannot open env file %s: %s", s, strerror(errno));
    evalstream(s,f);
    fclose(f);
    if (!getenv("VEENV")) {
      char *senv;
      senv = veAlloc(strlen(s)+strlen("VEENV=")+1,0);
      sprintf(senv,"VEENV=%s",s);
      putenv(senv);
    }
  } else {
    /* try to open default file if it exists */
    if ((f = veFindFile("env",DEFAULT_ENV))) {
      evalstream(DEFAULT_ENV,f);
      fclose(f);
    }
  }

  if ((s = veGetOption("user")) || (s = veGetOption("profile"))) {
    if (!(f = veFindFile("profile",s)))
      veFatalError(MODULE, "cannot open user profile %s: %s", s,
		   strerror(errno));
    evalstream(s,f);
    fclose(f);
    if (!getenv("VEUSER")) {
      char *senv;
      senv = veAlloc(strlen(s)+strlen("VEUSER=")+1,0);
      sprintf(senv,"VEUSER=%s",s);
      putenv(senv);
    }
  } else {
    /* try to open default file if it exists */
    if ((f = veFindFile("profile",DEFAULT_PROFILE))) {
      evalstream(DEFAULT_PROFILE,f);
      fclose(f);
    }
  }

  /* Only try to load device information on the master node */
  if (veMPIsMaster()) {
    if ((s = veGetOption("manifest"))) {
      if (!(f = veFindFile("manifest",s)))
	veFatalError(MODULE, "cannot open manifest %s: %s", s, strerror(errno));
      evalstream(s,f);
      fclose(f);
      if (!getenv("VEMANIFEST")) {
	char *senv;
	senv = veAlloc(strlen(s)+strlen("VEMANIFEST=")+1,0);
	sprintf(senv,"VEMANIFEST=%s",s);
	putenv(senv);
      }
    } else {
      /* try to open default file if it exists */
      if ((f = veFindFile("manifest",DEFAULT_MANIFEST))) {
	evalstream(DEFAULT_MANIFEST,f);
	fclose(f);
      }
    }

    if ((s = veGetOption("devices"))) {
      if (!(f = veFindFile("devices",s)))
	veFatalError(MODULE, "cannot open devices %s: %s", s, strerror(errno));
      evalstream(s,f);
      fclose(f);
    } else {
      /* try to open default file if it exists */
      if ((f = veFindFile("devices",DEFAULT_DEVICES))) {
	evalstream(DEFAULT_DEVICES,f);
	fclose(f);
      }
    }
  }

  /* run other scripts */
  {
    VeFileListElem *l;
    for (l = script_list.head; l; l = l->next) {
      if ((f = veFindFile(NULL,l->file))) {
	evalstream(l->file,f);
	fclose(f);
      }
    }
  }

  /* ... anything we needed was set by the scripts ... */
}

/* try to exit gracefully */
void veExit(void) {
  static int ran = 0; /* avoid atexit() loop */
  if (!ran) {
    ran = 1;
    VE_DEBUGM(2,("veExit: shutting down subsystems"));
    veMPRenderExit();
    VE_DEBUGM(2,("veExit: exiting"));
    exit(0);
  }
}

void veInit(int *argc, char **argv) {
  /* read in options from environment and command-line */
  /* initialize other pieces */
  veMPSlaveInit(argc,argv); /* rip out the slave option if there is one */

  runtime_options = veStrMapCreate();
  extract_envvar(); /* extract environment variables into options */
  extract_options(argc,argv);
  process_pre_options(); /* make sure root is set correctly */

  display_mutex = veThrMutexCreate();
  display_post = veThrCondCreate();

  veProbeRootDrivers(); /* discover what drivers we have available */

  veThrInitDelayGate();  /* initialize delay gate for spawned threads */
  vePfInit();       /* initialize profiler */
  vePfEvent(MODULE,"ve-init",NULL);
  veStatInit();     /* initialize stats gathering */
  veCoreInit();     /* initialize math and frames of reference */
  veDeviceInit();   /* initialize device queue */
  veClockInit();    /* initialize system clock */
  veTimerInit();    /* initialize system timer queue */
  veMPRenderInit(); /* intiialize multi-processing renderer */
  veRenderInit();   /* initialize the rendering system */
  veTXMInit();      /* initialize texture manager */
  veAudioInit();    /* pre-initialize the audio system */

  veBlueInit();

  process_post_options(); /* load env/profile/manifest/etc. */
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
  int anim_called = 0;

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
      /* synchronize data */
      veMPLocationPush();
      veMPPushStateVar(VE_DTAG_ANY,VE_MP_AUTO);
      vePfEvent(MODULE,"render-start",NULL);
      veMPRenderFrame(-1);
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

    /* process anim callback */
    if (anim_proc) {
      veThrMutexUnlock(display_mutex);
      veLockCallbacks();
      call_anim_proc();
      veUnlockCallbacks();
      veThrMutexLock(display_mutex);
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

void veEventLoop() {
  if (veMPIsMaster()) {
    veThreadInit(NULL,timer_thread,NULL,0,0);
  }
  /* turn this thread into the rendering thread */
  if (veMPIsMaster())
    render_thread(NULL);
  else
    wait_forever();
}

void veRun() {
  /* pre-run checks... */
  if (!veGetEnv())
    veFatalError(MODULE,"no environment loaded");
  if (!veGetProfile())
    veFatalError(MODULE,"no user profile loaded");

  veMPInit(); /* set-up slave processes */
  veMPEnvPush(); /* push environment to slaves */
  veMPProfilePush(); /* push profile to slaves */
  veMPRenderRun(); /* let my renderers go... */
  veMPAudioRun();
  veThrReleaseDelayed(); /* tell all held processing threads to 
			    get on with it */
  vePostRedisplay();   /* get everything to render at least once... */
  global_t0 = veClockHires();
  vePfEvent(MODULE,"event-loop",NULL);

  atexit(veExit);

  veEventLoop(); /* process VE events and render when necessary */
}

/* motion callbacks */
static int accept_all(int what, VeFrame *f, void *arg) {
  return VE_MC_ACCEPT;
}

static int reject_all(int what, VeFrame *f, void *arg) {
  return VE_MC_REJECT;
}

VeMotionCback VE_MC_ACCEPTALL = accept_all;
VeMotionCback VE_MC_REJECTALL = reject_all;

static VeMotionCback eye_cback = NULL, origin_cback = NULL;
static void *eye_arg = NULL, *origin_arg = NULL;

void veSetMotionCback(int what, VeMotionCback cback, void *arg) {
  if (what & VE_MC_EYE) {
    eye_cback = cback;
    eye_arg = arg;
  }
  if (what & VE_MC_ORIGIN) {
    origin_cback = cback;
    origin_arg = arg;
  }
}

int veCheckMotionCback(int what, VeFrame *f) {
  VeFrame fp;
  VeMotionCback cb = NULL;
  void *a = NULL;
  int res;

  switch (what) {
  case VE_MC_EYE:
    cb = eye_cback;
    a = eye_arg;
    break;
    
  case VE_MC_ORIGIN:
    cb = origin_cback;
    a = origin_arg;
    break;
    
  default:
    veError(MODULE,"invalid frame type for veCheckMotionCback (%d)",what);
    return VE_MC_REJECT;
  }
  
  if (!cb)
    return VE_MC_ACCEPT; /* no callback set */
  fp = *f;
  res = cb(what,&fp,a);
  if (res != VE_MC_ACCEPT && res != VE_MC_REJECT)
    res = VE_MC_REJECT; /* bad callback!  bad! bad! */
  return res;
}
