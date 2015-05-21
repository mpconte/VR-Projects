/* A simple loader for uncompressed WAV files which should work
   even if we have no useful native OS sound file loader */
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ve.h>

#define MODULE "ve_sndwav"

/* still need to deal with frequency... */
static int get_endian(void) {
  union {
    unsigned char c[2];
    unsigned short s;
  } x;
  x.s = 0x1122;
  if (x.c[0] == 0x11)
    return VE_AUDIO_BIG;
  else if (x.c[0] == 0x22)
    return VE_AUDIO_LITTLE;
  veFatalError(MODULE,"endian test failed to give a meaningful result (sizeof(short) = %d)",sizeof(short));
}

static void f_swap2(unsigned short *x) {
  *x = ((*x)&0xff)<<8 | ((*x)&0xff00)>>8;
}

static void f_swap4(unsigned *x) {
  *x = ((*x)&0xff)<<24 |
    ((*x)&0xff00)<<8 |
    ((*x)&0xff0000)>>8 |
    ((*x)&0xff000000)>>24;
}

#define swap2(x) f_swap2((unsigned short *)(x))
#define swap4(x) f_swap4((unsigned *)(x))

#define headersz 8
VeSound *veSoundLoadFile_WAV(char *file) {
  FILE *f;
  int c, k;
  unsigned long riffsz;
  unsigned char *buffer, *p;
  unsigned char *data = NULL;
  int datasz = 0;
  struct {
    char ID[4];
    long chunkSize;
    short wFormatTag;
    unsigned short wChannels;
    unsigned long dwSamplesPerSec;
    unsigned long dwAvgBytesPerSec;
    unsigned short wBlockAlign;
    unsigned short wBitsPerSample;
  } format;
  int has_format = 0;
  static int endian;
  static int init = 0;
  
  if (!init) {
    endian = get_endian();
    init = 1;
  }

  if (!(f = fopen(file,"r"))) {
    veError(MODULE,"failed to open audio file %s: %s",file,
	    strerror(errno));
    return NULL;
  }
  /* check for RIFF header... */
  if ((getc(f) != 'R') || (getc(f) != 'I') || 
      (getc(f) != 'F') || (getc(f) != 'F')) {
    veError(MODULE,"%s: not a WAVE file (no RIFF header)", file);
    fclose(f);
    return NULL;
  }
  /* read riffsz */
  riffsz = 0UL;
  for(k = 0; k < 4; k++) {
    if ((c = getc(f)) == EOF) {
      veError(MODULE,"%s: EOF reading RIFF size",file);
      fclose(f);
      return NULL;
    }
    riffsz = (riffsz<<8) | (unsigned long)c;
  }
  swap4(&riffsz);
  /* check for WAVE header... */
  if ((getc(f) != 'W') || (getc(f) != 'A') || 
      (getc(f) != 'V') || (getc(f) != 'E')) {
    veError(MODULE,"%s: not a WAVE file (no WAVE header)", file);
    fclose(f);
    return NULL;
  }
  buffer = veAlloc(riffsz-4,0);
  errno = 0;
  if ((k = fread(buffer,1,riffsz-4,f)) != riffsz-4) {
    veError(MODULE,"%s: failed to read WAVE file contents: %s (k = %d, riffsz = %d)",file,
	    strerror(errno),k,riffsz);
    veFree(buffer);
    fclose(f);
    return NULL;
  }
 
  fclose(f); /* file is read */

  /* look for a format chunk and a data chunk */
  p = buffer;
  while (riffsz >= 8) {
    if (p[0] == 'f' && p[1] == 'm' && p[2] == 't' && p[3] == ' ') {
      /* parse format */
      union {
	unsigned char c[4];
	unsigned long l;
      } endiancheck;
      unsigned long sz;

      if (riffsz <= format.chunkSize) {
	veError(MODULE,"%s: bad WAVE file - format section is too short",file);
	veFree(buffer);
	return NULL;
      }

      memcpy(&format,buffer,sizeof(format));
      /* do we need to swap? */
      endiancheck.l = 0x11223344;
      if (endiancheck.c[0] != 0x44) {
	/* need to swap... */
	swap4(&(format.chunkSize));
	swap2(&(format.wFormatTag));
	swap2(&(format.wChannels));
	swap4(&(format.dwSamplesPerSec));
	swap4(&(format.dwAvgBytesPerSec));
	swap2(&(format.wBlockAlign));
	swap2(&(format.wBitsPerSample));
      }
      p += format.chunkSize + (format.chunkSize % 2) + headersz;
      has_format = 1;
    } else if (p[0] == 'd' && p[1] == 'a' && p[2] == 't' && p[3] == 'a') {
      /* data secion */
      datasz = ((unsigned long)p[4]) |
	((unsigned long)p[5]<<8) |
	((unsigned long)p[6]<<16) |
	((unsigned long)p[7]<<24);
      if (datasz > (riffsz-8)) {
	veError(MODULE,"%s: bad WAVE file - stated data section size is too large",file);
	veFree(buffer);
	return NULL;
      }
      data = p + headersz;
      p += datasz + (datasz%2) + headersz;
      riffsz -= datasz + (datasz%2) + headersz;
    } else {
      /* skip to next section */
      unsigned long skip;
      skip = ((unsigned long)p[4]) |
	((unsigned long)p[5]<<8) |
	((unsigned long)p[6]<<16) |
	((unsigned long)p[7]<<24);
      skip += headersz; /* skip header too... */
      skip += (skip%2); /* make sure skip is even... */
      if (skip >= riffsz)
	riffsz = 0;
      else {
	riffsz -= skip;
	p += skip;
      }
    }
  }
  if (!has_format) {
    veError(MODULE,"%s: bad WAVE file - missing format chunk",file);
    veFree(buffer);
    return NULL;
  }
  /* a 0-length data section is okay, but a missing one is not... */
  if (!data)
    veError(MODULE,"%s: bad WAVE file - missing data section",file);
  
  /* verify format */
  /* we only support uncompressed formats (PCM) */
  if (format.wFormatTag != 1) {
    veError(MODULE,"%s: unsupported WAVE file - compression not supported",
	    file);
    veFree(buffer);
    return NULL;
  }
  if (format.wChannels <= 0) {
    veError(MODULE,"%s: unsupported WAVE file - (0 channels?)",file);
    veFree(buffer);
    return NULL;
  }
  if (format.wBlockAlign <= 0) {
    veError(MODULE,"%s: unsupported WAVE file - bad block align",file);
    veFree(buffer);
    return NULL;
  }
  if (format.dwSamplesPerSec <= 0) {
    veError(MODULE,"%s: unsupported WAVE file - bad samples per sec",file);
    veFree(buffer);
    return NULL;
  }
  if (format.wBitsPerSample <= 0) {
    veError(MODULE,"%s: unsupported WAVE file - invalid bits per sample",file);
    veFree(buffer);
    return NULL;
  }
  if (datasz % format.wBlockAlign) {
    veError(MODULE,"%s: bad WAVE file - data size is not multiple of block align",
	    file);
    veFree(buffer);
    return NULL;
  }

  /* okay - we have some data - now build a sound out of it */
  {
    VeSound *snd;
    int framesz;
    int smpfmt;
    unsigned long nsmp = 0;
    
    framesz = veAudioGetFrameSize();
    nsmp = datasz / format.wBlockAlign;
    
    /* determine actual sample format */
    if (format.wBitsPerSample <= 8)
      smpfmt = VE_AUDIO_8BIT;
    else if (format.wBitsPerSample <= 16)
      smpfmt = VE_AUDIO_16BIT;
    else if (format.wBitsPerSample <= 24)
      smpfmt = VE_AUDIO_24BIT;
    else if (format.wBitsPerSample <= 32)
      smpfmt = VE_AUDIO_32BIT;
    else {
      veError(MODULE,"%s: unsupported WAVE file (sample size is too big: %d)",
	      file, format.wBitsPerSample);
      veFree(buffer);
      return NULL;
    }

    snd = veSoundLoadRaw(format.dwSamplesPerSec,format.wChannels,nsmp,smpfmt,
			 VE_AUDIO_LITTLE,data);
    veFree(buffer);
    if (snd)
      snd->file = veDupString(file);

    /* hooray! */
    return snd;
  }

}
