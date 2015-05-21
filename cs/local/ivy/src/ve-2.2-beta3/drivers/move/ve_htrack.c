#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ve_core.h>
#include <ve_device.h>
#include <ve_debug.h>
#include <ve_tlist.h>
#include <ve_main.h>

#define MODULE "ve_htrack"

/* This is a built-in filter for the head-tracker which simply
   stuffs incoming data into the default eye frame.  It also 
   provides for simple linear correction of the data (separate
   correction frames for location and for direction/up).
*/

/* valid headtrack-data formats */
#define HT_POSQUAT 0 /* position/quaternion */
#define HT_MATRIX  1 /* 4x4 matrix */
#define HT_POS     2 /* position only (x,y,z)*/
#define HT_QUAT    3 /* quaternion only */

struct headtrack_priv {
  VeMatrix4 linear_loc_corr; /* linear correction for position */
  VeFrame vebase; /* what an identity rotation corresponds to */
  int format; /* expected format of data - all formats ultimately get
		 mapped to a frame */
  int keepevent;  /* default is 0 meaning to discard events - if
		     non-zero then CONTINUE is returned instead */
};

static struct headtrack_priv *loc_htrack_data = NULL, *dir_htrack_data = NULL;
static VeMatrix4 *calib_loc_corr = NULL, *calib_dir_corr = NULL;

static VeDeviceFilterInst *headtrack_inst(VeDeviceFilter *f, char **args,
					  int nargs, VeStrMap optargs) {
  VeDeviceFilterInst *inst;
  struct headtrack_priv *priv;
  char *s;

  priv = malloc(sizeof(struct headtrack_priv));
  assert(priv != NULL);
  veMatrixIdentity(&(priv->linear_loc_corr));
  priv->format = HT_POSQUAT; /* default */
  priv->keepevent = 0;

  veFrameIdentity(&(priv->vebase));

  /* read options */
  if ((s = veStrMapLookup(optargs,"format"))) {
    if (strcmp(s,"posquat") == 0)
      priv->format = HT_POSQUAT;
    else if (strcmp(s,"matrix") == 0)
      priv->format = HT_MATRIX;
    else if (strcmp(s,"pos") == 0)
      priv->format = HT_POS;
    else if (strcmp(s,"quat") == 0)
      priv->format = HT_QUAT;
    else {
      veError(MODULE, "headtrack filter: invalid argument to 'format' option: %s", s);
      free(priv);
      return NULL;
    }
  }
  
  if ((s = veStrMapLookup(optargs,"keepevent"))) {
    priv->keepevent = atoi(s);
  }

  if ((s = veStrMapLookup(optargs,"loccorr"))) {
    int k;
    char *save, *l, *e;
    save = l = veDupString(s);
    for(k = 0; k < 4; k++) {
      e = veNextLElem(&l);
      if (sscanf(e,"%f %f %f %f",
		 &(priv->linear_loc_corr.data[k][0]),
		 &(priv->linear_loc_corr.data[k][1]),
		 &(priv->linear_loc_corr.data[k][2]),
		 &(priv->linear_loc_corr.data[k][3])) != 4) {
	veError(MODULE, "headtrack filter: invalid argument to 'loccorr' option: %s", s);
	free(priv);
	free(l);
	return NULL;
      }
    }
    free(save);
  }

  if ((s = veStrMapLookup(optargs,"vebase.dir"))) {
    if (sscanf(s,"%f %f %f",
	       &(priv->vebase.dir.data[0]),
	       &(priv->vebase.dir.data[1]),
	       &(priv->vebase.dir.data[2])) != 3) {
      veError(MODULE, "headtrack filter: invalid argument to 'vebase.dir' option: %s", s);
      free(priv);
      return NULL;
    }
  }
  
  if ((s = veStrMapLookup(optargs,"vebase.up"))) {
    if (sscanf(s,"%f %f %f",
	       &(priv->vebase.up.data[0]),
	       &(priv->vebase.up.data[1]),
	       &(priv->vebase.up.data[2])) != 3) {
      veError(MODULE, "headtrack filter: invalid argument to 'vebase.up' option: %s", s);
      free(priv);
      return NULL;
    }
  }

  inst = malloc(sizeof(VeDeviceFilterInst));
  assert(inst != NULL);
  inst->filter = f;
  inst->arg = priv;

  if (!loc_htrack_data) {
    switch (priv->format) {
    case HT_POSQUAT:
    case HT_MATRIX:
    case HT_POS:
      loc_htrack_data = priv;
      calib_loc_corr = &priv->linear_loc_corr;
      break;
    }
  }

  if (!dir_htrack_data) {
    switch (priv->format) {
    case HT_POSQUAT:
    case HT_MATRIX:
    case HT_QUAT:
      dir_htrack_data = priv;
      calib_dir_corr = &priv->linear_loc_corr;
      break;
    }
  }

  VE_DEBUGM(1,("ve_htrack: linear loc corr:\n[ %1.4f %1.4f %1.4f %1.4f ]\n"
	      "[ %1.4f %1.4f %1.4f %1.4f ]\n[ %1.4f %1.4f %1.4f %1.4f ]\n"
	      "[ %1.4f %1.4f %1.4f %1.4f ]",
	      priv->linear_loc_corr.data[0][0],
	      priv->linear_loc_corr.data[0][1],
	      priv->linear_loc_corr.data[0][2],
	      priv->linear_loc_corr.data[0][3],
	      priv->linear_loc_corr.data[1][0],
	      priv->linear_loc_corr.data[1][1],
	      priv->linear_loc_corr.data[1][2],
	      priv->linear_loc_corr.data[1][3],
	      priv->linear_loc_corr.data[2][0],
	      priv->linear_loc_corr.data[2][1],
	      priv->linear_loc_corr.data[2][2],
	      priv->linear_loc_corr.data[2][3],
	      priv->linear_loc_corr.data[3][0],
	      priv->linear_loc_corr.data[3][1],
	      priv->linear_loc_corr.data[3][2],
	      priv->linear_loc_corr.data[3][3]));
  VE_DEBUGM(1,("ve_htrack: linear dir corr:\n[ %1.4f %1.4f %1.4f %1.4f ]\n"
	      "[ %1.4f %1.4f %1.4f %1.4f ]\n[ %1.4f %1.4f %1.4f %1.4f ]\n"
	      "[ %1.4f %1.4f %1.4f %1.4f ]",
	      priv->linear_loc_corr.data[0][0],
	      priv->linear_loc_corr.data[0][1],
	      priv->linear_loc_corr.data[0][2],
	      priv->linear_loc_corr.data[0][3],
	      priv->linear_loc_corr.data[1][0],
	      priv->linear_loc_corr.data[1][1],
	      priv->linear_loc_corr.data[1][2],
	      priv->linear_loc_corr.data[1][3],
	      priv->linear_loc_corr.data[2][0],
	      priv->linear_loc_corr.data[2][1],
	      priv->linear_loc_corr.data[2][2],
	      priv->linear_loc_corr.data[2][3],
	      priv->linear_loc_corr.data[3][0],
	      priv->linear_loc_corr.data[3][1],
	      priv->linear_loc_corr.data[3][2],
	      priv->linear_loc_corr.data[3][3]));
  return inst;
}

/* Following is the headtrack filter which automatically redirects
   incoming events to the default eye frame. */
static void headtrack_deinst(VeDeviceFilterInst *inst) {
  if (inst) {
    struct headtrack_priv *priv = (struct headtrack_priv *)(inst->arg);
    if (priv) {
      if (loc_htrack_data == priv) {
	loc_htrack_data = NULL;
	calib_loc_corr = NULL;
      }
      if (dir_htrack_data == priv) {
	dir_htrack_data = NULL;
	calib_dir_corr = NULL;
      }
      free(priv);
    }
    free(inst);
  }
}

static void orient_from_quat(VeFrame *eye, struct headtrack_priv *priv,
			     VeQuat *q) {
  /* extract axis and angle from quaternion */
  float sa, a;
  VeVector3 vnew,zero,znew,vec;
  VeMatrix4 m;
  VeFrame f;
  int i;

  veFrameIdentity(&f);
  a = acos(q->data[3])*2;
  sa = sqrt(1.0-q->data[3]*q->data[3]);
  if (fabs(sa) < 0.0005) sa = 1;
  for(i = 0; i < 3; i++)
    vec.data[i] = q->data[i]/sa;
  /* convert axis to VE co-ordinates using correction matrix */
  veMatrixMultPoint(&priv->linear_loc_corr,&vec,&vnew,NULL);
  vePackVector(&zero,0.0,0.0,0.0);
  veMatrixMultPoint(&priv->linear_loc_corr,&zero,&znew,NULL);
  for(i = 0; i < 3; i++)
    vec.data[i] = vnew.data[i]-znew.data[i];
  /* now rotate vebase to get new frame */
  /* note that we need to invert the rotation here...or do we? */
  veMatrixRotateArb(&m,&vec,VE_RAD,a);
  veMultFrame(&m,&priv->vebase,&f);
  eye->dir = f.dir;
  eye->up = f.up;
}

static int headtrack_proc(VeDeviceEvent *orig, void *arg) {
  struct headtrack_priv *priv = (struct headtrack_priv *)arg;
  VeMatrix4 mdat, m;
  VeVector3 vec, pos;
  VeFrame *eye, f;
  VeQuat q;
  VeDeviceE_Vector *v;
  int i,j;

  /* all headtracker data must be a vector */
  if (VE_EVENT_TYPE(orig) != VE_ELEM_VECTOR) {
    veWarning(MODULE,"headtracker received non-vector event");
    goto headtrack_ret;
  }
  v = VE_EVENT_VECTOR(orig);

  eye = veGetDefaultEye();
  veFrameIdentity(&f);

  switch(priv->format) {
  case HT_POSQUAT:
    if (v->size != 7) {
      veWarning(MODULE,"headtracker expected vector size %d for %s (got %d)",
		7,"posquat",v->size);
      goto headtrack_ret;
    }
    
    /* handle position */
    for(i = 0; i < 3; i++)
      pos.data[i] = v->value[i];
    veMatrixMultPoint(&priv->linear_loc_corr,&pos,&(f.loc),NULL);
    eye->loc = f.loc;
    
    /* handle orientation */
    for(i = 0; i < 4; i++)
      q.data[i] = v->value[3+i];
    orient_from_quat(eye,priv,&q);
    VE_DEBUGM(2,("eye.dir = [%1.4f %1.4f %1.4f] - eye.up = [%1.4f %1.4f %1.4f]",
		f.dir.data[0],f.dir.data[1],f.dir.data[2],
		f.up.data[0],f.up.data[1],f.up.data[2]));
    break;

  case HT_MATRIX:
    if (v->size != 16) {
      veWarning(MODULE,"headtracker expected vector size %d for %s (got %d)",
		16,"matrix",v->size);
      goto headtrack_ret;
    }
    for(i = 0; i < 4; i++)
      for(j = 0; j < 4; j++) {
	/* row ordering? */
	mdat.data[i][j] = v->value[i*4+j];
      }
    veRotMatrixToQuat(&q,&mdat);
    orient_from_quat(eye,priv,&q);

    veMatrixMult(&mdat,&priv->linear_loc_corr,&m);
    vePackVector(&pos,0.0,0.0,0.0);
    veMatrixMultPoint(&m,&pos,&vec,NULL);
    eye->loc = vec;
    break;

  case HT_POS:
    /* leave orientation untouched */
    if (v->size != 3) {
      veWarning(MODULE,"headtracker expected vector size %d for %s (got %d)",
		3,"pos",v->size);
      goto headtrack_ret;
    }
    /* handle position */
    for(i = 0; i < 3; i++)
      pos.data[i] = v->value[i];
    veMatrixMultPoint(&priv->linear_loc_corr,&pos,&(f.loc),NULL);
    eye->loc = f.loc;
    break;

  case HT_QUAT:
    /* leave position untouched */
    if (v->size != 4) {
      veWarning(MODULE,"headtracker expected vector size %d for %s (got %d)",
		4,"quat",v->size);
      goto headtrack_ret;
    }
    for(i = 0; i < 4; i++)
      q.data[i] = v->value[i];
    VE_DEBUGM(2,("quat is: (%g,%g,%g,%g)",q.data[0],q.data[1],q.data[2],q.data[3]));
    orient_from_quat(eye,priv,&q);
    VE_DEBUGM(2,("eye.dir = [%1.4f %1.4f %1.4f] - eye.up = [%1.4f %1.4f %1.4f]",
		f.dir.data[0],f.dir.data[1],f.dir.data[2],
		f.up.data[0],f.up.data[1],f.up.data[2]));
    break;
    
  default:
    veFatalError(MODULE,"headtracker has invalid format: %d",priv->format);
  }

  vePostRedisplay();

 headtrack_ret:
  return (priv->keepevent ? VE_FILT_CONTINUE : VE_FILT_DISCARD);
}

static VeDeviceFilter headtrack_filter = {
  "headtrack", headtrack_proc, NULL, headtrack_inst, headtrack_deinst
};

void veHeadTrackInit() {
  veDeviceAddFilter(&headtrack_filter);
}

/* following routines are for interactive calibration only and should
   be considered fragile */
VeMatrix4 *veHeadTrackGetLocCorr() {
  return calib_loc_corr;
}

VeMatrix4 *veHeadTrackGetDirCorr() {
  return calib_dir_corr;
}
