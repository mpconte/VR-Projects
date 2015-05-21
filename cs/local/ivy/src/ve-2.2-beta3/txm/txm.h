#ifndef TXM_H
#define TXM_H

#include <stdarg.h>

/* TXM is a texture manager - it's job is to keep textures and try
   to minimimze texture usage.  There is also an OpenGL layer that
   will load them in at render time as necessary.  TXM also handles
   reading textures in various formats.  The manager will also deal
   with textures in different contexts. */

/* There is a default global texture manager.  Other managers can be
   specified if you want to instantiate them specifically. */

/* Textures are referenced by a numeric identifier.  All valid identifiers
   are > 0. */

/* This version of TXM has been merged into the VE setup */

/* data formats */
#define TXM_UBYTE   (0)
#define TXM_USHORT  (1)
#define TXM_UINT    (2)
#define TXM_ULONG   (3)
#define TXM_FLOAT   (4)

/* manager flags */
#define TXM_MF_SHARED_IDS    (1<<0)  /* 1 = use same ids in all graphics
					contexts - for this to work correctly
					apps should use the txmReserveId call
					and *not* manage ids at the application
					level
					0 = assign ids dynamically in each
					context.
				     */

/* texture option formats */
#define TXM_OPT_INT    (0) /* int */
#define TXM_OPT_FLOAT  (1) /* float */
#define TXM_OPT_FLOAT2 (2) /* float * */
#define TXM_OPT_FLOAT3 (3) /* float * */
#define TXM_OPT_FLOAT4 (4) /* float * */

typedef struct txtexopt {
  int fmt;
  int opt_set, opt_id;
  union {
    int i;
    float f[4];
  } data;
  struct txtexopt *next;
} TXTexOpt;

/* known TXM option sets */
#define TXM_OPTSET_TXM     (0)
#define TXM_OPTSET_OPENGL  (1)

/* TXM texture options */
#define TXM_AUTOMIPMAP     (0)

typedef struct txtexture {
  void *data;
  int type;
  int ncomp;
  int width;
  int height;
  unsigned long cksum; /* used to help in texture comparisons */
  struct txtexopt *options;
  int unique;
} TXTexture;

typedef struct {
  int optset;
  int opt;
  int fmt;
} TXOption;

/* our interface to the underlying renderer -
   different managers can use different renderers */
typedef struct txrenderer {
  int (*reserve)(void);  /* returns a unique id in the current context */
  int (*ctxid)(void);    /* returns the number of the current context,
			    must be contiguous starting from 0 and increasing
			    (i.e. 0, 1, 2, ...) */
  int (*load)(int id, TXTexture *t); /* loads the given texture as
					id "id" in the current context */
  int (*unload)(int id); /* unloads texture id from the current context */
  int (*bind)(int id);   /* binds to this id so that subsequent 
			    renderer texture options use this texture
			    (e.g. glBindTexture...) */
  TXOption *options;
} TXRenderer;


typedef struct txcontext {
  int ntex; /* number of textures considered in this context */
  int *ids;    /* maps a texture's id to its id in the current context */
  /* if non-zero, then this texture has not been assigned an id in this
     context */
  int *loaded; /* if non-zero then the texture is loaded in this 
		  context */
  int space; /* space allocated in arrays */
  int bound; /* currently bound texture */
} TXContext;

typedef struct txmanager {
  int flags;
  TXTexture **textures;
  int ntextures, texspace;
  TXContext **contexts;
  int ncontexts, ctxspace;
  TXRenderer *renderer;
} TXManager;

/* other flags */
/* do not try to reuse the texture */
#define TXM_UNIQUE   (1<<0)
/* redefine vertical order - applies when reading from file */
#define TXM_INVERT   (1<<1)

TXManager *txmCreateMgr(void);
void txmDestroyMgr(TXManager *mgr);
TXContext *txmCreateCtx(void);
void txmDestroyCtx(TXContext *ctx);
TXTexture *txmCreateTex(void);
void txmDestroyTex(TXTexture *tex);
int txmGetMgrFlags(TXManager *mgr);
void txmSetMgrFlags(TXManager *mgr, int flags);
void txmClearMgrFlags(TXManager *mgr, int flags);
void txmSetRenderer(TXManager *mgr, TXRenderer *rd);
int txmReserveId(TXManager *mgr);
int txmPreloadContext(TXManager *mgr); /* goes through and loads all 
					  unloaded textures
					  in the current context */
int txmBindTexture(TXManager *mgr, int id); /* bind texture in the current
					       context */
int txmBoundTexture(TXManager *mgr); /* returns id of bound texture - 0 if
					none */
int txmReloadTexture(TXManager *mgr, int id); /* force texture to be reloaded
						 in all contexts (needed if
						 you change it) */
TXTexture *txmLoadFile(char *fname, char *ffmt, int flags);
int txmAddTexFile(TXManager *mgr, char *file, char *ffmt, int flags);
int txmAddTexFile2(TXManager *mgr, char *file, char *ffmt,
		   char *tfile, char *tfmt, int flags);
int txmAddTexture(TXManager *mgr, void *data, int type, int width, int height,
		  int ncomp, int flags);
int txmDelTexture(TXManager *mgr, int id);
TXTexture *txmLookupTexture(TXManager *mgr, int id);

/* option had better specify format */
int txmSetOption(TXManager *mgr, int id, int optset, int opt, ...);
int txmSetOptionV(TXManager *mgr, int id, int optset, int opt, va_list ap);

TXTexOpt *txmIntGetOption(TXTexture *tex, int optset, int opt);

/*
  The following is now a no-op - it is provided for backwards-compatibility.
  VE is now responsible for initializing the renderer.
*/
TXRenderer *txmOpenGLRenderer(void);


#endif /* TXM_H */
