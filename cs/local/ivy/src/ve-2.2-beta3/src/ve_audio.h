#ifndef VE_AUDIO_H
#define VE_AUDIO_H

#include <ve_version.h>
#include <ve_env.h> /* defines VeOption */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** misc
    This is the new audio interface for VE.  This is not compatible
    with 2.1 and earlier audio interfaces although some constants
    and types are re-used.  From an application layer the new interface
    is somewhat simpler.  The physical arrangement of outputs is
    abstracted and dealt with in the environment file.
*/

/** section Overview
    
    This is how VE models audio.
    <p>
    From the application point-of-view there are 0 or more audio channels.
    An audio channel represents a logical grouping of audio outputs.  For
    example, a set of loudspeakers that provide environmental sound may
    be a separate channel from a pair of headphones that provide personal
    sound.  Channels are identified by names.
    <p>
    The name "default" is always defined as the "default" audio channel
    for a given setup.  All channels can be accessed - i.e. if a program
    asks the library for a channel object for any channel name, a valid
    object will be returned.  However, that object may just be a pointer
    to a "null" channel which generates no output.
    <p>
v    A channel has 0 or more outputs.  If channels are on the same node,
    they may share outputs (e.g. the same output may be referenced by
    more than one channel).  Outputs also have location and orientation
    information associated with them as a <em>frame</em>, allowing the
    specification of a physical arrangement of speakers.
    <p>
    Sounds sent to a channel are rendered via an <em>engine</em>.  
    Audio rendering engines can be loaded at run-time as device drivers.
    Engines are responsible for generating positional audio as well as 
    delivering output to audio <em>devices</em>.  Delivery is left up to
    the engine layer to allow for the use of hardware-based positional audio
    (e.g. DirectX technologies).
 */

/** misc
    Multiple audio drivers may be used at once.  This is the operating system
    interface.  An audio device may have multiple audio outputs.  These
    outputs must be sequentially numbered starting from 0 up to <i>n</i>-1
    where <i>n</i> is the number of individual outputs the device supports.
    A device may optionally map names to these outputs.  In particular, the
    <code>veAudioDevGetOutput()</code> call must always recognize integer
    values for outputs and may optionally recognize names.
    <p>The only driver guaranteed
    to be available is the "null" driver which is nothing more than
    a sink.  It does not generate any actual output.</p>
*/

/** struct VeAudioInst
    The internal structure for an instance of a sound.  
    There may be many instances of the same sound playing at once.
    Every node involved in audio maintains a table of such 
    instances.
 */
typedef struct ve_audio_inst {
  /** member id
      Unique id of this particular instance.
      (Offset into internal table of sound instances.)
   */
  int id;
  /** member sid
      Identifier of the sound of which this is an instance.
  */
  int sid;
  /** member chid
      Identifier of the channel with which this instance is associated.
   */
  int chid;
  /** member itid
      Offset into the channel's instance table.
   */
  int itid;
  /** member snd
      Cached pointer to the local copy of the sound.
   */
  struct ve_sound *snd;
  /** member params
      Current parameters for this instance.
  */
  struct ve_audio_params *params;
  /** member next_frame
      Indicates the next frame of the sound that will be played.
      If this is less than 0 then the sound is "dead" (i.e.
      it is no longer playing).
   */
  int next_frame;
  /** member clean
      An array of two flags that are used to indicate when
      an instance is truly finished with.  The application 
      interface sets flag 0 when it has finished with the
      instance (by a call to <code>veAudioClean()</code>).
      The audio processing thread sets flag 1 when it has
      finished with it.  An instance is not reused until
      both clean flags are non-zero and <i>waiting</i> is 0
      (see below).
   */
  int clean[2];
  /** member waiting
      A count of threads waiting for this instance to complete.
      This exists to prevent disposing of an instance until
      both the application and the processing have finished
      with it.
  */
  int waiting;
  /** member finished_cond
      A condition variable that threads can sleep on to wait for
      this sound to finish playing.
  */
  VeThrCond *finished_cond;
  /** member edata
      A place for the rendering engine to hang information.
   */
  void *edata;
} VeAudioInst;

struct ve_audio_channel;

/** struct VeAudioChannelBuffer
    A structure used by the audio layer for storing output information
    for a channel.  It has <i>ch->noutputs</i> mono buffers referring
    to the channel's outputs in the order in which they are stored in
    the <i>ch->outputs</i> list.
    <p>A channel buffer must be associated with a channel. 
    (i.e. the <i>ch</i> field may not be <code>NULL</code>.
 */

typedef struct ve_audio_channel_buffer {
  /** member ch
      The channel with which this buffer is associated.
  */
  struct ve_audio_channel *ch; 
  /** member buf
      An array of buffers.  Once created there will be
      <i>ch->noutputs</i> buffers.  If the given channel
      has no outputs, then this field will be <code>NULL</code>.
      Each buffer will be <code>veGetFrameSize()</code> units
      long.
   */
  float **buf;
} VeAudioChannelBuffer;

VeAudioChannelBuffer *veAudioChannelBufferCreate(struct ve_audio_channel *);
void veAudioChannelBufferZero(VeAudioChannelBuffer *);
void veAudioChannelBufferDestroy(VeAudioChannelBuffer *);

/** section Audio Engine
    Audio is combined together into streams appropriate for sending to
    hardware by an engine.  For both development and research purposes
    the interface to the engine is specified so that the engine can
    be loaded as a module at run-time.  To this end, the engine
    must be prepared to provide the functions in the
    <code>VeAudioEngine</code> structure.
    <p>For audio engines that are built as a module, they should
    provide an interface of type "aengine" (see the 
    <a href="ve_driver.h.html">ve_driver</a> module for more information
    on how to write drivers and specify what they "provide").
    <p>Note that the name "default" is reserved to mean the first
    available engine, or if no engines are available, the built-in
    "mix" engine (which mixes all output channels equally).
 */

typedef struct ve_audio_engine {
  char *name;
  void (*init)(struct ve_audio_channel *ch, VeOption *options); 
  /* startup on a channel */
  void (*deinit)(struct ve_audio_channel *ch); /* exit on a channel */
  /* Render the next frame of the given instance and mix it into the
     given buffer. */
  void (*process)(struct ve_audio_channel *ch, VeAudioInst *i,
		  VeAudioChannelBuffer *buf);
  /* recover any resources attached to an instance */
  void (*clean)(struct ve_audio_channel *ch,VeAudioInst *i);
} VeAudioEngine;

void veAudioEngineAdd(char *name, VeAudioEngine *);
VeAudioEngine *veAudioEngineFind(char *name);
int veAudioEngineInit(VeAudioEngine *, struct ve_audio_channel *, 
		      VeOption *options);

struct ve_audio_device;

/** struct VeAudioDriver
    Represents a particular driver for a device.
 */
typedef struct ve_audio_driver {
  char *name;
  /* creation and destruction */
  struct ve_audio_device *(*inst)(struct ve_audio_driver *drv,
				  VeOption *options);
  void (*deinst)(struct ve_audio_device *);
  /* lookup sub-channel name */
  int (*getsub)(struct ve_audio_device *, char *name);
  /* pushes a frame into the buffer of a given sub-channel */
  int (*buffer)(struct ve_audio_device *, int sub, float *data, int dlen);
  /* removes any frames from the buffer - generates silence after the
     currently playing frame */
  void (*flush)(struct ve_audio_device *, int sub);
  /* waits for device to be ready to buffer another frame */
  int (*wait)(struct ve_audio_device *);
} VeAudioDriver;

void veAudioDriverAdd(char *name, VeAudioDriver *);
VeAudioDriver *veAudioDriverFind(char *name);

/* for other functions, call the device object driver directly... */

/** struct VeAudioDevice
    Any audio output is ultimately delivered to a device, which is the
    interface to the actual hardware.  Audio devices come in two
    varieties.  Normal devices are simply audio sinks in that, any
    code could send frames to the device's output.  Pseudo-devices are
    those that are just placeholders which allow specification and
    control of an audio-rendering platform's built-in device support.
    For example, if an audio renderer were built around OpenAL, then
    typically the OpenAL layer would be responsible for delivering
    frames to the audio devices - that is, the engine is tightly
    coupled to the audio devices.  In these cases, it is not necessary
    for the VeAudioDevice object to be able to receive data from a VE
    application.
    <p>A special device called "default" always exists at the
    initialization of any VE application.  This device uses the "null"
    driver, meaning that it does not generate any real audio output,
    but instead silently discards any frames given to it.
    This default device can be overridden at run-time by defining
    a real device with the name "default".  In fact, it is expected
    that the VE initialization scripts will contain a series of
    optional devices called "default" in order of increasing
    preference, so that, if one exists, then it will stick.  If one
    doesn't stick, then the system falls back on the null driver.
 */
typedef struct ve_audio_device {
  /** member next
      Used internally to store devices in lists.  Applications should
      ignore this field.
   */
  struct ve_audio_device *next;
  /** member name
      A name for this device.  This should be unique at any point in
      time - that is, no two active devices should share the same
      name.  There is always an active device called "default".
      If no real audio devices are present, then the default device is
      the "null" device.
  */
  char *name;
  /** member driver
      A pointer to the driver for this device.  For pseudo-devices,
      this pointer is unnecessary and may be <code>NULL</code>.
   */
  struct ve_audio_driver *driver;
  /** member options
      A list of device-specific options that apply to this device.
   */
  struct ve_option *options;
  /** member devpriv
      A hook for drivers to attach private data.  This value
      should not be modified or interpreted by applications.
   */
  void *devpriv;
} VeAudioDevice;

VeAudioDevice *veAudioDevCreate(char *name, char *driver, VeOption *options);
VeAudioDevice *veAudioDevFind(char *name);
void veAudioDevDestroy(char *name);
int veAudioDevGetSub(VeAudioDevice *, char *name);
int veAudioDevBuffer(VeAudioDevice *, int sub, float *data, int dlen);
void veAudioDevFlush(VeAudioDevice *, int sub);
int veAudioDevWait(VeAudioDevice *);

/** struct VeAudioChannel
    An audio channel represents the fundamental method of audio
    rendering from the application's point-of-view.  
 */
typedef struct ve_audio_channel {
  struct ve_audio_channel *next;
  /** member name
      The name of the channel.
   */
  char *name;
  /** member slave
      A set of three strings identifying what multi-processing slave
      should be used for this channel.  A value of <code>NULL</code> or
      the string <code>"auto"</code> both mean the same thing:  that
      the specifics of the value should be decided by the system at
      run-time.
      
      @member node
      The name of the node on which rendering for this channel should be
      performed.

      @member process
      The name of the process in which this channel should be rendered.
      This is not a number but a name - channels with the same name
      for their process field will be rendered in the same process.
      The name "unique" is reserved to indicate that a unique process
      should be allocated for this channel and that it should not be
      shared with other channels.
      
      @member thread
      The name of the thread in which this channel should be rendered.
      This is not a number but a name - channels with the same name for
      their thread field will be rendered in the same thread.
      The name "unique" is reserved to indicate that a unique thread
      should be allocated for this channel and that it should not be
      shared with other channels.
   */
  struct {
    char *node;
    char *process;
    char *thread;
  } slave;
  /** member engine
      A pointer to the engine object that will be rendering this
      channel.
   */
  struct ve_audio_engine *engine;
  /** member engine_priv
      A hook for the rendering engine to associate information
      with the VeAudioChannel object.  Applications should neither
      modify nor interpret this object.
  */
  struct ve_audio_output *outputs;
  /** member options
      A list of channel-specific options.  These options may be
      interpreted by the engine, but otherwise have no predefined
      meaning.
   */
  struct ve_option *options;

  /** member id 
   */
  int id;
  /** member udata
      Internal hook.  Applications should neither inspect nor modify
      this value.
   */
  void *udata;
} VeAudioChannel;

/** struct VeAudioOutput
 */
typedef struct ve_audio_output {
  struct ve_audio_output *next;
  char *name;
  struct ve_audio_device *device;
  int devout_id;
  VeFrame frame;
  struct ve_option *options;
} VeAudioOutput;

/** function veAudioOutputFree
    Frees the memory associated with an unused VeAudioOutput
    object.
 */
void veAudioOutputFree(VeAudioOutput *);

/** function veAudioFindChannel
    Looks for previously initialized audio channels.
 */
VeAudioChannel *veAudioFindChannel(char *name);

/** function veAudioChannelReg
    Registers a VeAudioChannel object with the library. 
    Note that this just associates a channel with the 
    <i>name</i> of the channel, but does not yet initialize it.
    This allows later channels with the same name to override
    existing channels before initialization.
    <p>Applications typically do not need to call this function
    as this is usually handled by VE initialization scripts.
 */
void veAudioChannelReg(VeAudioChannel *);

/** function veAudioChannelInit
    Initializes an audio channel.  This initializes the underlying
    engine and prepares devices for receiving output.  Applications
    typically do not need to call this function as it is called
    as needed by <code>veAudioInit()</code>.
 */
int veAudioChannelInit(VeAudioChannel *);

/** function veAudioChannelFree
    Frees a previously created audio channel.  The audio channel
    should not be initialized.
*/
void veAudioChannelFree(VeAudioChannel *);

/** section Internals
    The following isn't necessary to know in order to use the 
    application interface, but it is necessary to understand
    the data structures.
    Sounds are loaded into memory and then adjusted to the
    internal sampling frequency and sample format.  In the
    time-domain a sound is broken down into frames.  A frame
    is the unit on which processing is done.  A new instance
    of a sound may only be created on the boundary of a frame.
    A frame is some fixed number of samples.  Any sound that
    is not an even number of frames is padded with silence to
    make a round number of frames.  This does not typically need
    to known at the application layer - the library hides these
    details.
    <p>
    The frame size can only be set before initialization time.
    (see <code>veAudioSetFrameSize()</code>).  The frame size is determined
    by either the <code>-ve_opt audio_frame</code> command-line
    option or the <code>VE_AUDIO_FRAME</code> environment variable.
    If both are specified, then the command-line option takes
    precedence.  If neither are specified a built-in default value
    is used (see <code>veAudioGetFrameSize()</code>).
    Similarly, the sampling frequency at which processing is done
    faces similar restrictions (see <code>veAudioSetSampFreq()</code>).
    <p>
    Internally the library uses a floating-point representation
    of sound intensity.  That is, a single sample is represented
    by a float.  All sounds are processed as though they were 
    mono.  Stereo sound files are downmixed to audio when they
    are loaded.
*/

/** function veAudioSetFrameSize
    Sets the default frame size.  This may only be called before
    <code>veAudioInit()</code>.  Keep in mind that 
    <code>veAudioInit()</code> is called by <code>veInit()</code>.
    This is one of the few functions that can be called (and in fact 
    can only be called) before <code>veInit()</code>.
    <p>Note that this value can still be overridden by the
    command-line or the appropriate environment variable.

    @param size
    The new size of the frame in samples.

    @returns
    0 if successful, non-zero if the sample size cannot be set
    as requested.
 */
int veAudioSetFrameSize(int sz);

/** function veAudioGetFrameSize
    Allows an application to query the library's frame size
    for audio processing.  This function only returns a reliable
    value after a call to <code>veAudioInit()</code> or
    <code>veInit()</code>.  This is one of the few functions that
    can be safely called before <code>veInit()</code>.

    @returns
    The current frame size.
 */
int veAudioGetFrameSize(void);

/** function veAudioSetSampFreq
    Sets the default sampling frequency for processing.  
    This may only be called before
    <code>veAudioInit()</code>.  Keep in mind that 
    <code>veAudioInit()</code> is called by <code>veInit()</code>.
    This is one of the few functions that can be called (and in fact 
    can only be called) before <code>veInit()</code>.
    <p>Note that this value can still be overridden by the
    command-line or the appropriate environment variable.

    @param freq
    The new sampling frequency in Hz.
    
    @returns
    0 if successful or non-zero if the frequency cannot be set.
    Note that if the precise frequency is not available but 
    another frequency that is "close enough" is available, the
    library will silently choose that "close enough" frequency
    and return 0.  Use the <code>veAudioGetSampFreq()</code>
    function to determine the exact frequency.
 */
int veAudioSetSampFreq(int freq);

/** function veAudioGetSampFreq
    Allows an application to query the library's sampling frequency
    for audio processing.  This function only returns a reliable
    value after a call to <code>veAudioInit()</code> or
    <code>veInit()</code>.

    @returns
    The current sampling frequency.
 */
int veAudioGetSampFreq(void);

/** struct VeSound
    Represents a single sound.  This is the data for the sound
    that is stored in memory.
*/
typedef struct ve_sound {
  /** member id
      An internally assigned integer identifier for this
      sound.  Applications should ignore this field and leave
      it unchanged.  It is an offset into an internal table of
      sounds.
  */
  int id;
  /** member name
      A unique name that identifies this sound.  For 
      sounds loaded from a file this is the file name.
      For sounds created by the application, this value is
      optional.  If it is <code>NULL</code> it means that
      the sound cannot be retrieved by name (see
      <code>veAudioLookup()</code> and <code>veAudioFind()</code>).
   */
  char *name;
  /** member file
      If this is not <code>NULL</code> then it is the path of
      a file from which the data was loaded.  If it is
      <code>NULL</code> then the data was created in the program.
      This is set appropriately by the sound creation utilities
      (<code>veAudioLoadFile()</code> and <code>veAudioLoadRaw()</code>).
   */
  char *file;
  /** member nframes
      The number of frames that make up this sound.
   */
  int nframes;
  /** member data
      An array of data which must be at least
      <i>nframes</i>*<i>framesize</i> samples long.
   */
  float *data;
  /** member refcnt
      Used to ensure that a sound is not disposed before everything
      is finished with it.
   */
  int refcnt;
} VeSound;

/** function veAudioLoadFile
    Tries all built-in methods for loading the given file as a sound.
    The specific methods available are system-dependent.

    @param file
    The name of the file to load.

    @returns
    A pointer to a sound object if successful, or <code>NULL</code>
    if not.
 */
VeSound *veSoundLoadFile(char *file);

/* audio formats: 8 bits, 16 bits, 24 bits, 32 bits, and real values */
#define VE_AUDIO_8BIT  (0)
#define VE_AUDIO_16BIT (1)
#define VE_AUDIO_24BIT (2)
#define VE_AUDIO_32BIT (3)
#define VE_AUDIO_FLOAT (4)

/* endian */
#define VE_AUDIO_NATIVE (-1)
#define VE_AUDIO_LITTLE (0)
#define VE_AUDIO_BIG    (1)

/** function veAudioLoadRaw
    Creates a sound from raw data.
 */
VeSound *veSoundLoadRaw(int freq, int nsmp, int sampfmt, int endian, 
			int len, void *buf);
void veSoundUnref(VeSound *);
void veSoundFree(VeSound *);

/* abstract loading */

/* maximum number of characters used in storing a sound's name
   (must be unique in these chars) */
#define VE_SNDNAME_SZ 64

/** function veSoundAdd 
    Adds a sound to the sound table.
*/
int veSoundAdd(char *name, VeSound *sound); /* returns id on success */
VeSound *veSoundFindName(char *name);

/* short-hand for loading from file and linking */
int veSoundCreate(char *name, char *file);

/** section Instances

    Each sound is played by creating an <i>instance</i> of it.
    This instance has a set of <i>parameters</i> associated with it.
    (See the <code>VeAudioParams</code> structure below for details on
    available parameters.)  Once created, an application may:
    <ul>
    <li>change paramters of an instance while it plays</li>
    <li>request early termination of playback of an instance</li>
    <li>test to see if an instance has stopped playing</li>
    <li>wait for an instance to stop playing</li>
    </ul>
    Most audio rendering engines will have a limit on the number of
    instances that can play simultaneously.  Even if it finishes
    playing, an instance persists in the engine until it is 
    "cleaned" by the application.  By "cleaning" an instance, the
    application is relinquishing the slot and can no longer modify
    or query that instance.  Thus it is possible to clean an instance
    before it finishes playing, and the engine will automatically
    dispose of its resources as soon as it finishes - however, in
    this scenario, it would be impossible for the application to
    wait on or check the status of the instance.
    <p>Instances are represented to the application by an identifier
    of type <code>int</code>.</p>
 */

/* "source" here is not necessarily a good model
   (e.g. sounds that have different transmission characteristics in
   different directions) - there needs to be some concept of 
   "spread" as well.
*/
typedef struct ve_audio_params {
  int loop;
  float volume;
  VeFrame source;
  float spread; /* in degrees? 0.0 == even transmission in every direction */
} VeAudioParams;

void veAudioInitParams(VeAudioParams *);
void veAudioSetVolume(VeAudioParams *, float);
float veAudioGetVolume(VeAudioParams *);
void veAudioSetLoop(VeAudioParams *, int);
int veAudioGetLoop(VeAudioParams *);
VeFrame *veAudioGetFrame(VeAudioParams *);
float veAudioGetSpread(VeAudioParams *);
void veAudioSetSpread(VeAudioParams *, float);

/* manipulating instances... */
int veAudioInst(char *chname, VeSound *snd, VeAudioParams *);

/** function veAudioGetParams
    Returns the current parameter set for the given audio
    instance.

    @param instid
    The id of an audio instance created by 
    <code>veAudioInst()</code>.

    @param params_r
    A pointer to a <code>VeAudioParams</code> structure that
    will be filled in with the current parameters for this
    instance.

    @returns
    0 on success, non-zero on failuer (e.g. if <i>instid</i>
    is not a valid instance).
 */
int veAudioGetParams(int instid, VeAudioParams *params_r);
int veAudioUpdate(int instid, VeAudioParams *params);
int veAudioStop(int instid, int clean);
int veAudioIsDone(int instid);
int veAudioClean(int instid);
int veAudioWait(int instid); /* wait for it to finish */

/** section Global Control
    This section discusses functions that affect all audio output.
 */

/** function veAudioMute
    Toggles global "mute" flag.  This prevents the application from
    actually sending output to the audio devices, but all processing
    progresses as it would otherwise.  Some devices may implement this
    as sending "0" data to audio devices to maintain synchronization.
 */
int veAudioMute(int flag);

/** function veAudioInit
    Initializes the audio layer.  This function must be called before 
    any other function call in the audio layer except for those 
    explicitly labelled as being available for use before initialization.
    This function is typically called by <code>veInit()</code>.
 */
void veAudioInit(void);

/** function veMPAudioRun
    Initializes the MP audio renderer.  This starts up slaves.
 */
void veMPAudioRun(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_AUDIO_H */
