#ifndef VE_DRIVER_H
#define VE_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** misc
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

/** function veRegDynamicDriver
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
int veRegDynamicDriver(char *dllname);

/** function veAddDynDriverPath
  This function provides a way for adding directories to the dynamic
  driver search path (see veRegDynamicDriver).  Paths are searched in
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_DRIVER_H */
