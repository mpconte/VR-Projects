#ifndef VE_AUDIO_H
#define VE_AUDIO_H

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif /* __cplusplus */

/** misc
    The VE Audio library provides a simple interface for generating sounds.
    Right now this library
    generates sound locally on the same machine as the VE application.  A
    future revision will provide support for a client-server relationship
    whereby the sounds can be generated on a remote machine but controlled
    from the VE application.
    <p>
    This library is actually independent of the VE library itself.  It
    can be used without VE, and if used in conjunction of VE, it needs
    to be explicitly included and linked.
    <p>
    This library provides a 2D sound interface, not a 3D one.  A future
    project will look at providing a more comprehensive sound library with
    3D support.
    <p>
    The general model for the library is that sounds are loaded
    into memory and then instantiated as needed.  A simple example:
<pre>
    ...
    VeAudioSound *a;
    VeAudioInstance *a_inst;

    a = veAudioFind("a.wav");
    a_inst = veAudioInstance(VE_AUDIO_DEFAULT_OUTPUT, a,
                             VE_AUDIO_DEFAULT_PARAMS);
    while (!veAudioIsDone(a_inst)) {
       VeAudioParams *p;

       ... decide how to modify sound ...

       p = veAudioGetInstParams(a_inst);
       veAudioSetVolume(p,new_volume);
       veAudioSetPan(p,new_pan);
       veAudioRefreshInst(a_Inst);
    }
    veAudioClean(a_inst,0);
</pre>
    <p>Once sounds are instantiated, their parameters (volume, pan) can
    be modified as they play.  Sounds can also be paused in playback or 
    aborted.  Any instantiated sound must be cleaned up with a call to
    <code>veAudioClean()</code>.  This can either be an immediate clean-up
    (i.e. aborts playback and frees the instance) or a delayed clean-up
    (cleans up automatically when playback finishes).

    <p>Some data-types (VeAudioSound, VeAudioInst, VeAudioOutput, 
    VeAudioParams) should be treated as opaque.  Their details are covered
    in the <code>ve_audio_int.h</code> header file.
*/    

/* VeAudioSound, VeAudioInst, VeAudioOutput, VeAudioParams defined elsewhere */
#include <ve_audio_int.h>

/* The base API... */
#define VE_AUDIO_ERROR (0)
#define VE_AUDIO_DEFAULT_OUTPUT ((VeAudioOutput *)0)
#define VE_AUDIO_DEFAULT_PARAMS ((VeAudioParams *)0)

/* configuration */
/** function veAudioInit
    This function must be called before any other functions in the audio
    library are called.
*/
void veAudioInit();  /* must be called before anything else... */

/** function veAudioGetOutput
    The audio system can have any number of outputs.  If a specific output
    is desired it must be looked up by name using this function.  Output
    names are platform-specific.  All functions which require an output
    as an argument will also accept the macro 
    <code>VE_AUDIO_DEFAULT_OUTPUT</code> as a valid output.
    
    @param name
    The name of the output to lookup.
    
    @returns
    A pointer to a VeAudioOutput object if the output was found.  This pointer
    should <i>not</i> be freed by the application and should generally be
    treated as immutable.  If no such output was found, then a NULL pointer
    is returned.
*/
VeAudioOutput *veAudioGetOutput(char *name);  /* find a particular output */

/** function veAudioDefaultOutput
    This function returns the VeAudioOutput object corresponding to the
    default output.
    
    @returns
    A pointer to the default audio output.
 */
VeAudioOutput *veAudioDefaultOutput();

/* Note that the meaning of "dev" and "port" are entirely platform-dependent */

/** function veAudioInitParams
    Resets a VeAudioParams object.  The current defaults are:  volume at full
    (1.0), pan in the centre, and a non-looping sound.

    @param p
    A pointer to the audio parameter object to reset.
*/
void veAudioInitParams(VeAudioParams *p);

/** function veAudioGetInstParams
    Retrieves a pointer to an audio instance's set of parameters.  This
    pointer can be used to adjust an instance's volume, pan, etc.
    
    @param i
    The audio instance to query.
    
    @returns
    A pointer to the instance's audio parameters.
*/
VeAudioParams *veAudioGetInstParams(VeAudioInst *i);

/** function veAudioRefreshInst
    After modifying an instance's audio parameters, it must be explicitly
    refreshed so that the parameters take effect.  Depending upon the
    platform and parameters this may be a no-op, or it may be a moderately 
    expensive operation.  
    
    @param i
    The audio instance to refresh.
*/
void veAudioRefreshInst(VeAudioInst *i);

/* The 2D API */
/* volume = [0.0,1.0] - 0.0 = no sound, 1.0 = full volume
   pan = [-1.0,1.0] - -1.0 = full left, 0.0 = balanced, 1.0 = full right
*/

/** function veAudioGetVolume
    Returns the volume of an existing audio parameter set.
    The volume is real value from 0.0 to 1.0, inclusive.  A value
    of 0.0 is no volume - i.e. silence, and a value of 1.0 is maximum
    volume.  The library will currently not scale sound samples beyond their 
    original volume.

    @param p
    A pointer to the audio parameter set.
    
    @returns
    The volume as a real value (0.0 - 1.0).
*/
float     veAudioGetVolume(VeAudioParams *p);

/** function veAudioSetVolume
    Sets the volume of an existing audio parameter set.
    The volume is real value from 0.0 to 1.0, inclusive.  A value
    of 0.0 is no volume - i.e. silence, and a value of 1.0 is maximum
    volume.  The library will currently not scale sound samples beyond their 
    original volume.

    @param p
    A pointer to the audio parameter set.

    @param volume
    The new volume as a real value (0.0 - 1.0).
*/
void      veAudioSetVolume(VeAudioParams *p, float volume);

/** function veAudioGetPan
    Retrieves the stereo pan of an existing audio parameter set.
    The pan is a real value from -1.0 to 1.0, inclusive, with -1.0
    being full left channel, 0.0 being balanced between both channels,
    and 1.0 being full right channel.  The library will attenuate signals
    in a channel but will not currently amplify them.

    @param p
    A pointer to the audio parameter set.

    @returns
    The stereo pan as a real value (-1.0 - 1.0).
*/
float     veAudioGetPan(VeAudioParams *p);

/** function veAudioSetPan
    Sets the stereo pan of an existing audio parameter set.
    The pan is a real value from -1.0 to 1.0, inclusive, with -1.0
    being full left channel, 0.0 being balanced between both channels,
    and 1.0 being full right channel.  The library will attenuate signals
    in a channel but will not currently amplify them.

    @param p
    A pointer to the audio parameter set.

    @param pan
    The stereo pan as a real value (-1.0 - 1.0).
*/
void      veAudioSetPan(VeAudioParams *p, float pan);

/** function veAudioGetLoop
    Returns the loop flag of the existing audio parameter set.
    This is a boolean value - if non-zero then an instance will
    loop back to the beginning upon reaching the end of the sample
    instead of stopping.  If the value is zero then the behaviour
    is the default - when an instance reaches the end of the sample.
    
    <p>Currently looping support is rudimentary and may very well
    result in small gaps between the end of a sample and the time
    it restarts.

    @param p
    A pointer to the audio parameter set.

    @returns
    The state of looping for this parameter set (0 = off, non-zero = on).
*/
int       veAudioGetLoop(VeAudioParams *p);

/** function veAudioSetLoop
    Sets the loop flag of the existing audio parameter set.
    This is a boolean value - if non-zero then an instance will
    loop back to the beginning upon reaching the end of the sample
    instead of stopping.  If the value is zero then the behaviour
    is the default - when an instance reaches the end of the sample.
    
    <p>Currently looping support is rudimentary and may very well
    result in small gaps between the end of a sample and the time
    it restarts.

    @param p
    A pointer to the audio parameter set.

    @param loop
    The state of looping for this parameter set (0 = off, non-zero = on).
*/
void      veAudioSetLoop(VeAudioParams *p, int loop);

/** function veAudioLoad
    Creates a new sound by loading a file from the disk.  A sound needs
    to be loaded into memory before it can be instantiated.  This call
    should only be used if an audio file is expected to change on disk in the
    course of an application's execution.  Otherwise the 
    <code>veAudioFind()</code> call should be used instead.
    
    @param name
    The name of the audio file to load.
    
    @returns
    A pointer to a newly created VeAudioSound object.  If an error is
    encountered, a <code>NULL</code> pointer is returned.
*/
VeAudioSound *veAudioLoad(char *name);

/** function veAudioFind
    This function works in a similar manner to <code>veAudioLoad()</code>
    except that it will only load the file from the disk if it has not
    been previously loaded.  If a sound has previously loaded by a call
    to either <code>veAudioLoad()</code> or <code>veAudioFind()</code>
    then the previously loaded sound will be returned.  Unless specific
    circumstances exist where a sound on disk may change over time,
    applications should use this function instead of <code>veAudioLoad()</code>
    to avoid wasting memory.
    
    @param name
    The name of the audio file to find.

    @returns
    A pointer to the corresponding VeAudioSound object.  If the object
    does not currently exist, then it is loaded (if possible).  If an
    error is encountered, a <code>NULL</code> pointer is returned.
*/
VeAudioSound *veAudioFind(char *name); /* find ref if it exists, 
					  load if it doesn't */
/* App is responsible for ensuring no further instances exist before
   disposing of a sound. */
/** function veAudioDispose
    Frees resources associated with a sound object and removes it from
    memory.  After this call, this sound may not be instantiated without
    reloading it through either <code>veAudioLoad()</code> or
    <code>veAudioFind()</code>.  It is up to the application to ensure that
    there are no instances currently referencing this sound before disposing
    of it.
    
    @param sound
    A pointer to the sound object to dispose of.
*/
void veAudioDispose(VeAudioSound *sound);

/** function veAudioInstance
    Creates a new instance of a sound.  An instance is an actual playback
    of a loaded sound.  Playback is handled in the background under a 
    separate thread.

    @param output
    The audio output upon which to play a sound.  Currently this must be
    fixed during the existence of an instance of a sound.  Different
    instances of a sound can be playing simultaneously on different outputs
    or on the same output, but each instance can only play on a single output.
    The macro <code>VE_AUDIO_DEFAULT_OUTPUT</code> can be used to pick
    the default output of the audio system.  Otherwise, the argument
    must be an object that was retrieved from a call to 
    <code>veAudioGetOutput()</code>.

    @param sound
    The sound to instantiate.  This sound must have previously been
    loaded from a call to <code>veAudioLoad()</code> or 
    <code>veAudioFind()</code>.  This must be a non-NULL value.
    
    @param params
    The initial audio parameter set for this instance.  This data will
    be copied into a new audio parameter set, so the same parameter set
    can be used to instantiate several sounds.  The macro
    <code>VE_AUDIO_DEFAULT_PARAMS</code> can be used to indicate that
    the default parameters should be used.

    @returns
    A pointer to new VeAudioInst object.  This object will begin playing
    immediately.  A <code>NULL</code> pointer will be returned in the
    event of failure.  Creation of an instance will generally only fail
    if internal limits on the number of simlutaneous instances are reached.
*/
VeAudioInst *veAudioInstance(VeAudioOutput *output, VeAudioSound *sound, 
			     VeAudioParams *params);

/** function veAudioPause
    Pauses playback of an audio instance.  This does not clean an
    existing audio instance.  The instance will remain paused until
    aborted by a call to <code>veAudioClean()</code> or until continued
    with a call to <code>veAudioCont()</code>.
    
    @param i
    The instance to pause.
 */
void      veAudioPause(VeAudioInst *i);

/** function veAudioCont
    Continues playback of a previously paused audio instance.  If the
    instance is already playing then there is no effect.  Playback
    continues at the point where the instance was paused.
    
    @param i
    The instance to continue.
*/
void      veAudioCont(VeAudioInst *i);

/** function veAudioWait
    Blocks until the given instance has finished playing.

    @param i
    The instance the wait on.
*/
void      veAudioWait(VeAudioInst *i);

/** function veAudioClean
    Cleans a previously created instance.  The <b>now</b> argument indicates
    when the instance should be cleaned.  If non-zero, then the instance will
    be cleaned immediately - that is, it is effectively an abort.  If zero,
    then the audio will be cleaned when it finishes playing.  Note that
    in this event, the call will return immediately and the player thread
    will clean the instance when it finishes with it.
    <p>Note that cleaning an instance ultimately deallocates the memory
    associated with that instance and invalidates its pointer.  Applications
    should consider any instance passed to this function as destroyed and
    no longer accessible, even if the cleaning has been put off until
    playback has finished.  That means that no other access can be made
    to this instance, even checking to see if it has completed playing.
    Thus, only clean an instance when you no longer need to access it.
    <p>Every instance must be cleaned to recover the memory associated with
    it and the resources it occupies.  The default mixer
    has an upper-limit on the number of instances that can
    simultaneously exist.

    @param i
    The instance to clean.
    
    @param now
    When to clean the instance. A non-zero value indicates that playback should
    be aborted and the instance cleaned immmediately.  A zero value indicates
    that the system should wait until the instance finishes playback on its
    own before cleaning it.  If playback has already finished, then the
    instance is cleaned immediately and this parameter has no effect.
*/
void      veAudioClean(VeAudioInst *i, int now);

/** function veAudioIsDone
    A test to see if an existing un-cleaned instance is still playing.
    Note that a paused instance is not considered to be done.

    @param i
    The instance to check.
    
    @returns
    A zero value if the instance is still playing.  A non-zero value if
    the instance has finished playback.
*/
int       veAudioIsDone(VeAudioInst *i);
/* Convenience:  play the full clip of given sound on the default output device */

/** function veAudioPlayFull
    A convenience function.  Instantiates the given sound with the default
    parameters on the default output and immediately sets it to be cleaned
    when playback finishes.  This is useful for when you just want to
    playback a sound and have no interest in tracking its completion or
    modifying its parameters on the fly.
    
    @param sound
    The sound to instantiate.
*/
void      veAudioPlayFull(VeAudioSound *sound);

/* A 3D API should come later... */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_AUDIO_H */

