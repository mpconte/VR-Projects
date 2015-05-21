#ifndef VE_RENDER_H
#define VE_RENDER_H

#include <ve_version.h>
#include <ve_core.h>
#include <ve_env.h>

#ifdef __cplusplus
extern "C" {
#endif

/** misc
    The ve_render module defines those interfaces that a renderer
    implementation must provide.
*/

/* What the renderer needs to provide */
/** function veRenderInit
    Provided by renderer.  Initializes the renderer.  This should not
    open windows or outputs.
 */
void veRenderInit(void);   /* initialize the subsystem */

/** function veRenderOpenWindow
    Opens a window and prepares it for rendering.  An implementation may
    consider it an error for the same window to be opened twice.

    @param w
    The window to open.

    @returns
    0 if successful, non-zero if not successful.  If not sucessful
    an error should also be printed on stderr.
 */
int veRenderOpenWindow(VeWindow *w);

/** function veRenderCloseWindow
    Closes a previously-opened window.

    @param w
    The window to close.
*/
void veRenderCloseWindow(VeWindow *w);

/** function veRenderWindow
    Renders the given window.  Render callbacks are called as required.
    For double-buffered renderers this should not swap the window at all.
 */
void veRenderWindow(VeWindow *w);

/** function veRenderSwap
    For double-buffered renderers, this swaps buffers on the windows.
    No effect is required for single-buffered renderers.
 */
void veRenderSwap(VeWindow *w);

/** function veRenderGetWinInfo
    Returns geometry information about an open window.  The window
    must be open in order for this function to succeed.

    @param win
    The window to query.

    @param x
    A pointer to the variable where the x-coordinate of the window's
    origin will be stored.

    @param y
    A pointer to the variable where the y-coordinate of the window's
    origin will be stored.

    @param w
    A pointer to the variable where the width of the window will be
    stored.

    @param h
    A pointer to the variable where the height of the window will be
    stored.
*/
void veRenderGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h);

/** function veRenderReloadView
    Rebuilds the wall-view structure and loads the default projection
    and viewing matrices.  The eye and eye_frame members of the wall-view
    structure must already be defined.  This function primarily exists to
    allow the on-the-fly changing of the Z clipping planes.

    @param w
    The window being rendered.

    @param tm
    An estimate of the time at which this window will be displayed.
    
    @param znear
    The near Z clip plane.

    @param zfar
    The far Z clip plane.

    @param wv
    A pointer to the wall-view structure.  The "eye" and "eye_frame"
    members will be used to regenerate the remaining fields (which
    may be overwritten).
 */
void veRenderReloadView(VeWindow *w, long tm, float znear, float zfar,
			VeWallView *wv);

/** section Renderer Driver Interfaces
    The calls above are the calls that applications should make.
    The renderer drivers should provide functions with the same prototypes,
    but with the following names:
    <ul>
    <li><code>veRImplInit()</code>
    <li><code>veRImplOpenWindow()</code>
    <li><code>veRImplCloseWindow()</code>
    <li><code>veRImplWindow()</code>
    <li><code>veRImplSwap()</code>
    <li><code>veRImplGetWinInfo()</code>
    <li><code>veRImplReloadView()</code>
    </ul>
 */

/** section Rendering Callbacks

As of VE 2.2 basic rendering callbacks are standard across all
implementations.  The following interfaces do not need to be
provided by the rendering implementation.  They are managed as
part of the library.  See the notes on the <code>veRenderCall*()</code>
functions below for more information.

 */

typedef void (*VeRenderSetupProc)(VeWindow *);
typedef void (*VeRenderProc)(VeWindow *, long, VeWallView *);
typedef void (*VeRenderPreProc)(void);
typedef void (*VeRenderPostProc)(void);

/** function veRenderSetupCback
    Applications should set a window setup callback.  This callback will
    be called once for each window after it has been created.  When
    called, the window's rendering context will be active.  This callback
    should be used for initial setup of the rendering environment, loading
    textures (if known ahead of time).  The callback has the following type:
    <blockquote><code>void (*VeRenderSetupProc)(VeWindow *)</code></blockquote>
    The single argument is a pointer to the window structure representing
    the window being initialized.

    @param cback
    The callback to set.  Only one window callback exists at a time.
    If this value is <code>NULL</code> then any existing callback is
    removed.
*/
void veRenderSetupCback(VeRenderSetupProc cback);

/** function veRenderCback
    Every application should create a rendering callback.  This is the function
    that is called once per window per frame to actually render data to that
    window.  On multiple display systems, this function may be called by
    multiple threads at the same time.  If you cannot make your rendering
    function thread-safe, be sure to lock it as a critical section
    appropriately (see the <code>ve_thread.h</code> module).  The display
    callback has the following type:
    <blockquote><code>void (*VeRenderProc)(VeWindow *w, long tm, VeWallView *wv)</code></blockquote>
    The arguments are:
    <ul>
    <li><i>w</i> - a pointer to the window structure for the window being
    rendered.
    <li><i>tm</i> - a timestamp (relative to the clock) for the time that
    this frame is expected to be rendered at.  This is an estimate.
    <li><i>wv</i> - a pointer to the wall view structure indicating the
    geometry for this window.
    </ul>
    <p>Rendering functions should not initialize projection or view
    matricies (the VE library handles that) but may multiply the view
    matrix if necessary for rendering.  Proper matrix stack handling
    should be observed (i.e. appropriate push/pop call nesting).
    
    @param cback
    The display callback to set.
 */
void veRenderCback(VeRenderProc cback);

/** function veRenderPreCback
    An application can set a function to be called immediately before
    all windows are displayed.  Unlike the display callback itself, this
    function is only called once per frame (rather than once per thread).
    See also <code>veRenderPostCback()</code>.  The callback must
    have the following type:
    <blockquote><code>typedef void (*VeRenderPreProc)(void);</code></blockquote>

    @param cback
    The new callback to set.  If this value is <code>NULL</code> then
    any existing callback is cleared.
 */
void veRenderPreCback(VeRenderPreProc cback);

/** function veRenderPostCback
    An application can set a function to be called immediately after
    all windows are displayed.  Unlike the display callback itself, this
    function is only called once per frame (rather than once per thread).
    See also <code>veRenderPreCback()</code>.  The callback must
    have the following type:
    <blockquote><code>typedef void (*VeRenderPostProc)(void);</code></blockquote>

    @param cback
    The new callback to set.  If this value is <code>NULL</code> then
    any existing callback is cleared.
 */
void veRenderPostCback(VeRenderPostProc cback);


/** subsection Calling Callbacks
    Applications do not need to worry about calling rendering callbacks.
    The following functions are provided for rendering modules to call
    callbacks.
 */

/** function veRenderCallSetupCback
 */
void veRenderCallSetupCback(VeWindow *w);

/** function veRenderCallCback
 */
void veRenderCallCback(VeWindow *w, long tm, VeWallView *wv);

/** function veRenderCallPreCback
 */
void veRenderCallPreCback(void);

/** function veRenderCallPostCback
 */
void veRenderCallPreCback(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_RENDER_H */
