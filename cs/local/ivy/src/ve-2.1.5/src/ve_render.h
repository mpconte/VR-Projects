#ifndef VE_RENDER_H
#define VE_RENDER_H

#include <ve_env.h>

/** misc
    The ve_render module defines those interfaces that a renderer
    implementation must provide.
*/

/* What the renderer needs to provide */
/** function veRenderInit
    Provided by renderer.  Initializes the renderer.  This should not
    open windows or outputs.
 */
void veRenderInit();   /* initialize the subsystem */

/** function veRenderAllocMP
    Gives the renderer a chance to allocate nodes/processes/threads to
    otherwise unallocated windows in the environment.  This function is
    not usually called by programs - it is an internal callback which the
    MP interface uses.
*/
void veRenderAllocMP();

/** function veRenderRun
    Signals the renderer to create windows and outputs based upon the current
    environment (as determined by <code>veGetEnv()</code>.
*/
void veRenderRun();    /* create windows/graphics outputs based upon current
			  environment */
/** function veRender
    Render a frame based upon the current orientation information.
    Most renderers will use an implementation-dependent callback to
    get the application to provide the actual graphic details.
*/
void veRender();       /* render a frame */
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

#endif /* VE_RENDER_H */
