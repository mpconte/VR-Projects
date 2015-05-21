#ifndef VE_BLUE_H
#define VE_BLUE_H

#include <stdio.h>

#include <ve_env.h>
#include <ve_version.h>
#include <ve_error.h>
#include <ve_util.h>

#include <bluescript.h>

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif /* __cplusplus */

/** function veBlueInit
    Initializes VE's BlueScript interpreter.  This is typically called
    by the <code>veInit()</code> function.
 */
void veBlueInit(void);

/** type VeBlueStringProc
    As an alternative to the standard BlueScript procedure interface,
    this VE-specific procedure type allows communication with strings.
    The simplified interfaces <code>veBlueSetResult()</code>,
    <code>veBlueSetVar()</code>, <code>veBlueGetVar()</code> and
    <code>veBlueSetStringProc()</code> can also be used.
    <p>When called, this procedure should always return either
    <code>BS_OK</code> or <code>BS_ERROR</code> to indicate respectively
    that execution completely successfully or an error occurred.  Use
    the <code>veBlueSetResult()</code> function to specify the result
    of the procedure (if return value is <code>BS_OK</code>) or an
    error message (if return value is <code>BS_ERROR</code>).
    <p>Parameters to the procedure are passed as an array of strings.
    <code>argv[0]</code> is the name of the procedure.
*/
typedef int (*VeBlueStringProc)(int argc, char **argv);

/** function veBlueSetResult
    Sets the result of a procedure call to a copy of the given
    string.

    @param value
    The result of the procedure call.  This overrides any previously
    set result.  If value is <code>NULL</code> then the result is an
    empty string.
 */
void veBlueSetResult(char *value);

/** function veBlueSetVar
    Sets a variable in the BlueScript interpreter.

    @param name
    The name of the variable to set.

    @param global
    If this is 0, then the variable is set in the local context.
    Otherwise it is set in the global context.

    @param value
    The value of the variable.  Passing <code>NULL</code> here
    will unset the variable.
 */
void veBlueSetVar(char *name, int global, char *value);

/** function veBlueGetVar
    Retrieves the value of a variable from the BlueScript interpreter.

    @param name
    The name of the variable to retrieve.

    @param global
    If this is 0, then the variable is retrieved from the local
    context.  Otherwise, it is retrieved from the global context.

    @returns
    A pointer to a read-only string if the variable is found, or
    <code>NULL</code> if the variable is not found.
 */
char *veBlueGetVar(char *name, int global);

/** function veBlueSetProc
    Creates a procedure in VE's BlueScript interpreter using a
    simplified interface.  This interface uses strings instead
    of BlueScript objects.  This makes coding simpler at the
    loss of some efficiency.
 */
void veBlueSetProc(char *name, VeBlueStringProc proc);

/** function veBlueSetExtProc
    Creates a procedure in VE's BlueScript interpreter.  When the
    procedure is called, the given process will be called.  If an
    existing procedure already uses this name, then the existing
    procedure is overridden.
    <p>Use of this function assumes that you know how to use
    the BlueScript objects.  For a simpler but less efficient
    interface see the <code>veBlueSetStringProc</code> function.

    @param name
    The name of the procedure.

    @param proc
    A BlueScript procedure function to call when the procedure is
    called.
 */
void veBlueSetExtProc(char *name, BSExtProc proc, void *cdata);

/** function veBlueGetVector3
    Attempts to interpret a BlueScript object as a VeVector3
    object.

    @param o
    The object to interpret.

    @returns
    A pointer to a read-only VeVector3 object.  If the
    object cannot be interpreted as a valid VeVector3 object
    then <code>NULL</code> is returned and an error message is
    left in the interpreter's result object.
 */
VeVector3 *veBlueGetVector3(BSObject *o);

/** function veBlueSetVector3Result
    Sets the result of the interpreter to be a VeVector3
    object.

    @param v
    The vector that will be stored in the result.
 */
void veBlueSetVector3Result(VeVector3 *v);

/** function veBlueGetMatrix4
    Attempts to interpret a BlueScript object as a VeMatrix4
    object.

    @param o
    The object to interpret.

    @returns
    A pointer to a read-only VeMatrix4 object.  If the
    object cannot be interpreted as a valid VeMatrix4 object
    then <code>NULL</code> is returned and an error message is
    left in the interpreter's result object.
 */
VeMatrix4 *veBlueGetMatrix4(BSObject *o);

/** function veBlueSetMatrix4Result
    Sets the result of the interpreter to be a VeMatrix4
    object.

    @param v
    The vector that will be stored in the result.
 */
void veBlueSetMatrix4Result(VeMatrix4 *v);

/** function veBlueGetQuat
    Attempts to interpret a BlueScript object as a VeQuat
    object.

    @param o
    The object to interpret.

    @returns
    A pointer to a read-only VeQuat object.  If the
    object cannot be interpreted as a valid VeQuat object
    then <code>NULL</code> is returned and an error message is
    left in the interpreter's result object.
 */
VeQuat *veBlueGetQuat(BSObject *o);

/** function veBlueSetQuatResult
    Sets the result of the interpreter to be a VeQuat
    object.

    @param v
    The vector that will be stored in the result.
 */
void veBlueSetQuatResult(VeQuat *v);

/** function veBlueEvalString
    Evaluates a string using VE's BlueScript interpreter.

    @param s
    The string to evaluate.

    @returns
    0 if the evaluation was successful, or non-zero
    if an error occurred or an unexpected result code was
    encountered.
 */
int veBlueEvalString(char *s);

/** function veBlueEvalFile
    Evaluates a file using VE's BlueScript interpreter.

    @param filename
    The name of the file.

    @returns
    0 if the evaluation was successful, or non-zero
    if an error occurred or an unexpected result code was
    encountered.
 */
int veBlueEvalFile(char *filename);

/** function veBlueEvalStream
    Evaluates a stdio stream using VE's BlueScript interpreter.

    @param filename
    The stream to evaluate.

    @returns
    0 if the evaluation was successful, or non-zero
    if an error occurred or an unexpected result code was
    encountered.
 */
int veBlueEvalStream(FILE *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_BLUE_H */
