#ifndef VE_CORE_H
#define VE_CORE_H

/* The core VE routines - these do the work specific to the virtual
   environment */
#include <ve_version.h>
#include <ve_debug.h>
#include <ve_math.h>
#include <ve_env.h>

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

/** misc
    The ve_core module provides both the general math setup for view and
    projection matrices for each wall, as well as a central point for
    referencing the current structures (environment, profile) that describe
    the layout of the current setup.  The current point-of-view is also
    housed in this module.
*/

#define VE_EYE_MONO (-1)
#define VE_EYE_LEFT   0
#define VE_EYE_RIGHT  1

/** struct VeWallView
    Describes the current view and projection values for the
    scene that is currently being rendered.  The structure
    describes one particular view, and is usually associated with
    a particular VeWindow structure.
*/
typedef struct ve_wallview {
  /* where the camera is and where it is looking */
  /** member camera
    The location and orientation of the camera or viewpoint from
    which the scene is viewed.  The direction is typically perpendicular
    to the wall which is being viewed; it may not be looking directly
    at the centre of the wall.
    */
  VeFrame camera;
  /* frustum */
  /** member near far left right top bottom
    These values define the viewing frustum.  The corners are defined
    by left and right (in X), top and bottom (in Y) with all values in
    the near clipping plane (in Z).  The far value indicates the far 
    clipping plane.
    */
  float near, far, left, right, top, bottom;
  /* the matrices that are generated */
  /** member view
    The view matrix.  This is the transformation from world co-ordinates to
    the camera's frame.
    */
  VeMatrix4 view;
  /** member iview
    The inverse view matrix.  This is the transformation from the camera's
    frame to world co-ordinates.  As implied by its name, it is the inverse
    of the view member.
    */
  VeMatrix4 iview;
  /** member proj
    The projection matrix.  This matrix converts co-ordinates in the camera's
    frame to screen co-ordinates.
    */
  VeMatrix4 proj;
  /** member eye
      Which eye is being rendered by this wall.  This value is either
      <code>VE_EYE_LEFT</code>, <code>VE_EYE_RIGHT</code>, or 
      <code>VE_EYE_MONO</code> in non-stereoscopic situations.
  */
  int eye;
  /** member eye_frame
      Which eye frame (right now there is only one eye frame - 
      the "default eye") was used to generate this view.
  */
  VeFrame eye_frame;
} VeWallView;

/** function veCoreInit
    Initializes the ve_core module.  This is generally called from 
    <code>veInit()</code>.  Applications which use <code>veInit()</code>
    to initialize the library do not need to call this function.
 */
void veCoreInit();


/** function veGetOrigin
  Returns a pointer to the frame for the origin of the space.  This pointer
  is live - the origin itself can be modified by modifying the structured
  pointed to by the result.
  
  @returns
  A pointer to the origin's VeFrame structure.  Applications can safely
  assume that this function always returns a valid pointer.
  */
VeFrame *veGetOrigin();

/** function veSetOrigin
  Replaces the current origin of the environment with the data pointed to
  by <i>frame</i>.  After calling this function, the caller need not
  preserve the data pointed to by <i>frame</i> - the library makes a copy
  of the data and does not preserve any reference to <i>frame</i>.

  @param frame
  A pointer to the frame data to replace the origin with.
  */
void veSetOrigin(VeFrame *frame);

/** function veGetDefaultEye
  Returns a poitner to the frame for the default eye of the space.
  The default eye corresponds to the main user's location and orientation
  in the physical space of the environment.  The frame is defined in
  coordinates relative to the origin of the space (see veGetOrigin).
  <p>This pointer
  is live - the eye itself can be modified by modifying the structured
  pointed to by the result.
    
  @returns
  A pointer to the default eye's VeFrame structure.  Applications can
  safely assume that this function always returns a valid pointer.
 */
VeFrame *veGetDefaultEye();

/** function veSetDefaultEye
  Replaces the current default eye of the environment with the data
  pointed to by <i>frame</i>.  After calling this function, the caller
  need not preserve the data pointed to by <i>frame</i> - the library
  makes a copy of the data and does not preserve any reference to
  <i>frame</i>.

  @param frame
  A pointer to the frame data to replace the default eye with.
 */
void veSetDefaultEye(VeFrame *frame);

/** function veSetOrigin
  Replaces the current origin of the environment with the data pointed to
  by <i>frame</i>.  After calling this function, the caller need not
  preserve the data pointed to by <i>frame</i> - the library makes a copy
  of the data and does not preserve any reference to <i>frame</i>.

  @param frame
  A pointer to the frame data to replace the origin with.
  */
void veSetOrigin(VeFrame *frame);

/** function veSetEnv
  Sets the active environment.  This is the environment object that will
  be used for all other core functionality.  There can only be a
  single active environment at a time.  This object should not be
  deallocated until another object has been set as the active environment.

  @param env
  A pointer to the environment object to be used.
 */
void veSetEnv(VeEnv *env);

/** function veGetEnv
  Retrieves the current active environment.  This only returns a
  reference to the environment - it does not change the current environment
  at all.

  @returns 
  A pointer to the current active environment.  This function may
  return NULL if no environment has been setup.
 */
VeEnv *veGetEnv();

/** function veSetProfile
  Sets the active user profile.  This is the profile object that will
  be used by the system when it requires user-specific parameters while
  working.  This object should not be deallocated until another object
  has been set as the active profile.

  @param prof
  A pointer to the user profile object to be used.
 */
void veSetProfile(VeProfile *prof);

/** function veGetProfile
  Retrieves the current active user profile.  This only returns a
  reference to the profile - it does not change the current environment
  at all.
  
  @returns
  A pointer to the current active user profile.  This function may return
  NULL if no profile has been setup.
 */
VeProfile *veGetProfile();

/** function veCalcStereoEye
  Given the frame for an eye and a user profile, builds a new frame
  representing either the left or right eye.

  @param eye_in
  The frame representing the centre point between the eyes.
  
  @param out
  The generated frame.  This should not be the same structure as referenced
  by <i>eye_in</i>.

  @param pro
  The user profile to use for user-specific values.  Right now, only the
  <i>eyedist</i> value of the <i>stereo</i> compononent of the user profile
  is used.  This value is the separation between the user's eyes.

  @param which
  Which eye to generate the view for.  Should be one of two constants:
  <code>VE_EYE_LEFT</code> or <code>VE_EYE_RIGHT</code>.
 */
void veCalcStereoEye(VeFrame *eye_in, VeFrame *out,
		     VeProfile *pro, int which);

/** function veGetUnmapMatrix
    Constructs a matrix which converts from a frame's space to world space.
    
    @param m
    A pointer to the matrix where the result will be stored.
    
    @param f
    The frame from whose space we are converting.
 */
void veGetUnmapMatrix(VeMatrix4 *m, VeFrame *f);

/** function veGetMapMatrix
    Constructs a matrix which converts from world space to a frame's
    space.

    @param m
    A pointer to the matrix where the result will be stored.
    
    @param f
    The frame to whose space we are converting.
 */
void veGetMapMatrix(VeMatrix4 *m, VeFrame *f);

/** function veGetWindowView
    Given a window and a frame (representing an eye or camera) build the
    corresponding wall-view structure containing the related matrices.

  @param w
  The window for which to build the matrices.

  @param default_eye
  The user's default eye (needed for windows on walls with a base of "eye").
  If <code>NULL</code>, then the system's default eye is used. 
  
  @param eye
  The camera's position and orientation.  If <code>NULL</code>, then the
  the value of <i>default_eye</i> is used.
  
  @param wv
  The structure in which the matrices will be returned.
 */
void veGetWindowView(VeWindow *w, VeFrame *default_eye, 
		     VeFrame *eye, VeWallView *wv);

/** function veGetWindowClipView
    Given a window and a frame (representing an eye or camera) build the
    corresponding wall-view structure containing the related matrices.
    This varies from <code>veGetWindowView()</code> in that it also allows
    you to specify the Z clipping planes.

  @param w
  The window for which to build the matrices.

  @param default_eye
  The user's default eye (needed for windows on walls with a base of "eye").
  If <code>NULL</code>, then the system's default eye is used. 
  
  @param eye
  The camera's position and orientation.  If <code>NULL</code>, then the
  the value of <i>default_eye</i> is used.
  
  @param znear
  The value of the near Z clipping plane.

  @param zfar
  The value of the far Z clipping plane.

  @param wv
  The structure in which the matrices will be returned.

 */
void veGetWindowClipView(VeWindow *w, VeFrame *default_eye, 
			 VeFrame *eye, float znear, float zfar,
			 VeWallView *wv);

/** function veSetZClip
    Sets the global Z clip planes.  These are used for generating
    projection matrices.  There is currently only one set of Z clip planes
    for all windows.
  
  @param near
  The new value for the near Z clip plane.

  @param far
  The new value for the far Z clip plane.
 */
void veSetZClip(float near, float far);

/** function veGetZClip
    Retrieve the current value of the global Z clip planes.

    @param near_r
    A pointer to the variable where the value of the near Z clip
    plane will be stored.

    @param far_r
    A pointer to the variable where the value of the far Z clip
    plane will be stored.
*/
void veGetZClip(float *near_r, float *far_r);

/** function veFrameIdentity
    Fills in the given frame with our base reference.  Our base is a
    frame, positioned at the origin (i.e. <code>(0,0,0)</code>) looking
    along the negative Z-axis (specifically looking at point 
    <code>(0,0,-1)</code>) with up pointing along the positive Y-axis
    (i.e. along <code>(0,1,0)</code>).

  @param frame
  The structure in which to return the generated frame.
 */
void veFrameIdentity(VeFrame *frame);

/** function veMultFrame
    Multiplies a frame through an arbitrary matrix.  Given matrix <i>M</i>
    and frame <i>F</i>, the resulting frame <i>F<sup>'</sup></i> would be:
    <blockquote><i>F<sup>'</sup></i> = <i>MF</i></blockquote>
    
    @param m
    The matrix to multiply the frame by.

    @param f
    The frame to multiply.
    
    @param fp
    A pointer to the frame where the resulting value is stored.
*/
void veMultFrame(VeMatrix4 *m, VeFrame *f, VeFrame *fp);

/** function veMapFrame
    Given one frame of reference, converts a second frame 
    from the global co-ordinate system to the one
    represented by the frame of reference
    (i.e. where the origin is represented by the frame of reference's
     origin, and the bases are represented by the frame of reference's
     orientation).  The global co-ordinate system is the one with
     the origin at (0,0,0) and without rotation.
     <p>This function is effectively the inverse of <code>veMapFrame()</code>.

  @param ref
  The frame of reference.
  
  @param f
  The frame to convert.
  
  @param fp
  The structure which will be filled in with the converted frame.
 */
void veMapFrame(VeFrame *ref, VeFrame *f, VeFrame *fp);

/** function veUnmapFrame
    Given one frame of reference, converts a second frame 
    from the co-ordinate system represented by the frame of reference
    (i.e. where the origin is represented by the frame of reference's
     origin, and the bases are represented by the frame of reference's
     orientation) to the global co-ordinate system (i.e. the one in
     which the values of the original frame of reference are expressed).
     <p>This function is effectively the inverse of <code>veMapFrame()</code>.

  @param ref
  The frame of reference.
  
  @param f
  The frame to convert.
  
  @param fp
  The structure which will be filled in with the converted frame.
 */
void veUnmapFrame(VeFrame *ref, VeFrame *f, VeFrame *fp);


/** function veLockCallbacks
    Prevents callbacks from being processed until unlocked.  It is used
    internally to ensure that only one callback is processed at a time.
    Applications can use this function to prevent callbacks from being
    called.  Note that it should not be called from a callback itself -
    this will cause the library to terminate the application to avoid
    a deadlock.  Applications should also be careful that while they
    have callbacks locked out that they do not wait for action from
    a callback.
    <p>Generally this function is not required by applications, but may
    be required by some drivers and extensions to the library.  The
    internal modules of the library use this to ensure that only one
    thread is ever running in the application code (except for display
    callbacks which may be multi-threaded).
*/
void veLockCallbacks(void);
/** function veDeviceUnlockCallbacks
    Removes a previous lock on callbacks.  This should only be called if
    callbacks were previously locked out.  See 
    <code>veLockCallbacks()</code>.
*/
void veUnlockCallbacks(void);
/** function veCallbacksAreLocked
    A predicate function - checks to see if there is currently a lock
    on callbacks.
    
    @returns
    1 if callbacks are currently locked by any thread, and 0 if
    callbacks are not currently locked by any thread.
*/
int veCallbacksAreLocked(void);

/** misc
    The <code>veLockFrame()</code>, <code>veUnlockFrame()</code>,
    <code>veLockFrameExcl()</code>, <code>veUnlockFrameExcl()</code>,
    implement a task-specific synchronization device designed to allow all
    event callbacks which are ready to run, to run before the renderer
    starts drawing the next frame.  At the same time, it prevents the
    handlers which call these callbacks from starving the renderer.  Once
    the renderer decides to render a frame, only callbacks that try to lock
    the frame before it will get a chance to run before it renders.
*/

/** function veLockFrame
    Gets a non-exclusive lock on the current frame.  Threads which call
    callbacks should call this before locking off callbacks.  See the
    discussion above.
 */
void veLockFrame(void);

/** function veUnlockFrame
    Releases a non-exclusive lock on the current frame.  Threads which
    call callbacks should call this after locking off callbacks.  See the
    discussion above.
*/
void veUnlockFrame(void);

/** function veLockFrameExcl
    Gets an exclusive lock on the current frame.  This should only be called
    by the renderer thread.  If you are unsure, don't call this.
    See the discussion above.
 */
void veLockFrameExcl(void);

/** function veUnlockFrameExcl
    Releases a exclusive lock on the current frame.  This should only be
    called by the renderer thread.  If you are unsure, don't call this.
    See the discussion above.
*/
void veUnlockFrameExcl(void);

#ifdef __cplusplus
}
#endif

#endif /* VE_CORE_H */
