#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include "txm.h"

#if 0
#define DEBUG(x) dbgmsg x
#else
#define DEBUG(x)
#endif /* */

static void dbgmsg(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr,"txm debug: ");
  vfprintf(stderr,fmt,ap);
  fprintf(stderr,"\n");
  va_end(ap);
}

static void *n_alloc(int n) {
  void *v;
  v = calloc(1,n);
  assert(v != NULL);
  return v;
}
#define s_alloc(x) n_alloc(sizeof(x))
#define ckfree(x) { if(x) free(x); }

static TXManager *defaultManager = NULL;
static TXManager *getmgr(TXManager *m) {
  if (m)
    return m;
  if (!defaultManager)
    defaultManager = txmCreateMgr();
  return defaultManager;
}

static TXContext *getctx(TXManager *m) {
  int k;
  int n = m->ncontexts;
  if (!m && !(m = getmgr(m)))
    return NULL;
  if (!m->renderer || !m->renderer->ctxid)
    return NULL;
  if ((k = m->renderer->ctxid()) < 0)
    return NULL;
  if ((k < m->ncontexts) && m->contexts[k])
    return m->contexts[k];

  /* create a new context */

  /* grow the context space if necessary */
  if (k >= m->ncontexts) {
    m->ncontexts = k+1;
    if (m->ctxspace == 0) {
      m->ctxspace = 6;
      m->contexts = n_alloc(sizeof(TXContext *)*m->ctxspace);
    } else if (m->ncontexts >= m->ctxspace) {
      while (m->ncontexts >= m->ctxspace)
	m->ctxspace *= 2;
      m->contexts = realloc(m->contexts,sizeof(TXContext *)*m->ctxspace);
      assert(m->contexts != NULL);
    }
  }
  /* blank out new spaces */
  while (n < m->ncontexts)
    m->contexts[n++] = NULL;
  m->contexts[k] = txmCreateCtx();
  return m->contexts[k];
}

static int make_ctx_space(TXContext *ctx, int ind) {
  if (ind >= ctx->ntex) {
    int n = ctx->ntex;
    ctx->ntex = ind+1;
    if (ctx->space == 0) {
      ctx->space = (ctx->ntex < 16 ? 16 : ctx->ntex+1);
      ctx->ids = n_alloc(sizeof(int *)*ctx->space);
      ctx->loaded = n_alloc(sizeof(int *)*ctx->space);
    } else if (ctx->ntex >= ctx->space) {
      while (ctx->ntex >= ctx->space)
	ctx->space *= 2;
      ctx->ids = realloc(ctx->ids,sizeof(int *)*ctx->space);
      assert(ctx->ids != NULL);
      ctx->loaded = realloc(ctx->loaded,sizeof(int *)*ctx->space);
      assert(ctx->loaded != NULL);
    }
    while (n < ctx->ntex) {
      ctx->ids[n] = 0;
      ctx->loaded[n] = 0;
      n++;
    }
  }
  return 0;
}

static int typesize(int type) {
  switch (type) {
  case TXM_UBYTE:  return 1;
  case TXM_USHORT: return sizeof(short);
  case TXM_UINT:   return sizeof(int);
  case TXM_ULONG:  return sizeof(long);
  case TXM_FLOAT:  return sizeof(float);
  default:
    return 1;
  }
}

static float from_data(void *src, int type) {
  switch (type) {
  case TXM_UBYTE:  return (*((unsigned char *)src))/((float)255.0);
  case TXM_USHORT: return (*((unsigned short *)src))/((float)USHRT_MAX);
  case TXM_UINT:   return (*((unsigned *)src))/((float)UINT_MAX);
  case TXM_ULONG:  return (*((unsigned long *)src))/((float)ULONG_MAX);
  case TXM_FLOAT:  return (*((float *)src));
  default:
    return 0.0;
  }
}

static void to_data(void *dst, int type, float f) {
  switch (type) {
  case TXM_UBYTE:  (*(unsigned char *)dst) = (unsigned char)(f/255.0);
  case TXM_USHORT: (*(unsigned short *)dst) = (unsigned short)(f/USHRT_MAX);
  case TXM_UINT:   (*(unsigned int *)dst) = (unsigned int)(f/UINT_MAX);
  case TXM_ULONG:  (*(unsigned long *)dst) = (unsigned long)(f/ULONG_MAX);
  case TXM_FLOAT:  (*(float *)dst) = f;
  }
}

static void conv_data(void *dst, int dtype, void *src, int stype, int n) {
  char *dstp = (char *)dst, *srcp = (char *)src;
  int dsz = typesize(dtype);
  int ssz = typesize(stype);
  while (n > 0) {
    to_data(dstp,dtype,from_data(src,stype));
    dstp += dsz;
    srcp += ssz;
    n--;
  }
}

static int copy_data(void *dst, void *src, int type, int cnt) {
  int tsz = typesize(type);
  memcpy(dst,src,cnt*tsz);
}

TXManager *txmCreateMgr(void) {
  TXManager *m;
  m = s_alloc(TXManager);
  return m;
}

void txmDestroyMgr(TXManager *mgr) {
  int k;
  mgr = getmgr(mgr);
  if (mgr) {
    for(k = 0; k < mgr->ntextures; k++)
      txmDestroyTex(mgr->textures[k]);
    for(k = 0; k < mgr->ncontexts; k++)
      txmDestroyCtx(mgr->contexts[k]);
    free(mgr);
    if (mgr == defaultManager)
      defaultManager = NULL;
  }
}

int txmGetMgrFlags(TXManager *mgr) {
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  return mgr->flags;
}

void txmSetMgrFlags(TXManager *mgr, int flags) {
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  mgr->flags |= flags;
}

void txmClearMgrFlags(TXManager *mgr, int flags) {
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  mgr->flags &= ~(flags);
}

void txmSetRenderer(TXManager *mgr, TXRenderer *rd) {
  if (!rd)
    return; /* short-circuit */
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  mgr->renderer = rd;
}

/* bogus placeholder for backwards-compatibility */
TXRenderer *txmOpenGLRenderer(void) {
  return NULL;
}

static int get_unique_sharedid(void) {
  static int cnt = 0;
  cnt++;
  return cnt;
}

int txmReserveId(TXManager *mgr) {
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  if (mgr->flags & TXM_MF_SHARED_IDS)
    /* assign ids ourselves instead of letting the renderer do it */
    return get_unique_sharedid();
  if (!mgr->renderer || !mgr->renderer->reserve)
    return -1;
  return mgr->renderer->reserve();
}

/* set the renderer id for the texture */
static int assign_rid(TXManager *mgr, TXContext *ctx, int ind) {
  int k;
  /* if an rid has already been assigned, we're done */
  make_ctx_space(ctx,ind);
  if (ctx->ids[ind] > 0) {
    DEBUG(("assign_rid: rid already assigned in context"));
    return 0; /* already assigned */
  }

  if (mgr->flags & TXM_MF_SHARED_IDS) {
    /* see if this texture has already been assigned an id in another
       context */
    for(k = 0; k < mgr->ncontexts; k++)
      if (mgr->contexts[k] && (mgr->contexts[k]->ntex > ind) &&
	  (mgr->contexts[k]->ids[ind] > 0)) {
	DEBUG(("assign_rid: reusing id %d", mgr->contexts[k]->ids[ind]));
	ctx->ids[ind] = mgr->contexts[k]->ids[ind]; /* re-use existing id */
	return 0;
      }
  }
  /* reserve a new identifier */
  ctx->ids[ind] = txmReserveId(mgr);
  DEBUG(("assign_rid: using new id %d", ctx->ids[ind]));
  return 0;
}

static int load_texture(TXManager *mgr, TXContext *ctx, int ind) {
  int st;
  if (mgr->textures[ind] && !ctx->loaded[ind]) {
    if (ctx->ids[ind] <= 0)
      assign_rid(mgr,ctx,ind);
    if ((st = (mgr->renderer->load(ctx->ids[ind],mgr->textures[ind]))) == 0)
      ctx->loaded[ind] = 1;
    return st;
  }
  return 0;
}

int txmPreloadContext(TXManager *mgr) {
  /* goes through and loads all unloaded textures in the current context */
  TXContext *ctx;
  int k;
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  if (!mgr->renderer || !mgr->renderer->load)
    return -1;
  ctx = getctx(mgr);
  /* make sure we have sufficient id space in the context once */
  make_ctx_space(ctx,mgr->ntextures);
  for(k = 0; k < mgr->ntextures; k++)
    if (load_texture(mgr,ctx,k))
      return -1;
  return 0;
}
					  
int txmBindTexture(TXManager *mgr, int id) {
  /* bind texture in the current context */
  TXContext *ctx;
  mgr = getmgr(mgr);
  DEBUG(("txmBindTexture(0x%x,%d)",(unsigned)mgr,id));
  assert(mgr != NULL);
  if (!(ctx = getctx(mgr)))
    return -1;
  DEBUG(("txmBindTexture - context=0x%x",(unsigned)ctx));
  if (!mgr->renderer || !mgr->renderer->bind)
    return -1;
  id--; /* convert to index */
  if (id < 0 || id >= mgr->ntextures || !(mgr->textures[id]))
    return -1;
  DEBUG(("txmBindTexture - id=%d - ntex=%d",id,ctx->ntex));
  if ((ctx->ntex <= id) || !ctx->loaded[id]) {
    /* texture is not loaded in this context */
    DEBUG(("txmBindTexture - loading texture into context"));
    make_ctx_space(ctx,id);
    if (load_texture(mgr,ctx,id))
      return -1;
  }
  DEBUG(("txmBindTexture - binding texture %d",ctx->ids[id]));
  if (mgr->renderer->bind(ctx->ids[id]))
    return -1;
  ctx->bound = id;
  return 0;
}    

int txmBoundTexture(TXManager *mgr) {
  TXContext *ctx;
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  if (!(ctx = getctx(mgr)))
    return -1;
  return ctx->bound;
}

int txmReloadTexture(TXManager *mgr, int id) {
  /* force texture to be reloaded in all contexts (needed if you change it) */
  int k;
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  id--; /* convert to index */
  for(k = 0; k < mgr->ncontexts; k++) {
    if (mgr->contexts[k] && (mgr->contexts[k]->ntex > id))
      mgr->contexts[k]->loaded[id] = 0;
  }
  return 0;
}

static int texsize(int width, int height, int type, int ncomp) {
  int tsz = 0;
  switch (type) {
  case TXM_UBYTE:    tsz = 1; break;
  case TXM_USHORT:   tsz = 2; break;
  case TXM_UINT:     tsz = 4; break;
  case TXM_ULONG:    tsz = 4; break;
  case TXM_FLOAT:    tsz = 4; break;
  }
  return width*height*tsz*ncomp;
}

static unsigned long cksumtex(void *data, int sz) {
  unsigned long s = 0;
  unsigned char *b = (unsigned char *)data;
  while (sz > 0) {
    s <<= 1;
    s += (unsigned long)*b;
    sz--;
  }
  return s;
}

static int texeq(TXTexture *tex, void *data, int type, int width, int height,
		 int ncomp, unsigned long cksum) {
  if (!tex->unique &&
      (tex->type == type) &&
      (tex->width == width) &&
      (tex->height == height) &&
      (tex->ncomp == ncomp) &&
      (tex->cksum == cksum)) {
    /* likely a match - check data */
    if (memcmp(tex->data,data,texsize(width,height,type,ncomp)) == 0)
      return 1;
  }
  return 0;
}

extern TXTexture *txmLoadPNM(FILE *f, int flags);
extern TXTexture *txmLoadJPEG(FILE *f, int flags);
extern TXTexture *txmLoadTGA(FILE *f, int flags);

TXTexture *txmLoadFile(char *fname, char *ffmt, int flags) {
  FILE *f;
  TXTexture *tx = NULL;
  char rfmt[9];
  int k;

  if (!ffmt) {
    if (!(ffmt = strrchr(fname,'.'))) {
      fprintf(stderr, "TXM: cannot determine format of %s\n",fname);
      return NULL;
    }
    ffmt++;
  }
  /* convert format to lower-case */
  for(k = 0; k < 8 && ffmt[k]; k++)
    rfmt[k] = tolower(ffmt[k]);
  rfmt[k] = '\0';
  if (!(f = fopen(fname,"r"))) {
    fprintf(stderr, "TXM: cannot open %s\n",fname);
    return NULL;
  }
  /* pick loader */
  if (strcmp(rfmt,"pnm") == 0 || strcmp(rfmt,"pgm") == 0 ||
      strcmp(rfmt,"ppm") == 0)
    tx = txmLoadPNM(f,flags);
#ifdef HAS_JPEGLIB
  else if (strcmp(rfmt,"jpg") == 0 || strcmp(rfmt,"jpeg") == 0)
    tx = txmLoadJPEG(f,flags);
#endif
  else if (strcmp(rfmt,"tga") == 0)
    tx = txmLoadTGA(f,flags);
  else {
    fprintf(stderr, "TXM: no support for file type '%s'\n",rfmt);
    return NULL;
  }
  fclose(f);
  return tx;
}

int txmAddTexFile(TXManager *mgr, char *file, char *ffmt, int flags) {
  return txmAddTexFile2(mgr,file,ffmt,NULL,NULL,flags);
}

/* Allows a "transparency" mask to be loaded in addition to the texture.
   The second file is treated as an alpha channel. */
int txmAddTexFile2(TXManager *mgr, char *file, char *ffmt, 
		   char *afile, char *afmt, int flags) {
  /* use loader... */
  char rfmt[9];
  int k;
  TXTexture *ftx = NULL, *atx = NULL, *tx = NULL;
  FILE *f;
  int ret;

  mgr = getmgr(mgr);
  assert(mgr != NULL);

  if (!(ftx = txmLoadFile(file,ffmt,flags))) {
    fprintf(stderr, "TXM: cannot load file %s\n",file);
    return -1;
  }
  if (!afile) {
    tx = ftx;
    ftx = NULL;
  } else {
    if (ftx->ncomp != 1 && ftx->ncomp != 3) {
      fprintf(stderr, "TXM: cannot add alpha to texture file %s unless ncomp = 1 or 3\n",file);
      txmDestroyTex(ftx);
      return -1;
    }
    if (!(atx = txmLoadFile(afile,afmt,flags))) {
      fprintf(stderr, "TXM: cannot load alpha file %s\n",afile);
      txmDestroyTex(ftx);
      return -1;
    }
    if (atx->ncomp != 1) {
      fprintf(stderr, "TXM: alpha file %s must have ncomp = 1\n",afile);
      txmDestroyTex(atx);
      txmDestroyTex(ftx);
      return -1;
    }
    if (atx->width != ftx->width || atx->height != ftx->height) {
      fprintf(stderr, "TXM: alpha file %s does match dimensions of file %s\n",
	      afile,file);
      txmDestroyTex(atx);
      txmDestroyTex(ftx);
      return -1;
    }
    /* build new texture by combining the two... */
    {
      int i,m,sz;
      tx = txmCreateTex();
      tx->ncomp = ftx->ncomp+1;
      tx->width = ftx->width;
      tx->height = ftx->height;
      tx->data = malloc(tx->ncomp*tx->width*tx->height);
      tx->type = ftx->type;
      /* fill in ftx data... */
      m = tx->width*tx->height;
      sz = typesize(tx->type);
      for(i = 0; i < m; i++) {
	memcpy((char *)(tx->data)+tx->ncomp*i*sz,
	       (char *)(ftx->data)+ftx->ncomp*i*sz,
	       3*sz);
      }
      /* fill in atx data */
      if (atx->type == ftx->type) {
	for(i = 0; i < m; i++) {
	  memcpy((char *)(tx->data)+(tx->ncomp*i+3)*sz,
		 (char *)(atx->data)+atx->ncomp*i*sz,
		 sz);
	}
      } else {
	/* need to convert types */
	for(i = 0; i < m; i++) {
	  conv_data((char *)(tx->data)+(tx->ncomp*i+3)*sz,
		    tx->type,
		    (char *)(atx->data)+(atx->ncomp*i*sz),
		    atx->type,1);
	}
      }
    }
    /* phew - they're combined - we can ditch ftx and atx */
    txmDestroyTex(atx);
    txmDestroyTex(ftx);
  }
  
  ret = txmAddTexture(mgr,tx->data,tx->type,tx->width,tx->height,tx->ncomp,
		      flags);
  free(tx); /* do not free internal fields */
  return ret;
}

int txmAddTexture(TXManager *mgr, void *data, int type, int width, int height,
		  int ncomp, int flags) {
  int k,fn = -1;
  unsigned long cksum;
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  cksum = cksumtex(data,texsize(width,height,type,ncomp));

  DEBUG(("txmAddTexture start"));

  /* look for a duplicate/open slot */
  for(k = 0; k < mgr->ntextures; k++) {
    if (mgr->textures[k] == NULL)
      fn = k;
    else if (!(flags & TXM_UNIQUE) && 
	     texeq(mgr->textures[k],data,type,width,height,ncomp,cksum)) {
      /* duplicate texture */
      DEBUG(("txmAddTexture - found duplicate texture id %d",k+1));
      return k+1;
    }
  }
  DEBUG(("txmAddTexture - allocating new texture slot"));
  if (fn >= 0)
    k = fn; /* use existing empty slot */
  if (k >= mgr->ntextures) {
    if (mgr->texspace == 0) {
      mgr->texspace = 16; /* initial */
      mgr->textures = n_alloc(mgr->texspace*sizeof(TXTexture *));
    } else if (mgr->ntextures >= mgr->texspace) {
      while (mgr->ntextures >= mgr->texspace)
	mgr->texspace *= 2;
      mgr->textures = realloc(mgr->textures,mgr->texspace*sizeof(TXTexture *));
      assert(mgr->textures != NULL);
    }
    mgr->ntextures++;
  }
  DEBUG(("txmAddTexture - new slot is %d (id = %d)",k,k+1));
  mgr->textures[k] = s_alloc(TXTexture);
  mgr->textures[k]->data = data;
  mgr->textures[k]->type = type;
  mgr->textures[k]->ncomp = ncomp;
  mgr->textures[k]->width = width;
  mgr->textures[k]->height = height;
  mgr->textures[k]->cksum = cksum;
  mgr->textures[k]->options = NULL;
  mgr->textures[k]->unique = (flags & TXM_UNIQUE) ? 1 : 0;
  return k+1;
}

int txmDelTexture(TXManager *mgr, int id) {
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  id--;
  if (id < 0 || id >= mgr->ntextures)
    return -1;
  /* We'll mark this texture as "unloaded" in all contexts.
     This will not really unload it, but it will allow its id
     to be reused for a future texture. */
  txmReloadTexture(mgr,id);
  if (mgr->textures[id]) {
    txmDestroyTex(mgr->textures[id]);
    mgr->textures[id] = NULL;
  }
  return 0;
}

TXTexture *txmLookupTexture(TXManager *mgr, int id) {
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  id--;
  if (id >= 0 && id < mgr->ntextures)
    return mgr->textures[id];
  return NULL;
}

TXContext *txmCreateCtx(void) {
  TXContext *ctx;
  ctx = s_alloc(TXContext);
  return ctx;
}

void txmDestroyCtx(TXContext *ctx) {
  if (ctx) {
    ckfree(ctx->ids);
    ckfree(ctx->loaded);
    free(ctx);
  }
}

TXTexture *txmCreateTex(void) {
  TXTexture *tex;
  tex = s_alloc(TXTexture);
  return tex;
}

void txmDestroyTex(TXTexture *tex) {
  if (tex) {
    ckfree(tex->data);
    free(tex);
  }
}

static TXOption txm_options[] = {
  { TXM_OPTSET_TXM, TXM_AUTOMIPMAP, TXM_OPT_INT },
  { -1, -1, -1 }
};

TXTexOpt *txmIntGetOption(TXTexture *tex, int optset, int opt) {
  TXTexOpt *tp;
  if (!tex)
    return NULL;
  for(tp = tex->options; tp; tp = tp->next)
    if (tp->opt_set == optset && tp->opt_id == opt)
      return tp;
  return NULL;
}


int txmSetOptionV(TXManager *mgr, int id, int optset, int opt, va_list ap) {
  int i;
  TXTexture *t;
  TXTexOpt topt, *op;
  TXOption *o = NULL;
  float *f;
  
  id--;
  mgr = getmgr(mgr);
  assert(mgr != NULL);
  if (!(t = mgr->textures[id]))
    return -1;
  if (mgr->renderer && mgr->renderer->options) {
    for(i = 0; mgr->renderer->options[i].optset != -1; i++) {
      if (mgr->renderer->options[i].optset == optset &&
	  mgr->renderer->options[i].opt == opt) {
	o = &(mgr->renderer->options[i]);
	break;
      }
    }
  }
  if (!o) {
    for(i = 0; txm_options[i].optset != -1; i++) {
      if (txm_options[i].optset == optset &&
	  txm_options[i].opt == opt) {
	o = &(txm_options[i]);
	break;
      }
    }
  }
  if (!o)
    return -1;
  /* no such option */
  topt.fmt = o->fmt;
  topt.opt_set = o->optset;
  topt.opt_id = o->opt;
  topt.next = NULL;
  switch (o->fmt) {
  case TXM_OPT_INT:
    topt.data.i = va_arg(ap,int);
    break;
  case TXM_OPT_FLOAT:
    topt.data.f[0] = va_arg(ap,double);
    break;
  case TXM_OPT_FLOAT2:
    f = va_arg(ap,float *);
    topt.data.f[0] = f[0];
    topt.data.f[1] = f[1];
    break;
  case TXM_OPT_FLOAT3:
    f = va_arg(ap,float *);
    topt.data.f[0] = f[0];
    topt.data.f[1] = f[1];
    topt.data.f[2] = f[2];
    break;
  case TXM_OPT_FLOAT4:
    f = va_arg(ap,float *);
    topt.data.f[0] = f[0];
    topt.data.f[1] = f[1];
    topt.data.f[2] = f[2];
    topt.data.f[3] = f[3];
    break;
  }
  /* add or replace in the list? */
  if (op = txmIntGetOption(t,topt.opt_set,topt.opt_id)) {
    *op = topt;
    return 0;
  }
  /* not there already, must be added... */
  op = malloc(sizeof(TXTexOpt));
  assert(op != NULL);
  *op = topt;
  op->next = t->options;
  t->options = op;
  return 0;
}

int txmSetOption(TXManager *mgr, int id, int optset, int opt, ...) {
  int res;
  va_list ap;
  va_start(ap,opt);
  res = txmSetOptionV(mgr,id,optset,opt,ap);
  va_end(ap);
  return res;
}

/* special tool for debugging... */
int txmDumpPNM(TXTexture *t, char *file) {
  FILE *f;
  unsigned char *d;
  int i,m;
  int ncomp;

  if (!(f = fopen(file,"w")))
    return -1;

  ncomp = t->ncomp;
  m = t->width*t->height;
  d = malloc(ncomp*m);
  if (!d) {
    fclose(f);
    return -1;
  }
  /* convert type to ubyte */
  switch (t->type) {
  case TXM_UBYTE:
    memcpy(d,t->data,m*ncomp);
    break;
  case TXM_USHORT:
    {
      unsigned short *p = (unsigned short *)(t->data);
      for(i = 0; i < m*ncomp; i++)
	d[i] = (unsigned char)(p[i]>>8);
    }
    break;
  case TXM_UINT:
  case TXM_ULONG:
    {
      unsigned int *p = (unsigned int *)(t->data);
      for(i = 0; i < m*ncomp; i++)
	d[i] = (unsigned char)(p[i]>>24);
    }
    break;
  case TXM_FLOAT:
    {
      float f,*p = (float *)(t->data);
      for(i = 0; i < m*ncomp; i++) {
	f = p[i];
	if (f < 0.0)
	  f = 0.0;
	else
	  f = 1.0;
	d[i] = (unsigned char)(f*255.0);
      }
    }
    break;
  default:
    free(d);
    fclose(f);
    return -1;
  }
  /* convert components to either 1 or 3 */
  switch (ncomp) {
  case 1: break;
  case 2: 
    {
      int ins = 0;
      for(i = 0; i < m*ncomp; i += 2, ins++)
	d[ins] = d[i];
      ncomp = 1;
    }
    break;
  case 3: break;
  case 4:
    {
      int ins = 0;
      for(i = 0; i < m*ncomp; i += 4, ins += 3) {
	d[ins+0] = d[i+0];
	d[ins+1] = d[i+1];
	d[ins+2] = d[i+2];
      }
      ncomp = 3;
    }
    break;
  default:
    free(d);
    fclose(f);
    return -1;
  }
  /* now write */
  fprintf(f,"P%d\n%d %d 255\n",
	  (ncomp == 1 ? 5 : 6), t->width, t->height);
  fwrite(d,ncomp*m,1,f);
  fclose(f);
  return 0;
}
