#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "txm.h"

typedef unsigned char byte_t;
typedef unsigned short ushort_t;

static void swapshort(unsigned short *s) {
  static int do_swap = -1;
  if (do_swap < 0) {
    /* decide */
    union {
      unsigned long x;
      char c[4];
    } etest;
    etest.x = 0x11223344;
    if (etest.c[0] == 0x11)
      do_swap = 1;
    else
      do_swap = 0;
  }
  if (do_swap)
    (*s) = (((*s)>>8)&0xff)|(((*s)<<8)&0xff00);
}

/* code stolen from the "cq3" package - take with plenty o' salt */
TXTexture *txmLoadTGA(FILE *f, int flags) {
  struct tgaheader
  {
    byte_t idlen;
    byte_t cmtype;
    byte_t imgtype;
    byte_t cmspec[5];
    ushort_t xorig, yorig;
    ushort_t width, height;
    byte_t pixsize;
    byte_t imgdesc;
  } *tgahead;
  byte_t *img, *tga, *tgacur, *tgaend, *imgbuf;
  int tgalen, len, depth = 0;
  TXTexture *tx;

  fseek(f,0,SEEK_END);
  tgalen = ftell(f);
  rewind(f);
    
  imgbuf = malloc(tgalen);
  assert(imgbuf != NULL);
  if (fread(imgbuf,tgalen,1,f) != 1) {
    free(imgbuf);
    return NULL;
  }

  tga = (byte_t*)imgbuf;
  tgaend = tga + tgalen;
    
  tgahead = (struct tgaheader *)tga;
  swapshort(&tgahead->xorig);
  swapshort(&tgahead->yorig);
  swapshort(&tgahead->width);
  swapshort(&tgahead->height);
  printf("tga info: %d x %d (%d,%d)\n",
	 tgahead->width, tgahead->height, tgahead->xorig, tgahead->yorig);
  if (tgahead->imgtype != 2 && tgahead->imgtype != 10) {
    free(imgbuf);
    fprintf(stderr,"txmLoadTGA: bad tga image type\n");
    return NULL;
  }

  if (tgahead->pixsize == 24)
    depth = 3;
  else if (tgahead->pixsize == 32)
    depth = 4;
  else {
    free(imgbuf);
    fprintf(stderr,"txmLoadTGA: non 24 or 32 bit tga image\n");
    return NULL;
  }
    
  len = tgahead->width * tgahead->height * depth;
  img = (byte_t *) malloc(len);
  assert(img != NULL);

  tgacur = tga + sizeof(struct tgaheader) + tgahead->idlen;
  if (tgahead->imgtype == 10) {
    int i, j, packetlen;
    byte_t packethead;
    byte_t *c = img, *end = img + len;
    byte_t rlc[4];
        
    while (c < end) {
      packethead = *tgacur;
      if (++tgacur > tgaend) {
	free(img);
	free(imgbuf);
	fprintf(stderr,"Unexpected end of tga file");
	return NULL;
      }
      if (packethead & 0x80) {
	/* Run-length packet */
	packetlen = (packethead & 0x7f) + 1;
	memcpy(rlc, tgacur, depth);
	if ((tgacur += depth) > tgaend) {
	  free(img);
	  free(imgbuf);
	  fprintf(stderr,"Unexpected end of tga file");
	  return NULL;
	}
	for (j=0; j < packetlen; ++j)
	  for(i=0; i < depth; ++i)
	    *c++ = rlc[i];
      } else {
	/* Raw data packet */
	packetlen = packethead + 1;
	memcpy(c, tgacur, depth * packetlen);
	if ((tgacur += depth * packetlen) > tgaend) {
	  free(img);
	  free(imgbuf);
	  fprintf(stderr,"Unexpected end of tga file");
	  return NULL;
	}
	c += packetlen * depth;
      }
    }

    if (!(flags & TXM_INVERT)) {
      /* Flip image in y */
      int i, linelen;
      byte_t *temp;
            
      linelen = tgahead->width * depth;
      temp = (byte_t *) malloc(linelen);
      for (i=0; i < tgahead->height/2; ++i) {
	memcpy(temp, &img[i * linelen], linelen);
	memcpy(&img[i * linelen], &img[(tgahead->height - i - 1)
				      * linelen], linelen);
	memcpy(&img[(tgahead->height - i - 1) * linelen], temp,
	       linelen);
      }
      free(temp);
    }       
  } else {
    int i, linelen;
    
    if (tgaend - tgacur + 1 < len) {
      free(img);
      free(imgbuf);
      fprintf(stderr,"txmLoadTGA: bad tga image data length\n");
      return NULL;
    }

    /* Flip image in y */
    linelen = tgahead->width * depth;
    for (i=0; i < tgahead->height; ++i)
      memcpy(&img[i * linelen],
	     &tgacur[(tgahead->height - i - 1) * linelen], linelen);
  }

  /* Exchange B and R to get RGBA ordering */
  {
    int i;
    byte_t temp;

    for (i=0; i < len; i += depth) {
      temp = img[i];
      img[i] = img[i+2];
      img[i+2] = temp;
    }
  }

  tx = txmCreateTex();
  tx->data = img;
  tx->width = tgahead->width;
  tx->height = tgahead->height;
  tx->type = TXM_UBYTE;
  tx->ncomp = depth;
  free(imgbuf);
  return tx;
}  
