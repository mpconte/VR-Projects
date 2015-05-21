# include <stdio.h>
# include <stdlib.h>
# include <ve.h>
# include <GL/gl.h>
# define EYE_HEIGHT 1.6
# define ROT_VEL 45.0

# include <GL/glu.h>
# include <3ds.h>
# include <3dsRenderer.h>
# include <vem.h>

# define LIGHT 

struct world_state {
  int visuals;       /* display something                                   */
  float now;         /* time since simulation started                       */
  long startTime;    /* start time (in msec)                                */
  float rx, ry, rz, s, tx, ty, tz;
  GLfloat loc[3];
};
struct world_state globalState;

static struct t3DModel model; // possibly different on every processor
static char *root, *name;

static void timer_callback(void *v);

/*
 * Setup the window on each processor
 */
static void setupwin(VeWindow *w) 
{
  int i;

  static GLfloat lightamb[4] = {0.2, 0.2, 0.2, 1.0};
  static GLfloat lightdif[4] = {0.8, 0.8, 0.8, 1.0};
  static GLfloat lightspec[4] = {0.4, 0.4, 0.4, 1.0};
  static GLfloat loc[4] = {0.0, 10.0, 0.0, 1.0};
  static GLfloat diffuseMaterial[4] = {0.8, 0.8, 0.8, 1.0};
  static GLfloat mat_specular[4] = {1.0, 1.0, 1.0, 1.0};

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


# ifdef LIGHT
  glEnable(GL_LIGHTING);
  glEnable(GL_NORMALIZE);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lightamb); 

  /* one other light source */
  glLightfv(GL_LIGHT0, GL_AMBIENT, lightamb);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightdif);
  glLightfv(GL_LIGHT0, GL_SPECULAR, lightspec);
  glLightfv(GL_LIGHT0, GL_POSITION, loc);
  glEnable(GL_LIGHT0);

  glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseMaterial);
  glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
  glMaterialf(GL_FRONT, GL_SHININESS, 25.0);
  glColorMaterial(GL_FRONT, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
#endif
  
  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);
  if(Import3DS(&model, root, name) < 0)
    fprintf(stderr,"model load fails\n");
  else
    fprintf(stderr, "model loaded\n");
}

/*
 * On each processor, generate the appropriate redisplay
 */
static void display(VeWindow *w, long tm, VeWallView *wv)
{
  int x, z, c;
# ifdef LIGHT
  GLfloat loc[4] = {0.0, 5.0, 0.0, 1.0};

  loc[0] = globalState.loc[0];
  loc[1] = globalState.loc[1];  
  loc[2] = globalState.loc[2];
  loc[3] = 1.0;
  glLightfv(GL_LIGHT0, GL_POSITION, loc);
# endif

  glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
  glDisable(GL_TEXTURE_2D);

  c = 1;
  for(x= -10; x<10; x++){
    for(z= -10; z<10;z++) {
      if(c)
	glColor3ub(255,0,0);
      else
	glColor3ub(255,255,255);
      c = !c;
      glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(x, 0, z);
      glVertex3f(x, 0, z+1);
      glVertex3f(x+1, 0, z+1);
      glVertex3f(x+1, 0, z);
      glEnd ();
    }
    c = !c;
  }

  glPushMatrix();
  glTranslatef(globalState.tx, globalState.ty, globalState.tz);
  glScalef(globalState.s, globalState.s, globalState.s);
  glRotatef(globalState.rz, 0.0f, 0.0f, 1.0f);
  glRotatef(globalState.ry, 0.0f, 1.0f, 0.0f);
  glRotatef(globalState.rx, 1.0f, 0.0f, 0.0f);
  glScalef(model.scale, model.scale, model.scale);
  glTranslatef(-model.center_x,-model.center_y,-model.center_z);
  Render3DS(&model); 
  glPopMatrix();

  printf("%f %f %f %f %f %f %f\n", 
    globalState.tx, globalState.ty, globalState.tz,
    globalState.s, 
    globalState.rx, globalState.ry, globalState.rz);
}

static int exitcback(VeDeviceEvent *e, void *arg) {
  exit(0);
  /*NOTREACHED*/
  return -1;
}

static int reset(VeDeviceEvent *e, void *arg) {
  globalState.tx = 0.0f;
  globalState.ty = 0.8f;
  globalState.tz = 0.0f;
  globalState.rx = 0.0f;
  globalState.ry = 0.0f;
  globalState.rz = 0.0f;
  globalState.s = 2.0f;
  globalState.startTime = veClock();
  globalState.now = 0.0f;
  vePostRedisplay();
}

static int txp(VeDeviceEvent *e, void *arg) {
  globalState.tx += 0.1;
  vePostRedisplay();
}

static int txm(VeDeviceEvent *e, void *arg) {
  globalState.tx -= 0.1;
  vePostRedisplay();
}
  
static int typ(VeDeviceEvent *e, void *arg) {
  globalState.ty += 0.1;
  vePostRedisplay();
}

static int tym(VeDeviceEvent *e, void *arg) {
  globalState.ty -= 0.1;
  vePostRedisplay();
}
  
  
static int tzp(VeDeviceEvent *e, void *arg) {
  globalState.tz += 0.1;
  vePostRedisplay();
}

static int tzm(VeDeviceEvent *e, void *arg) {
  globalState.tz -= 0.1;
  vePostRedisplay();
}
  

static int rxp(VeDeviceEvent *e, void *arg) {
  globalState.rx += 5.0;
  vePostRedisplay();
}

static int rxm(VeDeviceEvent *e, void *arg) {
  globalState.rx -= 5.0;
  vePostRedisplay();
}
  
static int ryp(VeDeviceEvent *e, void *arg) {
  globalState.ry += 5.0;
  vePostRedisplay();
}

static int rym(VeDeviceEvent *e, void *arg) {
  globalState.ry -= 5.0;
  vePostRedisplay();
}
  
  
static int rzp(VeDeviceEvent *e, void *arg) {
  globalState.rz += 5.0;
  vePostRedisplay();
}

static int rzm(VeDeviceEvent *e, void *arg) {
  globalState.rz -= 5.0;
  vePostRedisplay();
}
  
  
static int sp(VeDeviceEvent *e, void *arg) {
  globalState.s *= 1.1;
  vePostRedisplay();
}

static int sm(VeDeviceEvent *e, void *arg) {
  globalState.s /= 1.1;
  vePostRedisplay();
}

static void notifier()
{
   VEM_get_pos(&globalState.loc[0], &globalState.loc[1], &globalState.loc[2]);
   globalState.loc[1] += 1.0; /* put light above the head */
}
  

/*
 * This is our timer callback
 */
# define STEP_TIME 0.05
static void timer_callback(void *unused) 
{
  globalState.now = (veClock() - globalState.startTime) * 1.0e-3;
  globalState.ry =  globalState.now * ROT_VEL;
  vePostRedisplay();
  veAddTimerProc(1000/30, timer_callback, (void *) NULL); // 30 times a second
}


int main(int argc, char **argv)
{
  veInit(&argc, argv);

  if(argc != 3) {
    fprintf(stderr,"Usage: 3dsVuewer <root> <3ds file>\n");
    exit(1);
  }
  veSetOption("depth", "1");


  veRenderSetupCback(setupwin);
  veRenderCback(display);

  reset(NULL, NULL);

  veMPAddStateVar(0, &globalState, sizeof(globalState), VE_MP_AUTO);

  /* certain things only happen on the master machine */
  if (veMPIsMaster()) {

    /* Setup processing callbacks */
    VEM_default_bindings();
    /*    VEM_check_collisions(collide); */
    VEM_initial_position(0.0f, EYE_HEIGHT, 10.0f);
    veDeviceAddCallback(exitcback, NULL, "exit");
    veDeviceAddCallback(rxp, NULL, "rxp");
    veDeviceAddCallback(rxm, NULL, "rxm");
    veDeviceAddCallback(ryp, NULL, "ryp");
    veDeviceAddCallback(rym, NULL, "rym");
    veDeviceAddCallback(rzp, NULL, "rzp");
    veDeviceAddCallback(rzm, NULL, "rzm");
    veDeviceAddCallback(txp, NULL, "txp");
    veDeviceAddCallback(txm, NULL, "txm");
    veDeviceAddCallback(typ, NULL, "typ");
    veDeviceAddCallback(tym, NULL, "tym");
    veDeviceAddCallback(tzp, NULL, "tzp");
    veDeviceAddCallback(tzm, NULL, "tzm");
    veDeviceAddCallback(sp, NULL, "sp");
    veDeviceAddCallback(sm, NULL, "sm");
    veDeviceAddCallback(reset, NULL, "reset");
    VEM_get_pos(&globalState.loc[0], &globalState.loc[1], &globalState.loc[2]);
    notifier();
    VEM_notify(notifier);

    veAddTimerProc(0, timer_callback, NULL);
    globalState.startTime = veClock();
    globalState.now = 0.0f;
  }

  txmSetRenderer(NULL, txmOpenGLRenderer());
  txmSetMgrFlags(NULL, TXM_MF_SHARED_IDS);


  root = argv[1];
  name = argv[2];
  veRun();
}
