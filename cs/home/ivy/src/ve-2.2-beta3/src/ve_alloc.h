#ifndef VE_ALLOC_H
#define VE_ALLOC_H

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif /* __cplusplus */

/** misc
    This module provides memory allocation and freeing abstractions
    for use within VE.  In particular, these abstractions:
    <ul>
    <li>Verify that the allocation succeeds and aborts program execution
    if it fails.</li>
    <li>Only frees a pointer if it is not NULL.</li>
    </ul>
    <p>In addition to the functions below, the macro
    <code>veAllocObj()</code> is defined.  This macro takes a datatype
    as an argument and allocates space for an object of that type.
    For example: <code>veAllocObj(VeEnv)</code> would allocate space
    to hold a <code>VeEnv</code> object.  The bytes of the object are
    initialized to 0 and the value of the macro is automatically cast
    to a pointer of the appropriate type.
*/

#define veAllocObj(x) ((x *)veAlloc(sizeof(x),1))

/** function veAlloc
    Allocates a region of memory.  Always returns a valid range of
    memory or aborts program execution.
    
    @param nbytes
    The number of bytes to allocate.  This must not be zero.
    If an allocation of 0 bytes is attempted, then the program will
    exit.

    @param clear
    If this value is zero, then all allocated bytes will be initialized to
    0.  If this value is not zero, then the initial value of the
    allocated bytes is undefined.

    @returns
    A pointer to a newly-allocated region of memory.
*/
void *veAlloc(unsigned nbytes, int zero);

/** function veRealloc
    Reallocates a previously allocated region of memory (like realloc).
 */
void *veRealloc(void *ptr, unsigned nbytes);

/** function veFree
 */
void veFree(void *);

/** function veDupString
    <p>Returns a duplicate of the given string.  The result
    has been newly allocated from the heap.</p>
    
    @param s
    The string to duplicate.
    
    @returns
    A duplicate of the given string.
*/
char *veDupString(char *s);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* VE_ALLOC_H */
