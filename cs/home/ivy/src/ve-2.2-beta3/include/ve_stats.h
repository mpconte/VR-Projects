#ifndef VE_STATS_H
#define VE_STATS_H

#include <ve_version.h>
#include <ve_error.h>
#include <ve_util.h>
#include <ve_thread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** misc
  <p>The ve_stats module provides a general method for reporting and
  managing statistics.  While the program is running there are numerous
  statistics (e.g. frame rate) which are interesting to devlopers and
  users.  It would be useful if these statistics could be viewed live
  (while the application is running) or logged for perusal after
  the program finishes.
  <p>This module provides the support for storing the data, managing
  access to the data, and a callback mechanism for informing other
  parts of the program of changes to interesting data.

 */

/** enum VeStatisticType
  This describes the format of a statistic.  The set of possible
  formats is deliberately limited so that it is relatively simple
  to interpret a statistic.  The meaning of the data member of the 
  VeStatistic structure is described for each value:

  @value VE_STAT_INT
  An integer.  "data" points to an "int".  (i.e. cast "data" to a "int *")
  @value VE_STAT_FLOAT
  A floating-point number.  "data" points to a "float".
  (i.e. cast "data" to a "float *")
  @value VE_STAT_STRING
  A string - an arbitrary value type.  Note that this is really
  a "catch-all".  If a statistic can be represented well by another
  type, then it is better to use that type rather than a string.
  "data" points to a "char *". (i.e. cast "data" to a "char **")
 */
typedef enum {
  VE_STAT_INT,
  VE_STAT_FLOAT,
  VE_STAT_STRING
} VeStatisticType;

struct ve_statistic;

/** type VeStatCallback
  The function prototype for any statistic callback.  A callback will
  get one argument - a pointer to the statistic that has changed, or
  a NULL pointer, indicating that some number of statistics (>1) have
  been updated and that the callback should decide for itself which
  statistics to look at.
 */
typedef void (*VeStatCallback)(struct ve_statistic *);

/** struct VeStatCallbackList
  A linked-list of statistic callbacks and their associated properties.
  */
typedef struct ve_stat_cback_list {
  /** member cback
    The callback to be called.
    */
  VeStatCallback cback;
  /** member last_call
    The time of the last callback in milliseconds
    (relative to veCurrentTime()).
    */
  long last_call;
  /** member min_interval
    The minimum time in milliseconds between calls to this callback.
    If zero, then there is the callback will be called as often as necessary.
    If greater than zero, then updates that occur before min_interval
    milliseconds have passed are discarded for this callback.
    */
  long min_interval;
  struct ve_stat_cback_list *next;
} VeStatCallbackList;

/** struct VeStatistic
  The VeStatistic structure represents a single statistic that may be
  gathered while the program is running.
  */
typedef struct ve_statistic {
  /** member module
    The name of the module from which this statistic originates.
    */
  char *module;

  /** member name
    The name of the statistic.  This name is only guaranteed to be
    unique within a module, so the combination of "module" and "name"
    is unique, but the name alone might not be.
    */
  char *name;

  /** member type
    The type of the statistic (from the enum VeStatisticType).  This
    defines the format of the data stored here.
    */
  VeStatisticType type;

  /** member units
    A string describing the units of the statistic.  This is for 
    display purposes only - this value will not be interpreted directly.
    */
  char *units;

  /** member data
    The actual statistic data.  The meaning of this value is dependent
    upon the "type" member.  (See the info on the VeStatisticType enum
    for more information.)
    */
  void *data;
  
  /** member last_change
    The time when this statistic was last changed, in milliseconds, relative
    to veCurrentTime().
    */
  long last_change;

  /** member listeners
    A linked-list of callbacks that should be called when this specific
    statistic is changed, as opposed to the general statistic callback which
    is triggered when any statistic changes.
    */
  VeStatCallbackList *listeners;
  
  /** member listen_mutex
    A mutex used for managing changes to the callback list
    */
  VeThrMutex *listen_mutex;

  /** member udata
    Often when maintaining a statistic, many other values are needed as
    well.  This holder is provided so that the modules which provide
    the statistic can also store the extra data necessary to maintain the
    statistic with the statistic structure.  Applications and callbacks
    should ignore this field and not modify it.
   */
  void *udata;
} VeStatistic;

/** struct VeStatisticList
  A linked-list of statistics, both used internally for tracking and
  as a means of walking through the set of available statistics.
  */
typedef struct ve_stat_list {
  VeStatistic *stat;
  struct ve_stat_list *next;
} VeStatisticList;

/** function veStatInit
    <p>Initializes the statistics sub-system.  Must be called from a
    thread-safe point before the statistics are used. 
*/
void veStatInit(void);

/** function veAddStatistic
  Called by a module to make a statistic available.
  Until this is done, no callbacks can be setup to listen to changes on
  this statistic.  Callbacks which are setup to listen to any changes
  will be notified immediately of this statistic.  Modules should ensure
  that there is valid data in the statistic as it may be processed
  immediately.

  @param stat
  A pointer to the statistic structure.  The memory for the structure
  is handled by the module and should not be freed without calling
  veRemoveStatistic.

  @returns
  0 on success, non-zero on failure.  Errors are logged using the
  standard VE error mechanism.  An error will be generated if the
  statistic has already been added.

  */
int veAddStatistic(VeStatistic *stat);

/** function veRemoveStatistic
  Called by a module to indicate that a statistic should no longer
  be made available.  After returning from this call, it is safe to
  free the statistic structure.  No further references to the statistic
  will be kept outside of the module.

  @param stat
  A pointer to the statistic structure to be removed.  This must be
  the same pointer that was passed to the corresponding veAddStatistic()
  call.

  @returns
  0 on success, non-zero on failure.  Errors are logged using the
  standard VE error mechanism.  An error will be generated if the
  statistic is not currently in use.
  */  
int veRemoveStatistic(VeStatistic *stat);

/** function veGetStatistics
  Returns a pointer to the head of the list of statistics.
  Applications and callbacks can use this function to determine
  the currently available statistics.

  @returns
  A pointer to the head of list if successful, and NULL if there
  is a failure or if the list is empty.
  */
VeStatisticList *veGetStatistics();

/** function veAddStatCallback
  Adds a callback to the given statistic.

  @param stat
  The statistic to add the callback to.  If this value is NULL
  then a general statistic callback will be setup - this callback
  will be called for any change to any statistic and whenever a new
  statistic is added.

  @param cback
  The callback that will be added.

  @param min_interval
  The minimum time in milliseconds that must pass between calls to this
  callback.  This helps throttle the update rate when slower frequencies
  are sufficient.  If set to zero, then the callback will be called every
  time the data changes.

  @returns
  0 on success, non-zero on failure.  Errors are logged using the
  standard VE error mechanism.  An error will be generated if the
  given callback is already listening on the given statistic, or
  if the given callback is already a general statistic callback.
 */
int veAddStatCallback(VeStatistic *stat, VeStatCallback cback, 
		      long min_interval);

/** function veRemoveStatCallback
  Removes a callback from the given statistic.

  @param stat
  The statistic to remove the callback from.  If this value is NULL
  then the corresponding general statistic callback (if it exists)
  will be removed.

  @param cback
  The callback that will be added.

  @returns
  0 on success, non-zero on failure.  Errors are logged using the
  standard VE error mechanism.
 */
int veRemoveStatCallback(VeStatistic *stat, VeStatCallback cback);

/** function veUpdateStatistic
  Called by a module to let the system know that this statistic has
  been updated.  The system will then arrange for the appropriate
  callbacks to be called.

  @param stat
  The statistic that has changed.

  @returns
  0 on success, non-zero on failure.  Errors are logged using the
  standard VE error mechanism.
  */
int veUpdateStatistic(VeStatistic *stat);

/** function veNewStatistic
  Creates an empty VeStatistic structure which the extra structures
  (i.e. synchronization units) initialized.

  @param module
  Value that will be placed in the module slot in the structure.
  @param name
  Value that will be placed in the name slot in the structure.
  @param units
  Value that will be placed in the units slot in the structure.

  @misc
  Note that for "module", "name" and "units" these strings will
  not be copied - the given pointers will just be stuffed into the
  structure.
  */
VeStatistic *veNewStatistic(char *module, char *name, char *units);

/** function veStatToString
  Converts a statistic to a printable string describing the statistic
  and its current value.

  @param stat
  The statistic to convert to a string.

  @param str_ret
  The string in which the result will be printed.

  @param len
  The length of the space pointed to by str_ret.
  */
int veStatToString(VeStatistic *stat, char *str_ret, int len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_STATS_H */
