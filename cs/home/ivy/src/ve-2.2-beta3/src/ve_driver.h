#ifndef VE_DRIVER_H
#define VE_DRIVER_H

#include <ve_version.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if 0
} /* strictly to fix auto-indenting in emacs */
#endif

/** section VE 2.2 Changes
    The ve_driver module has undergone some changes for VE 2.2.
    In particular, there is a "driver indexing" method. 
    Compatible drivers that provide a "probe" function allow VE to
    automatically determine what functionality a driver provides
    without requiring explicit "driver" statements in the input
    manifest.  It is anticipated that this will become the only
    method for conditional loading of drivers in future versions of
    VE.  The "driver" statement in the manifest file will be replaced
    with a more generic "load" statement to allow the loading of any
    shared object.
    <p>Currently known driver types:
    <table>
    <tr><th>Driver Type</th><th>Desc</th><th>Name Meaning</th></tr>
    <tr>
    <td><code>input</code></td>
    <td>input device</td>
    <td>input device type</td>
    </tr>
    <tr>
    <td><code>filter</code></td>
    <td>input event filter</td>
    <td>filter name</td>
    </tr>
    <tr>
    <td><code>render</code></td>
    <td>rendering implementation</td>
    <td>rendering interface</td>
    </tr>
    <tr>
    <td><code>audio</code></td>
    <td>audio hardware driver</td>
    <td>type of audio hardware</td>
    </tr>
    </table>

*/

/** section Description
  The ve_driver module provides general support for run-time loading
  of drivers.  The concept of a driver is general - just about anything
  can be loaded at run-time so long as it can make the appropriate calls
  to hook itself into the library.  Typically drivers are used for
  input devices but they can be used for other services as well.
  <p>
  If you are writing a dynamic driver the only magic to keep in mind is
  that you should have a function name ve_<i>drvname</i>_init where
  <i>drvname</i> is the name of your shared object.  This is the function
  that will be called to startup your shared object once it has been loaded.
  For example, if your shared object is called "foobar.so" then the 
  initialization function is ve_foobar_init.  For the sake of convenience
  there is a macro called <code>VE_INPUT_DRIVER_INIT(drvname)</code> which
  will generate the appropriate name.  It is highly recommended that you
  make use of this macro to help insulate your driver against future
  changes.
  */

/** function veLoadDriver
  This is the core function for dynamic drivers.  Given the name of a
  dynamic module (including the appropriate extension - i.e. ".so",
  ".dll", etc.) this function will find and initialize the driver.
  If the path of the driver is not absolute, then the following algorithm
  is used to search for the drivers.
  <ol>
  <li>Any paths specified via the veAddDynDriverPath function are checked
  first.
  <li>If the VEROOT environment variable is set, then the directories
  $VEROOT, $VEROOT/lib and $VEROOT/lib/drivers are checked.  Typically,
  VEROOT is set to the directory containing the VE distribution.
  <li>As a fallback, the directories "/usr/lib", "/usr/local/lib", etc.
  are checked.
  </ol>
  <p>A driver will only be loaded if it has not been previously loaded.
  
  @param dllname
  The filename of the driver to load, including any necessary extension.

  @returns
  0 on success, and non-zero on failure.  Errors will be reported using
  the standard VE error mechanisms.
  */
int veLoadDriver(char *dllname);

/** function veAddDynDriverPath
  This function provides a way for adding directories to the dynamic
  driver search path (see veLoadDriver).  Paths are searched in
  the order they are added.

  @param path
  The path to add to the list of search paths.

  @returns
  0 on success, and non-zero on failue.  The only real source of error is
  if the internal limit on user-specified dynamic driver paths is reached.
*/
int veAddDynDriverPath(char *path);

/* Following macro is for use by dynamically-loaded drivers... */
#define VE_DRIVER_INIT(drvname) ve_##drvname##_init

/* New macro for use in probing */
#define VE_DRIVER_PROBE(drvname) ve_##drvname##_probe

  /** function veFindDynFunc
      Searches all dynamically-loaded drivers for a particular symbol
      name.  If one is found, then the address of that symbol is returned.
      It is primarily meant for looking up functions at run-time.  Some
      modules may provide functions that are not available at link-time.
      This call is a method for finding those functions (assuming you
      know the name of the function) while the program is running.
      
      @param nm
      The name of the function you are looking up.
  */
void *veFindDynFunc(char *nm);

/** function veSetDriverRoot
    Sets the default root path to the VE installation.  This is used
    to search for drivers in various places under the VE directory.
    If unspecificed, then the environment variable VEROOT is used.  If
    that environment variable doesn't exist then no searching is done
    in the VE installation.
*/
void veSetDriverRoot(char *veroot);

/** function veDriverProvide
    Called by a driver's probe function to indicate what features
    it is prepared to provide.  A driver probe function should be
    defined as:
<blockquote><code>void VE_DRIVER_PROBE(<i>drvname</i>) (void *phdl);</code></blockquote>
    where <i>drvname</i> is the name of the final shared object
    file without the system extension (e.g. without ".so" on most Unix 
    systems).
    <p>The probe function should make multiple calls to 
    <code>veDriverProvide()</code> in order to let VE know what the driver
    provides.
    
    @param phdl
    Probe handle.  This is a pointer that is passed to the driver's
    probe function.  Return the value here.

    @param type
    The "type" of the object being provided.  Currently known values
    include "device" for input devices, "filter" for event filters
    and "audio" for audio drivers.  This type must match the desired
    type in a call to <code>veDriverRequire()</code>.

    @param name
    The "name" of the object being provided.  
 */
void veDriverProvide(void *phdl, char *type, char *name);

/** function veDriverRequire
    Allows VE to load any drivers that provide support for the
    given type and name combination.  If such a driver is already
    loaded this operation has no effect.

    @param type
    The type of object that is required.

    @param name
    The name of the object that is required.  As a special case,
    the name "*" may be used to inidicate that there should be
    at least one driver of this type loaded.

    @returns
    0 if the driver is successfully loaded (or was previously 
    loaded), or non-zero if either a matching driver cannot be
    found, or the driver fails to load.
*/
int veDriverRequire(char *type, char *name);

/** section Internal Interfaces
    The remaining interfaces are for internal use only and for
    use by select utilities.
 */

/* These interfaces are well-and-truly internal only - they are
   provided in order to abstract the platform-specific parts of
   driver loading.
*/
/* object provided by a driver */
typedef struct ve_driver_obj {
  struct ve_driver_obj *next;
  char *type;
  char *name;
} VeDriverObj;

/* provided by ve_driver.c */
void veDriverObjFree(VeDriverObj *);

/* information about what a driver can provide */
typedef struct ve_driver_info {
  /* linked-list for "directory contents" */
  struct ve_driver_info *next;

  char *name;  /* name of the driver */
  char *path;  /* path to the driver */

  VeDriverObj *obj;  /* list of what the driver provides */

  /* connection info */
  void *handle;  /* if non-NULL then driver is loaded */
} VeDriverInfo;

/* Platform implementations must provide the following
   functions */
VeDriverObj *veDrvImplProbe(char *path);
int veDrvImplLoad(VeDriverInfo *di);
int veDrvImplIsDrv(char *path);
void *veDrvImplFind(VeDriverInfo *di, char *sym);
void veDrvImplClose(VeDriverInfo *di);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_DRIVER_H */
