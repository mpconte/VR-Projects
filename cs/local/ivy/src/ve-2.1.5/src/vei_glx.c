/* This module contains code that is common across all GLX implementations */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ve_thread.h>
#include <ve_error.h>
#include <ve_stats.h>
#include <ve_mp.h>
#include <ve_util.h>
#include <vei_glx.h>

#define MODULE "vei_glx"

#define MAXPIPES 20
static struct {
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

int veiGlIsMaster(void) {
  GLXContext ctx;
  vei_glx_ctx_prop_t *p;
  ctx = glXGetCurrentContext();
  p = get_ctx_prop(ctx);
  return p->is_master;
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

static int glx_timing_test = 0;

static VeThrMutex *render_mutex = NULL;


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
  if (!glx_timing_test && (s = veiGlGetOption(w,"doublebuffer")) && atoi(s))
    attributeList[i++] = GLX_DOUBLEBUFFER; 
  if ((s = veiGlGetOption(w,"red"))) {
    attributeList[i++] = GLX_RED_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veiGlGetOption(w,"green"))) {
    attributeList[i++] = GLX_GREEN_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veiGlGetOption(w,"blue"))) {
    attributeList[i++] = GLX_BLUE_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veiGlGetOption(w,"alpha"))) {
    int k;
    k = atoi(s);
    attributeList[i++] = GLX_ALPHA_SIZE;
    attributeList[i++] = k;
    if (k > 0)
      alpha = 1;
  }
  if ((s = veiGlGetOption(w,"depth"))) {
    attributeList[i++] = GLX_DEPTH_SIZE;
    attributeList[i++] = atoi(s);
  }
  if ((s = veiGlGetOption(w,"accum"))) {
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
  if ((w->eye == VE_WIN_STEREO) ||
      ((s = veiGlGetOption(w,"stereo")) && atoi(s)))
    attributeList[i++] = GLX_STEREO;
  attributeList[i++] = None;
}

static void handle_expose_events(VeWindow *w) {
  VeiGlxWindow *gw = (VeiGlxWindow *)(w->udata);
  static long event_mask = StructureNotifyMask|ExposureMask|VisibilityChangeMask;
  XEvent e;
  XConfigureEvent *ce;

  VE_DEBUG(3,("handling expose events"));
  while (XCheckWindowEvent(gw->dpy,gw->win,event_mask,&e)) {
    VE_DEBUG(3,("got window event"));
    if (e.type == ConfigureNotify) {
      ce = (XConfigureEvent *)(&e);
      VE_DEBUG(3,("got configure event"));
      gw->x = ce->x;
      gw->y = ce->y;
      gw->w = ce->width;
      gw->h = ce->height;
      glViewport(0,0,gw->w,gw->h);
    }
  }
  VE_DEBUG(3,("finished expose events"));
}

static void open_window(char *tname, VeWindow *w) {
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

  VE_DEBUG(1,("opening window %s", w->name));

  gw = malloc(sizeof(VeiGlxWindow));
  assert(gw != NULL);
  w->udata = gw;
  
  gw->stereo = (w->eye == VE_WIN_STEREO) || 
    ((s = veiGlGetOption(w,"stereo")) && atoi(s));

  dname = w->display;
  if (dname && strcmp(dname,"default") == 0)
    dname = NULL;
  if (!(gw->dpy = XOpenDisplay(dname)))
    veFatalError(MODULE, "cannot open display: %s",
		 w->display ? w->display : "<NULL>");
  /* resolve options */
  optToAttList(w);

  gw->ext_swap_group = 0;
  if ((s = (char *)glXGetClientString(gw->dpy,GLX_EXTENSIONS))) {
    veNotice(MODULE, "window %s: GLX Extensions: %s", w->name, s);
    strcpy(ext,s);
    s = strtok(ext," \t\r\n");
    while (s) {
#ifndef DISABLE_SWAP_GROUP
      if (strcmp(s,"GLX_SGIX_swap_group") == 0) {
	if ((s = veiGlGetOption(w,"glx_swap_group")) && !atoi(s)) {
	  veNotice(MODULE, "window %s: explicitly disabling swap group");
	  gw->ext_swap_group = 0;
	} else {
	  veNotice(MODULE, "window %s: enabling swap group");
	  gw->ext_swap_group = 1;
	}
      }
#endif /* !DISABLE_SWAP_GROUP */
      s = strtok(NULL, " \t\r\n");
    }
  }
  

  vi = glXChooseVisual(gw->dpy,DefaultScreen(gw->dpy),attributeList);
  if (vi == NULL)
    veFatalError(MODULE, "cannot find matching visual for window %s (display %s)",
		 w->name ? w->name : "<NULL>", 
		 w->display ? w->display : "<NULL>");

  cmap = XCreateColormap(gw->dpy, RootWindow(gw->dpy, vi->screen),
			 vi->visual, AllocNone);
  swa.colormap = cmap;
  swa.border_pixel = 0;
  swa.event_mask = StructureNotifyMask;

  if (w->geometry == NULL)
    veFatalError(MODULE, "geometry missing on window %s", w->name);

  veiGlParseGeom(w->geometry,&x,&y,&width,&height);

  gw->win = XCreateWindow(gw->dpy, RootWindow(gw->dpy,vi->screen),
			  x, y, width, height, 0, vi->depth, InputOutput,
			  vi->visual, CWBorderPixel|CWColormap|CWEventMask, 
			  &swa);
  /* hide mouse cursor (by default) */
  if (!(s = veiGlGetOption(w,"cursor")) || !atoi(s)) {
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
  if ((s = veiGlGetOption(w,"noborder")) && atoi(s)) {
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
#ifndef DISABLE_SWAP_GROUP
	if (gw->ext_swap_group) {
	  veNotice(MODULE, "window %s: joining swap group %u",
		   w->name, (unsigned)(displayPipes[i].group));
	  glXJoinSwapGroupSGIX(gw->dpy,gw->win,displayPipes[i].group);
	}
#endif /* !DISABLE_SWAP_GROUP */
	gw->pipe = i;
	gw->ctx = glXCreateContext(gw->dpy, vi, 
				   (use_shared_ctx ? 
				    displayPipes[i].master : NULL),
				   GL_TRUE);
	prop = get_ctx_prop(gw->ctx);
	prop->is_master = 0;
	break;
      }
    if (i == MAXPIPES) {
      for(i = 0; i < MAXPIPES; i++) {
	if (!displayPipes[i].inuse) {
#ifndef DISABLE_SWAP_GROUP
	  if (gw->ext_swap_group) {
	    veNotice(MODULE, "window %s: creating swap group %u",
		     w->name, gw->win);
	  }
#endif /* !DISABLE_SWAP_GROUP */
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

  glXMakeCurrent(gw->dpy,gw->win,gw->ctx);
  
  /* blank the screen */
  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();
  if (!glx_timing_test)
    glXSwapBuffers(gw->dpy,gw->win);

  /* we should really lock callbacks while we are doing this */
  veLockCallbacks();
  veiGlSetupWindow(w);
  veUnlockCallbacks();

  glXMakeCurrent(gw->dpy,None,NULL);

  /* Let X know that I'm interested in events */
  XGetWindowAttributes(gw->dpy,gw->win,&wa);
  swa.event_mask = (wa.your_event_mask | event_mask);
  XChangeWindowAttributes(gw->dpy,gw->win,CWEventMask,&swa);

  /* and finally...disable the screensaver (I'm not nice about this
     in that I don't re-enable it afterwards...) */
  if ((s = veiGlGetOption(w,"nosaver")) && (atoi(s)))
    disable_scrsaver(gw->dpy);
}

void veiGlxSetVisualAttr(int *vattr) {
  int i = 0;
  if (!vattr)
    /* empty previous attributes */
    progAttrList[0] = None;

  while(vattr[i] != None) {
    progAttrList[i] = vattr[i];
    i++;
  }
  progAttrList[i] = None;
}

/* Important - in the MP model, this call will only work on the slave
   where the window is being rendered (which should be okay) */
void veRenderGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  VeiGlxWindow *gw = (VeiGlxWindow *)(win->udata);
  if (gw) {
    if (x) *x = gw->x;
    if (y) *y = gw->y;
    if (w) *w = gw->w;
    if (h) *h = gw->h;
  }
}

static VeStrMap threads_by_display = NULL;
static VeStrMap threads_by_name = NULL;
static VeStrMap threads_by_window = NULL;

static VeWindow **all_windows = NULL;
static int num_windows = 0;
static int num_threads = 0;
static VeThrBarrier *b_start, *b_entry, *b_exit;

static void *render_thread(void *v) {
  char *me = (char *)v;
  int i,n = 0;
  VeWindow **my_windows = NULL;
  VeEnv *env = veGetEnv();

  assert(env != NULL);

  /* find all of my windows */
  {
    VeStrMapWalk w = veStrMapWalkCreate(threads_by_window);
    if (veStrMapWalkFirst(w) == 0) {
      do {
	char *s;
	if ((s = (char *)veStrMapWalkObj(w)) && (strcmp(s,me) == 0)) {
	  /* this is mine ... */
	  n++;
	}
      } while (veStrMapWalkNext(w) == 0);
    }

    /* now build the list and open the windows */
    my_windows = malloc(sizeof(VeWindow *)*n);
    assert(my_windows != NULL);
    i = 0;
    if (veStrMapWalkFirst(w) == 0) {
      do {
	char *s;
	if ((s = (char *)veStrMapWalkObj(w)) && (strcmp(s,me) == 0)) {
	  /* this is mine ... */
	  my_windows[i] = veFindWindow(env,veStrMapWalkStr(w));
	  assert(my_windows[i] != NULL);
	  VE_DEBUG(1,("thread %d: taking window %s",
		   veThreadId(),my_windows[i]->name));
	  open_window(me,my_windows[i]);
	  i++;
	}
      } while (veStrMapWalkNext(w) == 0);
    }
    
  }

  /* let parent know that we've opened our windows... */
  veThrBarrierEnter(b_start);

  while(1) {
    static GLenum mmodes[] = { GL_COLOR, GL_TEXTURE, GL_PROJECTION,
			       GL_MODELVIEW };
    int j;
    veThrBarrierEnter(b_entry);
    for(i = 0; i < n; i++) {
      VeiGlxWindow *gw = (VeiGlxWindow *)(my_windows[i]->udata);
      glXMakeCurrent(gw->dpy,gw->win,gw->ctx);
      /* set modes to default */
      for(j = 0; j < sizeof(mmodes)/sizeof(GLenum); j++) {
	glMatrixMode(mmodes[j]);
	glLoadIdentity();
      }
      glViewport(0,0,gw->w,gw->h);
      VE_DEBUG(4,("rendering window %s",my_windows[i]->name));
      handle_expose_events(my_windows[i]);
      veiGlRenderWindow(my_windows[i]);
      glFinish();
      glXMakeCurrent(gw->dpy,None,NULL);
    }
    veThrBarrierEnter(b_exit);
  }
}

static void render_ctrl_handler(int msg, int tag, void *data, int dlen) {
  char *arg;
  if (msg != VE_MPMSG_CTRL)
    return;
  VE_DEBUG(5,("render_ctrl_handler - message (%d,%d) [%d bytes]",msg,tag,dlen));
  arg = ((VeMPCtrlMsg *)data)->arg;
  VE_DEBUG(7,("render_ctrl_handler - data = '%2x %2x %2x %2x %2x %2x %2x %2x %2x %2x ...'",
	      arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],arg[7],arg[8],arg[9]));
  switch (tag) {
  case VE_MPCTRL_WINDOW:
    {
      VeWindow *w;
      char *thr, *s, *dpy;
      VE_DEBUG(5,("render_ctrl_handler - taking window '%s'",arg));
      if (!(w = veFindWindow(veGetEnv(),arg)))
	veFatalError(MODULE,"unable to find requested slave window %s",arg);
      
      if (!threads_by_name)
	threads_by_name = veStrMapCreate();
      if (!threads_by_display)
	threads_by_display = veStrMapCreate();
      if (!threads_by_window)
	threads_by_window = veStrMapCreate();

      if (veStrMapLookup(threads_by_window,w->name))
	return; /* we already know about this window... */

      dpy = w->display ? w->display : "default";
      thr = w->slave.thread ? w->slave.thread : "auto";
      if ((s = (char *)veStrMapLookup(threads_by_display,dpy)))
	/* override with this display thread... */
	thr = s;
      if (!veStrMapLookup(threads_by_name,thr)) {
	/* new thread */
	veStrMapInsert(threads_by_name,thr,(void *)1);
	veStrMapInsert(threads_by_display,dpy,veDupString(thr));
      }
      veStrMapInsert(threads_by_window,w->name,veDupString(thr));
    }
    break;

  case VE_MPCTRL_RUN:
    {
      VE_DEBUG(5,("render_ctrl_handler - running renderer"));
      if (threads_by_name && threads_by_window) {
	VeStrMapWalk w;
	VeEnv *env = veGetEnv();
	int i;

	assert(env != NULL);
	w = veStrMapWalkCreate(threads_by_window);
	if (veStrMapWalkFirst(w) == 0) {
	  do {
	    num_windows++;
	  } while (veStrMapWalkNext(w) == 0);
	}
	all_windows = malloc(sizeof(VeWindow *)*num_windows);
	assert(all_windows != NULL);
	i = 0;
	if (veStrMapWalkFirst(w) == 0) {
	  do {
	    all_windows[i] = veFindWindow(env,veStrMapWalkStr(w));
	    assert(all_windows[i] != NULL);
	    i++;
	  } while (veStrMapWalkNext(w) == 0);
	}
	veStrMapWalkDestroy(w);


	w = veStrMapWalkCreate(threads_by_name);
	if (veStrMapWalkFirst(w) == 0) {
	  do {
	    num_threads++;
	  } while (veStrMapWalkNext(w) == 0);
	}
	b_start = veThrBarrierCreate(num_threads+1);
	b_entry = veThrBarrierCreate(num_threads+1);
	b_exit = veThrBarrierCreate(num_threads+1);
	if (veStrMapWalkFirst(w) == 0) {
	  do {
	    VeThread *t;
	    t = veThreadCreate();
	    /* try to create a kernel thread, if possible */
	    veThreadInit(t,render_thread,(void *)veStrMapWalkStr(w),1,
			 VE_THR_KERNEL);
	    veStrMapInsert(threads_by_name,veStrMapWalkStr(w),t);
	  } while (veStrMapWalkNext(w) == 0);
	}
	veStrMapWalkDestroy(w);
	veThrBarrierEnter(b_start);
      }
      if (!veMPIsAsync())
	veMPRespond(0);
      else {
	VE_DEBUG(4,("render_ctrl_handler - async client deliberately not responing"));
      }
    }
    break;
    
  case VE_MPCTRL_RENDER:
    VE_DEBUG(3,("render_ctrl_handler - rendering frame"));
    if (VE_ISDEBUG(4)) {
      VeFrame *o = veGetOrigin(), *e = veGetDefaultEye();
      VE_DEBUG(4,("origin = {%g %g %g} {%g %g %g} {%g %g %g}",
		  o->loc.data[0], o->loc.data[1], o->loc.data[2],
		  o->dir.data[0], o->dir.data[1], o->dir.data[2],
		  o->up.data[0], o->up.data[1], o->up.data[2]));
      VE_DEBUG(4,("eye = {%g %g %g} {%g %g %g} {%g %g %g}",
		  e->loc.data[0], e->loc.data[1], e->loc.data[2],
		  e->dir.data[0], e->dir.data[1], e->dir.data[2],
		  e->up.data[0], e->up.data[1], e->up.data[2]));
    }
    veThrMutexLock(render_mutex);
    veThrBarrierEnter(b_entry); /* let them start */
    veThrBarrierEnter(b_exit); /* wait for them to finish */
    veThrMutexUnlock(render_mutex);
    if (!veMPIsAsync())
      veMPRespond(0); /* indicate that we are done here */
    else {
      VE_DEBUG(4,("render_ctrl_handler - async client deliberately not responding"));
    }
    veiGlUpdateFrameRate();
    break;

  case VE_MPCTRL_SWAP:
    {
      VeiGlxWindow *gw;
      int i;
      VE_DEBUG(5,("render_ctrl_handler - swapping buffers"));

      veThrMutexLock(render_mutex);
      for(i = 0; i < num_windows; i++) {
	gw = (VeiGlxWindow *)(all_windows[i]->udata);
	VE_DEBUG(5,("render_ctrl_handler - swapping win %d 0x%x gw 0x%x",
		    i,all_windows[i]->udata,gw));
	glXMakeCurrent(gw->dpy,gw->win,gw->ctx);
	if (!glx_timing_test)
	  glXSwapBuffers(gw->dpy,gw->win);
	glXMakeCurrent(gw->dpy,None,NULL);
      }
      veThrMutexUnlock(render_mutex);

      VE_DEBUG(5,("render_ctrl_handler - buffers swapped"));
    }
    break;

  default:
    veWarning(MODULE,"unexpected ctrl message: %d",tag);
  }
}

void veiGlxCoreInit() {
  progAttrList[0] = None; 
  attributeList[0] = None;
  /* tell the multi-processing layer to send render commands to me... */
  veMPAddIntHandler(VE_MPMSG_CTRL,VE_DTAG_ANY,render_ctrl_handler);
  render_mutex = veThrMutexCreate();
  pipelist_mutex = veThrMutexCreate();
  plist_mutex = veThrMutexCreate();
}

void veRenderInit() {
  /* workaround for display list bug on Onyx2 IR2 graphics pipe */
  /* If this gets fixed, take this out - it may have some impact on
     performance */
  putenv("GLKONA_NOLDMA=1");

  XInitThreads();
  veiGlInit();
  veiGlxCoreInit();
}

void veRenderAllocMP() {
  /* At this point I'm not going to worry about imposing any restrictions
     on the MP settings in the library. */
}

void veRenderRun(void) {
  char *s;
  if ((s = veGetOption("glx_shared_ctx")))
    use_shared_ctx = atoi(s);
  if ((s = veGetOption("glx_timing_test")) && atoi(s))
    glx_timing_test = 1;
  veiGlRenderRun();
  if (veMPIsMaster()) {
    veMPSendCtrl(VE_MPTARG_ALL,VE_MPCTRL_RUN,NULL);
    /* wait for windows to be opened... */
    veMPGetResponse(VE_MPTARG_ALL);
  }
}

void veRender(void) {
  veiGlPreDisplay();
  /* update state information */
  veMPLocationPush();
  veMPPushStateVar(VE_DTAG_ANY,VE_MP_AUTO);
  /* let renderers run */
  veMPSendCtrl(VE_MPTARG_ALL,VE_MPCTRL_RENDER,NULL);
  veMPGetResponse(VE_MPTARG_ALL);
  /* now that everybody's back, swap */
  veMPSendCtrl(VE_MPTARG_ALL,VE_MPCTRL_SWAP,NULL);
  veiGlPostDisplay();
}
