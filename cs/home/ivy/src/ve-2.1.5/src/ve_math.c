#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <ve_error.h>
#include <ve_util.h>
#include <ve_math.h>

#define MODULE "ve_math"

void veMatrixIdentity(VeMatrix4 *m) {
  int i,j;
  for(i = 0; i < 4; i++)
    for(j = 0; j < 4; j++)
      if (i == j)
	m->data[i][j] = 1.0;
      else
	m->data[i][j] = 0.0;
}

void veMatrixRotate(VeMatrix4 *m, int axis, int fmt, float val) {
  float sn, cs;

  if (fmt == VE_DEG)
    val *= M_PI/180.0;
  veMatrixIdentity(m);
  sn = sin(val);
  cs = cos(val);
  switch(axis) {
  case VE_X:
    m->data[1][1] = m->data[2][2] = cs;
    m->data[2][1] = sn;
    m->data[1][2] = -sn;
    break;
  case VE_Y:
    m->data[0][0] = m->data[2][2] = cs;
    m->data[2][0] = -sn;
    m->data[0][2] = sn;
    break;
  case VE_Z:
    m->data[0][0] = m->data[1][1] = cs;
    m->data[0][1] = -sn;
    m->data[1][0] = sn;
    break;
  }
}

void veMatrixRotateArb(VeMatrix4 *m, VeVector3 *u, int fmt, float val) {
  float sn, cs;

  if (fmt == VE_DEG)
    val *= M_PI/180.0;
  veMatrixIdentity(m);
  sn = sin(val);
  cs = cos(val);

  /* This matrix from Foley, van Dam - (5.79), with a correction */
  m->data[0][0] = u->data[0]*u->data[0] + cs*(1 - u->data[0]*u->data[0]);
  m->data[0][1] = u->data[0]*u->data[1]*(1 - cs) - u->data[2]*sn;
  m->data[0][2] = u->data[0]*u->data[2]*(1 - cs) + u->data[1]*sn;
  m->data[1][0] = u->data[0]*u->data[1]*(1 - cs) + u->data[2]*sn;
  m->data[1][1] = u->data[1]*u->data[1] + cs*(1 - u->data[1]*u->data[1]);
  m->data[1][2] = u->data[1]*u->data[2]*(1 - cs) - u->data[0]*sn;
  m->data[2][0] = u->data[0]*u->data[2]*(1 - cs) - u->data[1]*sn;
  m->data[2][1] = u->data[1]*u->data[2]*(1 - cs) + u->data[0]*sn;
  m->data[2][2] = u->data[2]*u->data[2] + cs*(1 - u->data[2]*u->data[2]);
}

void veMatrixTranslate(VeMatrix4 *m, VeVector3 *v, float scale) {
  int i;
  veMatrixIdentity(m);
  for(i = 0; i < 3; i++)
    m->data[i][3] = v->data[i]*scale;
}

void veMatrixMult(VeMatrix4 *a, VeMatrix4 *b, VeMatrix4 *c) {
  int i,j,k;
  for(i = 0; i < 4; i++)
    for(j = 0; j < 4; j++) {
      c->data[i][j] = 0.0;
      for(k = 0; k < 4; k++)
	c->data[i][j] += a->data[i][k] * b->data[k][j];
    }
}

void veMatrixMultPoint(VeMatrix4 *a, VeVector3 *b, VeVector3 *c, 
		       float *scale_ret) {
  int i;
  for(i = 0; i < 3; i++)
    c->data[i] = a->data[i][0]*b->data[0] +
      a->data[i][1]*b->data[1] +
      a->data[i][2]*b->data[2] +
      a->data[i][3];
  if (scale_ret) {
    i = 3;
    *scale_ret = a->data[i][0]*b->data[0] +
      a->data[i][1]*b->data[1] +
      a->data[i][2]*b->data[2] +
      a->data[i][3];
  }
}

void veVectorCross(VeVector3 *a, VeVector3 *b, VeVector3 *c) {
  c->data[0] = a->data[1]*b->data[2] - a->data[2]*b->data[1];
  c->data[1] = a->data[2]*b->data[0] - a->data[0]*b->data[2];
  c->data[2] = a->data[0]*b->data[1] - a->data[1]*b->data[0];
}

float veVectorMag(VeVector3 *v) {
  float n;
  n = v->data[0]*v->data[0] + v->data[1]*v->data[1] + v->data[2]*v->data[2];
  if (n == 0.0)
    return n;
  else
    return sqrt(n);
}

void veVectorNorm(VeVector3 *v) {
  float n;
  n = v->data[0]*v->data[0] + v->data[1]*v->data[1] + v->data[2]*v->data[2];
  if (n == 0.0) {
    veWarning(MODULE, "veVectorNorm: vector magnitude is 0");
    return;
  }
  n = sqrt(n);
  v->data[0] /= n;
  v->data[1] /= n;
  v->data[2] /= n;
}

void veQuatNorm(VeQuat *q) {
  float n;
  n = q->data[0]*q->data[0] + q->data[1]*q->data[1] + q->data[2]*q->data[2] +
    q->data[3]*q->data[3];
  if (n == 0.0) { /* this should really be an epsilon test shouldn't it? */
    veWarning(MODULE, "veVectorNorm: vector magnitude is 0");
    return;
  }
  n = sqrt(n);
  q->data[0] /= n;
  q->data[1] /= n;
  q->data[2] /= n;
  q->data[3] /= n;
}

/* grabbed this math from the gluLookAt(3gl) man page */
void veMatrixLookAt(VeMatrix4 *m, VeVector3 *loc, VeVector3 *dir, 
		    VeVector3 *up) {
  VeVector3 f, upp, s, u;
  VeMatrix4 m1, m2;
  int i;

  f = *dir;
  veVectorNorm(&f);
  upp = *up;
  veVectorNorm(&upp);
  veVectorCross(&f,&upp,&s);
  veVectorCross(&s,&f,&u);

  veMatrixIdentity(&m1);
  for(i = 0; i < 3; i++) {
    m1.data[0][i] = s.data[i];
    m1.data[1][i] = u.data[i];
    m1.data[2][i] = -f.data[i];
  }
  veMatrixTranslate(&m2,loc,-1.0);
  veMatrixMult(&m1,&m2,m);
}

/* Similar to LookAt, except that it creates the inverse matrix */
void veMatrixInvLookAt(VeMatrix4 *m, VeVector3 *loc, VeVector3 *dir, 
		    VeVector3 *up) {
  VeVector3 f, upp, s, u;
  VeMatrix4 m1, m2;
  float base;

  f = *dir;
  veVectorNorm(&f);
  upp = *up;
  veVectorNorm(&upp);
  veVectorCross(&f,&upp,&s);
  veVectorCross(&s,&f,&u);

  /* Thanks to Maple for inverting this for me... */
  veMatrixIdentity(&m1);
  base = s.data[0]*u.data[1]*f.data[2] -
    s.data[0]*f.data[1]*u.data[2] -
    u.data[0]*s.data[1]*f.data[2] +
    u.data[0]*f.data[1]*s.data[2] +
    f.data[0]*s.data[1]*u.data[2] -
    f.data[0]*u.data[1]*s.data[2];
  m1.data[0][0] = (u.data[1]*f.data[2]-f.data[1]*u.data[2])/base;
  m1.data[0][1] = (f.data[1]*s.data[2]-s.data[1]*f.data[2])/base;
  m1.data[0][2] = (u.data[1]*s.data[2]-s.data[1]*u.data[2])/base;
  m1.data[1][0] = (f.data[0]*u.data[2]-u.data[0]*f.data[2])/base;
  m1.data[1][1] = (s.data[0]*f.data[2]-f.data[0]*s.data[2])/base;
  m1.data[1][2] = (s.data[0]*u.data[2]-u.data[0]*s.data[2])/base;
  m1.data[2][0] = (u.data[0]*f.data[1]-f.data[0]*u.data[1])/base;
  m1.data[2][1] = (f.data[0]*s.data[1]-s.data[0]*f.data[1])/base;
  m1.data[2][2] = (u.data[0]*s.data[1]-s.data[0]*u.data[1])/base;
  veMatrixTranslate(&m2,loc,1.0);  
  veMatrixMult(&m2,&m1,m);
}

float veVectorDot(VeVector3 *v1, VeVector3 *v2) {
  return v1->data[0]*v2->data[0] + v1->data[1]*v2->data[1] +
    v1->data[2]*v2->data[2];
}

void veMatrixFrustum(VeMatrix4 *m, float left, float right, float bottom,
		     float top, float near, float far) {
  /* ahh...another snarf from an OpenGL manual page */
  veMatrixIdentity(m);
  m->data[0][0] = 2*near/(right-left);
  m->data[1][1] = 2*near/(top-bottom);
  m->data[0][2] = (right+left)/(right-left);
  m->data[1][2] = (top+bottom)/(top-bottom);
  m->data[2][2] = -(far+near)/(far-near);
  m->data[2][3] = -(2*far*near)/(far-near);
  m->data[3][2] = -1.0;
  m->data[3][3] = 0.0;
}

void veQuatMult(VeQuat *a, VeQuat *b, VeQuat *c_ret) {
  /* beware...this way hacks lie...change the format of VeVector3/VeQuat at
     your own peril... */
  /* what on earth was I thinking here?  did I just make this up off the
     top of my head? */
  int i;
  c_ret->data[3] = a->data[3]*b->data[3] - 
    veVectorDot((VeVector3 *)a,(VeVector3 *)b);
  veVectorCross((VeVector3 *)a,(VeVector3 *)b,(VeVector3 *)c_ret);
  for(i = 0; i < 3; i++) 
    c_ret->data[i] += a->data[i]*b->data[3] + b->data[i]*a->data[3];
  veQuatNorm(c_ret);
}

void veQuatFromArb(VeQuat *q_ret, VeVector3 *axis, int fmt, float ang) {
  int i;
  float sa;
  if (fmt == VE_DEG)
    ang *= M_PI/180.0;
  sa = sin(ang/2.0);
  for(i = 0; i < 3; i++)
    q_ret->data[i] = axis->data[i]*sa;
  q_ret->data[3] = cos(ang/2.0);
  veQuatNorm(q_ret);
}

void veQuatToArb(VeVector3 *axis_ret, float *ang_ret, int fmt, VeQuat *q) {
  float ca,sa;
  int i;
  veQuatNorm(q);
  ca = q->data[3];
  *ang_ret = acos(ca)*2.0;
  sa = sqrt(1.0-ca*ca);
  if (fabs(sa) < 0.0005)
    sa = 1.0;
  for(i = 0; i < 3; i++)
    axis_ret->data[i] = q->data[i]/sa;
  if (fmt == VE_DEG)
    *ang_ret *= 180.0/M_PI;
}

void veQuatToRotMatrix(VeMatrix4 *m, VeQuat *q) {
  float xx,xy,xz,xw,yy,yz,yw,zz,zw;
  veQuatNorm(q);
  veMatrixIdentity(m);
  xx = q->data[0]*q->data[0];
  xy = q->data[0]*q->data[1];
  xz = q->data[0]*q->data[2];
  xw = q->data[0]*q->data[3];
  yy = q->data[1]*q->data[1];
  yz = q->data[1]*q->data[2];
  yw = q->data[1]*q->data[3];
  zz = q->data[2]*q->data[2];
  zw = q->data[2]*q->data[3];
  
  m->data[0][0] = 1 - 2*(yy+zz);
  m->data[0][1] = 2*(xy-zw);
  m->data[0][2] = 2*(xz+yw);
  m->data[1][0] = 2*(xy+zw);
  m->data[1][1] = 1 - 2*(xx+zz);
  m->data[1][2] = 2*(yz-xw);
  m->data[2][0] = 2*(xz-yw);
  m->data[2][1] = 2*(yz+xw);
  m->data[2][2] = 1 - 2*(xx+yy);
}

/* This algorithm from the Matrix and Quaternion FAQ
   http://www.j3d.org/matrix_faq/ 
*/
void veRotMatrixToQuat(VeQuat *q, VeMatrix4 *m) {
  float t,s,x,y,z,w;
  t = 1 + m->data[0][0] + m->data[1][1] + m->data[2][2];
  if (t > 1.0e-7) {
    s = sqrt(t)*2;
    x = (m->data[1][2] - m->data[2][1])/s;
    y = (m->data[2][0] - m->data[0][2])/s;
    z = (m->data[0][1] - m->data[1][0])/s;
    w = 0.25*s;
  } else if (m->data[0][0] > m->data[1][1] &&
	     m->data[0][0] > m->data[2][2]) {
    s = sqrt(1.0 + m->data[0][0] - m->data[1][1] - m->data[2][2])*2;
    x = 0.25*s;
    y = (m->data[0][1] + m->data[1][0])/s;
    z = (m->data[2][0] + m->data[0][2])/s;
    w = (m->data[1][2] - m->data[2][1])/s;
  } else if (m->data[1][1] > m->data[2][2]) {
    s = sqrt(1.0 + m->data[1][1] - m->data[0][0] - m->data[2][2])*2;
    x = (m->data[0][1] + m->data[1][0])/s;
    y = 0.25*s;
    z = (m->data[1][2] + m->data[2][1])/s;
    w = (m->data[2][0] - m->data[0][2])/s;
  } else {
    s = sqrt(1.0 + m->data[2][2] - m->data[0][0] - m->data[1][1])*2;
    x = (m->data[2][0] + m->data[0][2])/s;
    y = (m->data[1][2] + m->data[2][1])/s;
    z = 0.25*s;
    w = (m->data[0][1] - m->data[1][0])/s;
  }
  q->data[0] = x;
  q->data[1] = y;
  q->data[2] = z;
  q->data[3] = w;
}

void veMatrixPrint(VeMatrix4 *m, FILE *f) {
  int i;
  for(i = 0; i < 4; i++)
    fprintf(f, "[%g %g %g %g]\n", m->data[i][0], m->data[i][1],
	    m->data[i][2], m->data[i][3]);
}

void veVectorPrint(VeVector3 *v, FILE *f) {
  fprintf(f, "[%g %g %g]", v->data[0], v->data[1], v->data[2]);
}

void veQuatPrint(VeQuat *v, FILE *f) {
  fprintf(f, "[%g %gi %gj %gk]", v->data[0], v->data[1], v->data[2], 
	  v->data[3]);
}

void vePackVector(VeVector3 *v, float x, float y, float z) {
  v->data[0] = x;
  v->data[1] = y;
  v->data[2] = z;
}

int veMatrixReduce(VeMatrix4 *m) {
  int i,j,k;

  for(k = 0; k < 3; k++)
    for(i = k+1; i < 4; i++) {
      if (m->data[k][k] == 0.0)
	return 1; /* singular matrix */
      m->data[i][k] /= m->data[k][k];
      for(j = k+1; j < 3; j++)
	m->data[i][j] -= m->data[i][k]*m->data[k][j];
    }
  return 0;
}

void veMatrixSolve(VeMatrix4 *A, float *b) {
  int i,j;
  for(i = 1; i < 4; i++)
    for(j = 0; j < i; j++)
      b[i] -= A->data[i][j]*b[j];
  b[3] /= A->data[3][3];
  for(i = 2; i >= 0; i--) {
    for(j = i+1; j < 4; j++)
      b[i] -= A->data[i][j]*b[j];
    b[i] /= A->data[i][i];
  }
}

int veMatrixInvert(VeMatrix4 *m, VeMatrix4 *im) {
  int i,j;
  float v[3];
  
  if (veMatrixReduce(m))
    return 1;
  for(i = 0; i < 3; i++) {
    for(j = 0; j < 3; j++) {
      if (i == j)
	v[j] = 1.0;
      else
	v[j] = 0.0;
    }
    veMatrixSolve(m,v);
    for(j = 0; j < 3; j++)
      im->data[j][i] = v[j];
  }
  return 0;
}
