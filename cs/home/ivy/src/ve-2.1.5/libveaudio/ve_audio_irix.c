/* The IRIX-specific component of the Audio library.  Uses the
   AL (Audio Library) and AF (Audio File Library) library for most of the
   functionality.
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include <dmedia/audio.h>
#include <dmedia/audiofile.h>

#include <ve_audio.h>
#include <ve_audio_int.h>

static int noutputs = 0;
VeAudioOutput *outputs = NULL;

typedef struct devinfo_s {
    int resid;          /* resource ID of device */
    char *name;         /* name of device */
    int nchans;         /* number of channels */
    ALport p;           /* audio port connected to this device */
    stamp_t offset;
} devinfo_t;

static int device_setup(devinfo_t *dev)
{
    int r;
    ALport alp;
    ALconfig c;
    ALpv p;

    /* get the device associated with the given name */
    if (dev->name) {
      r = alGetResourceByName(AL_SYSTEM, dev->name, AL_DEVICE_TYPE);
      if (!r) {
        return -1;
      }
      dev->resid = r;
      /* if the name refers to an interface, select that interface on the device */
      if (r = alGetResourceByName(AL_SYSTEM, dev->name, AL_INTERFACE_TYPE)) {
        p.param = AL_INTERFACE;
        p.value.i = r;
        alSetParams(dev->resid, &p, 1);
      }
    } else {
      dev->resid = AL_DEFAULT_OUTPUT;
    }


    c = alNewConfig();
    if (!c) {
        return -1;
    }

    /* find out how many channels the device is. */
    p.param = AL_CHANNELS;
    if (alGetParams(dev->resid, &p, 1) < 0 || p.sizeOut < 0) {
        /* AL_CHANNELS unrecognized or GetParams otherwise failed */
        return -1;
    }

    dev->nchans = p.value.i;

    alSetChannels(c, dev->nchans);
    alSetDevice(c, dev->resid);
    alSetSampFmt(c, AL_SAMPFMT_FLOAT);

    alp = alOpenPort("monitor","w",c);
    if (!alp) {
        return 0;
    }

    alFreeConfig(c);

    dev->p = alp;
    return 0;
}

extern void *veAudio_player_thread(void *);

void veAudioImplInit() {
  /* create a single output */
  int i;
  ALpv pv[2];
  devinfo_t *dev;
  pthread_t pth;

  noutputs = 1;
  outputs = malloc(sizeof(VeAudioOutput)*noutputs);
  assert(outputs != NULL);

  outputs->name = "default";
  pthread_mutex_init(&outputs->mgmt_mutex,NULL);
  pthread_cond_init(&outputs->sounds_avail,NULL);
  for(i = 0; i < VE_AUDIO_MAX_INST; i++) {
    outputs->instances[i].clean[0] = outputs->instances[i].clean[1] = 1;
    outputs->instances[i].active = 0;
  }

  dev = malloc(sizeof(devinfo_t));
  assert(dev != NULL);
  dev->name = NULL;
  if (device_setup(dev)) {
    fprintf(stderr, "could not open device %s\n", dev->name ? dev->name :
	    "<null>");
    exit(1);
  }
  outputs->priv = dev;

  pv[0].param = AL_RATE;
  pv[0].value.ll = alDoubleToFixed((double)VE_AUDIO_FREQ);
  pv[1].param = AL_MASTER_CLOCK;
  pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
  alSetParams(dev->resid, pv, 2);

  pthread_create(&pth,NULL,veAudio_player_thread,outputs);
}

void veAudioImplPlay(VeAudioOutput *out, VeAudioSample *data, int dlen) {
  /* dlen is size of data, but we pass number of frames to alWriteFrames */
  devinfo_t *dev = (devinfo_t *)(out->priv);
  alWriteFrames(dev->p,data,dlen/2);
}

void veAudioImplNextFrame(VeAudioOutput *out) {
  /* wait until there is less than a frame in the buffer */
  devinfo_t *dev = (devinfo_t *)(out->priv);
  while(alGetFilled(dev->p) > VE_AUDIO_FRAME_LENGTH)
    usleep(10); /* something very small... */
}

VeAudioOutput *veAudioGetOutput(char *name) {
  int i;
  for(i = 0; i < noutputs; i++) 
    if (strcmp(outputs[i].name,name) == 0)
      return &outputs[i];
  return NULL;
}

VeAudioOutput *veAudioDefaultOutput() {
  return &outputs[0];
}

VeAudioSound *veAudioLoad(char *name) {
  AFfilehandle fh;
  VeAudioSound *snd;
  int n, i;

  fh = afOpenFile(name,"r",0);
  if (!fh) {
    fprintf(stderr, "could not open audio file %s\n", name);
    exit(1);
  }
  afSetVirtualSampleFormat(fh, AF_DEFAULT_TRACK, AF_SAMPFMT_FLOAT, 4);
  afSetVirtualRate(fh, AF_DEFAULT_TRACK, (double)VE_AUDIO_FREQ);
  
  snd = malloc(sizeof(VeAudioSound));
  assert(snd != NULL);
  n = afGetChannels(fh,AF_DEFAULT_TRACK);
  if (n == 1)
    snd->type = VE_AUDIO_MONO;
  else if (n == 2)
    snd->type = VE_AUDIO_STEREO;
  else {
    fprintf(stderr, "unsupported file format - %d channels\n", n);
    exit(1);
  }
  
  n = afGetFrameCount(fh, AF_DEFAULT_TRACK);
  snd->nframes = n/VE_AUDIO_FRAME_LENGTH;
  if (n % VE_AUDIO_FRAME_LENGTH)
    snd->nframes++;
  snd->frames = malloc(sizeof(VeAudioFrame)*snd->nframes);
  for(i = 0; i < snd->nframes; i++) {
    n = afReadFrames(fh,AF_DEFAULT_TRACK,snd->frames[i].data,VE_AUDIO_FRAME_LENGTH);
    snd->frames[i].dlen = n;
    if (n == -1) {
      fprintf(stderr, "failed to read audio file %s: %s\n", name,
	      strerror(errno));
    }
    /* pad out any short frames - should only be the last */
    while (n < VE_AUDIO_FRAME_LENGTH)
      snd->frames[i].data[n++] = 0;
  }
  afCloseFile(fh);

  snd->name = strdup(name);

  return snd;
}
