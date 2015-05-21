/* probedrv - a simple utility for probing VE drivers to see what they
   provide */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ve.h>

void probe_driver(char *path) {
  VeDriverObj *ol, *o;

  printf("%s: ",path); 
  fflush(stdout);
  if (!veDrvImplIsDrv(path)) {
    printf(" not a driver\n");
    return;
  }

  if (!(ol = veDrvImplProbe(path))) {
    printf(" unknown driver\n");
    return;
  }
  printf(" VE 2.2 driver\n");
  for(o = ol; o; o = o->next) {
    printf("\t%s %s\n",o->type, o->name);
  }
  veDriverObjFree(ol);
}

#define ARB 1024

int main(int argc, char **argv) {
  argv++; /* skip argv[0] */
  veShowNotices(1);
  while (*argv) {
    if (veIsFile(*argv))
      probe_driver(*argv);
    else if (veIsDir(*argv)) {
      VeDir *dir;
      if (!(dir = veOpenDir(*argv))) {
	printf("%s:  cannot read\n", *argv);
      } else {
	char p[ARB], *s;
	while (s = veReadDir(dir,0)) {
	  snprintf(p,ARB,"%s/%s",*argv,s);
	  probe_driver(p);
	}
	veCloseDir(dir);
      }
    } else {
      printf("%s:  invalid file type\n", *argv);
    }
    argv++;
  }
  return 0;
}
