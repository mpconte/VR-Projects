#ifndef VE_MAIN_H
#define VE_MAIN_H

#include <ve_version.h>
#include <ve_math.h>
#include <ve_env.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if 0
}
#endif

typedef void (*VeIdleProc)(void);

/** misc
    The ve_main module defines the functions and structures that bring
    the whole library together.
*/

/** function veInit
  The main VE initialization function.  All VE applications must call
  this function at the beginning to initialize the library.  This
  library has a number of side effects.  In particular, it will
  extract VE-relevant command-line options from the given argc/argv
  pair.  Along with <code>veRun()</code> it is part of the basic common
  VE program structure.  For example:
<p>
<pre>
        int main(int argc, char **argv) 
        {
            veInit(&amp;argc,argv);
            veSetOption(...);
            veDeviceAddCallback(...);
            <i>... initialize program and implementation specifics ...</i>
            veRun();
        }
</pre>
  
  <p>After it finishes parsing VE-specific command-line arguments,
  it removes them from the command-line.  It will stop parsing as
  soon as it sees an argument it doesn't understand, or "--".

  @param argc
  A pointer to the argc value passed to the main() function.

  @param argv
  The argv array of command-line arguments.
  
  @returns
  Zero if successful, a non-zero value if an error is encountered.
  Errors are reported using the standard VE error reporting routines.
  */
void veInit(int *argc, char **argv);

/** function veExit
    Attempts to shutdown the VE system cleanly and exit the program.
    It is recommended that this function be called instead of
    <code>exit()</code> to finish execution of the program.
    <p>The "exit" directive in filters calls this function.
 */
void veExit(void);

/** function veSetOption
    Sets run-time options.  Run-time options can be set either from the
    command-line, the environment or via this call.  The following options
    are recognized, along with their command-line and environment variable
    equivalents:
<p align="center">
<table border="2" cellpadding="2">
<tr><th><b>Name</b></th><th><b>Command-Line</b></th><th><b>Environment Variable</b></th><th><b>Purpose</b></th></tr>
<tr><td>env</td><td>-ve_env</td><td>VEENV</td><td>The environment file to load.</td></tr>
<tr><td>user</td><td>-ve_user</td><td>VEUSER</td><td>The user profile to load.</td></tr>
<tr><td>debug</td><td>-ve_debug</td><td>VEDEBUG</td><td>The debug level (as an integer).  A level of 0 means no debugging output.</td></tr>
<tr><td>manifest</td><td>-ve_manifest</td><td>VEMANIFEST</td><td>The device manifest to load.</td></tr>
<tr><td>devices</td><td>-ve_devices</td><td>VEDEVICES</td><td>The device usage file to load.</td></tr>
<tr><td>root</td><td>-ve_root</td><td>VEROOT</td><td>Path to the VE installation.  Used to located device driver files.</td></tr>
</table>
</p>   

    @param name
    The name of the option to set.

    @param value
    The new value of the option.  This is just treated as a string.
    A copy of this value is stored.  After returning, this string can
    be modified or freed.
 */
void veSetOption(char *name, char *value);

/** function veGetOption
    Retrieves the current value of a run-time option.

    @param name
    The name of the option to return.

    @returns
    A string containing the value of the option.  This string should not
    be modified or freed.
 */
char *veGetOption(char *name);

/** function veGetWinOption
    Retrieves the current value of a window-specific option.
    A window-specific option is searched for in the following locations,
    in the given order:
    <ul>
    <li>The given <code>VeWindow</code> structure (specific option for a window)</li>
    <li>The <code>VeGetOption()</code> call (defaults for program)</li>
    <li>Options set for the corresponding <code>VeWall</code> structure
        (defaults for wall)
    </li>
    <li>Options set for the corresponding <code>VeEnv</code> structure
        (defaults for environment)</li>
    </ul>
    The first matching option is returned.  If no option matches then
    <code>NULL</code> is returned.

    @param w
    The window for which to query an option.

    @param name
    The name of the option being queried.

    @returns
    The string containing the option value if successful, or 
    <code>NULL</code> if the option cannot be found.  Any returned
    string should be treated as "read-only".
 */
char *veGetWinOption(VeWindow *w, char *name);

/** function veParseGeom
    Parses a "geometry" string which describes the width and height and
    optionally the location of a window.  The string must have one of 
    two formats: <code><i>w</i>x<i>h</i></code> or <code><i>w</i>x<i>h</i>[+-]<i>x</i>[+-]<i>y</i></code>.  In the former case, x and y are set to 0.
    <p>If there is a parsing error, a fatal error is generated.</p>

    @param geom
    The geometry string to parse.

    @param w,h,x,y
    Pointers to the variables where the geometry values will be stored.
    All pointers must be non-NULL.
*/
void veParseGeom(char *geom, int *x, int *y, int *w, int *h);

/** function veSetIdleProc
    When no rendering needs to be done, the idle processing callback will
    be continuously called.  The callback should have the following type:
    <blockquote><code>void (*VeIdleProc)(void)</code></blockquote>
    <p>Idle callbacks are generally inefficient and should generally be
    avoided.  Timer callbacks are generally better (see 
    <a href="ve_timer.h.html">ve_timer</a> for more information).

    @param proc
    The idle callback to set.  If <code>NULL</code> then any existing
    idle callback will be cleared.
 */
void veSetIdleProc(VeIdleProc proc);

typedef void (*VeAnimProc)(float t, float dt);

/** function veSetAnimProc
    The animation callback is a callback which is called once per frame
    on the master node.  Both a global time value (relative to the time
    that <code>veRun()</code> was called) and the time interval since the
    last call to the animation procedure (0.0 if this is the first time
    the animation procedure is called) are passed as arguments.  If
    specified the animation callback will be called exactly once per frame.
    Note that specifying an animation callback which posts a redisplay may
    prevent an idle callback from ever being called.  The callback should
    have the following type:
    <blockquote><code>void (*VeAnimProc)(float t, float dt)</code></blockquote>.
    <p>If an animation callback posts a redisplay, the effect is that the
    VE program will run through a animate-render loop as quickly as possible
    without any pause in-between.  This can be useful for achieving the
    fastest possible frame-rate but may cause other threads to be starved.
    Use with care.  On a system with sufficiently fine-grained timers, timers
    are preferred for driving the system.</p>

    @param proc
    The idle callback to set.  If <code>NULL</code> then any existing
    idle callback will be cleared.
 */
void veSetAnimProc(VeAnimProc proc);

/** function vePostRedisplay
    Notifies the rendering threads that the screen needs to be redrawn.
    Normally the screen is not redrawn unless the application indicates
    that the state of the system has changed.
*/
void vePostRedisplay();

/** function veEventLoop
    The event loop starts up input and timer and rendering threads and
    then lets them run.  This function never returns.
 */
void veEventLoop();

/** function veRun
    Sets a VE application in motion.  This function never returns.  It
    starts the rendering threads running, releases any delayed threads
    and then enters the event loop.  See the notes for <code>veInit()</code>
    for more details.
*/
void veRun();

/** section Motion Callbacks
    Introduced in VE 2.2 is the notion of a "motion callback".  This
    is a callback that can be registered by applications so that other
    parts of the system (e.g. input drivers and filters) attempt to move
    either the eye or the origin, the application may override that
    change.  This is most typically used to implement some form of 
    collision detection whereby applications can reject attempts to
    move into occupied spaces.
    <p>A motion callback is called with a proposed frame for either the
    eye or the origin.  The callback is made before the real eye or origin
    is updated, so a comparison may be made to existing values.
    A motion callback must always return either 
    <code>VE_MC_ACCEPT</code> (to indicate that the new position is
    satisfactory) or <code>VE_MC_REJECT</code> (to indicate that the
    new position is unsatisfactory).
    <p>A motion callback has the following prototpye:
    <blockquote><code>int (*VeMotionCback)(int what, VeFrame *f, void *arg);</code></blockquote>
    The "what" parameter will be set to either <code>VE_MC_EYE</code> or
    <code>VE_MC_ORIGIN</code> to indicate which frame you are examining.
    The "f" parameter is a pointer to the proposed value for the given
    frame.  As noted above, the callback must return either
    <code>VE_MC_ACCEPT</code> or <code>VE_MC_REJECT</code>.
    The "arg" parameter is an application-specific value that can be
    set when the callback is set.  The library passes this value to the
    callback when it is called and does not interpret it in any way.
    <p>If no callback is set for a particular frame then it has the
    same effect as a callback that accepts any value passed to it.
 */

#define VE_MC_EYE    (1<<0)
#define VE_MC_ORIGIN (1<<1)
#define VE_MC_ALL    (VE_MC_EYE|VE_MC_ORIGIN);

#define VE_MC_ACCEPT 0
#define VE_MC_REJECT 1

typedef int (*VeMotionCback)(int what, VeFrame *f, void *arg);

/* special cases */
extern VeMotionCback VE_MC_REJECTALL;
extern VeMotionCback VE_MC_ACCEPTALL;


/** function veSetMotionCback
    An application interface used to set a motion callback.
    There may be two separate motion callback interfaces - one
    for the eye and one for the origin.

    @param what
    Indicates which callback you are setting.  Must be one of:
    <ul>
    <li><code>VE_MC_EYE</code> - set the callback for the eye.
    <li><code>VE_MC_ORIGIN</code> - set the callback for the origin.
    <li><code>VE_MC_ALL</code> - set the callback for eye and the origin.
    </ul>

    @param cback
    The new callback to set.  If this is <code>NULL</code> then
    any corresponding callback is removed.  There are two special
    values:
    <ul>
    <li><code>VE_MC_ACCEPTALL</code> - accepts any frame</li>
    <li><code>VE_MC_REJECTALL</code> - rejects any frame</li>
    </ul>
    
    @param arg
    An optional argument that will be passed to the callback when
    called.  The library does not interpret this value at all.  This
    value is meaningless for the special callbacks 
    (<code>VE_MC_ACCEPTALL</code> and <code>VE_MC_REJECTALL</code>)
    and should be set to <code>NULL</code> if it is not required.
 */
void veSetMotionCback(int what, VeMotionCback cback, void *arg);

/** function veCheckMotionCback
    Calls an application's motion callback (if any) for the
    given frame.  This callback is provided primarily for use
    by drivers and virtual devices, but it may be used by
    applications as well.

    @param what
    The type of frame you wish to check (must be either
    <code>VE_MC_EYE</code> or <code>VE_MC_ORIGIN</code>).

    @param f
    A pointer to the frame that you wish to check.  This
    frame will be protected against modifications by the
    application.

    @returns
    Either <code>VE_MC_ACCEPT</code> or <code>VE_MC_REJECT</code>,
    regardless of the application's callback.  If the application's
    callback returns an invalid value then <code>VE_MC_REJECT</code>
    is assumed as a default.  If no callback is set, then 
    <code>VE_MC_ACCEPT</code> is returned as a default.
 */
int veCheckMotionCback(int what, VeFrame *f);

/** function veFindFile
    Searches through known locations for the given file.  If the
    specified file name is absolute, then no searching is done.
    If it is relative then other locations will be searched (if it
    is not present in the current directory).

    @param type
    The type of file to look up.  This will be used to
    find the given file.  Known values are:
    <ul>
    <li><code>"env"</code> - environment files
    <li><code>"profile"</code> - user profiles
    <li><code>"manifest"</code> - device manifests
    <li><code>"devices"</code> - device use files
    <li><code>"driver"</code> - loadable drivers
    </ul>

    @param fname
    The file to search for.
    
    @returns
    A pointer to a stdio file structure which is opened for reading if
    successful, or <code>NULL</code> if no file can be found and opened.
 */
FILE *veFindFile(char *type, char *fname);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_MAIN_H */

