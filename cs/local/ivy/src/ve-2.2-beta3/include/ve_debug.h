#ifndef VE_DEBUG_H
#define VE_DEBUG_H

#include <ve_version.h>

/** misc
    <p>
    The ve_debug module provides internal debugging message support.
    Normally debugging statements are inserted in code using the
    <code>VE_DEBUG2</code>, <code>VE_ISDEBUG2</code>,
    <code>VE_DEBUG</code> and <code>VE_ISDEBUG</code> macros.</p>
    <p>The V2 interface (<code>VE_DEBUG2</code> and <code>VE_ISDEBUG2</code>)
    allow debugging statements to be grouped together in "module" groups,
    so that specific selection of which debugging statements are
    desired (e.g. only
    debugging about "communication" or "rendering") is more flexible
    at run-time.</p>
    <p>The V2 macros have the following syntax and meaning:</p>
    <dl>
    <dt><code>VE_DEBUG2(m,x,s)</code></dt>
    <dd>
    If debugging is enabled for module <code>m</code> (where <code>m</code>
    is a string, usually a string literal) and the integer value <code>x</code>
    is greater than the debugging level associated with module <code>m</code>,
    then the <code>printf()</code>-style message
    <code>s</code> will be printed as a debugging message.  Note that 
    <code>s</code> <i>must</i> have parentheses around it, e.g.
    <code>VE_DEBUG2("comm",1,("at this point foobar = %d",foobar))</code>
    </dd>
    <dt><code>VE_ISDEBUG2(m,x)</code></dt>
    <dd>If the integer value <code>x</code> is greater than the current
    debugging level associated with module <code>m</code>, then the value of 
    this macro is true (i.e.
    non-zero), else it is false (i.e. 0).  It is meant for use in 
    <code>if</code>-statements, e.g. <code>if (VE_ISDEBUG2("app",2)) ...</code>.
    </dd>
    </dl>

    <p>There is also an older V1 debugging interface (<code>VE_DEBUG</code>
    and <code>VE_ISDEBUG</code>) which does not take module arguments and
    is provided both for simplicity and for backwards-compatibility with
    applications or drivers which may use the old interface.  In this case
    where the module is not specified, the module name is assumed to be
    "app" (meaning "application").
    </p>

    <dl>
    <dt><code>VE_DEBUG(x,s)</code></dt>
    <dd>If the integer value <code>x</code> is greater than the current
    "app" debugging level, then the <code>printf()</code>-style message
    <code>s</code> will be printed as a debugging message.  Note that 
    <code>s</code> <i>must</i> have parentheses around it, e.g.
    <code>VE_DEBUG(1,("at this point foobar = %d",foobar))</code>
    </dd>
    <dt><code>VE_ISDEBUG(x)</code></dt>
    <dd>If the integer value <code>x</code> is greater than the current
    "app" debugging level, then the value of this macro is true (i.e.
    non-zero), else it is false (i.e. 0).  It is meant for use in 
    <code>if</code>-statements, e.g. <code>if (VE_ISDEBUG(2)) { ...</code>.
    </dd>
    </dl>

    <p>Debugging levels are module-specific (e.g. the "app" module
    may have its own debugging level) except for the global level.  The
    global level applies to any module which does not have a module-specific
    debugging level (it's the default - get it?).</p>

    <p>By default all debugging levels are 0.  Thus all debug statements
    with a debugging level greater than or equal to 0 are not displayed.  
    This option can be changed
    via the <code>veSetDebug2Level()</code>, 
    <code>veSetDebugLevel()</code> call, or via the 
    <code>-ve_debug</code> command-line option at run-time (e.g. 
    <code>prog -ve_debug 2</code> would run <code>prog</code> at a 
    global debug level of 2).  You can also use the syntax 
    <code>-ve_debug <i>module</i>:<i>level</i></code> to set a debugging 
    level other than the global one.  For example, <code>-ve_debug app:5</code>
    would set the debugging level for the "app" module to be "5" without
    affecting other debugging levels.</p> 
    <p>If the macro <code>VE_NODEBUG</code> is defined when <em>compiling</em>
    the VE library then all <code>VE_DEBUG</code> macros will generate
    a null statement and all <code>VE_ISDEBUG</code> macros will be equated 
    to 0 regardless of the debugging level.  This allows all debugging
    statements to be removed from the final executable without editing
    them out.
    </p>

    <p>Note that there is also a <code>VE_DEBUGM()</code> macro with the
    same syntax as <code>VE_DEBUG()</code>.  This uses the <code>MODULE</code>
    macro to get the name of the module and should typically only be used
    internally in VE and not by applications.</p>
*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef VE_NODEBUG
#define VE_DEBUG(x,s) ;
#define VE_ISDEBUG(x) 0
#define VE_DEBUGM(x,s) ;
#define VE_DEBUG2(m,x,s) ;
#define VE_ISDEBUG2(m,x) 0
#else
#define VE_DEBUG(x,s) { if (veGetDebug2Level("app") > x) veDebug s ; }
#define VE_ISDEBUG(x) (veGetDebug2Level("app") > x)
#define VE_DEBUGM(x,s) { if ((veGetDebug2Level(MODULE) > x)) { veSetDebugModule(MODULE); veDebug s ; } }
#define VE_DEBUG2(m,x,s) { if ((veGetDebug2Level(m) > x)) { veSetDebugModule(m); veDebug s ; } }
#define VE_ISDEBUG2(m,x) ((veGetDebug2Level(m) > x))
#endif /* VE_NODEBUG */

  /** section Debug V2 Interface
      The new debugging interface.  This includes interfaces from
      the old debugging module that have been ported unchanged.
   */

  /** function veDebugEnabled
      Tests to see if any debugging is enabled.  Helps speed up
      module-based debugging to avoid too many string tests.
      
      @returns
      0 if no debugging messages or modules are enabled, 
      non-zero otherwise.
  */
int veDebugEnabled(void);


  /** function veSetDebugStr
      <p>
      Modifies the debug level based upon the given string.  If the
      string is just an integer, then this is used to set the global
      debugging level.  Otherwise, the syntax:
      <blockquote><i>module</i>:<i>level</i></blockquote> is expected
      to set the debugging level for a particular module.  A blank module
      modifies the "global" level. Invalid strings are ignored.</p>
      <p>Multiple settings may be made at once by separating them by
      either whitespace or commas, e,g, "app:3,comm:1,5"
      would set the "app" debugging level to 3, the "comm" debugging level
      to 1 and the global debugging level to 5.</p>

      @param s
      The debugging string.
   */
void veSetDebugStr(char *s);

  /** function veGetDebugStr
      Returns a static pointer to a buffer containing a description of
      the current debugging state.  This string is appropriate for passing
      to a call to <code>veSetDebugStr()</code>.

      @returns
      A pointer to a static string buffer.  This buffer should not
      be modified and may be modified by a later call to this function.
   */
char *veGetDebugStr(void);
 
  /** function veSetDebug2Level
      Sets either a module-specific debug level, or the global debug level.

      @param m
      The module for which the set the debug level.  If this is 
      <code>NULL</code> then the global debug level is set.
      
      @param i
      The new debug level.
  */
void veSetDebug2Level(char *m, int i);

  /** function veGetDebug2Level
      Returns the current debug level for the program.  This can be
      either a module-specific debug level or the global debug level
      (see the <i>m</i> parameter).
      Although this introduces the overhead of a function call into the 
      debug macro,
      it does allow it link properly on platforms that only link procedures
      and not variables at run-time (e.g. Windows).

      @param m
      The module for which to get the debug level.  If this is
      <code>NULL</code> then the global debug level is returned.

      @returns
      The current debug level.
  */
int veGetDebug2Level(char *m);

  /** function veSetDebugModule
      Any further debugging messages in this thread will be
      assumed to come from the module passed as an argument to
      this function, until the next call to this function.
      It is mostly used inside the debug macros and does not
      typically need to be called directly.

      @param m
      The module name.  If this is <code>NULL</code> then
      no further module information is printed.  Note that this
      must be a pointer to a static area - that is, the ve_debug
      module does not make a copy of the module name, but instead
      maintains a copy of the pointer.  This is done for speed.
   */
void veSetDebugModule(char *m);

  /** function veDebug
      Writes out a debugging message.  This function does not check
      the current debug level and will operate regardless of the
      value of the <code>VE_NODEBUG</code> macro.  Options are given
      in <code>printf()</code> style - a format string followed by
      any necessary arguments, e.g. <code>veDebug("x = %d", x)</code>.
      
      @param s
      The format string.  This argument must always be specified. 
      All other arguments are optional.
  */
void veDebug(char *s, ...);

 /** function veSetDebugPrefix
      Allows the setting of an optional prefix that will be appended
      (in addition to the "debug" message) to all debugging output.
      This is primarily meant so that debugging messages from slaves
      can be distinguished from that of the master.

      @param txt
      If NULL then any existing prefix is removed.  If not NULL, then
      a copy of the given string will be stored for use as the prefix.
  */
void veSetDebugPrefix(char *txt);

  /** section Debug V1 Interface
      These next functions describe the old debugging interface
      which is maintained for backwards compatibility.  The interaction
      between the the V1 interface and the V2 interface is described
      above.  In short, all V1 interface debugging functions are 
      assumed to apply to the "app" module.  Use of V1 debugging
      statements in applications is acceptable and recommended for
      present and future development.
  */

  /** function veSetDebugLevel
      Sets the global debug level for the program.
      
      @param i
      The new debug level.
  */
void veSetDebugLevel(int i);
  /** function veGetDebugLevel
      Returns the current global debug level for the program.  Although this
      introduces the overhead of a function call into the debug macro,
      it does allow it link properly on platforms that only link procedures
      and not variables at run-time (e.g. Windows).

      @returns
      The current debug level.
  */
int veGetDebugLevel(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_DEBUG_H */
