#ifndef VE_MP_H
#define VE_MP_H

/** misc
    The "ve_mp" module provides multi-processing support for the VE library.
    Multi-processing support is for distributing the rendering load amongst
    several tasks either for efficiency (e.g. multiple threads on a multi-pipe
    machine) or out of necessity (e.g. a distributed cluster hooked together
    by networks).  It takes care of the following tasks:
    <ul>
    <li>creating and initializing slaves (local and remote processes, threads)
    <li>providing synchronization and data update methods
    </ul>

    <p>In any model, multi-processing works in a master-slave model.  The
    initial process (i.e. the one from which the program is started) acts
    as the master, processes all incoming device events and pushes data
    (generally state updates) to the slaves.

    <p>The MP support is integrated in such a way that existing programs
    should work <em>in their current single-process environments</em> 
    with nothing more than a recompile.  To make a program MP-compatible,
    you will need to:
    <ul>
    <li>guard any program initialization with a call to 
        <code>veMPIsMaster()</code>.  For example:
	<blockquote><code>if (veMPIsMaster()) veDeviceAddCallbck(...)</code></blockquote>
	Such guard is not built-in to the various functions (e.g. 
	veDeviceAddCallback) to avoid too much interdependence within the
	library's modules and to discourage code which would be
	incompatible with possible future models which would support
	more functionality in slaves.
    <li>arrange a data handler (see <code>veMPDataAddHandler()</code>)
        and push state and model information out to slaves.  The library
	will push initial environment and profile information to the
	slaves itself.  A simpler model in many programs is to simply
	set up state variables (see <code>veMPAddStateVar()</code>) which
	is a chunk of memory that will be automatically updated on all
	slaves at the beginning of each frame.
    </ul>
*/    

#define VE_DTAG_ANY (-1)
#define VE_DMSG_ANY (-1)

/** type VeMPDataHandler
    The type for an application MP callback.  This allows functions
    in the application to be called for registered message types.
    (see veMPDataAddHandler).
*/
typedef void (*VeMPDataHandler)(int tag, void *data, int dlen);

/** type VeMPIntDataHandler
    An internal data handler.  These are typically not used by applications
    but are used internally by the library to handle the underlying
    control communication.
 */
typedef void (*VeMPIntDataHandler)(int msg, int tag, void *data, int dlen);

/** function veMPSlaveInit
    Initializes the multi-processing for a slave.  This needs to be called
    before any other initialization in the system - either as the first
    call in veInit() or before any other VE initialization.  This is
    crucial as veMPSlaveInit() looks at the initial arguments to determine
    whether or not this is a slave process.  This call removes any
    arguments that are specific to the MP module from <i>argv</i> and 
    updates <i>argc</i> accordingly.
    <p>If this process is the master, then this function is
    used to cache the name of the program and the right set of arguments
    to use when creating slaves.  That is, this function should be called
    exactly once for all processes, including master and slaves.

    @param argc
    A pointer to an integer containing the number of arguments in 
    <i>argv</i> (as would be passed to <code>main()</code>).

    @param argv
    An array of char pointers representing strings (as would be
    passed to <code>main()</code>).  Element 0 is assumed to be
    the program name and is left untouched when processing options.

    @returns
    0 on success, non-zero on error.  Note that the contents of 
    argc and argv will be modified by this call.
 */
int veMPSlaveInit(int *argc, char **argv);

/** function veMPInit
    Initializes the MP subsystem on the master process creating the
    slave processes.  This function should not be called before an
    environment has been properly defined.  It is usually called
    automatically as part of the call to <code>veRun()</code>.
    On the master it starts the slaves and sends initialization
    information to them.  On all processes, it starts up a thread
    for handling incoming messages.

    @returns
    0 on success, non-zero on failure.
 */
int veMPInit(void);

/* data updates */
/* application interface */

/** function veMPDataAddHandler
    Adds a callback to handle data updates.

    @param tag
    If this is <code>VE_DTAG_ANY</code> then any incoming application
    data message is passed to the callback.  Otherwise, only those
    messages with the same tag as <i>tag</i> are passed.  This call
    functions both on the master and on the slaves (i.e. the master
    can receive its own data push messages).

    @param handler
    The handler to be called when an appropriate message is received.
    
    @returns
    0 on success, non-zero on failure.
*/
int veMPDataAddHandler(int tag, VeMPDataHandler handler);

/** function veMPDataPush
    Pushes data from the master process to the slave processes.
    This function can only be called on the master process.

    @param tag
    An identifier that will be passed to the callback on the slaves
    to help identify the contents of the message.

    @param data
    The message which is <i>dlen</i> bytes long.

    @param dlen
    The size of the message (in bytes) that is stored in <i>data</i>.
    
    @returns
    0 on success, non-zero on failure.
 */
int veMPDataPush(int tag, void *data, int dlen);

/** function veMPLocationPush
    Pushes the origin and default eye information to the slaves.
    This must be done after these values are updated by the program.

    @returns
    0 on success, non-zero on failure.
 */
int veMPLocationPush(void);

#define VE_MP_AUTO (1<<0)

/** function veMPAddStateVar
    Adds a state variable.  A state variable is a piece of memory that will
    be passed from master to slave(s), typically indicating some state of the
    application.  In order for state variables to be pushed correctly, this
    function must be called in the master and in all slaves.  It is best for
    state variables to be added before the call to <code>veRun()</code>.
    Applications are responsible for synchronization between master and slaves
    if state variables are created after the call to <code>veRun()</code>.

    @param tag
    An identifier that is used to identify this particular state variable.
    This value must be unique to this variable and be the same on both the
    master and slaves.  The value must also be greater than or equal to 0.

    @param var
    A pointer to the state variable.  This must point to a static 
    storage location.  If this is the address of a dynamic variable, 
    grave problems with the application will likely occur.
    
    @param vlen
    The size of the state variable in bytes.

    @param flags
    Controls how the state variable is handled.  Multiple flags can be combined
    with the bitwise-or (|) operator.  Currently the following
    values are defined:
    <ul>
    <li><code>VE_MP_AUTO</code> - state variable is automatically pushed
    before rendering each frame.  If this flag is defined then the application
    does not need to explicitly push the state variable to the slaves; it will
    be done automatically.  Without this flag, state variables need to be explicitly
    pushed to slaves with the <code>veMPPushStateVar()</code> function.</li>
    </ul>

    @returns
    0 on success, non-zero on failure.
    
 */
int veMPAddStateVar(int tag, void *var, int vlen, int flags);

/** function veMPPushStateVar
    Distributes state variables to slaves.  This function only has an effect on the
    master process.  On slave processes, it is a no-op.

    @param tag
    Indicates which state variable to push.  If this value is <code>VE_DTAG_ANY</code>,
    then all state variables are pushed, provided that they meet any constraints imposed
    by the flags (see below).

    @param flags
    Controls how variables are pushed.  Multiple flags can be combined
    with the bitwise-or (|) operator.  If this value is 0, then no constraints
    are made upon which variables are pushed.  Currently the following
    values are defined:
    <ul>
    <li><code>VE_MP_AUTO</code> - only state variables created with the
    <code>VE_MP_AUTO</code> flag will be pushed.  Applications should not need
    to use this flag - it is for internal use.</li>
    </ul>

    @returns
    0 on success, non-zero on failure.
 */
int veMPPushStateVar(int tag, int flags);

/* library interface */
/* reserved message types */
#define VE_MPMSG_DATA      0x0   /* application data sync */
#define VE_MPMSG_CTRL      0x1   /* slave renderer control messages */
#define VE_MPMSG_LOCATION  0x2   /* location message (origin/eye) */
#define VE_MPMSG_ENV       0x3   /* environment message */
#define VE_MPMSG_PROFILE   0x4   /* profile message */
#define VE_MPMSG_RESPONSE  0x5   /* response to a query */
#define VE_MPMSG_RECONNECT 0x6   /* implementation-dependent - tells client
				    to reconnect to the master as per 
				    arguments - used to switch over to
				    lower-latency communicaton paths */
#define VE_MPMSG_STATE     0x7   /* state information message */

#define VE_MPMSG__SYSDEP   0x7f  /* system-dependent - used by specific
				    MP implementations when they need some
				    non-standard communication */

/* tags for control messages */
#define VE_MPCTRL_WINDOW    0x1   /* open and manage given window name */
#define VE_MPCTRL_RUN       0x2   /* start renderer threads running */
#define VE_MPCTRL_RENDER    0x3   /* start rendering a frame */
#define VE_MPCTRL_SWAP      0x4   /* display a frame */

#define VE_MPCTRL__ARGSZ    256  /* maximum size of argument string */
typedef struct vemp_ctrl_msg {
  char arg[VE_MPCTRL__ARGSZ];
} VeMPCtrlMsg;

typedef struct vemp_response_msg {
  int result;
} VeMPRespMsg;

#define VE_MPTARG_ALL    (-1)

int veMPAddIntHandler(int msg, int tag, VeMPIntDataHandler handler);
int veMPIntPush(int target, int msg, int tag, void *data, int dlen);

/** function veMPIsMaster
    A predicate function that tests whether or not the current process
    is the master.

    @returns
    1 if this is the master process, 0 otherwise.
*/
int veMPIsMaster(void);

/** function veMPGetSlaveGuard
    Returns the current state of the slave guard (on or off).  When the
    slave guard is on (the default), then a number of VE functions are
    limited to "master-only" use - that is, when they are called on a slave,
    they act as no-ops.  Applications can control this behaviour with the
    <code>veMPSetSlaveGuard()</code> function.  The goal of this mechanism is
    to limit the need for the use of <code>veMPIsMaster()</code> in application
    code.  Functions that are guarded include:
    <ul>
    <li><code>veAddTimerProc()</code></li>
    <li><code>veDeviceAddCallback()</code></li>
    <li><code>veDeviceApplyEvent()</code></li>
    <li><code>veDeviceInsertEvent()</code></li>
    <li><code>veDeviceProcessEvent()</code></li>
    <li><code>veDevicePushEvent()</code></li>
    <li><code>veDeviceUse()</code></li>
    <li><code>veDeviceUseByDesc()</code></li>
    <li><code>veMPDataPush()</code></li>
    <li><code>veMPGetResponse()</code></li>
    <li><code>veMPLocationPush()</code></li>
    <li><code>veMPPushStateVar()</code></li>
    </ul>
    Specific implementations may also guard other functions.  For example,
    in the OpenGL implementations, the following functions are also guarded:
    <ul>
    <li><code>veiGlSetPreDisplayCback</code></li>
    <li><code>veiGlSetPostDisplayCback</code></li>
    </ul>
 */
int veMPGetSlaveGuard(void);

/** function veMPSetSlaveGuard
    Sets the state of the slave guard (see <code>veMPGetSlaveGuard()</code>).
    A non-zero value turns the slave guard on, a zero value turns it off.

    @param state
    The new state of the slave guard.
 */
void veMPSetSlaveGuard(int state);

/** function veMPTestSlaveGuard
    Returns a non-zero value if the guard is active and this is a slave
    process.
*/
int veMPTestSlaveGuard(void);

/** function veMPId
    Returns a unique id (within this VE application) of the current
    process.  This can be used to uniquely identify a process within
    VE.  Applications can assume that this value (say <i>i</i>) is such
    that 0 &lt;= <i>i</i> &lt; <code>veMPNumProcesses()</code>, but
    should not assume a specific meaning of any one value.  Specifically,
    one should not assume that id "0" is the master.  Use the
    <code>veMPIsMaster()</code> call instead.

    @returns
    The MP id of this process, or -1 if this is an asynchronous slave.
*/
int veMPId(void);

/** function veMPIsAsync
    Determines whether or not the current process is an asynchronous
    slave.  This is always false on the master process, but should not
    be used as a test to see if this is the master process.  See
    <code>veMPIsMaster()</code> instead.

    @returns
    1 if this process is an asynchronous slave, or 0 otherwise.
 */
int veMPIsAsync(void);

/** function veMPNumProcesses
    @returns
    The number of processes in this VE application.  Note that this
    will be a tight upper-bound, but not necessarily exact.  In situations
    where slaves can tolerably vanish, this value may be inexact.
 */
int veMPNumProcesses(void);

/** function veMPCleanup
    Attempts to cleanly shutdown the multi-processing subsystem and terminate
    any slave processes.  The MP module will generally set this up to be
    called on exit.
*/
void veMPCleanup(void);

/** function veMPSendCtrl
    A convenience function for sending control messages.  Control messages
    are internal to the library and should not be sent by applications.
    You have been warned.

    @param tag
    An appropriate control tag.

    @param arg
    The string argument for the control message.

    @returns
    0 on success, non-zero on failure.
*/
int veMPSendCtrl(int target, int tag, char *arg);

/** function veMPRespond
    Used by slaves to respond to a request.
    
    @param result
    Typically 0 to indicate success, or non-zero to indicate a failure
    of some kind.
*/
int veMPRespond(int result);

/** function veMPGetResponse
    Reads a response from the slave (or all slaves).  Note that only
    synchronous slaves can return responses.  For asynchronous slaves,
    communication is strictly unidirectional.  An attempt to get a response
    from an asynchronous slave will silently succeed.

    @param target
    If <code>VE_MPTARG_ALL</code> then responses are collected from all
    synchronous targets.
    The result is then "success" if all targets report success,
    otherwise failure (i.e. if at least one reports failure, then the
    result is failure).  Otherwise this is the id of the target from which
    to get a response.

    @returns
    The result returned from the slave - typically 0 to indicate success,
    or non-zero to indicate a failure of some kind.  An attempt to read a
    response from an asynchronous slave will always return 0.
 */
int veMPGetResponse(int target);

/* external OS-layer functions - not to be called from 
   any application */
/* e.g. for setting up local and remote processes... */
int veMP_OSSlaveInit(int *argc, char **argv, int *recv_h, int *send_h);
int veMP_OSSlaveSpawn(int id, char *method, char *node, char *process, int argc, char **argv, 
		      int *recv_h, int *send_h);
/* second stage of spawning */
int veMP_OSSlavePrepare(int id, char *method, char *node, char *process,
			int *recv_h, int *send_h);
int veMP_OSSend(int h, int msg, int tag, void *data, int dlen);
int veMP_OSRecv(int h, int *msg, int *tag, void **data, int *dlen);
int veMP_OSClose(int h);
int veMP_OSSetWD(char *dir);
char *veMP_OSGetWD(void);
void veMP_OSCleanup(void);
int veMP_OSReconnect(int tag, void *data, int dlen,
		     int *recv_h, int *send_h);
int veMP_OSSysdep(int tag, void *data, int dlen, int recv_h, int send_h);

void veMPSetSlaveProp(int id, int recv_h, int send_h);

#endif /* VE_DSYNC_H */
