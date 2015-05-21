/* audiofile library support */
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ve.h>

#define MODULE "ve_sndaf"

#ifndef HAS_AUDIOFILE
VeSound *veSoundLoadFile_AudioFile(char *file) {
  static int init = 0;
  if (!init) {
    /* show warning message once */
    veError(MODULE,"internal stumble:  tried to use audiofile library but\n"
	    "configuration says no support - check veSoundLoadFile");
    veWarning(MODULE,"will return automatic failure for AudioFile library");
    init = 1;
  }
  return NULL;
}
#else /* HAS_AUDIOFILE */

#ifdef __sgi
#include <dmedia/audiofile.h>
#else
#include <audiofile.h>
#endif /* __sgi */

VeSound *veSoundLoadFile_AudioFile(char *file) {
  AFfilehandle fh;
  VeSound *snd;
  int nchan, nframe;
  int fsz, i, n;
  float *buf;
  
  if (!(fh = afOpenFile(file,"r",0))) {
    veError(MODULE,"audiofile library could not open sound file '%s'",
	    file);
    return NULL;
  }
  afSetVirtualSampleFormat(fh,AF_DEFAULT_TRACK,AF_SAMPFMT_FLOAT,4);
  // Hack. 
  // afSetVirtualRate(fh,AF_DEFAULT_TRACK,(double)veAudioGetSampFreq());
  nchan = afGetChannels(fh,AF_DEFAULT_TRACK);
  if (nchan < 1) {
    veError(MODULE,"sound file has no tracks?");
    afCloseFile(fh);
    return NULL;
  }
  if (nchan > 2) {
    veError(MODULE,"sound file has more than 2 tracks - rejecting it");
    afCloseFile(fh);
    return NULL;
  }
  nframe = afGetFrameCount(fh,AF_DEFAULT_TRACK);
  fsz = veAudioGetFrameSize();

  snd = veAllocObj(VeSound);
  snd->nframes = nframe/fsz;
  if (n % fsz)
    snd->nframes++;
  snd->data = veAlloc(snd->nframes*sizeof(float)*fsz,0);
  buf = veAlloc(2*fsz*sizeof(float),0);

  for (i = 0; i < snd->nframes; i++) {
    memset(buf,0,fsz*sizeof(float)); /* zero buffer */
    n = afReadFrames(fh,AF_DEFAULT_TRACK,buf,fsz);
    if (n == -1) {
      veError(MODULE,"sound file read failed for '%s': %s",
	      file, strerror(errno));
      afCloseFile(fh);
      veFree(buf);
      veFree(snd->data);
      veFree(snd);
      return NULL;
    }
    if (nchan == 1)
      /* remember: snd->data is a (float *) */
      memcpy(snd->data+fsz*i,buf,fsz*sizeof(float));
    else {
      /* downmix stereo to mono (trivial) */
      int k;
      for (k = 0; k < fsz; k++)
	snd->data[fsz*i+k] = (buf[2*k] + buf[2*k+1])/2.0;
    }
  }

  afCloseFile(fh);
  veFree(buf);

  snd->file = veDupString(file);
  /* initialize other fields */
  snd->id = -1;
  snd->name = NULL;

  return snd;
}

#endif /* HAS_AUDIOFILE */
