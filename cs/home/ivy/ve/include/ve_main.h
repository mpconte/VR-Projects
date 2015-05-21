#ifndef VE_MAIN_H
#define VE_MAIN_H

#include <ve_math.h>
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

/* The following is described here, although the code is in ve_htrack.c */
/** function veHeadTrackInit
    Initializes the built-in head tracking filter which provides a simple
    way for an input device to be redirected to the default eye reference
    frame.  The built-in filter (called "headtrack") also provides for
    simple linear correction of the incoming data.  This function is
    typically called by <code>veInit()</code>.
 */
void veHeadTrackInit();

/** function veHeadTrackGetLocCorr
    This function is for interactive calibration purposes and should
    be considered fragile.
    
    @returns
    A pointer to the location correction matrix used by the head-tracker
    filter.  If multiple independent head-tracking filters are defined
    that handle positional data, then this matrix corresponds to the first
    filter that is defined.  This points to live data, so changes to
    this matrix will be reflected the next time the head-tracking filter
    receives data.
*/
VeMatrix4 *veHeadTrackGetLocCorr();

/** function veHeadTrackGetDirCorr
    This function is for interactive calibration purposes and should
    be considered fragile.
    
    @returns
    A pointer to the orientation correction matrix used by the head-tracker
    filter.  If multiple independent head-tracking filters are defined
    that handle orientation data, then this matrix corresponds to the first
    filter that is defined.  This points to live data, so changes to
    this matrix will be reflected the next time the head-tracking filter
    receives data.
*/
VeMatrix4 *veHeadTrackGetDirCorr();

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

