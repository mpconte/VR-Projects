/* Parts of the OpenGL interface that can be done strictly in OpenGL,
   i.e. without knowledge of the underlying implementation */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ve_clock.h>
#include <ve_thread.h>
#include <ve_error.h>
#include <ve_stats.h>
#include <ve_mp.h>
#include <vei_gl.h>

#define MODULE "vei_gl"

static VeiGlWindowProc window_cback = NULL;
static VeiGlDisplayProc display_cback = NULL;
static VeiGlPDisplayProc pre_display_cback = NULL, post_display_cback = NULL;

static int gl_draw_stats = 0;

/* this is not rigorous, but is unlikely to be used as a tag */
#define DMSG_LATENCY 0x90
#define DTAG_LATENCY_PROBE 0x98
#define DTAG_LATENCY_PUSH  0x99

#define RTSAMPLES 10
void veiGlUpdateRTime(float s) {
  static float rtsamp[RTSAMPLES];
  static int filled = 0;
  static int ind = 0;
  static VeStatistic *rtime_stat = NULL;
  static float rtime = 0.0;
  static VeThrMutex *mutex = NULL;
  if (!mutex) {
    mutex = veThrMutexCreate();
  }
  veThrMutexLock(mutex);
  if (!rtime_stat) {
    rtime_stat = veNewStatistic(MODULE, "gl_render_time", "msecs");
    rtime_stat->type = VE_STAT_FLOAT;
    rtime_stat->data = &rtime;
    veAddStatistic(rtime_stat);
  }
  rtsamp[ind++] = s;
  if (ind >= RTSAMPLES) {
    ind = 0;
    filled = 1;
  }
  if (filled) {
    int k;
    rtime = 0.0;
    for(k = 0; k < RTSAMPLES; k++)
      rtime += rtsamp[k];
    rtime /= RTSAMPLES;
    veUpdateStatistic(rtime_stat);
  }
  veThrMutexUnlock(mutex);
}

#define FTSAMPLES 10
void veiGlUpdateFTime(float s) {
  static float ftsamp[FTSAMPLES];
  static int filled = 0;
  static int ind = 0;
  static VeStatistic *ftime_stat = NULL;
  static float ftime = 0.0;
  static VeThrMutex *mutex = NULL;
  if (!mutex) {
    mutex = veThrMutexCreate();
  }
  veThrMutexLock(mutex);
  if (!ftime_stat) {
    ftime_stat = veNewStatistic(MODULE, "gl_frame_time", "msecs");
    ftime_stat->type = VE_STAT_FLOAT;
    ftime_stat->data = &ftime;
    veAddStatistic(ftime_stat);
  }
  ftsamp[ind++] = s;
  if (ind >= FTSAMPLES) {
    ind = 0;
    filled = 1;
  }
  if (filled) {
    int k;
    ftime = 0.0;
    for(k = 0; k < FTSAMPLES; k++)
      ftime += ftsamp[k];
    ftime /= FTSAMPLES;
    veUpdateStatistic(ftime_stat);
  }
  veThrMutexUnlock(mutex);
}

void veiGlUpdateFrameRate(void) {
  static VeStatistic *frame_rate_stat = NULL;
  static float frame_rate = 0.0;
  static int sample_frames = 10;
  static int nframes = 0;
  static long elapsed;
  float tm;

  if (!frame_rate_stat) {
    frame_rate_stat = veNewStatistic(MODULE, "gl_frame_rate", "Hz");
    frame_rate_stat->type = VE_STAT_FLOAT;
    frame_rate_stat->data = &frame_rate;
    veAddStatistic(frame_rate_stat);
    elapsed = veClock();
  }
  nframes++;
  if (nframes >= sample_frames) {
    tm = (veClock() - elapsed)/1000.0;
    frame_rate = nframes/tm;
    veUpdateStatistic(frame_rate_stat);
    nframes = 0;
    elapsed = veClock();
  }
}

static VeStatistic *mp_sync_time_stat = NULL;
static int mp_sync_sample_limit = 10;
static float mp_sync_time = 0.0, mp_sync_acc = 0.0;
static int mp_sync_samples = 0;

/* frame rate we are targetting - this is a poor hack but a non-critical
   one for where this is used */
#define TARGETED_HZ 60

char *veiGlGetOption(VeWindow *w, char *name) {
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

void veiGlSetWindowCback(VeiGlWindowProc proc) {
  window_cback = proc;
}

void veiGlSetupWindow(VeWindow *w) {
  if (window_cback)
    window_cback(w);
}

void veiGlSetPreDisplayCback(VeiGlPDisplayProc proc) {
  if (veMPTestSlaveGuard())
    return;
  pre_display_cback = proc;
}

void veiGlSetPostDisplayCback(VeiGlPDisplayProc proc) {
  if (veMPTestSlaveGuard())
    return;
  post_display_cback = proc;
}

void veiGlSetDisplayCback(VeiGlDisplayProc proc) {
  display_cback = proc;
}

void veiGlDisplayWindow(VeWindow *w, long tm, VeWallView *wv) {
  if (display_cback)
    display_cback(w,tm,wv);
}

void veiGlPreDisplay(void) {
  if (pre_display_cback)
    pre_display_cback();
  if (mp_sync_time_stat) {
    /* update latency stat - this eats into the frame rate so we
       should only check it if the stat exists */
    long t1,t2;
    float dt;
    int k;
    for(k = 0; k < veMPNumProcesses(); k++) {
      t1 = veClockNano();
      if (veMPIntPush(k,DMSG_LATENCY,DTAG_LATENCY_PROBE,NULL,0) == 0) {
	veMPGetResponse(k);
	t2 = veClockNano();
	dt = (t2-t1)*1.0e-9;
	veMPIntPush(k,DMSG_LATENCY,DTAG_LATENCY_PUSH,&dt,sizeof(dt));
      }
    }
  }
}

void veiGlPostDisplay(void) {
  if (post_display_cback)
    post_display_cback();
}

void veiGlRenderMonoWindow(VeWindow *w, long tm, VeFrame *eye_in, int steye) {
  VeWallView v;
  float viewm[16], projm[16];
  int i,j;
  long t1;
  VeFrame eye;

  veCalcStereoEye(eye_in,&eye,veGetProfile(),steye);
  veGetWindowView(w,eye_in,&eye,&v);
  v.eye = steye;

  for(i = 0; i < 4; i++)
    for(j = 0; j < 4; j++) {
      viewm[i+j*4] = v.view.data[i][j];
      projm[i+j*4] = v.proj.data[i][j];
    }

  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(projm);
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(viewm);

  if (w->viewport[2] != 0.0 && w->viewport[3] != 0.0) {
    glViewport(w->viewport[0],w->viewport[1],
	       w->viewport[2],w->viewport[3]);
  }
  
  /* render mono window */
  t1 = veClock();
  veiGlDisplayWindow(w,tm,&v);
  veiGlUpdateFTime((float)(veClock()-t1));

  if (gl_draw_stats) {
    glColor3f(1.0,1.0,1.0);
    veiGlPushTextMode(w);
    veiGlDrawStatistics(NULL,0);
    veiGlPopTextMode();
  }
}

void veiGlRenderWindow(VeWindow *w) {
  long t1;
  t1 = veClock();
  switch (w->eye) {
  case VE_WIN_MONO:
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,veGetDefaultEye(),
			  VE_EYE_MONO);
    break;
  case VE_WIN_LEFT:
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,veGetDefaultEye(),
			  VE_EYE_LEFT);
    break;
  case VE_WIN_RIGHT:
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,veGetDefaultEye(),
			  VE_EYE_RIGHT);
    break;
  case VE_WIN_STEREO:
    glDrawBuffer(GL_BACK_LEFT);
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,veGetDefaultEye(),
			  VE_EYE_LEFT);
    glDrawBuffer(GL_BACK_RIGHT);
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,veGetDefaultEye(),
			  VE_EYE_RIGHT);
    break;
  default:
    veFatalError(MODULE,"veiGlRenderWindow: unexpected value for"
		 " window eye: %d", w->eye);
  }
  veiGlUpdateRTime((float)(veClock()-t1));
}

void veiGlParseGeom(char *geom, int *x, int *y, int *w, int *h) {
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

static void latency_check(int msg, int tag, void *data, int dlen) {
  switch (tag) {
  case DTAG_LATENCY_PROBE:
    veMPRespond(0);
    break;
  case DTAG_LATENCY_PUSH:
    mp_sync_acc += *(float *)data;
    mp_sync_samples++;
    if (mp_sync_samples >= mp_sync_sample_limit) {
      mp_sync_time = mp_sync_acc/mp_sync_samples;
      mp_sync_acc = 0.0;
      mp_sync_samples = 0;
      veUpdateStatistic(mp_sync_time_stat);
    }
    break;
  }
}

void veiGlRenderRun(void) {
  char *s;
  if ((s = veGetOption("gl_draw_stats")))
    gl_draw_stats = atoi(s);

  if ((s = veGetOption("gl_sync_time")) && atoi(s) != 0) {
    mp_sync_time_stat = veNewStatistic(MODULE, "sync_time", "secs");
    mp_sync_time_stat->type = VE_STAT_FLOAT;
    mp_sync_time_stat->data = &mp_sync_time;
    veAddStatistic(mp_sync_time_stat);
    veMPAddIntHandler(DMSG_LATENCY,VE_DTAG_ANY,latency_check);
  }
}

void veiGlInit(void) {
  /* nothing at this point in time */
}
