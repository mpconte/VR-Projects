#ifndef VE_ENV_H
#define VE_ENV_H

#include <ve_version.h>

/* NOTE: udata fields in structures exist largely for platform-dependent
   implementations to store extra data in rather than having to mirror
   this tree - e.g. an OpenGL/GLUT implementation could store window
   information in udata */

/** misc
  The ve_env module describes the structures for the envrionment.
  These structures describe the real-world orientation and 
  configuration of displays (walls) as well as per-user data.
*/

#include <stdio.h>

#include <ve_math.h> /* for VeVector3 */

#ifdef __cplusplus
extern "C" {
#endif

  /** struct VeFrame
    <p>A frame is a way of viewing a space, specifying both the
    viewer's position and orientation.
    */
typedef struct ve_frame {
  /** member loc
    Represents the primary location in the world, also the 
    origin of the space.
    */
  VeVector3 loc;
  /** member dir
    A vector parallel to the negative z-axis of the space.
    In viewer terms, this is a point along the line of sight.
    Programs should not assume that this vector is normalized.
    */
  VeVector3 dir;
  /** member up
    A vector (i.e. independent of Loc and Dir) which is parallel
    to the y-axis and which points in the direction of increasing y.
    In viewer terms, this is the "up" direction.  Note that this vector
    must be perpendicular to the <i>dir</i> vector.
    */
  VeVector3 up;    /* a vector in the direction of up */
} VeFrame;

/* structures and functions for envrionment descriptions */
/** struct VeEnv
  The VeEnv structure contains the description for an environment,
  including the geometry of the walls, and any rendering options.
  **/
typedef struct ve_env {
  /** member name
    The name of the environment.  This value is always defined.
    If it is not specified in the environment file, then a name will be
    automatically generated for it.
    */
  char *name;
  /** member desc
    A string describing the environment.  This value may be NULL if no
    description has been defined.
    */
  char *desc;
  /** member nwalls
    The number of walls in the environment.  This is provided as a convenience.
    */
  int nwalls;
  /** member walls
    A singly-linked list of all walls in the environment.
    */
  struct ve_wall *walls;
  /** member options
    A singly-linked list of all environment-wide options.  Although many
    objects can support option lists, these options are assumed to be
    either rendering-specific - i.e. they are global defaults for 
    VeWindow options - or application-specific, in that they are 
    interpreted by the application.
    */
  struct ve_option *options;
  /** member udata
    An implementation-specific slot which may (or may not) be used to
    store extra data along with the environment structure.  Applications
    should not use the udata member at all unless explicitly advised to
    by an implementation.
    */
  void *udata;
} VeEnv;

/** struct VeOption
  An arbitrary option.  This is a name-value pair which is deliberately
  defined in a simple (if potentially inefficient) way.  It is used for
  implementation- and application-specific features and attributes for
  which it is convenient to be able to specify them in the environment
  file or through other general mechanisms (e.g. the -ve_opt command-line
  option).
  */
typedef struct ve_option {
  /** member name
    A string representing the name of the option.
    */
  char *name;
  /** member value
    The value of the option represented as a string.  Interpretation is
    left up to the implementation or application.
    */
  char *value;
  /** member next
    The linked-list pointer to the next item in the list.  NULL indicates
    the last item of the list.
    */
  struct ve_option *next;
} VeOption;

#define VE_REL_ORIGIN (0) /* object is relative to the origin */
#define VE_REL_EYE    (1) /* object is relative to the eye */

  /* for backwards compatibility */
#define VE_WALL_ORIGIN  (VE_REL_ORIGIN)
#define VE_WALL_EYE     (VE_REL_EYE)

/** struct VeWall
  A wall is a virtual construct inside the physical environment.  A
  wall is a virtual screen on which the world will be rendered.  A
  wall can have any number of outputs (VeWindow objects), one of which
  is usually a physical screen in the environment in the exact location
  of the virtual screen.
  */
typedef struct ve_wall {
  /** member name
     The name of the wall.
     */
  char *name;
  /** member desc
    A string describing the wall.  This value may be NULL if no
    description has been defined.
    */
  char *desc;
  /** member view
    A pointer to the VeView object which describes the geometry of this
    wall.
    */
  struct ve_view *view;
  /** member windows
    A linked-list of windows which are outputs of this wall.  This
    value may be zero if this wall has no windows.
    */
  struct ve_window *windows;
  /** member options
    A linked-list of rendering options that are specific to this wall.
    These will be applied to any windows which are outputs of this wall
    and will override any options specified in the environment object's
    option list.
    */
  struct ve_option *options;
  /** member udata
    The udata member is a hook for implementations to attach data to
    a wall object.  Applications should neither modify nor inspect this
    value.
    */
  void *udata;
  /** member next
    The linked-list pointer to the next item in the list.  NULL indicates
    the last item of the list.
    */
  struct ve_wall *next;
  /** member env
    A pointer to the environment object to which this wall belongs.
    */
  struct ve_env *env;
} VeWall;

/** struct VeView
  The view structure defines the geometry of a wall.  Currently a view
  is limited to a rectangular planar region, although this may be expanded
  in the future.
  */
typedef struct ve_view {
  /** member frame
    Defines the position and orientation of the wall.
    The <code>frame.loc</code> point represents the centre of the
    wall.  The <code>frame.dir</code> vector is the normal of the wall.
    The <code>frame.up</code>
    vector represents the vertical axis of the wall.  The horizontal axis
    is the cross product of the normal and the vertical axis.
    */
  VeFrame frame;
  /** member width
    The dimension of the wall along its horizontal axis.
    */
  float width;  /* width and height of view in world units */
  /** member height
    The dimension of the wall along its vertical axis.
    */
  float height;
  /** member wall
    A pointer to the wall to which this view object belongs.
    */
  struct ve_wall *wall;
  /** member base
      What this view is relative to.  Currently this is either
      <code>VE_REL_ORIGIN</code> to indicate that the frame of
      the wall is relative to the current origin of the system,
      or <code>VE_REL_EYE</code> to indicate that the frame of
      the wall is relative to the default eye of the system.
      Typically a wall in a cave has a base of <code>VE_REL_ORIGIN</code>
      and a head-mounted display has a base of <code>VE_REL_EYE</code>.
      The default is <code>VE_REL_ORIGIN</code>.
  */
  int base;
} VeView;

#define VE_WIN_MONO   0
#define VE_WIN_STEREO 1
#define VE_WIN_LEFT   2
#define VE_WIN_RIGHT  3

/** struct VeWindow
  An output of a VeWall object.  Each VeWindow object will correspond to
  an actual graphics window on a graphics pipe.
  <p>Typically windows render the image defined by the virtual geometry
  of the wall with which they are associated (as described in the VeView
  object).  However, there are many cases where it would be impractical
  to match the location and size of a window precisely with the physical
  wall.  To this end, a window provides a number of calibration values
  that are applied to the values in the VeView structure to help align
  the output image properly.  A program is included with the
  library (the "calibrate" example) which allows the user to fiddle
  with these values in real-time and to write out the corrected
  environment file.
  */
typedef struct ve_window {
  /** member name
    The name of the window.
    */
  char *name;
  /** member display
    The pipe where this window will be displayed.  The actual interpretation
    of this string is implementation-dependent.  Under an X-based
    implementation, this value is typically the name of the X console.
    A value of "default" is always considered to be valid when a
    reasonable default can be deduced.
    */
  char *display;
  /** member geometry
    The size and location of the window on its display expressed in
    X geometry string notation.  i.e. 
    "<code><i>width</i>x<i>height</i>+<i>xoffset</i>+<i>yoffset</i>".
    */
  char *geometry;
  /** member width_err
    This is the calibration value for the width of the image.  This value is
    added to the width member of the corresponding VeView structure
    and the resulting value is used as the width of the generated image.
    */
  float width_err;
  /** member height_err
    This is the calibration value for the height of the image.  This value is
    added to the height member of the corresponding VeView structure
    and the resulting value is used as the height of the generated image.
    */
  float height_err; /* width/height calibration */
  /** member offset_x
    This calibration value is the offset of the physical centre of the wall
    from the centre of the generated image along the horizontal axis.
    */
  float offset_x;
  /** member offset_y
    This calibration value is the offset of the physical centre of the wall
    from the centre of the generated image along the vertical axis.
    */
  float offset_y;    /* centre calibration */
  /** member id
    A numerical id for the window.  If the environment is constructed
    from a file, then a unique id (within the environment) is generated
    for each window.  If you create window structures by hand you should
    allocate an id with a call to <code>veWindowMakeID()</code>
    */
  int id;
  /** member slave
      A set of three strings identifying what multi-processing slave
      should be used for this window.  A value of <code>NULL</code> or
      the string <code>"auto"</code> both mean the same thing:  that
      the specifics of the value should be decided by the system at
      run-time.
      
      @member node
      The name of the node on which rendering for this window should be
      performed.

      @member process
      The name of the process in which this window should be rendered.
      This is not a number but a name - windows with the same name
      for their process field will be rendered in the same process.
      The name "unique" is reserved to indicate that a unique process
      should be allocated for this window and that it should not be
      shared with other windows.
      
      @member thread
      The name of the thread in which this window should be rendered.
      This is not a number but a name - windows with the same name for
      their thread field will be rendered in the same thread.
      The name "unique" is reserved to indicate that a unique thread
      should be allocated for this window and that it should not be
      shared with other windows.

   */
  struct {
    char *node;
    char *process;
    char *thread;
  } slave;
  /** member udata
    The udata member is provided as a hook for library implementations to
    store window-specific data.  Applications should neither modify nor
    inspect this value.
    */
  void *udata;
  /** member appdata
    The appdata member is a hook for applications to store per-window
    information.  Applications are free to set this value as they
    wish.  The library will preserve this value but will not modify it
    nor act on it in any way.
    */
  void *appdata;
  /** member options
    A linked-list of window-specific rendering options.  These override
    any options set in the wall or environment objects for this window.
    */
  struct ve_option *options;
  /** member next
    The linked-list pointer to the next item in the list.  NULL indicates
    the last item of the list.
    */
  struct ve_window *next;
  /** member wall
    A pointer to the wall object to which this window belongs.
    */
  struct ve_wall *wall;
  /** member eye
      Which eye displacement should be used when this window is
      rendered.  The following values are recognized:
      <dl>
      <dt><b>VE_WIN_MONO</b></dt>
      <dd>Render the window with no eye displacement.  This is the
      default.</dd>
      <dt><b>VE_WIN_LEFT</b></dt>
      <dd>Render the window with displacement for the left eye.</dd>
      <dt><b>VE_WIN_RIGHT</b></dt>
      <dd>Render the window with displacement for the right eye.</dd>
      <dt><b>VE_WIN_STEREO</b></dt>
      <dd>Where supported, render a stereoscopic image.  Platforms
      that do not support this option will emit a warning when the
      problem is first encountered and then treat this window as
      though it were labelled <b>VE_WIN_MONO</b></dd>
      </dl>
  */
  int eye;
  /** member distort
      A matrix which is an affine distortion compensation matrix for this
      window.  All geometry sent to this window will be multiplied by this
      matrix at the end.  Externally, only the 3x3 upper-left of this matrix
      is represented (affine 2D transform).
   */
  VeMatrix4 distort;
  /** member viewport
      The actual rectangle of the window to render to.  Can be used to effectively
      "blank" a signal in software.  Elements [0] and [1] of the area are the (x,y)
      coordinates of the first corner of the rectangle.  Elements [2] and [3] are the
      width and height.  If all values are 0 then the full window is used.
  */
  int viewport[4];
  /**  member async
       If non-zero, then this window should be run asynchronously
       if possible
       (i.e. we don't care if we stay in sync with it).  Only a
       slave for which all windows are marked "async" will be run
       asynchronously.
  */
  int async;
} VeWindow;


/** struct VeProfileDatum
  A value for a user profile.  The format of the data is implementation-
  dependent.
  */
typedef struct ve_profiledatum {
  /** member name
    The name of the datum.
    */
  char *name;
  /** member value
    The value of the datum expressed as a string.
    */
  char *value;
  /** member next
    The linked-list pointer to the next item in the list.  NULL indicates
    the last item of the list.
    */
  struct ve_profiledatum *next;
} VeProfileDatum;

/** struct VeProfileModule
  A collection of related user profile data.  These collections 
  conceptually correspond to a specific "module" or "driver" but the
  term is used loosely here.  Although it is suggested that profile
  data be grouped in this way there is nothing that enforces this;
  this concept of module is only used internally and need not match
  any other module concept in the library.
  */
typedef struct ve_profilemodule {
  /** member name
    The name of the module for which profile data is stored here.
    */
  char *name;
  /** member data
    A linked-list of datum objects containing the profile data.
    */
  VeProfileDatum *data;
  /** member next
    The linked-list pointer to the next item in the list.  NULL indicates
    the last item of the list.
    */
  struct ve_profilemodule *next;
} VeProfileModule;

/** struct VeProfile
  A user profile.  This structure contains simulation settings that
  are specific to a particular user.
  */
typedef struct ve_profile {
  /** member name
    A short name for the user following name token conventions.
    */
  char *name;
  /** member fullname
    Optionally, the user's full name.  If not specified this field is
    NULL. */
  char *fullname;
  /** member stereo_eyedist
    The distance between the eyes (in environment units - typically metres).
    This value is only used if images are being rendered in stereo.
    */
  float stereo_eyedist;
  /** member modules
    A linked-list of module-specific profile data.  This allows the
    user profile to be extended to cover data not previously thought of.
    */
  VeProfileModule *modules;
} VeProfile;

/** function veEnvWrite
  Writes an environment structure to a stream as a data file.
  This file will be a valid BlueScript file that can be processed
  using VE's BlueScript interpreter.  see the 
  <a href="ve_blue.h.html">ve_blue</a> module for more details on
  the BlueScript interpreter.

  @param env
  The VeEnv object to write to the stream.  All objects in the
  VeEnv structure and its children will be written to the file.
  @param stream
  The stream to which data is written.

  @returns
  Zero if successful, and a non-zero value if an error was encountered.
  Errors are reported using the standard VE error functions.
  */
int veEnvWrite(VeEnv *env, FILE *stream);

/** function veProfileWrite
  Writes a user profile to a stream as a data file.
  This file will be a valid BlueScript file that can be processed
  using VE's BlueScript interpreter.  see the 
  <a href="ve_blue.h.html">ve_blue</a> module for more details on
  the BlueScript interpreter.

  @param profile
  A pointer to the profile object which will be written.
  @param stream
  The stream to which data is written.
  */
int veProfileWrite(VeProfile *profile, FILE *stream);

/** function veViewFree
  Deallocates a previously allocated VeView object and any associated
  storage.  After this the object cannot be accessed.
  
  @param view
  The object to be deallocated.
  */
void veViewFree(VeView *view);

/** function veWindowFreeList
  Deallocates a linked-list of VeWindow objects previously allocated and
  any associated storage.  After this, the list and the objects within it 
  cannot be accessed.
  
  @param window
  The head of the list to be deallocated.
  */
void veWindowFreeList(VeWindow *window);

/** function veOptionFreeList
  Deallocates a linked-list of VeOption objects previously allocated and
  any associated storage.  After this, the list and the objects within it
  cannot be accessed.
  
  @param option
  The head of the list to be deallocated.
  */
void veOptionFreeList(VeOption *option);

/** function veWallFreeList
  Deallocates a linked-list of VeWall objects previously allocated and
  any associated storage.  After this, the list and the objects within it
  cannot be accessed.

  @param wall
  The head of the list to be deallocated.
  */
void veWallFreeList(VeWall *wall);

/** function veEnvFree
  Deallocates a VeEnv object and any objects contained there in.
  After this call, the object and any of its children can no longer be
  accessed.

  @param env
  The environment object to deallocate.
  */
void veEnvFree(VeEnv *env);

/** function veProfileFree
  Deallocates a VeProfile object and any objects contained there in.
  After this call, the object and any of its children can no longer be
  accessed.

  @param pro
  The profile object to deallocate.
*/
void veProfileFree(VeProfile *pro);

/** function veFindWall
  Finds the first wall in the environment which matches the given name.
  
  @param env
  The environment to search in.
  @param name
  The wall to search for.

  @returns
  A pointer to the corresponding VeWall object if found, NULL if not found.
 */
VeWall *veFindWall(VeEnv *env, char *name);

/** function veFindWindow
  Finds the first window belonging to any wall in the environment which
  matches the given name.
  
  @param env
  The environment to search in.
  @param name
  The window to search for.

  @returns
  A pointer to the corresponding VeWindow object if found, NULL if not found.
  */
VeWindow *veFindWindow(VeEnv *env, char *name);

/** function veFindWindowInWall
  Finds the first window belonging to a given wall which matches the given
  name.
  
  @param wall
  The wall to search in.
  @param name
  The window to search for.

  @returns
  A pointer to the corresponding VeWindow object if found, NULL if not found.
  */
VeWindow *veFindWindowInWall(VeWall *wall, char *name);

/** function veEnvGetOption
  Finds the value of the given option in the given option list if
  available.

  @param olist
  The head of the list of VeOption objects to search in.
  @param name
  The name of the option to search for.

  @returns
  A pointer to the option's value if found, NULL otherwise.
  Note that the returned value should not be modified at all - if
  you need to work with the value, copy it somewhere else first.
  */
char *veEnvGetOption(VeOption *olist, char *name);

/** function veProfileFindModule
  Finds a collection of module-specific data in a given user profile.

  @param profile
  The user profile to search in.
  @param module
  The name of the module to search for.

  @returns
  A pointer to the corresponding VeProfileModule object if found,
  NULL if not found.
  */
VeProfileModule *veProfileFindModule(VeProfile *profile, char *module);

/** function veProfileFindDatum
  Finds a specific profile value in a module-specific collection of
  profile data.

  @param module
  The profile module to search in.
  @param name
  The name of the value to search for.
  
  @returns
  A pointer to the corresponding VeProfileDatum structure if found,
  NULL if not found.  The returned pointer is live, in that the
  contents of the VeProfileDatum object can be updated and the
  updates also appear in the VeProfileModule object.
  */
VeProfileDatum *veProfileFindDatum(VeProfileModule *, char *);

/** function veProfileSetDatum
  Stores a profile datum in the given profile module.

  @param module
  The module to store the datum in.
  @param name
  The name of the datum.
  @param value
  The value of the datum expressed as a null-terminated string.

  @returns
  Zero on success, non-zero if an error is encountered.  Errors are
  reported using the standard VE error functions.
  */
int veProfileSetDatum(VeProfileModule *module, char *name, char *value);

/** function veWindowMakeID
  Creates a new unique identifier for a window.  This identifier is unique
  within the session (i.e. process).

  @returns
  A unique integer as an identifier.  This identifier should be assigned
  to a VeWindow structure's <code>id</code> member.
 */
int veWindowMakeID(void);

#ifdef __cplusplus
}
#endif

#endif /* VE_ENV_H */
