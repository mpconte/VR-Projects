#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ve.h>     /* We need this include for all of the main VE functions */

#include <GL/gl.h>
#include <GL/glu.h>

/* the number of milliseconds between frames */
/* The following value is slightly more than 60Hz */
#define FRAME_INTERVAL (1000/60)

struct {
  float value;
} state = { 0.0 };

/* constant to identify our main state variable */
#define SV_STATE 0

/* Callback declarations */
void setupwin(VeWindow *w); /* OpenGL init cback */
void display(VeWindow *w, long tm, VeWallView *wv); /* OpenGL render cback */
void update_value(void *); /* timer callback for updating rotations */



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
  veMPAddStateVar(SV_STATE,&state,sizeof(state),VE_MP_AUTO);

  /* Setup processing callbacks */
  if (veMPIsMaster()) {
    /* Notice that this block is protected by a call to veMPIsMaster() -
     * this insures that this code is only executed once in the master process.
     * This is important for cases where VE is used in a cluster or
     * other multiple-process environment. 
     */
    
    /* Setup a timer procedure.  This is a callback that will be called
     * once after the specified amount of time has elapsed.  We can
     * have the function repeatedly every 'n' seconds by registering
     * a new timer procedure in the callback itself (see the code for 
     * update_angles() below).  In this case, we will call the update_angles()
     * in "0" milliseconds, which means "as soon as possible".
     */
    veAddTimerProc(0,update_value,NULL);
  }

  if (veSoundCreate("sample","sample.wav")  < 0)
    veFatalError("audio","cannot load sample.wav");

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
  glTranslatef(0.0,0.0,-8.0);
  glRotatef(30.0,0.77,0.77,0.0);
  /* draw the cube */
  draw_cube(5.0);
  glPopMatrix();
}

/* update_angles() is our timer callback, and we effectively use it as
 * an animation loop.  Note that since our state changes in update_angles,
 * we'll also push data out to our slaves.
 */
void update_value(void *v) {
  static int inst = -1;
  static VeSound *snd;

  if (!(snd = veSoundFindName("sample")))
    veFatalError("audio","could not find sample sound in memory");

  if (veAudioIsDone(inst)) {
    veAudioClean(inst);
    inst = -1;
  }

  if (inst < 0) {
    inst = veAudioInst("default",snd,NULL);
    if (inst < 0)
      veFatalError("audio","failed to start playing sample audio");
    else
      printf("Starting playing audio sample\n");
  }

  /* Since update_value() is called as a timer procedure, it is only
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
  veAddTimerProc(FRAME_INTERVAL,update_value,v);
}
