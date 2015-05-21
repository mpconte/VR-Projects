/* Renderer code that is common across all renderers. */
#include <ve_render.h>

/* callback management */
static VeRenderSetupProc setup_cback = NULL;
static VeRenderProc render_cback = NULL;
static VeRenderPreProc prerender_cback = NULL;
static VeRenderPostProc postrender_cback = NULL;

void veRenderSetupCback(VeRenderSetupProc cback) {
  setup_cback = cback;
}

void veRenderCback(VeRenderProc cback) {
  render_cback = cback;
}

void veRenderPreCback(VeRenderPreProc cback) {
  if (veMPTestSlaveGuard())
    return;
  prerender_cback = cback;
}

void veRenderPostCback(VeRenderPostProc cback) {
  if (veMPTestSlaveGuard())
    return;
  postrender_cback = cback;
}

void veRenderCallSetupCback(VeWindow *w) {
  if (setup_cback)
    setup_cback(w);
    
}

void veRenderCallCback(VeWindow *w, long tm, VeWallView *wv) {
  if (render_cback)
    render_cback(w,tm,wv);
}

void veRenderCallPreCback(void) {
  if (prerender_cback)
    prerender_cback();
}

void veRenderCallPostCback(void) {
  if (postrender_cback)
    postrender_cback();
}
