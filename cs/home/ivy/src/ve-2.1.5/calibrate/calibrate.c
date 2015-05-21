/*
 * This is a VE application for calibrating the windows of a
 * VE environment.  It was originally designed for the IVY project
 * but is deliberately designed to be flexible and extensible so
 * it is more of a general calibration tool.
 *
 * This is the most straight-forward way to adjust the various
 * calibration values of window structures in VE.
 *
 * The output of this program is a new environment file containing
 * the calibration values.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "calibrate.h"

#define MODULE "calibrate"

VeWall *current_wall = NULL;
VeWindow *current_window = NULL;
VeiGlFont *data_font = NULL;
char *output_file = NULL;

/* init is app initialization (done on all nodes)
   setup is window initialization (done per window on slaves) 
*/
   
typedef struct {
  char *name;
  void (*init)(void);
  void (*setup)(VeWindow *);
  void (*event)(VeDeviceEvent *e);
  void (*display)(VeWindow *, long tm, VeWallView *);
} calibration;

extern void distort_init(void);
extern void viewport_setup(VeWindow *), windowerr_setup(VeWindow *),
  distort_setup(VeWindow *);
extern void viewport_event(VeDeviceEvent *), distort_event(VeDeviceEvent *),
  windowerr_event(VeDeviceEvent *);
extern void viewport_display(VeWindow *, long, VeWallView *), 
  distort_display(VeWindow *, long, VeWallView *), 
  windowerr_display1(VeWindow *, long, VeWallView *), 
  windowerr_display2(VeWindow *, long, VeWallView *),
  windowerr_display3(VeWindow *, long, VeWallView *);

/* we're not using the distort test for now - it's not finished */

calibration calmodes[] = {
  { "viewport", NULL, viewport_setup, viewport_event, viewport_display },
/*{ "distort", distort_init, distort_setup, distort_event, distort_display },*/
  { "windowerr", NULL, windowerr_setup, windowerr_event, windowerr_display1 },
  { "windowerr", NULL, NULL, windowerr_event, windowerr_display2 },
  { "windowerr", NULL, NULL, windowerr_event, windowerr_display3 },
  { NULL, NULL, NULL }
};
int current_mode = 0;

void print_results() {
  if (!output_file)
    veEnvWrite(veGetEnv(),stdout);
  else {
    FILE *f;
    if (!(f = fopen(output_file,"w")))
      veFatalError("calibrate","cannot open file %s for writing - %s",
		   output_file ? output_file : "<null>", strerror(errno));
    veEnvWrite(veGetEnv(),f);
    fclose(f);
  }
}

static void recv_wininfo(int tag, void *data, int dlen) {
  VeWindow *winfo = (VeWindow *)data;

  if (tag != DTAG_WININFO)
    veFatalError(MODULE,"non-WININFO packet sent to recv_wininfo");
  if (dlen != sizeof(VeWindow))
    veFatalError(MODULE,"invalid WININFO packet size");
  if (current_window) {
    int k;
    /* copy window calibration information */
    current_window->width_err = winfo->width_err;
    current_window->height_err = winfo->height_err;
    current_window->offset_x = winfo->offset_x;
    current_window->offset_y = winfo->offset_y;
    current_window->distort = winfo->distort;
    for(k = 0; k < 4; k++)
      current_window->viewport[k] = winfo->viewport[k];
  }
}

static void recv_curwin(int tag, void *data, int dlen) {
  VeWall *wl;
  VeWindow *w;
  char *walln, *winn;
  if (tag != DTAG_CURWIN)
    veFatalError(MODULE,"non-CURWIN packet sent to recv_curwin");
  walln = data;
  winn = walln + strlen(walln) + 1;
  for(wl = veGetEnv()->walls; wl; wl = wl->next)
    if (strcmp(wl->name,walln) == 0)
      break;
  if (!wl)
    return;
  for(w = wl->windows; w; w = w->next)
    if (strcmp(w->name,winn) == 0)
      break;
  if (!w)
    return;
  current_wall = wl;
  current_window = w;
}

void next_win(void) {
  char strbuf[1024];
  int n1,n2;
  if (!current_wall)
    current_wall = veGetEnv()->walls;
  if (current_window)
    current_window = current_window->next;
  while (!current_window) {
    current_wall = current_wall->next;
    if (!current_wall)
      current_wall = veGetEnv()->walls;
    current_window = current_wall->windows;
  }
  if ((n1 = strlen(current_wall->name))+(n2 = strlen(current_window->name))+2 
      >= 1024)
    veFatalError(MODULE,"wall/window name combo is too long: %s, %s",
		 current_wall->name, current_window->name);
  strcpy(strbuf,current_wall->name);
  strcpy(strbuf+n1+1,current_window->name);
  veMPDataPush(DTAG_CURWIN,strbuf,n1+n2+2);
  if (current_window)
    veMPDataPush(DTAG_WININFO,current_window,sizeof(VeWindow));
}

int control_func(VeDeviceEvent *e, void *arg) {
  if (!veDeviceToSwitch(e))
    return 0;
    
  if (strcmp(e->elem,"cyclemode") == 0) {
    current_mode++;
    if (calmodes[current_mode].name == NULL)
      current_mode = 0;
  } else if (strcmp(e->elem,"cyclewin") == 0) {
    next_win();
  } else if (strcmp(e->elem,"end") == 0) {
    print_results();
    exit(0);
  }
  if (current_window)
    veMPDataPush(DTAG_WININFO,current_window,sizeof(VeWindow));
  vePostRedisplay();
  return 0;
}

void forward_setup(VeWindow *w) {
  int k;
  for(k = 0; calmodes[k].name; k++) {
    if (calmodes[k].setup)
      calmodes[k].setup(w);
  }
}

int forward_events(VeDeviceEvent *e, void *arg) {
  if (current_mode >= 0) {
    if (calmodes[current_mode].event) {
      calmodes[current_mode].event(e);
      if (current_window)
	veMPDataPush(DTAG_WININFO,current_window,sizeof(VeWindow));
      vePostRedisplay();
    }
  }
  return 0;
}

void forward_display(VeWindow *w, long tm, VeWallView *wv) {
  if (current_mode >= 0) {
    if (calmodes[current_mode].display) {
      calmodes[current_mode].display(w,tm,wv);
    }
  }
}

int main(int argc, char **argv) {
  int c,k;
  extern char *optarg;

  veInit(&argc,argv);

  while((c = getopt(argc,argv,"o:")) != EOF)
    switch(c) {
    case 'o':
      output_file = optarg;
      break;
      /* ignore unknown options... */
    }

  veMPDataAddHandler(DTAG_CURWIN,recv_curwin);
  veMPDataAddHandler(DTAG_WININFO,recv_wininfo);
  veMPAddStateVar(DTAG_CURMODE,&current_mode,sizeof(current_mode),VE_MP_AUTO);
  veMPInit();
  next_win();

  veSetOption("doublebuffer","1");
  veSetOption("depth","1");
  
  veDeviceAddCallback(control_func,NULL,"control.*");
  veDeviceAddCallback(forward_events,NULL,"adjust.*");
  veiGlSetWindowCback(forward_setup);
  veiGlSetDisplayCback(forward_display);
  /* setup orientation */
  veFrameIdentity(veGetOrigin());
  veFrameIdentity(veGetDefaultEye());

  /* initialize modules */
  for(k = 0; calmodes[k].name; k++)
    if (calmodes[k].init)
      calmodes[k].init();

  veRun();
}
