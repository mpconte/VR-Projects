/* Audio devices using the PortAudio library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <portaudio.h>

#include <ve.h>

#define MODULE "driver:portaudio"
#define NUM_OUTPUT_STREAMS 1

/* The API needs work and will thus change */
#define BUFFER_DEPTH 3

struct ve_pa_priv {
  PortAudioStream *stream;
  float *bufs[BUFFER_DEPTH];
  int bufSize; /* in bytes */
  /* head - next frame to be sent out */
  /* tail - next frame to be filled in */
  int head, tail;
  int flush;
  VeThrMutex *mutex;
  VeThrCond *bufNotFull;
};

static int ve_pa_cback(void *inputBuffer, void *outputBuffer,
		       unsigned long framesPerBuffer,
		       PaTimestamp outTime, void *userData) {
  struct ve_pa_priv *p = (struct ve_pa_priv *)userData;
  if (p->flush) {
    p->head = p->tail = 0; /* reset buffers */
    p->flush = 0;
  }
  if (p->head == p->tail)
    /* buffer is empty - play silence */
    memset(outputBuffer,0,p->bufSize);
  else {
    memcpy(outputBuffer,p->bufs[p->head],p->bufSize);
    if (p->tail == -1)
      p->tail = p->head;
    p->head = ((p->head + 1) % BUFFER_DEPTH);
    veThrCondBcast(p->bufNotFull);
  }
  return 0;
}

static VeAudioDevice *ve_pa_inst(VeAudioDriver *drv, VeOption *options) {
  VeAudioDevice *d;
  PaError err;
  static int init = 0;
  int fsize = veAudioGetFrameSize(), k;
  struct ve_pa_priv *p = veAllocObj(struct ve_pa_priv);

  if (!init) {
    if ((err = Pa_Initialize()) != paNoError) {
      /* failed */
      veError(MODULE,"failed to initialize portaudio library: %s",
	      Pa_GetErrorText(err));
      veFree(p);
      return NULL;
    }
    init = 1;
  }

  err = Pa_OpenStream(&p->stream,paNoDevice,0,paFloat32,
		      NULL,Pa_GetDefaultOutputDeviceID(),
		      NUM_OUTPUT_STREAMS,paFloat32,NULL,veAudioGetSampFreq(),
		      veAudioGetFrameSize(),0,paClipOff,
		      ve_pa_cback,(void *)p);
  if (err != paNoError) {
    veError(MODULE,"failed to open portaudio stream: %s",
	    Pa_GetErrorText(err));
    veFree(p);
    return NULL;
  }
  
  /* create audio buffers */
  p->bufSize = fsize*sizeof(float);
  for (k = 0; k < BUFFER_DEPTH; k++)
    p->bufs[k] = veAlloc(p->bufSize,0);
  p->head = p->tail = p->flush = 0;
  p->mutex = veThrMutexCreate();
  p->bufNotFull = veThrCondCreate();

  d = veAllocObj(VeAudioDevice);
  d->driver = drv;
  d->options = options;
  d->devpriv = p;

  Pa_StartStream(p->stream);
  
  return d;
}

static void ve_pa_deinst(VeAudioDevice *d) {
  struct ve_pa_priv *p = (struct ve_pa_priv *)(d->devpriv);
  int k = 0;

  Pa_StopStream(p->stream);
  Pa_CloseStream(p->stream);
  for (k = 0; k < BUFFER_DEPTH; k++)
    veFree(p->bufs[k]);
  veThrCondDestroy(p->bufNotFull);
  veThrMutexDestroy(p->mutex);
  veFree(p);
  
  veOptionFreeList(d->options);
  veFree(d->name);
  veFree(d);
}

static int ve_pa_getsub(VeAudioDevice *d, char *name) {
  return -1;
}

static int ve_pa_buffer(VeAudioDevice *d, int sub, float *data, int dlen) {
  struct ve_pa_priv *p = (struct ve_pa_priv *)(d->devpriv);
  if (sub != -1) {
    veError(MODULE,"%s (portaudio): trying to buffer output on non-existent sub-channel %d",
	    d->name, sub);
    return -1;
  }
  veThrMutexLock(p->mutex);
  if (p->tail == -1) {
    veError(MODULE,"%s (portaudio): Did I say you could buffer data? Did I?");
    veFatalError(MODULE,"%s (portaudio): If I did, then I lied");
  }
  memset(p->bufs[p->tail],0,p->bufSize);
  memcpy(p->bufs[p->tail],data,dlen*sizeof(float));
  p->tail = (p->tail+1)%BUFFER_DEPTH;
  if (p->tail == p->head)
    p->tail = -1; /* just filled */
  veThrMutexUnlock(p->mutex);
  return 0;
}

static void ve_pa_flush(VeAudioDevice *d, int sub) {
  struct ve_pa_priv *p = (struct ve_pa_priv *)(d->devpriv);
  veThrMutexLock(p->mutex);
  p->flush = 1;
  veThrMutexUnlock(p->mutex);
}

static int ve_pa_wait(VeAudioDevice *d) {
  struct ve_pa_priv *p = (struct ve_pa_priv *)(d->devpriv);
  veThrMutexLock(p->mutex);
  while (p->tail == -1)
    veThrCondWait(p->bufNotFull,p->mutex);
  veThrMutexUnlock(p->mutex);
}

static VeAudioDriver VE_PA_Driver = {
  "portaudio",
  ve_pa_inst,
  ve_pa_deinst,
  ve_pa_getsub,
  ve_pa_buffer,
  ve_pa_flush,
  ve_pa_wait
};

void VE_DRIVER_INIT(portaudio)(void) {
  veAudioDriverAdd("portaudio",&VE_PA_Driver);
}

void VE_DRIVER_PROBE(portaudio)(void *phdl) {
  veDriverProvide(phdl,"audio","portaudio");
}

