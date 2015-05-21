/* note: this module uses the appdata pointer, so other modules had best
   beware... */
/* note: this module is something of a no-op - the code to update the
   distort value is missing.  Is it actually of any serious use? */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "calibrate.h"

#define MODULE "calibrate:distort"

/* tracking for distortion correction */
int current_pt = 0;
typedef struct {
  float pt[4][2]; /* distortion calibration points in screen space */
} cal_priv_t;
GLUquadricObj *q;
static float adjust_increment = 0.01;

static float def_cal_pts[4][2] = {
  { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 }
};

#if 0
void update_distort(void) {
  int N = 9;
  float src1[9][3], src2[9][3], *src[9], dst1[9], dst2[9];
  
  if (current_window) {
    int i;
    cal_priv_t *p = (cal_priv_t *)(current_window->appdata);
    /* build vectors of data */
    /* 4 corners, center and midway along each line */
    for(i = 0; i < 4; i++) {
      src1[i][0] = def_cal_pts[i][0];
      src1[i][1] = def_cal_pts[i][1];
      src1[i][2] = 1.0;
      dst1[i] = p->pt[i][0];
      dst2[i] = p->pt[i][1];
    }
    for(i = 0; i < 4; i++) {
      src1[i+4][0] = (def_cal_pts[i][0]+def_cal_pts[(i+1)%4][0])/2.0;
      src1[i+4][1] = (def_cal_pts[i][1]+def_cal_pts[(i+1)%4][1])/2.0;
      src1[i+4][2] = 1.0;
      dst1[i+4] = (p->pt[i][0]+p->pt[(i+1)%4][0])/2.0;
      dst2[i+4] = (p->pt[i][1]+p->pt[(i+1)%4][1])/2.0;
    }
    dst1[8] = src1[8][0] = 0;
    dst2[8] = src1[8][1] = 0;
    src1[8][2] = 1.0;

    for(i = 0; i < 4; i++) {
      dst1[8] += p->pt[i][0];
      dst2[8] += p->pt[i][1];
    }
    dst1[8] /= 4.0;
    dst2[8] /= 4.0;

    for(i = 0; i < N; i++) {
      src[i] = src2[i];
    }

    /* solve for d1..d3 */
    memcpy(src2,src1,sizeof(src1));
    leastsq(src,N,3,dst1);
    /* solve for d2..d3 */
    memcpy(src2,src1,sizeof(src1));
    leastsq(src,N,3,dst2);

    /* set values in res in distortion matrix */
    veMatrixIdentity(&(current_window->distort));
    for(i = 0; i < 3; i++) {
      current_window->distort.data[0][i] = dst1[i];
      current_window->distort.data[1][i] = dst2[i];
    }
    printf("disort = \n[ %2.4f %2.4f %2.4f ]\n[ %2.4f %2.4f %2.4f ]\n[ %2.4f %2.4f %2.4f ]\n",
	   current_window->distort.data[0][0],
	   current_window->distort.data[0][1],
	   current_window->distort.data[0][2],
	   current_window->distort.data[1][0],
	   current_window->distort.data[1][1],
	   current_window->distort.data[1][2],
	   current_window->distort.data[2][0],
	   current_window->distort.data[2][1],
	   current_window->distort.data[2][2]);
  }
}
#else
void update_distort(void) {
	/* do nothing */
}
#endif

static void recv_curpt(int tag, void *data, int dlen) {
  VeWindow *w = current_window;
  /* sanity checks */
  if (tag != DTAG_CURPT)
    veFatalError(MODULE,"non-CURPT message received by recv_distort");
  if (dlen != sizeof(current_pt))
    veFatalError(MODULE,"CURPT message is wrong size - got %d, expected %d",
		 dlen, sizeof(current_pt));
  memcpy(&current_pt,data,dlen);
}

static void recv_calpt(int tag, void *data, int dlen) {
  /* a cal_priv_t structure */
  VeWindow *w = current_window;
  /* sanity checks */
  if (!w)
    veFatalError(MODULE,"CALPT message without current window being set");
  if (tag != DTAG_CALPT)
    veFatalError(MODULE,"non-CALPT message received by recv_calpt");
  if (!w->appdata)
    veFatalError(MODULE,"CALPT message, but current window has no appdata");
  if (dlen != sizeof(cal_priv_t))
    veFatalError(MODULE,"CALPT message is wrong size - got %d, expected %d",
		 dlen, sizeof(cal_priv_t));
  memcpy(w->appdata,data,dlen);
}

static void recv_distort(int tag, void *data, int dlen) {
  /* the distort member */
  /* sanity checks */
  if (!w)
    veFatalError(MODULE,"DISTORT message without current window being set");
  if (tag != DTAG_DISTORT)
    veFatalError(MODULE,"non-DISTORT message received by recv_distort");
  if (dlen != sizeof(w->distort))
    veFatalError(MODULE,"DISTORT message is wrong size - got %d, expected %d",
		 dlen, sizeof(cal_priv_t));
  memcpy(w->distort,data,dlen);
}

void distort_init(void) {
  VeEnv *e = veGetEnv();
  VeWall *wl;
  VeWindow *w;
  int i;
  if (!e) {
    fprintf(stderr, "no env loaded\n");
    exit(1);
  }
  for(wl = e->walls; wl; wl = wl->next)
    for(w = wl->windows; w; w = w->next) {
      VeVector3 v1,v2;
      cal_priv_t *p;
      if (!current_window)
	current_window = w;
      /* initialize calibration points */
      p = malloc(sizeof(cal_priv_t));
      assert(p != NULL);
      for(i = 0; i < 4; i++) {
	v1.data[0] = def_cal_pts[i][0];
	v1.data[1] = def_cal_pts[i][1];
	v1.data[2] = 0.0;
	veMatrixMultPoint(&(w->distort),&v1,&v2,NULL);
	p->pt[i][0] = v2.data[0];
	p->pt[i][1] = v2.data[1];
      }
      w->appdata = (void *)p;
    }
  veMPDataAddHandler(DTAG_CURPT,recv_curpt);
  veMPDataAddHandler(DTAG_CALPT,recv_calpt);
  veMPDataAddHandler(DTAG_DISTORT,recv_distort);
}

void distort_setup(VeWindow *w) {
  if (!q) {
    q = gluNewQuadric();
    gluQuadricOrientation(q,GLU_INSIDE);
    gluQuadricTexture(q,GL_TRUE);
  }
}

void distort_event(VeDeviceEvent *e) {
  VeWindow *w = current_window;
  cal_priv_t *p;

  if (!w)
    return;

  p = (cal_priv_t *)(w->appdata);

  if (strcmp(e->elem,"cyclepoint") == 0) {
    if (!VeDeviceToSwitch(e))
      return;
    current_pt = (current_pt+1)%4;
    veMPDataPush(DTAG_CURPT,&current_pt,sizeof(current_pt));
  } else if (strcmp(e->elem,"finer") == 0) {
    adjust_increment /= 10;
    return;
  } else if (strcmp(e->elem,"coarser") == 0) {
    adjust_increment *= 10;
    return;
  }

  if (strncmp(e->elem,"value_") == 0) {
    float f = veDeviceToValuator(e)*adjust_increment;
    char *s = e->elem+6;
    if (strcmp(s,"x") == 0)
      p->pt[current_pt][0] += f;
    else if (strcmp(s,"y") == 0)
      p->pt[current_pt][1] += f;
    veMPDataPush(DTAG_CALPT,p,sizeof(cal_priv_t));
  } else if (strncmp(e->elem,"incr_",5) == 0) {
    char *s = e->elem+5;
    if (!veDeviceToSwitch(e))
      return;
    if (strcmp(s,"x") == 0)
      p->pt[current_pt][0] += adjust_increment;
    else if (strcmp(s,"y") == 0)
      p->pt[current_pt][1] += adjust_increment;
    veMPDataPush(DTAG_CALPT,p,sizeof(cal_priv_t));
  } else if (strncmp(e->elem,"decr_",5) == 0) {
    char *s = e->elem+5;
    if (!veDeviceToSwitch(e))
      return;
    if (strcmp(s,"x") == 0)
      p->pt[current_pt][0] -= adjust_increment;
    else if (strcmp(s,"y") == 0)
      p->pt[current_pt][1] -= adjust_increment;
    veMPDataPush(DTAG_CALPT,p,sizeof(cal_priv_t));
  }
}

#define TARGETSZ 0.05
void distort_display(VeWindow *w, long tm, VeWallView *wv) {
  int i;
  cal_priv_t *p = (cal_priv_t *)(w->appdata);

  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.0,1.0,-1.0,1.0,-1.0,1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  if (current_window == w)
    glColor3fv(current_col);
  else
    glColor3fv(wall_col);

  glBegin(GL_LINE_LOOP);
  for(i = 0; i < 4; i++)
    glVertex3f(p->pt[i][0],p->pt[i][1],0.0);
  glEnd();

  if (current_window == w)
    glColor3fv(current_bord);
  else
    glColor3fv(wall_bord);
  /* crosslines */
  glBegin(GL_LINES);
  glVertex3f((p->pt[0][0]+p->pt[1][0])/2.0,
	     (p->pt[0][1]+p->pt[1][1])/2.0,
	     0.0);
  glVertex3f((p->pt[2][0]+p->pt[3][0])/2.0,
	     (p->pt[2][1]+p->pt[3][1])/2.0,
	     0.0);
  glVertex3f((p->pt[0][0]+p->pt[3][0])/2.0,
	     (p->pt[0][1]+p->pt[3][1])/2.0,
	     0.0);
  glVertex3f((p->pt[2][0]+p->pt[1][0])/2.0,
	     (p->pt[2][1]+p->pt[1][1])/2.0,
	     0.0);
  glEnd();

  /* targets */
  for(i = 0; i < 4; i++) {
    if (i == current_pt)
      glColor3fv(current_pt_col);
    else if (current_window == w)
      glColor3fv(current_bord);
    else
      glColor3fv(wall_bord);
    glBegin(GL_LINES);
    glVertex3f(p->pt[i][0]-TARGETSZ,p->pt[i][1],0.0);
    glVertex3f(p->pt[i][0]+TARGETSZ,p->pt[i][1],0.0);
    glVertex3f(p->pt[i][0],p->pt[i][1]-TARGETSZ,0.0);
    glVertex3f(p->pt[i][0],p->pt[i][1]+TARGETSZ,0.0);
    glEnd();
    glPushMatrix();
    glTranslatef(p->pt[i][0],p->pt[i][1],0);
    gluDisk(q,0.6*TARGETSZ,0.7*TARGETSZ,16,2);
    glPopMatrix();
  }

  draw_data(w);
}
