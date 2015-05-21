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
#include <vei_gl.h> /* This includes the OpenGL-specific parts of VE, as well
		       as the OpenGL headers themselves. */

/* our rotational speed in deg/sec */
#define ROTSPEED 45.0

/* the number of milliseconds between frames */
/* The following value is slightly more than 60Hz */
#define FRAME_INTERVAL (1000/60)

/* state variables - which axes should we update and their current angles */
float x_ang = 0.0, y_ang = 0.0, z_ang = 0.0;
/* start off so that we are spinning around y */
int x_rot = 0, y_rot = 1, z_rot = 0;

/* Callback declarations */
void setupwin(VeWindow *w); /* OpenGL init cback */
void display(VeWindow *w, long tm, VeWallView *wv); /* OpenGL render cback */
void recv_data(int tag, void *data, int dlen); /* MP data cback */
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
  veiGlSetWindowCback(setupwin);
  veiGlSetDisplayCback(display);

  /* Setup a multi-processing data handler.  This is crucial in 
   * configurations where there are multiple rendering processes.
   * This function will be called on each of the slave procsses to
   * receive data that is sent from the master.
   *
   * Note that we can have multiple handlers with different tags,
   * but in this case we'll send all incoming messages to a single
   * handler.
   */
  veMPDataAddHandler(VE_DTAG_ANY,recv_data);

  /* Setup processing callbacks */
  if (veMPIsMaster()) {
    /* Notice that this block is protected by a call to veMPIsMaster() -
     * this insures that this code is only executed once in the master process.
     * This is important for cases where VE is used in a cluster or
     * other multiple-process environment. 
     */

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
  }

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
  glPushMatrix();
  /* rotate */
  glRotatef(x_ang,1.0,0.0,0.0);
  glRotatef(y_ang,0.0,1.0,0.0);
  glRotatef(z_ang,0.0,0.0,1.0);
  /* draw the cube */
  draw_cube(5.0);
  glPopMatrix();
}

/* In multi-processing setups, the slave rendering processes do not
 * necessarily share the same memory space as the master.  Thus we need
 * to push updates to them.  The recv_data() function has been setup in
 * the main() function as the handler for all incoming messages.  Incoming
 * messages are tagged to identify what they are.  The values of these
 * tags must greater than or equal to 0.
 *
 * In this case, we only need to push the rotation angles out, so we'll
 * define it as one tag.
 */
#define TAG_ANGLES  0

/* In recv_data, we want to handle any incoming messages so that we
 * can update the state information in the renderers appropriately.
 * 
 * Note that it is possible, but not necessary, for the master and any
 * number of slaves to share the same memory space (e.g. in a multi-threaded
 * setup) so be careful when updating global variables.  It can be safely
 * assumed that this function is serialized in any particular memory space
 * (i.e. there won't be two threads in this call at the same time).
 */
void recv_data(int tag, void *data, int dlen) {
  /* even we've only defined one tag, it's a good idea to protect ourselves
     against future modifications. */
  switch (tag) {
  case TAG_ANGLES:
    {
      float *f = (float *)data;
      if (dlen != sizeof(float)*3)
	veFatalError("cube","received invalid TAG_ANGLES data - expected %d bytes, got %d", sizeof(float)*3,dlen);
      x_ang = f[0];
      y_ang = f[1];
      z_ang = f[2];
    }
    break;
  default:
    veFatalError("cube","unexpected data tag: %d",tag);
    break;
  }
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
    /* update any angles that need updating */
    now = veClock();
    tm = (now-last_time)*1.0e-3; /* this is the elapsed time since the last
				    frame */
    if (x_rot) {
      x_ang += ROTSPEED*tm;
      if (x_ang > 360.0)
	x_ang -= 360.0;
    }
    if (y_rot) {
      y_ang += ROTSPEED*tm;
      if (y_ang > 360.0)
	y_ang -= 360.0;
    }
    if (z_rot) {
      z_ang += ROTSPEED*tm;
      if (z_ang > 360.0)
	z_ang -= 360.0;
    }

    /* did we actually change anything? */
    if (x_rot || y_rot || z_rot) {
      /* push these changes out to the slaves */
      float ang[3];
      ang[0] = x_ang;
      ang[1] = y_ang;
      ang[2] = z_ang;
      veMPDataPush(TAG_ANGLES,(void *)&ang,sizeof(ang));
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
      x_rot = !x_rot;
    else if (strcmp(e->elem,"y") == 0)
      y_rot = !y_rot;
    else if (strcmp(e->elem,"z") == 0)
      z_rot = !z_rot;
  }

  return 0;
}

