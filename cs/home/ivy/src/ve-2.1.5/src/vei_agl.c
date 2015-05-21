/* 
   This file is based upon: the following Apple source code.  Apart
   from being edited to run on VE.  Non-MacOSX-specific stuff has been
   stripped out for simplicity.

   File:		SetupGL Main Windowed.c

   Contains:	An example of the use of the SeupGL utility code for windowed applications.

   Written by:	Geoff Stahl

   Copyright:	2000 Apple Computer, Inc., All Rights Reserved
 
   Change History (most recent first):


   Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
   ("Apple") in consideration of your agreement to the following terms, and your
   use, installation, modification or redistribution of this Apple software
   constitutes acceptance of these terms.  If you do not agree with these terms,
   please do not use, install, modify or redistribute this Apple software.

   In consideration of your agreement to abide by the following terms, and subject
   to these terms, Apple grants you a personal, non-exclusive license, under Apple's
   copyrights in this original Apple software (the "Apple Software"), to use,
   reproduce, modify and redistribute the Apple Software, with or without
   modifications, in source and/or binary forms; provided that if you redistribute
   the Apple Software in its entirety and without modifications, you must retain
   this notice and the following text and disclaimers in all such redistributions of
   the Apple Software.  Neither the name, trademarks, service marks or logos of
   Apple Computer, Inc. may be used to endorse or promote products derived from the
   Apple Software without specific prior written permission from Apple.  Except as
   expressly stated in this notice, no other rights or licenses, express or implied,
   are granted by Apple herein, including but not limited to any patent rights that
   may be infringed by your derivative works or by other works in which the Apple
   Software may be incorporated.

   The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
   WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
   WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
   COMBINATION WITH YOUR PRODUCTS.

   IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
   OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
   (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* system includes ---------------------------------------------------------- */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ve_thread.h>
#include <ve_error.h>
#include <ve_stats.h>
#include <ve_mp.h>
#include <ve_util.h>
#include <ve_device.h>
#include <ve_keysym.h>
#include <vei_agl.h>

/* project includes --------------------------------------------------------- */

#include "Carbon_SetupGL.h"
#include "Carbon_Error_Handler.h"

/* prototypes --------------------------------------------------------------- */

#define MODULE "vei_agl"

/* Keep this simple - only one rendering thread */
static VeStrMap threads_by_name = NULL;
static VeStrMap threads_by_window = NULL;
static VeWindow **all_windows = NULL;
static int num_windows = 0;
static int num_threads = 0;
static VeThrBarrier *b_start, *b_entry, *b_exit;

static void InitToolbox(void);
static Boolean SetUp (void);
static void DoMenu (SInt32 menuResult);
static void DoUpdate (WindowRef pWindow);
static void DoEvent (void);

static GLint progAttrList[64];

/* For simplicity, assume that we are on a single screen.
   This should probably be fixed at some point in the future. */
/* These are wrong, but are corrected whenever a window is created */
static int scr_width = 640, scr_height = 480;

/* statics/globals (internal only) ------------------------------------------ */

/* Menu defs */
enum 
  {
    kMaxWindows = 200,
	
    kMenuApple = 128,
    kMenuFile = 129,
	
    kAppleAbout = 1,
    kFileQuit
  };

enum 
  {
    kForegroundSleep = 100000,
    kBackgroundSleep = 100000,
  };

static SInt32 gSleepTime = kForegroundSleep;
static Boolean gDone = false, gfFrontProcess = true;
static Boolean gMenu = false;

/* functions (internal/private) --------------------------------------------- */

/* -------------------------------------------------------------------------- */
#pragma mark -


/* This is a slightly hacky shim to provide keboard/mouse input
   to VE programs running under MacOSX. */
#define MAX_PHONY_DEVS 16
static char *keyboard_names[MAX_PHONY_DEVS];
static char *mouse_names[MAX_PHONY_DEVS];

static VeDevice *new_phony_driver(VeDeviceDriver *driver,
				  VeDeviceDesc *desc,
				  VeStrMap override) {
  int k;
  VeDeviceInstance *i;
  VeDevice *d;
  int iskbd = 0;
  char *drvname = "mouse";

  assert(desc != NULL);
  assert(desc->name != NULL);
  assert(desc->type != NULL);

  if (strcmp(driver->type,"mackeyboard") == 0 ||
      strcmp(driver->type,"keyboard") == 0)
    iskbd = 1;

  /* else it's a mouse... */
  VE_DEBUG(1,("vei_agl - creating %s driver shim",driver->type));

  for(k = 0; k < MAX_PHONY_DEVS; k++) {
    if (iskbd) {
      if (!keyboard_names[k]) {
	keyboard_names[k] = veDupString(desc->name);
	break;
      }
    } else {
      if (!mouse_names[k]) {
	mouse_names[k] = veDupString(desc->name);
	break;
      }
    }
  }
  if (k >= MAX_PHONY_DEVS)
    veFatalError(MODULE,"too many phony %s drivers in use",driver->type);
  i = veDeviceInstanceInit(driver,NULL,desc,override);
  i->idata = (void *)k;
  /* no options */
  d = veDeviceCreate(desc->name);
  d->instance = i;
  return d;
}

static void add_devices(void) {
  /* add "keyboard" and "mouse" devices */
  static VeDeviceDriver kbd = { "mackeyboard", new_phony_driver, NULL };
  static VeDeviceDriver mouse = { "macmouse", new_phony_driver, NULL };
  VeDeviceDesc *d;
  veDeviceAddDriver(&kbd);
  veDeviceAddDriver(&mouse);
  d = veDeviceDescCreate();
  /* default devices (no manifest required) */
  if (!veFindDeviceDesc("keyboard")) {
    d->name = veDupString("keyboard");
    d->type = veDupString("mackeyboard");
    veAddDeviceDesc(d);
  }
  d = veDeviceDescCreate();
  if (!veFindDeviceDesc("mouse")) {
    d->name = veDupString("mouse");
    d->type = veDupString("macmouse");
    veAddDeviceDesc(d);
  }
}

static pascal OSStatus QuitAppleEventHandler( const AppleEvent *appleEvt, AppleEvent* reply, SInt32 refcon )
{
#pragma unused (appleEvt, reply, refcon)
  gDone =  true;
  exit(0);
  return false;
}


static pascal OSStatus mouseHandler( EventHandlerCallRef nextHandler, EventRef theEvent, void *userData ) {
  UInt32 kind;
  int i;
  OSStatus result = eventNotHandledErr;
  kind = GetEventKind(theEvent);
  switch (kind) {
  case kEventMouseDown:
  case kEventMouseUp:
    for(i = 0; i < MAX_PHONY_DEVS; i++) {
      if (mouse_names[i]) {
	VeDeviceEvent *ve;
	VeDeviceE_Switch *sw;
	EventMouseButton mb;
	ve = veDeviceEventCreate(VE_ELEM_SWITCH,0);
	ve->device = veDupString(mouse_names[i]);
	/* identify mouse button */
	mb = 0;
	GetEventParameter(theEvent,kEventParamMouseButton,
			  typeMouseButton,NULL,sizeof(mb),
			  NULL,&mb);
	switch (mb) {
	case kEventMouseButtonPrimary: 
	  ve->elem = veDupString("left");
	  break;
	case kEventMouseButtonSecondary:
	  ve->elem = veDupString("right");
	  break;
	case kEventMouseButtonTertiary:
	  ve->elem = veDupString("middle");
	  break;
	default: 
	  {
	    char str[255];
	    sprintf(str,"button%d",(int)mb);
	    ve->elem = veDupString(str);
	  }
	  break;
	}
	ve->timestamp = veClock();
	sw = VE_EVENT_SWITCH(ve);
	sw->state = (kind == kEventMouseDown) ? 1 : 0;
	VE_DEBUG(1,("macmouse %s: button %s %d",ve->device,ve->elem,
		    sw->state));
	veDeviceInsertEvent(ve);
      }
    }
    result = noErr;
    break;
  case kEventMouseMoved:
  case kEventMouseDragged:
    for(i = 0; i < MAX_PHONY_DEVS; i++) {
      if (mouse_names[i]) {
	VeDeviceEvent *ve;
	VeDeviceE_Vector *v;
	HIPoint pt;
	ve = veDeviceEventCreate(VE_ELEM_VECTOR,2);
	ve->device = veDupString(mouse_names[i]);
	ve->elem = veDupString("axes");
	ve->timestamp = veClock();
	/* get position */
	GetEventParameter(theEvent,kEventParamMouseLocation,
			  typeHIPoint,NULL,sizeof(pt),
			  NULL,&pt);
	v = VE_EVENT_VECTOR(ve);
	v->min[0] = v->min[1] = -1.0;
	v->max[0] = v->max[1] = 1.0;
	v->value[0] = (2.0*pt.x/(float)(scr_width))-1.0;
	v->value[1] = (2.0*pt.y/(float)(scr_height))-1.0;
	veDeviceInsertEvent(ve);
      }
    }
    result = noErr;
    break;
  }
  return result;
}

static pascal OSStatus keybHandler( EventHandlerCallRef nextHandler, EventRef theEvent, void *userData ) {
  OSStatus result = eventNotHandledErr;
  UInt32 kind;
  int i;
  kind = GetEventKind(theEvent);
  switch (kind) {
  case kEventRawKeyDown:
  case kEventRawKeyUp:
    for(i = 0; i < MAX_PHONY_DEVS; i++) {
      if (keyboard_names[i]) {
	VeDeviceEvent *ve;
	VeDeviceE_Keyboard *k;
	char c[2];
	ve = veDeviceEventCreate(VE_ELEM_KEYBOARD,0);
	ve->device = veDupString(keyboard_names[i]);
	ve->timestamp = veClock();
	k = VE_EVENT_KEYBOARD(ve);
	k->state = (kind == kEventRawKeyDown) ? 1 : 0;
	c[0] = '\0';
	GetEventParameter(theEvent,kEventParamKeyMacCharCodes,
			  typeChar,NULL,sizeof(char),NULL,c);
	if (isalnum(c[0])) {
	  k->key = (int)c[0];
	  c[1] = '\0';
	  ve->elem = veDupString(c);
	} else {
	  char elem[255];
	  UInt32 keycode;
	  GetEventParameter(theEvent,kEventParamKeyCode,
			    typeUInt32,NULL,sizeof(UInt32),NULL,&keycode);
	  /* this is poor */
	  k->key = VEK_UNKNOWN;
	  sprintf(elem,"0x%04x",keycode);
	  ve->elem = veDupString(elem);
	}
	veDeviceInsertEvent(ve);
      }
    }
    result = noErr;
    break;
  }
  return result;
}

static pascal OSStatus winHandler( EventHandlerCallRef nextHandler, EventRef theEvent, void *userData ) {
  OSStatus result = eventNotHandledErr;
  VeWindow *w = (VeWindow *)userData;
  VeiAglWindow *gw = (VeiAglWindow *)(w->udata);
  /* I'll be cleverer here later */
  vePostRedisplay();
  return result;
}

/* -------------------------------------------------------------------------- */

static void InitToolbox(void)
{
  OSStatus err;
  long response;
  MenuHandle menu;
  EventTargetRef target;
	
  InitCursor();
  /* Should not need to add menus under Mac OS X */
  DrawMenuBar();

#if 0
  if ((err = AEInstallEventHandler( kCoreEventClass, kAEQuitApplication, 
				    NewAEEventHandlerUPP(QuitAppleEventHandler), 0, false )) != noErr)
    ExitToShell();
#endif

  target = GetApplicationEventTarget();
  { 
    EventTypeSpec specs[] = {
      { kEventClassMouse, kEventMouseDown },
      { kEventClassMouse, kEventMouseUp },
      { kEventClassMouse, kEventMouseMoved },
      { kEventClassMouse, kEventMouseDragged }
    };
    int nspecs = 4;
    InstallEventHandler(target, NewEventHandlerUPP(mouseHandler), 
			nspecs, specs, NULL, NULL);
  }
  {
    EventTypeSpec specs[] = {
      { kEventClassKeyboard, kEventRawKeyDown },
      { kEventClassKeyboard, kEventRawKeyUp }
    };
    int nspecs = 2;
    InstallEventHandler(target, NewEventHandlerUPP(keybHandler),
			nspecs, specs, NULL, NULL);
  }
}

/* -------------------------------------------------------------------------- */

static Boolean SetUp (void)
{
  InitToolbox ();
  if (!PreflightGL (false))
    veFatalError(MODULE, "Preflight check failed?");
  return true;
}

/* -------------------------------------------------------------------------- */

/* note the nasty use of numeric offsets in the following function
   - change the layout of attributeList and you need to change these too...
   */
static void optToAttList(VeWindow *w, structGLWindowInfo *glInfo) {
  char *s;
  int i = 0, alpha = 0;

  while (progAttrList[i] != AGL_NONE) {
	glInfo->aglAttributes[i] = progAttrList[i];
	i++;
  }
  glInfo->aglAttributes[i++] = AGL_RGBA;  /* no supprt for colormap mode... */
  glInfo->aglAttributes[i++] = AGL_ACCELERATED;
  glInfo->aglAttributes[i++] = AGL_NO_RECOVERY;
  if ((s = veiGlGetOption(w,"doublebuffer")) && atoi(s))
    glInfo->aglAttributes[i++] = AGL_DOUBLEBUFFER; 
  if (s = veiGlGetOption(w,"red")) {
    glInfo->aglAttributes[i++] = AGL_RED_SIZE;
    glInfo->aglAttributes[i++] = atoi(s);
  }
  if (s = veiGlGetOption(w,"green")) {
    glInfo->aglAttributes[i++] = AGL_GREEN_SIZE;
    glInfo->aglAttributes[i++] = atoi(s);
  }
  if (s = veiGlGetOption(w,"blue")) {
    glInfo->aglAttributes[i++] = AGL_BLUE_SIZE;
    glInfo->aglAttributes[i++] = atoi(s);
  }
  if (s = veiGlGetOption(w,"alpha")) {
    int k;
    k = atoi(s);
    glInfo->aglAttributes[i++] = AGL_ALPHA_SIZE;
    glInfo->aglAttributes[i++] = k;
    if (k > 0)
      alpha = 1;
  }
  if (s = veiGlGetOption(w,"depth")) {
    glInfo->aglAttributes[i++] = AGL_DEPTH_SIZE;
    glInfo->aglAttributes[i++] = atoi(s);
  }
  if (s = veiGlGetOption(w,"accum")) {
    int k;
    k = atoi(s);
    glInfo->aglAttributes[i++] = AGL_ACCUM_RED_SIZE;
    glInfo->aglAttributes[i++] = k;
    glInfo->aglAttributes[i++] = AGL_ACCUM_GREEN_SIZE;
    glInfo->aglAttributes[i++] = k;
    glInfo->aglAttributes[i++] = AGL_ACCUM_BLUE_SIZE;
    glInfo->aglAttributes[i++] = k;
    if (alpha) {
      glInfo->aglAttributes[i++] = AGL_ACCUM_RED_SIZE;
      glInfo->aglAttributes[i++] = k;
    }
  }
  if ((w->eye == VE_WIN_STEREO) ||
      ((s = veiGlGetOption(w,"stereo")) && atoi(s)))
    glInfo->aglAttributes[i++] = AGL_STEREO;
  glInfo->aglAttributes[i++] = AGL_NONE;
}

static void open_window(char *tname, VeWindow *w) {
  structGLWindowInfo *glInfo;
  GDHandle hGDWindow;
  Rect rectWin;
  VeiAglWindow *gw;
  int x,y,width,height;
  char *dname, *s, ext[1024];

  VE_DEBUG(1,("opening window %s", w->name));

  gw = malloc(sizeof(VeiAglWindow));
  assert(gw != NULL);
  w->udata = gw;
  gw->glInfo = malloc(sizeof(structGLWindowInfo));
  assert(gw->glInfo != NULL);
  glInfo = (structGLWindowInfo *)(gw->glInfo);

  gw->stereo = (w->eye == VE_WIN_STEREO);

  if (w->geometry == NULL)
    veFatalError(MODULE, "geometry missing on window %s", w->name);

  veiGlParseGeom(w->geometry,&x,&y,&width,&height);

  rectWin.left = x;
  rectWin.top = y;
  rectWin.right = x+width;
  rectWin.bottom = y+height;
  if (noErr != CreateNewWindow(kDocumentWindowClass, 
			       kWindowStandardDocumentAttributes,
			       &rectWin, &gw->win)) {
    veFatalError(MODULE, "CreateNewWindow error");
  }

  /* update screen size */
  FindGDHandleFromRect(&rectWin,&hGDWindow);
  scr_width = (**hGDWindow).gdRect.right - (**hGDWindow).gdRect.left;
  scr_height = (**hGDWindow).gdRect.bottom - (**hGDWindow).gdRect.top;

  { 
    /* set title */
    Str255 pstrTitle = "\p";
    strncat(pstrTitle,w->name,80);
    SetWTitle(gw->win,pstrTitle);
  }

  ShowWindow(gw->win);
  SelectWindow(gw->win);
  SetPortWindowPort(gw->win);

  glInfo->fAcceleratedMust = true;
  glInfo->VRAM = 0 * 1048576;
  glInfo->textureRAM = 0 * 1048576;
  glInfo->fDraggable = CheckMacOSX() ? true : false;
  glInfo->fmt = 0;

  /* resolve options */
  optToAttList(w,glInfo);

  BuildGLFromWindow(gw->win,&gw->ctx,glInfo,NULL);
  if (!gw->ctx)
    veFatalError(MODULE, "could not create context");

  {
    Rect rectPort;
    GetWindowPortBounds(gw->win,&rectPort);
    gw->x = rectPort.left;
    gw->y = rectPort.top;
    gw->w = rectPort.right - rectPort.left;
    gw->h = rectPort.top - rectPort.bottom;
  }

  aglSetCurrentContext(gw->ctx);
  aglReportError();
  aglUpdateContext(gw->ctx);
  aglReportError();
  
  /* blank the screen */
  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
  aglSwapBuffers(gw->ctx);

  /* register event handler for this window */
  {
    EventTypeSpec specs[] = {
      { kEventClassWindow, kEventWindowDrawContent },
      { kEventClassWindow, kEventWindowUpdate }
    };
    int nspecs = 2;
    EventTargetRef target = GetWindowEventTarget(gw->win);
    InstallEventHandler(target, NewEventHandlerUPP(winHandler),
			nspecs, specs, (void *)w, NULL);
    InstallStandardEventHandler(target);
  }
  
  /* we should really lock callbacks while we are doing this */
  veLockCallbacks();
  veiGlSetupWindow(w);
  veUnlockCallbacks();

  aglSetCurrentContext(NULL);
}

static void close_window(VeWindow *w) {
  VeiAglWindow *gw = (VeiAglWindow *)(w->udata);
  
  if (gw != NULL) {
    DestroyGLFromWindow(&gw->ctx,gw->glInfo);
    DisposeWindow(gw->win);
    free(gw->glInfo);
    free(gw);
  }
}

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
      VeiAglWindow *gw = (VeiAglWindow *)(my_windows[i]->udata);
      aglSetCurrentContext(gw->ctx);
      aglUpdateContext(gw->ctx);
      /* set modes to default */
      for(j = 0; j < sizeof(mmodes)/sizeof(GLenum); j++) {
	glMatrixMode(mmodes[j]);
	glLoadIdentity();
      }
      glViewport(0,0,gw->w,gw->h);
      veiGlRenderWindow(my_windows[i]);
    }
    aglSetCurrentContext(NULL);
    veThrBarrierEnter(b_exit);
  }
}

/* Important - in the MP model, this call will only work on the slave
   where the window is being rendered (which should be okay) */
void veRenderGetWinInfo(VeWindow *win, int *x, int *y, int *w, int *h) {
  VeiAglWindow *gw = (VeiAglWindow *)(win->udata);
  if (gw) {
    if (x) *x = gw->x;
    if (y) *y = gw->y;
    if (w) *w = gw->w;
    if (h) *h = gw->h;
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
      if (!threads_by_window)
	threads_by_window = veStrMapCreate();

      if (veStrMapLookup(threads_by_window,w->name))
	return; /* we already know about this window... */

      thr = w->slave.thread ? w->slave.thread : "auto";
      if (!veStrMapLookup(threads_by_name,thr)) {
	/* new thread */
	veStrMapInsert(threads_by_name,thr,(void *)1);
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
	    veThreadInit(t,render_thread,(void *)veStrMapWalkStr(w),0,0);
	    veStrMapInsert(threads_by_name,veStrMapWalkStr(w),t);
	  } while (veStrMapWalkNext(w) == 0);
	}
	veStrMapWalkDestroy(w);
	veThrBarrierEnter(b_start);
      }
      veMPRespond(0);
    }
    break;
    
  case VE_MPCTRL_RENDER:
    VE_DEBUG(5,("render_ctrl_handler - rendering frame"));
    veThrBarrierEnter(b_entry); /* let them start */
    veThrBarrierEnter(b_exit); /* wait for them to finish */
    veMPRespond(0); /* indicate that we are done here */
    veiGlUpdateFrameRate();
    break;

  case VE_MPCTRL_SWAP:
    {
      VeiAglWindow *gw;
      int i;
      VE_DEBUG(5,("render_ctrl_handler - swapping buffers"));
      for(i = 0; i < num_windows; i++) {
	gw = (VeiAglWindow *)(all_windows[i]->udata);
	VE_DEBUG(5,("render_ctrl_handler - swapping win %d 0x%x gw 0x%x",
		    i,all_windows[i]->udata,gw));
	aglSetCurrentContext(gw->ctx);
	aglSwapBuffers(gw->ctx);
      }
      aglSetCurrentContext(NULL);
      VE_DEBUG(5,("render_ctrl_handler - buffers swapped"));
    }
    break;
  }
}

/* -------------------------------------------------------------------------- */

void veiAglInit(void) {
  add_devices();
  if (!SetUp ())
    veFatalError(MODULE,"failed to initialize");
}

/* we don't support shared contexts under AGL for now, so always claim
   we are in the master */
int veiGlIsMaster(void) { return 1; }

void veRenderInit() {
  veiGlInit();
  veiAglInit();
  progAttrList[0] = AGL_NONE;
  veMPAddIntHandler(VE_MPMSG_CTRL,VE_DTAG_ANY,render_ctrl_handler);
}

void veRenderAllocMP() {}

void veRenderRun() {
  veiGlRenderRun();
  veMPSendCtrl(VE_MPTARG_ALL,VE_MPCTRL_RUN,NULL);
  veMPGetResponse(VE_MPTARG_ALL);
}

void veRender(void) {
  veiGlPreDisplay();
  veMPLocationPush();
  veMPPushStateVar(VE_DTAG_ANY,VE_MP_AUTO);
  veMPSendCtrl(VE_MPTARG_ALL,VE_MPCTRL_RENDER,NULL);
  veMPGetResponse(VE_MPTARG_ALL);
  veMPSendCtrl(VE_MPTARG_ALL,VE_MPCTRL_SWAP,NULL);
  veiGlPostDisplay();
}
