#ifndef NID_H
#define NID_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if 0
}
#endif

/** misc
    <p>NID (Network Input Device) is a simple protocol for integrating remote
    devices into a VE world.  The NID library is separate from the VE library
    and can be used separately - i.e. there are no dependencies upon the
    VE code itself.  The NID library uses the "veclock" library.

    <p>NID nominally uses TCP to deliver data, although its protocol could be
    easily adapted to any serial transport with reliable delivery.  By default
    NID runs on port 1138 although it can potentially run anywhere.  NID 
    follows a similar scheme to VE input devices.  A NID server provides any
    number of devices.  A device is made up of some number of elements.  Each
    element has one of the following types:
    <ul>
    <li><b>TRIGGER</b> - a "single-shot" switch.  There is no state information.
    A trigger is equivalent to saying "something happened".
    <li><b>SWITCH</b> - a two-state switch.  A switch is always in either an
    active (non-zero value) or inactive state (zero value).
    <li><b>VALUATOR</b> - an element that can have a range of values
    represented as a real number.  A valuator can either be bounded (i.e.
    minimum and maximum values specified) or unbounded (minimum and maximum
    both declared as 0.0 - no limit on values).  For a bounded valuator,
    a NID server must never return a value outside of those limits.  How
    a NID server accomplishes this (e.g. rejection of data or clamping) is
    implementation-specific.  An example of a bounded valuator would be a
    steering wheel - there is limit to how far you can turn the wheel.
    An example of an unbounded valuator would be the integration of a
    bicycle wheel.
    <li><b>VECTOR</b> - some number of valuators that are associated together.
    A vector has a fixed size.  If any valuators in that vector change, then
    the entire vector is reported as having changed.  Vectors are used where
    the data reported by the input device is multi-dimensional and those 
    dimensions are interdependent (e.g. a head-tracker) and reporting such
    changes independently would be erroneous.  Valuators in a vector can
    be bounded or unbounded independently of the limits on other valuators
    in the same vector.
    <li><b>KEYBOARD</b> - a special case of a collection of switches.  Each
    individual button is not reported.  In addition to reporting a state
    change, the index of the button on the keyboard for which the state change
    occurred is also reported.  This index corresponds to a value defined in
    the <code>nid_keysym.h</code> header file.
    </ul>

    <p>The NID protocol in general terms requires that both ends initially
    send a handshake packet upon connecting.  The handshake packet contains
    the protocol version (major and minor revision) that each end speaks.
    If an end is happy with the offered protocol, it should send an "ack"
    packet.  If not, then it should send a "nak" packet.  Then, it should
    wait for the other end's response to the handshake.  After receiving
    the response, if the end had either sent a "nak" in response to the
    handshake, or received a "nak", it should close the connection.  In
    other words, if either side does not accept the handshake, both sides
    should close the connection.

    <p>By default a NID server should send no data.  It is up to the client
    to either poll the state of a device, or request that the server send 
    data when the state of the input device changes.  There is some support
    for throttling the reporting of device changes to avoiding queueing
    up too many changes and thus hurting the latency of input.

    <p>When data is sent over the wire, it is sent in "network-order"
    byte ordering (big-endian).  Real values are sent in their IEEE
    binary format (again using big-endian byte ordering).  The following
    data-types are defined as abstractions:
    <ul>
    <li><code>nid_uint32</code> - an unsigned 32-bit integer
    <li><code>nid_int32</code> - a signed 32-bit integer
    <li><code>nid_real</code> - a 32-bit real number in IEEE format.  On
    most systems, this is just "float".
    </ul>

    <p>Every client-side function is also represented as a library call,
    so clients using this library should not need to explictily send and
    receive packets.
**/

/* protocol version */
#define NID_PROTO_MAJOR          1
/* temporary downgrade - fix before releasing */
#define NID_PROTO_MINOR          3 
#define NID_PROTO_REV            0

/** section Protocol History
    <p>This documentation describes support for the 1.4 revision of the
    NID protocol.  The following is a brief list of changes that have
    been made in various revisions of the protocol:</p>

    <p><b>Revision 1.4</b></p>
    <ul>
    <li>added TIME_PING_PONG packet</li>
    <li>added QUERY_CAP packet</li>
    <li>add support for "REV" portion of protocol.
    A revision (less than a minor step) must have the following properties:
    <ul>
    <li>is protocol compatible with all MAJOR.MINOR servers</li>
    <li>behaviour of any existing exchanges does not change</li>
    <li>new commands may be added with the following catches:</li>
           <ul>
           <li>QUERY_CAP call is used to test if server supports that call</li>
	   <li>client copes cleanly with any revision capability being nak'ed</li>
	   <li>server nak's any unknown requests</li>
	   </ul>
   </ul>
   <p>
   Revisions will allow small changes to the protocol without voiding
   compatibility with older code (1.4 and later only).  
   Note that the "nak any unknown
   request" is stipulated for the first time in 1.4.  With the next minor
   step after 1.4, handshake rules may change (e.g. it may be valid for
   1.5 to handshake okay against 1.4).
   </p>
   </li>
   </ul>
   </p>

   <p><b>Revision 1.3</b> 
   <ul>
   <li>added SET_EVENT_SINK request</li>
   <li>allows redirection of event delivery to other,
   potentially more efficient routes (e.g. UDP)</li>
   </ul>


   <p><b>Revision 1.2</b>
   <ul>
   <li>added COMPRESS_EVENTS, UNCOMPRESS_EVENTS, DUMP_EVENTS requests,
   EVENTS_AVAIL response</li>
   </ul>

   <p><b>Revision 1.1</b> 
   <ul>
   <li>added FIND_DEVICE call</li>
   <li>allow naming of elements (servers should assign arbitrary names if none)</li>
   <li>longer strings for device naming and typing</li>
   </ul>
*/

/* Strings in the protocol are a maximum of this much long */
#define NID_DEVICE_STR_SZ 128

/* default port/service - by no means official */
#define NID_DEFAULT_PORT         1138
#define NID_SERVICE_NAME         "nid"

/* packet types */
/* general packets */
#define NID_PKT_HANDSHAKE        0x0
#define NID_PKT_ACK              0x1
#define NID_PKT_NAK              0x2

/* client requests */
#define NID_PKT_ENUM_DEVICES    0x10
#define NID_PKT_ENUM_ELEMENTS   0x11
#define NID_PKT_QUERY_ELEMENTS  0x12
#define NID_PKT_LISTEN_ELEMENTS 0x13
#define NID_PKT_IGNORE_ELEMENTS 0x14
#define NID_PKT_SET_VALUE       0x15
#define NID_PKT_GET_VALUE       0x16
#define NID_PKT_FIND_DEVICE     0x17
#define NID_PKT_TIME_SYNCH      0x18
#define NID_PKT_COMPRESS_EVENTS 0x19
#define NID_PKT_UNCOMPRESS_EVENTS 0x20
#define NID_PKT_DUMP_EVENTS     0x21
#define NID_PKT_SET_EVENT_SINK  0x22
#define NID_PKT_QUERY_CAP       0x23

#define NID_PKT_DEVICE_FUNC     0x49

/* server responses */
#define NID_PKT_DEVICE_LIST     0x50
#define NID_PKT_ELEMENT_LIST    0x51
#define NID_PKT_ELEMENT_STATES  0x52
#define NID_PKT_ELEMENT_EVENTS  0x53
#define NID_PKT_RETURN_VALUE    0x54
/* this has no payload */
#define NID_PKT_EVENTS_AVAIL    0x55

#define NID_PKT_DEVICE_RESP     0x99

/* element types */
#define NID_ELEM_TRIGGER        0x0
#define NID_ELEM_SWITCH         0x1
#define NID_ELEM_VALUATOR       0x2
#define NID_ELEM_KEYBOARD       0x3
#define NID_ELEM_VECTOR         0x4

/* maximum size of a vector */
#define NID_VECTOR_MAX_SZ   16

/* known values */
#define NID_VALUE_REFRESH_RATE  0x0

/** section Data Types */

/** misc
    Note that the nid_uint32, nid_int32 and nid_real types 
    are provided to abstract out the specific implementation of
    these types.  They should not be assumed to relate to any
    specific type (e.g. "float") in general.
*/

/** type nid_uint32
    An unsigned 32-bit integer.
*/
typedef unsigned nid_uint32;
/** type nid_int32
    A signed 32-bit integer.
*/
typedef int      nid_int32;
/** type nid_real
    A 32-bit signed, real
*/
typedef float    nid_real;


/** struct NidHeader
    All NID packets begin with this header.  Many packets are followed by
    a payload (see the <code>NidPacket</code> structure).
**/
typedef struct nid_header {
  /** member size
      The size of the packet minus 4 bytes (the size of the "size" field).
  */
  nid_uint32 size;
  /** member request
      A number identifying a particular request.  The existing servers do
      not support interleaving of requests, but may in the future.  This
      field is also used to distinguish between state updates and responses
      to an explicit request.
  */
  nid_uint32 request;
  /** member type
      The type of the packet.  The types are defined by the <code>NID_PKT_*</code>
      macros in the <code>nid.h</code> header file.
  */
  nid_uint32 type;
} NidHeader;

/* payloads */

/** typedef struct nid_time_ping_pong
    Used in the ping-pong algorithm for time-synching.  This algorithm
    is meant for millisecond accuracy, although the actual accuracy
    is dependent upon how the time values below are determined.
*/
typedef struct nid_time_ping {
  nid_uint32 k;
  nid_uint32 ct0, ctk;
  nid_uint32 st1, stk;
} NidTimePingPong;

/** struct NidTimeSynch
    Used for synchronizing a Nid server's clock with the client's.  The
    string format for the time is same as is used in the "veclock" library.
*/
typedef struct nid_time_synch {
  nid_uint32 refpt;
  char absolute[NID_DEVICE_STR_SZ];
} NidTimeSynch;

/* device-specific calls */
/** struct NidDeviceFunc
    An extended function call.  Allows for non-standard special functions to
    be accessed on a particular device.  This should only be used for
    cases which are not covered by the NID protocol itself and where such
    functionality is desperately needed.  In other words, the use of 
    special function calls is strongly discouraged.
*/
typedef struct nid_device_func {
  /** member devid
      The identifier of the device.
  */
  nid_uint32 devid;
  /** member func
      The name of the special function to be called.  No pre-defined names
      exist.  These are all specific to the device in question and should
      be considered non-portable.
  */
  char func[NID_DEVICE_STR_SZ];
  /** member args
      Arguments to the special function.  The arguments are represented as
      a string.  The size of the arguments is limited by the size of a string
      in NID.
  */
  char args[NID_DEVICE_STR_SZ];
} NidDeviceFunc;

/** struct NidDeviceResp
    The response to a special function call.  This is effectively an "ack"
    with extra data.  If a function call fails, then a regular NID "nak"
    packet should be sent.
**/
typedef struct nid_device_resp {
  nid_uint32 code;
  char data[NID_DEVICE_STR_SZ];
} NidDeviceResp;

/** struct NidHandshake
    A handshake payload which consists of the major and minor revisions
    of the protocol.  These should be defined in the <code>nid.h</code>
    header file as the <code>NID_PROTO_MAJOR</code> and <code>NID_PROTO_MINOR</code>
    macros.  Note that where supported, the <code>NID_PROTO_REV</code> is
    not used as part of the handshake.
*/
typedef struct nid_handshake {
  nid_uint32 protocol_major;
  nid_uint32 protocol_minor;
} NidHandshake;

/** struct NidNak
    A non-acknowledgement.  This response indicates that a command failed.
*/
typedef struct nid_nak {
  /** member reason
      Optionally a command may include a numeric code indicating why
      it failed.  There are no generally defined values for the reason -
      this is specific to each command.
  */
  nid_uint32 reason;
} NidNak;

/** struct NidEnumElements
    A request to return a list of all elements of a particular device.
*/
typedef struct nid_enum_elements {
  /** member id
      The numeric identifier of the device for which you are requesting
      data.
  */
  nid_uint32 id;
} NidEnumElements;

/** struct NidElementId
    A structure identifying a specific element of a specific device 
    using numeric identifiers.
*/
typedef struct {
  nid_uint32 devid;
  nid_uint32 elemid;
} NidElementId;

/** struct NidElementRequests
    An array of elements identified using NidElementId structures.
    This payload is used to request details of specific device elements.
*/
typedef struct nid_element_requests {
  nid_uint32 num_requests;
  NidElementId *requests;
} NidElementRequests;

/** struct NidFindDevice
    A device name and type specifier used for locating devices
    on the server.  These specifiers are either the name of a particular
    device or type, or an asterisk "*" to denote a wildcard.
*/
typedef struct nid_find_device {
  char name[NID_DEVICE_STR_SZ];
  char type[NID_DEVICE_STR_SZ];
} NidFindDevice;

/** struct NidDeviceInfo
    The basic information of a device returned in response to a FIND_DEVICE
    or ENUM_DEVICES call.
*/
typedef struct nid_device_info {
  /** member devid
      The numeric identifier of the device.  This will be unique within
      a particular server.  It is used as an argument to other calls to
      identifiy the device.
   */
  nid_uint32 devid;
  /** member name
      The name of the device.  If the name of the device is larger than the
      given space in the string, the name will be truncated to fit.  This
      name is unique within a particular server.
  */
  char name[NID_DEVICE_STR_SZ];
  /** member type
      The type of the device.  This is just a string identifying the class
      of the device.  There may be several devices on a server with the same
      type.  Types are not pre-defined in the protocol - they are specific
      to however a server chooses to report a type.  Servers should try to
      keep type names consistent.
  */
  char type[NID_DEVICE_STR_SZ];
} NidDeviceInfo;

/** struct NidDeviceList
    An array of NidDeviceInfo structures.
*/
typedef struct nid_device_list {
  nid_uint32 num_devices;
  NidDeviceInfo *devices;
} NidDeviceList;

/** struct NidElementInfo
    The details of an element, excluding its current state.
*/
typedef struct nid_elem_info {
  /** member name
      The name of the element.  This is unique within a particular device
      but not necessarily between devices - e.g. two devices may have elements
      with the same name.
  */
  char name[NID_DEVICE_STR_SZ];
  /** member elemid
      A numeric identifier for this element.  This identifier is unique
      within a particular device.
  */
  nid_uint32 elemid;
  /** member type
      The type of the element (i.e. trigger, switch, valuator, etc.).
  */
  nid_uint32 type;
  /** member vsize
      If the element is a vector, the size of the vector.  This must not
      change once defined.  Each vector can have its own size, but a vector
      cannot be resized during any particular client's connection.  A
      hard-coded constant (<code>NID_VECTOR_MAX_SZ</code>) limits the
      maximum size of a vector.  If this constant is changed then the
      protocol number should be changed to avoid problems with programs
      expecting a different maximum vector size.
  */
  nid_uint32 vsize;
  /** member min,max
      The bounds on valuators and vectors.  For a valuator, only the first
      item in the array (index 0) is used.  If both min and max are 0.0 for
      a valuator, or for a particular offset in a vector, then that particular
      valuator is unbounded - that is, no explicit limits are set on its
      possible value.
  */
  nid_real   min[NID_VECTOR_MAX_SZ], max[NID_VECTOR_MAX_SZ];
} NidElementInfo;

/** struct NidElementList
    An array of NidElementList structures for a particular device.  
    This is sent in response to an ENUM_ELEMENTS request.
*/
typedef struct nid_elem_list {
  nid_uint32 devid;
  nid_uint32 num_elems;
  NidElementInfo *elements;
} NidElementList;

/** struct NidElementState
    The state of a particular element.  The value of the type field determines
    which of the representations of the "data" union is valid.
*/
typedef struct nid_elem_state {
  /** member timestamp
      The time at which this element state came into being.  This is relative
      to the server's clock which can be synchronized through NID.
      (See the <code>nidTimeSynch()</code> command.)  Otherwise, this value
      is interpreted as a value returned from the <code>veClock()</code>
      command from the veclock library.  This value is in milliseconds.
  */
  nid_uint32 timestamp;
  /** member devid
      The numeric identifier of the device.
  */
  nid_uint32 devid;
  /** member elemid
      The numeric identifier of the element.
  */
  nid_uint32 elemid;
  /** member type
      The type of the element.  This is provided as a convenience.  Its
      type must always match the real type of the element.
  */
  nid_uint32 type;
  /** member data
      The actual state of the element.  Note the use of "switch_" instead
      of "switch" to avoiding clashing with the reserved word "switch" in C.
      For the keyboard element, "code" indicates to which key the given state
      applies.  Key codes are defined in the <code>nid_keysym.h</code> header
      file.
  */
  union {
    nid_uint32 switch_;
    nid_real valuator;
    struct {
      nid_uint32 code;
      nid_uint32 state;
    } keyboard;
    struct {
      nid_uint32 size;
      nid_real data[NID_VECTOR_MAX_SZ];
    } vector;
  } data;
} NidElementState;

/** struct NidStateList
    An array of element states.  Sent in response to a poll or as a 
    requested automatic update.
*/
typedef struct nid_state_list {
  nid_uint32 num_states;
  NidElementState *states;
} NidStateList;

#define NID_SINK_STR_SZ 512

/** struct NidEventSink
    A string representing a new place to which to deliver events.
    See <code>nidSetEventSink()</code>.
*/
typedef struct nid_event_sink {
  char sink[NID_SINK_STR_SZ];
} NidEventSink;

/** struct NidValue
    A server-specific value.  The only currently defined value is 
    <code>NID_VALUE_REFRESH_RATE</code> which determines the maximum
    rate at which the server will send data.
*/
typedef struct nid_value {
  /** member id
      A numeric identifier for the value.
  */
  nid_uint32 id;
  /** member value
      The value itself.
  */
  nid_int32  value;
} NidValue;

/** struct NidPacket
    An actual packet.  Although a packet is sent as a contiguous piece
    of data, it is represented in the library as a header plus a pointer
    to a payload.
*/
typedef struct nid_packet {
  NidHeader header;
  void *payload;
} NidPacket;

/* main client functions */

/** section Client Function-Specific Calls */

/** function nidOpen
    Opens a connection to a NID server and attempts a handshake.
    
    @param host
    The name of the host to which to connect.  This must be specified.
    @param port
    The port number to connect to on the remote host.  If this value is
    less than or equal to 0 then the default NID port is used.
    @returns
    If the connection or the handshake fails, then a value less than 0
    is returned.  In this case, the connection is not open.
    Otherwise a value greater than or equal to 0 is returned.  This is
    an identifier for this particular connection and must be specified
    as an argument to other calls to the client library.
*/
int nidOpen(char *host, int port);

/** function nidRegister
    Registers a given file descriptor as a NID connection.  The effect
    is the same as the <code>nidOpen()</code> call except that the connection
    is established elsewhere.  This function will attempt a handshake on
    the given connection.
    
    @param fd
    The file descriptor upon which to attempt a NID connection.
    @returns
    If the handshake fails, then a value less than 0 is returned.
    The given file descriptor is not closed or otherwise affected.
    If the handshake succeeds then a value greater than or equal to 0 is
    returned.  This is an identifier for this particular connection and
    must be specified as an argument to other calls to the client library.
*/
int nidRegister(int fd); /* like nidOpen, but socket opened elsewhere */

/** function nidClose
    Closes an existing NID connection returned from either <code>nidOpen()</code>
    or <code>nidRegister()</code>.

    @param conn
    The identifier of the connection to close as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
*/
void nidClose(int conn);

/** function nidLastError
    Returns the last error state of a NID connection.  A value of 0 indicates 
    that no error state has been seen, a value of -1 indicates an internal 
    error, and any other value is a value returned as a reason in a "nak" 
    packet.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @returns
    The last error state of the NID connection identified by <b>conn</b>.
*/
int nidLastError(int conn);

/** function nidLastErrorMsg
    The text associated with the last error state (see <code>nidLastError()</code>).
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @returns
    The last error text.  A valid pointer to a null-terminated string is
    always returned, although the text may not be meaningful if no error
    state has been seen.
*/
char *nidLastErrorMsg(int conn);

/** function nidEnumDevices
    Retrieves a list of all devices on a NID server.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @returns
    An array of device identifiers as a <code>NidDeviceList</code> structure.
    This array must be freed with <code>nidFreeDeviceList()</code> when the
    client is done with it.
*/
NidDeviceList *nidEnumDevices(int conn);

/** function nidEnumElements
    Retrieves a list of elements for a given device.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param dev
    The numeric identifier of the device for which to return the list of
    elements.
    @returns
    An array of element identifiers as a <code>NidElementList</code>
    structure.  This array must be freed with <code>nidFreeElementList()</code>
    when the client is done with it.
*/
NidElementList *nidEnumElements(int conn, int dev);

/** function nidQueryElements
    Retrieves the state of the specified set of elements.  The elements can
    be from different devices or the same device.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param ids
    An array of <code>NidElementId</code> structures identifying the elements
    which should be queried.
    @param nids
    The number of structures in the array given by <b>ids</b>.
    @returns
    An array of element states as a <code>NidStateList</code> structure.
    This array must be freed with <code>nidFreeStateList()</code> when the
    client is done with it.
*/
NidStateList *nidQueryElements(int conn, NidElementId *ids, int nids);

/** function nidListenElements
    Requests that the NID server automatically send updates for the given
    elements.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param ids
    An array of <code>NidElementId</code> structures identifying the elements
    which should be listened to.
    @param nids
    The number of structures in the array given by <b>ids</b>.
    @returns
    A value of 0 on success, and a non-zero value on failure.
*/
int nidListenElements(int conn, NidElementId *ids, int nids);

/** function nidIgnoreElements
    Requests that the NID server should stop sending automatic updates for 
    the given elements.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param ids
    An array of <code>NidElementId</code> structures identifying the elements
    which should be ignored.
    @param nids
    The number of structures in the array given by <b>ids</b>.
    @returns
    A value of 0 on success, and a non-zero value on failure.
*/
int nidIgnoreElements(int conn, NidElementId *ids, int nids);

/** function nidFindDevice
    Attempts to locate a particular device or set of devices on the server.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param name
    The exact name of the device or "*" to indicate any device.
    @param type
    The exact type of the device or "*" to indicate any type of device.
    @returns
    An array of matching devices as a <code>NidDeviceList</code> structure.
    This array must be freed with <code>nidFreeDeviceList()</code> when the
    client is through with it.
*/
NidDeviceList *nidFindDevice(int conn, char *name, char *type);

/** function nidNextEvents
    Reads the next automatic update of events on the given connection.
    If there are none currently available, this function will block until
    events are available, depending upon the value of the "wait" parameter.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    
    @param wait
    This parameter affects how the function behaves if there are no events
    currently available.  If 0, then the function returns immediately.  If
    non-zero, then the function will wait until events are available and
    return the first packet of events.

    @returns
    An array of element states as a <code>NidStateList</code> structure.
    This array must be freed with <code>nidFreeStateList()</code> when the
    client is done with it.  If no events are available and wait is 0, then
    <code>NULL</code> is returned.
*/
NidStateList *nidNextEvents(int conn, int wait);

/** function nidSetValue
    Sets a server-specific configuration value.  Currently the only defined 
    value is
    <code>NID_VALUE_REFRESH_RATE</code>.  This is a value in Hz which
    indicates the maximum rate at which updates should be automatically
    sent to the client.  If events are generated on the server faster than
    the refresh rate, then the events will be batched and delayed to meet
    the refresh rate.  A higher refresh rate will reduce the latency of
    input but increase the overhead of communication.  High refresh rates
    may increase latency by overwhelming communication between the client
    and server.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param id
    The numeric identifier of the value to set.  Currently the only
    defined value is <code>NID_VALUE_REFRESH_RATE</code>.  Other servers
    may define other values.

    @param val
    The new value.  Currently values can only be integers.
    
    @returns 
    A value of 0 on success, and a non-zero value on failure.
*/
int nidSetValue(int conn, nid_uint32 id, nid_int32 val);

/** function nidGetValue
    Retrieves a server-specific configuration value.  See
    <code>nidSetValue()</code> for details of known value identifiers.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param id
    The numeric identifier of the value to set.  Currently the only
    defined value is <code>NID_VALUE_REFRESH_RATE</code>.  Other servers
    may define other values.
    @param val_ret
    A pointer to an integer where the retrieved value will be stored.
    Currently values can only be integers.    

    @returns
    A value of 0 on success, and a non-zero value on failure.
 */
int nidGetValue(int conn, nid_uint32 id, nid_int32 *val_ret);

/** function nidTimePingPong
    Called from a client, initiates a ping-pong time synchronization
    to eliminate the offset between the server's clock and the client's
    clock.  Note that the work "clock" in this case refers to the 
    NID software's view of time and does not affect any actual clock in
    the computer.  Also note that this algorithm does not attempt to
    correct for any variance in the rate of the clocks of the two machines.
    Once an offset has been determined, that offset is used to generate
    a time synchronization packet that is then sent to the server.
    <p>This function does not typically need to be called by an application
    using the NID library as this synchronization is done automatically at
    connection time in the 1.4 protocol and later.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.

    @returns
    A value of 0 on success, and a non-zero value on failure.
*/
int nidTimePingPong(int conn);

/** function nidTimeSynch
    Provides a clock reference point to the NID server.  The NID server
    should use this reference point to adjust the clock it uses to
    generate timestamps on automatically reported events.
    The default NID server just passes this information to its
    instance of the veclock library.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param ref
    The reference point in clock time which the given string
    represents.
    @param absolute
    The wall clock time to which the reference point corresponds.
    The format of this string is the same as is used in the veclock
    library.
*/
void nidTimeSynch(int conn, nid_uint32 ref, char *absolute);

/** function nidCompressEvents
    Requests that any events on the server should be buffered and
    compressed by the server.  Events are compressed by combining adjacent
    valuator events ("adjacent" meaning that they are separated only by
    valuator events from any device/element) into a single event.  Instead
    of streaming events, the server will only send an <code>EVENTS_AVAIL</code>
    packet when it has new events in its buffer.  The stored events can be
    retrieved using a <code>DUMP_EVENTS</code> request.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    
    @returns
    0 if the request is sucessful, non-zero otherwise.
*/
int nidCompressEvents(int conn);

/** function nidUncompressEvents
    Requests that the server no longer compress events and return them
    as they occur (i.e. in old-fashioned streaming mode).  See
    <code>nidCompressEvents()</code>.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.

    @returns
    0 if the request is sucessful, non-zero otherwise.
*/
int nidUncompressEvents(int conn);

/** function nidDumpEvents
    Requests that the server send any compressed events it may have buffered.
    The response will be an <code>ELEMENT_STATES</code> packet.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.

    @returns
    The set of states returned, or <code>NULL</code> if there is an
    error, or no states are available.
*/
NidStateList *nidDumpEvents(int conn);

/** function nidSetEventSink
    Requests that the server redirect any automatically delievered events
    to the given connection, described as a string.
    Control remains with the original connection.  This will redirect
    all automatic updates to the new sink.  The following strings are
    recognized - others may be supported on specific implementations.
    <ul>
    <li><b>default</b> - deliver events on control connection</li>
    <li><b>tcp</b> <i>host</i> <i>port</i> - make a new TCP connection 
    to the given host and port</li>
    <li><b>udp</b> <i>host</i> <i>port</i> - make a new UDP connection
    to the given host and port</li>
    </ul>

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    
    @param sink
    A string describing the new sink to which the server should send events.

    @returns
    0 if the request is sucessful, non-zero otherwise.
*/
int nidSetEventSink(int conn, char *sink);


/** function nidChangeTransport
    This is a higher-level wrapper for <code>nidSetEventSink()</code> which
    creates the necessary network objects internally.  Most client code
    will use this function instead of <code>nidSetEventSink()</code>.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.

    @param tport
    One of
    <ul>
    <li>"default" - use the control connection</li>
    <li>"tcp" - use a new TCP connection</li>
    <li>"udp" - use a new UDP connection</li>
    </ul>

    @returns
    0 if the request is sucessful, non-zero otherwise.
*/
int nidChangeTransport(int conn, char *tport);

/** function nidProcessTransport
    Called on the server side to reconfigure a connection for the
    requested event transport.  Client code should never call this.
    
    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.

    @param tport
    The transport/sink string sent by the client in a 
    <code>NID_PKT_SET_EVENT_SINK</code> packet.
    
    @returns
    0 if the request is sucessful, non-zero otherwise.
*/
int nidProcessTransport(int conn, char *tport);

/** function nidFreeDeviceList
    Frees memory associated with a NidDeviceList structure allocated by
    the NID library.  After freeing, the passed pointer will no longer be
    valid and should not be de-referenced.
    
    @param l
    The structure to free.
*/
void nidFreeDeviceList(NidDeviceList *l);

/** function nidFreeElementList
    Frees memory associated with a NidElementList structure allocated by
    the NID library.  After freeing, the passed pointer will no longer be
    valid and should not be de-referenced.
    
    @param l
    The structure to free.
*/    
void nidFreeElementList(NidElementList *l);

/** function nidFreeStateList
    Frees memory associated with a NidStateList structure allocated by
    the NID library.  After freeing, the passed pointer will no longer be
    valid and should not be de-referenced.
    
    @param l
    The structure to free.
*/    
void nidFreeStateList(NidStateList *l);

/** function nidFreeElementRequests
    Frees memory associated with a NidElementRequests structure allocated by
    the NID library.  After freeing, the passed pointer will no longer be
    valid and should not be de-referenced.
    
    @param l
    The structure to free.
*/    
void nidFreeElementRequests(NidElementRequests *l);

/** function nidFreePacket
    Frees a NidPacket structure and any payload associated with a packet.
    If you wish to preserve the payload of a packet, but free the packet 
    itself, save the pointer to the payload somewhere else and set the
    <code>payload</code> member of the packet structure to <code>NULL</code>
    before calling this function.

    @param p
    The packet to free.
*/
void nidFreePacket(NidPacket *p);

/* low-level calls - typically used by servers */

/** section Low-Level Calls */

/** function nidReceivePacket
    Retrieves the next packet from the given connection.  This function
    generally only needs to be used by servers or clients that wish to
    access the library at a lower-level.  Where possible, clients should
    use the function-specific calls.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param request
    If this value is greater than or equal to 0, then incoming packets will
    be discarded until one arrives with a request field in its header that
    matches this parameter.  If this value is less than 0, then the next packet
    that arrives is returned regardless of its request field.
    <p>Please note that a request value of 0 is reserved for automatic
    event updates.

    @returns
    A pointer to the decoded packet.  This structure needs to be freed with
    a call to <code>nidFreePacket</code> when the program is finished with it.
*/
NidPacket *nidReceivePacket(int conn, int request); /* req<0 means next packet,
						    regardless of request */

/** function nidTransmitPacket
    Transmits a packet over a NID connection.  This function takes care
    of any byte-ordering issues - the given packet should be in local
    machine byte-order.  This function generally only needs to be called
    by servers and clients who wish to access the library at a low-level.
    Where possible, clients should use the function-specific calls.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @param pkt
    A pointer to the packet to transmit.
    
    @returns
    0 on success, non-zero on failure.
 */
int nidTransmitPacket(int conn, NidPacket *pkt);

/** function nidConnectionFd
    Returns the system file descriptor for the stream corresponding to the
    given NID connection.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @returns
    The system file descriptor as an integer.  On UNIX-like systems this
    should always be a value greater than or equal to zero.
*/
int nidConnectionFd(int conn);

/** function nidEventFd
    Returns the system file descriptor for the stream corresponding to the
    given NID connection's transport for events.  This may or may not be
    same as the result returned by <code>nidConnectionFd()</code>.  All
    asynchronous events should be sent via this file descriptor.

    @param conn
    The identifier of the connection as returned from either 
    <code>nidOpen()</code> or <code>nidRegister()</code>.
    @returns
    The system file descriptor as an integer.  On UNIX-like systems this
    should always be a value greater than or equal to zero.
*/
int nidEventFd(int conn);

/** function nidLogToSyslog
    By default, NID logs any messages to the stream associated with
    stderr.  Optionally, NID can send messages to the system
    log (where available).

    @param bool
    If non-zero, then any further messages will be sent to the system log.
    If zero, then any further messages will be sent to stderr.
*/
void nidLogToSyslog(int bool); /* call with 1 to send message to syslog() */

/*
  This function is deliberately undocumented with cdoc-style comments.
  It should not be called from outside the library, but is exposed in
  this header file for easy access from various NID modules.
*/
void _nidLogMsg(char *fmt, ...); /* internal call for logging messages */

/* 
   Note - these calls are deliberately undocumented with cdoc-style comments
   as they should never be used from outside of the library.  
   They are used for porting the core NID transport code from one 
   platform to another.
*/
#define NID_TPORT_TCP 0
#define NID_TPORT_UDP 1
int nidOsOpenSocket(char *host, int port);
int nidOsCloseSocket(int fd);
int nidOsWritePacket(int tport, int fd, char *data, int len);
char *nidOsNextPacket(int tport, int fd, int *len, int wait);
int nidOSReconnect(int conn, int fd, char *tport);
int nidOSHandleReconnect(char *tport);
nid_uint32 nidOs_h2n_uint32(nid_uint32);
nid_uint32 nidOs_n2h_uint32(nid_uint32);
nid_int32 nidOs_n2h_int32(nid_int32);
nid_int32 nidOs_h2n_int32(nid_int32);
nid_real nidOs_n2h_real(nid_real);
nid_real nidOs_h2n_real(nid_real);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NID_H */
