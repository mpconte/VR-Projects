/*
 * fly.c        $Revision: 1.2 $
 */

#include <ve.h>
#include <ve_audio.h>
#include "stdio.h"
#include "math.h"
#include "skyfly.h"

#if defined(_WIN32)
#pragma warning (disable:4244)  /* disable bogus conversion warnings */
#pragma warning (disable:4305)  /* VC++ 5.0 version of above warning. */
#endif

#define cosf(a)  	cos((float)a)
#define sinf(a)  	sin((float)a)
#define sqrtf(a)  	sqrt((float)a)
#define expf(a)  	exp((float)a)

#ifndef EPS
#define EPS 1.0e-3
#endif /* EPS */
/* speed of transition of "x" and "y" controls */
#define XRATE 2.5
#define YRATE 6.8

typedef struct paper_plane_struct {
    float           Pturn_rate;
    float           PX, PY, PZ, PZv;
    float           Pazimuth;
    float           Proll;
    int             Pcount, Pdirection;
  float damage;
  enum {
    PLANE__OKAY,
    PLANE__HIT,
    PLANE__DYING,
    PLANE__DEAD
  } state;
} paper_plane;

typedef struct shot_struct {
  float PX, PY, PZ; /* where it is */
  float Pazimuth, Pelev; /* angles denoting direction */
  float Speed;      /* speed of the shot */
  float expl_radius;
  float expl_alpha;
  long expiry;
  enum {
    SHOT__DISABLED,
    SHOT__FIRING,
    SHOT__EXPLOSION
  } state;
} shot;

#define MAX_ELEV M_PI*30.0/180.0
#define SHOT_TIME 5000
#define SHOT_EXPL_TIME 2000
#define SHOT_SPEED 0.7

static paper_plane  flock[NUM_PLANES];
static shot shots[NUM_SHOTS];
static float  X, Y, Z, Azimuth, Elevation, Speed;
static int Keyboard_mode;
static float control_x = 0.0, control_y = 0.0, control_throttle = 0.0;
static float rcontrol_x = 0.0, rcontrol_y = 0.0;

extern float time_factor;
extern float *A;
extern int Wxorg, Wyorg;

static float terrain_height(float x, float y);

#ifdef USE_AUDIO
extern VeAudioSound *expl_sound, *shot_sound;
void audio_add_sound(VeAudioSound *, float *);
#endif /* USE_AUDIO */
int Xgetbutton(int b);
int Xgetvaluator(int v);

void set_controls(float x, float y, float throttle) {
  rcontrol_x = x*1.5;
  rcontrol_y = y*4.0;
  control_throttle = throttle;
}

void init_positions(void)
{
    int             i;

    X = GRID_RANGE / 2.;
    Y = GRID_RANGE / 2.;
    Z = 1.5;

    /*
     * randomly position the planes near center of world
     * take MAX of height above terrain and 0, so planes
     * don't fall into canyon.  Canyon has negative elevation
     */

    for (i = 0; i < NUM_PLANES; i++) {
        flock[i].PX = (float) IRND(20) + GRID_RANGE / 2 - 10;
        flock[i].PY = (float) IRND(20) + GRID_RANGE / 2 - 10;
        flock[i].PZ = MAX(terrain_height(flock[i].PX, flock[i].PY),0.) +
                2.*(float)i/NUM_PLANES+.3;
        flock[i].Pazimuth = ((float)IRND(256) / 128.) * M_PI;
	flock[i].state = PLANE__OKAY;
	flock[i].damage = 0.0;
    }
    for (i = 0; i < NUM_SHOTS; i++) {
      shots[i].PX = shots[i].PY = shots[i].PZ = 0.0;
      shots[i].Pazimuth = shots[i].Pelev = 0.0;
      shots[i].Speed = 0.0;
      shots[i].state = SHOT__DISABLED;
    }
	Speed = 0.1f;
	Azimuth = M_PI / 2.;
	Elevation = 0.0;

#if 0
//	if (Init_pos) {
//		X = Init_x;
//		Y = Init_y;
//		Z = Init_z;
//		Azimuth = Init_azimuth;
//		Keyboard_mode = 1;
//    }
#endif
}

int _frame = 0;

#define SOFTEN(out,in) (out = 0.5*(out) + 0.5*(in))

void create_shot(void) {
  int i;
  float d[3];
  for(i = 0; i < NUM_SHOTS; i++) {
    /* look for an available shot */
    if (shots[i].state == SHOT__DISABLED)
      break;
  }
  if (i >= NUM_SHOTS)
    return; /* no more shots */

  /* originate from user's position and orientation */
  shots[i].PX = X;
  shots[i].PY = Y;
  shots[i].PZ = Z;
  shots[i].Pazimuth = Azimuth;
  shots[i].Pelev = Elevation;
  shots[i].Speed = Speed + SHOT_SPEED;
  shots[i].expiry = veClock() + SHOT_TIME;
  shots[i].state = SHOT__FIRING;
  shots[i].expl_radius = SHOT_RADIUS;
  shots[i].expl_alpha = 0.8;
  d[0] = X; d[1] = Y; d[2] = Z;
#ifdef USE_AUDIO
  audio_add_sound(shot_sound,d);
#endif
}

void fly(perfobj_t *viewer_pos) {
  static int init = 0;
  float *d = (float *)viewer_pos->vdata;

  /* track the first plane - what a hack */
  d[0] = flock[0].PX;
  d[1] = flock[0].PY;
  d[2] = flock[0].PZ+1.0;
  if (!init) {
    d[3] = -flock[0].Pazimuth + M_PI/2.0;
    d[4] = flock[0].PZv * (-M_PI);
    d[5] = flock[0].Proll * RAD_TO_DEG;
  } else {
    SOFTEN(d[3],-flock[0].Pazimuth + M_PI/2.0);
    SOFTEN(d[4],flock[0].PZv * (-M_PI));
    SOFTEN(d[5],flock[0].Proll * RAD_TO_DEG);
  }
}

void fly_stick(perfobj_t *viewer_pos)
{
	float       terrain_z, xpos, ypos, xcntr, ycntr;
	float       delta_speed = .0003;
	static float min_speed = 0.0, max_speed = 0.2;
        /*
         * simple flight model
         */

	Speed = control_throttle * (max_speed - min_speed) + min_speed;

	if (fabs(control_x-rcontrol_x) < EPS)
	  control_x = rcontrol_x;
	else {
	  int sgn1,sgn2;
	  sgn1 = rcontrol_x > control_x ? 1 : -1;
	  control_x += sgn1*XRATE;
	  sgn2 = rcontrol_x > control_x ? 1 : -1;
	  if (sgn1 != sgn2)
	    control_x = rcontrol_x;
	}

	if (fabs(control_y-rcontrol_y) < EPS)
	  control_y = rcontrol_y;
	else {
	  int sgn1,sgn2;
	  sgn1 = rcontrol_y > control_y ? 1 : -1;
	  control_y += sgn1*YRATE;
	  sgn2 = rcontrol_y > control_y ? 1 : -1;
	  if (sgn1 != sgn2)
	    control_y = rcontrol_y;
	}

	/* following two scale factors determine how fast you turn/climb */
        xpos = control_x*0.01;
        ypos = -control_y*0.01;

        /*
         * move in direction of view
         */

        Azimuth += xpos;
	if (Azimuth > 2*M_PI)
	  Azimuth -= 2*M_PI;
	if (Azimuth < -2*M_PI)
	  Azimuth += 2*M_PI;
	Elevation += ypos;
	if (Elevation < -MAX_ELEV)
	  Elevation = -MAX_ELEV;
	if (Elevation > MAX_ELEV)
	  Elevation = MAX_ELEV;
        X += cosf(-Azimuth + M_PI / 2.) * cosf(Elevation) * Speed * time_factor;
        Y += sinf(-Azimuth + M_PI / 2.) * cosf(Elevation) * Speed * time_factor;
        Z -= sinf(Elevation) * Speed * time_factor;

    /*
     * keep from getting too close to terrain
     */

    terrain_z = terrain_height(X, Y);
    if (Z < terrain_z +.5)
        Z = terrain_z +.5;

    X = MAX(X, 1.);
    X = MIN(X, GRID_RANGE);
    Y = MAX(Y, 1.);
    Y = MIN(Y, GRID_RANGE);
    Z = MIN(Z, 20.);

    *((float *) viewer_pos->vdata + 0) = X;
    *((float *) viewer_pos->vdata + 1) = Y;
    *((float *) viewer_pos->vdata + 2) = Z;
    *((float *) viewer_pos->vdata + 3) = Azimuth;
    *((float *) viewer_pos->vdata + 4) = Elevation;
    *((float *) viewer_pos->vdata + 5) = control_x*12;
}

#define SQ(x) ((x)*(x))

void track_shots(perfobj_t *sobj) {
  int i,j,n = 0;
  float d[3];
  extern float CellSize;
  extern int NumCells;
  long now = veClock();

  for(i = 0; i < NUM_SHOTS; i++) {
    switch (shots[i].state) {
    case SHOT__DISABLED:
      continue; /* not active */
    case SHOT__EXPLOSION:
      if (now > shots[i].expiry)
	shots[i].state = SHOT__DISABLED;
      shots[i].expl_radius += 0.02;
      shots[i].expl_alpha *= 0.9;
      sobj->flags[0] = PD_DRAW_EXPLOSION;
      sobj->vdata[0] = 1.0;
      sobj->vdata[1] = shots[i].PX;
      sobj->vdata[2] = shots[i].PY;
      sobj->vdata[3] = shots[i].PZ;
      sobj->vdata[4] = shots[i].expl_radius;
      sobj->vdata[5] = shots[i].expl_alpha;
      sobj++;
      n++;
      break;
    case SHOT__FIRING:
      /* advance a frame... */
      shots[i].PX += cosf(-shots[i].Pazimuth + M_PI/2.0) * cosf(shots[i].Pelev) * shots[i].Speed * time_factor;
      shots[i].PY += sinf(-shots[i].Pazimuth + M_PI/2.0) * cosf(shots[i].Pelev) * shots[i].Speed * time_factor;
      shots[i].PZ -= sinf(shots[i].Pelev)*shots[i].Speed * time_factor;
      /* are we in collision */
      for(j = 1; j < NUM_PLANES; j++) {
	float dm;
	if (flock[j].state == PLANE__DYING || flock[j].state == PLANE__DEAD)
	  continue;
	dm = SQ(shots[i].PX-flock[j].PX)+SQ(shots[i].PY-flock[j].PY)+
	  SQ(shots[i].PZ-flock[j].PZ);
	if (dm < SQ(EXPL_RADIUS)) {
	  /* plane j is hit */
	  /* shot i is disabled */
	  dm = 1.0 - (dm / SQ(EXPL_RADIUS));
	  if (dm < 0.0)
	    dm = 0.0;
	  flock[j].damage += dm;
	  if (flock[j].damage > 1.0) {
	    flock[j].state = PLANE__DYING;
	    flock[j].Pdirection = (random()%2 ? -1 : 1);
	  } else
	    flock[j].state = PLANE__HIT;
	  shots[i].Speed = 0.0;
	  shots[i].state = SHOT__EXPLOSION;
	  d[0] = shots[i].PX; d[1] = shots[i].PY; d[2] = shots[i].PZ;
#ifdef USE_AUDIO
	  audio_add_sound(expl_sound,d);
#endif /* USE_AUDIO */
	  shots[i].expiry = now + SHOT_EXPL_TIME;
	}
      }
      if (terrain_height(shots[i].PX,shots[i].PY)+0.4 > shots[i].PZ) {
	/* shot i is disabled */
	shots[i].Speed = 0.0;
	shots[i].state = SHOT__EXPLOSION;
	d[0] = shots[i].PX; d[1] = shots[i].PY; d[2] = shots[i].PZ;
#ifdef USE_AUDIO
	audio_add_sound(expl_sound,d);
#endif /* USE_AUDIO */
	shots[i].expiry = now + SHOT_EXPL_TIME;
      }
      if (shots[i].PX < 0.0 || shots[i].PX > NumCells*CellSize ||
	  shots[i].PY < 0.0 || shots[i].PY > NumCells*CellSize) {
	if (shots[i].state == SHOT__FIRING) {
	  shots[i].Speed = 0.0;
	  shots[i].state = SHOT__DISABLED;
	}
      }
      if (now > shots[i].expiry) {
	if (shots[i].state == SHOT__FIRING) {
	  shots[i].Speed = 0.0;
	  shots[i].state = SHOT__DISABLED;
	}
      }

      if (shots[i].Speed != 0.0) {
	/* draw this shot */
	sobj->flags[0] = PD_DRAW_SHOT;
	*((float *)sobj->vdata + 0) = 1.0; /* signify that this entry is
					    meaningful */
	*((float *)sobj->vdata + 1) = shots[i].PX;
	*((float *)sobj->vdata + 2) = shots[i].PY;
	*((float *)sobj->vdata + 3) = shots[i].PZ;
	*((float *)sobj->vdata + 4) = shots[i].Pazimuth;
	*((float *)sobj->vdata + 5) = shots[i].Pelev;
	sobj++;
	n++;
      }
    }
  }

  while(n < NUM_SHOTS) {
    *((float *)sobj->vdata + 0) = 0.0; /* signify non-use */
    sobj++;
    n++;
  }
}

void fly_paper_planes(perfobj_t *paper_plane_pos)
{
	int                 i;
	float               speed = .08;
	float               terrain_z;

	/*
	 * slow planes down in cyclops mode since
     * frame rate is doubled 
     */

    for (i = 0; i < NUM_PLANES; i++) {
        /*
         * If plane is not turning, one chance in 50 of
         * starting a turn
         */
      if (flock[i].state == PLANE__DEAD) {
	flock[i].PX = flock[i].PY = 0.0;
	flock[i].PZ = -200.0; /* well below terrain */
	continue;
      }

      if (flock[i].state == PLANE__DYING) {
	/* spiral to the ground */
	float ht = terrain_height(flock[i].PX,flock[i].PY);
	if (ht - 0.4 > flock[i].PZ) {
	  flock[i].state = PLANE__DEAD;
	  continue;
	}
	flock[i].Pturn_rate = 0.1;
	flock[i].Pdirection = 1;
	flock[i].Proll += flock[i].Pdirection * flock[i].Pturn_rate;
	flock[i].Pazimuth -= flock[i].Proll *.05;
	flock[i].PZv -= 0.002;
      } else {
        if (flock[i].Pcount == 0 && IRND(50) == 1) {
            /* initiate a roll */
            /* roll for a random period */
            flock[i].Pcount = IRND(100);
            /* random turn rate */
            flock[i].Pturn_rate = IRND(100) / 10000.;
            flock[i].Pdirection = IRND(3) - 1;
        }
        if (flock[i].Pcount > 0) {
            /* continue rolling */
            flock[i].Proll += flock[i].Pdirection * flock[i].Pturn_rate;
            flock[i].Pcount--;
        } else
            /* damp amount of roll when turn complete */
            flock[i].Proll *=.95;

        /* turn as a function of roll */
        flock[i].Pazimuth -= flock[i].Proll *.05;

        /* follow terrain elevation */
        terrain_z=terrain_height(flock[i].PX,flock[i].PY);

        /* use a "spring-mass-damp" system of terrain follow */
        flock[i].PZv = flock[i].PZv - 
            .01 * (flock[i].PZ - (MAX(terrain_z,0.) +
                         2.*(float)i/NUM_PLANES+.3)) - flock[i].PZv *.04;

        /* U-turn if fly off world!! */
        if (flock[i].PX < 1 || flock[i].PX > GRID_RANGE - 2 || flock[i].PY < 1 || flock[i].PY > GRID_RANGE - 2)
            flock[i].Pazimuth += M_PI;
      }

        /* move planes */
        flock[i].PX += cosf(flock[i].Pazimuth) * speed * time_factor;
        flock[i].PY += sinf(flock[i].Pazimuth) * speed * time_factor;
        flock[i].PZ += flock[i].PZv;

    }

	for (i = 0; i < NUM_PLANES; i++) {
		*((float *) paper_plane_pos[i].vdata + 0) = flock[i].PX;
		*((float *) paper_plane_pos[i].vdata + 1) = flock[i].PY;
		*((float *) paper_plane_pos[i].vdata + 2) = flock[i].PZ;
		*((float *) paper_plane_pos[i].vdata + 3) = flock[i].Pazimuth * RAD_TO_DEG;
		*((float *) paper_plane_pos[i].vdata + 4) = flock[i].PZv * (-500.);
		*((float *) paper_plane_pos[i].vdata + 5) = flock[i].Proll *RAD_TO_DEG;
		paper_plane_pos[i].vdata[6] = flock[i].damage;
	}
}

/* compute height above terrain */
static float terrain_height(float x, float y)
{

    float           dx, dy;
    float           z00, z01, z10, z11;
    float           dzx1, dzx2, z1, z2;
    int             xi, yi;

    x /= XYScale;
    y /= XYScale;
    xi = MIN((int)x, GridDim-2);
    yi = MIN((int)y, GridDim-2);
    dx = x - xi;
    dy = y - yi;

    /*
                View looking straight down onto terrain

                        <--dx-->

                        z00----z1-------z10  (terrain elevations)
                         |     |         |   
                      ^  |     Z at(x,y) |   
                      |  |     |         |   
					 dy  |     |         |
                      |  |     |         |   
					  |  |     |         |
                      V z00----z2-------z10
                      (xi,yi)

                        Z= height returned
                        
    */

    z00 = A[xi * GridDim + yi];
    z10 = A[(xi + 1) * GridDim + yi];
    z01 = A[xi * GridDim + yi + 1];
    z11 = A[(xi + 1) * GridDim + yi + 1];

    dzx1 = z10 - z00;
    dzx2 = z11 - z01;

    z1 = z00 + dzx1 * dx;
    z2 = z01 + dzx2 * dx;

    return (ScaleZ*((1.0 - dy) * z1 + dy * z2));
}

