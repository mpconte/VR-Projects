/* A "null" renderer which is just placeholders for the rendering functions.
   Useful for when you want to test parts of the library that don't need the
   renderer. */
#include <ve_render.h>

#define MODULE "vei_null.h"

void veRenderInit() {}
void veRenderAllocMP() {}
void veRenderRun() {}
void veRender() {}
void veRenderGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  if (x) *x = 0;
  if (y) *y = 0;
  if (w) *w = 0;
  if (h) *h = 0;
}
