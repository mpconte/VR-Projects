#include <ve_core.h>
#include <ve_env.h>
#include <ve_driver.h>
#include <ve_render.h>

#define MODULE "ve_render_shim"

/* This module is responsible for isolating the library from the
   actual rendering driver */

#define FNAME(x) "veRImpl" #x

void veRenderInit(void) {
  static void *(*_f)(void) = NULL;
  if (!_f) {
    /* do we have a driver loaded */
    if (!(_f = (void *(*)(void))veFindDynFunc(FNAME(Init)))) {
      if (veDriverRequire("render","*"))
	veFatalError(MODULE,"no rendering support available");
    }
    if (!(_f = (void *(*)(void))veFindDynFunc(FNAME(Init))))
      veFatalError(MODULE,"bogus rendering driver - missing " FNAME(RenderInit));
  }
  _f();
}

int veRenderOpenWindow(VeWindow *w) {
  static int (*_f)(VeWindow *) = NULL;
  if (!_f) {
    if (!(_f = (int (*)(VeWindow *))veFindDynFunc(FNAME(OpenWindow))))
      veFatalError(MODULE,"bad render driver - missing " FNAME(OpenWindow));
  }
  return _f(w);
}

void veRenderCloseWindow(VeWindow *w) {
  static void (*_f)(VeWindow *) = NULL;
  if (!_f) {
    if (!(_f = (void (*)(VeWindow *))veFindDynFunc(FNAME(CloseWindow))))
      veFatalError(MODULE,"bad render driver - missing " FNAME(CloseWindow));
  }
  _f(w);
}

void veRenderWindow(VeWindow *w) {
  static void (*_f)(VeWindow *) = NULL;
  if (!_f) {
    if (!(_f = (void (*)(VeWindow *))veFindDynFunc(FNAME(Window))))
      veFatalError(MODULE,"bad render driver - missing " FNAME(Window));
  }
  _f(w);
}

void veRenderSwap(VeWindow *w) {
  static void (*_f)(VeWindow *) = NULL;
  if (!_f) {
    if (!(_f = (void (*)(VeWindow *))veFindDynFunc(FNAME(Swap))))
      veFatalError(MODULE,"bad render driver - missing " FNAME(Swap));
  }
  _f(w);
}

void veRenderGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  static void (*_f)(VeWindow *, int *, int *, int *, int *) = NULL;
  if (!_f) {
    if (!(_f = (void (*)(VeWindow *, int *, int *, int *, int *))
	  veFindDynFunc(FNAME(GetWinInfo))))
      veFatalError(MODULE,"bad render driver - missing " FNAME(GetWinInfo));
  }
  _f(win,x,y,w,h);
}

void veRenderReloadView(VeWindow *w, long tm, float znear, float zfar,
			VeWallView *wv) {
  static void (*_f)(VeWindow *, long, float, float, VeWallView *) = NULL;
  if (!_f) {
    if (!(_f = (void (*)(VeWindow *, long, float, float, VeWallView *))
	  veFindDynFunc(FNAME(ReloadView))))
      veFatalError(MODULE,"bad render driver - missing " FNAME(ReloadView));
  }
  _f(w,tm,znear,zfar,wv);
}
