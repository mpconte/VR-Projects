/* interface between ve and BlueScript */
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ve.h>
#include <ve_keysym.h>

#include <bluescript.h>

#define MODULE "ve_blue"

/*@bsdoc

package vemath {
    longname {VE Math Procedures}
    desc {An interface to vectors, quaternions, matrices, etc.}
    doc {
	The following objects and procedures are only available to
	scripts running under the VE library.
    }
}

 */

/* The interpreter we use for all code */
static BSInterp *interp = NULL;
/* Serialize access to the interpreter */
static VeThrMutex *interp_mutex = NULL;

static int check_result(BSInterp *i, int code, char *ctx) {
  if (code != BS_OK) {
    if (bsIsResultClear(i)) {
      if (code != BS_ERROR)
	bsAppendResult(i,"invalid code returned from ",ctx," body: ",
		       bsCodeToString(code),NULL);
      else
	bsAppendResult(i,"error in ",ctx," script",NULL);
    } else {
      bsAppendResult(i,"\ncalled from '",ctx,"'",NULL);
    }
    return BS_ERROR;
  }
  return BS_OK;
}

static VeOption *mk_option(char *name, char *value) {
  VeOption *o = veAllocObj(VeOption);
  o->name = veDupString(name);
  o->value = veDupString(value ? value : "");
  return o;
}

static VeOption *get_option_body(BSInterp *i, BSObject *o) {
  BSParseSource ps;
  BSList l;
  VeOption *head = NULL, *tail = NULL, *opt;
  int k = 0, n;
  char *name, *value;
  bsListInit(&l);
  if (bsParseScript(i,bsStringSource(&ps,bsObjGetStringPtr(o)),&l) != BS_OK) {
    veWarning(MODULE,"option body is not a valid script");
    return NULL;
  }
  for (k = 0; k < bsListSize(&l); k++) {
    BSObject *x;
    BSList *ll;
    if (!(x = bsListIndex(&l,k)) || !(ll = bsObjGetList(i,x))) {
      veWarning(MODULE,"invalid line in option body - not a list");
      continue;
    }
    n = bsListSize(ll);
    if (n < 1 || n > 2) {
      veWarning(MODULE,"line in option body is list of size %d: expected 1 or 2 - ignoring",n);
      continue;
    }
    name = bsObjGetStringPtr(bsListIndex(ll,0));
    value = (n >= 2 ? bsObjGetStringPtr(bsListIndex(ll,1)) : "");
    opt = mk_option(name,value);
    if (tail)
      tail->next = opt;
    else
      head = opt;
    tail = opt;
  }
  return head;
}

/*@bsdoc

object VeVector3 {
    doc {
	This object represents the same data as <code>VeVector3</code>
	object in VE.  VeVector3 objects are required for some math
	functions.  Lists of 3 floats will be automatically converted
	to VeVector3 objects as needed.
    }
}

*/

/* math support */
static void vec3_freeproc(void *p, void *cdata) {
  if (p)
    veFree(p);
}

static void *vec3_copyproc(void *p, void *cdata) {
  VeVector3 *v1, *v2;
  v1 = (VeVector3 *)p;
  v2 = veAllocObj(VeVector3);
  *v2 = *v1;
  return (void *)v2;
}

/*@bsdoc
object VeVector3 {
    doc {
	An analogue to the <code>VeVector3</code> type in VE.
    }
}
*/

static BSCacheDriver vec3_driver = {
  "VeVector3",
  0,             /* id - get it later */
  vec3_freeproc, /* freeproc */
  vec3_copyproc, /* copyproc */
  NULL           /* cdata */
};

BSCacheDriver *veBlueGetVector3Driver(void) {
  if (vec3_driver.id == 0)
    vec3_driver.id = bsUniqueId();
  return &vec3_driver;
}

/* ...consider making this a visible function so that apps
      providing their own scripts can use this... */

static VeVector3 *get_vector3(BSInterp *i, BSObject *o) {
  VeVector3 *v = NULL, vp;
  BSCacheDriver *d;
  d = veBlueGetVector3Driver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  if (bsObjGetCache(o,d->id,(void **)&v)) {
    /* try to convert... */
    if (sscanf(bsObjGetStringPtr(o),"%f %f %f",
	       &(vp.data[0]), &(vp.data[1]), &(vp.data[2])) != 3) {
      bsSetStringResult(i,"string is not a vector3",BS_S_STATIC);
      return NULL;
    }
    v = veAllocObj(VeVector3);
    *v = vp;
    bsObjAddCache(o,d,(void *)v);
  }
  return v;
}

static void set_vector3(BSObject *o, VeVector3 *v) {
  char buf[256];
  VeVector3 *vo;
  BSCacheDriver *d;
  
  snprintf(buf,256,"%g %g %g",v->data[0],v->data[1],v->data[2]);
  bsObjSetString(o,buf,-1,BS_S_VOLATILE);
  d = veBlueGetVector3Driver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  vo = veAllocObj(VeVector3);
  *vo = *v;
  bsObjAddCache(o,d,(void *)vo);
}

VeVector3 *veBlueGetVector3(BSObject *o) {
  return get_vector3(interp,o);
}

void veBlueSetVector3Result(VeVector3 *v) {
  set_vector3(bsGetResult(interp),v);
}

/*@bsdoc
procedure v3add {
    usage {v3add <v1> <v2>}
    returns {A VeVector3 object corresponding to <i>v1 + v2</i>.}
}
*/

static int cmd_v3add(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3add <v1> <v2>";
  VeVector3 *v1, *v2, r;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v1 = get_vector3(i,objv[1])) ||
      !(v2 = get_vector3(i,objv[2])))
    return BS_ERROR;
  r.data[0] = v1->data[0]+v2->data[0];
  r.data[1] = v1->data[1]+v2->data[1];
  r.data[2] = v1->data[2]+v2->data[2];
  set_vector3(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure v3sub {
    usage {v3sub <v1> <v2>}
    returns {A VeVector3 object corresponding to <i>v1 - v2</i>.}
}
*/
static int cmd_v3sub(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3sub <v1> <v2>";
  VeVector3 *v1, *v2, r;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v1 = get_vector3(i,objv[1])) ||
      !(v2 = get_vector3(i,objv[2])))
    return BS_ERROR;
  r.data[0] = v1->data[0]-v2->data[0];
  r.data[1] = v1->data[1]-v2->data[1];
  r.data[2] = v1->data[2]-v2->data[2];
  set_vector3(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure v3scale {
    usage {v3scale <v> <f>}
    returns {A VeVector3 object which is vector <i>v</i> scaled by
	floating-point factor <i>f</i>.}
}
*/
static int cmd_v3scale(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3scale <v> <f>";
  VeVector3 *v, r;
  BSFloat f;
  if (!(v = get_vector3(i,objv[1])) ||
      (bsObjGetFloat(i,objv[2],&f) != BS_OK))
    return BS_ERROR;
  r.data[0] = v->data[0]*f;
  r.data[1] = v->data[1]*f;
  r.data[2] = v->data[2]*f;
  set_vector3(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure v3cross {
    usage {v3cross <v1> <v2>}
    returns {A VeVector3 object which is the cross-product of vectors
	<i>v1</i> and <i>v2</i>.}
}
*/
static int cmd_v3cross(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3cross <v1> <v2>";
  VeVector3 *v1, *v2, r;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v1 = get_vector3(i,objv[1])) ||
      !(v2 = get_vector3(i,objv[2])))
    return BS_ERROR;
  veVectorCross(v1,v2,&r);
  set_vector3(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure v3dot {
    usage {v3dot <v1> <v2>}
    returns {The dot-product (or inner-product) of vectors <i>v1</i>
	and <i>v2</i>.}
}
*/
static int cmd_v3dot(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3dot <v1> <v2>";
  VeVector3 *v1, *v2, r;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v1 = get_vector3(i,objv[1])) ||
      !(v2 = get_vector3(i,objv[2])))
    return BS_ERROR;
  bsSetFloatResult(i,veVectorDot(v1,v2));
  return BS_OK;
}

/*@bsdoc
procedure v3ind {
    usage {v3ind <v> (0|1|2|x|y|z)}
    returns {The specified component of vector <i>v</i>.}
}
*/
static int cmd_v3ind(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3ind <v> <i>, where i = 0, 1 or 2 or x, y, or z";
  VeVector3 *v;
  BSInt k;
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v = get_vector3(i,objv[1])))
    return BS_ERROR;
  if (bsObjGetInt(i,objv[2],&k) != BS_OK) {
    char *s;
    bsClearResult(i);
    s = bsObjGetStringPtr(objv[2]);
    if (strcmp(s,"x") == 0)
      k = 0;
    else if (strcmp(s,"y") == 0)
      k = 1;
    else if (strcmp(s,"z") == 0)
      k = 2;
    else {
      bsSetStringResult(i,"invalid index for v3ind",BS_S_STATIC);
      return BS_ERROR;
    }
  } else if (k < 0 || k > 2) {
    bsSetStringResult(i,"index is out of range for v3ind",BS_S_STATIC);
    return BS_ERROR;
  }
  bsSetFloatResult(i,v->data[k]);
  return BS_OK;
}

/*@bsdoc
procedure v3mag {
    usage {v3mag <v>}
    returns {The magnitude of vector <i>v</i>.}
}
*/
static int cmd_v3mag(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3mag <v>";
  VeVector3 *v;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v = get_vector3(i,objv[1])))
    return BS_ERROR;
  bsSetFloatResult(i,veVectorMag(v));
  return BS_OK;
}

/*@bsdoc
procedure v3norm {
    usage {v3norm <v>}
    returns {A copy of vector <i>v</i> whose magnitude has been
	normalized to 1.}
}
*/
static int cmd_v3norm(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: v3norm <v>";
  VeVector3 *v, r;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v = get_vector3(i,objv[1])))
    return BS_ERROR;
  r = *v;
  veVectorNorm(&r);
  set_vector3(bsGetResult(i),&r);
  return BS_OK;
}

/* quaternion */
static void quat_freeproc(void *p, void *cdata) {
  if (p)
    veFree(p);
}

static void *quat_copyproc(void *p, void *cdata) {
  VeQuat *v1, *v2;
  v1 = (VeQuat *)p;
  v2 = veAllocObj(VeQuat);
  *v2 = *v1;
  return (void *)v2;
}

/*@bsdoc
object VeQuat {
    doc {
	An analogue to the <code>VeQuat</code> type in VE.
    }
}
*/

static BSCacheDriver quat_driver = {
  "VeQuat",
  0,             /* id - get it later */
  quat_freeproc, /* freeproc */
  quat_copyproc, /* copyproc */
  NULL           /* cdata */
};

BSCacheDriver *veBlueGetQuatDriver(void) {
  if (quat_driver.id == 0)
    quat_driver.id = bsUniqueId();
  return &quat_driver;
}

static VeQuat *get_quat(BSInterp *i, BSObject *o) {
  VeQuat *v = NULL, vp;
  BSCacheDriver *d;
  d = veBlueGetQuatDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  if (bsObjGetCache(o,d->id,(void **)&v)) {
    /* try to convert... */
    if (sscanf(bsObjGetStringPtr(o),"%f %f %f %f",
	       &(vp.data[0]), &(vp.data[1]), &(vp.data[2]), 
	       &(vp.data[3])) != 4) {
      bsSetStringResult(i,"string is not a quat",BS_S_STATIC);
      return NULL;
    }
    v = veAllocObj(VeQuat);
    *v = vp;
    bsObjAddCache(o,d,(void *)v);
  }
  return v;
}

static void set_quat(BSObject *o, VeQuat *v) {
  char buf[256];
  VeQuat *vo;
  BSCacheDriver *d;
  
  snprintf(buf,256,"%g %g %g %g",v->data[0],v->data[1],v->data[2],
	   v->data[3]);
  bsObjSetString(o,buf,-1,BS_S_VOLATILE);
  d = veBlueGetQuatDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  vo = veAllocObj(VeQuat);
  *vo = *v;
  bsObjAddCache(o,d,(void *)vo);
}

VeQuat *veBlueGetQuat(BSObject *o) {
  return get_quat(interp,o);
}

void veBlueSetQuatResult(VeQuat *v) {
  set_quat(bsGetResult(interp),v);
}

/*@bsdoc
procedure qnorm {
    usage {qnorm <q>}
    returns {A normalized quaternion.}
}
 */
static int cmd_qnorm(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: qnorm <q>";
  VeQuat *q, r;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(q = get_quat(i,objv[1])))
    return BS_ERROR;
  r = *q;
  veQuatNorm(&r);
  set_quat(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure qmult {
    usage {qmult <q1> <q2>}
    returns {The product of the two quaternions (which is itself a quaternion).}
}
*/
static int cmd_qmult(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: qmult <q1> <q2>";
  VeQuat *q1, *q2, r;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(q1 = get_quat(i,objv[1])) ||
      !(q2 = get_quat(i,objv[2])))
    return BS_ERROR;
  veQuatMult(q1,q2,&r);
  set_quat(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure qarb {
    usage {qarb <v> <ang>}
    returns {A quaternion corresponding to a rotation of <i>ang</i>
	degrees around an arbitrary axis represented by VeVector3 <i>v</i>.}
}
*/
static int cmd_qarb(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: qarb <axis> <angle>";
  VeQuat r;
  VeVector3 *v;
  BSFloat f;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(v = get_vector3(i,objv[1])) ||
      (bsObjGetFloat(i,objv[2],&f) != BS_OK))
    return BS_ERROR;
  veQuatFromArb(&r,v,VE_DEG,f);
}

/*@bsdoc
procedure qaxis {
    usage {qaxis <q>}
    returns {The axis of rotation corresponding to this quaternion.}
}
*/
static int cmd_qaxis(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: qaxis <q>";
  VeQuat *q;
  VeVector3 r;
  float a;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(q = get_quat(i,objv[1])))
    return BS_ERROR;
  veQuatToArb(&r,&a,VE_RAD,q);
  set_vector3(bsGetResult(i),&r);
  return BS_OK;
}

/*@bsdoc
procedure qang {
    usage {qang <q>}
    returns {The angle of rotation in degrees corresponding to this quaternion.}
}
*/
static int cmd_qang(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: qang <q>";
  VeQuat *q;
  VeVector3 r;
  float a;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(q = get_quat(i,objv[1])))
    return BS_ERROR;
  veQuatToArb(&r,&a,VE_RAD,q);
  bsSetFloatResult(i,a);
  return BS_OK;
}

/*@bsdoc
procedure qind {
    usage {qind <q> (0|1|2|3|x|y|z|w)}
    returns {The specified element of the quaternion.}
}
*/
static int cmd_qind(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: qind <q> <i>, where i = 0, 1, 2, or 3 or x, y, z, or w";
  VeQuat *q;
  BSInt k;
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(q = get_quat(i,objv[1])))
    return BS_ERROR;
  if (bsObjGetInt(i,objv[2],&k) != BS_OK) {
    char *s;
    bsClearResult(i);
    s = bsObjGetStringPtr(objv[2]);
    if (strcmp(s,"x") == 0)
      k = 0;
    else if (strcmp(s,"y") == 0)
      k = 1;
    else if (strcmp(s,"z") == 0)
      k = 2;
    else if (strcmp(s,"w") == 0)
      k = 3;
    else {
      bsSetStringResult(i,"invalid index for qind",BS_S_STATIC);
      return BS_ERROR;
    }
  } else if (k < 0 || k > 3) {
    bsSetStringResult(i,"index is out of range for qind",BS_S_STATIC);
    return BS_ERROR;
  }
  bsSetFloatResult(i,q->data[k]);
  return BS_OK;
}

/*@bsdoc
object VeMatrix4 {
    doc {
	An analogue to the <code>VeMatrix4</code> type in VE.
    }
}
*/

/* matrix4 */
static void mtx4_freeproc(void *p, void *cdata) {
  if (p)
    veFree(p);
}

static void *mtx4_copyproc(void *p, void *cdata) {
  VeMatrix4 *m1, *m2;
  m1 = (VeMatrix4 *)p;
  m2 = veAllocObj(VeMatrix4);
  *m2 = *m1;
  return (void *)m2;
}

static BSCacheDriver mtx4_driver = {
  "VeMatrix4",
  0,
  mtx4_freeproc,
  mtx4_copyproc,
  NULL
};

BSCacheDriver *veBlueGetMatrix4Driver(void) {
  if (mtx4_driver.id == 0)
    mtx4_driver.id = bsUniqueId();
  return &mtx4_driver;
}

static VeMatrix4 *get_matrix4(BSInterp *i, BSObject *o) {
  VeMatrix4 *m = NULL, mp;
  BSCacheDriver *d;

  d = veBlueGetMatrix4Driver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  if (bsObjGetCache(o,d->id,(void **)&m)) {
    /* try to convert */
    /* format is not important - ignore '{' '}', whitespace */
    /* otherwise, as long as we get 16 floats, we're happy */
    char *s;
    int x,y,skip;
    s = bsObjGetStringPtr(o);
    for (x = 0; x < 4; x++) {
      for (y = 0; y < 4; y++) {
	while (isspace(*s) || *s == '{' || *s == '}')
	  s++;
	skip = 0;
	if (sscanf(s,"%f%n",&(mp.data[x][y]),&skip) != 1) {
	  bsSetStringResult(i,"string is not a matrix",BS_S_STATIC);
	  return NULL;
	}
	s += skip;
      }
    }
    m = veAllocObj(VeMatrix4);
    *m = mp;
    bsObjAddCache(o,d,(void *)m);
  }
  return m;
}

static void set_matrix4(BSObject *o, VeMatrix4 *m) {
  char buf[256];
  int x;
  BSString *s;
  VeMatrix4 *mo;
  BSCacheDriver *d;

  bsObjSetString(o,NULL,0,BS_S_VOLATILE);
  s = bsObjGetString(o);
  for (x = 0; x < 4; x++) {
    snprintf(buf,256,"{%g %g %g %g}",
	     m->data[x][0], m->data[x][1], m->data[x][2], m->data[x][3]);
    if (x > 0)
      bsStringAppChr(s,' ');
    bsStringAppend(s,buf,-1);
  }
  d = veBlueGetMatrix4Driver();
  assert(d != NULL);
  assert(d->id != (BSId)0);
  mo = veAllocObj(VeMatrix4);
  *mo = *m;
  bsObjAddCache(o,d,(void *)mo);
}

VeMatrix4 *veBlueGetMatrix4(BSObject *o) {
  return get_matrix4(interp,o);
}

void veBlueSetMatrix4Result(VeMatrix4 *v) {
  set_matrix4(bsGetResult(interp),v);
}

/*@bsdoc
procedure m4ident {
    usage {m4ident}
    returns {An identity matrix.}
}
*/
static int cmd_m4ident(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4ident";
  VeMatrix4 m;
  
  if (objc != 1) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  
  veMatrixIdentity(&m);
  set_matrix4(bsGetResult(i),&m);
  return BS_OK;
}

/*@bsdoc
procedure m4rotate {
    usage {m4rotate <v> <ang>}
    returns {A matrix representing a rotation of <i>ang</i> degrees
	around an arbitrary axis given by VeVector3 <i>v</i>.}
}
*/
static int cmd_m4rotate(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4rotate <axis> <angle>";
  VeMatrix4 m;
  VeVector3 *axis;
  BSFloat angle;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  if (!(axis = get_vector3(i,objv[1])))
    return BS_ERROR;
  if (bsObjGetFloat(i,objv[2],&angle))
    return BS_ERROR;
  veMatrixRotateArb(&m,axis,VE_DEG,angle);
  set_matrix4(bsGetResult(i),&m);
  return BS_OK;
}

/*@bsdoc
procedure m4trans {
    usage {m4trans <v>}
    returns {A matrix representing a translation given
	by VeVector3 <i>v</i>.}
}
*/
static int cmd_m4trans(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4trans <vector>";
  VeMatrix4 m;
  VeVector3 *axis;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(axis = get_vector3(i,objv[1])))
    return BS_ERROR;
  veMatrixTranslate(&m,axis,1.0);
  set_matrix4(bsGetResult(i),&m);
  return BS_OK;
}

/*@bsdoc
procedure m4mult {
    usage {m4mult <m1> <m2>}
    returns {The product of the two matrices.}
}
*/
static int cmd_m4mult(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4mult <m1> <m2>";
  VeMatrix4 m, *m1, *m2;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(m1 = get_matrix4(i,objv[1])) ||
      !(m2 = get_matrix4(i,objv[2])))
    return BS_ERROR;

  veMatrixMult(m1,m2,&m);
  set_matrix4(bsGetResult(i),&m);
  return BS_OK;
}

/*@bsdoc
procedure m4multv {
    usage {m4multv <m> <v>}
    returns {The transformation of vector <i>v</i> by matrix <i>m</i>.}
}
*/
static int cmd_m4multv(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4multv <m> <v>";
  VeMatrix4 *m1;
  VeVector3 v, *v1;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(m1 = get_matrix4(i,objv[1])) ||
      !(v1 = get_vector3(i,objv[2]))) {
    bsAppendResult(i,", failed getting matrix or vector",NULL);
    return BS_ERROR;
  }

  veMatrixMultPoint(m1,v1,&v,NULL);
  set_vector3(bsGetResult(i),&v);
  return BS_OK;
}

/*@bsdoc
procedure m4invert {
    usage {m4invert <m>}
    returns {The inverse of matrix <i>m</i>.  If the matrix cannot be
	inverted then an error is generated.}
}
*/
static int cmd_m4invert(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4invert <m>";
  VeMatrix4 m, mi, *m1;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(m1 = get_matrix4(i,objv[1])))
    return BS_ERROR;
  /* copy it */
  m = *m1;
  if (veMatrixInvert(&m,&mi)) {
    bsSetStringResult(i,"matrix cannot be inverted",BS_S_STATIC);
    return BS_ERROR;
  } else {
    set_matrix4(bsGetResult(i),&mi);
    return BS_OK;
  }
}

/*@bsdoc
procedure m4ind {
    usage {m4ind <m> (0|1|2|3) (0|1|2|3)}
    returns {The given element of the matrix.}
}
*/

static int cmd_m4ind(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: m4ind <m> <i> <j>, where i,j = [0..3] or [x,y,z,w]";
  VeMatrix4 *m;
  BSInt x,y;

  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(m = get_matrix4(i,objv[1])))
    return BS_ERROR;
  if (bsObjGetInt(i,objv[2],&x)) {
    char *s = bsObjGetStringPtr(objv[2]);
    if (strcmp(s,"x") == 0)        x = 0;
    else if (strcmp(s,"y") == 0)   x = 1;
    else if (strcmp(s,"z") == 0)   x = 2;
    else if (strcmp(s,"w") == 0)   x = 3;
    else {
      bsClearResult(i);
      bsAppendResult(i,"invalid matrix index: ",s,NULL);
      return BS_ERROR;
    }
  }
  if (bsObjGetInt(i,objv[3],&y)) {
    char *s = bsObjGetStringPtr(objv[3]);
    if (strcmp(s,"x") == 0)        y = 0;
    else if (strcmp(s,"y") == 0)   y = 1;
    else if (strcmp(s,"z") == 0)   y = 2;
    else if (strcmp(s,"w") == 0)   y = 3;
    else {
      bsClearResult(i);
      bsAppendResult(i,"invalid matrix index: ",s,NULL);
      return BS_ERROR;
    }
  }
  bsSetFloatResult(i,m->data[x][y]);
  return BS_OK;
}

/* scalar functions */
/*@bsdoc
procedure deg2rad {
    usage {deg2rad <a>}
    returns {Converts a value in degrees to a value in radians.}
}
*/
static int cmd_deg2rad(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: deg2rad <angle>";
  BSFloat f;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (bsObjGetFloat(i,objv[1],&f))
    return BS_ERROR;
  bsSetFloatResult(i,f*M_PI/180.0);
  return BS_OK;
}

/*@bsdoc
procedure rad2deg {
    usage {rad2deg <a>}
    returns {Converts a value in radians to a value in degrees.}
}
*/
static int cmd_rad2deg(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: rad2deg <angle>";
  BSFloat f;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (bsObjGetFloat(i,objv[1],&f))
    return BS_ERROR;
  bsSetFloatResult(i,f*180.0/M_PI);
  return BS_OK;
}

/*@bsdoc
procedure sqrt {
    usage {sqrt <x>}
    returns {The square-root of the given value.}
} 
*/
static int cmd_sqrt(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: sqrt <x>";
  BSFloat f;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (bsObjGetFloat(i,objv[1],&f))
    return BS_ERROR;
  bsSetFloatResult(i,sqrt(f));
  return BS_OK;
}

/*@bsdoc
procedure abs {
    usage {abs <x>}
    returns {The absolute value of the given value.}
} 
*/
static int cmd_abs(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: abs <x>";
  BSFloat f;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (bsObjGetFloat(i,objv[1],&f))
    return BS_ERROR;
  bsSetFloatResult(i,abs(f));
  return BS_OK;
}

#define TRIG_SIN  0
#define TRIG_COS  1
#define TRIG_TAN  2
#define TRIG_ASIN 3
#define TRIG_ACOS 4
/* ATAN is a special case... */

/*@bsdoc
procedure sin {
    usage {sin <a>}
    returns {The sine of the angle <i>a</i>.  The angle must be given in radians.}
}
procedure cos {
    usage {cos <a>}
    returns {The cosine of the angle <i>a</i>.  The angle must be given in radians.}
}
procedure tan {
    usage {tan <a>}
    returns {The tangent of the angle <i>a</i>.  The angle must be given in radians.}
}
procedure asin {
    usage {asin <a>}
    returns {The arc sine of <i>a</i> in radians.  If <i>a</i> is out
	of the range -1..1 then an error will be generated.}
}
procedure acos {
    usage {acos <a>}
    returns {The arc coasine of <i>a</i> in radians.  If <i>a</i> is out
	of the range -1..1 then an error will be generated.}
}
*/

static int cmd_trig(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  BSFloat f;
  
  if (objc != 2) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",bsObjGetStringPtr(objv[0])," <f>",NULL);
    return BS_ERROR;
  }
  if (bsObjGetFloat(i,objv[1],&f))
    return BS_ERROR;
  switch ((int)cdata) {
  case TRIG_SIN:  f = sin(f); break;
  case TRIG_COS:  f = cos(f); break;
  case TRIG_TAN:  f = tan(f); break;
  case TRIG_ASIN: 
    if (f < -1.0 || f > 1.0) {
      bsSetStringResult(i,"asin: value out of range",BS_S_STATIC);
      return BS_ERROR;
    }
    f = asin(f);
    break;
  case TRIG_ACOS:
    if (f < -1.0 || f > 1.0) {
      bsSetStringResult(i,"acos: value out of range",BS_S_STATIC);
      return BS_ERROR;
    }
    f = acos(f);
    break;
  default:
    veFatalError(MODULE,"BlueScript trig function called for invalid case: %d",
		 (int)cdata);
  }
  bsSetFloatResult(i,f);
  return BS_OK;
}

/*@bsdoc
procedure atan {
    usage {atan <a>}
    usage {atan <y> <x>}
    returns {The arc tangent of its arguments.}
    doc {
	If only one argument is specified, then the arc tangent of
	that value is returned.  If two arguments are specified then
	the arc tangent of <i>y/x</i> is returned - additionally, the
	signs of <i>y</i> and <i>x</i> are used to determine correct
	quadrant for the result.
    }
}
*/

static int cmd_atan(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: atan <f>  or  atan <y> <x>";

  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (objc == 2) {
    BSFloat f;
    if (bsObjGetFloat(i,objv[1],&f))
      return BS_ERROR;
    bsSetFloatResult(i,atan(f));
  } else {
    BSFloat y,x;
    assert(objc == 3);
    if (bsObjGetFloat(i,objv[1],&y) ||
	bsObjGetFloat(i,objv[2],&y))
      return BS_ERROR;
    bsSetFloatResult(i,atan2(y,x));
  }
  return BS_OK;
}

/* Environment Commands */

static VeEnv *ctx_env = NULL;
static VeWall *ctx_wall = NULL;
static VeWindow *ctx_window = NULL;
static VeOption **ctx_optlist = NULL;

/*@bsdoc
package veenv {
    longname {VE Environment}
    desc {Procedures for generating environments, audio setups, user profiles.}
    doc {
	The following objects and procedures are only available to
	scripts running under the VE library.
	<p>Note that many of the following procedures may be available
	only within specific contexts (e.g. the <code>wall</code>
				       procedure is only available
				       when called from within the
				       body of an <code>env</code> command).
    }
}
*/

/*@bsdoc
procedure option {
    usage {option <name> <value>}
    desc {Sets a generic option for an object.}
    doc {
	<p><i>Valid in: audio, env, output, wall, window</i></p>
    }
}
*/
static int cmd_option(BSInterp *i, int objc, BSObject *objv[],
		      void *cdata) {
  static char *usage = "usage: option <name> <value>";
  VeOption *o;
  if (!ctx_optlist) {
    bsSetStringResult(i,"'option' command in invalid context",BS_S_STATIC);
    return BS_ERROR;
  }
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  o = veAllocObj(VeOption);
  o->name = veDupString(bsObjGetStringPtr(objv[1]));
  o->value = veDupString(bsObjGetStringPtr(objv[2]));
  o->next = *ctx_optlist;
  *ctx_optlist = o;
  return BS_OK;
}

/*@bsdoc
procedure {window...display} {
    usage {display <value>}
    desc {Specifies the destination display for a window.}
    doc {<p><i>Valid in: window</i></p>}
}

procedure {window...geometry} {
    usage {geometry <width>x<height>[(+|-)<x>(+|-)<y>]}
    desc {Specifies the location and size of a window on a display.}
    doc {<p><i>Valid in: window</i></p>}
}
*/
static int cmd_window_string(BSInterp *i, int objc,
			     BSObject *objv[], void *cdata) {
  char *prop, *val;

  prop = bsObjGetStringPtr(objv[0]);
  
  if (objc != 2) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",prop," <value>",NULL);
    return BS_ERROR;
  }
  
  val = bsObjGetStringPtr(objv[1]);

  if (!ctx_window) {
    bsClearResult(i);
    bsAppendResult(i,"cannot set '",prop,"' outside of window context",NULL);
    return BS_ERROR;
  }

  if (strcmp(prop,"display") == 0) {
    veFree(ctx_window->display);
    ctx_window->display = veDupString(val);
  } else if (strcmp(prop,"geometry") == 0) {
    veFree(ctx_window->geometry);
    ctx_window->geometry = veDupString(val);
  } else {
    bsClearResult(i);
    bsAppendResult(i,"unknown string property '",prop,"'",NULL);
    return BS_ERROR;
  }
  return BS_OK;
}

/*@bsdoc
procedure window...err {
    usage {err <x> <y>}
    desc {Sets the width and height correction values for a window.}
    doc {<p><i>Valid in: window</i></p>}
}
procedure window...offset {
    usage {offset <x> <y>}
    desc {Sets the location correction values for a window.}
    doc {<p><i>Valid in: window</i></p>}
}
*/
static int cmd_window_float2(BSInterp *i, int objc, BSObject *objv[],
			     void *cdata) {
  char *prop;
  BSFloat val[2];

  prop = bsObjGetStringPtr(objv[0]);
  
  if (objc != 3) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",prop," <x> <y>",NULL);
    return BS_ERROR;
  }
  
  if (bsObjGetFloat(i,objv[1],&(val[0])) != BS_OK ||
      bsObjGetFloat(i,objv[2],&(val[1])) != BS_OK)
    return BS_ERROR;

  if (!ctx_window) {
    bsClearResult(i);
    bsAppendResult(i,"cannot set '",prop,"' outside of window context",NULL);
    return BS_ERROR;
  }
  
  if (strcmp(prop,"err") == 0) {
    ctx_window->width_err = val[0];
    ctx_window->height_err = val[1];
  } else if (strcmp(prop,"offset") == 0) {
    ctx_window->offset_x = val[0];
    ctx_window->offset_y = val[1];
  } else {
    bsClearResult(i);
    bsAppendResult(i,"unknown float[2] property '",prop,"'",NULL);
    return BS_ERROR;
  }
  
  return BS_OK;
}

/*@bsdoc
procedure window...slave {
    usage {slave [<node> [<process> [<thread>]]]}
    desc {Determines where in a multi-processing environment,
	rendering for a particular window should take place.}
    doc {<p><i>Valid in: window</i></p>}
}
*/
static int cmd_window_slave(BSInterp *i, int objc, BSObject *objv[],
			    void *cdata) {
  static char *usage = "slave [<node> [<process> [<thread>]]]";
  if (objc < 1 || objc > 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!ctx_window) {
    bsSetStringResult(i,"cannot set 'slave' outside of 'window' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  if (objc > 1) {
    veFree(ctx_window->slave.node);
    ctx_window->slave.node = veDupString(bsObjGetStringPtr(objv[1]));
  }
  if (objc > 2) {
    veFree(ctx_window->slave.process);
    ctx_window->slave.process = veDupString(bsObjGetStringPtr(objv[2]));
  }
  if (objc > 3) {
    veFree(ctx_window->slave.thread);
    ctx_window->slave.thread = veDupString(bsObjGetStringPtr(objv[3]));
  }
  return BS_OK;
}

/*@bsdoc
procedure window...viewport {
    usage {viewport <x> <y> <width> <height>}
    desc {Sets the viewport for the a specific window.}
    doc {<p><i>Valid in: window</i></p>}
}
*/
static int cmd_window_viewport(BSInterp *i, int objc, BSObject *objv[],
			       void *cdata) {
  static char *usage = "viewport <x> <y> <width> <height>";
  int k;
  BSFloat v[4];

  if (objc != 5) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!ctx_window) {
    bsSetStringResult(i,"cannot set 'viewport' outside of 'window' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  for (k = 0; k < 4; k++) {
    if (bsObjGetFloat(i,objv[k+1],&(v[k])) != BS_OK)
      return BS_ERROR;
  }
  for (k = 0; k < 4; k++)
    ctx_window->viewport[k] = v[k];
  return BS_OK;
}

/*@bsdoc
procedure window...distort {
    usage {distort <m>}
    usage {distort {<m11> <m12> <m13> <m14>} {<m21> <m22> <m23> <m24>} {<m31> <m32> <m33>
	<m34>} {<m41> <m42> <m43> <m44>}}
    desc {Specifies an arbitrary distortion matrix for this window.}
    doc {
	<p><i>Valid in: window</i></p>
	If a single argument is offered, it is treated as a VeMatrix4
	object.  The other syntax is preserved for
	backwards-compatibility; its future use is strongly discouraged.
    }
}
*/
static int cmd_window_distort(BSInterp *i, int objc, BSObject *objv[],
			      void *cdata) {
  static char *usage = "distort {m11 m12 m13 m14} {m21 m22 m23 m24} "
    "{m31 m32 m33 m34} {m41 42 43 44}\n\tor\ndistort <matrix>";
  int u,v;

  if (objc != 5 && objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!ctx_window) {
    bsSetStringResult(i,"cannot set 'distort' outside of 'window' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  if (objc == 2) {
    VeMatrix4 *m;
    if (!(m = get_matrix4(i,objv[1])))
      return BS_ERROR;
    ctx_window->distort = *m;
  } else {
    for (u = 0; u < 4; u++) {
      BSList *l;
      if (!(l = bsObjGetList(i,objv[u+1])))
	return BS_ERROR;
      if (bsListSize(l) != 4) {
	bsSetStringResult(i,usage,BS_S_STATIC);
	return BS_ERROR;
      }
    }
    for (u = 0; u < 4; u++) {
      BSList *l;
      l = bsObjGetList(i,objv[u+1]);
      assert(l != NULL);
      for (v = 0; v < 4; v++) {
	BSFloat f;
	if (bsObjGetFloat(i,bsListIndex(l,v),&f) != BS_OK)
	  return BS_ERROR;
	ctx_window->distort.data[u][v] = f;
      }
    }
  }
  return BS_OK;
}

/*@bsdoc
procedure window...sync {
    usage {sync}
    desc {Marks this window as being synchronously rendered (the default).}
    doc {<p><i>Valid in: window</i></p>}
}

procedure window...async {
    usage {async}
    desc {Marks this window as being asynchronously rendered.  That
	is, the rendering engine will not wait for this window to
	finish drawing before proceeding to the next frame.}
    doc {<p><i>Valid in: window</i></p>}
}

*/
static int cmd_window_sync(BSInterp *i, int objc, BSObject *objv[],
			   void *cdata) {
  if (objc != 1) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",bsObjGetStringPtr(objv[0]),NULL);
    return BS_ERROR;
  }
  if (!ctx_window) {
    bsClearResult(i);
    bsAppendResult(i,"cannot set '",bsObjGetStringPtr(objv[0]),
		   "' outside of 'window' context",NULL);
    return BS_ERROR;
  }
  ctx_window->async = (cdata ? 1 : 0);
  return BS_OK;
}

/*@bsdoc
procedure window...eye {
    usage {eye (mono|left|right|stereo)}
    desc {Specifies how the view from this window is to be drawn.}
    doc {<p><i>Valid in: window</i></p>
	If a window does not have an "eye" command associated with it,
	then it will be a "mono" window by default.
    }
    
}
*/
static int cmd_window_eye(BSInterp *i, int objc, BSObject *objv[],
			  void *cdata) {
  static char *usage = "eye (mono|left|right|stereo)";
  char *s;
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!ctx_window) {
    bsSetStringResult(i,"cannot set 'eye' outside of 'window' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  s = bsObjGetStringPtr(objv[1]);
  if (strcmp(s,"mono") == 0)
    ctx_window->eye = VE_WIN_MONO;
  else if (strcmp(s,"left") == 0)
    ctx_window->eye = VE_WIN_LEFT;
  else if (strcmp(s,"right") == 0)
    ctx_window->eye = VE_WIN_RIGHT;
  else if (strcmp(s,"stereo") == 0)
    ctx_window->eye = VE_WIN_STEREO;
  else {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  return BS_OK;
}

/*@bsdoc
procedure window {
    usage {window <name> <body>}
    desc {Describes a window.}
    doc {<p><i>Valid in: wall</i></p>}
    example {A simple window} {window eg {<br />
	&nbsp; &nbsp; display :0<br />
	&nbsp; &nbsp; geometry 640x480+0+0<br />
	&nbsp; &nbsp; err 0.0 0.0<br />
	&nbsp; &nbsp; offset 0.0 0.0<br />
	&nbsp; &nbsp; slave auto auto auto<br />
	&nbsp; &nbsp; viewport 0 0 640 480<br />
	&nbsp; &nbsp; distort [m4ident]<br />
	&nbsp; &nbsp; sync<br />
	&nbsp; &nbsp; eye mono<br />
	&nbsp; &nbsp; option noborder 1<br />
    }}
}
*/
static int cmd_window(BSInterp *i, int objc, BSObject *objv[],
		      void *cdata) {
  static char *usage = "window <name> { <body> }";
  int code;
  VeOption **save_optlist;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  if (!ctx_wall) {
    bsSetStringResult(i,"window command is only valid within a 'wall' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }

  if (ctx_window) {
    bsSetStringResult(i,"cannot nest window commands",BS_S_STATIC);
    return BS_ERROR;
  }

  ctx_window = veAllocObj(VeWindow);
  ctx_window->name = veDupString(bsObjGetStringPtr(objv[1]));
  ctx_window->id = veWindowMakeID();
  veMatrixIdentity(&(ctx_window->distort));
  ctx_window->viewport[0] =
    ctx_window->viewport[1] =
    ctx_window->viewport[2] =
    ctx_window->viewport[3] = 0.0;

  /* evaluate the body */
  
  /* context push */
  save_optlist = ctx_optlist;
  ctx_optlist = &(ctx_window->options);
  bsInterpNest(i);
  /* load procedures */
  bsSetExtCProc(i,NULL,"sync",cmd_window_sync,NULL);
  bsSetExtCProc(i,NULL,"async",cmd_window_sync,(void *)1);
  bsSetExtCProc(i,NULL,"eye",cmd_window_eye,NULL);
  bsSetExtCProc(i,NULL,"display",cmd_window_string,NULL);
  bsSetExtCProc(i,NULL,"geometry",cmd_window_string,NULL);
  bsSetExtCProc(i,NULL,"offset",cmd_window_float2,NULL);
  bsSetExtCProc(i,NULL,"err",cmd_window_float2,NULL);
  bsSetExtCProc(i,NULL,"slave",cmd_window_slave,NULL);
  bsSetExtCProc(i,NULL,"viewport",cmd_window_viewport,NULL);
  bsSetExtCProc(i,NULL,"distort",cmd_window_distort,NULL);
  bsSetExtCProc(i,NULL,"option",cmd_option,NULL);

  code = bsEval(i,objv[2]);

  /* pop context */
  bsInterpUnnest(i);
  ctx_optlist = save_optlist;

  code = check_result(i,code,"window");

  if (code != BS_OK) {
    veWindowFreeList(ctx_window);
  } else {
    /* save new window */
    ctx_window->wall = ctx_wall;
    ctx_window->next = ctx_wall->windows;
    ctx_wall->windows = ctx_window;
  }

  ctx_window = NULL; /* remove context */

  return code;
}

/*@bsdoc
procedure wall...size {
    usage {size <width> <height>}
    desc {Specifies the size of a wall in physical units.}
    doc {<p><i>Valid in: env</i></p>}
}
*/
static int cmd_wall_size(BSInterp *i, int objc, BSObject *objv[],
			 void *cdata) {
  static char *usage = "size <width> <height>";
  BSFloat w,h;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (bsObjGetFloat(i,objv[1],&w) != BS_OK ||
      bsObjGetFloat(i,objv[2],&h) != BS_OK)
    return BS_ERROR;
  if (!ctx_wall) {
    bsSetStringResult(i,"cannot set 'size' outside of 'wall' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  ctx_wall->view->width = w;
  ctx_wall->view->height = h;
  return BS_OK;
}

/*@bsdoc
procedure wall...base {
    usage {base (origin|eye)}
    desc {Specifies the space in which the wall is described.}
    doc {<p><i>Valid in: env</i></p>
	Walls are defined either to the origin of the physical space
	or they are defined relative to the eye.  For example, a fixed
	screen is defined relative to the origin.  A head-mounted
	display would be defined relative to the eye.}
}
*/
static int cmd_wall_base(BSInterp *i, int objc, BSObject *objv[],
			 void *cdata) {
  static char *usage = "base (origin|eye)";
  char *s;
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!ctx_wall) {
    bsSetStringResult(i,"cannot set 'base' outside of 'wall' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  s = bsObjGetStringPtr(objv[1]);
  if (strcmp(s,"origin") == 0)
    ctx_wall->view->base = VE_WALL_ORIGIN;
  else if (strcmp(s,"eye") == 0)
    ctx_wall->view->base = VE_WALL_EYE;
  else {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  return BS_OK;
}

/*@bsdoc
procedure wall...loc {
    usage {loc <x> <y> <z>}
    desc {Specifies the centre of the wall.}
    doc {<p><i>Valid in: wall</i></p>}
}
procedure wall...dir {
    usage {dir <x> <y> <z>}
    desc {Specifies the direction in which the wall faces.}
    doc {<p><i>Valid in: wall</i></p>}
}
procedure wall...up {
    usage {up <x> <y> <z>}
    desc {Specifies the orientation of the wall by providing a vector
	in the "up" direction.}
    doc {<p><i>Valid in: wall</i></p>}
}
*/
/* set a 3-dim vector (float) in a wall */
static int cmd_wall_vector(BSInterp *i, int objc, BSObject *objv[],
			   void *cdata) {
  BSFloat v[3];
  char *prop;
  int k;

  prop = bsObjGetStringPtr(objv[0]);

  if (objc != 4 && objc != 2) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",prop," <x> <y> <z>  -or-  ",prop," <v>",NULL);
    return BS_ERROR;
  }

  if (!ctx_wall) {
    bsClearResult(i);
    bsAppendResult(i,"cannot set '",prop,"' outside of wall context",NULL);
    return BS_ERROR;
  }

  if (ctx_window) {
    bsClearResult(i);
    bsAppendResult(i,"cannot set '",prop,"' inside of window context",NULL);
    return BS_ERROR;
  }
  

  if (objc == 2) {
    VeVector3 *vv;
    if (!(vv = get_vector3(i,objv[1])))
      return BS_ERROR;
    v[0] = vv->data[0];
    v[1] = vv->data[1];
    v[2] = vv->data[2];
  } else {
    for (k = 0; k < 3; k++) {
      if (bsObjGetFloat(i,objv[k+1],&(v[k])) != BS_OK)
	return BS_ERROR;
    }
  }

  if (strcmp(prop,"loc") == 0) {
    for (k = 0; k < 3; k++)
      ctx_wall->view->frame.loc.data[k] = v[k];
  } else if (strcmp(prop,"dir") == 0) {
    for (k = 0; k < 3; k++)
      ctx_wall->view->frame.dir.data[k] = v[k];
  } else if (strcmp(prop,"up") == 0) {
    for (k = 0; k < 3; k++)
      ctx_wall->view->frame.up.data[k] = v[k];
  } else {
    bsClearResult(i);
    bsAppendResult(i,"unknown vector property '",prop,"'",NULL);
    return BS_ERROR;
  }
  
  return BS_OK;
}

/*@bsdoc
procedure wall...desc {
    usage {desc <string>}
    desc {Specifies a human-readable description of this wall.}
    doc {<p><i>Valid in: wall</i></p>}
}
*/
static int cmd_wall_desc(BSInterp *i, int objc, BSObject *objv[],
			void *cdata) {
  static char *usage = "desc <desc>";

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  
  if (!ctx_wall) {
    bsSetStringResult(i,"desc command is only valid within an 'wall' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }

  veFree(ctx_wall->desc);
  ctx_wall->desc = veDupString(bsObjGetStringPtr(objv[1]));
  return BS_OK;
}

/*@bsdoc
procedure wall {
    usage {wall <name> <body>}
    desc {Describes a wall in the environment.}
    doc {<p><i>Valid in: env</i></p>}
    example {A simple wall} {wall {<br />
	&nbsp; &nbsp; desc "A simple wall"<br />
	&nbsp; &nbsp; size 1.0 1.0<br />
	&nbsp; &nbsp; base origin<br />
	&nbsp; &nbsp; loc 0.0 0.0 -3.0<br />
	&nbsp; &nbsp; dir 0.0 0.0 1.0<br />
	&nbsp; &nbsp; up 0.0 1.0 0.0<br />
    }}
}
*/
static int cmd_wall(BSInterp *i, int objc, BSObject *objv[],
		    void *cdata) {
  static char *usage = "wall <name> { <body> }";
  int code;
  VeOption **save_optlist;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  if (!ctx_env) {
    bsSetStringResult(i,"wall command is only valid within an 'env' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }

  if (ctx_wall) {
    bsSetStringResult(i,"cannot nest wall commands",BS_S_STATIC);
    return BS_ERROR;
  }

  ctx_wall = veAllocObj(VeWall);
  ctx_wall->name = veDupString(bsObjGetStringPtr(objv[1]));
  ctx_wall->view = veAllocObj(VeView);

  /* context push */
  save_optlist = ctx_optlist;
  ctx_optlist = &(ctx_wall->options);
  bsInterpNest(i);
  bsSetExtCProc(i,NULL,"window",cmd_window,NULL);
  bsSetExtCProc(i,NULL,"base",cmd_wall_base,NULL);
  bsSetExtCProc(i,NULL,"loc",cmd_wall_vector,NULL);
  bsSetExtCProc(i,NULL,"dir",cmd_wall_vector,NULL);
  bsSetExtCProc(i,NULL,"up",cmd_wall_vector,NULL);
  bsSetExtCProc(i,NULL,"desc",cmd_wall_desc,NULL);
  bsSetExtCProc(i,NULL,"option",cmd_option,NULL);
  bsSetExtCProc(i,NULL,"size",cmd_wall_size,NULL);

  code = bsEval(i,objv[2]);

  /* pop context */
  bsInterpUnnest(i);
  ctx_optlist = save_optlist;

  code = check_result(i,code,"wall");

  if (code != BS_OK) {
    veWallFreeList(ctx_wall);
  } else {
    ctx_wall->env = ctx_env;
    ctx_wall->next = ctx_env->walls;
    ctx_env->walls = ctx_wall;
    ctx_env->nwalls++;
  }

  ctx_wall = NULL; /* remove context */
  
  return code;
}

/*@bsdoc
procedure env...desc {
    usage {desc <string>}
    desc {Specifies a human-readable description of this environment.}
    doc {<p><i>Valid in: env</i></p>}
}
*/
static int cmd_env_desc(BSInterp *i, int objc, BSObject *objv[],
			void *cdata) {
  static char *usage = "desc <desc>";

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  
  if (!ctx_env) {
    bsSetStringResult(i,"desc command is only valid within an 'env' context",
		      BS_S_STATIC);
    return BS_ERROR;
  }

  veFree(ctx_env->desc);
  ctx_env->desc = veDupString(bsObjGetStringPtr(objv[1]));
  return BS_OK;
}

static VeAudioChannel *ctx_audiochan = NULL;
static VeAudioOutput *ctx_audioout = NULL;

/* audio environment notes... 

   audiodevice <name> <driver> {
       <args> ...
   }

   audio default {
       output {
           device <devicename>.<outputname>
	   loc ...
	   dir ...
	   up ...
	   option ...
       }
       ...
   }

*/

/*@bsdoc
procedure output...loc {
    usage {loc <x> <y> <z>}
    desc {Specifies the centre of the audio output.}
    doc {<p><i>Valid in: output</i></p>}
}
procedure output...dir {
    usage {dir <x> <y> <z>}
    desc {Specifies the direction in which the audio output faces.}
    doc {<p><i>Valid in: output</i></p>}
}
procedure output...up {
    usage {up <x> <y> <z>}
    desc {Specifies the orientation of the audio output by providing a
	vector in the "up" direction.}
    doc {<p><i>Valid in: output</i></p>}
}
*/
static int cmd_audio_vector(BSInterp *i, int objc, BSObject *objv[],
			    void *cdata) {
  BSFloat v[3];
  char *prop;
  int k;

  prop = bsObjGetStringPtr(objv[0]);

  if (objc != 4 && objc != 2) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",prop," <x> <y> <z>  -or-  ",prop," <v>",NULL);
    return BS_ERROR;
  }

  if (!ctx_audioout) {
    bsClearResult(i);
    bsAppendResult(i,"cannot set '",prop,"' outside of 'output' context",NULL);
    return BS_ERROR;
  }

  if (objc == 2) {
    VeVector3 *vv;
    if (!(vv = get_vector3(i,objv[1])))
      return BS_ERROR;
    v[0] = vv->data[0];
    v[1] = vv->data[1];
    v[2] = vv->data[2];
  } else {
    for (k = 0; k < 3; k++) {
      if (bsObjGetFloat(i,objv[k+1],&(v[k])) != BS_OK)
	return BS_ERROR;
    }
  }

  if (strcmp(prop,"loc") == 0) {
    for (k = 0; k < 3; k++)
      ctx_audioout->frame.loc.data[k] = v[k];
  } else if (strcmp(prop,"dir") == 0) {
    for (k = 0; k < 3; k++)
      ctx_audioout->frame.dir.data[k] = v[k];
  } else if (strcmp(prop,"up") == 0) {
    for (k = 0; k < 3; k++)
      ctx_audioout->frame.up.data[k] = v[k];
  } else {
    bsClearResult(i);
    bsAppendResult(i,"unknown vector property '",prop,"'",NULL);
    return BS_ERROR;
  }
  
  return BS_OK;
}

/*@bsdoc
procedure output...device {
    usage {device <name> [<output>]}
    desc {Specifies the audio device to use for this output.}
    doc {<p><i>Valid in: output</i></p>}
}
*/
static int cmd_output_device(BSInterp *i, int objc, BSObject *objv[],
			     void *cdata) {
  static char *usage = "usage: device <name> [<output>]";
  VeAudioDevice *dev;
  int sub = -1;

  assert(ctx_audioout != NULL);

  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  if (!(dev = veAudioDevFind(bsObjGetStringPtr(objv[1])))) {
    bsClearResult(i);
    bsAppendResult(i,"no such audio device: ",
		   bsObjGetStringPtr(objv[1]),NULL);
    return BS_ERROR;
  }

  if (objc >= 3) {
    if ((sub = veAudioDevGetSub(dev,bsObjGetStringPtr(objv[2]))) < 0) {
      bsClearResult(i);
      bsAppendResult(i,"no such output identifier for device ",
		     bsObjGetStringPtr(objv[1]),": ",
		     bsObjGetStringPtr(objv[2]),NULL);
      return BS_ERROR;
    }
  }

  ctx_audioout->device = dev;
  ctx_audioout->devout_id = sub;
  return BS_OK;
}

/*@bsdoc
procedure output {
    usage {output <name> <body>}
    desc {Specifies an output of an audio channel.}
    doc {<p><i>Valid in: audio</i></p>}
    example {A simple output} {output eg {<br />
	&nbsp; &nbsp; device default 0<br />
	&nbsp; &nbsp; loc 0.0 0.0 -3.0<br />
	&nbsp; &nbsp; dir 0.0 0.0 1.0<br />
	&nbsp; &nbsp; up 0.0 1.0 0.0<br />
    }}
}
*/
static int cmd_audio_output(BSInterp *i, int objc, BSObject *objv[],
			    void *cdata) {
  static char *usage = "usage: output <name> { <body> }";
  int code;
  VeOption **save_optlist;

  assert(ctx_audiochan != NULL);

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  ctx_audioout = veAllocObj(VeAudioOutput);
  ctx_audioout->name = veDupString(bsObjGetStringPtr(objv[1]));
  ctx_audioout->devout_id = -1;
  veFrameIdentity(&ctx_audioout->frame);
  
  save_optlist = ctx_optlist;
  ctx_optlist = &(ctx_audioout->options);
  
  bsInterpNest(i);
  bsSetExtCProc(i,NULL,"loc",cmd_audio_vector,NULL);
  bsSetExtCProc(i,NULL,"dir",cmd_audio_vector,NULL);
  bsSetExtCProc(i,NULL,"up",cmd_audio_vector,NULL);
  bsSetExtCProc(i,NULL,"device",cmd_output_device,NULL);
  code = bsEval(i,objv[2]);
  bsInterpUnnest(i);

  ctx_optlist = save_optlist;

  code = check_result(i,code,"output");

  if (code != BS_OK)
    veAudioOutputFree(ctx_audioout);
  else {
    ctx_audioout->next = ctx_audiochan->outputs;
    ctx_audiochan->outputs = ctx_audioout;
  }
  ctx_audioout = NULL;
  return code;
}

/*@bsdoc
procedure audio...engine {
    usage {engine <driver> <body>}
    desc {Specifies an audio engine to use to render a particular
	audio channel.}
    doc {<p><i>Valid in: audio</i></p>}
}
*/
static int cmd_audio_engine(BSInterp *i, int objc, BSObject *objv[],
			    void *cdata) {
  static char *usage = "usage: engine <name> { <body> }";
  VeAudioEngine *e;
  VeOption *opts = NULL;

  assert(ctx_audiochan != NULL);

  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  
  if (!(e = veAudioEngineFind(bsObjGetStringPtr(objv[1])))) {
    bsClearResult(i);
    bsAppendResult(i,"no such engine: ",bsObjGetStringPtr(objv[1]),NULL);
    return BS_ERROR;
  }
  if (objc > 2)
    opts = get_option_body(i,objv[2]);

  ctx_audiochan->engine = e;

  veOptionFreeList(opts);
  return BS_OK;
}

/*@bsdoc
procedure audio {
    usage {audio <name> <body>}
    desc {Specifies an audio channel.}
    example {A simple audio channel} {audio default {<br />
	&nbsp; &nbsp; engine mix {}<br />
	&nbsp; &nbsp; output o0 {<br />
	    &nbsp; &nbsp; &nbsp; &nbsp; device default 0<br />
	    &nbsp; &nbsp; }<br />
	&nbsp; &nbsp; output o1 {<br />
	    &nbsp; &nbsp; &nbsp; &nbsp; device default 1<br />
	    &nbsp; &nbsp; }<br />
    }}
}
*/
static int cmd_audio(BSInterp *i, int objc, BSObject *objv[],
			 void *cdata) {
  static char *usage = "usage: audio <name> { <body> }";
  int code;
  VeOption **save_optlist;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  ctx_audiochan = veAllocObj(VeAudioChannel);
  ctx_audiochan->name = veDupString(bsObjGetStringPtr(objv[1]));

  save_optlist = ctx_optlist;
  ctx_optlist = &(ctx_audiochan->options);

  bsInterpNest(i);
  bsSetExtCProc(i,NULL,"option",cmd_option,NULL);
  bsSetExtCProc(i,NULL,"output",cmd_audio_output,NULL);
  bsSetExtCProc(i,NULL,"engine",cmd_audio_engine,NULL);
  code = bsEval(i,objv[2]);
  bsInterpUnnest(i);

  ctx_optlist = save_optlist;

  code = check_result(i,code,"audio");
  if (code != BS_OK)
    veAudioChannelFree(ctx_audiochan);
  else
    veAudioChannelReg(ctx_audiochan);
  return code;
}

/*@bsdoc
procedure audiodevice {
    usage {audiodevice <name> <driver> [<options>]}
    desc {Specifies a platform audio device to initialize and use.}
}
*/
static int cmd_audiodevice(BSInterp *i, int objc, BSObject *objv[],
			   void *cdata) {
  static char *usage = "audiodevice <name> <driver> [{ <body> }]";
  int code;
  char *name;
  VeAudioDevice *dev;
  VeOption *opts = NULL;

  if (objc != 3 && objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  dev = veAudioDevCreate((name = bsObjGetStringPtr(objv[1])),
			 bsObjGetStringPtr(objv[2]),
			 (objc >= 4 ? 
			  (opts = get_option_body(i,objv[3])) : NULL));
  if (!dev) {
    char *x;
    if ((x = veEnvGetOption(opts,"optional")) && atoi(x))
      /* just warn */
      veWarning(MODULE,"failed to initialize audio device '%s'",name);
    else
      /* die die die */
      veFatalError(MODULE,"failed to initialize audio device '%s'",name);
  }

  return BS_OK;
}

/*@bsdoc
procedure env {
    usage {env <name> <body>}
    desc {Defines the environment.}
}
*/
static int cmd_env(BSInterp *i, int objc, BSObject *objv[],
		   void *cdata) {
  static char *usage = "env <name> { <body> }";
  int code;
  VeOption **save_optlist;
  if (ctx_env) {
    bsSetStringResult(i,"cannot have multiple 'env' commands",BS_S_STATIC);
    return BS_ERROR;
  }
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  ctx_env = veAllocObj(VeEnv);
  ctx_env->name = veDupString(bsObjGetStringPtr(objv[1]));

  /* context push */
  save_optlist = ctx_optlist;
  ctx_optlist = &(ctx_env->options);
  bsInterpNest(i);
  bsSetExtCProc(i,NULL,"desc",cmd_env_desc,NULL);
  bsSetExtCProc(i,NULL,"wall",cmd_wall,NULL);
  bsSetExtCProc(i,NULL,"option",cmd_option,NULL);

  code = bsEval(i,objv[2]);

  /* context pop */
  bsInterpUnnest(i);
  ctx_optlist = save_optlist;

  code = check_result(i,code,"env");

  if (code != BS_OK) {
    veEnvFree(ctx_env);
  } else {
    veSetEnv(ctx_env);
  }
  ctx_env = NULL;
  return code;
}

/* profile */
static VeProfile *ctx_profile = NULL;
static VeProfileModule *ctx_module = NULL;

static int module_handler(BSInterp *i, int objc, BSObject *objv[],
			  void *cdata) {
  VeProfileDatum *d;
  if (objc != 2) {
    bsSetStringResult(i,"module options should be set as '<name> <value>'",
		      BS_S_STATIC);
    return BS_ERROR;
  }
  assert(ctx_module != NULL);
  d = veAllocObj(VeProfileDatum);
  d->name = veDupString(bsObjGetStringPtr(objv[0]));
  d->value = veDupString(bsObjGetStringPtr(objv[1]));
  d->next = ctx_module->data;
  ctx_module->data = d;
  return BS_OK;
}

/*@bsdoc
procedure profile...module {
    usage {module <name> <options>}
    desc {Specifies a module of options for the user profile.}
    doc {<p><i>Valid in: profile</i></p>
	A module is simply a way of grouping user profile options.  
	The anticipated model is that if a particular program requires
	user-specific settings, it defines a new module and searches
	in that module for the settings it needs.
	The options contained in a module are specific to whatever
	uses that module - there are no pre-defined names or values.
    }
    example {A simple profile module} {
	module eg {<br />
	    &nbsp; &nbsp; file /tmp/eg<br />
	    &nbsp; &nbsp; config 0 11 32<br />
	}
    }
}
*/

static int cmd_profile_module(BSInterp *i, int objc, BSObject *objv[],
			      void *cdata) {
  static char *usage = "usage: module <name> { <body> }";
  int code;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  assert(ctx_profile != NULL);

  ctx_module = veAllocObj(VeProfileModule);
  ctx_module->name = veDupString(bsObjGetStringPtr(objv[1]));
  ctx_module->next = ctx_profile->modules;
  ctx_profile->modules = ctx_module;
  
  bsInterpNest(i);
  bsSetExtUnknown(i,NULL,module_handler,NULL);
  
  code = bsEval(i,objv[2]);
  
  bsInterpUnnest(i);
  
  code = check_result(i,code,"module");
  ctx_module = NULL;
  return code;
}

/*@bsdoc
procedure profile...eyedist {
    usage {eyedist <d>}
    desc {
	Specifies the eye separation in physical units.
    }
    doc {<p><i>Valid in: profile</i></p>}
}
*/
static int cmd_profile_eyedist(BSInterp *i, int objc, BSObject *objv[],
			       void *cdata) {
  static char *usage = "usage: eyedist <d>";
  BSFloat f;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  assert(ctx_profile != NULL);
  if (bsObjGetFloat(i,objv[1],&f))
    return BS_ERROR;
  ctx_profile->stereo_eyedist = f;
  return BS_OK;
}

/*@bsdoc
procedure profile...fullname {
    usage {fullname <string>}
    desc {Specifies the user's full name.}
    doc {<p><i>Valid in: profile</i></p>}
}
*/
static int cmd_profile_fullname(BSInterp *i, int objc, BSObject *objv[],
				void *cdata) {
  static char *usage = "usage: fullname <name>";
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  assert(ctx_profile != NULL);
  veFree(ctx_profile->fullname);
  ctx_profile->fullname = veDupString(bsObjGetStringPtr(objv[1]));
  return BS_OK;
}

/*@bsdoc
procedure profile {
    usage {profile <name> <body>}
    desc {Specifies the user profile.}
    example {A simple profile} {
	profile joe {<br />
	    &nbsp; &nbsp; fullname "Joe Blough"<br />
	    &nbsp; &nbsp; eyedist 0.04<br />
	    &nbsp; &nbsp; module experiment1 {<br />
		&nbsp; &nbsp; &nbsp; &nbsp; correction 0.0 1.0 -2.04<br />
	    &nbsp; &nbsp; }<br />
	}
    }
}
*/
static int cmd_profile(BSInterp *i, int objc, BSObject *objv[],
		       void *cdata) {
  static char *usage = "profile <name> { <body> }";
  int code;

  if (ctx_profile) {
    bsSetStringResult(i,"cannot have multiple 'profile' commands",BS_S_STATIC);
    return BS_ERROR;
  }
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  ctx_profile = veAllocObj(VeProfile);
  ctx_profile->name = veDupString(bsObjGetStringPtr(objv[1]));

  /* context push */
  bsInterpNest(i);
  bsSetExtCProc(i,NULL,"module",cmd_profile_module,NULL);
  bsSetExtCProc(i,NULL,"eyedist",cmd_profile_eyedist,NULL);
  bsSetExtCProc(i,NULL,"fullname",cmd_profile_fullname,NULL);

  code = bsEval(i,objv[2]);

  /* context pop */
  bsInterpUnnest(i);

  code = check_result(i,code,"profile");

  if (code != BS_OK)
    veProfileFree(ctx_profile);
  else
    veSetProfile(ctx_profile);
  return code;
}

static int cmd_optset(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  VeStrMap m = (VeStrMap)cdata;
  char *name, *value;
  if (!m)
    veFatalError(MODULE,"setup to parse an optset without a map to store it in");
  
  if (objc < 1 || objc > 2) {
    bsSetStringResult(i,"options must specified as '<name> <value>'",BS_S_STATIC);
    return BS_ERROR;
  }
  name = bsObjGetStringPtr(objv[0]);
  value = (objv[1] ? bsObjGetStringPtr(objv[1]) : "");
  veFree(veStrMapLookup(m,name)); /* free old value (if any) */
  veStrMapInsert(m,name,veDupString(value));
  return BS_OK;
}

/*@bsdoc
package veinput {
    longname {VE Input}
    desc {Procedures for creating input devices and manipulating input
	events.}
}
*/

/* devices */
/*@bsdoc
procedure use {
    usage {use <name> [[<type>] <options>]}
}

procedure device {
    usage {device <name> <type> [<options>]}
}
*/
static int cmd_use(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *use_usage = "usage: use <name> [[<type>] { <options> }]";
  static char *device_usage = "usage: device <name> <type> [{ <options> }]";
  char *name = NULL;
  char *type = NULL;
  VeStrMap values = NULL;
  int code;
  int use_mode = (cdata ? 1 : 0);

  bsClearResult(i);
  if (!veMPIsMaster())
    return BS_OK;

  if (use_mode) {
    if (objc < 2 || objc > 4) {
      bsSetStringResult(i,use_usage,BS_S_STATIC);
      return BS_ERROR;
    }
    if (objc > 2) {
      /* parse options - always last element */
      values = veStrMapCreate();
      bsInterpNest(i);
      bsSetExtUnknown(i,NULL,cmd_optset,(void *)values);
      code = bsEval(i,objv[objc-1]);
      bsInterpUnnest(i);
      if (code != BS_OK) {
	veStrMapDestroy(values,veFree);
	if (bsIsResultClear(i))
	  bsAppendResult(i,"unexpected result code from option set: ",
			 bsCodeToString(code),NULL);
	return code;
      }
    }
    if (objc > 3)
      type = bsObjGetStringPtr(objv[2]);
  } else { /* device mode */
    if (objc < 3 || objc > 4) {
      bsSetStringResult(i,device_usage,BS_S_STATIC);
      return BS_ERROR;
    }
    type = bsObjGetStringPtr(objv[2]);
    if (objc > 3) {
      /* parse options - always last element */
      values = veStrMapCreate();
      bsInterpNest(i);
      bsSetExtUnknown(i,NULL,cmd_optset,(void *)values);
      code = bsEval(i,objv[objc-1]);
      bsInterpUnnest(i);
      if (code != BS_OK) {
	veStrMapDestroy(values,veFree);
	if (bsIsResultClear(i))
	  bsAppendResult(i,"unexpected result code from option set: ",
			 bsCodeToString(code),NULL);
	return code;
      }
    }
  }
  
  name = bsObjGetStringPtr(objv[1]);

  if (type) {
    /* declare it */
    VeDeviceDesc *d;
    d = veAllocObj(VeDeviceDesc);
    d->name = veDupString(name);
    d->type = veDupString(type);
    d->options = values;
    veAddDeviceDesc(d);
    if (use_mode) {
      if (!veDeviceUse(name,NULL)) {
	if (veDeviceIsOptional(name,NULL)) {
	  veWarning(MODULE,"unable to use optional device %s",name);
	} else {
	  bsClearResult(i);
	  bsAppendResult(i,"failed to use device ",name,NULL);
	  return BS_ERROR;
	}
      }
    }
  } else {
    /* just use it */
    assert(use_mode == 1); /* otherwise this shouldn't be possible */
    if (!veDeviceUse(name,values)) {
      if (veDeviceIsOptional(name,values)) {
	veWarning(MODULE,"unable to use optional device %s",name);
      } else {
	bsClearResult(i);
	bsAppendResult(i,"failed to use device ",name,NULL);
	if (values)
	  veStrMapDestroy(values,veFree);
	return BS_ERROR;
      }
      if (values)
	veStrMapDestroy(values,veFree);
    }
  }
  return BS_OK;
}

/*@bsdoc
object VeDeviceEvent {
    doc {An input event.}
}
*/

/* event procs */
#define EV_TYPE           (0) 
#define EV_TIMESTAMP      (1)
#define EV_DEVICE         (2)
#define EV_ELEM           (3)
#define EV_INDEX          (4)
#define EV_STATE          (5)
#define EV_KEY            (6)
#define EV_MIN            (7)
#define EV_MAX            (8)
#define EV_VALUE          (9)
#define EV_VMIN          (10)
#define EV_VMAX          (11)
#define EV_VVALUE        (12)
#define EV_COPY          (13)
#define EV_PUSH          (14)
#define EV_RENAME        (15)
#define EV_DUMP          (16)

static struct {
  char *name;
  int id;
} event_proc_names[] = {
  { "type", EV_TYPE },
  { "timestamp", EV_TIMESTAMP },
  { "device", EV_DEVICE },
  { "elem", EV_ELEM },
  { "index", EV_INDEX },
  { "state", EV_STATE },
  { "key", EV_KEY },
  { "min", EV_MIN },
  { "max", EV_MAX },
  { "value", EV_VALUE },
  { "vmin", EV_VMIN },
  { "vmax", EV_VMAX },
  { "vvalue", EV_VVALUE },
  { "copy", EV_COPY },
  { "push", EV_PUSH },
  { "rename", EV_RENAME },  /* device.elem */
  { "dump", EV_DUMP },
  { NULL, -1 }
};

static struct {
  char *name;
  int id;
} event_type_names[] = {
  { "trigger", VE_ELEM_TRIGGER },
  { "switch", VE_ELEM_SWITCH },
  { "keyboard", VE_ELEM_KEYBOARD },
  { "valuator", VE_ELEM_VALUATOR },
  { "vector", VE_ELEM_VECTOR },
  { NULL, -1 }
};

static int event_destroy(BSObject *o);
static int event_proc(BSObject *o, BSInterp *i, int objc, BSObject **objv);

static BSOpaqueDriver event_driver = {
  "VeDeviceEvent",
  0,                         /* id */
  NULL,                 /* makerep */
  event_destroy,        /* destroy */
  event_proc,              /* proc */
};

static int event_destroy(BSObject *o) {
  BSOpaque *p;
  assert(o != NULL);
  assert(o->opaqueRep != NULL);
  assert(o->opaqueRep->driver == &event_driver);
  if (o->opaqueRep->data)
    veDeviceEventDestroy((VeDeviceEvent *)(o->opaqueRep->data));
  return 0;
}

static BSObject *make_event_object(VeDeviceEvent *e) {
  BSOpaque *p;

  p = veAllocObj(BSOpaque);
  if (event_driver.id == 0)
    event_driver.id = bsUniqueId();
  p->driver = &event_driver;
  p->data = (void *)e;
  return bsObjOpaque(p);
}

/*@bsdoc
procedure event {
    usage {event (trigger|switch|valuator|vector|keyboard)}
    desc {Creates a new event.}
}
*/
static int cmd_event(BSInterp *i, int objc, BSObject **objv, void *cdata) {
  /* create a new event */
  static char *usage = "usage: event (trigger|switch|valuator|vector|keyboard)";
  VeDeviceEvent *e;
  int type;
  int size = 0;
  char *s;
  BSObject *o;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  s = bsObjGetStringPtr(objv[1]);
  if (strcmp(s,"trigger") == 0)
    type = VE_ELEM_TRIGGER;
  else if (strcmp(s,"switch") == 0)
    type = VE_ELEM_SWITCH;
  else if (strcmp(s,"valuator") == 0)
    type = VE_ELEM_VALUATOR;
  else if (strcmp(s,"vector") == 0) {
    type = VE_ELEM_VECTOR;
    size = 1;
  } else if (strcmp(s,"keyboard") == 0)
    type == VE_ELEM_KEYBOARD;
  else {
    bsClearResult(i);
    bsAppendResult(i,"invalid event type: ",s,NULL);
    return BS_ERROR;
  }
  e = veDeviceEventCreate(type,size);
  o = make_event_object(e);
  bsSetResult(i,o);
  bsObjDelete(o);
  return BS_OK;
}

/*@bsdoc
object VeDeviceEvent {
    procedure type {
    }
    procedure timestamp {
    }
    procedure device {
    }
    procedure elem {
    }
    procedure index {
    }
    procedure state {
    }
    procedure key {
    }
    procedure min {
    }
    procedure max {
    }
    procedure value {
    }
    procedure vmin {
    }
    procedure vmax {
    }
    procedure vvalue {
    }
    procedure copy {
    }
    procedure push {
    }
    procedure rename {
    }
    procedure dump {
    }
}
*/
static int event_proc(BSObject *o, BSInterp *i, int objc, BSObject **objv) {
  VeDeviceEvent *e;
  BSOpaque *p;
  int k;

  assert(o != NULL);
  assert(o->repValid & BS_T_OPAQUE);
  p = o->opaqueRep;
  assert(p != NULL);
  assert(p->driver == &event_driver);

  e = (VeDeviceEvent *)(p->data);
  assert(e != NULL);

  if ((k = bsObjGetConstant(i,objv[0],event_proc_names,
			    sizeof(event_proc_names[0]),"event proc")) < 0)
    return BS_ERROR;
		       
  switch (event_proc_names[k].id) {
  case EV_TYPE:
  case EV_TIMESTAMP:
  case EV_DEVICE:
  case EV_ELEM:
  case EV_INDEX:
    /* standard - valid for all events */
    {
      BSInt v_int;
      char *v_str;
      int typeind, type;
      if (objc > 2) {
	bsClearResult(i);
	bsAppendResult(i,"usage: $e ",bsObjGetStringPtr(objv[0]),
		       " [<value>]",NULL);
	return BS_ERROR;
      } 

      if (objc == 2) {
	/* set value first */
	switch (event_proc_names[k].id) {
	case EV_TIMESTAMP:
	  if (bsObjGetInt(i,objv[1],&v_int))
	    return BS_ERROR;
	  e->timestamp = (long)v_int;
	  break;
	case EV_INDEX:
	  if (bsObjGetInt(i,objv[1],&v_int))
	    return BS_ERROR;
	  e->index = v_int;
	  break;
	case EV_DEVICE:
	  veFree(e->device);
	  e->device = veDupString(bsObjGetStringPtr(objv[1]));
	  break;
	case EV_ELEM:
	  veFree(e->elem);
	  e->elem = veDupString(bsObjGetStringPtr(objv[1]));
	  break;
	case EV_TYPE:
	  /* get type */
	  if ((typeind = bsObjGetConstant(i,objv[1],event_type_names,
					  sizeof(event_type_names[0]),
					  "event type")) < 0)
	    return BS_ERROR;
	  type = event_type_names[typeind].id;
	  if (type != veDeviceEventType(e)) {
	    /* change type - use standard methods */
	    switch (type) {
	    case VE_ELEM_TRIGGER:
	      {
		veDeviceEContentDestroy(e->content);
		e->content = veDeviceEContentCreate(VE_ELEM_TRIGGER,0);
	      }
	      break;
	    case VE_ELEM_SWITCH:
	      {
		int z;
		z = veDeviceToSwitch(e);
		veDeviceEContentDestroy(e->content);
		e->content = veDeviceEContentCreate(VE_ELEM_SWITCH,0);
		VE_EVENT_SWITCH(e)->state = z;
	      }
	      break;
	    case VE_ELEM_KEYBOARD:
	      {
		int z;
		z = veDeviceToSwitch(e);
		veDeviceEContentDestroy(e->content);
		e->content = veDeviceEContentCreate(VE_ELEM_KEYBOARD,0);
		VE_EVENT_KEYBOARD(e)->state = z;
		/* key is undefined... */
	      }
	      break;
	    case VE_ELEM_VALUATOR:
	      {
		float f;
		f = veDeviceToValuator(e);
		veDeviceEContentDestroy(e->content);
		e->content = veDeviceEContentCreate(VE_ELEM_VALUATOR,0);
		VE_EVENT_VALUATOR(e)->value = f;
	      }
	      break;
	    case VE_ELEM_VECTOR:
	      {
		float f;
		f = veDeviceToValuator(e);
		veDeviceEContentDestroy(e->content);
		/* size is unknown - vector ops will grow as necessary */
		e->content = veDeviceEContentCreate(VE_ELEM_VECTOR,1);
		VE_EVENT_VECTOR(e)->value[0] = f;
	      }
	      break;
	    default:
	      veFatalError(MODULE,"event_proc: unknown type for event set");
	    }
	  }
	}
      }

      /* now get the value */
      switch (event_proc_names[k].id) {
      case EV_TIMESTAMP:
	bsSetIntResult(i,e->timestamp);
	break;
      case EV_INDEX:
	bsSetIntResult(i,e->index);
	break;
      case EV_DEVICE:
	bsSetStringResult(i,e->device,BS_S_VOLATILE);
	break;
      case EV_ELEM:
	bsSetStringResult(i,e->elem,BS_S_VOLATILE);
	break;
      case EV_TYPE:
	{
	  int z, type = veDeviceEventType(e);
	  for (z = 0; event_type_names[z].name; z++)
	    if (event_type_names[z].id == type)
	      break;
	  if (!event_type_names[z].name)
	    veFatalError(MODULE,"event has invalid type");
	  bsSetStringResult(i,event_type_names[z].name,BS_S_STATIC);
	}
	break;
      default:
	veFatalError(MODULE,"misplaced proc handler (timestamp/etc.)");
      }
    }
    break;

  case EV_STATE:
    if (objc > 2) {
      bsSetStringResult(i,"usage: $e state [<value>]",BS_S_STATIC);
      return BS_ERROR;
    }
    if (objc == 2) {
      BSInt k;
      if (veDeviceEventType(e) != VE_ELEM_SWITCH && 
	  veDeviceEventType(e) != VE_ELEM_KEYBOARD) {
	bsSetStringResult(i,"state can only be set for switch "
			  "or keyboard events", BS_S_STATIC);
	return BS_ERROR;
      }
      if (bsObjGetInt(i,objv[1],&k))
	return BS_ERROR;
      if (veDeviceEventType(e) == VE_ELEM_SWITCH) {
	VE_EVENT_SWITCH(e)->state = (k ? 1 : 0);
      } else if (veDeviceEventType(e) == VE_ELEM_KEYBOARD) {
	VE_EVENT_KEYBOARD(e)->state = (k ? 1 : 0);
      }
    }
    bsSetIntResult(i,veDeviceToSwitch(e));
    break;

  case EV_KEY:
    if (objc > 2) {
      bsSetStringResult(i,"usage: $e key [<value>]",BS_S_STATIC);
      return BS_ERROR;
    }
    if (objc == 2) {
      if (veDeviceEventType(e) != VE_ELEM_KEYBOARD) {
	bsSetStringResult(i,"key can only be set for keyboard events",
			  BS_S_STATIC);
	return BS_ERROR;
      }
      VE_EVENT_KEYBOARD(e)->key = veStringToKeysym(bsObjGetStringPtr(objv[1]));
    }
    bsSetIntResult(i,(veDeviceEventType(e) == VE_ELEM_KEYBOARD ?
		      VE_EVENT_KEYBOARD(e)->key :
		      VEK_UNKNOWN));
    break;

  case EV_MIN:
  case EV_MAX:
    {
      BSFloat f;
      if (objc > 2) {
	bsClearResult(i);
	bsAppendResult(i,"usage: $e ",bsObjGetStringPtr(objv[0])," [<value>]",
		       NULL);
	return BS_ERROR;
      }
      if (objc == 2) {
	if (veDeviceEventType(e) != VE_ELEM_VALUATOR) {
	  bsClearResult(i);
	  bsAppendResult(i,bsObjGetStringPtr(objv[0]), 
			 " can only be set for valuator events",
			 BS_S_STATIC);
	  return BS_ERROR;
	}
	if (bsObjGetFloat(i,objv[1],&f))
	  return BS_ERROR;
	if (event_proc_names[k].id == EV_MIN)
	  VE_EVENT_VALUATOR(e)->min = f;
	else
	  VE_EVENT_VALUATOR(e)->max = f;
      }
      if (veDeviceEventType(e) != VE_ELEM_VALUATOR)
	bsSetFloatResult(i,0.0);
      else
	bsSetFloatResult(i,(event_proc_names[k].id == EV_MIN ?
			    VE_EVENT_VALUATOR(e)->min :
			    VE_EVENT_VALUATOR(e)->max));
    }
    break;

  case EV_VALUE:
    {
      BSFloat f;
      if (objc > 2) {
	bsSetStringResult(i,"usage: $e value [<value>]",BS_S_STATIC);
	return BS_ERROR;
      }
      if (objc == 2) {
	if (veDeviceEventType(e) != VE_ELEM_VALUATOR) {
	  bsSetStringResult(i,"value can only be set for valuator events",
			    BS_S_STATIC);
	  return BS_ERROR;
	}
	if (bsObjGetFloat(i,objv[1],&f))
	  return BS_ERROR;
	VE_EVENT_VALUATOR(e)->value = f;
      }
      switch (veDeviceEventType(e)) {
      case VE_ELEM_SWITCH:
	bsSetIntResult(i,VE_EVENT_SWITCH(e)->state);
	break;
      case VE_ELEM_KEYBOARD:
	bsSetIntResult(i,VE_EVENT_KEYBOARD(e)->state);
	break;
      case VE_ELEM_VALUATOR:
	bsSetFloatResult(i,VE_EVENT_VALUATOR(e)->value);
	break;
      default:
	bsSetIntResult(i,0);
	break;
      }
    }
    break;

  case EV_VMIN:
  case EV_VMAX:
  case EV_VVALUE:
    {
      BSInt x;
      if (objc > 3) {
	bsClearResult(i);
	bsAppendResult(i,"usage: $e ",bsObjGetStringPtr(objv[0])," <i> [<value>]",
		       NULL);
	return BS_ERROR;
      }
      if (veDeviceEventType(e) != VE_ELEM_VECTOR) {
	bsClearResult(i);
	bsAppendResult(i,bsObjGetStringPtr(objv[0]),
		       " is only valid for vector events",NULL);
	return BS_ERROR;
      }
      if (bsObjGetInt(i,objv[1],&x))
	return BS_ERROR;
      if (objc == 3) {
	BSFloat f;
	if (bsObjGetFloat(i,objv[2],&f))
	  return BS_ERROR;
	if (x < 0) {
	  bsClearResult(i);
	  bsAppendResult(i,"index for ",bsObjGetStringPtr(objv[0]),
			 " is invalid",NULL);
	  return BS_ERROR;
	}
	if (x >= VE_EVENT_VECTOR(e)->size) {
	  VeDeviceE_Vector *v;
	  int y;
	  v = (VeDeviceE_Vector *)veDeviceEContentCreate(VE_ELEM_VECTOR,
							 x+1);
	  for (y = 0; y < VE_EVENT_VECTOR(e)->size; y++) {
	    v->min[y] = VE_EVENT_VECTOR(e)->min[y];
	    v->max[y] = VE_EVENT_VECTOR(e)->max[y];
	    v->value[y] = VE_EVENT_VECTOR(e)->value[y];
	  }
	  while (y < x) {
	    v->min[y] = v->max[y] = v->value[y] = 0.0;
	    y++;
	  }
	  veDeviceEContentDestroy(e->content);
	  e->content = (VeDeviceEContent *)v;
	}
	switch (event_proc_names[k].id) {
	case EV_VMIN:
	  VE_EVENT_VECTOR(e)->min[x] = f;
	  break;
	case EV_VMAX:
	  VE_EVENT_VECTOR(e)->max[x] = f;
	  break;
	case EV_VVALUE:
	  VE_EVENT_VECTOR(e)->value[x] = f;
	  break;
	}
      }
      if (x < 0 || x >= VE_EVENT_VECTOR(e)->size)
	bsSetIntResult(i,0);
      else {
	switch (event_proc_names[k].id) {
	case EV_VMIN:
	  bsSetFloatResult(i,VE_EVENT_VECTOR(e)->min[x]);
	  break;
	case EV_VMAX:
	  bsSetFloatResult(i,VE_EVENT_VECTOR(e)->max[x]);
	  break;
	case EV_VVALUE:
	  bsSetFloatResult(i,VE_EVENT_VECTOR(e)->value[x]);
	  break;
	default:
	  bsSetIntResult(i,0);
	}
      }
    }
    break;

  case EV_COPY:
    {
      BSObject *o;
      if (objc != 1) {
	bsSetStringResult(i,"usage: $e copy",BS_S_STATIC);
	return BS_ERROR;
      }

      o = make_event_object(veDeviceEventCopy(e));
      bsSetResult(i,o);
      bsObjDelete(o);
    }
    break;

  case EV_PUSH:
    {
      int where = VE_QUEUE_TAIL;
      if (objc > 2) {
	bsSetStringResult(i,"usage: $e push [head|tail]",BS_S_STATIC);
	return BS_ERROR;
      }
      if (objc == 2) {
	char *s;
	s = bsObjGetStringPtr(objv[1]);
	if (strcmp(s,"head") == 0)
	  where = VE_QUEUE_HEAD;
	else if (strcmp(s,"tail") == 0)
	  where = VE_QUEUE_TAIL;
	else {
	  bsSetStringResult(i,"invalid push mode - should be 'head' | 'tail'",
			    BS_S_STATIC);
	  return BS_ERROR;
	}
      }
      veDevicePushEvent(veDeviceEventCopy(e),where,0);
    }
    break;

  case EV_RENAME:
    {
      char *s;

      if (objc != 2) {
	bsSetStringResult(i,"usage: $e rename device.elem",BS_S_STATIC);
	return BS_ERROR;
      }
      s = bsObjGetStringPtr(objv[1]);
      if (*s != '.' || strncmp(s,"*.",2)) {
	/* device name is specified... */
	char *c;
	veFree(e->device);
	e->device = veDupString(s);
	if ((s = strchr(e->device,'.'))) {
	  *s = '\0';
	  s++;
	}
      } else {
	/* device name is not specified */
	s = strchr(s,'.');
	assert(s != NULL); /* otherwise above test was bogus... */
	s++;
      }
      if (s && strcmp(s,"*")) {
	veFree(e->elem);
	e->elem = veDupString(s);
      }
    }
    break;

#define CKNULL(x) ((x)?(x):"<null>")

  case EV_DUMP:
    {
      if (objc != 1) {
	bsSetStringResult(i,"usage: $e dump",BS_S_STATIC);
	return BS_ERROR;
      }
      switch(e->content->type) {
      case VE_ELEM_TRIGGER:
	{
	  fprintf(stderr, "[%ld] %s.%s trigger\n",
		  e->timestamp, CKNULL(e->device), CKNULL(e->elem));
	}
	break;
      case VE_ELEM_SWITCH:
	{
	  VeDeviceE_Switch *sw = (VeDeviceE_Switch *)(e->content);
	  fprintf(stderr, "[%ld] %s.%s switch %d\n",
		  e->timestamp, CKNULL(e->device), CKNULL(e->elem),
		  sw->state);
	}
	break;
      case VE_ELEM_KEYBOARD:
	{
	  VeDeviceE_Keyboard *kb = (VeDeviceE_Keyboard *)(e->content);
	  fprintf(stderr, "[%ld] %s.%s keyboard %d %d\n",
		  e->timestamp, CKNULL(e->device), CKNULL(e->elem),
		  kb->key,kb->state);
	}
	break;
      case VE_ELEM_VALUATOR:
	{
	  VeDeviceE_Valuator *val = (VeDeviceE_Valuator *)(e->content);
	  fprintf(stderr, "[%ld] %s.%s valuator %g (%g:%g)\n",
		  e->timestamp, CKNULL(e->device), CKNULL(e->elem),
		  val->value, val->min, val->max);
	}
	break;
      case VE_ELEM_VECTOR:
	{
	  VeDeviceE_Vector *vec = (VeDeviceE_Vector *)(e->content);
	  int i;
	  fprintf(stderr, "[%ld] %s.%s vector %d",
		  e->timestamp, CKNULL(e->device), CKNULL(e->elem),
		  vec->size);
	  for(i = 0; i < vec->size; i++)
	    fprintf(stderr, " %g (%g:%g)", vec->value[i], vec->min[i],
		    vec->max[i]);
	  fprintf(stderr, "\n");
	}
	break;
      default:
	fprintf(stderr, "[%ld] unknown event (type = %d)\n",
		e->timestamp, e->content->type);
      }
    }
    break;

  default:
    veFatalError(MODULE,"recognized but unimplemented event proc");
  }
  /* if we get here, assume success */
  return BS_OK;
}

typedef struct ve_blue_filter_ctx {
  BSContext *ctx;
  BSObject *body;
} VeBlueFilterCtx;

static int filter_proc(VeDeviceEvent *e, void *arg) {
  int code;
  int fcode = VE_FILT_CONTINUE;

  VeBlueFilterCtx *c = (VeBlueFilterCtx *)arg;
  BSObject *o;

  veThrMutexLock(interp_mutex);

  o = make_event_object(e);
  
  bsSet(interp,c->ctx,"e",o);
  bsClearResult(interp);
  {
    /* cleaner way of doing this? */
    BSContext *save;
    save = interp->stack;
    interp->stack = c->ctx;
    code = bsEval(interp,c->body);
    interp->stack = save;
  }
  bsUnset(interp,c->ctx,"e");
  
  /* prevent event from being destroyed */
  o->opaqueRep->data = NULL;
  bsObjDelete(o);

  switch (code) {
  case BS_OK:
    fcode = VE_FILT_CONTINUE;
    break;

  case BS_BREAK:
    fcode = VE_FILT_DELIVER;
    break;
   
  case BS_CONTINUE:
    fcode = VE_FILT_RESTART;
    break;
    
  case BS_ERROR:
    veError(MODULE,"BlueScript error: %s",
	    bsObjGetStringPtr(bsGetResult(interp)));
    fcode = VE_FILT_ERROR;
    break;

  case BS_RETURN: /* check value */
    {
      char *s;
      s = bsObjGetStringPtr(bsGetResult(interp));
      if (strcmp(s,"continue") == 0)
	fcode = VE_FILT_CONTINUE;
      else if (strcmp(s,"deliver") == 0)
	fcode = VE_FILT_DELIVER;
      else if (strcmp(s,"discard") == 0)
	fcode = VE_FILT_DISCARD;
      else if (strcmp(s,"restart") == 0)
	fcode = VE_FILT_RESTART;
      else {
	veError(MODULE,"Unexpected BlueScript result: '%s'",s);
	fcode = VE_FILT_ERROR;
      }
    }
    break;

  default:
    veError(MODULE,"Unexpected BlueScript code: %d (%s)",
	    code,bsCodeToString(code));
    fcode = VE_FILT_ERROR;
  }

  veThrMutexUnlock(interp_mutex);
  return fcode;
}

/*@bsdoc 
procedure filter {
    usage {filter <devspec> <body>}
}
*/

static int cmd_filter(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: filter <devspec> { <body> }";
  VeDeviceSpec *spec;
  VeBlueFilterCtx *c;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  /* add a filter for callback... */
  if (!(spec = veDeviceParseSpec(bsObjGetStringPtr(objv[1])))) {
    bsSetStringResult(i,"invalid device spec for filter",BS_S_STATIC);
    return BS_ERROR;
  }
  
  c = veAllocObj(VeBlueFilterCtx);
  c->ctx = bsContextPush(NULL); /* create a context for us */
  c->body = bsObjCopy(objv[2]);

  veDeviceFilterAdd(spec,
		    veDeviceFilterCreate(filter_proc,c),
		    VE_FTABLE_TAIL);

  return BS_OK;
}

/*@bsdoc
package vemisc {
    longname {VE Miscellaneous}
    desc {Various VE support procedures.}
}
*/

/* include another configuration file */
/*@bsdoc
procedure include {
    usage {include <filename>}
}
*/
static int cmd_include(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: include <filename>";
  FILE *f;
  int code;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  if (!(f = veFindFile(NULL,bsObjGetStringPtr(objv[1])))) {
    bsClearResult(i);
    bsAppendResult(i,"cannot open configuration file ",
		   bsObjGetStringPtr(objv[1]),NULL);
    return BS_ERROR;
  }

  code = bsEvalStream(i,f);
  fclose(f);
  if (code == BS_ERROR) {
    if (bsIsResultClear(i))
      bsAppendResult(i,"error processing file ",
		     bsObjGetStringPtr(objv[1]),NULL);
    else
      bsAppendResult(i,"\nin file '",bsObjGetStringPtr(objv[1]),
		     "'",NULL);
  }
  return code;
}

/* access functions */
#define LOC 0
#define DIR 1
#define UP  2

#define ORIGIN 0
#define EYE    1
/*@bsdoc
procedure frame_origin {
    usage {frame_origin (loc|dir|up) [<v>]}
    desc {Sets and/or queries a component of the origin.}
}

procedure frame_eye {
    usage {frame_eye (loc|dir|up) [<v>]}
    desc {Sets and/or queries a component of the default eye.}
}
*/
static int cmd_frame(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage[2] = {
    "usage: frame_origin (loc|dir|up) [<v>]",
    "usage: frame_eye (loc|dir|up) [<v>]"
  };
  VeFrame *f;
  VeVector3 *fv, *v;
  char *s;

  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,usage[cdata ? 1 : 0],BS_S_STATIC);
    return BS_ERROR;
  }

  switch ((int)cdata) {
  case ORIGIN:
    f = veGetOrigin();
    break;
  case EYE:
    f = veGetDefaultEye();
    break;
  default:
    veFatalError(MODULE,"cmd_frame: invalid frame type (internal error)");
  }
  
  s = bsObjGetStringPtr(objv[1]);
  if (strcmp(s,"loc") == 0)
    fv = &(f->loc);
  else if (strcmp(s,"dir") == 0)
    fv = &(f->dir);
  else if (strcmp(s,"up") == 0)
    fv = &(f->up);
  else {
    bsClearResult(i);
    bsAppendResult(i,"invalid frame member - ", s,
		   " - must be loc, dir or up", NULL);
    return BS_ERROR;
  }

  if (objc == 3) {
    /* set value */
    if (!(v = get_vector3(i,objv[2]))) {
      return BS_ERROR;
    }
    *fv = *v;
    vePostRedisplay();
  }

  set_vector3(bsGetResult(i),fv);
  return BS_OK;
}

/*@bsdoc
procedure exit {
    usage {exit [<msg>]}
    desc {Exits the VE program.}
}
*/
static int cmd_exit(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  if (objc > 1) {
    fprintf(stderr, "BlueScript exit: %s\n",
	    bsObjGetStringPtr(objv[1]));
  }
  veExit();
}

/*@bsdoc
procedure echo {
    usage {echo <msg> ...}
    desc {Writes the given message (made by concatenating command-line
				    arguments with spaces) to stderr.}
}
*/
static int cmd_echo(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  int k;
  for (k = 1; k < objc; k++) {
    if (k > 1) {
      fputc(' ',stderr);
    }
    fputs(bsObjGetStringPtr(objv[k]),stderr);
  }
  fputc('\n',stderr);
  return BS_OK;
}

static int report_code(BSInterp *i, int code) {
  if (code == BS_OK || code == BS_RETURN)
    return 0;
  if (code != BS_ERROR)
    veError(MODULE,"Unexpected BlueScript result code: %s (%d)",
	    bsCodeToString(code), code);
  else if (bsIsResultClear(i))
    veError(MODULE,"BlueScript error (no details available)");
  else
    veError(MODULE,"BlueScript error: %s",
	    bsObjGetStringPtr(bsGetResult(i)));
  return -1;
}

int veBlueEvalStream(FILE *stream) {
  int k;
  veThrMutexLock(interp_mutex);
  k = report_code(interp,bsEvalStream(interp,stream));
  veThrMutexUnlock(interp_mutex);
  return k;
}

int veBlueEvalFile(char *filename) {
  int k;
  veThrMutexLock(interp_mutex);
  k = report_code(interp,bsEvalFile(interp,filename));
  veThrMutexUnlock(interp_mutex);
  return k;
}

int veBlueEvalString(char *s) {
  int k;
  veThrMutexLock(interp_mutex);
  k = report_code(interp,bsEvalString(interp,s));
  veThrMutexUnlock(interp_mutex);
  return k;
}

void veBlueSetExtProc(char *name, BSExtProc proc, void *cdata) {
  (void) bsSetExtProc(interp,name,proc,cdata);
}

static int string_proc(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  VeBlueStringProc proc = (VeBlueStringProc)cdata;
  char **argv;
  int k, code;

  argv = veAlloc((objc+1)*sizeof(char *),0);
  for (k = 0; k < objc; k++)
    argv[k] = bsObjGetStringPtr(objv[k]);
  argv[k] = NULL;

  code = proc(objc,argv);
  
  veFree(argv);

  if (code != BS_OK) {
    if (code == BS_ERROR) {
      if (bsIsResultClear(i))
	bsSetStringResult(i,"unknown error in application-provided function",
			  BS_S_STATIC);
    } else {
      if (bsIsResultClear(i))
	bsAppendResult(i,"unexpected result code from application-provided function: ",
		       bsCodeToString(code),NULL);
      code = BS_ERROR;
    }
  }
  return code;
}


void veBlueSetProc(char *name, VeBlueStringProc proc) {
  if (proc)
    bsSetExtProc(interp,name,string_proc,(void *)proc);
  else
    bsUnsetProc(interp,name);
}

void veBlueSetResult(char *value) {
  bsSetStringResult(interp,value,BS_S_VOLATILE);
}

void veBlueSetVar(char *name, int global, char *value) {
  BSObject *o;
  o = bsObjString(value,-1,BS_S_VOLATILE);
  bsSet(interp,(global ? interp->global : NULL),
	name,o);
  bsObjDelete(o);
}

char *veBlueGetVar(char *name, int global) {
  BSObject *o;
  if (!(o = bsGet(interp, 
		  (global ? interp->global : NULL),
		  name,0)))
    return NULL;
  return bsObjGetStringPtr(o);
}

void veBlueInit(void) {
  static int init = 0;

  if (init)
    veFatalError(MODULE,"attempt to call veBlueInit twice");
  init = 1;

  interp_mutex = veThrMutexCreate();

  /* create standard interpreter */
  interp = bsInterpCreateStd();

  /* extra functionality */
  /* include - include contents of other files */
  veBlueSetExtProc("include",cmd_include,NULL);

  /* VE commands */

  /* env - creates environment */
  veBlueSetExtProc("env",cmd_env,NULL);

  /* profile - creates user profile */
  veBlueSetExtProc("profile",cmd_profile,NULL);

  /* device/use - declare/use devices */
  /*
     format should *mostly* match up, except for the following:
     - options cannot have names that match an existing BlueScript
       command (this is probably fixable with some minor BlueScript
       changes)
     - options must be lists of size 1 or 2 (not greater than 2)
   */
  veBlueSetExtProc("device",cmd_use,(void *)0);
  veBlueSetExtProc("use",cmd_use,(void *)1);

  /* mathematics */
  veBlueSetExtProc("v3add",cmd_v3add,NULL);
  veBlueSetExtProc("v3sub",cmd_v3sub,NULL);
  veBlueSetExtProc("v3scale",cmd_v3scale,NULL);
  veBlueSetExtProc("v3cross",cmd_v3cross,NULL);
  veBlueSetExtProc("v3dot",cmd_v3dot,NULL);
  veBlueSetExtProc("v3mag",cmd_v3mag,NULL);
  veBlueSetExtProc("v3norm",cmd_v3norm,NULL);
  veBlueSetExtProc("v3ind",cmd_v3ind,NULL);

  veBlueSetExtProc("qnorm",cmd_qnorm,NULL);
  veBlueSetExtProc("qmult",cmd_qmult,NULL);
  veBlueSetExtProc("qarb",cmd_qarb,NULL);
  veBlueSetExtProc("qaxis",cmd_qaxis,NULL);
  veBlueSetExtProc("qang",cmd_qang,NULL);
  veBlueSetExtProc("qind",cmd_qind,NULL);

  veBlueSetExtProc("m4ident",cmd_m4ident,NULL);
  veBlueSetExtProc("m4rotate",cmd_m4rotate,NULL);
  veBlueSetExtProc("m4trans",cmd_m4trans,NULL);
  veBlueSetExtProc("m4mult",cmd_m4mult,NULL);
  veBlueSetExtProc("m4multv",cmd_m4multv,NULL);
  veBlueSetExtProc("m4invert",cmd_m4invert,NULL);
  veBlueSetExtProc("m4ind",cmd_m4ind,NULL);

  veBlueSetExtProc("deg2rad",cmd_deg2rad,NULL);
  veBlueSetExtProc("rad2deg",cmd_rad2deg,NULL);
  veBlueSetExtProc("sin",cmd_trig,(void *)TRIG_SIN);
  veBlueSetExtProc("cos",cmd_trig,(void *)TRIG_COS);
  veBlueSetExtProc("tan",cmd_trig,(void *)TRIG_TAN);
  veBlueSetExtProc("asin",cmd_trig,(void *)TRIG_ASIN);
  veBlueSetExtProc("acos",cmd_trig,(void *)TRIG_ACOS);
  veBlueSetExtProc("atan",cmd_atan,NULL);
  veBlueSetExtProc("sqrt",cmd_sqrt,NULL);
  veBlueSetExtProc("abs",cmd_abs,NULL);

  /* filter functions */
  veBlueSetExtProc("filter",cmd_filter,NULL);
  veBlueSetExtProc("event",cmd_event,NULL);

  /* origin/eye access */
  veBlueSetExtProc("frame_origin",cmd_frame,(void *)ORIGIN);
  veBlueSetExtProc("frame_eye",cmd_frame,(void *)EYE);

  /* generic */
  veBlueSetExtProc("exit",cmd_exit,NULL);
  veBlueSetExtProc("echo",cmd_echo,NULL);

  /* audio */
  veBlueSetExtProc("audio",cmd_audio,NULL);
  veBlueSetExtProc("audiodevice",cmd_audiodevice,NULL);
}
