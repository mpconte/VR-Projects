#ifndef VE_DEVICE_H
#define VE_DEVICE_H

/* The abstract model of a device */
#include <ve_version.h>
#include <ve_error.h>
#include <ve_util.h>

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif /* __cplusplus */

/** misc
    The ve_device module provides a flexible model for input devices.
    Input devices and drivers are loaded at run-time either explicitly
    from a program or from a user-provided manifest.  Input devices
    generate events which can then be filtered, through explicit
    structures or through filters loaded at run-time as part of the
    device settings.  The event process model allows for the use of
    virtual devices - that is, remapping incoming events to arbitrary
    names that allow the application to be built independent of the
    actual input devices and to provide for the adoption of new input
    devices and methods with rebuilding the program.
    <p>
    Devices are represented as collections of elements.  Each element
    represents a particular piece of input data.  For example, a joystick
    may be divided into several elements - each button would be an element,
    an axis or a collection of axes would be another element, and so on.
    An element is uniquely identified by a pair of a device name
    and an element name.
    <p>
    Events that have a similar structure to elements are generated
    representing changes of state.  These events are passed through
    any number of filters as determined by the system's filter table.
    Through a filtering, an event can be modified, renamed (i.e. its
    device and/or its element name changed), duplicated, or discarded.
    <p>
    Finally, events are delivered to the application's callbacks.
    Callbacks can be set to receive specific events or general collections
    of events.
*/

/** section Element Content 
    The core data in an event or an element is called
    <i>element content</i>.
*/

/* element content - used for events and elements */
/** enum VeDeviceEType
    All elements and events use a type field 
    There are several types of element data.  
*/
typedef enum ve_device_etype {
  VE_ELEM_UNDEF = -1,
  /** value VE_ELEM_TRIGGER
      A trigger is an element without state.  It is used to indicate
      an event that occurs.  For example, a vending machine would
      have a trigger to indicate that a quarter has been inserted.
      There is no state to report - simply that a quarter has been
      inserted.
  */
  VE_ELEM_TRIGGER = 0,
  /** value VE_ELEM_SWITCH
      A switch is an element with two states - an active state and
      an inactive state.  An obvious real-world example is a light
      switch which is either "on" or "off".  Switch events occur when
      the switch transitions from one state to another.  Other 
      common examples include joystick and mouse buttons.
  */
  VE_ELEM_SWITCH,
  /** value VE_ELEM_VALUATOR
      A valuator is an element with a range of possible values represented
      as a real number.  A valuator may be either bounded, meaning that
      it has both a minimum and maximum value, or unbounded, meaning that
      any value is possible.  Unbounded valuators are represented as
      having minimum and maximum values that are both 0.0.  A throttle
      control or a steering wheel would be a valuator.
  */
  VE_ELEM_VALUATOR,
  /** value VE_ELEM_VECTOR
      A vector is an array of valuators.  The size of a vector is
      fixed throughout its lifetime.  When a vector is created it may
      be given any size, but thereafter, that size remains fixed.
      Each valuator in the vector has an independent set of bounds.
      Unbounded and bounded valuators can be mixed within the same
      vector.
      <p>Vectors are typically used in cases where a set of valuators
      are interdependent or represent multi-dimensional data and reporting
      changes of individual valuators would be erroneous.  For example,
      a head-tracker will typically have several dimensions of data, and
      any collection of changes in the data should be reported as a change
      to the head-tracker data as a whole.  In other words, it would be
      wrong to report a change in the head's x-axis location and then
      a change in the head's y-axis location when the head had moved along
      a path which caused simultaneous displacements along both axes.
      <p>Filters can be used to isolate individual valuators within a
      vector if so desired.
   */
  VE_ELEM_VECTOR,
  /** value VE_ELEM_KEYBOARD
      A keyboard is a special case of an input device.  It is really
      a collection of switches.  However, the set of switches is generally
      indefinite from the program's point of view.  A keyboard is
      usually considered a device and its individual keys elements.
      Thus there are no keyboard "elements", but there are keyboard events.
      A keyboard event will represent both the state of the switch as
      well as including a code which identifies the key to which the
      event applies.
      <p>General practice is that the name of an element in a keyboard
      device corresponds to the key (if it has an ASCII representation)
      or the name of the key (e.g. Control_L or F1).
  */
  VE_ELEM_KEYBOARD
} VeDeviceEType;


/** struct VeDeviceEContent
    The actual content in an element or device is stored in a structure
    that is modelled after the VeDeviceEContent structure.  All actual
    content structures have the fields defined in VeDeviceEContent in
    common, and these fields always appear first and in the same order
    as in VeDeviceEContent.
    <p>In general, you only use the info in a VeDeviceEContent structure
    to figure out what it really is, and then cast to the type of
    structure that you really want to use.  Note that a VeDeviceEContent
    structure is not guaranteed to be as large as real content structures,
    so you cannot pass VeDeviceEContent structures around by value - it
    must always be passed by reference.
 */
/* VeDeviceEContent is a shadow structure - you need to cast it to the
   right type to get all the fields - VeDeviceEContent may be smaller
   than real instances, so we cannot pass it by value - it must be by
   reference.
*/
/* the type field must always be first */
typedef struct ve_device_econtent {
  /** member type
      The type field determines what type of content we are actually
      dealing with.
   */
  VeDeviceEType type;
} VeDeviceEContent;

/* instances: */
/** struct VeDeviceE_Trigger
    If the type of content is <code>VE_ELEM_TRIGGER</code> then there
    is no effectively no content.  For consistency this structure is
    defined for a trigger, but currently it contains no fields beyond
    the ones in <code>VeDeviceEContent</code>.
*/
typedef struct ve_device_e_trigger {
  VeDeviceEType type;
} VeDeviceE_Trigger;

/** struct VeDeviceE_Switch
    If the type of content is <code>VE_ELEM_SWITCH</code> then this
    structure represents the real content.
*/
typedef struct ve_device_e_switch {
  VeDeviceEType type;
  /** member state
      If non-zero, then the switch is in an "active" state.  If
      zero, then the switch is in an "inactive" state.  Most functions
      will limit the possible values to 1 and 0.
  */
  int state; /* boolean */
} VeDeviceE_Switch;

/** struct VeDeviceE_Valuator
    If the type of content is <code>VE_ELEM_VALUATOR</code> then this
    structure represents the real content.
*/
typedef struct ve_device_e_valuator {
  VeDeviceEType type;
  /** member min,max
      These values represent the bounds of the valuator.  If both values
      are 0.0 then the valuator is unbounded.  A valuator either has both
      a minimum and a maximum or neither - there is no way to specify a
      only one of the two bounds.
   */
  float min,max;
  /** member value
      The current value of the valuator.  The VE library will generally
      constrain this value to be within the bounds (if defined) but these
      conditions may be violated by user-supplied filters.  In cases where
      these limits are critical to the application, filters may be applied
      to clamp values.
  */
  float value;
} VeDeviceE_Valuator;

/** struct VeDeviceE_Vector
    If the type of content is <code>VE_ELEM_VECTOR</code> then this
    structure represents the real data.
 */
typedef struct ve_device_e_vector {
  VeDeviceEType type;
  /** member size
      The size of the vector.  A vector's size should not change after
      it has been created.
  */
  int size;
  /** member min,max
      The bounds of the individual valuators.  Both min and max are
      arrays of size <b>size</b>.  For valuator <i>n</i>, min[<i>n</i>]
      represents the minimum bound, and max[<i>n</i>] represents the 
      maximum bound.  If both min[<i>n</i>] and max[<i>n</i>] are 0.0
      then valuator <i>i</i> of the vector is unbounded.
  */
  float *min, *max;
  /** member value
      The value of the individual valuators in the vector.  
      This is an array of size <b>size</b>. The value of
      valuator <i>n</i> is value[<i>n</i>].
  */
  float *value;
} VeDeviceE_Vector;

/** struct VeDeviceE_Keyboard
    If the type of content is <code>VE_ELEM_KEYBOARD</code> then
    this structure represents the real data.  Note that this
    structure will never be used as an element - only as event
    data.
 */
/* keyboard is used for events but not for elements */
typedef struct ve_device_e_keyboard {
  VeDeviceEType type;
  /** member key
      The code for the key to which this data relates.
      Constants for key codes are defined in the <code>ve_keysym.h</code>
      header files.  All input drivers which provide keyboards should
      conform to these pre-defined constants as much as possible to
      ensure consistency.
   */
  int key;
  /** member state
      As with a switch, a non-zero value indicates an "active" or
      "down" state for the key, and a zero value indicates an "inactive"
      or "up" state.
  */
  int state;
} VeDeviceE_Keyboard;

/** function veDeviceEContentCreate
    Allocates memory for and initializes the given type of content.

    @param type
    The type of content to create.
    
    @param vsize
    If the type of content being created is <code>VE_ELEM_VECTOR</code>
    then this is the size of the vector to create.  Otherwise this
    parameter is ignored.

    @returns
    A pointer to the newly-allocated element content structure.
*/
VeDeviceEContent *veDeviceEContentCreate(VeDeviceEType type, int vsize);

/** function veDeviceEContentCopy
    Creates a duplicate of the given content.  The copy does not share
    any memory with the original but contains the same information.
    
    @param c
    The content to copy.

    @returns
    A newly-allocated element content structure which contains the same
    data as the original.
*/
VeDeviceEContent *veDeviceEContentCopy(VeDeviceEContent *c);

/** function veDeviceEContentDestroy
    Frees a previously allocated veDeviceEContent structure.  Note that
    this frees all memory associated with the element content.
    
    @param c
    The element content structure to free.
*/
void veDeviceEContentDestroy(VeDeviceEContent *c);

/** section Elements and Device Models
    The first place that element content is used is in elements
    and device models.  A <i>device model</i> is an abstract representation
    of a device which shows both its structure and its current state.
    <p>Devices do not require device models, but without a model, the
    application has no information of the structure of a device.
 */

/** struct VeDeviceElement
    An element in a device model is a combination of a name (identifying
    the element) and element content.
 */
typedef struct ve_device_element {
  /** member name
      The name of the element.  Every element must have a name.
  */
  char *name;
  /** member content
      A pointer to the element content structure for this element.
      The type of the element is defined in the content structure.
  */
  VeDeviceEContent *content;
} VeDeviceElement;

/** function veDeviceElementCreate
    Creates an element.  The created element is not associated with
    any model.
    
    @param name
    The name of the element.
    
    @param type
    The type of the element content.

    @param vsize
    If the element content is of type <code>VE_ELEM_VECTOR</code>
    then this parameter determines the size of the vector.  Otherwise
    this parameter is ignored.
    
    @returns
    A newly-allocated element.
 */
VeDeviceElement *veDeviceElementCreate(char *name, VeDeviceEType type,
				       int vsize);
/** function veDeviceParseElem
    Creates an element based upon a string describing an element.
    A string description has the following format:
    <blockquote>[<b>elem</b>] <i>name</i> <i>type</i> [<i>type_args</i> ...]</blockquote>
    The word <b>elem</b> at the beginning is optional.  If it is there it
    will be ignored.  This means that it is a bad idea to call an element
    "elem".  A name for the element and a type must be specified.  Following
    the type may be initialization options for that particular type of
    element content.  The following types and arguments are supported:
<dl>
<dt><b>trigger</b></dt>
<dd>A trigger has no arguments</dd>
<dt><b>switch</b> [<i>state</i>]</dt>
<dd>A switch may optionally take an initial state for the switch.  If the
initial state is not specified, then it is left undefined.</dd>
<dt><b>valuator</b> [<i>min</i> [<i>max</i> [<i>value</i>]]]</dt>
<dd>If no arguments are given to a valuator, then its minimum, maximum
and value all default to 0.0.  If the arguments are specified (as
real numbers) then they are initialized appropriately.</dd>
<dt><b>vector</b> <i>size</i> [<b>{</b> [<i>min</i> [<i>max</i> [<i>value</i>]]] <b>}</b>]</dt>
<dd>A size must always be specified for a vector.  Each valuator in the
vector can have an initializer as for the "valuator" type, but it must
be surrounded in curly braces: "{}".</dd>
</dd>
</dl>
For example:
<blockquote>elem foobar valuator -5.0 5.0</blockquote>
would create an element called "foobar" as a valuator with a minimum 
bound of -5.0 and a maximum bound of 5.0.  Since the value was not
specified it defaults to 0.0.
<blockquote>skippy vector 4 {-2.0 2.0} {} {0.0 0.0 40.0}</blockquote>
creates a vector called "skippy" of size 4.  The first valuator of the
vector will have a minimum bound of -2.0 and a maximum bound of 2.0
and a value of 0.0 (default).  The second valuator will be set to defaults
(no bounds, value = 0.0).  The third valuator is unbounded and has an
initial value of 40.0.  The fourth valuator's parameters are not specified
so they are set to the defaults.

@param spe
The string to parse.

@returns
A pointer to a newly-created element.  If an error is encountered
a <code>NULL</code> pointer is returned.
*/
VeDeviceElement *veDeviceParseElem(char *spec);

/** function veDeviceElementDestroy
    Destroys a previously allocated element, including its element content.
    
    @param e
    The element to destroy.
*/
void veDeviceElementDestroy(VeDeviceElement *e);

/** struct VeDeviceModel
    A device model is just a hash of names to VeDeviceElement pointers.
    See <code>ve_util.h</code> for more information on working with
    string maps.  Convenience functions are provided so that you do
    not need to access the string map directly.
*/
typedef struct ve_device_model {
  VeStrMap elems; /* maps to VeDeviceElement pointers */
} VeDeviceModel;

/** function veDeviceCreateModel
    Creates a new VeDeviceModel structure with an empty element map.

    @returns
    A newly-allocated VeDeviceModel object.
*/
VeDeviceModel *veDeviceCreateModel();

/** function veDeviceAddElem
    Adds an element to a device model.  The element must have already
    been allocated and created.  This does not create a copy of the
    element - the device model stores the given pointer.  Thus the
    given element should not be freed after passing it to this function.
    Once an element is added to a device model, the device model will
    take care of freeing the memory associated with the element when
    necessary.  If an element already exists in the device model with the
    name of the new element, the old element is removed and destroyed,
    and the new element is inserted in its place.

    @param model
    The model to add the element to.

    @param elem
    The element to add.
    
    @returns
    0 on success, non-zero on failure.
*/
int veDeviceAddElem(VeDeviceModel *model, VeDeviceElement *elem);

/** function veDeviceAddElemSpec
    Adds an element to a device model given a string representation
    of that element.  This in effect a convenience function for
    <blockquote><code>veDeviceAddElem(model,veDeviceParseElem(spec))</code></blockquote>
    with some added error checking.

    @param model
    The model to add the element to.
    
    @param spec
    The string describing the element in a form that is acceptable to
    <code>veDeviceParseElem()</code>.

    @returns
    0 on success, non-zero on failure.
*/
int veDeviceAddElemSpec(VeDeviceModel *model, char *spec);

/** function veDeviceFindElem
    Looks up an element by name in a device model.

    @param model
    The model to search in.
    
    @param name
    The name of the element you are looking for.
    
    @returns
    A pointer to the element if one exists in this model with the
    given name.  <code>NULL</code> otherwise.
*/
VeDeviceElement *veDeviceFindElem(VeDeviceModel *model, char *name);

/** section Device Manifest
    Devices are put together from a number of pieces.  First, a
    <i>device description</i> describes that specifics of a particular device
    - name, type, driver, settings, etc.  These descriptions are usually
    read in from a manifest.  A <i>manifest</i> lists possible devices in the
    system.  A device needs to be <i>used</i> before it is accessible to
    the system.  The manifest is generally fixed for a particular computer
    system.  Individual applications then specify which devices they actually
    use.  There is general support in the VE library for loading a device
    usage file at run-time so that the set of devices to use and their
    mappings can be easily loaded at run-time.
    <p>A manifest also contains driver references, which let the system
    know what drivers need to be loaded for what types of devices.
    Driver references have general types - currently driver references
    for devices and filters are supported.  Other driver references may
    be supported in the future.
    <p>There is a single device manifest in memory at run-time.  All
    additions or removals are done to this global manifest.
 */

/** struct VeDeviceDesc
    A description of a device.  Every description must include a name
    and a type.  Names and types are just strings.  As well, an
    arbitrary number of options can be defined for a device.  These
    options are usually device specific (e.g. the x11 driver has a
    "display" option which allows you to specify which X server you
    want to use for input devices).  These options are stored in
    a string map when the entries being strings.  Strings in the
    options map are all allocated copies - that is, they have their
    own storage.  If an option is removed from the string map, its
    value (i.e. the pointer to its contents) should be retrieved and
    freed first.
*/
typedef struct ve_device_desc {
  /** member name
      The name of this device as a null-terminated string.
  */
  char *name;
  /** member type
      The type of this device as a null-terminated string.
  */
  char *type;
  /** member options
      Device-specific options as a string map.  The value of an option
      is stored as string.
  */
  VeStrMap options; /* maps to strings */
} VeDeviceDesc;

/** function veDeviceDescCreate
    Creates an empty device description.

    @returns
    A newly-allocated VeDeviceDesc object.
*/
VeDeviceDesc *veDeviceDescCreate();

/** function veDeviceDescDestroy
    Frees memory allocated to a VeDeviceDesc object.  This includes
    any strings in the options map as well as the object itself.
    
    @param desc
    The description to free.
*/
void veDeviceDescDestroy(VeDeviceDesc *desc);

/** function veDeviceDescOption
    Retrieves an option from a description.

    @param desc
    The description from which to retrieve the option.
    
    @param name
    The name of the option to retrieve.

    @returns
    The value of the option if it is defined, <code>NULL</code> otherwise.
*/
char *veDeviceDescOption(VeDeviceDesc *desc, char *name);

/* assume there is only one global manifest - it is purely informational.
   Devices must be explicitly *used* before the content is meaningful. */

/** function veClearDeviceManifest
    Clears the in-memory global device manifest.  This does not affect
    any data on disk.  After calling this function, the manifest will
    contain no data.
    
    @returns
    0 on success, non-zero on failure.
*/
int veClearDeviceManifest(); /* clears the current device manifest */

/** function veAddDeviceDesc
    Adds a device description to the global device manifest.
    
    @param desc
    The description to add to the manifest.
    
    @returns
    0 on success, non-zero on failure.
*/
int veAddDeviceDesc(VeDeviceDesc *desc);

/** function veFindDeviceDesc
    Looks up an existing device description in the manifest by device
    name.
    
    @param name
    The name of the device to locate.

    @returns
    A pointer to the description of the device if it exists, <code>NULL</code>
    otherwise.
*/
VeDeviceDesc *veFindDeviceDesc(char *name);

/** function veDeviceAddDriverRef
    Adds a driver reference to the manifest.  Driver references are used
    to locate run-time loadable drivers for devices, filters, etc.
    A driver reference consists of a type (currently either <b>"device"</b>
    or <b>"filter"</b>) and a name.  For drivers, the name is the type of
    the device.  That is, all devices of the same type share the same
    driver.

    @param type
    The type of driver reference to add.  This is a string.  Currently
    valid values are <b>device</b> and <b>filter</b>.
    
    @param name
    The name of the specific object to add.  For devices, this
    is the type of the device.  For filters, this is the name of the
    filter.

    @param driverpath
    The path to the driver.  This path will be searched using VE's
    usual rules for loading drivers (see <code>ve_driver.h</code>
    for driver details).
 */
int veDeviceAddDriverRef(char *type, char *name, char *driverpath);

/** function veDeviceFindDriverRef
    Looks up a driver reference by driver type (<b>device</b> or
    <b>filter</b>) and name.

    @param type
    The type of driver reference to find.  This is a string.  Currently
    valid values are <b>device</b> and <b>filter</b>.
    
    @param name
    The name of the specific object to look up.  For devices, this
    is the type of the device.  For filters, this is the name of the
    filter.

    @returns
    A string containing the path to the driver if it was found,
    or <code>NULL</code> if it was not found.  The returned string
    should be treated as static and should <em>not</em> be freed by the
    application.
 */
char *veDeviceFindDriverRef(char *type, char *name);

/** function veReadDeviceManifest
    Reads data from the given stdio stream and appends it to the global
    device manifest.  This will overwrite any definitions that exist
    both in the stream and in the global manifest.  Other definitions
    currently in the manifest will not be overwritten.  If you wish
    to ensure that only those devices in the manifest are defined,
    call <code>veClearDeviceManifest()</code> before calling this function.
    <p>The following entries can be found in a manifest file:
    <ul>
    <li><b>driver</b> <i>type</i> <i>name</i> <i>path</i> - a driver reference.
    <li><b>device</b> <i>name</i> <i>type</i> [<b>{</b><i>options</i> ...<b>}</b>] - a device description.  Options are spread out over separate lines.
    On each option line, the first word is the name of the option and the 
    remainder of the line is the value of the option.
    <li><b>use</b> <i>name</i>  [ [<i>type</i>] <b>{</b><i>override-options</i> ...<b>}</b>] - uses a device.  Format is the same as a "device" line with the 
    following changes.  Both type and options may be omitted, but if "type" is
    included then options must also be specified.  This means that there are
    three valid formats for the "use" statement:
    <ul>
    <li><code>use devicename</code> - use the device with the options given
    in the "device" declaration.</li>
    <li><code>use devicename { newopts ... }</code> - use the device with the
    options given in the "device" declaration but override those options with
    any values given in the option section of this "use" statement.</li>
    <li><code>use devicename devicetype { newopts ... }</code> - define a
    new device and use it all in one line.  The options section must be
    specified (even if it is empty) or "devicetype" will be interpreted as
    options (and it looks like the second form).</li>
    </ul>
    </li>
    <li><b>filter</b> ... - a filter table entre (see 
    <code>veDeviceParseFTableEntry()</code></li>
    </ul>
    Blank lines and lines beginning with '#' are ignored.
    <p>Here is an example:
<pre>
        # This is a device manifest
        # Entries can be in any order
        # - for x11keyboard type devices, load the x11drv.so driver
        driver device x11keyboard x11drv.so
        # - for x11mouse type devices, load the x11drv.so driver - VE knows
        #   enough to only load the driver once
        driver device x11mouse x11drv.so
        # - declare a device called "keyboard" - since we omit options,
        #   the defaults for that driver are used
        device keyboard x11mouse
        # - the following is a Flock of Birds head-tracker
        device headtrack fob {
                line /dev/ttya
                speed 115200
                flow rtscts
                hemisphere forward
        }
</pre>

     @param stream
     The stdio stream from which to read the manifest.

     @param fname
     The name of the file from which data is being read.  This is only
     used for informational purposes in error messages.  If NULL, a
     fixed string will be substituted by the function.

     @returns
     0 on success, non-zero on failure.
*/
int veReadDeviceManifest(FILE *stream, char *fname);

/** function veGetDeviceManifest
    Returns a string map containing all known names in the device manifest.
    The returned string map must not be altered by the calling program.
    The string map should only be used to determine the set of names
    in the device manifest - the data mapped to the string in the map
    should be left alone by the calling program.

    @returns
    A pointer to the current device manifest or <code>NULL</code> if there
    is currently no device manifest.
 */
VeStrMap veGetDeviceManifest(void);

/** section Devices and Drivers
    Support for devices is provided through device drivers.  Each driver
    is responsible for providing support for a particular device type.
    The VE library keeps an internal table mapping known device types to
    drivers.  When a device is created in the system, the VE library uses
    the driver to <i>instantiate</i> the device, creating both a link to
    the driver but a private structure for the driver containing the
    information specific to that instance of the device.  A device
    description (see the section on the manifest above) is required to
    instantiate a device.
    <p>Be wary of the distinction between general drivers (i.e. shared
    objects and libraries) which are system objects, versus device drivers
    (module that handles input devices) which are VE objects.
    <p>A device in the system has up to two parts.  One part is a model
    (as discussed earlier).  The model is an abstract representation of
    a device.  The other part is an instance, which represents an
    input device that will generate events.  We group devices into two
    sets: <i>real devices</i> - that is, devices with instances that
    represent some source of input events for the program, and 
    <i>virtual devices</i> - devices that do not generate input events,
    but which we may model, or refer to by name.  The distinction is
    purely conceptual - the system itself makes no explicit distinction
    between virtual and real devices.  Real devices always have an instance
    but may or may not have a model.  Without a model, real devices can
    still generate input events, but the structure of the device cannot be 
    inspected from the program.  Virtual devices do not have an instance
    and may or may not have a model.  Virtual devices are typically used
    for devices that are inherent to the application.  For example,
    a driving simulator might have a virtual steering wheel, accelerator,
    brake, gear shift, etc.  The real devices that control these may
    be joysticks, mice, keyboards, etc.  The accelerator is virtual.
    If we provide a model for it, then that model can track changes made
    to its state by incoming events.  However, even if we do not provide
    a model, we can still map incoming events to the accelerator.  A
    device which has neither a model nor an instance is considered to be
    <i>purely virtual</i>.  Purely virtual devices cannot be inspected but
    we can still receive and handle events mapped to these devices.  The
    section below on events and filters describe some of the mechanisms
    for managing events and mapping events from one device to another.
    <p>Device drivers are expected to spawn any threads required to
    collect incoming data and generate events when a device is instantiated.
 */

/** struct VeDeviceDriver
    Entry in the internal driver table that maps a device type to the
    code that instantiates devices of that type.  If a particular
    piece of driver code can support more than one type of device,
    then multiple VeDeviceDriver structures will need to be created.
 */
typedef struct ve_device_driver {
  /** member type
      The type of device that this driver supports.
   */
  char *type;       /* name of the type this driver provides */
  /** member inst
      The function that will create the device.  The arguments to the function
      will be a pointer to this driver structure as well as a device
      description.  The function should return a pointer to a VeDevice
      structure (see below) on success, and <code>NULL</code> on failure.
  */
  struct ve_device *(*inst)(struct ve_device_driver *d,VeDeviceDesc *desc,VeStrMap override);
  /** member deinst
      The function that will destroy the device.  This function should dispose
      of the VeDevice structure and any associated structures.
   */
  void (*deinst)(struct ve_device *);
  /** member devfunc
      A function that is called for generic functions.  This used to 
      optionally implement the functionality for <code>veDeviceFunc</code>.
      If this value is <code>NULL</code> then generic functions are not
      supported by this device.
  */
  int (*devfunc)(struct ve_device *, char *func, char *args, char *resp_r,
		 int rsz);
} VeDeviceDriver;

/** struct VeDeviceInstance
    An instance of a device consists of a reference to its driver, plus
    a private structure containing the instance-specific information.
 */
typedef struct ve_device_instance {
  /** member driver
      A pointer to the driver structure from which this instance was spawned.
  */
  VeDeviceDriver *driver;
  /** member idata
      The instance-specific data.  This structure is private to the driver
      and should not be referenced from outside the driver.
  */
  void *idata;  /* data specific to instance */
  /** member options
      A string map of options that are specific to this instance of this
      device.
   */
  VeStrMap options;
} VeDeviceInstance;
/** function veDeviceInstanceInit
    Creates and initializes a device instance structure.  This
    function call is provided as a convenience for device drivers.

    @param driver
    The driver from which this device was created or <code>NULL</code>
    if there is none.

    @param idata
    The private (driver-specific) structure for this instance, or
    <code>NULL</code> if there is none.

    @param desc
    The description from which to initialize the option set, or
    <code>NULL</code> if no description is provided.

    @param override
    A set of options that override those in the description or
    <code>NULL</code> if there are no override options.
    
    @returns
    A pointer to a newly created instance structure if successful,
    or <code>NULL</code> if an error occurred.
*/
VeDeviceInstance *veDeviceInstanceInit(VeDeviceDriver *driver, void *idata,
				       VeDeviceDesc *desc,
				       VeStrMap override);

/** function veDeviceInstOption
    Retrieves an option from an instance.  Device driver writers should
    create an instance with <code>veDeviceInstanceInit()</code> and
    retrieve options with <code>veDeviceInstOption()</code> rather than
    <code>veDeviceDescOption()</code>.
    
    @param i
    The instance from which to retrieve the option.

    @param name
    The name of the option to retrieve.

    @returns
    The value of the option if it is defined, <code>NULL</code> otherwise.
 */
char *veDeviceInstOption(VeDeviceInstance *i, char *name);

/** function veDeviceAddDriver
    Adds a device driver to the system table.
    
    @param d
    The driver to add.

    @returns
    0 on success, non-zero on failure.
 */
int veDeviceAddDriver(VeDeviceDriver *d);

/** function veDeviceFindDriver
    Finds a device driver in the system table for the given device type.
    
    @param type
    The type of device for which we want to find a driver.
    
    @returns
    A pointer to a VeDeviceDriver structure if successful, 
    <code>NULL</code> if a driver cannot be found.
*/
VeDeviceDriver *veDeviceFindDriver(char *type);

/** struct VeDevice
    An actual device.  Only those devices with either an instance
    or model should have a VeDevice structure associated with them.
    It is possible to create a purely virtual device structure (with
    null entries for the instance and the model) but this structure does
    not serve any purpose.
 */
typedef struct ve_device {
  /** member name
      The name of the device (from the device description used to
      generate this device).
  */
  char *name;
  /** member instance
      A pointer to the instance of this device.  Virtual devices will
      have a null value here.
  */
  VeDeviceInstance *instance;
  /** member model
      A pointer to the model for this device.  This will be a null value
      if the device has no model.
  */
  VeDeviceModel *model;
} VeDevice;

/** function veDeviceCreate
    Creates a new device.  The structure will initially be purely virtual
    in that the instance and model will not be defined.  This function
    just allocates the structure - it does not attempt to build all of
    the aspects of the device object.  See <code>veDeviceUse()</code> for
    this.

    @param name
    The name of the device to create.

    @returns
    A pointer to a newly-allocated VeDevice object.
*/
VeDevice *veDeviceCreate(char *name);

/** function veDeviceCreateVirtual
    Creates a new virtual device.  This device will be locatable
    using <code>veDeviceFind()</code> and if it has a model then
    incoming events will be applied to it.

    @param name
    The name of the device.
    
    @param model
    An optional model for the device.  If this is <code>NULL</code> then
    a purely virtual device is created (i.e. without a model).

    @returns
    A pointer to the newly-created VeDevice object or <code>NULL</code>
    if there is an error or if a device with that name already exists.
*/
VeDevice *veDeviceCreateVirtual(char *name, VeDeviceModel *model);

/** function veDeviceUseByDesc
    Creates a device based upon a device description.  This will also load
    any drivers the device may need as defined by driver references.  If
    the driver has been previously created via a <code>veDeviceUseByDesc()</code>
    or <code>veDeviceUse()</code> call, then a new device will not be created
    and a pointer to the old device will be returned.  The device will be
    instantiated if necessary.

    @param desc
    A pointer to the device description.

    @returns
    A pointer to the device.  If the device is not in use, then the device
    will be instantiated.  <code>NULL</code> is returned in the event of an
    error.
*/
VeDevice *veDeviceUseByDesc(VeDeviceDesc *desc, VeStrMap override);

/** function veDeviceUse
    Creates a device based upon a device description from the global
    device manifest.  This is the command that is typically used for
    instantiating devices.  Its behaviour is the same as 
    <code>veDeviceUseByDesc()</code> except that the device description
    comes from the global manifest rather than being explicitly given
    as a parameter.

    @param name
    The name of the device to use.
    
    @returns
    A pointer to the device.  If the device is not in use, then the device
    will be instantiated.  <code>NULL</code> is returned in the event of an
    error, including the case if the device cannot be found in the manifest.
 */
VeDevice *veDeviceUse(char *name, VeStrMap override);

/** function veDeviceIsOptional
    Checks a device's predefined values or its override map to
    determine if it is optional or not;

    @param name
    The name of the device is checked.

    @param override
    An optional set of override values to use as part of the
    check.  To not pass overrides, specify <code>NULL</code> as this
    argument.

    @returns
    0 if the device is not optional, 1 if the device is marked as
    optional.  If there is an error, then the device is assumed to
    be non-optional (i.e. 0 is returned).
 */
int veDeviceIsOptional(char *name, VeStrMap override);

/** function veDeviceFind
    Finds the device structures of active devices.  This will not
    instantiate a device.  Only devices that have been previously used
    can be retrieved with this function.  Note that purely virtual devices
    do not exist as active devices and thus cannot be found by this
    command.

    @param name
    The name of the device to find.

    @returns
    A pointer to the device if it has been previously used,
    <code>NULL</code> if the device is not in use.
*/
VeDevice *veDeviceFind(char *name); /* find active devices */

/** function veDeviceFunc
    Make an arbitrary call to a device.  This functionality is
    meant to allow special case functionality not anticipated by
    the VE library and is likely used for calibration, one-off
    cases where a special application or module needs to speak
    directly to a device.
    <p>Note that VE itself does not implement these calls.  It
    is up to the individual device driver to implement these calls.

    @param device
    The device for which the call is to be made.
    
    @param func
    The name of the function the device will processed.
    VE does not interpret this value - it is passed to the device driver.

    @param args
    Arguments to the function (as a string).  VE does not interpret
    this value - it is passed to the device driver.
    
    @param resp_r
    A string buffer into which the result of the function call will
    be stored.  If you don't want to specify a result buffer, use
    the value <code>NULL</code> here and a value of 0 for <i>rsz</i>.

    @param rsz
    The size of the result buffer.  A size of 0 implies that nothing
    will be stored in the result buffer.

    @returns
    0 if the call was successful, or non-zero if there was a failure.
*/
int veDeviceFunc(VeDevice *device, char *func, char *args,
		 char *resp_r, int rsz);

/** section Events
    Real devices generate events.  Events combine element data with
    information about the time the event occured and from which 
    device and element the event is believed to have originated.
 */

/** struct VeDeviceEvent
    The object that represents a single event.  A number of macros
    are provided to simplify working with event structures.  All
    arguments to these macros are pointers to VeDeviceEvent objects.
<dl>
<dt><code>VE_EVENT_TYPE(x)</code></dt>
<dd>Returns the type of the event pointed to by <code>x</code>.</dd>
<dt><code>VE_EVENT_KEYBOARD(x)</code></dt>
<dd>Returns a pointer to a VeDeviceE_Keyboard structure which is the
event's element content.  If the type of the event is not 
<code>VE_ELEM_KEYBOARD</code> then the result of this macro is undefined.</dd>
<dt><code>VE_EVENT_TRIGGER(x)</code></dt>
<dd>Returns a pointer to a VeDeviceE_Trigger structure which is the
event's element content.  If the type of the event is not 
<code>VE_ELEM_TRIGGER</code> then the result of this macro is undefined.</dd>
<dt><code>VE_EVENT_SWITCH(x)</code></dt>
<dd>Returns a pointer to a VeDeviceE_Switch structure which is the
event's element content.  If the type of the event is not 
<code>VE_ELEM_SWITCH</code> then the result of this macro is undefined.</dd>
<dt><code>VE_EVENT_VALUATOR(x)</code></dt>
<dd>Returns a pointer to a VeDeviceE_Valuator structure which is the
event's element content.  If the type of the event is not 
<code>VE_ELEM_VALUATOR</code> then the result of this macro is undefined.</dd>
<dt><code>VE_EVENT_VECTOR(x)</code></dt>
<dd>Returns a pointer to a VeDeviceE_Vector structure which is the
event's element content.  If the type of the event is not 
<code>VE_ELEM_VECTOR</code> then the result of this macro is undefined.</dd>
</dl>
*/
typedef struct ve_device_event {
  /** member timestamp
      The time at which this event occurred.  This time is typically
      the value of <code>veClock()</code> at the time the event occurred.
      Its units are milliseconds and the time is relative to the clock's
      zero reference point.
  */
  long timestamp;
  /** member device
      The name of the device from which this event originates.  This
      may be a real, virtual or purely virtual device.  This name may be
      modified by filters before being passed to callbacks.
   */
  char *device;
  /** member elem
      The name of the element from which this event originates.  This
      element may or may not be a declared member of the device.  In other
      words, there are no restrictions on what this value may be.
  */
  char *elem;       /* names of device and element of device */
  /** member index
      Filters and callbacks may operate on a specific valuator in a vector.
      In those cases, this field is used to indicate which valuator in
      the vector to use.
  */
  int index;                 /* if etype == valuator and this event is
				delievered/ref to a vector then this index
				(if >= 0) indicates the appropriate piece
				of a vector */
  /** member content
      The actual data (i.e. element content) for the event.
  */
  VeDeviceEContent *content; /* data about the event (depends upon etype) */
} VeDeviceEvent;

#define VE_EVENT_TYPE(x) ((x)->content->type)
#define VE_EVENT_KEYBOARD(x) ((VeDeviceE_Keyboard *)((x)->content))
#define VE_EVENT_TRIGGER(x) ((VeDeviceE_Trigger *)((x)->content))
#define VE_EVENT_SWITCH(x) ((VeDeviceE_Switch *)((x)->content))
#define VE_EVENT_VALUATOR(x) ((VeDeviceE_Valuator *)((x)->content))
#define VE_EVENT_VECTOR(x) ((VeDeviceE_Vector *)((x)->content))

/** function veDeviceEventCreate
    Creates a device event object.
    
    @param type
    The type of the event to create.

    @param vsize
    If the event is of type <code>VE_ELEM_VECTOR</code> then this argument
    is the size of the vector.

    @returns
    A pointer to the newly created object, or <code>NULL</code> if an
    error occurs.
 */
VeDeviceEvent *veDeviceEventCreate(VeDeviceEType type, int vsize);

/** function veDeviceEventInit
    Creates a device event object and initializes some fields.
    Note that the call <code>veDeviceEventInit(type,vsize,NULL,NULL)</code>
    is not equivalent to <code>veDeviceEventCreate(type,vsize)</code>.
    Some fields (e.g. timestamp) are implicitly initialized (e.g. with
    the current time).
    
    @param type
    The type of the event to create.

    @param vsize
    If the event is of type <code>VE_ELEM_VECTOR</code> then this argument
    is the size of the vector.

    @param device
    The name of the device from which this event originates, or 
    <code>NULL</code> to not initialize the device field.

    @param elem
    The name of the element from which this event originates, or
    <code>NULL</code> to not initialize the event field.
    
    @returns
    A pointer to the newly created object, or <code>NULL</code> if an
    error occurs.
 */
VeDeviceEvent *veDeviceEventInit(VeDeviceEType type, int vsize,
				 char *device, char *elem);

#define veDeviceEventType(e) ((e)?((e)->content?((e)->content->type):\
    VE_ELEM_UNDEF):VE_ELEM_UNDEF)

/** function veDeviceEventCopy
    Creates a copy of an event.  The new event shares no memory with the old
    event - i.e. you can modify or destroy the old event without affecting
    the new event and vice versa.

    @param e
    The event to copy.

    @returns
    A duplicate of the given event.
 */
VeDeviceEvent *veDeviceEventCopy(VeDeviceEvent *e);

/** function veDeviceEventDestroy
    Destroys an event object and frees any memory associated with it.
    
    @param e
    The event to destroy.
 */
void veDeviceEventDestroy(VeDeviceEvent *e);

/** function veDeviceEventFromElem
    Creates an event based upon an element structure.  The device
    name must also be specified.  Other fields (e.g. timestamp) are
    filled in the same way as <code>veDeviceEventInit()</code>.
    
    @param device
    The name of the device to use, or the <code>NULL</code> to leave
    the device field of the event unspecified.

    @returns
    A pointer to the newly created object, or <code>NULL</code> if an
    error occurs.
*/
VeDeviceEvent *veDeviceEventFromElem(char *device, VeDeviceElement *el);

/** function veDeviceApplyEventToModel
    Updates a device model based upon the given event.  This function
    does not consider the device name in the event, but the element
    named in the event must exist in the device model, otherwise the
    function has no effect.  If the element does exist in the model,
    then its state is updated to match the state information in the
    event's element content.
    
    @param m
    The model to update.
    
    @param e
    The event which is used to update the model.
 */
void veDeviceApplyEventToModel(VeDeviceModel *m, VeDeviceEvent *e);

/** function veDeviceApplyEvent
    Applies an event to a matching device if that device has a model
    associated with it.  In effect, this function looks up a device
    based upon the device name in the event and then uses
    <code>veDeviceApplyEventToModel()</code> to update the device model.
    For this function to update a device, the device must have a
    corresponding VeDevice object.

    @param e
    The event to apply.
 */
void veDeviceApplyEvent(VeDeviceEvent *e);

/** section Callbacks
    A device sets up any number of callbacks to process events.  When an
    event is received, it is filtered and then passed to the first callback
    that matches its device and element specifications.
    <p>
    All callbacks must be of type <code>VeDeviceEventProc</code> which
    is a function which takes two arguments:  a pointer to the event
    which is being passed to it, and an abritrary argument which should
    be supplied by the application when setting up the callback.
    <p>
    Although events are generally consumed by the first callback they
    are passed to, there is one exception.  If a callback matches one
    valuator of a vector, then callback processing will continue
    after handling that callback.  Callbacks can abort any further
    processing by returning a non-zero value.
    <p>
    Callbacks are passed pointers to events whose memory is managed elsewhere.
    They should not destroy these events.
 */
typedef int (*VeDeviceEventProc)(VeDeviceEvent *e, void *arg);

/** struct VeDeviceSpec
    For both callbacks and filters, events are selected based upon
    a device specification.  A specification has three parts - a device part,
    an element part, and an index part.
    The device part defines which devices match the specification.
    It can either be a specific device or "*" which is a wildcard meaning
    that any device matches the device part.  The device part can be omitted
    which is equivalent to specifying "*".  The element part defines which
    elements match the specification.  Like the device part, it is either
    a specific name or "*" denoting any element.  However, specific strings
    can match either an element's name or an element's type.  For example,
    an element part of "foo" would match an element with a name of "foo" and
    would also match an element of type "foo".  Types are given obvious
    names ("trigger", "switch", "valuator", "vector", "keyboard").
    The index part is only meaningful if the type of element being matched
    is a vector, otherwise it is ignored.  If specified, it must be an
    integer denoting a specific valuator in a vector.
    <p>See <code>veDeviceParseSpec()</code> for how to express specifications
    as a string.
 */
typedef struct ve_device_spec {
  /** member device
      The device part of the specification.  If NULL, then the device
      part is not specified.
  */
  char *device;
  /** member elem
      The element part of the specification.  If NULL, then the element
      part is not specified.
  */
  char *elem;
  /** member index
      If this value is greater then or equal to 0 then it matches a specific
      valuator of a vector.  If this is less than 0 or if the element type
      being matched is not a vector then this field is ignored.
  */
  int index; /* only valid if >= 0 */
} VeDeviceSpec;

/** function veDeviceSpecCreate
    Creates a new empty specification.  By default the specification is
    all unspecified, meaning that it is equivalent to wildcards.
    
    @returns
    A pointer to a newly created VeDeviceSpec object.
 */
VeDeviceSpec *veDeviceSpecCreate();

/** function veDeviceSpecDestroy
    Destroys a previously created device specification and frees any memory
    associated with it.
    
    @param s
    The specification to destroy.
*/
void veDeviceSpecDestroy(VeDeviceSpec *s);

/** function veDeviceParseSpec
    Parses a device specification given as a string.  The three parts
    are given in order separated by periods:
    <blockquote><i>device</i>.<i>elem</i>.<i>index</i></blockquote>
    Any part can be omitted.  If trailing parts (e.g. index, or elem and
    index) are omitted, then the periods preceeding them must also be
    omitted.  If a part is omitted is left unspecified 
    in the structure.
    For devices and elements this leaves a null field and for the index this
    sets the value to -1.  For example:
<dl>
<dt><b>foobar</b></dt>
<dt><b>foobar.*</b></dt>
<dd>Matches any element of any device called foobar.  Both cases are
equivalent.</dd>
<dt><b>*.thingy</b></dt>
<dd>Matches any element called "thingy" or of type "thingy" in any device.</dd>
<dt><b>foobar.thingy.4</b></dt>
<dd>Matches the valuator with index 4 of device foobar and element name/type
thingy.  If the event being matched is not for a vector, then the index
portion is ignored.</dd> 
</dl>
    @param str
    The string containg the specification to parse.

    @returns
    A pointer to a newly-created device specification or <code>NULL</code>
    if an error is encountered.
*/
VeDeviceSpec *veDeviceParseSpec(char *str);

/** function veDeviceMatchSpec
    Compares an event to a device specification.

    @param e
    The event.
    
    @param s
    The specification.

    @returns
    A boolean value.  True (non-zero) if the event matches the specification
    nad false (zero) otherwise.
*/
int veDeviceMatchSpec(VeDeviceEvent *e, VeDeviceSpec *s);

/** function veDeviceAddCallbackSpec
    Adds a callback to the internal list of callbacks given a
    device specification object.

    @param p
    The callback to add.
    
    @param arg
    An application-supplied argument that will be passed to the callback
    when it is called due to an event matching the given specification.
    The library does not interpret this argument at all - it is provided
    for applications to pass some context to callbacks.

    @param s
    A device specification object defining which events should be passed
    to this callback.

    @returns
    0 on success, non-zero on failure.
 */
int veDeviceAddCallbackSpec(VeDeviceEventProc p, void *arg, VeDeviceSpec *s);

/** function veDeviceAddCallback
    Adds a callback to the internal list of callbacks given a device
    specification as a string.  This is the more common function for
    adding callbacks.

    @param p
    The callback to add.
    
    @param arg
    An application-supplied argument that will be passed to the callback
    when it is called due to an event matching the given specification.
    The library does not interpret this argument at all - it is provided
    for applications to pass some context to callbacks.

    @param spec
    A device specification as a string suitable for 
    <code>veDeviceParseSpec()</code>.

    @returns
    0 on success, non-zero on failure.
 */
int veDeviceAddCallback(VeDeviceEventProc p, void *arg, char *spec);

/** function veDeviceRemoveCallback
    Removes a callback previously added to the internal list.  All
    callbacks that call the given function will be removed.
    
    @param p
    All callbacks that would call this function will be removed from
    the internal list.

    @returns
    0 on success, non-zero on failure.
 */
int veDeviceRemoveCallback(VeDeviceEventProc p);

/** function veDeviceHandleCallback
    Processes callbacks for an event.  This function will call any
    callbacks that are appropriate for the given event based upon
    the internal list of callbacks.  If a callback returns a non-zero
    value then any further processing is aborted.
    
    @param e
    The event to pass to callbacks.

    @returns
    The result of the last callback called.
 */
int veDeviceHandleCallback(VeDeviceEvent *e);

/* used to prevent the input system from processing callbacks */


/** section Filters
    Incoming events can be passed through any number of filters.  Filters
    can generally modify events, including changing data, renaming events,
    discarding events or generating new events.
    <p>
    A filter is in effect a callback that is passed an event.
    Filters are arranged sequentially in a filter table.  Normally an
    event is applied to each filter in the table in turn.  The
    return value of the result determines the result of the filter
    and affects the disposition of the library towards that event:
<dl>
<dt><b>VE_FILT_CONTINUE</b></dt>
<dd>Continue processing the event with the next applicable filter in the
table.  If no applicable filters remain, pass the event to the appropriate
callbacks, if any.</dd>
<dt><b>VE_FILT_RESTART</b></dt>
<dd>Continue processing the event, but start at the beginning of the table
rather than the current position.  Beware that it is possible to build 
infinite loops in the filter table using the restart result.</dd>
<dt><b>VE_FILT_DISCARD</b></dt>
<dd>Stop all further filter processing of the event and discard it without
passing it to callbacks.</dd>
<dt><b>VE_FILT_DELIVER</b></dt>
<dd>Stop all further filter processing of the event and immediately pass it to
any applicable callbacks.</dd>
<dt><b>VE_FILT_ERROR</b></dt>
<dd>Indicates that the filter encountered an error in processing the event.
The event is discarded and an error reported through VE's error module.</dd>
</dd>
</dl>
<p>Applications are free to create any type of filter they wish.
The VE library provides a run-time method for creating filters using
the BlueScript language.  Note that the VE-2.1 filter model is no longer
supported.  See the <a href="ve_blue.h.html">ve_blue</a> module for
more information on the BlueScript interface to VE.</p>
*/

#define VE_FILT_ERROR    (-1)
#define VE_FILT_CONTINUE   0
#define VE_FILT_RESTART    1
#define VE_FILT_DISCARD    2
#define VE_FILT_DELIVER    3

/** struct VeDeviceFilter
    This represents a specific filter that will be used to process
    an event.  All filter types (external, script, etc.) ultimately
    generate a <code>VeDeviceFilter</code> object which is placed
    into the filter table.
 */
typedef struct ve_device_filter {
  /** member proc
      A pointer to the function containing the actual execution code for
      this filter.  The format of proc is the same as a callback function.
  */
  VeDeviceEventProc proc;
  /** member cdata
      A private object passed to the filter process.  The meaning
      of this object is dependent upon the filter implementation.
      No other meaning is assigned to it.
  */
  void *cdata;
} VeDeviceFilter;

/** function veDeviceFilterCreate
    Creates a new filter object.

    @param proc
    The value for the <em>proc</em> field.

    @param cdata
    The value for the <em>cdata</em> field.

    @returns
    A pointer to a newly-created filter object.
 */
VeDeviceFilter *veDeviceFilterCreate(VeDeviceEventProc proc,
				     void *cdata);

/** subsection Filter Table
    How to apply filters to events is described in the system's filter
    table.  The filter table is a sequence of <code>VeDeviceFilter</code>
    objects, each associated with a <em>device specification</em> which
    identifies those events to which this filter applies.
    <p>Normal processing of the filter table runs from the head to the
    tail, applying each matching filter.  This processing can be changed
    by the filters themselves (see the return codes above for specifics).
 */

#define VE_FTABLE_HEAD 0
#define VE_FTABLE_TAIL 1

/** function veDeviceFilterAdd
    Adds a filter to the global table.

    @param spec
    The specification for the device.  The table will store the
    pointer to this object, so the object must not be volatile.
    Applications should be careful not to modify this object after
    passing it 

    @param filter
    The filter to add.

    @param where
    If <i>where</i> is <code>VE_FTABLE_HEAD</code> then the entry is inserted
    at the beginning of the table.  If <i>where</i> is <code>VE_FTABLE_TAIL</code>
    then the entry is inserted at the end of the table.  Other values for
    this parameter are undefined.

    @returns
    0 on success, non-zero on error.
 */
int veDeviceFilterAdd(VeDeviceSpec *spec, VeDeviceFilter *filter, int where);

/** function veDeviceFilterProcess
    Process an event through the filter table.  This runs an event
    until a terminal status is encountered.

    @param e
    The event to filter.

    @returns
    The last result code returned from the filter.  Note that even
    if the result is <code>VE_FILT_DISCARD</code> this function does
    <em>not</em> free the event pointed to by <i>e</i>.  Just the
    result is returned.  However, new events may be created as
    side-effects from filter processing.
 */
int veDeviceFilterProcess(VeDeviceEvent *e);

/** struct VeDeviceFTableEntry
   An entry in the filter table.
*/
typedef struct ve_device_ftable_entry {
  VeDeviceSpec *spec;
  VeDeviceFilter *filter;
  struct ve_device_ftable_entry *next;
} VeDeviceFTableEntry;

/** section Event Queue
    Events are conceptually organized into a queue.  All events are
    processed in the order that they arrive.  Events are not necessarily
    queued - if no events are being processed when an event arrives
    from a device, it is processed immediately.  Events are only queued
    when other processing is going on that prevents the event from being
    processed immediately.  Events that are generated due to processing
    another event are also queued (e.g. from a filter) are added to
    queue but are typically added at the head of the queue rather than
    the tail since they are assumed to have occured at the same time
    as the original event.
 */

/** function veDevicePushEvent
    Adds an event to the event queue.  This function is thread-safe.
    
    @param e
    The event to add.  This pointer will be added to the queue so it
    should not refer to volatile memory.  The library will take care
    of freeing resources related to the event at a later point.
    
    @param where
    Where to add the event.  If this is <code>VE_QUEUE_HEAD</code> then
    the event is added to the head of the queue.  If this is 
    <code>VE_QUEUE_TAIL</code> then it is added to the tail.  Other
    values for this parameter are undefined.
    
    @param disp
    Disposition of this event.  Valid values are 
    <code>VE_FILT_CONTINUE</code> for normal processing or
    <code>VE_FILT_DELIVER</code> to indicate that filter table processing
    should be skipped.  The value 0 is always a valid default meaning
    normal processing.
    
    @returns
    0 on success, non-zero on failure.
*/
int veDevicePushEvent(VeDeviceEvent *e, int where, int disp);

/** function veDeviceEventPending
    Checks to see if there are any events in the queue.

    @returns
    0 if there are no events in the queue, 1 if there are events available.
 */
int veDeviceEventPending(); /* 1 if events are available */

/** function veDeviceNextEvent
    Retrieves the next event in the queue (if any).

    @param where
    Where to retrieve the event from.  If this is <code>VE_QUEUE_HEAD</code> then
    the event is retrieved from the head of the queue.  If this is 
    <code>VE_QUEUE_TAIL</code> then it is retrieved from the tail.  Other
    values for this parameter are undefined.

    @param disp
    If not <code>NULL</code> then the current disposition of the event
    will be stored here.  The disposition describes the kind of processing
    this event expects:
    <ul>
    <li><code>VE_FILT_CONTINUE</code> - normal processing</li>
    <li><code>VE_FILT_DELIVER</code> - skip filter table processing
    but deliver directly to callback</li>
    </ul>

    @returns
    A pointer to an event object if there are events in the queue.  
    <code>NULL</code> is returned if the queue is empty.  The returned
    event is removed from the queue - it is up to the calling program to
    destroy the event when it has finished with it.
 */
VeDeviceEvent *veDeviceNextEvent(int where, int *disp);

/** function veDeviceProcessQueue
    Processes all events pending in the queue.
 */
void veDeviceProcessQueue(void);

#define VE_QUEUE_HEAD 0
#define VE_QUEUE_TAIL 1

/* event processing */
/** function veDeviceProcessEvent
    Processes an event.  This entails applying all applicable filter chains
    in the filter table, applying the event to a device model (if
    applicable) and then passing it to any existing callbacks.  Only
    one event may be processed at a time and other threads that attempt
    to process events while this thread is processing will be blocked.
    <p>Note that there are special circumstances for vectors that get
    processed through a filter chain which selects a specific index
    of the vector.  In this case, the filter only applies to that valuator
    and not the whole vector.  A new event of type valuator is created to
    be filtered.  If that event does not change its type, device or element,
    then the new filtered content is merged back into the vector and the new
    event is destroyed.  Otherwise
    if any of the type, device or element names change, and the result is
    not merged back into the vector and the filtered event is treated as
    a new valuator event separate from the original vector event.  In this
    case (where a particular index of a vector is selected) the result of the 
    filter only ever applies to the new valuator event and not to the vector
    event (which always continues).

    @param e
    The event to process.

    @returns
    0 on success, non-zero on error, either internal or from a filter
    (i.e. <code>VE_FILT_ERROR</code> status).
 */
int veDeviceProcessEvent(VeDeviceEvent *e);

/** function veDeviceInsertEvent
    Similar to <code>veDeviceProcessEvent()</code> but meant as a hook
    for devices to deliver events to the system.  If there are no
    events in the queue, then this will attempt to process the event
    immediately.  Otherwise the event will be queued for later processing.
    
    @param e
    The event to insert.
    
    @returns
    0 on success, non-zero on error, either internal or from a filter
    (i.e. <code>VE_FILT_ERROR</code> status).  Note that if the event
    needs to be queued for later the result will be 0 and any filter
    errors will be delivered to the thread that processes the event
    from the queue.
*/
int veDeviceInsertEvent(VeDeviceEvent *e); /* for device drivers... */

#define VE_DEVICE_NOBLOCK 0
#define VE_DEVICE_DISCARD 1
#define VE_DEVICE_QUEUE   2
/** function veDeviceBlockEvents
    Prevents events from being dispatched to handlers.  No events
    will be processed until <code>veDeviceUnblockEvents()</code> is
    called.
    
    @param disp
    The disposition towards events.  The following constants are defined
    and recognized:
    <ul>
    <li><code>VE_DEVICE_DISCARD</code> - any blocked events should be
    discarded.</li>
    <li><code>VE_DEVICE_QUEUE</code> - any blocked events should be queued
    and will be processed once the block is lifted.
*/
void veDeviceBlockEvents(int disp);

/** function veDeviceUnblockEvents
    Removes a previous block on events.  If the disposition towards blocked
    events was to queue them, then when the event queue handler next runs,
    all blocked events will be processed.  If the disposition towards
    blocked events was to discard them, then all events that were blocked
    are lost, and only events generated after the block is lifted will be
    processed.  If no block currently exists, then this callback has no
    effect.
*/
void veDeviceUnblockEvents(void);

/* must be called before handling any device stuff */
/** function veDeviceInit
    This function must be called before calling any other ve_device
    functions.  It is generally called by <code>veInit()</code>.
*/
int veDeviceInit();

/** function veDeviceToValuator
    Forces an event to be treated as a valuator.  The actual effect of
    this function varies depending upon the input event.  The result
    will be a valuator value.  For triggers, the value
    is always 1.0.  For switches and keyboards the value is either 0.0
    (state == 0) or 1.0 (state == 1).  For vectors, the first valuator in 
    the vector is returned.  For valuators, the value of the valuator is
    returned.
    <p>This function is provided as a convenience and is meant to handle
    a reasonable number of common cases.  More complex conversions should be
    done by hand or with the conversion filters.</p>

    @param e
    The event to interpret.

    @returns
    A valuator value.  If this is not a valuator type (valuator
    or vector) then the implicit range of the valuator is 0.0 - 1.0.
*/
float veDeviceToValuator(VeDeviceEvent *e);

/** function veDeviceToSwitch
    Forces an event to be treated as a switch.  The actual effect of
    this function varies depending upon the input event.  The result
    will be either 0 or 1.  For triggers, the value is always 1.
    For switches and keyboards, the value of the event's state is returned.
    For valuators, a threshold at (min+max)/2 is set - values above the
    threshold are 1, values below the threshold are 0.  If the valuator is
    unbounded then implicit ranges of 0.0 - 1.0 are used.  Vectors are treated
    the same as a valuator, except that only the first value is used.
			 
    <p>This function is provided as a convenience and is meant to handle
    a reasonable number of common cases.  More complex conversions should be
    done by hand or with the conversion filters.</p>

    @param e
    The event to interpret.

    @returns
    A switch value.
*/
int veDeviceToSwitch(VeDeviceEvent *e);

/** section Controls
    A control is, in effect, a canned set of callbacks for 
    receiving events and creating behaviours based upon those
    events.  Controls may either affect VE's state directly
    (e.g. the origin, the eye, motion callbacks) or may generate
    events on their own.
    <p>Controls have properties of both filters,in that they
    receive and process events, and of devices, in that they may
    be sources of events.
 */

/** struct VeDeviceCtrlDesc
    A description of a control, as specified in a manifest file.
 */
typedef struct ve_device_ctrl_desc {
  /** member inputname 
      The device name to which inputs will be delivered - 
      may be <code>NULL</code> to indicate that no events
      should be generated.
   */
  char *inputname;
  /** member outputname
      The device name from which inputs will be generated - 
      may be <code>NULL</code> to indicate that no events
      should be generated.
   */
  char *outputname;
  /** member type
      The type of control.  Indicates which driver to use
      to create the control.
   */
  char *type;
  /** member options
      Control-specific options as a string map.  The value of an option
      is stored as string.
  */
  VeStrMap options; /* maps to strings */
} VeDeviceCtrlDesc;

VeDeviceCtrlDesc *veDeviceCtrlDescCreate(void);
void veDeviceCtrlDescDestroy(VeDeviceCtrlDesc *);

void veDeviceCtrlDescSetOption(VeDeviceCtrlDesc *, char *name, char *value);
char *veDeviceCtrlDescGetOption(VeDeviceCtrlDesc *, char *name);

struct ve_device_ctrl;

typedef struct ve_device_ctrl_driver {
  char *type;
  int (*init)(struct ve_device_ctrl *, VeStrMap options);
  int (*event)(struct ve_device_ctrl *, VeDeviceEvent *e);
  void (*deinit)(struct ve_device_ctrl *);
} VeDeviceCtrlDriver;

typedef struct ve_device_ctrl {
  /* internal list pointers */
  struct ve_device_ctrl *next, *prev;
  /* data */
  char *inputname;
  char *outputname;
  char *type;
  VeDeviceCtrlDriver *driver;
  void *priv;  /* data private to this instance */
} VeDeviceCtrl;

VeDeviceCtrl *veDeviceCtrlCreate(VeDeviceCtrlDesc *desc);
/* use "*" here as a wildcard... */
VeDeviceCtrl *veDeviceCtrlFind(char *inputname, char *outputname);
void veDeviceCtrlDestroy(VeDeviceCtrl *ctrl);
void veDeviceCtrlAddDriver(VeDeviceCtrlDriver *drv);
VeDeviceCtrlDriver *veDeviceCtrlFindDriver(char *type);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* VE_DEVICE_H */
