/*
  New-style OpenGL support built as a separate module.
  This module contains OpenGL support which works on all OpenGL implementations.
  This module alone is not sufficient for OpenGL support - it is typically compiled
  together with another module (e.g. glx) which handles the 
  platform-specific OpenGL windowing system interface.
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* deal with different ways of including OpenGL header */

#if defined(WIN32)
#include <windows.h>
#pragma warning (disable:4244)          /* disable bogus conversion warnings */
#include <GL/gl.h>
#include <GL/glu.h>
/* normal POSIX-like environment */
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <ve.h>

#define TARGETED_HZ 60

#define MODULE "driver:opengl"

/* implementation driver stubs */
void veiGlStubInit(void) {
  static void (*func)(void) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlInit")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlInit");
  func();
}

int veiGlStubOpenWindow(VeWindow *w) {
  static int (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlOpenWindow")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlOpenWindow");
  return func(w);
}

void veiGlStubCloseWindow(VeWindow *w) {
  static void (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlCloseWindow")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlCloseWindow");
  func(w);
}

void veiGlStubSetWindow(VeWindow *w) {
  static void (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlSetWindow")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlSetWindow");
  func(w);
}

void veiGlStubUnsetWindow(VeWindow *w) {
  static void (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlUnsetWindow")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlUnsetWindow");
  func(w);
}

void veiGlStubSwapBuffers(VeWindow *w) {
  static void (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlSwapBuffers")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlSwapBuffers");
  func(w);
}

void veiGlStubGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  static void (*func)(VeWindow *,int *, int *, int *, int *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlGetWinInfo")))
    veFatalError(MODULE,"invalid OpenGL implementation: no veiGlSwapBuffers");
  func(win,x,y,w,h);
}

/* this function is optional */
void veiGlStubPreRender(VeWindow *w) {
  static void (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlPreRender")))
    return;
  func(w);
}

void veiGlStubPostRender(VeWindow *w) {
  static void (*func)(VeWindow *) = NULL;
  if (!func && !(func = veFindDynFunc("veiGlPostRender")))
    return;
  func(w);
}

void veRImplGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  veiGlStubGetWinInfo(win,x,y,w,h);
}

void veRImplReloadView(VeWindow *w, long tm, 
		     float znear, float zfar,
		     VeWallView *wv) {
  /* look at values in wall view */
  VeFrame eye;
  float viewm[16], projm[16];
  int i,j;
  
  /* rebuild window view */
  veCalcStereoEye(&(wv->eye_frame),&eye,veGetProfile(),wv->eye);
  veGetWindowClipView(w,&(wv->eye_frame),&eye,znear,zfar,wv);

  /* construct GL matrices */
  for(i = 0; i < 4; i++)
    for(j = 0; j < 4; j++) {
      viewm[i+j*4] = wv->view.data[i][j];
      projm[i+j*4] = wv->proj.data[i][j];
    }

  VE_DEBUGM(4,("veRImplReloadView (%s)", w->name ? w->name : "<null>"));
  VE_DEBUGM(8,("viewm = \n[%f %f %f %f]\n[%f %f %f %f]\n"
	       "[%f %f %f %f]\n[%f %f %f %f]",
	       viewm[0], viewm[1], viewm[2], viewm[3],
	       viewm[4], viewm[5], viewm[6], viewm[7],
	       viewm[8], viewm[9], viewm[10], viewm[11],
	       viewm[12], viewm[13], viewm[14], viewm[15]
	       ));
  VE_DEBUGM(8,("projm = \n[%f %f %f %f]\n[%f %f %f %f]\n"
	       "[%f %f %f %f]\n[%f %f %f %f]",
	       projm[0], projm[1], projm[2], projm[3],
	       projm[4], projm[5], projm[6], projm[7],
	       projm[8], projm[9], projm[10], projm[11],
	       projm[12], projm[13], projm[14], projm[15]
	       ));
  
  /* update OpenGL matrices */
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(projm);
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(viewm);
}

void veiGlRenderMonoWindow(VeWindow *w, long tm, VeFrame *eye_in, int steye) {
  VeWallView v;
  long t1;
  VeFrame eye;
  float znear, zfar;
  static GLenum mmodes[] = { GL_COLOR, GL_TEXTURE, GL_PROJECTION,
			     GL_MODELVIEW };
  int j;

  VE_DEBUGM(4,("veiGlRenderMonoWindow enter"));

  veiGlStubPreRender(w);

  if (!eye_in)
    eye_in = veGetDefaultEye();

  memset(&v,0,sizeof(VeWallView));
  v.eye = steye;
  v.eye_frame = *eye_in;

  /* reset all matrices */
  for(j = 0; j < sizeof(mmodes)/sizeof(GLenum); j++) {
    glMatrixMode(mmodes[j]);
    glLoadIdentity();
  }

  /* set viewport */
  if (w->viewport[2] != 0.0 && w->viewport[3] != 0.0) {
    glViewport(w->viewport[0],w->viewport[1],
	       w->viewport[2],w->viewport[3]);
  } else {
    int width,height;
    veRImplGetWinInfo(w,NULL,NULL,&width,&height);
    glViewport(0,0,width,height);
  }

  /* load correct matrices */
  veGetZClip(&znear,&zfar);
  veRImplReloadView(w,tm,znear,zfar,&v);
  
  /* call application callback */
  veRenderCallCback(w,tm,&v);

  /* for testing */
  /* glFinish(); */

  veiGlStubPostRender(w);

  VE_DEBUGM(4,("veiGlRenderMonoWindow exit"));
}

/* should we check for OpenGL errors when rendering? */
static int check_errors = 0;

/* rendering interfaces we must supply */
void veRImplInit(void) {
  char *s;
  if ((s = getenv("VEGLERROR")))
    check_errors = atoi(s);
  veiGlStubInit(); /* let implementation handle it */
}

int veRImplOpenWindow(VeWindow *w) {
  return veiGlStubOpenWindow(w);
}

void veRImplCloseWindow(VeWindow *w) {
  veiGlStubCloseWindow(w);
}

void veRImplSwap(VeWindow *w) {
  VE_DEBUGM(4,("veRImplSwap %s",w->name ? w->name : "<null>"));
  veiGlStubSetWindow(w);
  veiGlStubSwapBuffers(w);
  veiGlStubUnsetWindow(w);
}


void veRImplWindow(VeWindow *w) {
  long t1;
  if (check_errors) {
    while (glGetError() != GL_NO_ERROR)
      ;
  }
  veiGlStubSetWindow(w); /* activate our window */
  switch (w->eye) {
  case VE_WIN_MONO:
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,NULL,VE_EYE_MONO);
    break;
  case VE_WIN_LEFT:
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,NULL,VE_EYE_LEFT);
    break;
  case VE_WIN_RIGHT:
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,NULL,VE_EYE_RIGHT);
    break;
  case VE_WIN_STEREO:
    glDrawBuffer(GL_BACK_LEFT);
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,NULL,VE_EYE_LEFT);
    glDrawBuffer(GL_BACK_RIGHT);
    veiGlRenderMonoWindow(w,veClock()+1000/TARGETED_HZ,NULL,VE_EYE_RIGHT);
    break;
  default:
    veFatalError(MODULE,"veRenderWindow: unexpected value for"
		 " window eye: %d", w->eye);
  }
  if (check_errors) {
    GLenum e;
    char *s;
    while ((e = glGetError()) != GL_NO_ERROR) {
      s = (char *)gluErrorString(e);
      veError(MODULE,"OpenGL error: %s",s ? s : "<unknown>");
    }
  }
  veiGlStubUnsetWindow(w);
}

void VE_DRIVER_PROBE(opengl) (void *phdl) {
  veDriverProvide(phdl,"render","opengl");
}

void VE_DRIVER_INIT(opengl) (void) {
  /* make sure we have an OpenGL implementation driver
     loaded */
  if (veDriverRequire("glimpl","*"))
    veFatalError(MODULE,"no OpenGL implementation available");
}
