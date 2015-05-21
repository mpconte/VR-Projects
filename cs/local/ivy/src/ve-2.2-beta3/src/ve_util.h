#ifndef VE_UTIL_H
#define VE_UTIL_H

#include <ve_version.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

/** misc
    The ve_util module provides some generic abstractions for
    possibly unportable commands or common data structures.
*/

/** function veSysError
    <p>Returns the generic system error string, if there is one.
    For Unix systems this is typically the result of
    <code>strerror(errno)</code>.
 */
char *veSysError(void);

/** function veVsnprintf
    <p>An abstraction for the common but unportable <code>vsnprintf()</code>
    function.</p>
*/
void veVsnprintf(char *, int, char *, va_list);

/** function veSnprintf
    <p>An abstraction for the common but unportable <code>snprintf()</code>
    function.</p>
*/
void veSnprintf(char *, int, char *, ...);

/** function veWaitForData
    <p>Specific to Unix - this function will block (this thread only) until
    there is data available for reading on the given file descriptor.</p>

    @param fd
    The file descriptor to block on.
*/
int  veWaitForData(int fd);

/** function vePeekFd
    This function determines if data is available for reading on this
    file descriptor.  This functino returns immediately and does not block,
    nor does it consume any data from the file descriptor.
    Note that the definition of the return value for
    this function has changed in VE 2.2 and later.  In particular, the
    exact number of available bytes is no longer returned.
    
    @param fd
    The file descriptor to peek at.

    @returns
    A non-zero value if there is data available for reading or
    0 is no data is available for reading (i.e. a read() call would blovk).
*/
int  vePeekFd(int fd);

/** function veMicroSleep
    <p>Put the current thread to sleep for the given number of microseconds.  
    On most Unix systems, this is just "usleep".  Please be aware that
    on many systems that there is a minimum amount of time (usually 10-20
    milliseconds) that a thread may sleep for, depending upon the
    scheduler on the system.
    
    @param usecs
    The number of microseconds to sleep for.
*/
void veMicroSleep(int usecs);

/** function veIsFile
    Predicate to check to see if the given pathname is a regular file.

    @param path
    The path to check.
    
    @returns
    1 if the path exists and is a regular file or 0 otherwise.
 */
int veIsFile(char *path);

/** function veIsDir
    Predicate to check to see if the given pathname is a directory.

    @param path
    The path to check.
    
    @returns
    1 if the path exists and is a directory or 0 otherwise.
 */
int veIsDir(char *path);

/** function veIsPathAbsolute
    Predicate to check to see if the given pathname is absolute.
    This will vary from major platform to major platform (e.g. Windows
    vs. Unix).

    @param path
    The path to check.
    
    @returns
    1 if the path is absolute, 0 if the path is relative to the current
    directory.
*/
int veIsPathAbsolute(char *path);

/** type VeDir
    An opaque type representing a directory whose contents are being
    walked through.
 */
typedef void *VeDir;

/** function veOpenDir
    Starts a walk through the contents of a directory.  Note that
    any successful call to <code>veOpenDir()</code> should be matched with a
    call to <code>veCloseDir()</code> to release any resources that
    were allocated while opening this directory.

    @param dir
    The name of the directory you wish to walk through.
    
    @returns
    A <code>VeDir</code> object on success, or <code>NULL</code>
    otherwise.  The <code>veSysError()</code> function should be
    able to provide a more specific message to as the cause of
    failure if <code>NULL</code> is returned.
*/
VeDir veOpenDir(char *dir);

/** function veCloseDir
    Releases any resources allocated upon a successful call to
    <code>veCloseDir()</code>.  This function should be called
    at the end of any directory walk.

    @param d
    The directory object for which to release resources.  After
    this function is called, this object may no longer be used
    by the application.
 */
void veCloseDir(VeDir d);

#define VE_DIR_HIDDEN (1<<0)

/** function veReadDir
    Returns the next directory entry in a directory walk.
    
    @param d
    The directory object for which to retrieve the next element
    in the walk.

    @param flags
    Flags that control what values are returned.  Currently
    the following flags are defined:
    <ul>
    <li><code>VE_DIR_HIDDEN</code> - if not specified then any
    "hidden" files (system-specific) are not returned in a directory
    walk.  To see all hidden files, this flag must be specified for
    all calls to <code>veReadDir()</code>.</li>
    </ul>

    @returns
    A pointer to a null-terminated character array containing the
    name of the element in the directory on success.  This string
    should be treated as read-only and should not be considered
    accessible after the next call to <code>veReadDir()</code> or
    <code>veCloseDir()</code> on this directory object.  The
    value <code>NULL</code> is returned when the end of the walk has
    been reached.
 */
char *veReadDir(VeDir d, int flags);

/** section String Maps/Hash Tables
    <p>VeStrMap and VeStrMapWalk are a simple hash-table implementation
    which maps strings to arbitrary objects (i.e. represented as "void *")
    in a reasonably efficient manner.  Other pieces of code that use
    string maps should indicate what kind of data is being stored in the
    string map.  The implementation manages its own memory for the names
    in the hash table, but does not manage memory for data (i.e. the values).
    Thus, before destroying a string map, it is important to walk through it
    and dispose of the values appropriately (not the names) if you want to
    avoid memory leaks.
    <p>To walk through an existing string map, you need to create a walk.
    The code should look something like this (assuming <i>m</i> is an
    valid string map reference).
<pre>
        VeStrMapWalk w;
        w = veStrMapWalkCreate(m);
        if (veStrMapWalkFirst(w) == 0) {
                do {
                        name = veStrMapWalkStr(w);
                        data = veStrMapWalkObj(w);
                        ... do something interesting with name and data ...
                } while (veStrMapWalkNext(w) == 0);
        }
        veStrMapWalkDestroy(w);
</pre>
*/
typedef struct ve_stringmaprec *VeStrMap;
typedef struct ve_stringmapwalkrec *VeStrMapWalk;

/** function veStrMapCreate
    Creates a new string map.

    @returns
    A reference to a new string map.  The reference can be passed by value
    to other functions.
*/
VeStrMap veStrMapCreate();

/** type VeStrMapFreeProc
    A simple type definition to help in casting pesky functions for
    use with <code>veStrMapDestroy()</code>.
 */
typedef void (*VeStrMapFreeProc)(void *);

/** function veStrMapDestroy
    Destroys a previously created string map.  This will free any memory
    allocated to names in the map.  If the <i>freeval</i> parameter is
    non-null, then the given function pointer will be used to free any
    values in the map.  

    @param m
    The string map to destroy.

    @param freeval
    If non-null, then this function will be called for each value in the
    string map.  It should be used to free the memory associated with the
    value.  If this is <code>NULL</code> then no processing will be done
    for the values in the map.  For convenience, <code>NULL</code> values
    will not be passed to this function.  This allows the safe use of 
    <code>free()</code> here.
*/
void veStrMapDestroy(VeStrMap m, VeStrMapFreeProc freeval);

/** function veStrMapExists
    Check to see if a key exists in the map.

    @param m
    The string map to search in.

    @param name
    The name to search for.

    @returns
    1 if the name is found in the map, 0 if there is no entry in the
    map for that key.
 */
int veStrMapExists(VeStrMap m, char *name);

/** function veStrMapLookup
    Lookup a value in the string map.
    
    @param m
    The string map to search in.

    @param name
    The name to search for.

    @returns
    The data stored under the given name if it exists, or <code>NULL</code>
    otherwise.
*/
void *veStrMapLookup(VeStrMap m, char *name);

/** function veStrMapInsert
    Adds a name and value pair to the string map.  If a value with the
    same name already exists, the previous value is overwritten.  Be
    careful of this if you want to avoid memory leaks - if you need
    to dispose of values, you need to do it before you insert an object.

    @param m
    The string to map to insert into.
    
    @param name
    The name to be associated with the value.
    
    @param obj
    The value you are storing.  The string map stores this value and
    returns it when requested but does not otherwise interpret it.

    @returns
    0 on success, non-zero on failure.
*/
int veStrMapInsert(VeStrMap m, char *name, void *obj);

/** function veStrMapDelete
    Removes the value with the given name from the string map if it
    exists.

    @param m
    The string map to remove the value from.

    @param name
    The name of the value to remove.

    @returns
    0 on success, non-zero on failure.  If the value does not exist
    in the string map, 0 is still returned.
*/
int veStrMapDelete(VeStrMap m, char *name);

/** function veStrMapWalkCreate
    Creates a new walk through a string map.  To visit an arbitrary
    set of elements in a string map without prior knowledge of their
    names, you need to use a VeStrMapWalk object.  When initialized
    the walk will reference the given map but will not point to any
    particular place in the map.  Several walks can be progress at
    any point in time.  The order of a walk is arbitrary, but every
    node will be visited.  An object pointed to by a walk can be
    safely deleted, but after being deleted a walk may only be advanced
    or reset, not dereferenced.

    @param m
    The string map to associate this walk with.

    @returns
    A reference to a newly-created walk.
*/
VeStrMapWalk veStrMapWalkCreate(VeStrMap m);

/** function veStrMapWalkDestroy
    Destroys a previously created walk, freeing any memory associated
    with it.  This does not affect the string map.

    @param w
    The walk to destroy.
*/
void veStrMapWalkDestroy(VeStrMapWalk w);

/** function veStrMapWalkFirst
    Sets the position of the walk to the first value in the map.

    @param w
    The walk whose position should be set.
    
    @returns
    0 if successful.  If unsuccessful, or if there are no values in
    the map, a non-zero value is returned.
*/
int veStrMapWalkFirst(VeStrMapWalk w);

/** function veStrMapWalkNext
    Advances the position of the walk to the subsequent value in the map.
    
    @param w
    The walk whose position should be set.

    @returns
    0 if successful.  If unsuccessful, or if there is no subsequent value
    (i.e. the walk has been exhausted), a non-zero value is returned.
*/
int veStrMapWalkNext(VeStrMapWalk w);

/** function veStrMapWalkObj
    Dereferences a walk to retrieve the value stored at the current position
    of the walk.
    
    @param w
    The walk which should be dereferenced.
    
    @returns
    The value at the current position of the walk or <code>NULL</code>
    if the walk is not at a valid position.
*/
void *veStrMapWalkObj(VeStrMapWalk w);

/** function veStrMapWalkObj
    Dereferences a walk to retrieve the name of the value stored at the 
    current position of the walk.
        
    @param w
    The walk which should be dereferenced.
    
    @returns
    The name of the value at the current position of the walk or 
    <code>NULL</code> if the walk is not at a valid position.
*/
char *veStrMapWalkStr(VeStrMapWalk w);

/* for debugging/performance analysis */
/** function veStrMapStats
    Prints out some statistics on stderr of the current properties of
    the hash table.  Strictly for debugging and tuning purposes.
    
    @param m
    The map to print out statistics for.
 */
void veStrMapStats(VeStrMap m);

#ifdef __cplusplus
}
#endif

#endif /* VE_UTIL_H */
