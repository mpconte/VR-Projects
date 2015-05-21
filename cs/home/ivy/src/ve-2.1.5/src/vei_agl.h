#ifndef VE_IMPL_AGL_H
#define VE_IMPL_AGL_H

/** misc
    This is a rudimentary port of VE to Apple's AGL interface which is
    the link between OpenGL and the Carbon API.
    <p>Functions in this module that are not implementations of another
    API should only be called sparingly by applications, as they are not
    portable to other OpenGL-based implementations of VE.  Applications
    which include the <code>vei_agl.h</code> header file do not need to
    include the <code>vei_gl.h</code> header file or other OpenGL
    header files.  These are included automatically.
    <p>This module implements functions from the <code>ve_render.h</code>
    and <code>vei_gl.h</code> modules.
*/

#include <vei_gl.h>
#include <Carbon/Carbon.h>
#include <AGL/agl.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

/** struct VeiAglWindow
    Represents the AGL window.  There is one VeiAglWindow structure for
    each open VeWindow object.
*/
typedef struct vei_agl_window {
  /** member win
      The window which the render is using for this window.
  */
  WindowRef win;
  /** member ctx
      The AGL context which this window is rendered with.  Each window
      has its own AGL context.
  */
  AGLContext ctx;
  /** member stereo
      A boolean value - if non-zero, then this window is stereo.
  */
  int stereo;
  /** member x,y,w,h
      A cached copy of the window's geometry.  It is assumed in the
      current model that the geometry does not change once the window
      has been opened.
  */
  int x,y,w,h;
  /** member glInfo
      A structure used by the Apple initialization code.  It is
      referenced here, but should be treated as opaque by applications.
  */
  void *glInfo;
} VeiAglWindow;

#ifdef __cplusplus
}
#endif

#endif /* VE_IMPL_AGL_H */
