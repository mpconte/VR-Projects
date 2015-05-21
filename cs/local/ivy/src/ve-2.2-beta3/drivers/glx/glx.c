/* GLX implementation for OpenGL driver */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "autocfg.h"

/* Use the OpenGL driver's header */
#include "../opengl/opengl.h"
#include <GL/glx.h>
#include <ve.h>

#define MODULE "driver:glx"

#define CKNULL(x) ((x)?(x):"<null>")

typedef struct {
  Display *dpy;
  Window win;
  GLXContext ctx;
  int x, y, w, h;
  int stereo;
  int ext_swap_group;
  int pipe;
} VeiGlxWindow;

#define MAXPIPES 20
static struct vei_glx_display_pipe {
  int inuse;
  char *display;
  Drawable group;  /* used for setting swap groups */
  GLXContext master;  /* master context (for sharing) */
  VeStrMap contexts; /* contexts indexed by thread name */
} displayPipes[MAXPIPES];
static VeThrMutex *pipelist_mutex = NULL;
static int vei_glx_num_pipes = 0;

/* list type for context-specific properties */
typedef struct {
  GLXContext ctx;
  VeStrMap dlist;
  int is_master;
} vei_glx_ctx_prop_t;
static vei_glx_ctx_prop_t *ctx_prop_list = NULL;
static int plist_sz = 0, plist_spc = 0;
static VeThrMutex *plist_mutex = NULL;
#if defined(__linux)
/* by default do not use shared contexts on Linux */
#define SHARED_CTX_DEF 0
#else
/* elsewhere use shared contexts by default */
#define SHARED_CTX_DEF 1
#endif
static int use_shared_ctx = SHARED_CTX_DEF;

static void disable_scrsaver(Display *dpy) {
  int timeout, interval, prefer_blanking, allow_exposures;
  XGetScreenSaver(dpy,&timeout,&interval,&prefer_blanking,
		  &allow_exposures);
  timeout = 0;
  XSetScreenSaver(dpy,timeout,interval,prefer_blanking,
		  allow_exposures);
  XResetScreenSaver(dpy);
}

static vei_glx_ctx_prop_t *get_ctx_prop(GLXContext ctx) {
  int i;
  if (!ctx)
    return NULL;
  veThrMutexLock(plist_mutex);
  for(i = 0; i < plist_sz; i++) {
    if (ctx_prop_list[i].ctx == ctx) {
      veThrMutexUnlock(plist_mutex);
      return &(ctx_prop_list[i]);
    }
  }
  /* make space */
  if (plist_sz >= plist_spc) {
    if (plist_spc == 0)
      plist_spc = 16; /* default value */
    else while (plist_sz >= plist_spc)
      plist_spc *= 2;
    ctx_prop_list = realloc(ctx_prop_list,plist_spc*sizeof(vei_glx_ctx_prop_t));
    assert(ctx_prop_list != NULL);
  }
  ctx_prop_list[plist_sz].ctx = ctx;
  ctx_prop_list[plist_sz].dlist = veStrMapCreate();
  plist_sz++;
  veThrMutexUnlock(plist_mutex);
  return &(ctx_prop_list[plist_sz-1]);
}

/* snarfed from GLUT which snarfed it from Motif... */
/* The following X property format is defined in Motif 1.1's
   Xm/MwmUtils.h, but GLUT should not depend on that header
   file. Note: Motif 1.2 expanded this structure with
   uninteresting fields (to GLUT) so just stick with the
   smaller Motif 1.1 structure. */
typedef struct {
#define MWM_HINTS_DECORATIONS   2
  long flags;
  long functions;
  long decorations;
  long input_mode;
} MotifWmHints;

#define MAXATTR 32
static int progAttrList[MAXATTR];
static int attributeList[MAXATTR];

static Bool waitForNotify(Display *d, XEvent *e, char *arg) {
  return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}


/* note the nasty use of numeric offsets in the following function
   - change the layout of attributeList and you need to change these too...
   */
static void optToAttList(VeWindow *w) {
  char *s;
  int i = 0, alpha = 0;

  while (progAttrList[i] != None) {
	attributeList[i] = progAttrList[i];
	i++;
  }
  attributeList[i++] = GLX_RGBA;  /* no supprt for colormap mode... */
  if ((s = veGetWinOption(w,"doublebuffer")) && atoi(s))
    attributeList[i++] = GLX_DOUBLEBUFFER; 
  if ((s = veGetWinOption(w,"red"))) {
    attributeList[i++] = GLX_RED_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veGetWinOption(w,"green"))) {
    attributeList[i++] = GLX_GREEN_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veGetWinOption(w,"blue"))) {
    attributeList[i++] = GLX_BLUE_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veGetWinOption(w,"alpha"))) {
    int k;
    k = atoi(s);
    attributeList[i++] = GLX_ALPHA_SIZE;
    attributeList[i++] = k;
    if (k > 0)
      alpha = 1;
  }
  if ((s = veGetWinOption(w,"depth"))) {
    attributeList[i++] = GLX_DEPTH_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veGetWinOption(w,"accum"))) {
    int k;
    k = atoi(s);
    attributeList[i++] = GLX_ACCUM_RED_SIZE;
    attributeList[i++] = k;
    attributeList[i++] = GLX_ACCUM_GREEN_SIZE;
    attributeList[i++] = k;
    attributeList[i++] = GLX_ACCUM_BLUE_SIZE;
    attributeList[i++] = k;
    attributeList[i++] = GLX_ACCUM_ALPHA_SIZE;
    attributeList[i++] = k;
  }
  if ((w->eye == VE_WIN_STEREO))
    attributeList[i++] = GLX_STEREO;
  attributeList[i++] = None;
}

static void handle_expose_events(VeWindow *w) {
  VeiGlxWindow *gw = (VeiGlxWindow *)(w->udata);
  static long event_mask = StructureNotifyMask|ExposureMask|VisibilityChangeMask;
  XEvent e;
  XConfigureEvent *ce;

  VE_DEBUGM(3,("handling expose events"));
  while (XCheckWindowEvent(gw->dpy,gw->win,event_mask,&e)) {
    VE_DEBUGM(3,("got window event"));
    if (e.type == ConfigureNotify) {
      ce = (XConfigureEvent *)(&e);
      VE_DEBUGM(3,("got configure event"));
      gw->x = ce->x;
      gw->y = ce->y;
      gw->w = ce->width;
      gw->h = ce->height;
    }
  }
  VE_DEBUGM(3,("finished expose events"));
}

void veiGlPreRender(VeWindow *w) {
  if (w->udata)
    handle_expose_events(w);
}

void veiGlInit(void) {
  XInitThreads();
  progAttrList[0] = None;
  attributeList[0] = None;
  pipelist_mutex = veThrMutexCreate();
  plist_mutex = veThrMutexCreate();
}

int veiGlOpenWindow(VeWindow *w) {
  VeiGlxWindow *gw;
  XVisualInfo *vi;
  Colormap cmap;
  long event_mask = StructureNotifyMask|ExposureMask|VisibilityChangeMask;
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  int x,y,width,height;
  XEvent event;
  XSizeHints hints;
  MotifWmHints mhints;
  vei_glx_ctx_prop_t *prop;
  char *dname, *s, ext[1024];

  VE_DEBUGM(1,("opening window %s", w->name));

  gw = malloc(sizeof(VeiGlxWindow));
  assert(gw != NULL);
  w->udata = gw;
  
  gw->stereo = (w->eye == VE_WIN_STEREO);

  dname = w->display;
  if (dname && strcmp(dname,"default") == 0)
    dname = NULL;
  if (!(gw->dpy = XOpenDisplay(dname)))
    veFatalError(MODULE, "cannot open display: %s",CKNULL(w->display));
  if ((s = veGetWinOption(w,"glx_sync")) && atoi(s))
    XSynchronize(gw->dpy,1);
  /* resolve options */
  optToAttList(w);

  gw->ext_swap_group = 0;
  if ((s = (char *)glXGetClientString(gw->dpy,GLX_EXTENSIONS))) {
    veNotice(MODULE, "window %s: GLX Extensions: %s", w->name, s);
    strcpy(ext,s);
    s = strtok(ext," \t\r\n");
    while (s) {
#ifndef NO_SWAP_GROUP
      if (strcmp(s,"GLX_SGIX_swap_group") == 0) {
	if ((s = veGetWinOption(w,"glx_swap_group")) && !atoi(s)) {
	  veNotice(MODULE, "window %s: explicitly disabling swap group");
	  gw->ext_swap_group = 0;
	} else {
	  veNotice(MODULE, "window %s: enabling swap group");
	  gw->ext_swap_group = 1;
	}
      }
#endif /* !NO_SWAP_GROUP */
      s = strtok(NULL, " \t\r\n");
    }
  }
  

  vi = glXChooseVisual(gw->dpy,DefaultScreen(gw->dpy),attributeList);
  if (vi == NULL)
    veFatalError(MODULE, "cannot find matching visual for window %s (display %s)",
		 CKNULL(w->name), CKNULL(w->display));

  cmap = XCreateColormap(gw->dpy, RootWindow(gw->dpy, vi->screen),
			 vi->visual, AllocNone);
  swa.colormap = cmap;
  swa.border_pixel = 0;
  swa.event_mask = StructureNotifyMask;

  if (w->geometry == NULL)
    veFatalError(MODULE, "geometry missing on window %s", w->name);

  veParseGeom(w->geometry,&x,&y,&width,&height);

  gw->win = XCreateWindow(gw->dpy, RootWindow(gw->dpy,vi->screen),
			  x, y, width, height, 0, vi->depth, InputOutput,
			  vi->visual, CWBorderPixel|CWColormap|CWEventMask, 
			  &swa);
  /* hide mouse cursor (by default) */
  if (!(s = veGetWinOption(w,"cursor")) || !atoi(s)) {
    /* to hide the cursor, we create a blank one and use that */
    static char null[] = { 0x0 };
    Pixmap p; /* I don't really care what the foreground/background is here */
    Cursor c;
    XColor cl;
    memset(&cl,0,sizeof(cl));
    cl.red = cl.green = cl.blue = 0;
    cl.flags = DoRed|DoGreen|DoBlue;
    p = XCreatePixmapFromBitmapData(gw->dpy, gw->win, null, 1, 1, 0, 0, 1);
    c = XCreatePixmapCursor(gw->dpy,p,p,&cl,&cl,0,0);
    XDefineCursor(gw->dpy,gw->win,c);
  }
  if (w->name)
    XStoreName(gw->dpy,gw->win,w->name);
  hints.flags = USPosition|USSize;
  hints.x = x;
  hints.y = y;
  hints.width = width;
  hints.height = height;
  XSetWMNormalHints(gw->dpy,gw->win,&hints);
  if ((s = veGetWinOption(w,"noborder")) && atoi(s)) {
    /* Convince Motif-compliant WM not to decorate window */
    Atom a = XInternAtom(gw->dpy, "_MOTIF_WM_HINTS", 0);
    mhints.flags = MWM_HINTS_DECORATIONS;
    mhints.decorations = 0;

    XChangeProperty(gw->dpy,gw->win,a,a,32,
		    PropModeReplace, (const unsigned char *)&mhints, 4);
  }
  
  {
    int i;
    veThrMutexLock(pipelist_mutex);
    for(i = 0; i < MAXPIPES; i++)
      if (displayPipes[i].inuse && 
	  strcmp(displayPipes[i].display,w->display) == 0) {
	/* attach to this swap group */
#ifdef HAS_SWAPGROUP
	if (gw->ext_swap_group) {
	  veNotice(MODULE, "window %s: joining swap group %u",
		   w->name, (unsigned)(displayPipes[i].group));
	  glXJoinSwapGroupSGIX(gw->dpy,gw->win,displayPipes[i].group);
	}
#endif /* HAS_SWAPGROUP */
	gw->pipe = i;
	if (!(gw->ctx = glXCreateContext(gw->dpy, vi, 
					 (use_shared_ctx ? 
					  displayPipes[i].master : NULL),
					 GL_TRUE)))
	  veFatalError(MODULE,"glXCreateContext failed");
	prop = get_ctx_prop(gw->ctx);
	prop->is_master = 0;
	break;
      }
    if (i == MAXPIPES) {
      for(i = 0; i < MAXPIPES; i++) {
	if (!displayPipes[i].inuse) {
#ifndef HAS_SWAPGROUP
	  if (gw->ext_swap_group) {
	    veNotice(MODULE, "window %s: creating swap group %u",
		     w->name, gw->win);
	  }
#endif /* !HAS_SWAPGROUP */
	  displayPipes[i].inuse = 1;
	  displayPipes[i].display = veDupString(w->display);
	  displayPipes[i].group = gw->win;
	  gw->ctx = glXCreateContext(gw->dpy, vi, NULL, GL_TRUE);
	  prop = get_ctx_prop(gw->ctx);
	  prop->is_master = 1;
	  displayPipes[i].master = gw->ctx;
	  displayPipes[i].contexts = veStrMapCreate();
	  gw->pipe = i;
	  if (i >= vei_glx_num_pipes)
	    vei_glx_num_pipes = i+1;
	  break;
	}
      }
      if (i == MAXPIPES) /* try to continue */
	veWarning(MODULE, "MAXPIPES (%d) exceeded - swap groups may be suboptimal", MAXPIPES);
    }
    veThrMutexUnlock(pipelist_mutex);
  }

  XMapWindow(gw->dpy,gw->win);
  XIfEvent(gw->dpy, &event, waitForNotify,(char *)gw->win);

  veiGlSetWindow(w);
  
  /* blank the screen */
  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();
  glXSwapBuffers(gw->dpy,gw->win);

  /* we should really lock callbacks while we are doing this */
  veLockCallbacks();
  veRenderCallSetupCback(w);
  veUnlockCallbacks();

  /* Let X know that I'm interested in events */
  VE_DEBUGM(2,("%s: setting up event mask",CKNULL(w->name)));
  XGetWindowAttributes(gw->dpy,gw->win,&wa);
  swa.event_mask = (wa.your_event_mask | event_mask);
  XChangeWindowAttributes(gw->dpy,gw->win,CWEventMask,&swa);

  /* and finally...disable the screensaver (I'm not nice about this
     in that I don't re-enable it afterwards...) */
  if ((s = veGetWinOption(w,"nosaver")) && (atoi(s)))
    disable_scrsaver(gw->dpy);

  VE_DEBUGM(2,("%s: flushing GL queue",CKNULL(w->name)));
  glFinish();

  VE_DEBUGM(2,("%s: flushing X queue",CKNULL(w->name)));
  XSync(gw->dpy,0);

  veiGlUnsetWindow(w);

  VE_DEBUGM(2,("%s: open window finished ok",w->name ? w->name : "<null>"));
  return 0;
}

void veiGlCloseWindow(VeWindow *w) {
  VeiGlxWindow *gw;
  gw = (VeiGlxWindow *)(w->udata);
  if (gw) {
    /* destroy context? */
    XDestroyWindow(gw->dpy,gw->win);
    XCloseDisplay(gw->dpy);
    free(gw);
    w->udata = NULL; /* done */
    /* anything else to cleanup? */
  }
}

void veiGlSetWindow(VeWindow *w) {
  VeiGlxWindow *gw;
  gw = (VeiGlxWindow *)(w->udata);
  VE_DEBUGM(5,("veiGlSetWindow(0x%x) - gw = 0x%x",
	     (unsigned)w,(unsigned)gw));
  glXMakeCurrent(gw->dpy,gw->win,gw->ctx);
}

void veiGlUnsetWindow(VeWindow *w) {
  VeiGlxWindow *gw;
  gw = (VeiGlxWindow *)(w->udata);
  VE_DEBUGM(5,("veiGlUnsetWindow(0x%x) - gw = 0x%x",
	       (unsigned)w,(unsigned)gw));
#ifndef HAS_GLX_NOUNSET
  glXMakeCurrent(gw->dpy,None,NULL);
#endif
}

void veiGlSwapBuffers(VeWindow *w) {
  VeiGlxWindow *gw;
  gw = (VeiGlxWindow *)(w->udata);
  VE_DEBUGM(5,("veiGlSwapBuffers(0x%x) - gw = 0x%x",
	       (unsigned)w,(unsigned)gw));
  glXSwapBuffers(gw->dpy,gw->win);
}

void veiGlGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  VeiGlxWindow *gw;
  gw = (VeiGlxWindow *)(win->udata);
  if (gw) {
    if (x) *x = gw->x;
    if (y) *y = gw->y;
    if (w) *w = gw->w;
    if (h) *h = gw->h;
  }
}

/* TXM code */

#ifndef NO_SHARED_CONTEXTS
#define CTXTYPE GLXContextID
#else
#define CTXTYPE GLXContext
#endif /* NO_SHARED_CONTEXTS */

/* assume we are dealing with some reasonably finite number
   of contexts */
#define MAX_CONTEXTS 128
static CTXTYPE ctxid[MAX_CONTEXTS];

static char *err2str(int err) {
  switch (err) {
  case GL_NO_ERROR: return "no error";
  case GL_INVALID_ENUM: return "invalid enum";
  case GL_INVALID_VALUE: return "invalid value";
  case GL_INVALID_OPERATION: return "invalid operation";
  case GL_STACK_OVERFLOW: return "stack overflow";
  case GL_STACK_UNDERFLOW: return "stack underflow";
  case GL_OUT_OF_MEMORY: return "out of memory";
  case GL_TABLE_TOO_LARGE: return "table too large";
  default: return "<unknown>";
  }
}

static void clear_err(void) {
  (void) glGetError();
}

static void check_err(char *op) {
  int err;
  if (err = glGetError()) {
    fprintf(stderr,"OpenGL error: %s: 0x%x (%s)\n",
	    op, err, err2str(err));
  }
}

static pthread_key_t ctxcache_key;
static int cache_init = 0;
static void free_ctxcache(void *x) {
  if (x) { free(x); }
}
typedef struct ogl_ctxcache {
  int id;
  CTXTYPE ctx;
} ogl_ctxcache_t;

static int ogl_reserve(void) {
  GLuint k;
  clear_err();
  glGenTextures(1,&k);
  check_err("reserving texture id");
  return (int)k;
}

/* although this is a linear search, this should generally be reasonable
   as the context set should be quite small */
/* I've created separate ogl_ctxid functions for each case for cleanliness
   and readability.  It is possible to glom them together with #ifdef's but it
   becomes really hard to trace. 
*/
#if defined(NO_SHARED_CONTEXTS)
static int ogl_ctxid(void) {
  GLXContext ctx;
  int k;
  GLXContext match;
  ogl_ctxcache_t *cache;

  if (!(cache = pthread_getspecific(ctxcache_key))) {
    cache = calloc(1,sizeof(ogl_ctxcache_t));
    assert(cache != NULL);
    cache->id = -1;
    pthread_setspecific(ctxcache_key,(void *)cache);
  }

  if (!(ctx = glXGetCurrentContext())) {
    fprintf(stderr, "TXM: no active OpenGL context\n");
    abort();
  }
  match = ctx;
  if (match == cache->ctx && cache->id >= 0)
    return cache->id;
  for(k = 0; (k < MAX_CONTEXTS) && (ctxid[k] != 0); k++) {
    if (ctxid[k] == match) {
      cache->ctx = match;
      cache->id = k;
      return k;
    }
  }
  if (k >= MAX_CONTEXTS) {
    fprintf(stderr, "TXM: too many OpenGL contexts in program (max = %d)\n",
	    MAX_CONTEXTS);
    abort();
  }
  ctxid[k] = match;
  cache->ctx = match;
  cache->id = k;
  return k;
}
#else /* using shared contexts... */
static int ogl_ctxid(void) {
  GLXContext ctx;
  int k;
  GLXContextID xid, match;
  ogl_ctxcache_t *cache;
  
  if (!(cache = pthread_getspecific(ctxcache_key))) {
    cache = calloc(1,sizeof(ogl_ctxcache_t));
    assert(cache != NULL);
    cache->id = -1;
    pthread_setspecific(ctxcache_key,(void *)cache);
  }

  if (!(ctx = glXGetCurrentContext())) {
    fprintf(stderr, "TXM: no active OpenGL context\n");
    abort();
  }

  /* determine xid for this context */
  xid = 0;
  glXQueryContextInfoEXT(glXGetCurrentDisplay(),ctx,
			 GLX_SHARE_CONTEXT_EXT,
			 (int *)&xid);
  if (xid == 0)
    xid = glXGetContextIDEXT(ctx);
  match = xid;

  if (match == cache->ctx)
    return cache->id;
  for(k = 0; (k < MAX_CONTEXTS) && (ctxid[k] != 0); k++) {
    if (ctxid[k] == match) {
      cache->ctx = match;
      cache->id = k;
      return k;
    }
  }
  if (k >= MAX_CONTEXTS) {
    fprintf(stderr, "TXM: too many OpenGL contexts in program (max = %d)\n",
	    MAX_CONTEXTS);
    abort();
  }
  ctxid[k] = match;
  cache->ctx = match;
  cache->id = k;
  return k;
}
#endif 

static int ogl_load(int id, TXTexture *t) {
  TXTexOpt *o;
  int type, err;
  int fmts[5] = {
    -1, /* not a real value */
    GL_LUMINANCE,
    GL_LUMINANCE_ALPHA,
    GL_RGB,
    GL_RGBA
  };
  switch(t->type) {
  case TXM_UBYTE:
    type = GL_UNSIGNED_BYTE;
    break;
  case TXM_USHORT:
    type = GL_UNSIGNED_SHORT;
    break;
  case TXM_UINT:
  case TXM_ULONG:
    type = GL_UNSIGNED_INT;
    break;
  case TXM_FLOAT:
    type = GL_FLOAT;
    break;
  default:
    return -1;
  }
  if (t->ncomp <= 0 || t->ncomp > 4)
    return -1;
  clear_err();
  glBindTexture(GL_TEXTURE_2D,id);
  check_err("binding texture for load");
  if (!(o = txmIntGetOption(t,TXM_OPTSET_TXM,TXM_AUTOMIPMAP)) || 
      (o->data.i)) {
    /* mipmap */
    gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA,t->width,t->height,
		      fmts[t->ncomp],type,t->data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
		    GL_LINEAR_MIPMAP_LINEAR);
  } else {
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t->width,t->height,0,fmts[t->ncomp],
		 type,t->data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
		    GL_LINEAR);
  }
  check_err("loading texture");
  /* process other options */
  for(o = t->options; o; o = o->next) {
    if (o->opt_set == TXM_OPTSET_OPENGL) {
      if (o->fmt == TXM_OPT_INT)
	glTexParameteri(GL_TEXTURE_2D,o->opt_id,o->data.i);
      else
	glTexParameterfv(GL_TEXTURE_2D,o->opt_id,o->data.f);
    }
  }
  check_err("setting texture parameters");
  return 0;
}

static int ogl_unload(int id) {
  GLuint d;
  d = (GLuint)id;
  clear_err();
  glDeleteTextures(1,&d);
  check_err("unloading texture");
  return 0;
}

static int ogl_bind(int id) {
  clear_err();
  glBindTexture(GL_TEXTURE_2D,id);
  check_err("binding texture");
  return 0;
}

static TXOption ogl_txoptions[] = {
  { TXM_OPTSET_OPENGL, GL_TEXTURE_MIN_FILTER, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_MAG_FILTER, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_MIN_LOD, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_MAX_LOD, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_BASE_LEVEL, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_MAX_LEVEL, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_WRAP_S, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_WRAP_T, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_WRAP_R, TXM_OPT_INT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_PRIORITY, TXM_OPT_FLOAT },
  { TXM_OPTSET_OPENGL, GL_TEXTURE_BORDER_COLOR, TXM_OPT_FLOAT4 },
  { -1, -1, -1 }
};

static TXRenderer the_opengl_renderer = {
  ogl_reserve,
  ogl_ctxid,
  ogl_load,
  ogl_unload,
  ogl_bind,
  ogl_txoptions
};

TXRenderer *veTXMImplRenderer(void) {
  if (!cache_init) {
    pthread_key_create(&ctxcache_key,(void *)free_ctxcache);
    cache_init = 1;
  }
  return &the_opengl_renderer;
}

void VE_DRIVER_PROBE(glx) (void *phdl) {
  veDriverProvide(phdl,"glimpl","glx");
  veDriverProvide(phdl,"txmrender","glx");
}

void VE_DRIVER_INIT(glx) (void) {
  if (veDriverRequire("render","opengl"))
    veFatalError(MODULE,"cannot load OpenGL support");
}

