# include <GL/gl.h>

typedef struct
{
  GLfloat red;
  GLfloat blue;
  GLfloat green;
} color;

struct {
  int numStrips;
  float length,width,height;
  float stripLength;
  color *stripsLeft;
  color *stripsRight;
} hallway;

void configureHallway();
void drawHallFloor();
void drawHallCeiling();
void drawLeftWall();
void drawRightWall();
void flickerWalls(int);
void drawTarget(float);
void drawLine(float);
void seedRandomGenerator();
void getRandomColor(color *);
