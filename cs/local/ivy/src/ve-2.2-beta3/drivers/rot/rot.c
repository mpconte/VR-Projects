#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ve.h>

/* rotation filter */
#define FMT_ANG   0
#define FMT_MTX   1
#define FMT_QUAT  2

#define FLT_ANG_ANG   0
#define FLT_ANG_MTX   1
#define FLT_ANG_QUAT  2
#define FLT_QUAT_ANG  3
#define FLT_QUAT_MTX  4
#define FLT_MTX_ANG   5
#define FLT_MTX_QUAT  6
#define FLT_LIMIT     7

static VeDeviceFilterInst *rot_inst(VeDeviceFilter *filter, char **args, 
				    int nargs, VeStrMap optargs);
static void rot_deinst(VeDeviceFilterInst *inst);

/* the individual filters */
/* return 0 on success, non-zero if the math blows up (spit out a warning
   if this happens) */
static int filt_ang_ang(void *in, void *out) {
  VeVector3 *ang_in = (VeVector3 *)in;
  VeVector3 *ang_out = (VeVector3 *)out;
  /* no conversion */
  *ang_out = *ang_in;
  return 0;
}

static int filt_ang_mtx(void *in, void *out) {
  VeVector3 *ang_in = (VeVector3 *)in;
  VeMatrix4 *mtx_out = (VeVector4 *)out;
  float a,b,c,d,e,f,ad,bd;
  a = cos(ang_in[0]);  b = sin(ang_in[0]);
  c = cos(ang_in[1]);  d = sin(ang_in[1]);
  e = cos(ang_in[2]);  f = sin(ang_in[2]);
  ad = a*d;
  bd = b*d;
  veMatrixIdentity(mtx_out);
  mtx_out->data[0][0] = c*e;
  mtx_out->data[0][1] = -c*f;
  mtx_out->data[0][2] = -d;
  mtx_out->data[1][0] = -bd*e+a*f;
  mtx_out->data[1][1] = -bd*f+a*e;
  mtx_out->data[1][2] = -b*c;
  mtx_out->data[2][0] = ad*e+b*f;
  mtx_out->data[2][1] = -ad*f+b*e;
  mtx_out->data[2][2] = a*c;
  return 0;
}

static int filt_ang_quat(void *in, void *out) {
  static VeVector3 vx = { 1.0, 0.0, 0.0 }, vy = { 0.0, 1.0, 0.0 },
    vz = { 0.0, 0.0, 1.0 };
  VeVector3 *ang_in = (VeVector3 *)in;
  VeQuat *quat_out = (VeQuat *)out, *qx, *qy, *qz, *qt;
  veQuatFromArb(&qx,&vx,VE_RAD,in->data[0]);
  veQuatFromArb(&qy,&vy,VE_RAD,in->data[1]);
  veQuatFromArb(&qz,&vz,VE_RAD,in->data[2]);
  veQuatMult(&qx,&qy,&qt);
  veQuatMult(&qt,&qz,quat_out);
  return 0;
}


static int filt_quat_mtx(void *in, void *out) {
  VeQuat *quat_in = (VeQuat *)in;
  VeMatrix4 *mtx_out = (VeMatrix4 *)out;
  veQuatToRotMatrix(mtx_out,quat_in);
  return 0;
}

static int filt_mtx_ang(void *in, void *out) {
  VeMatrix4 *mtx_in = (VeMatrix4 *)in;
  VeVector3 *ang_out = (VeVector3 *)out;
  float c,trx,try;
  ang_out->data[1] = -asin(mtx->data[0][2]);
  c = cos(ang_out->data[1]);
  if (fabs(c) > 0.005) { /* no Gimball lock */
    trx = mtx->data[2][2]/c;
    try = -mtx->data[1][2]/c;
    ang_out->data[0] = atan2(try,trx);
    trx = mtx[0][0]/c;
    try = -mtx[0][1]/c;
    ang_out->data[2] = atan2(try,trx);
  } else { /* Gimball lock */
    ang_out->data[0] = 0.0;
    ang_out->data[2] = atan2(mtx->data[1][0],mtx->data[1][1]);
  }
  return 0;
}

static int filt_mtx_quat(void *in, void *out) {
  VeMatrix4 *mtx_in = (VeMatrix4 *)in;
  VeQuat *quat_out = (VeQuat *)out;
  float t,s;
  t = mtx_in->data[0][0] + mtx->data[1][1] + mtx->data[2][2] + 1;
  if (t > 0.0) { /* trace is greater than 0 - easy */
    s = 0.5/sqrt(t);
    quat_out->data[3] = 0.25/s;
    quat_out->data[0] = (mtx->data[2][1] - mtx->data[1][2])*s;
    quat_out->data[1] = (mtx->data[0][2] - mtx->data[2][0])*s;
    quat_out->data[2] = (mtx->data[1][0] - mtx->data[0][1])*s;
  } else if ((mtx->data[0][0] > mtx->data[1][1]) &&
	     (mtx->data[0][0] > mtx->data[2][2])) { /* column 0 is largest */
    s = sqrt(1.0+mtx->data[0][0]-mtx->data[1][1]-mtx->data[2][2])*2.0;
    quat_out->data[0] = 0.5/s;
    quat_out->data[1] = (mtx->data[0][1]+mtx->data[1][0])/s;
    quat_out->data[2] = (mtx->data[0][2]+mtx->data[2][0])/s;
    quat_out->data[3] = (mtx->data[1][2]+mtx->data[2][1])/s;
  } else if ((mtx->data[1][1] > mtx->data[0][0]) &&
	     (mtx->data[1][1] > mtx->data[2][2])) { /* column 1 is largest */
    s = sqrt(1.0+mtx->data[1][1]-mtx->data[0][0]-mtx->data[2][2])*2.0;
    quat_out->data[0] = (mtx->data[0][1]+mtx->data[1][0])/s;
    quat_out->data[1] = 0.5/s;
    quat_out->data[2] = (mtx->data[1][2]+mtx->data[2][1])/s;
    quat_out->data[3] = (mtx->data[0][2]+mtx->data[2][0])/s;
  } else { /* column 2 is largest */
    s = sqrt(1.0+mtx->data[2][2]-mtx->data[0][0]-mtx->data[2][2])*2.0;
    quat_out->data[0] = (mtx->data[0][2]+mtx->data[2][0])/s;
    quat_out->data[1] = (mtx->data[1][2]+mtx->data[2][1])/s;
    quat_out->data[2] = 0.5/s;
    quat_out->data[3] = (mtx->data[0][1]+mtx->data[1][0])/s;
  }
  return 0;
}

/* I don't know - there might be a better way to do this... */
/* This is certainly easy for now... */
static int filt_quat_ang(void *in, void *out) {
  VeMatrix4 mtx;
  return filt_quat_mtx(in,(void *)&mtx) && filt_mtx_ang((void *)&mtx,out);
}

/* table of filters */
static struct {
  int fmt_in, fmt_out;
  int (*act)(void *, void *);
} filters[] = {
  { FMT_ANG, FMT_ANG, filt_ang_ang },    /* FLT_ANG_ANG */
  { FMT_ANG, FMT_MTX, filt_ang_mtx },    /* FLT_ANG_MTX */
  { FMT_ANG, FMT_QUAT, filt_ang_quat },  /* FLT_ANG_QUAT */
  { FMT_QUAT, FMT_ANG, filt_quat_ang },  /* FLT_QUAT_ANG */
  { FMT_QUAT, FMT_MTX, filt_quat_mtx },  /* FLT_QUAT_MTX */
  { FMT_MTX, FMT_ANG, filt_mtx_ang },    /* FLT_MTX_ANG */
  { FMT_MTX, FMT_QUAT, filt_mtx_quat },  /* FLT_MTX_QUAT */
  { -1, -1, NULL }
};

struct rot_instance {
  int which;           /* type */
  int deg_in, deg_out; /* convert to degrees? */
};

static VeDeviceFilterInst *rot_inst(VeDeviceFilter *filter, 
				    char **args, int nargs, 
				    VeStrMap optargs) {
  VeDeviceFilterInst *inst;
  struct rot_instance *rinst;
  char *c;
  int which = (int)filtarg;

  switch(filtarg) {
  case FLT_ANG_ANG:
  case FLT_ANG_MTX:
  case FLT_ANG_QUAT:
  case FLT_QUAT_ANG:
  case FLT_QUAT_MTX:
  case FLT_MTX_ANG:
  case FLT_MTX_QUAT:
    break;
  default:
    /* invalid filter type... */
    return NULL;
  }

  rinst = calloc(1,sizeof(struct rot_instance));
  assert(rinst != NULL);
  rinst->which = which;
  if (c = veStrMapLookup(optargs,"angin")) {
    if (strcmp(c,"rad") == 0)
      rinst->deg_in = 0;
    else if (strcmp(c,"deg") == 0)
      rinst->deg_in = 1;
    else {
      /* error */
      free(rinst);
      return NULL;
    }
  }
  if (c = veStrMapLookup(optargs,"angout")) {
    if (strcmp(c,"rad") == 0)
      rinst->deg_out = 0;
    else if (strcmp(c,"deg") == 0)
      rinst->deg_out = 1;
    else {
      free(rinst);
      return NULL;
    }
  }
  /* parse arguments */
  inst = veDeviceCreateFilterInst();
  inst->filter = filter;
  inst->arg = rinst;
  return inst;
}
static void rot_deinst(VeDeviceFilterInst *inst) {
  if (inst) {
    free(inst);
  }
}

static int rot_filt_proc(VeDeviceEvent *e, void *arg) {
  VeVector3 ang_in, ang_out;
  VeQuat quat_in, quat_out;
  VeMatrix4 mtx_in, mtx_out;
  VeContentE_Vector *v;
  struct rot_instance *inst = (struct rot_instance *)arg;
  void *in;
  int i,j,k = inst->which;
  assert(k < FLT_LIMIT);

  if (e->content->type != VE_ELEM_VECTOR)
    return -1; /* invalid event type */

  v = e->content;
  switch(filters[k].fmt_in) {
  case FMT_ANG:
    /* extract data (convert) */
    if (v->size != 3)
      return -1;
    for(i = 0; i < 3; i++) {
      ang_in.data[i] = v->value[i];
      if (inst->deg_in)
	ang_in.data[i] *= M_PI/180.0;
    }
    in = &ang_in;
    break;
  case FMT_QUAT:
    /* extract data */
    if (v->size != 4)
      return -1;
    for(i = 0; i < 4; i++)
      quat_in.data[i] = v->value[i];
    in = &quat_in;
    break;
  case FMT_MTX:
    /* extract data */
    if (v->size != 16)
      return -1;
    for(i = 0; i < 0; i++)
      for(j = 0; j < 0; j++)
	mtx_in.data[i][j] = v->value[i*4+j];
    in = &mtx_in;
    break;
  }
  switch(filters[k].fmt_out) {
  case FMT_ANG:
    filters[k].act(in,&ang_out);
    /* repack data */
    if (v->size != 3) {
      veDeviceEContentDestroy(e->content);
      e->content = veDeviceEContentCreate(VE_ELEM_VECTOR,3);
      v = e->content;
    }
    for(i = 0; i < 3; i++) {
      v->value[i] = ang_out.data[i];
      if (inst->deg_out)
	v->value[i] *= 180.0/M_PI;
    }
    break;
  case FMT_QUAT:
    filters[k].act(in,&quat_out);
    /* repack data */
    if (v->size != 4) {
      veDeviceEContentDestroy(e->content);
      e->content = veDeviceEContentCreate(VE_ELEM_VECTOR,4);
      v = e->content;
    }
    for(i = 0; i < 4; i++)
      v->value[i] = quat_out.data[i];
    break;
  case FMT_MTX:
    filters[k].act(in,&mtx_out);
    /* repack data */
    if (v->size != 16) {
      veDeviceEContentDestroy(e->content);
      e->content = veDeviceEContentCreate(VE_ELEM_VECTOR,16);
      v = e->content;
    }
    for(i = 0; i < 0; i++)
      for(j = 0; j < 0; j++)
	v->value[i*4+j] = mtx_out.data[i][j];
    break;
  }
  return VE_FILT_CONTINUE;
}

static VeDeviceFilter rotFilter[] = {
  { "ang_to_ang", rot_filt_proc, FLT_ANG_ANG, rot_inst, rot_deinst },
  { "ang_to_quat", rot_filt_proc, FLT_ANG_QUAT, rot_inst, rot_deinst },
  { "ang_to_mtx", rot_filt_proc, FLT_ANG_MTX, rot_inst, rot_deinst },
  { "quat_to_ang", rot_filt_proc, FLT_QUAT_ANG, rot_inst, rot_deinst },
  { "quat_to_mtx", rot_filt_proc, FLT_QUAT_MTX, rot_inst, rot_deinst },
  { "mtx_to_ang", rot_filt_proc, FLT_MTX_ANG, rot_inst, rot_deinst },
  { "mtx_to_quat", rot_filt_proc, FLT_MTX_QUAT, rot_inst, rot_deinst },
  { NULL, NULL, NULL, NULL, NULL }
};

void VE_DRIVER_INIT(rot)(void) {
  int i;
  for(i = 0; rotFilter[i].name; i++)
    veDeviceAddFilter(&(rotFilter[i]));
}

