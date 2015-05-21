#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calibrate.h"

static float current_col[3] = { 1.0, 0.0, 0.0 };
static float current_bord[3] = { 1.0, 1.0, 1.0 };
static float wall_col[3] = { 0.0, 0.6, 0.0 };
static float wall_bord[3] = { 0.0, 0.6, 0.6 };

static void mkviewport(VeWindow *w) {
  if (w->geometry) {
    int d[2];
    veiGlParseGeom(w->geometry,&(d[0]),&(d[1]),
		   &(w->viewport[2]),&(w->viewport[3]));
    w->viewport[0] = w->viewport[1] = 0;
  }
}

void viewport_setup(VeWindow *w) {
  glShadeModel(GL_FLAT);
  glClearColor(0,0,0,0);
}

/* calibrate viewport */
void viewport_event(VeDeviceEvent *e) {
  VeWindow *w = current_window;
  if (!w) {
    return;
  }
  if (!veDeviceToSwitch(e) && VE_EVENT_TYPE(e) != VE_ELEM_VALUATOR) {
    return;
  }
  if (w->viewport[2] == 0 || w->viewport[3] == 0)
    mkviewport(w);

  if (strncmp(e->elem,"incr_",5) == 0) {
    char *s = e->elem+5;
    if (strcmp(s,"left") == 0) {
      w->viewport[0]++;
      w->viewport[2]--;
      if (w->viewport[3] < 0)
	w->viewport[3] = 0;
    } else if (strcmp(s,"lower") == 0) {
      w->viewport[1]++;
      w->viewport[3]--;
      if (w->viewport[3] < 0)
	w->viewport[3] = 0;
    } else if (strcmp(s,"right") == 0) {
      w->viewport[2]--;
      if (w->viewport[2] < 0)
	w->viewport[2] = 0;
    } else if (strcmp(s,"upper") == 0) {
      w->viewport[3]--;
      if (w->viewport[3] < 0)
	w->viewport[3] = 0;
    }
  } else if (strncmp(e->elem,"decr_",5) == 0) {
    char *s = e->elem+5;
    if (strcmp(s,"left") == 0) {
      w->viewport[0]--;
      if (w->viewport[0] < 0)
	w->viewport[0] = 0;
      w->viewport[2]++;
    } else if (strcmp(s,"lower") == 0) {
      w->viewport[1]--;
      if (w->viewport[1] < 0)
	w->viewport[1] = 0;
      w->viewport[3]++;
    } else if (strcmp(s,"right") == 0) {
      w->viewport[2]++;
    } else if (strcmp(s,"upper") == 0) {
      w->viewport[3]++;
    }
  } else if (strncmp(e->elem,"value_",6) == 0) {
    char *s = e->elem+6;
    int k = (int)(veDeviceToValuator(e)*4);
    fprintf(stderr, "%s %d %f\n",s,k,veDeviceToValuator(e));
    if (k > 0) {
      if (strcmp(s,"left") == 0) {
	w->viewport[0] += k;
	w->viewport[2] -= k;
	if (w->viewport[3] < 0)
	  w->viewport[3] = 0;
      } else if (strcmp(s,"lower") == 0) {
	w->viewport[1] += k;
	w->viewport[3] -= k;
	if (w->viewport[3] < 0)
	  w->viewport[3] = 0;
      } else if (strcmp(s,"right") == 0) {
	w->viewport[2] -= k;
	if (w->viewport[2] < 0)
	  w->viewport[2] = 0;
      } else if (strcmp(s,"upper") == 0) {
	w->viewport[3] -= k;
	if (w->viewport[3] < 0)
	  w->viewport[3] = 0;
      }
    } else {
      k = -k;
      if (strcmp(s,"left") == 0) {
	w->viewport[0] -= k;
	if (w->viewport[0] < 0)
	  w->viewport[0] = 0;
	w->viewport[2] += k;
      } else if (strcmp(s,"lower") == 0) {
	w->viewport[1] -= k;
	if (w->viewport[1] < 0)
	  w->viewport[1] = 0;
	w->viewport[3] += k;
      } else if (strcmp(s,"right") == 0) {
	w->viewport[2] += k;
      } else if (strcmp(s,"upper") == 0) {
	w->viewport[3] += k;
      }
    }
  }
}

void viewport_display(VeWindow *w, long tm,
		      VeWallView *wv) {
  int x,y,wid,hgt;
  char str[80];
  int incr, roff = 15;

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  veRenderGetWinInfo(w,&x,&y,&wid,&hgt);
  glOrtho(-0.5,wid-0.5,-0.5,hgt-0.5,-1,1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  /* draw grid */
  if (current_window == w)
    glColor3fv(current_col);
  else
    glColor3fv(wall_col);

  glBegin(GL_LINES);
  for(x = 1; x < wid-1; x += 5) {
    glVertex3i(x,0,0);
    glVertex3i(x,hgt-1,0);
  }
  for(y = 1; y < hgt-1; y += 5) {
    glVertex3i(0,y,0);
    glVertex3i(wid-1,y,0);
  }
  glEnd();

  /* draw border */
  if (current_window == w)
    glColor3fv(current_bord);
  else
    glColor3fv(wall_bord);
  glBegin(GL_LINE_LOOP);
  glVertex3i(0,0,0);
  glVertex3i(wid-1,0,0);
  glVertex3i(wid-1,hgt-1,0);
  glVertex3i(0,hgt-1,0);
  glEnd();

  incr = veiGlStringHeight(data_font,"X");
  x = y = roff;

  veiGlPushTextMode(w);
  glColor3f(1.0,1.0,1.0);
  sprintf(str,"Wall: %s",w->wall->name);
  glRasterPos2i(x,y);
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  y += incr;
  sprintf(str,"Window: %s",w->name);
  glRasterPos2i(x,y);
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  y += incr;
  sprintf(str,"Viewport: %d %d %d %d",
	  w->viewport[0], w->viewport[1],
	  w->viewport[2], w->viewport[3]);
  glRasterPos2i(x,y);
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  y += incr;
  sprintf(str,"Viewport Adjust");
  glRasterPos2i(x,y);
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  y += incr;
  veiGlPopTextMode();
}
