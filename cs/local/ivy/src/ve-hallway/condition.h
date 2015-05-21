/*
 * Encapsulation of a set of Trials and an individual Trial
 *
 * These are used to represent both practice (training) trials and the
 * real experiment.
 */

/* possible values for profile */
# define VELOCITY     1
# define ACCELERATION 2

/* possible values for kind */
# define REAL        1
# define SIMULATED   2

typedef struct {
  int profile;
  int kind;
  int flickeringWalls;
  int isVisible;
  float motion;
  float distance;
} Trial;

typedef struct {
  int n;
  char name[256];
  Trial *trial;
} Trials;
  
Trials *configureTrials(char *);
Trials *configureTrainingSession();
