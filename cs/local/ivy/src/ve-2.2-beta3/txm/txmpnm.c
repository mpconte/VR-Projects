/* Support for PGM/PPM files */
/* Simple texture loader for P6 PPM files */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "txm.h"

#define rowoffs(x) ((x)*compp*width)
#define coloffs(x) ((x)*compp)
TXTexture *txmLoadPNM(FILE *f, int flags) {
  /* textures should be in raw (P6) PPM or (P5) PGM format*/
  int width, height, maxval, i, compp, row, ptype;
  unsigned char *data = NULL, *idata = NULL;
  TXTexture *tx;

  /* check for header*/
  if (fgetc(f) != 'P' ||
      (((ptype = fgetc(f)) != '6') && ptype != '5') ||
      fgetc(f) != '\n') {
    goto load_ppm_err;
  }

  switch (ptype) {
  case '5':
    compp = 1;
    break;
  case '6':
    compp = 3;
    break;
  default:
    goto load_ppm_err;
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
        goto load_ppm_err;
      }
    }
  }

  if (!isspace(fgetc(f))) {
    goto load_ppm_err;
  }

  data = malloc(width*height*compp);
  assert(data != NULL);

  if (fread(data,compp*sizeof(unsigned char),width*height,f) !=
      width*height) {
    goto load_ppm_err;
  }
  f = NULL;

  if (maxval != 255) {
    /* scale it to fit in a byte */
    for(i = 0; i < width*height*compp; i++)
      data[i] = (unsigned char)((float)data[i]*255.0/(float)maxval);
  }

  /* now, some nastiness - the first row in the PPM file is
     the "top" of the file, but the first row of the buffer
     should be the "bottom" of the image */
  if (!(flags & TXM_INVERT)) {
    idata = malloc(width*height*compp);
    assert(idata != NULL);
    for(row = 0; row < height; row++) {
      memcpy(idata+rowoffs(row),
	     data+rowoffs(height-row-1),
	     width*compp);
    }
    free(data);
    data = idata;
  }

  tx = txmCreateTex();
  tx->data = data;
  tx->width = width;
  tx->height = height;
  tx->type = TXM_UBYTE;
  tx->ncomp = compp;
  return tx;

 load_ppm_err:
  if (data)
    free(data);
  if (idata)
    free(idata);
  return NULL;
}
