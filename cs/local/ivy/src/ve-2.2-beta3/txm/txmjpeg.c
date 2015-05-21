#include "autocfg.h"
#ifdef HAS_JPEGLIB
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>
#include "txm.h"

typedef unsigned char byte_t;

static void
jpg_noop(j_decompress_ptr cinfo)
{
}

static boolean
jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    fprintf(stderr,"Premature end of jpeg file\n");
    return TRUE;
}

static void
jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{

    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
    if (cinfo->src->bytes_in_buffer < 0)
        fprintf(stderr,"Premature end of jpeg file\n");
}

static void
jpeg_mem_src(j_decompress_ptr cinfo, byte_t *mem, int len)
{
    cinfo->src = (struct jpeg_source_mgr *)
        (*cinfo->mem->alloc_small)((j_common_ptr) cinfo,
                                   JPOOL_PERMANENT,
                                   sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_noop;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_noop;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

/* basic methodology for jpeglib snarfed from elsewhere */
TXTexture *txmLoadJPEG(FILE *f, int flags) {
  TXTexture *tx;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  byte_t *imgbuf;
  byte_t *img, *c;
  int jpglen;

  fseek(f,0,SEEK_END);
  jpglen = ftell(f);
  rewind(f);
    
  imgbuf = malloc(jpglen);
  assert(imgbuf != NULL);
  if (fread(imgbuf,jpglen,1,f) != 1) {
    free(imgbuf);
    return NULL;
  }

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, (byte_t *) imgbuf, jpglen);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  if (cinfo.output_components != 3) {
    free(imgbuf);
    fprintf(stderr,"txmLoadJPEG: bad number of jpg components - expected 3, got %d\n", cinfo.output_components);
    return NULL;
  }

  img = (byte_t *) malloc(cinfo.output_width * cinfo.output_height * 3);
  if (flags & TXM_INVERT)
    c = img;
  else
    c = img + (cinfo.output_height-1)*cinfo.output_width*3;
  assert(img != NULL);
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, &c, 1);
    if (flags & TXM_INVERT)
      c += cinfo.output_width * 3;
    else
      c -= cinfo.output_width * 3;
  }

  tx = txmCreateTex();
  tx->data = img;
  tx->width = cinfo.output_width;
  tx->height = cinfo.output_height;
  tx->type = TXM_UBYTE;
  tx->ncomp = 3;

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  free(imgbuf);
  return tx;
}
#endif /* HAS_JPEGLIB */
