/* wrapper around txm */
#include "autocfg.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ve.h>

#define MODULE "ve_txm"

int veTXMReserveId(void) { return txmReserveId(NULL); }
int veTXMPreloadContext(void) { return txmPreloadContext(NULL); }
int veTXMBindTexture(int id) { return txmBindTexture(NULL,id); }
int veTXMBoundTexture(void) { return txmBoundTexture(NULL); }
int veTXMReloadTexture(int id) { return txmReloadTexture(NULL,id); }
TXTexture *veTXMLoadFile(char *fname, char *ffmt, int flags) {
  return txmLoadFile(fname,ffmt,flags);
}
int veTXMAddTexFile(char *file, char *ffmt, int flags) {
  return txmAddTexFile(NULL,file,ffmt,flags);
}
int veTXMAddTexFile2(char *file, char *ffmt,
		     char *tfile, char *tfmt, int flags) {
  return txmAddTexFile2(NULL,file,ffmt,tfile,tfmt,flags);
}
int veTXMAddTexture(void *data, int type, int width, int height,
		    int ncomp, int flags) {
  return txmAddTexture(NULL,data,type,width,height,ncomp,flags);
}
int veTXMDelTexture(int id) {
  return txmDelTexture(NULL,id);
}
TXTexture *veTXMLookupTexture(int id) {
  return txmLookupTexture(NULL,id);
}

/* option had better specify format */
int veTXMSetOption(int id, int optset, int opt, ...) {
  int res;
  va_list ap;
  va_start(ap,opt);
  res = txmSetOptionV(NULL,id,optset,opt,ap);
  va_end(ap);
  return res;
}

/* find a renderer and initialize internals */
void veTXMInit(void) {
  static TXRenderer *(*f)(void);
  /* want exactly one txmrender driver */
  if (!(f = (TXRenderer *(*)(void))veFindDynFunc("veTXMImplRenderer"))) {
    if (veDriverRequire("txmrender","*"))
      veFatalError(MODULE,"no texture manager support available");
  }
  if (!(f = (TXRenderer *(*)(void))veFindDynFunc("veTXMImplRenderer"))) {
    veFatalError(MODULE,"bogus texture manager driver - missing veTXMImplRenderer()");
  }
  txmSetRenderer(NULL,f());
}

