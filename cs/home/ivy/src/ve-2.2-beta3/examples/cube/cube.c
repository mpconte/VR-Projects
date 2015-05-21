/*
 * This is a simple program to demonstrate how to build basic OpenGL
 * programs with VE.  In this case, we will be drawing a cube with different
 * colours on each face and then spinning it.  The user can control:
 *
 * - which axes (x, y, and/or z) to spin around
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ve.h>     /* We need this include for all of the main VE functions */

#include <GL/gl.h>
#include <GL/glu.h>

/* our rotational speed in deg/sec */
#define ROTSPEED 45.0

/* the number of milliseconds between frames */
/* The following value is slightly more than 60Hz */
#define FRAME_INTERVAL (1000/60)

struct {
  float ang[3];
  int rot[3];
} state = {
  { 0.0, 0.0, 0.0 },
  { 0, 1, 0 }
};

/* constant to identify our main state variable */
#define SV_ANGLES 0

/* Callback declarations */
void setupwin(VeWindow *w); /* OpenGL init cback */
void display(VeWindow *w, long tm, VeWallView *wv); /* OpenGL render cback */
void update_angles(void *); /* timer callback for updating rotations */
int axis_toggle(VeDeviceEvent *, void *); /* input event callback */

int main(int argc, char **argv) {
  /*
   * Every VE program should start with a call to "veInit()".
   * This sets up basic structures and allows the library to parse and
   * process any command-line arguments.
   *
   */
  veInit(&argc,argv);
  /*
   * After returning from veInit(), any arguments the library recognized
   * will have been removed.  Thus, if we so desired, we could check for
   * program-specific options at this point.
   *
   */
  
  /*
   * If we have specific needs for the framebuffer configuration that we
   * will be using, we need to let the library know at this point by
   * setting options.  Option names and values are strings.
   */
  veSetOption("depth","1");          /* request a depth buffer (Z-buffer) */
  veSetOption("doublebuffer","1");   /* request a double-buffered config */
  /* Other possible options:
     - "red", "green", "blue", "alpha" - request a specific minimum depth for 
       red, green, blue, and alpha components.  All buffers will have 
       bits for red, green, and blue, but not necessarily alpha.  If your
       program needs an alpha component in the frame-buffer you should include
       
       veSetOption("alpha","1");

     Note that options can also be set via the command-line, e.g.
    
       prog -ve_opt alpha 1

     but in general, requirements of the program should be hardcoded.
  */

  /* We need to setup two OpenGL callbacks - one for initializing a window
   * when it is created, and another for rendering a window (similar to a
   * GLUT display callback).
   */
  veRenderSetupCback(setupwin);
  veRenderCback(display);

  /* Setup a multi-processing data handler.  This is crucial in 
   * configurations where there are multiple rendering processes.
   * This function will be called on each of the slave procsses to
   * receive data that is sent from the master.
   *
   * By using a "state variable" we allow VE to take care of
   * the details of synchronizing the data.  VE_MP_AUTO tells
   * VE to automatically synchronize this buffer before each
   * frame.
   */
  veMPAddStateVar(SV_ANGLES,&state,sizeof(state),VE_MP_AUTO);

  /* Setup the input event callbacks.
   * In this case, any event with a device name "axis" (regardless of
   * element name) will be passed to the axis_toggle() function 
   */
  veDeviceAddCallback(axis_toggle,NULL,"axis");
    
  /* Setup a timer procedure.  This is a callback that will be called
   * once after the specified amount of time has elapsed.  We can
   * have the function repeatedly every 'n' seconds by registering
   * a new timer procedure in the callback itself (see the code for 
   * update_angles() below).  In this case, we will call the update_angles()
   * in "0" milliseconds, which means "as soon as possible".
   */
  veAddTimerProc(0,update_angles,NULL);

  /* It is generally a good idea to set the Z clip planes at this point.
   * There is only one set of global values.  This will likely be improved
   * in a later version of the library.
   */
  veSetZClip(0.1,10.0);

  /* Once our initialization is complete, we enter VE's event loop and
   * let the library take over from here.
   */
  veRun();

  /* veRun() should never return, but good form indicates we should
   * handle the unexpected as gracefully as possible.
   */
  return 0;
}

/* The GL initialization callback.  This is called once for each window.
 * At this point, it is safe to make OpenGL calls.  It is generally used
 * to setup the OpenGL context.  Many VE programs will use multiple windows
 * and will not share OpenGL contexts.  Thus you should be sure to
 * define any lights, materials, textures, etc. in *every* window.
 *
 * Calls to this callback are serialized - that is, there are no other threads
 * in the user program when this function is called.
 */
void setupwin(VeWindow *w) {
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);
}

/* draw a cube axis-aligned around the origin where each dimension is 2*f */
void draw_cube(float f) {
  glBegin(GL_QUADS);

  glColor3f(1.0,0.0,0.0); /* red */
  glVertex3f(-f,-f,-f);
  glVertex3f(-f,-f,f);
  glVertex3f(f,-f,f);
  glVertex3f(f,-f,-f);
  
  glColor3f(0.0,1.0,0.0); /* green */
  glVertex3f(f,-f,f);
  glVertex3f(f,-f,-f);
  glVertex3f(f,f,-f);
  glVertex3f(f,f,f);

  glColor3f(0.0,0.0,1.0); /* blue */
  glVertex3f(f,f,-f);
  glVertex3f(f,f,f);
  glVertex3f(-f,f,f);
  glVertex3f(-f,f,-f);

  glColor3f(1.0,1.0,0.0); /* yellow */
  glVertex3f(-f,f,f);
  glVertex3f(-f,f,-f);
  glVertex3f(-f,-f,-f);
  glVertex3f(-f,-f,f);

  glColor3f(0.0,1.0,1.0); /* cyan */
  glVertex3f(-f,-f,-f);
  glVertex3f(f,-f,-f);
  glVertex3f(f,f,-f);
  glVertex3f(-f,f,-f);

  glColor3f(1.0,0.0,1.0); /* magenta */
  glVertex3f(-f,-f,f);
  glVertex3f(f,-f,f);
  glVertex3f(f,f,f);
  glVertex3f(-f,f,f);

  glEnd();
}

/* The GL rendering callback.  This is called once for each window in each
 * frame that is rendered.  Note that these callbacks may be called in
 * parallel - that is, it is possible for multiple threads to be executing
 * this code at once.
 *
 * You should not initialize the projection or modelview matrices - VE
 * sets these up for you.  You can push/pop/multiply the existing matrices
 * as you normally would, or read the existing matrices to help in culling.
 */
void display(VeWindow *w, long tm, VeWallView *wv) {
  glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  /* rotate */
  glRotatef(state.ang[0],1.0,0.0,0.0);
  glRotatef(state.ang[1],0.0,1.0,0.0);
  glRotatef(state.ang[2],0.0,0.0,1.0);
  /* draw the cube */
  draw_cube(5.0);
  glPopMatrix();
}

/* update_angles() is our timer callback, and we effectively use it as
 * an animation loop.  Note that since our state changes in update_angles,
 * we'll also push data out to our slaves.
 */
void update_angles(void *v) {
  static long last_time = -1;
  long now;
  float tm;

  if (last_time < 0) {
    /* this is our first call - store the current value of the clock,
       so we can measure the real interval that we are being called on. */
    last_time = veClock();
  } else {
    int k;
    /* update any angles that need updating */
    now = veClock();
    tm = (now-last_time)*1.0e-3; /* this is the elapsed time since the last
				    frame */
    for (k = 0; k < 3; k++) {
      if (state.rot[k]) {
	state.ang[k] += ROTSPEED*tm;
	if (state.ang[k] > 360.0)
	  state.ang[k] -= 360.0;
      }
    }
    /* did we actually change anything? */
    if (state.rot[0] || state.rot[1] || state.rot[2]) {
      /* changes will be pushed out to the slaves automatically
         when the frame is redrawn */
      /* let the library know that the screen should be redrawn */
      vePostRedisplay();
    }
    /* save the current value of the clock */
    last_time = now;
  }

  /* Since update_angles() is called as a timer procedure, it is only
   * called once and then the library (deliberately) forgets about it.
   * If we want to use it as an animation loop, we need to keep requeueing
   * it, with an appropriate delay.  The delay is in milliseconds, and
   * is defined using the FRAME_INTERVAL macro (see the top of the code).
   *
   * Increasing the value of FRAME_INTERVAL makes for slower updates.
   * Decreasing the value of FRAME_INTERVAL makes for faster updates.
   * Note that the speed of the animation is bounded by how fast the 
   * screen can actually be rendered.
   *
   * As a matter of good practice, we will add the timer procedure again
   * with the same argument it was called with (v).  In this particular
   * program, this value is always NULL, but again we are protecting against
   * future modifications.
   */
  veAddTimerProc(FRAME_INTERVAL,update_angles,v);
}

/* We handle incoming device events in the axis_toggle() callback
 * that we setup earlier.  We'll use these events to update which
 * angles to rotate around.
 *
 * Note that we do not need to push this data out to the slaves - the
 * processing which determines the new value of the angles is done on
 * the master.
 *
 * "arg" is an optional argument that is supplied by the program and
 * is not derived from the event (second argument to veDeviceAddCallback).
 * In this case, we are not using it (it will always be NULL).
 *
 * An event callback should always return 0.  Future support may allow
 * control of how further processing is done through return codes.
 */
int axis_toggle(VeDeviceEvent *e, void *arg) {
  /* ignore everything except for:
     - switch or keyboard events that are "down" (i.e. state is non-zero)
     - trigger events
  */
  if (VE_EVENT_TYPE(e) != VE_ELEM_TRIGGER &&
      !(VE_EVENT_TYPE(e) == VE_ELEM_SWITCH && VE_EVENT_SWITCH(e)->state) &&
      !(VE_EVENT_TYPE(e) == VE_ELEM_KEYBOARD && VE_EVENT_KEYBOARD(e)->state)) {
    return 0;
  }
  
  /* The event indicates that we should do something.  Based upon
     the element name in the event, toggle the various axis flags.
   */
  if (e->elem) {
    if (strcmp(e->elem,"x") == 0)
      state.rot[0] = !state.rot[0];
    else if (strcmp(e->elem,"y") == 0)
      state.rot[1] = !state.rot[1];
    else if (strcmp(e->elem,"z") == 0)
      state.rot[2] = !state.rot[2];
  }

  return 0;
}

