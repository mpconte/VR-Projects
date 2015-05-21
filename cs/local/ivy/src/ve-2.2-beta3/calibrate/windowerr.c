#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "calibrate.h"

static float current_col[3] = { 1.0, 0.0, 0.0 };
static float current_bord[3] = { 1.0, 1.0, 1.0 };
static float wall_col[3] = { 0.0, 0.6, 0.0 };
static float wall_bord[3] = { 0.0, 0.6, 0.6 };
static float current_tex[3] = { 1.0, 1.0, 1.0 };
static float wall_tex[3] = { 0.0, 0.6, 0.0 };

float grid_increment = 0.1;
static float adjust_increment = 0.01;
float cube_size = 5.0;

char *cube_texture = "pattern.ppm";
char *check_texture = "check.ppm";
/* keep these in check with other files by hend... */
GLuint cubetex = 1, checktex = 2;

static void draw_data(VeWindow *w) {
  char str[80];
  int incr, roff = 15,x,y;
  
#if 0
  x = y = roff;
  incr = veiGlStringHeight(data_font,"X");
  veiGlPushTextMode(w);
  glColor3f(1.0,1.0,1.0);
  /* offset a bit to ensure that we are in view... */
  sprintf(str, "Wall: %s", w->wall->name);
  glRasterPos2i(x,y); y += incr;
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  sprintf(str, "Window: %s", w->name);
  glRasterPos2i(x,y); y += incr;
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  sprintf(str, "Centre Offset: (%2.4f,%2.4f)", w->offset_x, w->offset_y);
  glRasterPos2i(x,y); y += incr;
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  sprintf(str, "Size Offset: (%2.4f,%2.4f)", w->width_err, w->height_err);
  glRasterPos2i(x,y); y += incr;
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  sprintf(str, "Resolution: %g", adjust_increment);
  glRasterPos2i(x,y); y += incr;
  veiGlDrawString(data_font,str,VEI_GL_LEFT);
  veiGlPopTextMode();
#endif
}

/* Simple texture loader for P6 PPM files */
static unsigned char *load_ppm_texture(FILE *f, int *width_r, int *height_r) {
  /* textures should be in raw (P6) PPM format*/
  int width, height, maxval, i;
  unsigned char *data = NULL;

  /* check for header*/
  if (fgetc(f) != 'P' || fgetc(f) != '6' || fgetc(f) != '\n') {
    fprintf(stderr, "load_ppm_texture: file does not have 'P6' PPM header\n");
    goto load_err;
  }
  
  {
    int *vals[3];
    int c;

    vals[0] = &width; vals[1] = &height; vals[2] = &maxval;
    for(i = 0; i < 3; i++) {
      /* skip any leading crap */
      c = getc(f);
      while (isspace(c) || c == '#') {
        if (c == '#') {
          while (c != EOF && c != '\n')
            c = getc(f);
        }
        if (c != EOF)
          c = getc(f);
      }
      ungetc(c,f);
      if (fscanf(f,"%d",vals[i]) != 1) {
        fprintf(stderr, "load_ppm_texture: file has corrupt or missing PPM header\n");
        goto load_err;
      }
    }
  }
  
  if (!isspace(fgetc(f))) {
    fprintf(stderr, "load_ppm_texture: file is missing newline/whitespace after PPM header\n");
    goto load_err;
  }
  data = malloc(width*height*3*sizeof(unsigned char));
  assert(data != NULL);
  if (fread(data,3*sizeof(unsigned char),width*height,f) != width*height) {
    fprintf(stderr, "load_ppm_texture: truncated PPM file\n");
    goto load_err;
  }
  if (maxval != 255) {
    /* scale it to fit in a byte */
    for(i = 0; i < width*height*3; i++)
      data[i] = (unsigned char)((float)data[i]*255.0/(float)maxval);
  }
  if (width_r)
    *width_r = width;
  if (height_r)
    *height_r = height;
  return data;

 load_err:
  if (data)
    free(data);
  return NULL;
}

int load_and_def_texture(char *file) {
  int width, height;
  FILE *f = NULL;
  unsigned char *data = NULL;

  if (!file) {
    fprintf(stderr, "load_and_def_texture: no texture file defined\n");
    goto load_and_def_err;
  }

  if (!(f = fopen(file,"r"))) {
    fprintf(stderr, "load_and_def_texture: could not load texture file %s\n",
            file);
    goto load_and_def_err;
  }

  if (!(data = load_ppm_texture(f,&width,&height)))
    goto load_and_def_err;

  fclose(f);
  f = NULL;

  gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA,width,height,GL_RGB,
		    GL_UNSIGNED_BYTE,data);

  free(data);
  data = NULL;

  return 0;

 load_and_def_err:
  if (f)
    fclose(f);
  if (data)
    free(data);
  return -1;
}

void windowerr_setup(VeWindow *w) {
  glShadeModel(GL_FLAT);
  glClearColor(0,0,0,0);
  glBindTexture(GL_TEXTURE_2D,cubetex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  if (load_and_def_texture(cube_texture))
    exit(1);

  glBindTexture(GL_TEXTURE_2D,checktex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  if (load_and_def_texture(check_texture))
    exit(1);
  glBindTexture(GL_TEXTURE_2D,0);

}

static void draw_cube(float f) {
  glBegin(GL_QUADS);

  glTexCoord2f(0,0);
  glVertex3f(-f,-f,-f);
  glTexCoord2f(1,0);
  glVertex3f(-f,-f,f);
  glTexCoord2f(1,1);
  glVertex3f(f,-f,f);
  glTexCoord2f(0,1);
  glVertex3f(f,-f,-f);

  glTexCoord2f(1,1);
  glVertex3f(f,-f,f);
  glTexCoord2f(0,1);
  glVertex3f(f,-f,-f);
  glTexCoord2f(0,0);
  glVertex3f(f,f,-f);
  glTexCoord2f(1,0);
  glVertex3f(f,f,f);

  glTexCoord2f(0,0);
  glVertex3f(f,f,-f);
  glTexCoord2f(1,0);
  glVertex3f(f,f,f);
  glTexCoord2f(1,1);
  glVertex3f(-f,f,f);
  glTexCoord2f(0,1);
  glVertex3f(-f,f,-f);

  glTexCoord2f(1,1);
  glVertex3f(-f,f,f);
  glTexCoord2f(0,1);
  glVertex3f(-f,f,-f);
  glTexCoord2f(0,0);
  glVertex3f(-f,-f,-f);
  glTexCoord2f(1,0);
  glVertex3f(-f,-f,f);

  glTexCoord2f(0,0);
  glVertex3f(-f,-f,-f);
  glTexCoord2f(1,0);
  glVertex3f(f,-f,-f);
  glTexCoord2f(1,1);
  glVertex3f(f,f,-f);
  glTexCoord2f(0,1);
  glVertex3f(-f,f,-f);

  glTexCoord2f(0,0);
  glVertex3f(-f,-f,f);
  glTexCoord2f(1,0);
  glVertex3f(f,-f,f);
  glTexCoord2f(1,1);
  glVertex3f(f,f,f);
  glTexCoord2f(0,1);
  glVertex3f(-f,f,f);
  glEnd();
}

static struct {
  char *name; int offs;
} adjust[] = {
  { "height", offsetof(VeWindow,height_err) },
  { "width", offsetof(VeWindow,width_err) },
  { "x", offsetof(VeWindow,offset_x) } ,
  { "y", offsetof(VeWindow,offset_y) },
  { NULL, 0 }
};

void windowerr_event(VeDeviceEvent *e) {
  VeWindow *w = current_window;
  float f = 0;
  char *s = NULL;
  int *v = NULL;
  int k;

  fprintf(stderr,"windowerr_event: %s.%s\n",e->device,e->elem);

  if (!w)
    return;
  /* deal with special cases first */
  if (strcmp(e->elem,"finer") == 0) {
    adjust_increment /= 10;
    return;
  } else if (strcmp(e->elem,"coarser") == 0) {
    adjust_increment *= 10;
    return;
  }

  if (strncmp(e->elem,"value_",6) == 0) {
    s = e->elem+6;
    f = veDeviceToValuator(e)*adjust_increment;
  } else if (strncmp(e->elem,"incr_",5) == 0) {
    if (!veDeviceToSwitch(e))
      return;
    s = e->elem+5;
    f = adjust_increment;
  } else if (strncmp(e->elem,"decr_",5) == 0) {
    if (!veDeviceToSwitch(e))
      return;
    s = e->elem+5;
    f = -adjust_increment;
  }

  if (!s)
    return; /* unrecognized */

  for(k = 0; adjust[k].name; k++) {
    if (strcmp(adjust[k].name,s) == 0) {
      float *v = (float *)((char *)w+adjust[k].offs);
      *v += f;
      return;
    }
  }
  /* if we get here, then we didn't find that name */
  /* but we don't care... */
}

void windowerr_display1(VeWindow *w, long tm, VeWallView *wv) {
  /* draw a grid in the wall, denote the centre */
  float x,y,wid,hgt,widerr,hgterr,ox,oy,mn,mx;
  float dm[16];
  int i,j;

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);

  wid = w->wall->view->width/2.0;
  hgt = w->wall->view->height/2.0;
  
  widerr = w->width_err/2.0;
  hgterr = w->height_err/2.0;
  ox = w->offset_x;
  oy = w->offset_y;

  printf("window %f x %f [%f,%f - %f,%f]\n",
	 wid, hgt, ox, oy, widerr, hgterr);

  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-wid-widerr+ox,wid+widerr+ox,
	  -hgt-hgterr+oy,hgt+hgterr+oy,
	  -1,1);
  glMatrixMode(GL_MODELVIEW);
  for(i = 0; i < 4; i++)
    for(j = 0; j < 4; j++)
      dm[i+j*4] = w->distort.data[i][j];
  glLoadMatrixf(dm);
  

  if (current_window == w)
    glColor3fv(current_col);
  else
    glColor3fv(wall_col);

  /* draw grid */
  glBegin(GL_LINES);
  mn = floor(-wid);
  while (mn < -wid)
    mn += grid_increment;
  mx = ceil(wid);
  while (mx > wid)
    mx -= grid_increment;
  for(x = mn; x <= mx; x += grid_increment) {
    glVertex3f(x,-hgt,0);
    glVertex3f(x,hgt,0);
  }
  mn = floor(-hgt);
  while (mn < -hgt)
    mn += grid_increment;
  mx = ceil(hgt);
  while (mx > hgt)
    mx -= grid_increment;
  for(y = mn; y <= mx; y += grid_increment) {
    glVertex3f(-wid,y,0);
    glVertex3f(wid,y,0);
  }
  glEnd();

  /* draw border */
  if (current_window == w)
    glColor3fv(current_bord);
  else
    glColor3fv(wall_bord);

  glBegin(GL_LINE_LOOP);
  glVertex3f(-wid,-hgt,0);
  glVertex3f(wid,-hgt,0);
  glVertex3f(wid,hgt,0);
  glVertex3f(-wid,hgt,0);
  glEnd();
  glBegin(GL_LINES);
  glVertex3f(-wid*0.1,0,0);
  glVertex3f(wid*0.1,0,0);
  glVertex3f(0,-hgt*0.1,0);
  glVertex3f(0,hgt*0.1,0);
  glEnd();

  draw_data(w);
}

void windowerr_display2(VeWindow *w, long tm, VeWallView *wv) {
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glBindTexture(GL_TEXTURE_2D,1);

  if (current_window == w)
    glColor3fv(current_tex);
  else
    glColor3fv(wall_tex);
  
  draw_cube(cube_size);
  glTranslatef(0.0,0.0,-3.0);
  glRotatef(45.0,0.0,1.0,0.0);
  glRotatef(45.0,1.0,0.0,0.0);
  glColor3fv(current_tex);
  draw_cube(0.3);

  draw_data(w);
}

void windowerr_display3(VeWindow *w, long tm, VeWallView *wv) {
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glBindTexture(GL_TEXTURE_2D,2);

  if (current_window == w)
    glColor3fv(current_tex);
  else
    glColor3fv(wall_tex);
  
  draw_cube(cube_size);
  glTranslatef(0.0,0.0,-3.0);
  glRotatef(45.0,0.0,1.0,0.0);
  glRotatef(45.0,1.0,0.0,0.0);
  glColor3fv(current_tex);
  draw_cube(0.3);

  draw_data(w);
}
