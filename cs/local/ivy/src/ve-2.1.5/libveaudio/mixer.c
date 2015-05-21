#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ve_audio.h>

/* The VE Audio mixers */
#define SBUF_SIZE (VE_AUDIO_FRAME_LENGTH*VE_AUDIO_NCHAN)
void veAudioStereoMixAndSend(VeAudioOutput *output) {
  /* generates an interleaved stereo output */
  VeAudioSample data[SBUF_SIZE];
  VeAudioFrame *frames[VE_AUDIO_MAX_INST];
  int is_stereo[VE_AUDIO_MAX_INST];
  float scale_l[VE_AUDIO_MAX_INST], scale_r[VE_AUDIO_MAX_INST];
  /* channel 0 = left, channel 1 = right */
  int i,j,nsrcs = 0;

  /* build a list of frames to mix... */
  for(i = 0; i < VE_AUDIO_MAX_INST; i++) {
    if (output->instances[i].active) {
      frames[nsrcs] = &(output->instances[i].sound->frames[output->instances[i].next_frame]);
      is_stereo[nsrcs] = (output->instances[i].sound->type == VE_AUDIO_STEREO);
      scale_l[nsrcs] = output->instances[i].params.volume_f;
      scale_r[nsrcs] = output->instances[i].params.volume_f;
      if (output->instances[i].params.pan_f > 0)
	scale_l[nsrcs] *= 1.0 - output->instances[i].params.pan_f;
      else if (output->instances[i].params.pan_f < 0)
	scale_r[nsrcs] *= 1.0 + output->instances[i].params.pan_f;
      nsrcs++;
    }
  }

  if (nsrcs > 0) {
    for(j = 0; j < VE_AUDIO_FRAME_LENGTH; j++) {
      data[2*j] = (VeAudioSample)0;
      data[2*j + 1] = (VeAudioSample)0;
      for(i = 0; i < nsrcs; i++) {
	if (is_stereo[i]) {
	  data[2*j] += frames[i]->data[2*j]*scale_l[i];
	  data[2*j+1] += frames[i]->data[2*j+1]*scale_r[i];
	} else {
	  data[2*j] += frames[i]->data[j]*scale_l[i];
	  data[2*j+1] += frames[i]->data[j]*scale_r[i];
	}
      }
    }
    veAudioImplPlay(output,data,SBUF_SIZE);
  }
}
