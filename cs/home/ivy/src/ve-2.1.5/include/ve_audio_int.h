#ifndef VE_AUDIO_INT_H
#define VE_AUDIO_INT_H

/** misc
    This header file contains internal structures for the library.
    In general, these structures should not be accessed by applications.
    There are however, some macros in this header which can help in
    tuning performance of the library.  Some platforms may choose to
    ignore these parameters if they do not use the generic library mixers
    (e.g. if they are using hardware mixers).
    If these parameters are changed than the library and programs dependent
    upon the library will need to be recompiled for them to take effect.
    The following parameters are the important ones:
<dl>
<dt><b>VE_AUDIO_MAX_INST</b></dt>
<dd>The number of instances that can be played simultaneously.  More
instances will require more memory and more computing time to mix.
</dd>
<dt><b>VE_AUDIO_FREQ</b></dt>
<dd>The frequency at which samples are played back, expressed as an integer
in Hz.  Be careful adjusting this parameter unless you are positive that
the value you are setting is supported on your platform.  A higher frequency
requires more processing time and memory but provides better quality output.
</dd>
<dt><b>VE_AUDIO_FRAME_LENGTH</b></dt>
<dd>The block-size that the mixer will use in sample units.  The
playback thread tries to stay one block ahead to avoid drop-outs.  This
means that the maximum latency of the system is roughly twice the frame
length divided by the sample frequency.  For example, if the frame length
is 500 samples, and the frequency is 44100 Hz, then the worst case latency
for witnessing the effect of a change to an instance, is about 226 ms.
Adjusting the frame length is tricky - changing the frame length does not
generally affect the overall computing time required for the mixer (the
same number of samples get mixed no matter what) unless you go to an
extremely small frame length (i.e. where the overhead of a call to the
mixer becomes significant compared to the mixer work itself).  Smaller
frame lengths provide better latency for audio, but require more frequent
interrupts by the thread.  Systems with other large tasks or frequent
high-priority tasks may benefit from a larger frame length.
</dd>
</dl>
<p>Some of these parameters should admittedly be adjustable at run-time
rather than compilation-time.  This will be addressed in a future revision.
*/

/* Internal structures for the VE Audio library - applications should treat 
   these as transparent */
#include <pthread.h>

/* --------------- */

/* This section contains "tunable" paramters.  Future code may make these
   tunable "on the fly" but for now, they require recompilation of the
   library.  Note that half-assed attempts to resample soundfiles that are
   bang on the sample-size or frequency will be made, but this is better
   done externally to the library by a decent audio editor. */

/* Note that certain platforms will have certain parameters that they
   "like". */
#define VE_AUDIO_MAX_INST     64          /* in effect, the max sounds that
					     can be played at once */
#define VE_AUDIO_SAMPLE_SIZE  16          /* size of a sample in bits */
#define VE_AUDIO_SAMPLE_MAG   1.0         /* largest value */
#define VE_AUDIO_SAMPLE_TYPE  float       /* data type to use for samples */
#define VE_AUDIO_FREQ         44100       /* frequency at which to render 
					     data */
/* we work in units of "frames" - all processing is done on a per-frame
   basis (i.e. parameters cannot vary over a frame).  Smaller frame sizes
   require more interrupts/processing but provide better resolution (both
   in time changes and start/stop times. */
/* Note that this value is for a single stream.  For interleaved stereo,
   an actual frame will have 10000 sample units (but 2 sample units are
   played simultaneously). */
#define VE_AUDIO_FRAME_LENGTH 500        /* a little less than 1/80th of a second at 44.1kHz */

/* Following are tunable, but *highly* platform dependent */
#define VE_AUDIO_NCHAN        2           /* number of output channels,
					     typically 2 for stereo systems */
#define VE_AUDIO_FMT_IS_INT   0           /* define as 0 or 1, 1 implies
					     floating values */

/* --------------- */

typedef VE_AUDIO_SAMPLE_TYPE VeAudioSample;
typedef struct {
  VeAudioSample data[2*VE_AUDIO_FRAME_LENGTH];
  int dlen; /* should always be VE_AUDIO_FRAME_LENGTH */
} VeAudioFrame;

typedef struct ve_audio_params {
  /* 2D parameters */
  float volume_f;
  float pan_f;
  int loop;
} VeAudioParams;

typedef enum {
  VE_AUDIO_MONO,     /* standard 1-channel audio */
  VE_AUDIO_STEREO    /* standard 2-channel audio */
} VeAudioSoundType;

typedef struct ve_audio_sound {
  char *name;
  VeAudioSoundType type;
  int nframes;
  VeAudioFrame *frames;
} VeAudioSound;

typedef struct ve_audio_inst {
  struct ve_audio_output *output;
  VeAudioSound *sound;
  VeAudioParams params;
  int next_frame; /* < 0 implies that instance is "dead" */
  int clean[2];   /* clean-up bits */
  int active;
  pthread_cond_t finished_cond;
  void *appdata;  /* for applications to tie something to an instance */
} VeAudioInst;

typedef struct ve_audio_output {
  char *name;
  VeAudioInst instances[VE_AUDIO_MAX_INST];
  pthread_mutex_t mgmt_mutex;
  void *priv; /* private data for platform-dependent stuff */
  pthread_cond_t sounds_avail;
} VeAudioOutput;

/* implementations provide:
   veAudioLoad(), veAudioGetOutput(), plus:
*/

void veAudioImplInit(); /* platform-specific initialization */
/* play a burst of data */
void veAudioImplPlay(VeAudioOutput *out, VeAudioSample *data, int dlen);
/* do not return until we should begin calculating the next frame */
void veAudioImplNextFrame(VeAudioOutput *out);

#endif /* VE_AUDIO_INT_H */

