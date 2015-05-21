#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <sys/time.h>
#include <ve_core.h>
#include <ve_thread.h>
#include <ve_device.h>
#include <ve_driver.h>
#include <ve_debug.h>

#define MODULE "ve_core"

static float core_znear, core_zfar;
static VeFrame core_origin, core_defaulteye;
static VeEnv *environment = NULL;
static VeProfile *profile = NULL;
static VeThrMutex *cback_mutex = NULL;

static int cbacks_locked = 0;

void veLockCallbacks() {
  VE_DEBUG(4,("[%2x] Locking callbacks",veThreadId()));
  veThrMutexLock(cback_mutex);
  VE_DEBUG(4,("[%2x] Callbacks locked",veThreadId()));
  cbacks_locked = 1;
}

void veUnlockCallbacks() {
  cbacks_locked = 0;
  VE_DEBUG(4,("[%2x] Unlocking callbacks",veThreadId()));
  veThrMutexUnlock(cback_mutex);
}

int veCallbacksAreLocked() {
  return cbacks_locked;
}

VeFrame *veGetOrigin() {
  return &core_origin;
}

void veSetOrigin(VeFrame *l) {
  if (l != &core_origin)
    core_origin = *l;
}

VeFrame *veGetDefaultEye() {
  return &core_defaulteye;
}

void veSetDefaultEye(VeFrame *l) {
  if (l != &core_defaulteye)
    core_defaulteye = *l;
}

void veSetEnv(VeEnv *env) {
  environment = env;
}

VeEnv *veGetEnv() {
  return environment;
}

void veSetProfile(VeProfile *p) {
  profile = p;
}

VeProfile *veGetProfile() {
  return profile;
}

void veCalcStereoEye(VeFrame *eye_in, VeFrame *out, 
		    VeProfile *pro, int which) {
  VeVector3 d;
  int i;
  float sc;
  
  if (which == VE_EYE_LEFT)
    sc = 1.0;
  else
    sc = -1.0;

  veVectorCross(&eye_in->up,&eye_in->dir,&d);
  veVectorNorm(&d);
  out->up = eye_in->up;
  out->dir = eye_in->dir;
  for(i = 0; i < 3; i++)
    out->loc.data[i] = eye_in->loc.data[i] + sc*d.data[i]*(pro->stereo_eyedist/2.0);
}

void veGetUnmapMatrix(VeMatrix4 *m, VeFrame *f) {
  if (f == NULL)
    f = veGetOrigin();
  /* build a matrix that converts from env space to world space */
  veMatrixInvLookAt(m,&f->loc,&f->dir,&f->up);
}
 
void veGetMapMatrix(VeMatrix4 *m, VeFrame *f) {
  if (f == NULL)
    f = veGetOrigin();
  /* build a matrix that converts from env space to world space */
  veMatrixLookAt(m,&f->loc,&f->dir,&f->up);
}

void veGetWindowView(VeWindow *w, VeFrame *default_eye,
		     VeFrame *eye, VeWallView *wv) {
  VeView *view = w->wall->view;
  VeMatrix4 m;
  VeFrame f;
  VeVector3 v_cent, v_norm, zero_in, zero, cp;
  int i;
  float sc = 1.0;
  float ratio;

  if (!default_eye)
    default_eye = veGetDefaultEye();
  if (!eye)
    eye = default_eye;

  veGetUnmapMatrix(&m,veGetOrigin());
  
  if (view->base == VE_WALL_EYE)
    /* convert the wall-view from eye-space to "global"-space 
       (which will be interpreted as "origin"-space */
    veUnmapFrame(default_eye,&(view->frame),&f);
  else {
    if (view->base != VE_WALL_ORIGIN)
      veWarning(MODULE,"veGetWindowView: wall has unknown base (%d) - assuming 'origin'",view->base);
    /* wall-view is already in origin space */
    f = view->frame;
  }

  /* convert our viewing data into world space */
  veMatrixMultPoint(&m,&eye->loc,&wv->camera.loc,NULL);
  veMatrixMultPoint(&m,&f.loc,&v_cent,NULL);
  veMatrixMultPoint(&m,&f.dir,&v_norm,NULL);
  veMatrixMultPoint(&m,&f.up,&wv->camera.up,NULL);
  vePackVector(&zero_in,0.0,0.0,0.0);
  veMatrixMultPoint(&m,&zero_in,&zero,NULL);
  for(i = 0; i < 3; i++) {
    v_norm.data[i] -= zero.data[i];
    wv->camera.up.data[i] -= zero.data[i];
  }

  /* now look from the camera to the closest point on the screen plane */
  /* but do it with the orientation (i.e. up) of the screen */
  if (veVectorDot(&v_norm,&wv->camera.loc) > veVectorDot(&v_norm,&v_cent))
    /* normal is pointing away from the camera */
    sc = -1.0;
  for(i = 0; i < 3; i++)
    wv->camera.dir.data[i] = sc*v_norm.data[i];
  veMatrixLookAt(&wv->view,&wv->camera.loc,&wv->camera.dir,&wv->camera.up);
  veMatrixInvLookAt(&wv->iview,&wv->camera.loc,&wv->camera.dir,
		    &wv->camera.up);
  
  /* now work out the screen co-ordinates in the new space */
  veMatrixMultPoint(&wv->view,&v_cent,&cp,NULL);

  /* need to translate these points into the near clipping plane to
     build a correct frustum projection */
  ratio = core_znear/fabs(cp.data[2]);
  {
    float wid = (view->width + w->width_err)/2.0;
    float hgt = (view->height + w->height_err)/2.0;
    VeMatrix4 fstm;
    wv->near = core_znear;
    wv->far = core_zfar;
    wv->left = (cp.data[0] - wid + w->offset_x)*ratio;
    wv->right = (cp.data[0] + wid + w->offset_x)*ratio;
    wv->top = (cp.data[1] + hgt + w->offset_y)*ratio;
    wv->bottom = (cp.data[1] - hgt + w->offset_y)*ratio;

    veMatrixFrustum(&fstm,wv->left,wv->right,wv->bottom,wv->top,
		    wv->near,wv->far);
    veMatrixMult(&fstm,&w->distort,&wv->proj);
  }
  /* huzzah! */
}

void veSetZClip(float near, float far) {
  core_znear = near;
  core_zfar = far;
}

void veFrameIdentity(VeFrame *f) {
  vePackVector(&f->loc,0.0,0.0,0.0);
  vePackVector(&f->dir,0.0,0.0,-1.0);
  vePackVector(&f->up,0.0,1.0,0.0);
}

void veMultFrame(VeMatrix4 *m, VeFrame *f, VeFrame *fp) {
  VeVector3 o, zero_in;
  int i;

  veMatrixMultPoint(m,&f->loc,&fp->loc,NULL);
  veMatrixMultPoint(m,&f->dir,&fp->dir,NULL);
  veMatrixMultPoint(m,&f->up,&fp->up,NULL);
  vePackVector(&zero_in,0.0,0.0,0.0);
  veMatrixMultPoint(m,&zero_in,&o,NULL);
  for(i = 0; i < 3; i++) {
    fp->dir.data[i] -= o.data[i];
    fp->up.data[i] -= o.data[i];
  }
}

void veMapFrame(VeFrame *ref, VeFrame *f, VeFrame *fp) {
  VeMatrix4 m;
  
  veMatrixLookAt(&m,&ref->loc,&ref->dir,&ref->up);
  veMultFrame(&m,f,fp);
}

void veUnmapFrame(VeFrame *ref, VeFrame *f, VeFrame *fp) {
  VeMatrix4 m;
  
  veMatrixInvLookAt(&m,&ref->loc,&ref->dir,&ref->up);
  veMultFrame(&m,f,fp);
}

/* The frame interlock */
static int fl_locked = 0;
static int fl_epending = 0;
static VeThrMutex *fl_mutex;
static VeThrCond *fl_eclear, *fl_zero;

void veLockFrame(void) {
  veThrMutexLock(fl_mutex);
  VE_DEBUG(5,("[%d] veLockFrame enter",veThreadId()));
  while (fl_epending)
    veThrCondWait(fl_eclear,fl_mutex);
  while (fl_locked < 0)
    veThrCondWait(fl_zero,fl_mutex);
  fl_locked++;
  VE_DEBUG(5,("[%d] veLockFrame exit",veThreadId()));
  veThrMutexUnlock(fl_mutex);
}

void veUnlockFrame(void) {
  veThrMutexLock(fl_mutex);
  VE_DEBUG(5,("[%d] veUnlockFrame enter",veThreadId()));
  assert(fl_locked > 0);
  fl_locked--;
  if (fl_locked == 0)
    veThrCondBcast(fl_zero);
  VE_DEBUG(5,("[%d] veUnlockFrame exit",veThreadId()));
  veThrMutexUnlock(fl_mutex);
}

void veLockFrameExcl(void) {
  veThrMutexLock(fl_mutex);
  VE_DEBUG(5,("[%d] veLockFrameExcl enter",veThreadId()));
  if (fl_locked != 0) {
    fl_epending++;
    while(fl_locked != 0)
      veThrCondWait(fl_zero,fl_mutex);
    fl_epending--;
    if (fl_epending == 0)
      veThrCondBcast(fl_eclear);
  }
  fl_locked = -1;
  VE_DEBUG(5,("[%d] veLockFrameExcl exit",veThreadId()));
  veThrMutexUnlock(fl_mutex);
}

void veUnlockFrameExcl(void) {
  veThrMutexLock(fl_mutex);
  VE_DEBUG(5,("[%d] veLockFrameExcl enter",veThreadId()));
  assert(fl_locked == -1);
  fl_locked = 0;
  veThrCondBcast(fl_zero);
  VE_DEBUG(5,("[%d] veUnlockFrameExcl exit",veThreadId()));
  veThrMutexUnlock(fl_mutex);
}

void veCoreInit() {
  veFrameIdentity(&core_origin);
  veFrameIdentity(&core_defaulteye);
  core_znear = 0.01;
  core_zfar = 100.0;
  cback_mutex = veThrMutexCreate();
  fl_mutex = veThrMutexCreate();
  fl_eclear = veThrCondCreate();
  fl_zero = veThrCondCreate();
}
