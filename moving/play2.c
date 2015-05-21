# include <stdio.h>
# include <stdlib.h>
# include <math.h>
# include <ve.h>
# include <GL/gl.h>
# define EYE_HEIGHT 1.6
# define ROT_VEL 45.0

# include <GL/glu.h>
# include <3ds.h>
# include <3dsRenderer.h>
# include <vem.h>

# define LIGHT 
# define NOBJS 5

struct transform {
  float rx, ry, rz, s, tx, ty, tz;
};

struct {
  char *dir;
  char *model;
  struct transform t;
} models[NOBJS] = {
  { "sarah", "sarah.3ds", {0, 0, 0, 2.0, 2, 1, 2}},
  { "sarah", "sarah.3ds", {0, 0, 0, 2.0, -2, 1, 2}},
  { "sarah", "sarah.3ds", {0, 0, 0, 2.0, 2, 1, -2}},
  { "sarah", "sarah.3ds", {0, 0, 0, 2.0, -2, 1, -2}},
  { "sarah", "sarah.3ds", {0, 0, 0, 2.0, 0, 1, 0}}
};
static struct t3DModel model[NOBJS]; // possibly different on every processor


struct world_state {
  int hit;
  float now;         /* time since simulation started                       */
  long startTime;    /* start time (in msec)                                */
  struct transform tr[NOBJS];
  GLfloat loc[3];
};
struct world_state globalState;


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
  for(i=0;i<NOBJS;i++) {
    if(Import3DS(&model[i], models[i].dir, models[i].model) < 0) {
      fprintf(stderr,"model %s %s load fails\n", models[i].dir, models[i].model);
      exit(1);
    } else {
      fprintf(stderr, "model loaded\n");
    }
  }
}

/*
 * On each processor, generate the appropriate redisplay
 */
static void display(VeWindow *w, long tm, VeWallView *wv)
{
  int x, z, c, i;
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

  for(i=0;i<NOBJS;i++) {
    glPushMatrix();

    glTranslatef(globalState.tr[i].tx, globalState.tr[i].ty, globalState.tr[i].tz);
/*
    printf("%d: %f %f %f %f %f %f %f\n",i,
    globalState.tr[i].tx, globalState.tr[i].ty, globalState.tr[i].tz,
    globalState.tr[i].s,
    globalState.tr[i].rx, globalState.tr[i].ry, globalState.tr[i].rz);
*/
    glScalef(globalState.tr[i].s, globalState.tr[i].s, globalState.tr[i].s);
    glRotatef(globalState.tr[i].rz, 0.0f, 0.0f, 1.0f);
    glRotatef(globalState.tr[i].ry, 0.0f, 1.0f, 0.0f);
    glRotatef(globalState.tr[i].rx, 1.0f, 0.0f, 0.0f);

    glScalef(model[i].scale, model[i].scale, model[i].scale);
    glTranslatef(-model[i].center_x,-model[i].center_y,-model[i].center_z);
    Render3DS(&model[i]); 
    glPopMatrix();
  }
}

static int exitcback(VeDeviceEvent *e, void *arg) {
  exit(0);
  /*NOTREACHED*/
  return -1;
}

static int reset(VeDeviceEvent *e, void *arg) {
  globalState.startTime = veClock();
  globalState.now = 0.0f;
  globalState.hit = 0;
  vePostRedisplay();
}

static int hitme(VeDeviceEvent *e, void *arg) {
  globalState.hit = 1;
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
  int i;
  globalState.now = (veClock() - globalState.startTime) * 1.0e-3;
  globalState.tr[0].ty =2 -   2 * cos(globalState.now);
  if(globalState.hit) {
    for(i=1;i<NOBJS;i++)
      globalState.tr[i].ty =2 -   2 * cos(globalState.now + 1000 * i);
  }
  vePostRedisplay();
  veAddTimerProc(1000/30, timer_callback, (void *) NULL); // 30 times a second
}


int main(int argc, char **argv)
{
  int i;

  veInit(&argc, argv);
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
    veDeviceAddCallback(reset, NULL, "reset");
    veDeviceAddCallback(hitme, NULL, "hitme");
    VEM_get_pos(&globalState.loc[0], &globalState.loc[1], &globalState.loc[2]);
    notifier();
    VEM_notify(notifier);

    veAddTimerProc(0, timer_callback, NULL);
    globalState.startTime = veClock();
    globalState.now = 0.0f;
    globalState.hit = 0;
    for(i=0;i<NOBJS;i++) {
      globalState.tr[i] = models[i].t;
    }
  }

  txmSetRenderer(NULL, txmOpenGLRenderer());
  txmSetMgrFlags(NULL, TXM_MF_SHARED_IDS);


  veRun();
}
