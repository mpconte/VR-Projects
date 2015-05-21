/* All GL-based fonts are described here - if you add a built-in font,
   you should define it here */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ve_thread.h>
#include <ve_error.h>
#include <ve_stats.h>
#include <ve_render.h>
#include <vei_gl.h>

#define MODULE "vei_gl_font"

extern VeiGlBitmapFont Vei_GlFont_9x15, Vei_GlFont_8x13,
  Vei_GlFont_7x13, Vei_GlFont_6x10;
static VeiGlFont font_list[] = {
  { "6x10", VEI_GL_FONT_BITMAP, &Vei_GlFont_6x10 },
  { "7x13", VEI_GL_FONT_BITMAP, &Vei_GlFont_7x13 },
  { "8x13", VEI_GL_FONT_BITMAP, &Vei_GlFont_8x13 },
  { "9x15", VEI_GL_FONT_BITMAP, &Vei_GlFont_9x15 },
  { NULL, VEI_GL_FONT_NONE, NULL }
};

VeiGlFont *veiGlGetFont(char *name) {
  int i;
  for(i = 0; font_list[i].name; i++)
    if (strcmp(font_list[i].name,name) == 0)
      return &font_list[i];
  return NULL;
}

/* This code pretty much borrowed directly from GLUT-3.7
   Copyright (c) Mark J. Kilgard, 1994. */
static void draw_bitmap_str(VeiGlBitmapFont *font, char *str) {
  GLint swapbytes, lsbfirst, rowlength;
  GLint skiprows, skippixels, alignment;
  const VeiGlBitmapChar *ch;
  int c;

  /* Save current modes. */
  glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapbytes);
  glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlength);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippixels);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);

  /* Little endian machines (DEC Alpha for example) could
     benefit from setting GL_UNPACK_LSB_FIRST to GL_TRUE
     instead of GL_FALSE, but this would require changing the
     generated bitmaps too. */
  glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
  glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  while (*str) {
    c = (int)*((unsigned char *)str);
    if (c < font->first || c >= font->first + font->num_chars)
      continue;
    ch = font->ch[c - font->first];
    if (ch) {
      glBitmap(ch->width, ch->height, ch->xorig, ch->yorig,
	       ch->advance, 0, ch->bitmap);
    }
    str++;
  }
  
  /* Restore saved modes. */
  glPixelStorei(GL_UNPACK_SWAP_BYTES, swapbytes);
  glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlength);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippixels);
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}

void veiGlPushTextMode(VeWindow *win) {
  int w,h;
  veRenderGetWinInfo(win,NULL,NULL,&w,&h);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glRasterPos2i(w/2,h/2);
}

void veiGlPopTextMode() {
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

/* We only support ASCII here - oooh evil */
void veiGlDrawChar(VeiGlFont *font, int c) {
  char s[2];
  if (font == NULL)
    font = &(font_list[0]); /* find some default */
  *((unsigned char *)s) = (unsigned char)(c&0xff);
  s[1] = '\0';
  switch(font->type) {
  case VEI_GL_FONT_BITMAP:
    draw_bitmap_str((VeiGlBitmapFont *)(font->font),s);
    break;
  default:
    /* this space intentionally left blank */;
  }
}

int veiGlStringHeight(VeiGlFont *font, char *str) {
  VeiGlBitmapFont *fn;

  if (font == NULL)
    font = &(font_list[0]); /* find some default */

  switch(font->type) {
  case VEI_GL_FONT_BITMAP:
    fn = (VeiGlBitmapFont *)(font->font);
    return fn->line_height;
  default:
    /* this space intentionally left blank */;
  }
  return 0;
}

int veiGlStringWidth(VeiGlFont *font, char *str) {
  int w = 0;
  int c;
  VeiGlBitmapFont *fn;
  VeiGlBitmapChar *ch;

  if (font == NULL)
    font = &(font_list[0]); /* find some default */

  switch(font->type) {
  case VEI_GL_FONT_BITMAP:
    fn = (VeiGlBitmapFont *)(font->font);
    while (*str) {
      c = (int)*((unsigned char *)str);
      if (c < fn->first || c >= fn->first + fn->num_chars)
	continue;
      ch = (VeiGlBitmapChar *)(fn->ch[c - fn->first]);
      if (ch)
	w += ch->advance;
      str++;
    }
    break;
  default:
    /* this space intentionally left blank */;
  }
  return w;
}

void veiGlDrawString(VeiGlFont *font, char *str, VeiGlJustify justify) {
  if (font == NULL)
    font = &(font_list[0]); /* find some default */

  switch(justify) {
  case VEI_GL_LEFT:
    break;
  case VEI_GL_RIGHT:
    glBitmap(0,0,0,0,-veiGlStringWidth(font,str),0,NULL);
    break;
  case VEI_GL_CENTER:
    glBitmap(0,0,0,0,-veiGlStringWidth(font,str)/2,0,NULL);
    break;
  }

  switch(font->type) {
  case VEI_GL_FONT_BITMAP:
    draw_bitmap_str((VeiGlBitmapFont *)(font->font),str);
    break;
  default:
    /* this space intentionally left blank */;
  }
}

void veiGlDrawStatistics(VeiGlFont *font, int max) {
  VeStatisticList *l;
  int i, incr;
  GLboolean lighting, texture2d, depthtest;
  char str[200];
  
  glGetBooleanv(GL_LIGHTING,&lighting);
  glGetBooleanv(GL_TEXTURE_2D,&texture2d);
  glGetBooleanv(GL_DEPTH_TEST,&depthtest);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glColor3f(1.0,1.0,1.0);
  incr = veiGlStringHeight(font,"X");
  for(l = veGetStatistics(), i = 0; 
      l && (max <= 0 || i < max); 
      l = l->next, i++) {
    glRasterPos2i(0,i*incr+2);
    veStatToString(l->stat,str,200);
    veiGlDrawString(font,str,VEI_GL_LEFT);
  }
  if (lighting)
    glEnable(GL_LIGHTING);
  if (texture2d)
    glEnable(GL_TEXTURE_2D);
  if (depthtest)
    glEnable(GL_DEPTH_TEST);
}

