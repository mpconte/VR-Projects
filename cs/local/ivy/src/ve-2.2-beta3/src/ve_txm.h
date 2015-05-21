#ifndef VE_TXM

#include <txm.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** misc
    This module provides a slightly simpler wrapper around the
    TXM (Texture Manager) library.  The TXM library has now been
    integrated into VE.  As such, some aspects are simpler (e.g.
    VE works out what renderer driver is needed).  The raw "txm"
    functions are still available, but for simplicitly, the following
    wrappers are provided which assume the presence of a single manager.
*/

int veTXMReserveId(void);
int veTXMPreloadContext(void); /* goes through and loads all 
				  unloaded textures
				  in the current context */
int veTXMBindTexture(int id); /* bind texture in the current
				 context */
int veTXMBoundTexture(void); /* returns id of bound texture - 0 if
				none */
int veTXMReloadTexture(int id); /* force texture to be reloaded
				   in all contexts (needed if
				   you change it) */
TXTexture *veTXMLoadFile(char *fname, char *ffmt, int flags);
int veTXMAddTexFile(char *file, char *ffmt, int flags);
int veTXMAddTexFile2(char *file, char *ffmt,
		   char *tfile, char *tfmt, int flags);
int veTXMAddTexture(void *data, int type, int width, int height,
		  int ncomp, int flags);
int veTXMDelTexture(int id);
TXTexture *veTXMLookupTexture(int id);

/* option had better specify format */
int veTXMSetOption(int id, int optset, int opt, ...);

/* find a renderer and initialize internals */
void veTXMInit(void);

/* following must be provided by driver */
TXRenderer *veTXMImplRenderer(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VE_TXM_H */
