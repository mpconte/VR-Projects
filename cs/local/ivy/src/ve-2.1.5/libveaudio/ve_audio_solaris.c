/* The Solaris-specific component of the Audio library.  Uses the
   SGI audiofile library for reading files
   and basic Solaris audio calls for the rest.

   This port is currently unfinished and will not compile.
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>

#include <audiofile.h>
#include <sys/audio.h>

#include <ve_audio.h>
#include <ve_audio_int.h>

static int noutputs = 0;
VeAudioOutput *outputs = NULL;
static int swab_out = 0;

typedef struct devinfo_s {
  char *name;         /* name of device */
  char *path;         /* path to device */
  int fd;             /* file descriptor to open path on device */
  int nchans;         /* number of channels */
  short *convbuf;     /* buffer used to convert from internal float mixer */
} devinfo_t;

static char *known_audio_devs[] = {
  "/dev/audio",
  "/dev/dsp",
  "/dev/audio1",
  NULL
};

static int device_setup(devinfo_t *dev)
{
  int i,dat,x;
  if (dev->path) {
    if ((dev->fd = open(dev->path,O_WRONLY)) < 0) {
      fprintf(stderr, "failed to open path %s\n", dev->path);
      return -1; /* failed to open specified output */
    }
  } else {
    for(i = 0; known_audio_devs[i]; i++)
      if ((dev->fd = open(known_audio_devs[i],O_WRONLY)) >= 0)
	break;
    if (dev->fd < 0) {
      fprintf(stderr, "could not find any known audio devices\n");
      return -1;
    }
  }

  /* configure output device */
  swab_out = 0;
  x = dat = AFMT_S16_NE;
  if ((ioctl(dev->fd,SNDCTL_DSP_SETFMT,&dat) < 0) || x != dat) {
    swab_out = 1;
    if (ioctl(dev->fd,SNDCTL_DSP_GETFMTS,&dat) < 0) {
      fprintf(stderr, "failed to read available audio formats\n");
      return -1;
    }
    x = 0;
    if (!x && (dat & AFMT_S16_LE)) {
      x = dat = AFMT_S16_LE;
      if ((ioctl(dev->fd,SNDCTL_DSP_SETFMT,&dat) < 0) || x != dat)
	x = 0;
      else
	x = 1;
    }
    if (!x && (dat & AFMT_S16_BE)) {
      x = dat = AFMT_S16_BE;
      if ((ioctl(dev->fd,SNDCTL_DSP_SETFMT,&dat) < 0) || x != dat)
	x = 0;
      else
	x = 1;
    }
    if (!x) {
      fprintf(stderr, "failed to set desired audio format\n");
      return -1;
    }
  }
  x = dat = 2;
  if ((ioctl(dev->fd,SNDCTL_DSP_CHANNELS,&dat) < 0) || x != dat) {
    fprintf(stderr, "failed to set stereo output\n");
    return -1;
  }
  dat = VE_AUDIO_FREQ;
  if (ioctl(dev->fd,SNDCTL_DSP_SPEED,&dat) < 0) {
    fprintf(stderr, "failed to set desired sample rate\n");
    return -1;
  }
  fprintf(stderr, "got dsp speed of %d\n", dat);
  if (!dev->convbuf) {
    dev->convbuf = malloc(sizeof(short)*VE_AUDIO_FRAME_LENGTH*2);
    assert(dev->convbuf != NULL);
  }

  return 0;
}

extern void *veAudio_player_thread(void *);

void veAudioImplInit() {
  /* create a single output */
  int i;
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

  dev = calloc(1,sizeof(devinfo_t));
  assert(dev != NULL);
  if (device_setup(dev)) {
    fprintf(stderr, "could not open/init device %s\n", dev->name ? dev->name :
	    "<null>");
    exit(1);
  }
  outputs->priv = dev;

  pthread_create(&pth,NULL,veAudio_player_thread,outputs);
}

void veAudioImplPlay(VeAudioOutput *out, VeAudioSample *data, int dlen) {
  /* dlen is size of data, but we pass number of frames to alWriteFrames */
  int i;
  devinfo_t *dev = (devinfo_t *)(out->priv);
  for(i = 0; i < dlen; i++) {
    dev->convbuf[i] = (short)(data[i]*32768.0);
    if (swab_out) {
      unsigned short *s = (unsigned short *)&(dev->convbuf[i]);
      *s = (((*s)&0xff)<<8)|(((*s)&0xff00)>>8);
    }
  }
  write(dev->fd,dev->convbuf,dlen*sizeof(short));
}

void veAudioImplNextFrame(VeAudioOutput *out) {
  /* wait until there is less than a frame in the buffer */
  /* if we're using OSS here, there are effectively two buffers in
     memory, so the actual write() call will do the blocking for us */
  /* In that case, this function call does nothing */
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
