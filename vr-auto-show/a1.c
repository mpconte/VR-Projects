# include <stdio.h>
# include <math.h>
# include <ve.h>
# include <GL/gl.h>
# include <GL/glut.h>
# include <GL/glu.h>
# define EYE_HEIGHT 1.6

# include <3ds.h>
# include <3dsRenderer.h>
# include <vem.h>

/* Rotation increment per frame (degrees)
# define ROT_SPEED 15.0

// Room dimensions (metres)
# define ROOM_WIDTH 15
# define ROOM_LENGTH 15
# define ROOM_HEIGHT 5

/* the number of milliseconds between frames */
/* The following value is slightly more than 60Hz */
#define FRAME_INTERVAL (1000/60) 

/* Number of Cars in the scene */
# define NUM_CARS 4

struct transform {
	float rx, ry, rz, s, tx, ty, tz;
};


static float current[4] = { 0.0, EYE_HEIGHT, 5.0, 1.0 };
static GLfloat lightPos[4] = {0.0, ROOM_HEIGHT, 0.0, 1.0};

static float x_vector[3] = {1.0, 0.0, 0.0};
static float y_vector[3] = {0.0, 1.0, 0.0};
static float z_vector[3] = {0.0, 0.0, 1.0};

static float x_rotate_vector[3];
static float y_rotate_vector[3];
static float z_rotate_vector[3];

static int pan_clicked = -1; //-1 if panning disabled with joystick movement

static int hit_car_index = -1; //index of struct array of cars that's currently "hit"


struct {
	char *dir;
	char *model;
	char current_axis; //current axis selected for rotation
	int rotate_x_axis; // if 1 or -1, rotate car about its x-axis
	int rotate_y_axis; // if 1 or -1, rotate car about its y-axis
	int rotate_z_axis; // if 1 or -1, rotate car about its z-axis
	struct transform t;
} cars[NUM_CARS] = {
	{"cars", "diablo.3ds", 'y', 0, 1, 0, {0,0,0,4.5,-12.0,1.0, -12.0}},
	{"cars", "Hummer N260907.3DS", 'y', 0, 1, 0, {0,0,0,4.5,12.0,2.0,-12.0}},
	{"cars", "Mersedes 1928 N300708.3DS", 'y', 0, 1, 0, {0,0,0,4.5,-12.0,1.0, 0.0}},
	{"cars", "Corvette.3ds", "Corvette", 0, 1, 0, {0,0,0,4.5,12.0,1.0,0.0}}
};


static struct t3DModel car[NUM_CARS];
static struct t3DModel light;

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
  glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
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

	for (i=0; i<NUM_CARS;i++)
	{
	  if(Import3DS(&car[i], cars[i].dir, cars[i].model)< 0)
	    fprintf(stderr,"model load fails\n");
	  else
	    fprintf(stderr, "model loaded\n");
	}

	if (Import3DS(&light, "cars", "fan object.3ds") < 0)
		fprintf(stderr, "model load fails\n");
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
  GLfloat loc[4] = {0.0, 4.0, 0.0, 1.0};

  glLightfv(GL_LIGHT0, GL_POSITION, loc);
# endif

  glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
  glDisable(GL_TEXTURE_2D);

  //draw checkerboard floor here
  c = 1;
  for(x= -ROOM_WIDTH; x<ROOM_WIDTH; x++){
    for(z= -ROOM_LENGTH; z<ROOM_LENGTH;z++) {
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

	// five grey walls (texture with "VR Auto Show")

	// left wall
	glColor3ub(225, 225, 221);
	glBegin(GL_QUADS);
	glNormal3f(1,0,0);
	glVertex3f(-ROOM_WIDTH, 0, -ROOM_LENGTH);
	glVertex3f(-ROOM_WIDTH, 0, ROOM_LENGTH);
	glVertex3f(-ROOM_WIDTH, 5, ROOM_LENGTH);
	glVertex3f(-ROOM_WIDTH, 5, -ROOM_LENGTH);
	glEnd();
	
	//right wall
	glColor3ub(225, 225, 221);
	glBegin(GL_QUADS);
	glNormal3f(-1,0,0);
	glVertex3f(ROOM_WIDTH, 0, -ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 0, ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 5, ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 5, -ROOM_LENGTH);
	glEnd();

	// back wall	
	glColor3ub(225, 225, 221);
	glBegin(GL_QUADS);
	glNormal3f(0,0,1);
	glVertex3f(-ROOM_WIDTH, 0, -ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 0, -ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 5, -ROOM_LENGTH);
	glVertex3f(-ROOM_WIDTH, 5, -ROOM_LENGTH);
	glEnd();
	
	// ceiling
	glColor3ub(225, 225, 221);
	glBegin(GL_QUADS);
	glNormal3f(0,-1,0);
	glVertex3f(-ROOM_WIDTH, 5, ROOM_LENGTH);
	glVertex3f(-ROOM_WIDTH, 5, -ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 5, -ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 5, ROOM_LENGTH);
	glEnd();
	
	// back wall	
	glColor3ub(225, 225, 221);
	glBegin(GL_QUADS);
	glNormal3f(0,0,1);
	glVertex3f(-ROOM_WIDTH, 0, ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 0, ROOM_LENGTH);
	glVertex3f(ROOM_WIDTH, 5, ROOM_LENGTH);
	glVertex3f(-ROOM_WIDTH, 5, ROOM_LENGTH);
	glEnd();							


	int i = 0;

	for (i=0; i<NUM_CARS; i++)
	{
	 // if car is interacting with user, make manipulative axis green and other white
	 if (i == hit_car_index)
	 {	   	
		if (cars[i].current_axis == 'x')
			glColor3f(0, 255, 0);
		else
			glColor3f(255, 255, 255);

		glBegin(GL_LINES);
		glVertex3f(cars[i].t.tx - 4.0f, 
			   cars[i].t.ty, 
			   cars[i].t.tz);
		glVertex3f(cars[i].t.tx + 4.0f, 
			   cars[i].t.ty, 
			   cars[i].t.tz);
		glEnd();


		if (cars[i].current_axis == 'y')
			glColor3f(0, 255, 0);
		else
			glColor3f(255, 255, 255);

		glBegin(GL_LINES);
		glVertex3f(cars[i].t.tx, 
			   cars[i].t.ty - 4.0f, 
			   cars[i].t.tz);
		glVertex3f(cars[i].t.tx, 
			   cars[i].t.ty + 4.0f, 
			   cars[i].t.tz);
		glEnd();
		


		if (cars[i].current_axis == 'z')
			glColor3f(0, 255, 0);
		else
			glColor3f(255, 255, 255);

		glBegin(GL_LINES);
		glVertex3f(cars[i].t.tx, 
			   cars[i].t.ty, 
			   cars[i].t.tz - 4.0f);
		glVertex3f(cars[i].t.tx, 
			   cars[i].t.ty, 
			   cars[i].t.tz + 4.0f);
		glEnd();		
	 }
		

	  glPushMatrix();

	  glTranslatef(cars[i].t.tx, cars[i].t.ty, cars[i].t.tz );
	
   	  glScalef(cars[i].t.s, cars[i].t.s, cars[i].t.s);

	  glRotatef(cars[i].t.rz, 0.0f, 0.0f, 1.0f);
          //glRotatef(cars[i].t.rz, 0.0f, 0.0f, car[i].center_z);
	  //glRotatef(cars[i].t.rz, z_rotate_vector[0], z_rotate_vector[1], z_rotate_vector[2]);

	  glRotatef(cars[i].t.ry, 0.0f, 1.0f, 0.0f);
	  //glRotatef(cars[i].t.ry, 0.0f, car[i].center_y, 0.0f);
   	  //glRotatef(cars[i].t.ry, y_rotate_vector[0], y_rotate_vector[1], y_rotate_vector[2]);
	
	  glRotatef(cars[i].t.rx, 1.0f, 0.0f, 0.0f);
	  //glRotatef(cars[i].t.rx, car[i].center_x, 0.0f, 0.0f); 
	  //glRotatef(cars[i].t.rx, x_rotate_vector[0], x_rotate_vector[1], x_rotate_vector[2]);
	

	  glScalef(car[i].scale, car[i].scale, car[i].scale);

 	  //?
 	  //glTranslatef(-car[i].center_x,-car[i].center_y,-car[i].center_z);	  	  
	  Render3DS(&car[i]); 
	  
	  glPopMatrix();
	}

	//Render3DS(&light);
}

static int exitcback(VeDeviceEvent *e, void *arg) {
  exit(0);
  return -1;
}

/* 
Timer call back
*/
static void timer_callback(void *unused)
{
	int i;
	for (i=0; i < NUM_CARS; i++)
	{		
		// rotate cars here
		if (cars[i].rotate_x_axis != 0)
			cars[i].t.rx += cars[i].rotate_x_axis*ROT_SPEED;

		if (cars[i].rotate_y_axis != 0)
			cars[i].t.ry += cars[i].rotate_y_axis*ROT_SPEED;

		if (cars[i].rotate_z_axis != 0)
			cars[i].t.rz += cars[i].rotate_z_axis*ROT_SPEED;
											
	}
	vePostRedisplay();
	veAddTimerProc(1000/30, timer_callback, (void *)NULL);
}


static void notifier()
{
   VEM_get_pos(&current[0], &current[1], &current[2]);  
}


static int collision(float *pos, float *from)
{
	int i;
	
	// Check for car collision
	for (i=0; i< NUM_CARS; i++)
	{
		if ( sqrt( ((pos[0] - cars[i].t.tx) * (pos[0] - cars[i].t.tx)) + ((pos[2] - cars[i].t.tz) * (pos[2] - cars[i].t.tz))) <= 4.0f)
		{
			printf("CAR COLLISION with %s\n", cars[i].name);
			hit_car_index = i;
			return 1;
		}
	}

	// Check for wall collision
	if ( fabs(pos[0])  >= ROOM_WIDTH || fabs(pos[2] >= ROOM_LENGTH))
	{
		printf("WALL COLLISION\n");
		hit_car_index = -1;
		return 1;		
	}

	printf("NO COLLISION\n");
	hit_car_index = -1;	
	return 0;
}


// Switch current manipulative axis rotation here
static int axisChange(VeDeviceEvent *e, void *arg) {
	
	if (hit_car_index > -1)
	{
		if (cars[hit_car_index].current_axis == 'z')
			cars[hit_car_index].current_axis = 'x';
		else if (cars[hit_car_index].current_axis == 'y')
			cars[hit_car_index].current_axis = 'z';
		else if (cars[hit_car_index].current_axis == 'x')
			cars[hit_car_index].current_axis = 'y';
	}
	
	return 0;
}

// invoke counter-clockwise rotation for current manipulative axis of a car
static int ccw(VeDeviceEvent *e, void *arg) {

	if (hit_car_index > -1)
	{
		if (cars[hit_car_index].current_axis == 'x')
			cars[hit_car_index].rotate_x_axis = 1;
		else if (cars[hit_car_index].current_axis == 'y')
			cars[hit_car_index].rotate_y_axis = 1;
		else if (cars[hit_car_index].current_axis == 'z')
			cars[hit_car_index].rotate_z_axis = 1;	
	}

	return 0;
}

// invoke clockwise rotation for current manipulative axis of a car
static int cw(VeDeviceEvent *e, void *arg) {

	if (hit_car_index > -1)
	{
		if (cars[hit_car_index].current_axis == 'x')
			cars[hit_car_index].rotate_x_axis = -1;
		else if (cars[hit_car_index].current_axis == 'y')
			cars[hit_car_index].rotate_y_axis = -1;
		else if (cars[hit_car_index].current_axis == 'z')
			cars[hit_car_index].rotate_z_axis = -1;	
	}

	return 0;
}

// invoke no rotation for current manipulative axis of a car
static int stop(VeDeviceEvent *e, void *arg) {

	if (hit_car_index > -1)
	{
		if (cars[hit_car_index].current_axis == 'x')
			cars[hit_car_index].rotate_x_axis = 0;
		else if (cars[hit_car_index].current_axis == 'y')
			cars[hit_car_index].rotate_y_axis = 0;
		else if (cars[hit_car_index].current_axis == 'z')
			cars[hit_car_index].rotate_z_axis = 0;	
	}

	return 0;
}


int main(int argc, char **argv)
{
  veInit(&argc, argv);

  veSetOption("depth", "1");


  veRenderSetupCback(setupwin);
  veRenderCback(display);

  reset(NULL, NULL);

  //veMPAddStateVar(0, &globalState, sizeof(globalState), VE_MP_AUTO);

  /* certain things only happen on the master machine */
  if (veMPIsMaster()) {

    /* Setup processing callbacks */
    VEM_default_bindings();
    VEM_check_collisions(collision);

    VEM_initial_position(0.0f, EYE_HEIGHT, ROOM_LENGTH-1);
    veDeviceAddCallback(exitcback, NULL, "exit");

    veDeviceAddCallback(axisChange, NULL, "axisChange");
    veDeviceAddCallback(cw, NULL, "cw");	
    veDeviceAddCallback(ccw, NULL, "ccw");
    veDeviceAddCallback(stop, NULL, "stop");

    notifier();
    VEM_notify(notifier);
  }

  // new
  veAddTimerProc(0,timer_callback,NULL);

  txmSetRenderer(NULL, txmOpenGLRenderer());
  txmSetMgrFlags(NULL, TXM_MF_SHARED_IDS);

  veRun();
}
