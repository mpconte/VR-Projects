#ifndef TXMF_H
#define TXMF_H

/* This is the "native" TXM file format */
#include <txm.h>

/* define some types for us here... */
#define txmf_uint32_t unsigned

/* Cute things to hide in side the code, part I:
   The TXMF header signature, when expressed in hex, represents
   Douglas Adams' birthday: March 11, 1952 */
#define TXMF_SIG 0x19520311

#define txmf_swap(x) ((((x)&0xffU)<<24)|(((x)&0xff00U)<<8)|(((x)&0xff0000U)>>8)|(((x)&0xff000000U>>8)))

typedef struct txmf_header{
  txmf_uint32_t sig;    /* must always match */
  txmf_uint32_t width;  /* width of image in pixels - must be power of 2 */
  txmf_uint32_t height; /* height of image in pixels - must be power of 2 */
  txmf_uint32_t ncomp;  /* number of components, >=1, <= 4 */
  txmf_uint32_t format; /* pixel format - currently only TXMF_UBYTE is
			   supported */
  txmf_uint32_t flags;  /* flags - see below */
} TXMFHeader;

#define TXMF_FMT_UBYTE (0)    /* currently the only supported format */

#define TXMF_F_MIPMAP (1<<0)  /* image is mipmapped - mipmaps are located
				 in the block that follows after the main
				 image itself */
#define TXMF_F_SWAPPED (1<<1) /* file is byte-swapped from local architecture
				 - this flag is computed when a file is
				   loaded - the value in the file should
				   always be ignored */

void *txmfReadFile(FILE *f, TXMFHeader *hdr);
void *txmfMipmap(TXMFHeader *hdr, int level);

/* okay - how do I load a *native* format into TXM?  It doesn't
   allow the loading of mipmaps directly right now... */

#endif /* TXMF_H */
