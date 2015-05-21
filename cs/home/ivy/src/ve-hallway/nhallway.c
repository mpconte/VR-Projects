# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <sys/time.h>

# include "nhallway.h"


# define CONFIG_FILE "hallway.config"

void configureHallway()
{
  FILE* hallway_file;
  char str [256];
  float n1, n2;
  int i;
  
  if((hallway_file = fopen (CONFIG_FILE, "r")) == NULL) {
    fprintf(stderr,"Unable to read %s\n", CONFIG_FILE);
    exit(1);
  }

  /* skip coments */
  do   {
    if(fgets (str, 256, hallway_file) == NULL){
      fprintf(stderr,"unexpected end of file in config file\n");
      exit(1);
    }
  } while (str [0] == '%');
  
  /* read hallway length */
  sscanf (str, "%f", &n1); 
  hallway.length = n1;

  /* skip coments */
  do  {
    if(fgets (str, 256, hallway_file) == NULL) {
      fprintf(stderr,"unexpected end of file in config file\n");
      exit(1);
    }
  } while (str [0] == '%');

  /* read hallway width and height */
  sscanf (str, "%f %f", &n1, &n2); 
  hallway.width = n1;
  hallway.height = n2;

  /* skip coments */
  do  {
    fgets (str, 256, hallway_file);
  } while (str [0] == '%');

  /* read strip length */
  sscanf (str, "%f", &n1); 
  hallway.stripLength = n1;
  
  fclose(hallway_file);
  
  /* initialize the rest of the hallway */
  hallway.numStrips = (int) hallway.length / hallway.stripLength;
  hallway.stripsLeft = (color*) malloc(hallway.numStrips * sizeof(color));
  hallway.stripsRight = (color*) malloc(hallway.numStrips * sizeof(color));
  if((hallway.stripsLeft == (color *)NULL)||(hallway.stripsRight == (color *)NULL)) {
    fprintf(stderr,"Out of memory in configure_hallway\n");
    exit(0);
  }
  
  seedRandomGenerator();
  
  for(i=0;i<hallway.numStrips;i++)  {
    getRandomColor(&(hallway.stripsLeft[i]));
    getRandomColor(&(hallway.stripsRight[i]));
  }
}


/*
 * The floor runs in the z plane in the rectange defined by
 * (0,-hallway.width/2) to (hallway.length,hallway.width/2)
 * It is drawn in grey (level 0.5)
 */
void drawHallFloor()
{
  glColor3f(0.5,0.5,0.5);
  glBegin(GL_QUADS);
  glVertex3f(0.0f, -hallway.width/2.f, 0.0f);
  glVertex3f(hallway.length, -hallway.width/2.f, 0.0f);
  glVertex3f(hallway.length, hallway.width/2.f, 0.0f);
  glVertex3f(0.0f,  hallway.width/2.f, 0.0f);
  glEnd();
}


/*
 * The ceiling runs in the z=hallway.ceiling  plane in the rectange defined by
 * (0,-hallway.width/2) to (hallway.length,hallway.width/2)
 * It is drawn in grey (level 0.7)
 */
void drawHallCeiling()
{
  glColor3f(0.7,0.7,0.7);
  glBegin(GL_QUADS);
  glVertex3f(0.0f, -hallway.width/2.f, hallway.height);
  glVertex3f(hallway.length, -hallway.width/2.f, hallway.height);
  glVertex3f(hallway.length, hallway.width/2.f, hallway.height);
  glVertex3f(0.0f,  hallway.width/2.f, hallway.height);
  glEnd();
}

/*
 * Draw the left wall. This consists of a bunch of strips
 * (numStrips), each of length stripLength. Hopefully numStrips *
 * stripLength == hallway.length, so it all ties together. The value
 * of hallway.stripsLeft[i] defines the colour of strip i
 */
void drawLeftWall()
{
  int i;
  glBegin(GL_QUAD_STRIP);
  for(i=0;i<hallway.numStrips;i++) {
    glColor3f(hallway.stripsLeft[i].red, hallway.stripsLeft[i].green,
	      hallway.stripsLeft[i].blue);
    glVertex3f(i*hallway.stripLength, hallway.width/2.f, hallway.height);
    glVertex3f(i*hallway.stripLength, hallway.width/2.f, 0.0f);
  }
  glEnd();  
}

/*
 * Draw the right wall. This consists of a bunch of strips
 * (numStrips), each of length stripLength. Hopefully numStrips *
 * stripLength == hallway.length, so it all ties together. The value
 * of hallway.stripsLeft[i] defines the colour of strip i
 */
void drawRightWall()
{
  int i;
  glBegin(GL_QUAD_STRIP);
  for(i=0;i<hallway.numStrips;i++) {
    glColor3f(hallway.stripsRight[i].red, hallway.stripsRight[i].green,
	      hallway.stripsRight[i].blue);
    glVertex3f(i*hallway.stripLength, -hallway.width/2.f, hallway.height);
    glVertex3f(i*hallway.stripLength, -hallway.width/2.f, 0.0f);
  }
  glEnd();  
}

/*
 * Flicker the walls in the hallway. This randomly flickers up
 * to maxflicker stripes on each wall
 */
void flickerWalls(int maxflicker)
{
  int i, flicker, strip;

  /* do the left wall */
  flicker =  (int)(maxflicker * drand48()) + 1;
  for (i=0;i<flicker;i++) {
    strip = (int) hallway.numStrips * drand48();
    getRandomColor(&(hallway.stripsLeft[strip]));
  }

  /* and the right */
  flicker =  (int)(maxflicker * drand48()) + 1;
  for (i=0;i<flicker;i++) {
    strip = (int) hallway.numStrips * drand48();
    getRandomColor(&(hallway.stripsRight[strip]));
  }
}

/*
 * drawTarget - draw the target at a specific x position in the
 * hallway.
 */
void drawTarget(float xpos)
{
  glColor3f(1.0f, 1.0f, 1.0f);
  glLineWidth(5.0);
  glBegin(GL_LINE_STRIP);
  glVertex3f(xpos, -hallway.width/2, hallway.height);
  glVertex3f(xpos, hallway.width/2, hallway.height);
  glVertex3f(xpos, hallway.width/2, 0.0f);
  glVertex3f(xpos, -hallway.width/2, 0.0f);
  glVertex3f(xpos, -hallway.width/2, hallway.height);
  glEnd();
    
    
  glBegin(GL_LINES);
  glVertex3f(xpos, 0.0f, 0.0f);
  glVertex3f(xpos, 0.0f, hallway.height);
  glEnd();

  glBegin(GL_LINES);
  glVertex3f(xpos, -hallway.width/2, hallway.height/2);
  glVertex3f(xpos, hallway.width/2, hallway.height/2);
  glEnd();
}

/*
 * drawLine - draw a frame on the hallway (starting position)
 */
void drawLine(float xpos)
{
  glColor3f(0.0f, 1.0f, 0.0f);
  glLineWidth(5.0);
  glBegin(GL_LINE_STRIP);
  glVertex3f(xpos, -hallway.width/2, hallway.height);
  glVertex3f(xpos, hallway.width/2, hallway.height);
  glVertex3f(xpos, hallway.width/2, 0.0f);
  glVertex3f(xpos, -hallway.width/2, 0.0f);
  glVertex3f(xpos, -hallway.width/2, hallway.height);
  glEnd();
}

/*
 * Seed the random number generator so that it starts with a new
 * sequence each time.
 */
void seedRandomGenerator()
{
  struct timeval *tp, tempr;
  struct timezone *tzp, tempz;
  
  tp = &tempr;
  tzp = &tempz;
  
  if(gettimeofday(tp,tzp) != 0) {
    fprintf(stderr,"ERROR GETTING TIME OF DAY\n");
    exit(1);
  }
  srand(tp->tv_usec);
}

/*
 * Create a random colour.
 */
void getRandomColor(color *c)
{
  c->red   = (GLfloat) drand48();
  c->blue  = (GLfloat) drand48();
  c->green = (GLfloat) drand48();  
}
