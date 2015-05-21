#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <ve.h>      /* core VE routines */
#include <vei_gl.h>  /* OpenGL implementation of VE */

# include "nhallway.h"
# include "condition.h"

/* one-dimensional constant-accel, constant-vel profiles */
# include "linear.c"

#include <moog.h>

/*
 * The experiment is in one of a limited number of states (defined by curState)
 * and is always in a particular trial (curTrialSet). and started at a specific
 * time startTime.
 *
 * State & Definitions:
 *
 * S_STARTUP - before the experiment begins. Screen tells you either to push
 *             left for expt, right for demos.
 *             you push a button. You then start the demos.
 * S_BETWEEN_TRIALS - screen is blank, and remains that way until you push
 *             a button. This permits the operator to reset the trike (or not).
 * S_SHOWING_TARGET - screen shows hallway and target. Motion of trike is
 *             ignored.
 * S_MOVING_TO_TARGET - you move (either simulated or real) towards the
 *             target.
 * S_BUTTON_PRESSED - subjected indicates that they are at the target.
 *
 */
# define S_STARTUP                0
# define S_BETWEEN_TRIALS         1
# define S_SHOWING_TARGET         2
# define S_MOVING_TO_TARGET       3
# define S_BUTTON_PRESSED         4
# define S_MOOG_INIT              5
# define S_TRIAL_RESET            6
# define S_SHUTDOWN               7

MoogPlatform plat;

/* current state of the experiment */
int curState = S_STARTUP;              
Trials *curTrialSet = (Trials *)NULL;
int curTrialNo = -1;
struct timeval startTime;
#define MLEN 256
char last_status[MLEN];
long last_time;

/*
 * compute elapsed time in seconds (as a float) since startTime
 */
static float elapsedTime()
{
  struct timeval now;

  if(gettimeofday(&now, NULL) < 0) {
    fprintf(stderr,"Unable to get time of day (elapsedTime)\n");
    exit(1);
  }

  return ((now.tv_sec - startTime.tv_sec) + 1e-6 *
    (now.tv_usec - startTime.tv_usec));
}

/* how we translate 1D motion onto the moog */
float moog_origin[3] = { 0.25, 0.0, -0.22 };
float moog_vec[3] = { -1.0, 0.0, 0.0 };
float moog_down[3] = { 0.0, 0.0, -0.04 }; /* shutdown location */
float setup_acc = 0.02; /* acceleration for setting up, shutting down */

float trivial_p1[3], trivial_p2[3], trivial_a, trivial_mid, trivial_d,
  trivial_t0;
long trivial_t1, trivial_t2;
int trivial_active = 0;

#define SQ(x) ((x)*(x))
void set_trivial_path(float p1[3], float p2[3], float a) {
  trivial_active = 0;
  trivial_d = SQ(p2[0] - p1[0]) + SQ(p2[1] - p1[1]) + SQ(p2[2] - p1[2]);
  if (trivial_d != 0.0)
    trivial_d = sqrt(trivial_d);
  trivial_t1 = moogTimeMsec();
  trivial_t2 = trivial_t1 + (long)(2*sqrt(trivial_d/a)*1000);
  if (trivial_t1 == trivial_t2)
    return;
  memcpy(trivial_p1,p1,sizeof(float)*3);
  memcpy(trivial_p2,p2,sizeof(float)*3);
  trivial_t0 = trivial_t1*1.0e-3;
  trivial_mid = (trivial_t1 + trivial_t2)/2 * 1.0e-3 - trivial_t0;
  trivial_a = a;
  trivial_active = 1;
}

void goto_trivial(float dest[3]) {
  MoogReply r;
  float p1[3];
  moogGetLastReply(plat,&r);
  p1[0] = r.length[MOOG_AXIS_X];
  p1[1] = r.length[MOOG_AXIS_Y];
  p1[2] = r.length[MOOG_AXIS_Z];
  set_trivial_path(p1,dest,setup_acc);
}

int next_trivial(long mt,float tg[3]) {
  float t;
  float x;
  if (!trivial_active)
    return 0;
  if (mt > trivial_t2) {
    trivial_active = 0;
    return 0;
  }
  t = mt*1.0e-3 - trivial_t0;
  if (t < trivial_mid)
    x = 0.5*trivial_a*t*t;
  else
    x = 0.5*trivial_a*trivial_mid*trivial_mid +
      trivial_a*trivial_mid*(t - trivial_mid) -
      0.5*trivial_a*(t - trivial_mid)*(t - trivial_mid);
  x /= trivial_d;
  tg[0] = (trivial_p2[0]-trivial_p1[0])*x + trivial_p1[0];
  tg[1] = (trivial_p2[1]-trivial_p1[1])*x + trivial_p1[1];
  tg[2] = (trivial_p2[2]-trivial_p1[2])*x + trivial_p1[2];
}

#define BRAKE_ACC 1.0
float brake_p[3], brake_vd[3], brake_vm, brake_a;
float brake_lp[3];
long brake_t1;

/* start braking from trial */
void start_braking() {
  float t = elapsedTime(), x;
  int i;
  x = trial_pos(t);
  brake_vm = trial_vel(t);
  for(i = 0; i < 3; i++) {
    brake_vd[i] = moog_vec[i];
    brake_p[i] = moog_vec[i]*x + moog_origin[i];
  }
  brake_a = BRAKE_ACC;
  brake_t1 = moogTimeMsec();
}

int braking_pt(float *p) {
  long t2;
  float t,v;
  int i;
  t2 = moogTimeMsec();
  t = (t2 - brake_t1)*1.0e-3;
  v = brake_vm - brake_a*t;
  if (v <= 0.0) {
    for(i = 0; i < 3; i++)
      p[i] = brake_lp[i];
    return 0;
  } else {
    for(i = 0; i < 3; i++)
      brake_lp[i] = p[i] = brake_p[i] + (brake_vm*t - 0.5*brake_a*t*t)*brake_vd[i];
    return 1;
  }
}

FILE *out;
Trials *demo, *trials;
float trikePos = 0.0;


/*
 * Set the trike somewhere in the world
 */
void set_pos(float x, float y, float theta) {
  vePackVector(&veGetOrigin()->loc,x,y,0.75);
  vePackVector(&veGetOrigin()->dir,cos(M_PI*theta/180.0),
                                   sin(M_PI*theta/180.0),0.0);
  vePackVector(&veGetOrigin()->up,0.0,0.0,1.0);
  /* veMPLocationPush(); */
}

/*
 * Process a button press event. We differentiate between left and right
 * button presses.
 */
static void processButtonPress(int left, int right) 
{
  static struct timeval lasttime;
  static int first = 1;
  struct timeval thistime;
  float dt;

  /* debounce the key buttons, must by 0.50 sec since last push */
  if(!first) {
    gettimeofday(&thistime, NULL);
    dt = (thistime.tv_sec - lasttime.tv_sec) + 1e-6 *
      (thistime.tv_usec - lasttime.tv_usec);
    if(dt < 0.50)
      return;
  } else
    first = 0;
  gettimeofday(&lasttime, NULL);

  /* useful little hack while debugging */
  if(left && right)
    exit(0);

  /* process button press event based on state */
  switch(curState) {
  case S_STARTUP: /* press left for expt, right for demo */
    if(right) {
      curTrialNo = -1;
      curTrialSet = demo;
    } else {
      curTrialNo = -1;
      curTrialSet = trials;
    }
    curState = S_BETWEEN_TRIALS;
    break;
  case S_BETWEEN_TRIALS: /* press any button to start a trial */
    curTrialNo++;
    curState = S_SHOWING_TARGET;
    trikePos = 0.0;
    set_pos(trikePos, 0.0f, 0.0f);
    break;
  case S_SHOWING_TARGET: /* we are looking at the target, and push button */
    if(gettimeofday(&startTime, NULL) < 0) {
      fprintf(stderr,"Unable to get Time of Day?\n");
      exit(1);
    }
    curState = S_MOVING_TO_TARGET;
    new_trial(curTrialSet->trial[curTrialNo].profile == VELOCITY ? T_CVEL : T_CACC,
	      curTrialSet->trial[curTrialNo].motion);
    fprintf(stderr, "new trial: %f %f %f\n",lin_ti,lin_ts,lin_te);
    break;
  case S_MOVING_TO_TARGET: /* we are moving to the (now invisible) target */
    fprintf(out,"%8s %4d %4d %1d %1d %1d %1d %f %f %f %f\n",
	    curTrialSet->name, curTrialNo, curTrialSet->n,
	    curTrialSet->trial[curTrialNo].profile,
	    curTrialSet->trial[curTrialNo].kind,
	    curTrialSet->trial[curTrialNo].flickeringWalls,
	    curTrialSet->trial[curTrialNo].isVisible,
	    curTrialSet->trial[curTrialNo].motion,
	    curTrialSet->trial[curTrialNo].distance,
	    trikePos, elapsedTime());
    
    /* either go to startup or between trials */
    start_braking();
    curState = S_BUTTON_PRESSED;
    trikePos = 0.0;
    set_pos(trikePos, 0.0f, 0.0f);
    break;
  case S_MOOG_INIT:
  case S_TRIAL_RESET:
  case S_SHUTDOWN:
  case S_BUTTON_PRESSED:
    break; /* ignore */
  default:
    fprintf(stderr,"Can't happen state %d\n", curState);
    exit(1);
  }
  vePostRedisplay();
}

#define DATA_STATE 1
typedef struct {
  int curState;
  int curTrialNo;
  int isdemos;
} progstate;

void recv_data(int tag, void *data, int dlen) {
  switch (tag) {
  case DATA_STATE:
    assert(dlen == sizeof(progstate));
    curState = ((progstate *)data)->curState;
    curTrialNo = ((progstate *)data)->curTrialNo;
    if (((progstate *)data)->isdemos)
      curTrialSet = demo;
    else
      curTrialSet = trials;
    break;
  }
}

void predisplay(void) {
  progstate s;
  s.curState = curState;
  s.curTrialNo = curTrialNo;
  s.isdemos = (curTrialSet == demo) ? 1 : 0;
  veMPDataPush(DATA_STATE,&s,sizeof(s));
}

/*
 * Draw the user's view. View differs depending upon the state
 */
void display(VeWindow *w) {
  char buf[80];
  int i;

  glClearColor(0,0,0,0);
  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  /* draw the environment. This changes depending on the state of the expt. */
  switch(curState) {
  case S_SHOWING_TARGET:
  case S_MOVING_TO_TARGET:
    if((curTrialSet == NULL)||(curTrialNo > curTrialSet->n)||(curTrialNo < 0)){
      fprintf(stderr,"Impossible condition in display\n");
      exit(1);
    }
    if(curTrialSet->trial[curTrialNo].flickeringWalls)
      flickerWalls(1);
    drawHallCeiling();
    drawHallFloor();
    drawLeftWall();
    drawRightWall();
    if(curState == S_SHOWING_TARGET)
      drawTarget(curTrialSet->trial[curTrialNo].distance);
    drawLine(0.0f);
    break;
  case S_MOOG_INIT:
    veiGlPushTextMode(w);
    glRasterPos2i(200, 200);
    glColor3f(0.7,0.7,0.7);
    /*
    sprintf(buf,"Moving Motion Platform to Start Position", curTrialNo+1);
    veiGlDrawString(NULL, buf, VEI_GL_LEFT);
    */
    veiGlPopTextMode();
    break;
  case S_TRIAL_RESET:
    veiGlPushTextMode(w);
    glRasterPos2i(200, 200);
    glColor3f(0.7,0.7,0.7);
    /*
    sprintf(buf,"Resetting Motion Platform", curTrialNo+1);
    veiGlDrawString(NULL, buf, VEI_GL_LEFT);
    */
    veiGlPopTextMode();
    break;
  case S_SHUTDOWN:
    veiGlPushTextMode(w);
    glRasterPos2i(200, 200);
    glColor3f(0.7,0.7,0.7);
    /*
    sprintf(buf,"Parking Motion Platform", curTrialNo+1);
    veiGlDrawString(NULL, buf, VEI_GL_LEFT);
    */
    veiGlPopTextMode();
    break;
  case S_BETWEEN_TRIALS:
    veiGlPushTextMode(w);
    glRasterPos2i(200, 200);
    glColor3f(0.7,0.7,0.7);
    if(curTrialSet == demo)
      sprintf(buf,"Demo %d", curTrialNo+1);
    else
      sprintf(buf, "Condition %d", curTrialNo+1);
    veiGlDrawString(NULL, buf, VEI_GL_LEFT);
    veiGlPopTextMode();
    break;
  case S_STARTUP:
    veiGlPushTextMode(w);
    glRasterPos2i(200, 200);
    sprintf(buf, "%s", "Push left button for expt, right for trial");
    glColor3f(0.7,0.7,0.7);
    veiGlDrawString(NULL, buf, VEI_GL_LEFT);
    veiGlPopTextMode();
    break;
  case S_BUTTON_PRESSED:
    break;
  default:
    fprintf(stderr,"Bad state %d (can't happen)\n",curState);
    exit(1);
  }
}

/* for a SIMULATED trial (i.e. no moog motion), then the profile
   drives the display */
/* reply callback */
void moog_reply(MoogPlatform p, MoogReply *r, void *arg) {
  if ((curState == S_MOVING_TO_TARGET) && (curTrialSet != NULL) &&
      (curTrialSet->trial[curTrialNo].kind == REAL)) {
    /* use moog position to update user's position in space */
    int k;
    float x;
    /* find largest element in moog vector */
    k = (fabs(moog_vec[1]) > fabs(moog_vec[0])) ? 1 : 0;
    k = (fabs(moog_vec[2]) > fabs(moog_vec[k])) ? 2 : k;
    switch(k) {
    case 0: x = r->length[MOOG_AXIS_X]; break;
    case 1: x = r->length[MOOG_AXIS_Y]; break;
    case 2: x = r->length[MOOG_AXIS_Z]; break;
    }
    x = (x - moog_origin[k])/moog_vec[k];
    fprintf(stderr,"moog_vec = { %f %f %f }, k = %d\n",
	    moog_vec[0],moog_vec[1],moog_vec[2]);
    fprintf(stderr,"position is %f\n",x);
    set_pos(x,0.0,0.0);
    vePostRedisplay();
  }
}

/* motion callback for the moog */
void motion(MoogPlatform p, MoogTarget *t, void *arg) {
  /* t is in moogTimeMsec() units */
  float f[3];
  assert(t->type == MOOG_MODE_DOF);
  
  /*
  fprintf(stderr, "oldt = %d %f %f %f\n",
	  curState, tg->position_x, tg->position_y, tg->position_z);
  */
  switch (curState) {
  case S_BUTTON_PRESSED:
    /* get next path on braking path */
    if (!braking_pt(f)) {
      goto_trivial(moog_origin);
      if(curTrialNo == (curTrialSet->n - 1)) {
	curState = S_MOOG_INIT;
      } else {
	curState = S_TRIAL_RESET;
      }
    }
    t->data.dof.position_x = f[0];
    t->data.dof.position_y = f[1];
    t->data.dof.position_z = f[2];
    break;

  case S_MOOG_INIT:
    if (!next_trivial(moogTimeMsec(),f)) {
      curState = S_STARTUP;
      vePostRedisplay();
    } else {
      t->data.dof.position_x = f[0];
      t->data.dof.position_y = f[1];
      t->data.dof.position_z = f[2];
    }
    break;
  case S_TRIAL_RESET:
    if (!next_trivial(moogTimeMsec(),f)) {
      curState = S_BETWEEN_TRIALS;
      vePostRedisplay();
    } else {
      t->data.dof.position_x = f[0];
      t->data.dof.position_y = f[1];
      t->data.dof.position_z = f[2];
    }
    break;
  case S_SHUTDOWN:
    if (!next_trivial(moogTimeMsec(),f))
      exit(0);
    else {
      t->data.dof.position_x = f[0];
      t->data.dof.position_y = f[1];
      t->data.dof.position_z = f[2];
    }
    break;

  case S_MOVING_TO_TARGET:
    {
      float p[3];
      float x, tm;
      tm = elapsedTime();
      if (tm >= lin_te) {
	/* fake push the button for the user... */
	/* we should probably mark this some other way... */
	processButtonPress(1,0);
      } else {
	if (curTrialSet != NULL && 
	    curTrialSet->trial[curTrialNo].kind == REAL) {
	  x = trial_pos(tm);
	  t->data.dof.position_x = moog_origin[0] + moog_vec[0]*x;
	  t->data.dof.position_y = moog_origin[1] + moog_vec[1]*x;
	  t->data.dof.position_z = moog_origin[2] + moog_vec[2]*x;
	}
      }
    }
    break;
  }
  /*
  fprintf(stderr, "newt = %d %f %f %f\n",
	  curState, tg->position_x, tg->position_y, tg->position_z);
  */
}

/*
 * Timer callback. Basically, if we are in trial, and undergoing
 * simulated motion, we simulate this here
 */
/*ARGSUSED*/
void advance(void *arg)
{
  float t;

  /* if the trike is 'simulated', update the position */
  if((curState == S_MOVING_TO_TARGET) && (curTrialSet != NULL)) {
    if (curTrialSet->trial[curTrialNo].kind == SIMULATED) {
      t = elapsedTime();
      trikePos = trial_pos(t);
      set_pos(trikePos, 0.0f, 0.0f);
    } else if (curTrialSet->trial[curTrialNo].kind == REAL) {
      MoogReply r;
      moogGetLastReply(&r,NULL);
    } else {
      fprintf(stderr, "invalid kind of trial: %d\n",
	      curTrialSet->trial[curTrialNo].kind);
      exit(1);
    }
    vePostRedisplay();
  }

  /* and we want to be called again... */
  veAddTimerProc(10, advance, NULL);
}

int button_callback(VeDeviceEvent *e, void *arg) {
  if (strcmp("left",e->elem) == 0) {
    processButtonPress(VE_EVENT_SWITCH(e)->state,0);
  } else if (strcmp("right",e->elem) == 0) {
    processButtonPress(0,VE_EVENT_SWITCH(e)->state);
  }
  vePostRedisplay();
}

/*
 * Deal with trike motion event callbacks. If we are in
 * mode S_MOVING_TO_TARGET *and* the current mode is a real motion
 * then we update the trike position, else we do nothing in terms
 * of the trike. However, we always check the buttons and
 * process the button press events.
 */
int trike_callback(VeDeviceEvent *e, void *arg) {
  VeDeviceE_Vector *evec;
  static float lastx = 0.0f;


  if(!strcmp("trike",e->device) && !strcmp("state", e->elem)) {
    evec = (VeDeviceE_Vector *)e->content;

    /* if the trike is 'real', update the position */
    if((curState == S_MOVING_TO_TARGET) && (curTrialSet != NULL) &&
       (curTrialSet->trial[curTrialNo].kind == REAL)){
      trikePos += (evec->value[0] - lastx);
      lastx = evec->value[0];
      set_pos(trikePos, 0.0f, 0.0f);
    }

    lastx = evec->value[0];

    if((evec->value[3] > 0)||(evec->value[4] > 0.0))
      processButtonPress(evec->value[3] > 0, evec->value[4] > 0);
    vePostRedisplay();
  }
  return 0;
}


/*
 * Change the view direction based on the tracker state.
 *
 * (i)   tracker works in inches
 * (ii)  tracker has -z forward, +y up, +x to the right.
 * (iii) orientations are azimuth, elevation, roll
 */
# define INCH2METER  (1/40.0)
# define TRACKER_HEIGHT (54.0)
static void set_view(float *state) {
  float eyex, eyey, eyez;
  float v1, v2, v3;
  float e1, e2, e3;
  float vp1, vp2, vp3;
  float ep1, ep2, ep3;
  float eq1, eq2, eq3;
  float up1, up2, up3;
  float sn, cs, len;
  int i;

  eyex = state[1] * INCH2METER;
  eyey = (TRACKER_HEIGHT -state[2]) * INCH2METER; 
  eyez = -state[0] * INCH2METER;

  /* point in azimuth direction (v lookat, vp up) */
  v1 = sin(state[3] * M_PI / 180.0);
  v2 = 0.0;
  v3 = -cos(state[3] * M_PI / 180.0);

  vp1 = 0.0;
  vp2 = 1.0;
  vp3 = 0.0;

  /* elevation added (direction vector) */
  e1 = v1 * cos(state[4] * M_PI / 180.0);
  e2 = sin(state[4] * M_PI / 180.0);
  e3 = v3 * cos(state[4] * M_PI / 180.0);

  /* normalize the bastard */
  len = sqrt(e1 * e1 + e2 * e2 + e3 * e3);
  e1 /= len; e2 /= len; e3 /= len;

  ep1 = v1 * cos((90.0 + state[4]) * M_PI / 180.0);
  ep2 = sin((90.0 + state[4]) * M_PI / 180.0);
  ep3 = v3 * cos((90.0 + state[4]) * M_PI / 180.0);

  /* normalize the bastard */
  len = sqrt(ep1 * ep1 + ep2 * ep2 + ep3 * ep3);
  ep1 /= len; ep2 /= len; ep3 /= len;

  /* normal vector to e and ep */
  eq1 = e2 * ep3 - ep2 * e3;
  eq2 = -(e1 * ep3 - e3 * ep1);
  eq3 = e1 * ep2 - ep1 * e2;

  /* normalize the bastard */
  len = sqrt(eq1 * eq1 + eq2 * eq2 + eq3 * eq3);
  eq1 /= len; eq2 /= len; eq3 /= len;

  cs = cos(state[5] * M_PI / 180.0);
  sn = sin(state[5] * M_PI / 180.0);
  up1 = cs * ep1 + sn * eq1;
  up2 = cs * ep2 + sn * eq2;
  up3 = cs * ep3 + sn * eq3;
  
  /* normalize the bastard */
  len = sqrt(up1 * up1 + up2 * up2 + up3 * up3);
  up1 /= len; up2 /= len; up3 /= len;

  vePackVector(&veGetDefaultEye()->loc, eyex, eyey, eyez);
  vePackVector(&veGetDefaultEye()->dir,e1, e2, e3);
  vePackVector(&veGetDefaultEye()->up, up1, up2, up3);
  veMPLocationPush();
}

/*
 * Deal with callbacks on the polhemus. This always updates
 * the operator's view based on where you are looking.
 */
int polhemus_callback(VeDeviceEvent *e, void *arg) {
  VeDeviceE_Vector *evec;

  if(!strcmp("polhemus",e->device) && !strcmp("state", e->elem)) {
    evec = (VeDeviceE_Vector *)e->content;
    set_view(evec->value);
    vePostRedisplay();
  }
  return 0;
}
void redisplay(VeWindow *w, long tm, VeWallView *wv) {
  display(w);
}


void setupwin(VeWindow *win) {
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);
}

int exitcback(VeDeviceEvent *e, void *arg) {
  /* don't exit directly - shutdown the moog first */
  curState = S_SHUTDOWN;
  goto_trivial(moog_down);
  return 0;
}

int main(int argc, char **argv) {
  veInit(&argc,argv);

  /* do basic experiment initialization */
  seedRandomGenerator();
  demo = configureTrainingSession();
  /*
  if(argc == 2)
    trials = configureTrials(argv[1]);
  else
  */
  trials = configureTrials("input.config");

  if((out = fopen("junk.out", "w")) == NULL) {
    fprintf(stderr,"unable to create output file\n");
    exit(1);
  }

  /* randomizeTrials(trials); */

  configureHallway();

  /* set global rendering options */
  veSetOption("depth","1");        /* depth buffer is required */
  veSetOption("doublebuffer","1"); /* double buffering is required */

  /* setup GL callbacks */
  veiGlSetWindowCback(setupwin);
  veiGlSetDisplayCback(redisplay);

  /* The world is static, so we don't need a specific data callback - the
   * data can be generated on every slave. NB: This has never been
   * tested on a multi-CPU implementation
   */

  veMPDataAddHandler(VE_DTAG_ANY,recv_data);

  if (veMPIsMaster()) { /* only do control on the master */

      /* setup device callbacks */
      veDeviceAddCallback(trike_callback,NULL,"trike");
      veDeviceAddCallback(polhemus_callback,NULL,"polhemus");
      veDeviceAddCallback(exitcback,NULL,"exit");
      veDeviceAddCallback(button_callback,NULL,"button");

      /* setup the application */
      set_pos(0.0,0.0,0.0);

      veiGlSetPreDisplayCback(predisplay);

      veAddTimerProc(10,advance,NULL);

      printf("Connecting moog platform\n");
      if (!(plat = moogConnect(NULL,MOOG_MODE_DOF))) {
	printf("Failed to connect to moog: %s\n",moogError(NULL));
	exit(1);
      }
      printf("Resetting moog platform\n");
      if (moogReset(plat)) {
	printf("Failed to reset moog: %s\n",moogError(plat));
	exit(1);
      }
      printf("Engaging moog platform\n");
      if (moogEngage(plat)) {
	printf("Failed to engage moog: %s\n",moogError(plat));
	exit(1);
      }
      printf("Moog platform ready\n");
      
      moogSetMotionCback(plat,motion,NULL);

      curState = S_MOOG_INIT;
      goto_trivial(moog_origin);
  }

  /* let VE take over */
  printf("Initialization complete\n");
  veRun();
}
