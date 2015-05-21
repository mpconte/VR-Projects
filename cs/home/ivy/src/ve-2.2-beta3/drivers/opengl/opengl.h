#ifndef OPENGL_H
#define OPENGL_H

/* standard OpenGL headers */
#if defined(WIN32)
#include <windows.h>
#pragma warning (disable:4244)          /* disable bogus conversion warnings */
#endif
/* disable "special" Apple interface for now... */
#if defined(__APPLE__) && 0
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif /* __APPLE__ */

#include <ve.h>

#ifdef __cplusplus
extern "C" {
#endif
  /* The following is only there to keep the above from confusing indentation
     in Emacs */
#if 0
}
#endif

/* internal functions for the OpenGL module */
void veiGlInit(void); /* initialize implementation */
int veiGlOpenWindow(VeWindow *w); /* platform-specific window opening */
void veiGlCloseWindow(VeWindow *w); /* platform-specific window closing */
void veiGlSetWindow(VeWindow *w); /* set active window (NULL = deactivate) */
void veiGlUnsetWindow(VeWindow *w); /* unset active window (if supported) */
void veiGlSwapBuffers(VeWindow *w); /* swap window (active window only) */
void veiGlGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* OPENGL_H */
