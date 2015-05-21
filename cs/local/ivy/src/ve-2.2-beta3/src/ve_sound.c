/* The sound table */
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve.h>

#define MODULE "ve_sound"

extern VeSound *veSoundLoadFile_AudioToolbox(char *);
extern VeSound *veSoundLoadFile_AudioFile(char *);
extern VeSound *veSoundLoadFile_WAV(char *);

VeSound *veSoundLoadFile(char *file) {
  VeSound *s;
#ifdef HAS_AUDIOTOOLBOX
  if ((s = veSoundLoadFile_AudioToolbox(file)))
    return s;
#endif
#ifdef HAS_AUDIOFILE
  if ((s = veSoundLoadFile_AudioFile(file)))
    return s;
#endif
  /* built-in WAV loader */
  if ((s = veSoundLoadFile_WAV(file)))
    return s;

  veError(MODULE,"sound file '%s' cannot be loaded by any loader",
	  file);
  return NULL;
}

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

static void convert_format(float *dest, int chan, int fmt, int endian,
			   int len, void *buffer) {
  unsigned char *b = (unsigned char *)buffer;

  if (endian < 0)
    endian = get_endian();

  if (endian != VE_AUDIO_LITTLE && endian != VE_AUDIO_BIG)
    veFatalError(MODULE,"convert_format: invalid endian value (%d)",endian);

  switch (fmt) {
  case VE_AUDIO_8BIT:
    /* each element is from 0 to 255 (treat 128 as 0) */
    {
      int k;
      while (len > 0) {
	*dest = 0.0;
	for(k = 0; k < chan; k++) {
	  *dest += (*(b++)-128)/128.0;
	}
	if (chan > 0)
	  *dest /= chan;
	dest++;
	len--;
      }
    }
    break;
  case VE_AUDIO_16BIT:
    /* each element is from -32768 to 32767 */
    {
      union {
	unsigned short u;
	short s;
      } x;
      unsigned long c[2];
      
      int k;
      if (endian == VE_AUDIO_LITTLE) {
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    x.u = c[0] | (c[1]<<8);
	    *dest += x.s/32768.0;
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      } else { /* endian == VE_AUDIO_BIG */
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    x.u = c[1] | (c[0]<<8);
	    *dest += x.s/32768.0;
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      }
    }
    break;
  case VE_AUDIO_24BIT:
    {
      union {
	unsigned long u;
	long s;
      } x;
      unsigned long c[3];

      int k;
      if (endian == VE_AUDIO_LITTLE) {
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    c[2] = *(b++);
	    x.u = c[0] | (c[1]<<8) | (c[2]<<16);
	    *dest += x.s/8388608.0; /* 2^23 = 2^24/2 */
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      } else { /* endian == VE_AUDIO_BIG */
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    c[2] = *(b++);
	    x.u = c[2] | (c[1]<<8) | (c[0]<<16);
	    *dest += x.s/8388608.0; /* 2^23 = 2^24/2 */
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      }
    }
    break;
  case VE_AUDIO_32BIT:
    {
      union {
	unsigned long u;
	long s;
      } x;
      unsigned long c[4];

      int k;
      if (endian == VE_AUDIO_LITTLE) {
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    c[2] = *(b++);
	    c[3] = *(b++);
	    x.u = c[0] | (c[1]<<8) | (c[2]<<16) | (c[3]<<24);
	    *dest += x.s/2147483648.0; /* 2^31 = 2^31/2 */
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      } else { /* endian == VE_AUDIO_BIG */
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    c[2] = *(b++);
	    c[3] = *(b++);
	    x.u = c[3] | (c[2]<<8) | (c[1]<<16) | (c[0]<<24);
	    *dest += x.s/2147483648.0; /* 2^23 = 2^24/2 */
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      }
    }
    break;
  case VE_AUDIO_FLOAT:
    {
      union {
	unsigned long u;
	float f;
      } x;
      unsigned long c[4];

      int k;
      if (endian == VE_AUDIO_LITTLE) {
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    c[2] = *(b++);
	    c[3] = *(b++);
	    x.u = c[0] | (c[1]<<8) | (c[2]<<16) | (c[3]<<24);
	    *dest += x.f;
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      } else { /* endian == VE_AUDIO_BIG */
	while (len > 0) {
	  *dest = 0.0;
	  for(k = 0; k < chan; k++) {
	    c[0] = *(b++);
	    c[1] = *(b++);
	    c[2] = *(b++);
	    c[3] = *(b++);
	    x.u = c[3] | (c[2]<<8) | (c[1]<<16) | (c[0]<<24);
	    *dest += x.f; /* 2^23 = 2^24/2 */
	  }
	  if (chan > 0)
	    *dest /= chan;
	  dest++;
	  len--;
	}
      }
    }
    break;
  default:
    veFatalError(MODULE,"convert_format: invalid format %d",fmt);
  }
}

/* assume out already has enough space */
static void resample(float *in, int len, int freq_in, 
		     float *out, int freq_out) {
  float ratio = (freq_out/(float)freq_in);
  /* if we're within 5%, don't bother resampling */
  if (ratio >= 0.95 && ratio <= 1.05) {
    memcpy(out,in,sizeof(float)*len);
    return;
  }
  /* TBD */
  veFatalError(MODULE,"no resampling support yet - use only frequency %d",
	       freq_out);
}

VeSound *veSoundLoadRaw(int freq, int chan, int nsmp, int sampfmt, int endian, 
			void *buf) {
    VeSound *snd;
    unsigned long framesz;
    float *b1;
    unsigned long newsmp;

    framesz = veAudioGetFrameSize();

    snd = veAllocObj(VeSound);
    assert(snd != NULL);

    /* convert using two buffers (one temporary) */
    /* format conversion - convert from stored format to float */
    b1 = veAlloc(nsmp*sizeof(float),1);
    convert_format(b1,chan,sampfmt,endian,nsmp,buf);
    
    /* resample */
    /* size of final version in samples */
    newsmp = (nsmp * veAudioGetSampFreq())/freq;
    snd->nframes = newsmp/framesz + (newsmp % framesz ? 1 : 0);
    /* allocate second buffer to be 10% larger than necessary
       - this is not arbitrary - the resmapling algorithm is allowed to
         simply copy if the input freq is within 10% of the resulting freq
       - it would be better to be more accurate about this
    */
    snd->data = veAlloc((snd->nframes*framesz+(snd->nframes*framesz/10))*sizeof(float),1);
    resample(b1,nsmp,freq,snd->data,veAudioGetSampFreq());

    veFree(b1);

    snd->id = -1;
    snd->name = NULL;
    snd->file = NULL;

    return snd;
}

/* The sound table */
typedef struct ve_sound_table {
  int nextfree;
  VeSound *sound;
} VeSoundTable;

static VeSoundTable *sound_table = NULL;
static int sound_table_use = 0;
static int sound_table_space = 0;
static int sound_firstfree = -1; /* nothing explicitly free - 
				    check use/space */
static VeStrMap sound_by_name = NULL;

int veSoundAdd(char *name, VeSound *sound) {
  /* returns id on success */
  
  if (!sound->name && name)
    sound->name = veDupString(name);
  
  if (sound_firstfree >= 0) {
    /* use existing slot */
    assert(sound_firstfree < sound_table_use &&
	   sound_firstfree < sound_table_space);
    assert(sound_table[sound_firstfree].sound == NULL);
    
    sound->id = sound_firstfree;
    sound_firstfree = sound_table[sound_firstfree].nextfree;
  } else {
    if (sound_table_use >= sound_table_space) {
      /* resize table */
      if (sound_table_space == 0)
	sound_table_space = 32; /* get us started... */
      while (sound_table_use >= sound_table_space)
	sound_table_space *= 2;
      sound_table = veRealloc(sound_table,sound_table_space*
			    sizeof(VeSoundTable));
      assert(sound_table != NULL);
    }
    sound->id = sound_table_use++;
  }
  sound_table[sound->id].nextfree = -1;
  sound_table[sound->id].sound = sound;
  if (name) {
    if (!sound_by_name)
      sound_by_name = veStrMapCreate();
    veStrMapInsert(sound_by_name,name,sound);
  }
  sound->refcnt++; /* increase reference count */
  return sound->id;
}

/* the unref operation may need to be made thread-safe
   (as might a ref-incr operation...) */
void veSoundUnref(VeSound *s) {
  if (s) {
    s->refcnt--;
    if (s->refcnt <= 0)
      veSoundFree(s);
  }
}

VeSound *veSoundFindName(char *name) {
  if (!sound_by_name || !name)
    return NULL;
  return (VeSound *)veStrMapLookup(sound_by_name,name);
}

VeSound *veSoundFindId(int id) {
  if (id < 0 || id >= sound_table_use)
    return NULL;
  assert(sound_table != NULL);
  return sound_table[id].sound;
}

int veSoundRemove(int id) {
  VeSound *s;
  if (id < 0 || id >= sound_table_use)
    return -1;
  assert(sound_table != NULL);
  if (!sound_table[id].sound)
    return -1;
  s = sound_table[id].sound;
  sound_table[id].sound = NULL;
  sound_table[id].nextfree = sound_firstfree;
  sound_firstfree = id;
  veSoundUnref(s);
  return 0;
}

void veSoundFree(VeSound *s) {
  if (s) {
    veFree(s->file);
    veFree(s->name);
    veFree(s->data);
    veFree(s);
  }
}

/* short-hand for loading from file and linking */
int veSoundCreate(char *name, char *file) {
  VeSound *s;
  
  if ((s = veSoundFindName(name)))
    return s->id;
  if (!(s = veSoundLoadFile(file)))
    return -1;
  if (veSoundAdd(name,s) < 0)
    return -1;
  return s->id;
}
