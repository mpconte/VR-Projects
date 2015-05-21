#ifndef VE_DEBUG_H
#define VE_DEBUG_H

/** misc
    <p>
    The ve_debug module provides internal debugging message support.
    Normally debugging statements are inserted in code using the
    <code>VE_DEBUG</code> and <code>VE_ISDEBUG</code> macros as follows:</p>
    <dl>
    <dt><code>VE_DEBUG(x,s)</code></dt>
    <dd>If the integer value <code>x</code> is greater than the current
    global debugging level, then the <code>printf()</code>-style message
    <code>s</code> will be printed as a debugging message.  Note that 
    <code>s</code> <i>must</i> have parentheses around it, e.g.
    <code>VE_DEBUG(1,("at this point foobar = %d",foobar))</code>
    </dd>
    <dt><code>VE_ISDEBUG(x)</code></dt>
    <dd>If the integer value <code>x</code> is greater than the current
    global debugging level, then the value of this macro is true (i.e.
    non-zero), else it is false (i.e. 0).  It is meant for use in 
    <code>if</code>-statements, e.g. <code>if (VE_ISDEBUG(2)) { ...</code>.
    </dd>
    </dl>
    <p>By default the debugging level is 0.  Thus all debug statements
    should use a minimum debugging level of 1.  This option can be changed
    via the <code>veSetDebugLevel()</code> call or via the 
    <code>-ve_debug</code> command-line option at run-time (e.g. 
    <code>prog -ve_debug 2</code> would run <code>prog</code> at a 
    debug level of 2).</p>
    <p>If the macro <code>VE_NODEBUG</code> is defined when <em>compiling</em>
    the VE library then all <code>VE_DEBUG</code> macros will generate
    a null statement and all <code>VE_ISDEBUG</code> macros will be equated 
    to 0 regardless of the debugging level.  This allows all debugging
    statements to be removed from the final executable without editing
    them out.
    </p>
*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef VE_NODEBUG
#define VE_DEBUG(x,s) ;
#define VE_ISDEBUG(x) 0
#else
#define VE_DEBUG(x,s) { if (veGetDebugLevel() > x) veDebug s ; }
#define VE_ISDEBUG(x) (veGetDebugLevel() > x)
#endif /* VE_NODEBUG */

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
  /** function veSetDebugLevel
      Sets the debug level for the program.  There is only a single
      global debug level which affects all debug statements compiled
      into the program.
      
      @param i
      The new debug level.
  */
void veSetDebugLevel(int i);
  /** function veGetDebugLevel
      Returns the current debug level for the program.  Although this
      introduces the overhead of a function call into the debug macro,
      it does allow it link properly on platforms that only link procedures
      and not variables at run-time (e.g. Windows).

      @returns
      The current debug level.
  */
int veGetDebugLevel(void);

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_DEBUG_H */
