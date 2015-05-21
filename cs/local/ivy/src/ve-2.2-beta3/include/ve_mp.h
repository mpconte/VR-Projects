#ifndef VE_MP_H
#define VE_MP_H

#include <ve_version.h>

#ifdef __cplusplus
extern "C" {
#endif

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

    <p>The MP layer has been reworked significantly for VE 2.2.  In particular
    slaves are now created opportunistically using the <code>veMPGetSlave()</code>
    call to retrieve connection information for slaves.  This makes the whole layer
    much more flexible and less dependent upon a particular purpose 
    (e.g. the rendering subsystem).
*/    

#define VE_DTAG_ANY (-1)
#define VE_DMSG_ANY (-1)

#define VE_MP_SELF  (-1)

/** type VeMPDataHandler
    The type for an application MP callback.  This allows functions
    in the application to be called for registered message types.
    (see veMPDataAddHandler).
*/
typedef void (*VeMPDataHandler)(int tag, void *data, int dlen);

/** struct VeMPPkt
    Describes a single communication packet (in either direction).
 */
typedef struct ve_mp_impl_pkt {
  /** member seq
      A monotonically increasing sequence number.  These are generally
      assigned and interpreted by the platform-independent layer.  The
      implementation layer should pass them unchanged.
   */
  unsigned long seq;   /* sequence number */
  /** member ch
      Specifies the channel over which this data should be sent.  
      Every implementation must provide at least a <code>VE_MP_RELIABLE</code>
      channel (see below).  For
      the sending side of an implementation, if the specified channel 
      cannot be used or does not exist, then the <code>VE_MP_RELIABLE</code>
      channel must be used.  For the receiving side, the channel over which
      this packet was received should be specified here.
   */
  int ch;              /* channel we came in/out on */
  /** member msg
      The message type id (as specified in the platform-independent
      MP interface).
   */
  int msg;             /* message id */
  /** member tag
      The tag id (as specified in the platform-independent MP interface).
   */
  int tag;             /* tag id */
  /** member dlen
      The size of the data buffer in bytes.  This is the size of
      <em>meaningful</em> data in the buffer not necessarily the size
      of the space allocated for the buffer.  If this value is 0, then
      <code>data</code> may be <code>NULL</code>.  If this value is
      &gt; 0 then <code>data</code> must <em>not</em> be <code>NULL</code>
      (except in one special case - see <code>veMPPktDestroy()</code> below.
   */
  int dlen;            /* size of meaningful data in buffer */
  /** member data
      The buffer containing the payload of the packet.  This may be
      <code>NULL</code> if the size of the payload (<code>dlen</code>
      field) is 0.  The sending side of an implementation should not
      assume that this piece of memory is safe beyond the call to the
      <code>veMPImplSend()</code> function.  The receiving side of an
      implementation should always allocate a new piece of memory here
      and allow the library/application to handle the freeing of the
      memory at a later date.
   */
  void *data;          /* data buffer */
} VeMPPkt;

/** type VeMPPktHandler
    The "new"-style callback for internal data handlers.  It replaces
    the <code>VeMPIntDataHandler</code> type which is no longer supported.
    <p>In general, <i>src</i> identifies what the source of the message
    is.  For slave callbacks, <i>src</i> is always 
    <code>VE_MPTARG_MASTER</code>.  On master
    callbacks, <i>src</i> identifies the slave from which the packet
    was received.
 */
typedef void (*VeMPPktHandler)(int src, VeMPPkt *p);

/** function veMPGetSlave
    When called on the master, returns the identifier for the given slave.
    If no connection for the given slave exists then VE attempts to establish
    one.  For any given slave connection (identified by a 
    (<i>node</i>,<i>process</i>) pair) once a connection is established its
    identifier will not change for the duration of the program's execution.
    Thus, it is safe and valid for calling procedures to cache the return value
    of this function for a particular (<i>node</i>,<i>process</i>) pair.

    <p>Currently the VE communication system does not allow for communication
    between slaves.  On slaves, this function will always indicate failure.

    @param node
    The name of the node on which the slave should reside.  The value 
    <code>"auto"</code> or the <code>NULL</code> pointer here can be used
    to indicate the node on which the master is running.

    @param process
    The name of the process in which the slave should reside.  Process
    names are abstract.  Slaves that share the same process name on the
    same node will run within the same process.  The value <code>"auto"</code>
    is valid for all nodes.  For the master node, <code>"auto"</code> refers
    to the master process.  So an MP setup in which all slaves have nodes
    set to <code>"auto"</code> and processes set to <code>"auto"</code> is
    actually a single process (but possibly multi-threaded) program.
    The <code>NULL</code> pointer can be used in place of <code>"auto"</code>.
    <p>The reserved name <code>"unique"</code> can be used here to indicate
    that a process that is not shared by any other slave should be created.
    Note that in this case the calling procedure must save the returned
    identifier - a subsequent call to this function with the process name
    <code>"unique"</code> will generate a new, different slave process.

    @param allow_fail
    If this is 0, then failure to establish a connection to a slave will be
    considered a fatal error.  If this is non-zero then failure to
    establish a connection will not print an error message and return
    control to the calling procedure (with an error code).

    @params
    A slave connection identifier which is &gt;= 0 on success, or
    a value &lt; 0 on error.
 */
int veMPGetSlave(char *node, char *process, int allow_fail);

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

/** function veMPEnvPush
    Pushes the environment information to the slaves.
    This is typically done automatically by VE's initialization.
    The environment data is always pushed over the <code>VE_MP_RELIABLE</code>
    interface.

    @returns
    0 on success, non-zero on failure.
 */
int veMPEnvPush(void);

/** function veMPProfilePush
    Pushes the user profile information to the slaves.
    This is typically done automatically by VE's initialization.
    The profile data is always pushed over the <code>VE_MP_RELIABLE</code>
    interface.

    @returns
    0 on success, non-zero on failure.
 */
int veMPProfilePush(void);

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
/* 0x5 - was VE_MPMSG_RESPONSE - now obsolete */
/* 0x6 - was VE_MPMSG_RECONNECT - now obsolete */
#define VE_MPMSG_STATE     0x7   /* state information message */
#define VE_MPMSG_INIT      0x8   /* slave initialization message */

/* new MP services - these values are reserved here but the 
   appropriate modules must register themselves.  Values are defined
   here to help avoid collisions between different modules. */
#define VE_MPMSG_RENDER    0x20  /* MP graphics rendering module */
#define VE_MPMSG_AUDIO     0x21  /* MP audio rendering module */

/* All values VE_MPMSG__UNRESV are explicitly defined as not
   used by any internal VE module - they are free for applications
   and external modules to use. */
#define VE_MPMSG__UNRESV   0x40

#define VE_MPMSG__SYSDEP   0x7f  /* system-dependent - used by specific
				    MP implementations when they need some
				    non-standard communication */

#define VE_MPTARG_ALL    (-1)
#define VE_MPTARG_MASTER (-2)

/* Internal functions.  Applications should not use these. */
int veMPAddMasterHandler(int msg, int tag, VeMPPktHandler handler);
int veMPAddSlaveHandler(int msg, int tag, VeMPPktHandler handler);
int veMPIntPush(int target, int ch, int msg, int tag, void *data, int dlen);
int veMPIntPushToMaster(int ch, int msg, int tag, void *data, int dlen);

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

/** function veMPNumProcesses
    @returns
    The number of processes in this VE application.  Note that this
    will be a tight upper-bound, but not necessarily exact.  In situations
    where slaves can tolerably vanish, this value may be inexact.
 */
int veMPNumProcesses(void);

/** function veMPSlaveProcess
    @param k
    The slave identifier, must be &gt;= 0 and &lt; 
    <code>veMPNumProcesses()</code>.  This function is only
    valid on the master node.  This function exists primarily for
    use during initialization of MP sub-modules.
    <p>On slave nodes, the function is meaningful if called with the
    argument <code>VE_MP_SELF</code> in which case, the process name
    of the calling slave will be returned.

    @returns
    A read-only string containing the name of this slave's process.
    For invalid slave identifiers, the result will be <code>NULL</code>.
 */
char *veMPSlaveProcess(int k);

/** function veMPSlaveNode
    @param k
    The slave identifier, must be &gt;= 0 and &lt; 
    <code>veMPNumProcesses()</code>.  This function is only
    valid on the master node.  This function exists primarily for
    use during initialization of MP sub-modules.
    <p>On slave nodes, the function is meaningful if called with the
    argument <code>VE_MP_SELF</code> in which case, the node name
    of the calling slave will be returned.

    @returns
    A read-only string containing the name of this slave's node.
    For invalid slave identifiers, the result will be <code>NULL</code>.
 */
char *veMPSlaveNode(int k);

/** function veMPCleanup
    Attempts to cleanly shutdown the multi-processing subsystem and terminate
    any slave processes.  The MP module will generally set this up to be
    called on exit.
*/
void veMPCleanup(void);

/** function veMPSendMsg
    A generic message send function.  This is only meant for internal
    use in VE and should not be used by applications.  It supercedes
    <code>veMPSendCtrl()</code> (which is no longer available).

    @param ch
    The channel over which to send.  Must be <code>VE_MP_RELIABLE</code>
    or <code>VE_MP_FAST</code>.

    @param target
    The slave id to which to send the message.  There are two reserved
    values:
    <ul>
    <li><code>VE_MPTARG_ALL</code> - is a broadcast from the master to
    all slaves</li>
    <li><code>VE_MPTARG_MASTER</code> - for the special case of a slave
    sending a message to the master</li>
    </ul>

    @param msg
    The message-type identifier.
    
    @param tag
    The message tag.

    @param data
    The body of data.  This value can be <code>NULL</code> iff 
    <i>dlen</i> is 0.

    @param dlen
    The size of the body of data in bytes.

    @returns
    0 if successful, non-zero on failure.
 */
int veMPSendMsg(int ch, int target, int msg, int tag, void *data, int dlen);

/** function veMPRenderFrame
    Gets the MP layer to render a frame by communicating with
    slaves.

    @param frame
    Specifies the frame number to generate.  If this value is
    negative, then a frame number will be chosen randomly.
    Unless you really know what you are doing, you should generally
    not specify a frame number - i.e. always pass '-1' as this value.
 */
void veMPRenderFrame(long frame);

/** section MP Rendering
    <p>
    This section is an extension to existing control messages.  The
    MP rendering has been redesigned from the simple VE 2.1 interface.
    This change requires that all rendering implementations (library 
    implementations, not applications) be updated to deal with the new
    control messages.  This spec extends the <code>RENDER</code> and
    <code>SWAP</code> messages.
    </p>
    <p>The master and all slaves maintain their own frame counter which
    should be initialized to 0.  In addition, each slave will keep a 
    <code>swapped</code> flag which is initialized to 1.
    The first frame that will be actually
    rendered will be frame 1.  The master may re-initialize the frame counter
    to avoid overflow at anytime.  However, it is guaranteed that two
    subsequent frames will never share the same frame number.  (This guarantee
    may be extended in the future to gurantee uniqueness over <i>n</i> frames
    if necessary.)  The idea is that the master sends a steady stream of
    <code>RENDER</code> and <code>SWAP</code> messages with ever increasing
    (and occasionally resetting) frame numbers.  Slaves respond to these
    packets with information about their own state.  If a packet represents
    a "future" state for the slave, then the slave advances to that state
    (if possible) and updates its own state accordingly.  This way, if a
    message is lost or delivered out-of-order, the system should hold
    together.</p>
*/

/** section MP Implementation
    <p>
    The following section describes the implementation-abstraction interface
    for multi-processing under VE.  This interface is not used by applications
    and developers using VE typically do not need to consider the following
    information.  However, developers extending or porting VE should be
    aware of the following interface.
    </p>
    <p>The core ve_mp module is platform-independent.  The low-level details
    of how to instantiate slaves and the transport of data between nodes
    is left up to the implementation layer.  All implementation layers should
    provide the following interface.</p>
 */
/* new implementation layer interface */

/** subsection Packets
    This section describes the abstract structure of packets that are
    sent on the wire.  Implementations are free to encode the packets in
    anyway they see fit and may add information as needed, but must present
    the data in the following structure to the platform-independent level.
*/


/** function veMPPktCreate
    Creates a new packet with appropriately allocated data space.

    @param dlen
    The size of the data buffer to allocate.  If this is &lt;= 0
    then no data buffer is allocated (<code>data</code> member will be
    <code>NULL</code>).

    @returns
    A newly allocated packet structure.  Except for 
    <code>dlen</code> and <code>data</code>, all other fields will
    be initialized to '0' (or its equivalent).
 */
VeMPPkt *veMPPktCreate(int dlen);
/** function veMPPktDestroy
    Frees a previously allocated packet.  This is the one case where it
    is valid for a packet to have a <code>NULL</code> value for the
    <code>data</code> member, but a non-zero value for the <code>dlen</code>
    member.  This allows a calling function to preserve the buffer space of
    the packet without copying its contents by saving the pointer elsewhere
    and setting the <code>data</code> member of the packet to <code>NULL</code>
    (to prevent this function from freeing the buffer).

    @param pkt
    The packet to destroy.  All memory associated with the packet
    (including its buffer space) is freed.  See the description above for
    details on preserving the buffer space.
 */
void veMPPktDestroy(VeMPPkt *p);

/** type VeMPImplConn
    An opaque type representing a connection.  The platform-independent
    layer assumes that this is a pointer-like value, in that it can be
    compared against <code>NULL</code> and that <code>NULL</code> is never
    a valid connection.
 */
typedef void *VeMPImplConn;  /* object type for representing a connection between
				master and slave (both sides) */

/* predefined connection types */
#define VE_MP_REMOTE "remote"   /* refers to a slave on a remote node */
#define VE_MP_LOCAL  "local"    /* refers to a slave on the same node as the master
				   but in a separate process */
#define VE_MP_THREAD "thread"   /* refers to a slave on the same node as the master
				   but in a separate thread */

/* default communication channels */
#define VE_MP_RELIABLE   (0)    /* guaranteed delivery of messages, but at possibly
				   poor performance */
#define VE_MP_FAST       (1)    /* fast performance, possibly lost messages though... */


/* although part of "veMP" this is not an application interface, but
   is instead an interface for the underlying implementation to create
   slave threads as necessary. */
void veMPCreateSlave(VeMPImplConn);

/* create on the slave's side */
VeMPImplConn veMPImplSlaveCreate(int *args, char **argv);
/* create on the master's side */
VeMPImplConn veMPImplCreate(char *type, int id, char *node, int argc, char **argv);
/* prepare a connection - negotiate any further communication channels,
   trade authentication info, etc */
int veMPImplPrepare(VeMPImplConn c, int flags);

int veMPImplSend(VeMPImplConn c, VeMPPkt *p);
/* for conveniences */
int veMPImplSendv(VeMPImplConn c, unsigned long seq, int ch,
		  int msg, int tag, void *data, int dlen);
/* specify a timeout in microseconds here 
   0 = don't wait at all
   -1 = wait forever - don't return until we have a full packet
 */
/** function veMPImplRecv
    Recieves a packet from a connection.

    @param c
    The connection over which to receive a packet - must not be
    <code>NULL</code>.

    @param p_r
    A pointer to a <code>VeMPPkt</code> pointer.  If a packet
    is received successfully then a new packet structure is allocated
    and the pointer to this structure is stored here.  Otherwise,
    this value is set to <code>NULL</code>.

    @param tmout
    A time-out.  If this value is &gt; 0 then the function call
    will wait at most this many <em>micro</em>seconds before returning.
    Note that it may wait longer due to operating-system implementation
    constraints.  If this value is equal to 0 then the function call
    will return immediately if no packet is available.  If this value is
    &lt; 0 then the function will wait indefinitely for a packet (i.e.
    time-out is disabled).

    @returns
    0 on successful reception of a packet, &lt; 0 if an error
    occurred on the connection while reading, or &gt; 0 if the
    function timed-out while waiting for a packet to arrive.
 */
int veMPImplRecv(VeMPImplConn c, VeMPPkt **p_r, long tmout);

/* kill off any remaining connections/resources before exiting */
void veMPImplCleanup(void);

/** function veMPImplSysdep
    System-dependent callback.  Allows implementations to use
    the <code>VE_MPMSG__SYSDEP</code> message type to pass information
    between master and slave when needed.  Usually called by the
    platform-independent MP layer whenever a "SYSDEP" message is
    received.
 */
int veMPImplSysdep(VeMPImplConn c, VeMPPkt *p);

/** function veMPImplWait
    Waits for data to become available on one or more of the
    connections in the set.

    @param c
    An array of size <i>n</i>.  Some entries may be <code>NULL</code>
    (these will be ignored).  This is the input set of connections to
    wait upon.

    @param c_res
    An array of size <i>n</i>.  This array will be updated with the
    set of connections upon which data is available.  After calling
    this function, some or all entries may be <code>NULL</code>.
    Non-<code>NULL</code> entries are those connections with real
    data.  Their positions will be the same as the positions in the
    input array.

    @param n
    The size of the <i>c</i> and <i>c_res</i> arrays.

    @returns
    0 on success, non-zero on failure.
 */
int veMPImplWait(VeMPImplConn *c, VeMPImplConn *c_res, int n);

#ifdef __cplusplus
}
#endif

#endif /* VE_MP_H */
