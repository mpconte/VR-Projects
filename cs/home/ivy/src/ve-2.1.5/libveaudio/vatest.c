#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ve_audio.h>

int main(int argc, char **argv) {
  VeAudioSound *snd;
  VeAudioInst *i;
  VeAudioParams par;
  int k;
  
  printf("initializing\n");
  veAudioInit();
  for(k = 1; k < argc; k++) {
    printf("finding audiofile\n");
    snd = veAudioFind(argv[k]);
    if (!snd) {
      fprintf(stderr, "failed to find audio: %s\n", argv[1]);
      exit(1);
    }
    printf("creating instance\n");
    veAudioInitParams(&par);
    veAudioSetLoop(&par,1);
    i = veAudioInstance(NULL,snd,&par);
    if (!i) {
      fprintf(stderr, "failed to create instance\n");
    } else {
      printf("instance created\n");
      veAudioClean(i,0);
    }
  }
  printf("sleeping\n");
  sleep(5);
  printf("playback complete\n");
}
