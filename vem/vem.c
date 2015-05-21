/*
 * vem is a very simple library to allow motion within ve (ve motion).
 *
 * Basically it defines a set of static motion operations so you can move about.
 * There are two callbacks that are of interest. collision, which is called to
 * allow the user to check for collisions, and notifier, which lets the user
 * know that the user has moved and a redisplay will happen). This is useful
 * if you are moving things around with the user.
 *
 * Michael Jenkin, June 2007.
 * Modified by Matthew Conte, Octoboer 2008
 */

# include <math.h>
# include <stdio.h>

# include <ve.h>

# include "vem.h"

static float loc0[4] = { 0.0, 0.0, 0.0, 1.0 };
static float loc[4] = { 0.0, 0.0, 0.0, 1.0 };
static float dir[3] = { 0.0, 0.0, -1.0 };
static float up[3] = { 0.0, 1.0, 0.0 };
static float right[3] = {1.0, 0.0, 0.0};

/* NEW */
static int pan_clicked = -1; // 1 if in pan mode, -1 otherwise
static float total_tilt; // total amount of tilting from original head posiion
static float total_pan; // total amount of panning from original head position

/* NEW */
// Enable/Disable panning mode
static int switch_pan()
{
	pan_clicked *= -1;
	return 0;
}


static int defaultCollision(float *pos, float *from)
{
/*   printf("Checking collision with [%f %f %f]\n",pos[0],pos[1],pos[2]); */
  return 0;
}

static int (*collision)() = defaultCollision;

static void defaultNotifier() {}
static void (*notifier)() = defaultNotifier;

/*
 * Rotate a vector by an angle (in degrees) about a given axis
 */
static void rotarb(float *axis, float ang, float *val) {
  float nval[3];
  float m[3][3];
  float d, sn, cs;
  int i;
  ang *= M_PI / 180.0;
  sn = sin(ang);
  cs = cos(ang);

  /* This matrix from Foley, van Dam - (5.79), with a correction */
  m[0][0] = axis[0]*axis[0] + cs*(1 - axis[0]*axis[0]);
  m[0][1] = axis[0]*axis[1]*(1 - cs) - axis[2]*sn;
  m[0][2] = axis[0]*axis[2]*(1 - cs) + axis[1]*sn;
  m[1][0] = axis[0]*axis[1]*(1 - cs) + axis[2]*sn;
  m[1][1] = axis[1]*axis[1] + cs*(1 - axis[1]*axis[1]);
  m[1][2] = axis[1]*axis[2]*(1 - cs) - axis[0]*sn;
  m[2][0] = axis[0]*axis[2]*(1 - cs) - axis[1]*sn;
  m[2][1] = axis[1]*axis[2]*(1 - cs) + axis[0]*sn;
  m[2][2] = axis[2]*axis[2] + cs*(1 - axis[2]*axis[2]);
  d = 0.0;
  for(i = 0; i < 3; i++) {
    nval[i] = m[i][0]*val[0]+m[i][1]*val[1]+m[i][2]*val[2];
    d += nval[i]*nval[i];
  }
  if (d != 0.0) {
    d = sqrt(d);
    nval[0] /= d; nval[1] /= d; nval[2] /= d;
  }
  for(i = 0; i < 3; i++)
    val[i] = nval[i];

}

static void move(float d) {
  int i;
  float tloc[3];
  for(i = 0; i < 3; i++)
    tloc[i] = loc[i] + dir[i]*d;
  if(!(*collision)(tloc,loc)) {
    for(i=0;i<3;i++)
      loc[i] = tloc[i];
  }

}

static void pan(float a) {
  /* rotate axes around up */
  rotarb(up,a,dir);
  rotarb(up,a,right);
}

void tilt(float a) {
  /* rotate axes around right */
  rotarb(right,a,up);
  rotarb(right,a,dir);
}

static void twist(float a) {
  /* rotate axes around dir */
  rotarb(dir,a,up);
  rotarb(dir,a,right);
}

/*
 * Reset the viewer to the ve origin
 */
static void pose_reset()
{
  int i;

  for(i=0;i<3;i++)
    loc[i] = loc0[i];
  loc[3] = 1.0;
  dir[0] = dir[1] = 0.0;
  dir[2] = -1.0;
  up[0] = up[2] = 0.0; up[1] = 1.0;
  right[0] = 1.0; right[1] = 0.0; right[2] = 0.0;
}

static void pose_update(void) {
  VeFrame *f;
  int i;
  f = veGetOrigin();
  for(i = 0; i < 3; i++) {
    f->loc.data[i] = loc[i];
    f->dir.data[i] = dir[i];
    f->up.data[i] = up[i];
  }
  /*  f->loc.data[1] += 1.145; */ // eye offset in IVY
  (*notifier)();
  vePostRedisplay();
}

/* New method to allow moving with axis movement */
static int moving(VeDeviceEvent *e, void *arg) {
  printf("MOVING\n");
  printf("Event %s %s\n",e->device,e->elem);
  if(VE_EVENT_TYPE(e) != VE_ELEM_VALUATOR) {
    fprintf(stderr,"driver: internal logic error...that should be a valuator\n");
    return(0);
  }
  VeDeviceE_Valuator *v = VE_EVENT_VALUATOR(e);
  float val = v->value;
  float min = v->min;
  float max = v->max;
  printf("%f <= %f <= %f\n",min,val,max);
 
  if (pan_clicked == -1)
  {
	  tilt(-total_tilt);
	  //pan(-total_pan);
	  total_pan = 0;
	  total_tilt = 0;

	  move(-val);
  }
  else if (pan_clicked == 1)
  {
	  total_tilt += val;
	  tilt(val);
  }

  pose_update();
}


static int paning(VeDeviceEvent *e, void *arg) {
  printf("PANING\n");
  printf("Event %s %s\n",e->device,e->elem);
  if(VE_EVENT_TYPE(e) != VE_ELEM_VALUATOR) {
    fprintf(stderr,"driver: internal logic error...that should be a valuator\n");
    return(0);
  }
  VeDeviceE_Valuator *v = VE_EVENT_VALUATOR(e);
  float val = v->value;
  float min = v->min;
  float max = v->max;
  printf("%f <= %f <= %f\n",min,val,max);
 
  if (pan_clicked == 1)
	total_pan += val;

  pan(-val);
  pose_update();
}

static int tilting(VeDeviceEvent *e, void *arg) {
  printf("TILTING\n");
  printf("Event %s %s\n",e->device,e->elem);
  if(VE_EVENT_TYPE(e) != VE_ELEM_VALUATOR) {
    fprintf(stderr,"driver: internal logic error...that should be a valuator\n");
    return(0);
  }
  VeDeviceE_Valuator *v = VE_EVENT_VALUATOR(e);
  float val = v->value;
  float min = v->min;
  float max = v->max;
  printf("%f <= %f <= %f\n",min,val,max);
 
  tilt(-val);
  pose_update();
}


int VEM_pan_inc(VeDeviceEvent *e, void *arg) { pan(10.0); pose_update(); return 0;}
int VEM_pan_dec(VeDeviceEvent *e, void *arg) { pan(-10.0); pose_update(); return 0;}
int VEM_tilt_inc(VeDeviceEvent *e, void *arg) { tilt(10.0); pose_update(); return 0;}
int VEM_tilt_dec(VeDeviceEvent *e, void *arg) { tilt(-10.0); pose_update(); return 0;}
int VEM_twist_inc(VeDeviceEvent *e, void *arg) { twist(10.0); pose_update(); return 0;}
int VEM_twist_dec(VeDeviceEvent *e, void *arg) { twist(-10.0); pose_update();return 0;}
int VEM_forward(VeDeviceEvent *e, void *arg) { move(0.5); pose_update(); return 0;}
int VEM_backward(VeDeviceEvent *e, void *arg) { move(-0.5); pose_update(); return 0;}
int VEM_reset(VeDeviceEvent *e, void *arg) { pose_reset(); pose_update(); return 0;}

void VEM_default_bindings()
{
  veDeviceAddCallback(VEM_pan_inc, NULL, "pan_inc");
  veDeviceAddCallback(VEM_pan_dec, NULL, "pan_dec");
  veDeviceAddCallback(VEM_tilt_inc, NULL, "tilt_inc");
  veDeviceAddCallback(VEM_tilt_dec, NULL, "tilt_dec");
  veDeviceAddCallback(VEM_twist_inc, NULL, "twist_inc");
  veDeviceAddCallback(VEM_twist_dec, NULL, "twist_dec");
  veDeviceAddCallback(VEM_forward, NULL, "forward");
  veDeviceAddCallback(VEM_backward, NULL, "backward");
  veDeviceAddCallback(VEM_reset, NULL, "reset");
  veDeviceAddCallback(moving, NULL, "moving");
  veDeviceAddCallback(tilting, NULL, "tilting");
  veDeviceAddCallback(paning, NULL, "paning");
  veDeviceAddCallback(switch_pan, NULL, "switch_pan");
}

void VEM_initial_position(float x, float y, float z)
{
  loc0[0] = loc[0] = x;
  loc0[1] = loc[1] = y;
  loc0[2] = loc[2] = z;
  pose_update();
}
  

void VEM_no_collisions()
{
  collision = defaultCollision;
}

void VEM_check_collisions(int (*fn)())
{
  collision = fn;
}

void VEM_notify(void (*fn)())
{
  notifier = fn;
}


void VEM_get_pos(float *x, float *y, float *z) {
  *x = loc[0];
  *y = loc[1];
  *z = loc[2];
}
