/*
 * Deal with the conditions that the subject will be given.
 * This basically establishes a global structure that contains the
 * trial information and provides utilities to play with them.
 */
# include <stdio.h>

# include "condition.h" 

# define BUFLEN 256

Trials *configureTrials(char *fname)
{
  FILE *trialfile;
  char key1[BUFLEN], key2[BUFLEN], str[BUFLEN];
  Trial *theTrials;
  Trials *expt;
  int numTrials, i, i1, i2;
  float f1, f2;

  if((expt = (Trials *)malloc(sizeof(Trials))) == (Trials *)NULL){
    fprintf(stderr,"Unable to allocate Trials in configureTrials\n");
    exit(1);
  }
  if((trialfile = fopen(fname, "r")) == NULL) {
    fprintf(stderr,"Unable to open %s\n", fname);
    exit(1);
  }

  /* skip coments */
  do  {
    if(fgets (str, 256, trialfile) == NULL) {
      fprintf(stderr, "Unexpected eof in %s\n", fname);
      exit(1);
    }
  } while (str [0] == '%');
  
  if(sscanf (str, "%d", &numTrials) != 1) {
    fprintf(stderr, "Unable to parse number of trials in input file\n");
    exit(1);
  }

  /* allocate space for number of trials */
  if((theTrials = (Trial *)malloc(numTrials * sizeof(Trial))) == (Trial *)NULL) {
    fprintf(stderr,"Out of memory creating trials tables\n");
    exit(1);
  }
  
  /* read in the trials */
  fprintf(stderr,"Input conditions dump %s\n",fname);
  for(i = 0;i<numTrials;) {
    if(fgets(str, 256, trialfile) == NULL) {
      fprintf(stderr, "Unexpected eof in %s\n", fname);
      exit(1);
    }
    fprintf(stderr, "%s\n", str);
    if(str[0] != '%') {

      if(sscanf(str,"%s %s %f %d %f %d",key1, key2, &f1, &i1, &f2, &i2) != 6) {
	fprintf(stderr,"Parse of input line |%s| failed\n", str);
	exit(1);
      }

      /* there are velocity conditions and acceleration conditions */
      if(!strcmp(key1,"velocity"))
	theTrials[i].profile = VELOCITY;
      else if(!strcmp(key1,"acceleration"))
	theTrials[i].profile = ACCELERATION;
      else {
	fprintf(stderr,"invalid profile (acceleration or velocity)\n");
	exit(1);
      }

      /* there are real motion and simulated motion conditions */
      if(!strcmp(key2, "real"))
	theTrials[i].kind = REAL;
      else if(!strcmp(key2, "simulated"))
	theTrials[i].kind = SIMULATED;
      else {
	fprintf(stderr, "invalid motion type (real or simulated)\n");
	exit(1);
      }

      theTrials[i].motion = f1;                   /* acceleration or vel    */
      theTrials[i].distance = f2;                 /* distance to target (m) */
      theTrials[i].flickeringWalls = i1;          /* walls flicker (0==no)  */
      theTrials[i].isVisible = i2;                /* target stays visible   */
      i++;
    }
  }
  fclose(trialfile);

  expt->n = numTrials;
  expt->trial = theTrials;
  strcpy(expt->name, fname);
  return expt;
}

/*
 * Do the training session. This is very easy here, we just read in from the
 * training session file.
 */
Trials *configureTrainingSession()
{
  return configureTrials("trainingset.input");
}

/*
 * Randomize a set of trials. This assumes that the random number generator
 * has been initialized appropriately. If not, well, you get what you get.
 */
void randomizeTrials(Trials *trials)
{
  int i, j, k, p;
  float q;

  printf("Randomizing trials 0x%x\n", trials);
  /* shufflen n times */
  for(i=0;i<trials->n;i++){
    printf("i %d n %d\n",i,trials->n);
    j = (int)(trials->n * drand48()) % trials->n;
    k = (int)(trials->n * drand48()) % trials->n;

    printf("swapping %d and %d\n",j,k);

    p = trials->trial[k].profile;
    trials->trial[k].profile = trials->trial[j].profile;
    trials->trial[j].profile = p;
    
    p = trials->trial[k].kind;
    trials->trial[k].kind = trials->trial[j].kind;
    trials->trial[j].kind = p;
    
    p = trials->trial[k].flickeringWalls;
    trials->trial[k].flickeringWalls = trials->trial[j].flickeringWalls;
    trials->trial[j].flickeringWalls = p;

    p = trials->trial[k].isVisible;
    trials->trial[k].isVisible = trials->trial[j].isVisible;
    trials->trial[j].isVisible = p;

    q = trials->trial[k].motion;
    trials->trial[k].motion = trials->trial[j].motion;
    trials->trial[j].motion = q;

    q = trials->trial[k].distance;
    trials->trial[k].distance = trials->trial[j].distance;
    trials->trial[j].distance = q;
  }
}

