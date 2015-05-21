#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ve_audio.h>

void veAudioInitParams(VeAudioParams *p) {
  p->volume_f = 1.0;
  p->pan_f = 0.0;
  p->loop = 0; /* sounds do not loop by default */
}

/* common functions */
int veAudioGetLoop(VeAudioParams *p) {
  return p->loop;
}

void veAudioSetLoop(VeAudioParams *p, int l) {
  p->loop = l;
}

float     veAudioGetVolume(VeAudioParams *p) {
  return p->volume_f;
}

void      veAudioSetVolume(VeAudioParams *p, float v) {
  p->volume_f = v;
}

float     veAudioGetPan(VeAudioParams *p) {
  return p->pan_f;
}

void      veAudioSetPan(VeAudioParams *p, float pan) {
  p->pan_f = pan;
}

void veAudioPlayFull(VeAudioSound *snd) {
  VeAudioInst *i;

  i = veAudioInstance(NULL,snd,NULL);
  if (i)
    veAudioClean(i,0); /* clean when done */
}

VeAudioInst *veAudioInstance(VeAudioOutput *output, VeAudioSound *sound,
			     VeAudioParams *params) {
  VeAudioInst *inst;
  int i;

  if (!output)
    output = veAudioDefaultOutput();
  pthread_mutex_lock(&output->mgmt_mutex);
  inst = NULL;
  for(i = 0; i < VE_AUDIO_MAX_INST; i++) {
    if (output->instances[i].clean[0] && output->instances[i].clean[1]) {
      inst = &output->instances[i];
      break;
    }
  }
  if (!inst) {
    pthread_mutex_unlock(&output->mgmt_mutex);
    return NULL; /* no more instances available */
  }

  /* setup instance */
  inst->output = output;
  inst->sound = sound;
  if (params)
    inst->params = *params;
  else
    veAudioInitParams(&inst->params);
  inst->next_frame = 0;
  inst->clean[0] = inst->clean[1] = 0;
  inst->active = 1; /* set playing immediately */
  pthread_cond_init(&inst->finished_cond,NULL);
  pthread_cond_broadcast(&output->sounds_avail);
  pthread_mutex_unlock(&output->mgmt_mutex);
  return inst;
}

void veAudioClean(VeAudioInst *inst, int now) {
  pthread_mutex_lock(&inst->output->mgmt_mutex);
  inst->clean[0] = 1; /* mark our clean bit */
  if (now)
    inst->active = 0;
  pthread_mutex_unlock(&inst->output->mgmt_mutex);
}

/* implementation should use this to spawn threads for its outputs */
void *veAudio_player_thread(void *v) {
  VeAudioOutput *output = (VeAudioOutput *)v;
  int i, active;

  while(1) {
    veAudioStereoMixAndSend(output);
    /* advance frame counters */
    active = 0;
    pthread_mutex_lock(&output->mgmt_mutex);
    for(i = 0; i < VE_AUDIO_MAX_INST; i++) {
      if (output->instances[i].active) {
	output->instances[i].next_frame++;
	if (output->instances[i].next_frame >= 
	    output->instances[i].sound->nframes) {
	  if (output->instances[i].params.loop) {
	    output->instances[i].next_frame = 0;
	    active++;
	  } else {
	    output->instances[i].active = 0;
	    output->instances[i].clean[1] = 1; /* we're done with it */
	    output->instances[i].next_frame = -1;
	    /* wake up any waiting threads */
	    pthread_cond_broadcast(&output->instances[i].finished_cond);
	  }
	} else {
	  active++;
	}
      } else if (output->instances[i].clean[0] && !output->instances[i].clean[1]) {
	output->instances[i].clean[1] = 1; /* paused or stopped, and cleaned */
	/* and in case a thread is waiting... */
	pthread_cond_broadcast(&output->instances[i].finished_cond);
      }
    }
    if (!active) {
      pthread_cond_wait(&output->sounds_avail,&output->mgmt_mutex);
    }
    pthread_mutex_unlock(&output->mgmt_mutex);
    if (active) {
      veAudioImplNextFrame(output);
    }
  }
}

VeAudioParams *veAudioGetInstParams(VeAudioInst *i) {
  return i ? &i->params : NULL;
}

void veAudioRefreshInst(VeAudioInst *i) {
  /* a NOP for now */
}

static struct ve_audio_soundrec {
  char *name;
  VeAudioSound *sound;
  struct ve_audio_soundrec *next;
} *known_sounds = NULL;

VeAudioSound *veAudioFind(char *name) {
  struct ve_audio_soundrec *c;

  for(c = known_sounds; c; c = c->next)
    if (strcmp(name,c->name) == 0)
      break;
  
  if (!c) {
    VeAudioSound *snd = veAudioLoad(name);
    if (!snd)
      return NULL;
    c = malloc(sizeof(struct ve_audio_soundrec));
    assert(c != NULL);
    c->name = strdup(name);
    c->sound = snd;
    c->next = known_sounds;
    known_sounds = c;
  }

  return c->sound;
}

void veAudioDispose(VeAudioSound *snd) {
  struct ve_audio_soundrec *pre, *c;

  /* remove from list of known sounds */
  for(pre = NULL, c = known_sounds; c; pre = c, c = c->next)
    if (c->sound == snd)
      break;
  if (c) {
    if (pre)
      pre->next = c->next;
    else
      known_sounds = c->next;
    free(c->name);
    free(c);
  }
  /* destroy the sound itself */
  free(snd->name);
  free(snd->frames);
  free(snd);
}

void veAudioPause(VeAudioInst *inst) {
  pthread_mutex_lock(&inst->output->mgmt_mutex);
  if (!inst->clean[0] || !inst->clean[1])
    inst->active = 0;
  pthread_mutex_unlock(&inst->output->mgmt_mutex);
}

void veAudioCont(VeAudioInst *inst) {
  pthread_mutex_lock(&inst->output->mgmt_mutex);
  if (!inst->clean[0] || !inst->clean[1]) {
    pthread_cond_broadcast(&inst->output->sounds_avail);
    inst->active = 1;
  }
  pthread_mutex_unlock(&inst->output->mgmt_mutex);
}

void veAudioWait(VeAudioInst *inst) {
  pthread_mutex_lock(&inst->output->mgmt_mutex);
  if (inst->active)
    pthread_cond_wait(&inst->finished_cond,&inst->output->mgmt_mutex);
  pthread_mutex_unlock(&inst->output->mgmt_mutex);
}

int veAudioIsDone(VeAudioInst *inst) {
  return inst->clean[1];
}

void veAudioInit() {
  veAudioImplInit();
}
