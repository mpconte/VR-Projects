#ifndef VE_ERROR
#define VE_ERROR

#ifdef __cplusplus
extern "C" {
#endif

  /** misc
      <p>The ve_error module is a general method for reporting internal
      errors to the application and/or user.  Its API is also available
      for applications to use for application errors if the author
      chooses.</p>
      <p>Errors are loosely grouped into the following categories:
      notices, warning, errors, and fatal errors (a.k.a. "fatals").
      Notices are information messages which relate information about
      the state of the library, detected or default settings, but which
      do not indicate a problem.  Warnings are detected problems from
      which the library believes it can recover with little impact
      on the functioning of the system.  Errors are problems from which
      the library can recover but which may significantly impact 
      operations.  Fatals are errors from which the library cannot
      (or should not) recover.</p>
      <p>Of these, notices and warnings are the only optional messages.
      That is, the application may choose to disregard notices and
      warnings.  By default notices are not shown, and warnings are
      shown.</p>
  */

/* error message handling */
#define VE_ERR_NOTICE   0
#define VE_ERR_WARNING  1
#define VE_ERR_ERROR    2
#define VE_ERR_FATAL    3

typedef void (*VeErrorHandler)(int type, char *module, char *msg);

  /** function veShowNotices
      <p>Sets the library's disposition to notices - if non-zero then
      notices will be passed to the error handler.  If zero then notices
      will not be passed to the error handler.  By default, notices are
      not passed to the error handler.</p>

      @param i
      If non-zero, notices are passed on.  If zero, notices are discarded.
  */
void veShowNotices(int i);
  /** function veShowWarnings
      <p>Sets the library's disposition to warnings - if non-zero then
      warnings will be passed to the error handler.  If zero then warnings
      will not be passed to the error handler.  By default, warnings are
      passed to the error handler.</p>

      @param i
      If non-zero, warnings are passed on.  If zero, warnings are discarded.
  */
void veShowWarnings(int i);
  /** function veNotice
      <p>Generates a notice.  The message is passed in <code>printf()</code>
      style.</p>
      
      @param module
      The name of the module from which the notice originates.  All
      VE library messages have modules which begin with "ve_" or "vei_".
      Applications should avoid these prefixes.
      
      @param fmt
      The format string of the message, in <code>printf()</code>-style.
  */
void veNotice(char *module, char *fmt, ...);
  /** function veWarning
      <p>Generates a warning.  The message is passed in <code>printf()</code>
      style.</p>
      
      @param module
      The name of the module from which the notice originates.  All
      VE library messages have modules which begin with "ve_" or "vei_".
      Applications should avoid these prefixes.
      
      @param fmt
      The format string of the message, in <code>printf()</code>-style.
  */
void veWarning(char *module, char *fmt, ...);
  /** function veError
      <p>Generates an error.  The message is passed in <code>printf()</code>
      style.</p>
      
      @param module
      The name of the module from which the notice originates.  All
      VE library messages have modules which begin with "ve_" or "vei_".
      Applications should avoid these prefixes.
      
      @param fmt
      The format string of the message, in <code>printf()</code>-style.
  */
void veError(char *module, char *fmt, ...);
  /** function veFatalError
      <p>Generates an error.  The message is passed in <code>printf()</code>
      style.  After delivering the message to the error handler, the
      program will be terminated.</p>
      
      @param module
      The name of the module from which the notice originates.  All
      VE library messages have modules which begin with "ve_" or "vei_".
      Applications should avoid these prefixes.
      
      @param fmt
      The format string of the message, in <code>printf()</code>-style.
  */
void veFatalError(char *module, char *fmt, ...);
  /** function veSetErrorHandler
      <p>There is a single global error handler through which all messages
      pass.  It is the job of this handler to report the message to the
      user or application.  The default error handler directs all messages
      to <code>stderr</code>.<p>
      <p>An error handler is defined as:
<pre>
        void (*VeErrorHandler)(int type, char *module, char *msg)
</pre>
      The <code>type</code> argument is one of the following constants,
      each representing the corresponding type of the error being passed to
      the handler:  <code>VE_ERR_NOTICE</code>, <code>VE_ERR_WARNING</code>,
      <code>VE_ERR_ERROR</code>, <code>VE_ERR_FATAL</code>.
      The <code>module</code> argument is the name of the VE module from
      which the message originated (as passed to the appropriate error
      function) and the <code>msg</code> argument is the error message.
      Error handlers should relate both pieces of information to the
      user.</p>
      <p>When writing error handlers, you should also beware when
      handling fatals.  A fatal will terminate the program as soon as the
      error handler returns, so relying on an asynchronous event to deliver
      the message may result in the message never reaching the user.
      </p>

      @param handler
      The new error handler to install.  If this is <code>NULL</code> then
      the default error handler is installed.
   */
void veSetErrorHandler(VeErrorHandler handler);

#ifdef __cplusplus
}
#endif

#endif /* VE_ERROR */
