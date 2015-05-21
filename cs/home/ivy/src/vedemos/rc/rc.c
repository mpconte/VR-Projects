#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
double floor(double o);
#define ACC 48

#include <ve.h>
#include <vei_gl.h>

/* Some <math.h> files do not define M_PI... */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* #define FRAME_INTERVAL (1000/30) */
#define FRAME_INTERVAL 25

#define DATA_STATE  0

#define FOG
#define FOG_D 0.1
#define DIST 30

int draw_stats = 0;

GLfloat difmat1[4] = { 1.0, 0.4, 0.4, 1.0 };
GLfloat difamb1[4] = { 1.0, 0.4, 0.4, 1.0 };
GLfloat difmat2[4] = { 0.6, 0.6, 0.6, 1.0 };
GLfloat difamb2[4] = { 0.6, 0.6, 0.6, 1.0 };
GLfloat difmat3[4] = { 1.0, 1.0, 1.0, 1.0 };
GLfloat difamb3[4] = { 1.0, 1.0, 1.0, 1.0 };
GLfloat difmat4[4] = { 0.5, 0.5, 1.0, 1.0 };
GLfloat difamb4[4] = { 0.5, 0.5, 1.0, 1.0 };
GLfloat difmat5[4] = { 1.0, 1.0, 0.5, 1.0 };
GLfloat difamb5[4] = { 1.0, 1.0, 0.5, 1.0 };
GLfloat matspec1[4] = { 1.0, 1.0, 1.0, 0.0 };
GLfloat matspec2[4] = { 0.774, 0.774, 0.774, 1.0 };
GLfloat matspec4[4] = { 0.5, 0.5, 1.0, 1.0 };
GLfloat dif_zwart[4] = { 0.3, 0.3, 0.3, 1.0 };
GLfloat amb_zwart[4] = { 0.4, 0.4, 0.4, 1.0 };
GLfloat spc_zwart[4] = { 0.4, 0.4, 0.4, 1.0 };
GLfloat dif_copper[4] = { 0.5, 0.3, 0.1, 1.0 };
GLfloat amb_copper[4] = { 0.2, 0.1, 0.0, 1.0 };
GLfloat spc_copper[4] = { 0.3, 0.1, 0.1, 1.0 };
GLfloat fogcol[4] = { 1.0, 1.0, 1.0, 1.0 };
GLfloat hishin[1] = { 100.0 };
GLfloat loshin[1] = { 5.0 };
GLfloat lightpos[4] = { 1.0, 1.0, 1.0, 0.0 };
GLfloat lightamb[4] = { 0.2, 0.2, 0.2, 1.0 };
GLfloat lightdif[4] = { 0.8, 0.8, 0.8, 1.0 };

GLubyte texture[32][32][3];
GLubyte sky[32][32][3];

int rnd(int i)
{
   return (int) ((double) drand48()*i);
}

void make_texture(void)
{
    int i,j;
    GLubyte r, g, b;
    for (i=0;i<32;i++)
    {
	for (j=0;j<32;j++)
	{
	    r = 100 + rnd(156);
	    g = 100 + rnd(156);
	    b = (b+g)/2 - rnd(100);
	    texture[i][j][0] = r/2;
	    texture[i][j][1] = g/2;
	    texture[i][j][2] = b/2;
	    r = rnd(100);
	    b = rnd(100)+156;
	    sky[i][j][1] = sky[i][j][0] = r;
	    sky[i][j][2] = b;
	}
    }
}

#define PMAX 10000
extern int tot;


GLfloat dx[PMAX], dy[PMAX], dz[PMAX];
GLfloat strips[27][PMAX][3], normal[27][PMAX][3], bnormal[2][PMAX][3];
GLdouble cum_al = 0.0, view_al = 0.0;
int opt[PMAX];
GLfloat r1[PMAX], r2[PMAX], r3[PMAX];

/* Group these values together as a state - makes life easier */
struct {
  float plaatje;
  float speed;
  int frame;
  int angle;
  float angle2;
  float angle3;
} RCState = { 0.0, 0, 0, 0, 0, 0 };

GLfloat al[PMAX], rl[PMAX], hd[PMAX], pt[PMAX];
GLfloat x[PMAX], y[PMAX], z[PMAX];

void copper_texture(void)
{
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif_copper);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb_copper);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spc_copper);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 13);
}

void groen_texture(void)
{
    glMaterialfv(GL_FRONT, GL_DIFFUSE, difmat4);
    glMaterialfv(GL_FRONT, GL_AMBIENT, difamb4);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matspec4);
    glMaterialf(GL_FRONT, GL_SHININESS, 5.0);
}

void rood_texture(void)
{
    glMaterialfv(GL_FRONT, GL_DIFFUSE, difmat1);
    glMaterialfv(GL_FRONT, GL_AMBIENT, difamb1);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matspec1);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.1*128);
}

void metaal_texture(void)
{
    glMaterialfv(GL_FRONT, GL_DIFFUSE, difmat2);
    glMaterialfv(GL_FRONT, GL_AMBIENT, difamb2);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matspec2);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.6*128.0);
}

void wit_texture(void)
{
    glMaterialfv(GL_FRONT, GL_DIFFUSE, difmat3);
    glMaterialfv(GL_FRONT, GL_AMBIENT, difamb3);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matspec1);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.8*128.0);
}

void geel_texture(void)
{
    glMaterialfv(GL_FRONT, GL_DIFFUSE, difmat5);
    glMaterialfv(GL_FRONT, GL_AMBIENT, difamb5);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matspec1);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.8*128.0);
}

void zwart_texture(void)
{
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif_zwart);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb_zwart);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spc_zwart);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 90);
}

#define VERTEX(I,J) glNormal3fv(normal[I][J]); glVertex3fv(strips[I][J]);

void do_display (void)
{
    int i,j,s,t, tmp;
    int rr = 0;
    cum_al = 0.0;

    metaal_texture();

    for (s=0;s<24;s += 2)
    {
	t = s+2;
	if (!(t&7))
	    t=t-8;

	if (s == 16)
	    rood_texture();
	glBegin(GL_QUADS);
	for (i=0;i<tot;i=j)
	{
	    tmp = 0;
	    for (j=i+1;j<tot;j++)
		if ((tmp=tmp+tmp+opt[j]) > 4000 || (!(j%(3*DIST))))
		    break;

	    if (j>=tot)
		j = 0;

	    rr++;
	    VERTEX(s, j);
	    VERTEX(s, i);
	    VERTEX(t, i);
	    VERTEX(t, j);
	    if (!j)
		break;
	}
	glEnd();
    }
    printf("Split up to %d parts.\n", rr);
    rood_texture();
    for (i=0;i<tot-2;i+=DIST)
    {
	if (!(i%(DIST*5)))
	    continue;

	glBegin(GL_QUADS);
	glNormal3fv(bnormal[0][i]);
	glVertex3fv(strips[24][i]);
	glVertex3fv(strips[24][i+5]);
	glVertex3fv(strips[26][i+5]);
	glVertex3fv(strips[26][i]);

	glNormal3fv(bnormal[1][i]);
	glVertex3fv(strips[25][i]);
	glVertex3fv(strips[25][i+5]);
	glVertex3fv(strips[26][i+5]);
	glVertex3fv(strips[26][i]);
	glEnd();
    }

    wit_texture();
    for (i=0;i<tot-2;i+=DIST)
    {
	if (i%(DIST*5))
	    continue;

	glBegin(GL_QUADS);
	glNormal3fv(bnormal[0][i]);
	glVertex3fv(strips[24][i]);
	glVertex3fv(strips[24][i+5]);
	glVertex3fv(strips[26][i+5]);
	glVertex3fv(strips[26][i]);

	glNormal3fv(bnormal[1][i]);
	glVertex3fv(strips[25][i]);
	glVertex3fv(strips[25][i+5]);
	glVertex3fv(strips[26][i+5]);
	glVertex3fv(strips[26][i]);
	glEnd();
    }

    groen_texture();
    glBegin(GL_QUADS);
    for (i=0;i<tot;i+=90)
    {
	if (dy[i]<0.2)
	    continue;
	glNormal3f(-1.0, 0.0, 0.0);
	glVertex3f(strips[22][i][0]-0.7,-4,strips[22][i][2]-0.7);
	glVertex3f(strips[22][i][0]-0.2,strips[16][i][1],strips[22][i][2]-0.2);
	glVertex3f(strips[22][i][0]-0.2,strips[16][i][1],strips[22][i][2]+0.2);
	glVertex3f(strips[22][i][0]-0.7,-4,strips[22][i][2]+0.7);

	glNormal3f(0.0, 0.0, 1.0);
	glVertex3f(strips[22][i][0]+0.7,-4,strips[22][i][2]+0.7);
	glVertex3f(strips[22][i][0]+0.2,strips[16][i][1],strips[22][i][2]+0.2);
	glVertex3f(strips[22][i][0]-0.2,strips[16][i][1],strips[22][i][2]+0.2);
	glVertex3f(strips[22][i][0]-0.7,-4,strips[22][i][2]+0.7);

	glNormal3f(0.0, 0.0, -1.0);
	glVertex3f(strips[22][i][0]+0.7,-4,strips[22][i][2]-0.7);
	glVertex3f(strips[22][i][0]+0.2,strips[16][i][1],strips[22][i][2]-0.2);
	glVertex3f(strips[22][i][0]-0.2,strips[16][i][1],strips[22][i][2]-0.2);
	glVertex3f(strips[22][i][0]-0.7,-4,strips[22][i][2]-0.7);

	glNormal3f(1.0, 0.0, 0.0);
	glVertex3f(strips[22][i][0]+0.7,-4,strips[22][i][2]-0.7);
	glVertex3f(strips[22][i][0]+0.2,strips[16][i][1],strips[22][i][2]-0.2);
	glVertex3f(strips[22][i][0]+0.2,strips[16][i][1],strips[22][i][2]+0.2);
	glVertex3f(strips[22][i][0]+0.7,-4,strips[22][i][2]+0.7);
    }
    glEnd();
}

void do_one_wheel(void)
{
    float a,p,q;
    int i;
    copper_texture();
    glBegin(GL_QUAD_STRIP);
    for (i=0;i<=ACC;i++)
    {
	a = i*M_PI*2/ACC;
	p = cos(a);
	q = sin(a);
	glNormal3f(p, q, 0.4);
	glVertex3f(0.7*p, 0.7*q, 0.8);
	glNormal3f(0.0, 0.0, 1.0);
	glVertex3f(0.8*p, 0.8*q, 0.7);
    }
    glEnd();

    glBegin(GL_QUAD_STRIP);
    for (i=0;i<=ACC;i++)
    {
	a = i*M_PI*2/ACC;
	p = cos(a);
	q = sin(a);
	glNormal3f(0.0, 0.0, 1.0);
	glVertex3f(0.7*p, 0.7*q, 0.8);
	glVertex3f(0.6*p, 0.6*q, 0.8);
    }
    glEnd();

    glBegin(GL_QUAD_STRIP);
    for (i=0;i<=ACC;i++)
    {
	a = i*M_PI*2/ACC;
	p = cos(a);
	q = sin(a);
	glNormal3f(-p, -q, 0.0);
	glVertex3f(0.6*p, 0.6*q, 0.8);
	glVertex3f(0.6*p, 0.6*q, 0.7);
    }
    glEnd();

    glBegin(GL_QUADS);

    for (i=0;i<=12;i++)
    {
	a = i*M_PI/6;
	p = cos(a);
	q = sin(a);
	glNormal3f(p, q, 0.0);
	glVertex3f(0.65*p + 0.08*q, 0.65*q + 0.08*p, 0.75);
	glVertex3f(0.65*p - 0.08*q, 0.65*q - 0.08*p, 0.75);
	glVertex3f(-0.08*q, -0.08*p, 0.95);
	glVertex3f(0.08*q, 0.08*p, 0.95);
	if (!i)
	    rood_texture();
    }

    zwart_texture();
    glNormal3f(0.0, 1.0, 0.0);
    glVertex3f(0.1, 0.0, 0.8);
    glVertex3f(-0.1, 0.0, 0.8);
    glVertex3f(-0.1, 0.0, -0.8);
    glVertex3f(0.1, 0.0, -0.8);

    glNormal3f(1.0, 0.0, 0.0);
    glVertex3f(0.0, 0.1, 0.8);
    glVertex3f(0.0, -0.1, 0.8);
    glVertex3f(0.0, -0.1, -0.8);
    glVertex3f(0.0, 0.1, -0.8);

    glEnd();
}

void init_wheel(void)
{
  glNewList(2, GL_COMPILE);
  do_one_wheel();
  glRotatef(180.0, 0.0, 1.0, 0.0);
  do_one_wheel();
  glRotatef(180.0, 0.0, 1.0, 0.0);
  glEndList();
}

void display_wheel(float w)
{
    int ww = w;
    glPopMatrix();
    glPushMatrix();
    glTranslatef(x[ww], y[ww], z[ww]);
    glRotatef(r3[ww]*180/M_PI, 0.0, 0.0, 1.0);
    glRotatef(-r2[ww]*180/M_PI, 0.0, 1.0, 0.0);
    glRotatef(r1[ww]*180/M_PI, 1.0, 0.0, 0.0);
    glTranslatef(-0.15*(w-ww), 0.8, 0.0);
    glRotatef(-w, 0.0, 0.0, 1.0);
    glCallList(2);
    glRotatef(w, 0.0, 0.0, 1.0);
    glTranslatef(0.0, -0.8, 0.0);
    zwart_texture();
    glBegin(GL_QUADS);
    glNormal3f(0.0, 1.0, 0.0);
    glVertex3f(-0.3, 1.1, 0.3);
    glVertex3f(1.0, 1.1, 0.3);
    glVertex3f(1.0, 1.1, -0.3);
    glVertex3f(0.3, 1.1, -0.3);
    glEnd();
}

void display_cart(float w)
{
    display_wheel(w);
    geel_texture();
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);
    glBegin(GL_QUADS);
    glNormal3f(0.0, 1.0, 0.0);
    glVertex3f(0.5, 0.8, -0.70);
    glVertex3f(0.5, 0.8, 0.70);
    glVertex3f(-2.0, 0.8, 0.70);
    glVertex3f(-2.0, 0.8, -0.70);

    glNormal3f(1.0, 0.0, 0.0);
    glVertex3f(-2.0, 0.8, -0.70);
    glVertex3f(-2.0, 0.8, 0.70);
    glVertex3f(-2.0, 2.3, 0.70);
    glVertex3f(-2.0, 2.3, -0.70);

    glNormal3f(0.71, 0.71, 0.0);
    glVertex3f(-2.0, 2.3, -0.70);
    glVertex3f(-2.0, 2.3, 0.70);
    glVertex3f(-1.7, 2.0, 0.70);
    glVertex3f(-1.7, 2.0, -0.70);

    glNormal3f(0.12, 0.03, 0.0);
    glVertex3f(-1.7, 2.0, -0.70);
    glVertex3f(-1.7, 2.0, 0.70);
    glVertex3f(-1.4, 0.8, 0.70);
    glVertex3f(-1.4, 0.8, -0.70);

    glNormal3f(0.0, 1.0, 0.0);
    glVertex3f(-1.4, 0.8, -0.70);
    glVertex3f(-1.4, 0.8, 0.70);
    glVertex3f(0.0, 0.8, 0.70);
    glVertex3f(0.0, 0.8, -0.70);

    glNormal3f(1.0, 0.0, 0.0);
    glVertex3f(0.0, 0.8, -0.70);
    glVertex3f(0.0, 0.8, 0.70);
    glVertex3f(0.0, 1.5, 0.70);
    glVertex3f(0.0, 1.5, -0.70);

    glNormal3f(0.5, 0.3, 0.0);
    glVertex3f(0.0, 1.5, -0.70);
    glVertex3f(0.0, 1.5, 0.70);
    glVertex3f(-0.5, 1.8, 0.70);
    glVertex3f(-0.5, 1.8, -0.70);

    glNormal3f(1.0, 0.0, 0.0);
    glVertex3f(-0.5, 1.8, -0.70);
    glVertex3f(-0.5, 1.8, 0.70);
    glVertex3f(-0.5, 0.8, 0.70);
    glVertex3f(-0.5, 0.8, -0.70);

    zwart_texture();
    glNormal3f(0.0, 0.0, 1.0);
    glVertex3f(-1.8, 0.8, 0.70);
    glVertex3f(-1.8, 1.3, 0.70);
    glVertex3f(0.0, 1.3, 0.70);
    glVertex3f(0.0, 0.8, 0.70);

    glVertex3f(-1.8, 0.8, -0.70);
    glVertex3f(-1.8, 1.6, -0.70);
    glVertex3f(0.0, 1.4, -0.70);
    glVertex3f(0.0, 0.8, -0.70);

    glVertex3f(-2.0, 0.8, 0.70);
    glVertex3f(-2.0, 2.3, 0.70);
    glVertex3f(-1.7, 2.0, 0.70);
    glVertex3f(-1.4, 0.8, 0.70);

    glVertex3f(-2.0, 0.8, -0.70);
    glVertex3f(-2.0, 2.3, -0.70);
    glVertex3f(-1.7, 2.0, -0.70);
    glVertex3f(-1.4, 0.8, -0.70);

    glVertex3f(0.0, 0.8, -0.70);
    glVertex3f(0.0, 1.5, -0.70);
    glVertex3f(-0.5, 1.8, -0.70);
    glVertex3f(-0.5, 0.8, -0.70);

    glVertex3f(0.0, 0.8, 0.70);
    glVertex3f(0.0, 1.5, 0.70);
    glVertex3f(-0.5, 1.8, 0.70);
    glVertex3f(-0.5, 0.8, 0.70);
    glEnd();
}

void display(VeWindow *w, long tm, VeWallView *wv)
{
    int plaatje2, l;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glCallList(1);
    glPopMatrix();
    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE,
	&texture[0][0][0]);

    glBegin(GL_QUADS);
    glNormal3f(0.0, 1.0, 0.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(-120, -4.1, -120);
    glTexCoord2f(0.0, 1.0); glVertex3f(-120, -4.1, 120);
    glTexCoord2f(1.0, 1.0); glVertex3f(120, -4.1, 120);
    glTexCoord2f(1.0, 0.0); glVertex3f(120, -4.1, -120);
    glEnd();
    glDisable(GL_TEXTURE_2D);

/* The sky moves with us to give a perception of being infinitely far away */

    groen_texture();
    l = RCState.plaatje;
    glBegin(GL_QUADS);
    glNormal3f(0.0, 1.0, 0.0);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-400+x[l], 21+y[l], -400+z[l]);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-400+x[l], 21+y[l], 400+z[l]);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(400+x[l], 21+y[l], 400+z[l]);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(400+x[l], 21+y[l], -400+z[l]);
    glEnd();

    display_cart(RCState.plaatje);

    plaatje2 = RCState.plaatje + 40;
    if (plaatje2 >= tot)
	plaatje2 -= tot;
    display_cart(plaatje2);

    plaatje2 = RCState.plaatje + 20;
    if (plaatje2 >= tot)
	plaatje2 -= tot;
    display_cart(plaatje2);

    plaatje2 = RCState.plaatje - 20;
    if (plaatje2 < 0)
	plaatje2 += tot;
    display_wheel(plaatje2);
    glPopMatrix();

    if (draw_stats) {
      glShadeModel (GL_FLAT);
      glColor3f(1.0,1.0,1.0);
      veiGlPushTextMode(w);
      veiGlDrawStatistics(NULL,0);
      veiGlPopTextMode();
      glShadeModel (GL_SMOOTH);
    }
}

void setupwin(VeWindow *win) {
  glNewList(1, GL_COMPILE);
  do_display();
  glEndList();

  glShadeModel (GL_SMOOTH);
  glFrontFace(GL_CCW);
  glEnable(GL_DEPTH_TEST);

    glClearColor(fogcol[0], fogcol[1], fogcol[2], fogcol[3]);
    glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightamb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightdif);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glColor3f(1.0, 1.0, 1.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

#ifdef FOG
/* fog */
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogcol);
    glFogf(GL_FOG_DENSITY, 0.001);
    glFogf(GL_FOG_START, 20.0);
    glFogf(GL_FOG_END, 55.0);
    glHint(GL_FOG_HINT, GL_NICEST);
#endif

    make_texture();
    init_wheel();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
/*
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE,
	&texture[0][0][0]);
*/
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

/* ARGSUSED1 */
int parsekey(VeDeviceEvent *e, void *arg)
{
  VeDeviceE_Keyboard *kbd;

  if (VE_EVENT_TYPE(e) != VE_ELEM_KEYBOARD)
    return 0;
  kbd = VE_EVENT_KEYBOARD(e);
  if (!kbd->state)
    return 0;
  if (strcmp(e->elem,"stop") == 0)
    RCState.speed = 0;
  switch (kbd->key) {
  case 'R': /* restart */
  case 'r': 
    RCState.plaatje = 0.0;
    RCState.speed = 0.0;
    RCState.frame = 0;
    RCState.angle = 0;
    RCState.angle2 = 0.0;
    RCState.angle3 = 0.0;
    break;
  }
  return 0;
}

/* This almost certainly isn't right... */
void altSetCamera(void) {
  VeVector3 u,v;
  float plaatje2;
  int l,l2,i;
  l = RCState.plaatje;
  plaatje2 = RCState.plaatje + 10;
  
  if (plaatje2 >= tot)
    plaatje2 -= tot;
  l2 = plaatje2;

  vePackVector(&v,x[l]+3*dx[l],y[l]+3*dy[l],z[l]+3*dz[l]);
  vePackVector(&u,x[l2]+3*dx[l],y[l2]+3*dy[l],z[l2]+3*dz[l]);
  for(i = 0; i < 3; i++) {
    u.data[i] -= v.data[i];
    /* push viewer back into the car */
    v.data[i] -= u.data[i]*0.8;
  }
  veGetOrigin()->loc = v;
  veGetOrigin()->dir = u;
  vePackVector(&(veGetOrigin()->up),dx[l],dy[l],dz[l]);
}


void Animate(void *v)
{
    int l1;

    l1 = RCState.plaatje;
    RCState.speed += (y[l1] - y[l1+4])*2-0.005;

    RCState.speed -= (fabs(rl[l1]-al[l1]) + fabs(pt[l1]) + fabs(hd[l1])) * RCState.speed/200;

    if (RCState.frame > 450)
	RCState.speed -= 0.2208;

    if (RCState.speed < 0)
	RCState.speed = 0;

    if (RCState.frame < 10)
	RCState.speed = 0;

    if (RCState.frame == 10)
	RCState.speed = 4.5;

    if (RCState.frame > 155 && RCState.frame < 195)
	RCState.speed = 7.72;

    RCState.plaatje += RCState.speed;

    if (RCState.plaatje >= tot) {
	RCState.plaatje = 0;
	RCState.frame = 0;
    } else {
      if (RCState.plaatje < 0)
	RCState.plaatje += tot;
      
      altSetCamera();
      
      RCState.angle2 = RCState.frame*4*M_PI/503;
      RCState.angle3 = RCState.frame*6*M_PI/503;
      vePostRedisplay();
      RCState.frame++;
    }
    veAddTimerProc(FRAME_INTERVAL,Animate,v);
}

void myReshape(int w, int h)
{
  altSetCamera();
  glViewport (0, 0, w, h);
}

int exitcback(VeDeviceEvent *e, void *arg) {
  exit(0);
  return 0;
}

int main(int argc, char *argv[])
{
    char *c;
    extern void calculate_rc(void);

    veInit(&argc,argv);

    calculate_rc();

    veSetOption("depth","1");
    veSetOption("doublebuffer","1");
    veiGlSetDisplayCback(display);
    veiGlSetWindowCback(setupwin);

    veDeviceAddCallback(parsekey,NULL,"keyboard");
    veDeviceAddCallback(exitcback,NULL,"exit");
    veAddTimerProc(0,Animate,0);

    veMPAddStateVar(DATA_STATE,&RCState,sizeof(RCState),VE_MP_AUTO);

    if (c = veGetOption("draw_stats"))
      draw_stats = atoi(c);

    veRun();
    return 0;             /* ANSI C requires main to return int. */
}
