
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ve.h>
#include <vei_gl.h>
#include <ve_audio.h>

#include "skyfly.h"

/* maximum active sounds at once */
#define MAX_SOUNDS 32

#define FRAME_INTERVAL 10
#define AUDIO_INTERVAL 30
float time_factor = 1.0;
float game_speed = 20.0;

int use_stick = 1;
int use_tracker = 1;
int draw_stats = 0;
int use_audio = 0;

void create_shot(void);
void init_misc(void);
void init_skyfly(void);
void sim_singlechannel(void);
void handle_draw(VeWindow *, VeWallView *);
void set_controls(float, float, float);

#define SKY_R 0.23f
#define SKY_G 0.35f
#define SKY_B 0.78f

#define TERR_DARK_R 0.27f
#define TERR_DARK_G 0.18f
#define TERR_DARK_B 0.00f

#define TERR_LITE_R 0.24f
#define TERR_LITE_G 0.53f
#define TERR_LITE_B 0.05f

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} palette_t;

palette_t       pal[256];
static int buttons[BUTCOUNT] = { 0 };
static int mouse_x, mouse_y;

GLboolean show_timer = GL_FALSE;
GLboolean fullscreen = GL_TRUE;

#ifdef USE_AUDIO
/* use ve_audio's internal limit */
VeAudioSound *expl_sound, *shot_sound;
char *expl_sound_file = "../sounds/boom.wav";
char *shot_sound_file = "../sounds/cannon.aiff";
static struct {
  VeAudioInst *instance;
  float pos[3];
} active_sounds[MAX_SOUNDS];
static float last_audio_camera[3];
#endif /* USE_AUDIO */

void init_audio() {
#ifdef USE_AUDIO
  int i;
  expl_sound = veAudioFind(expl_sound_file);
  if (!expl_sound)
    veWarning("skyfly","cannot find sound file %s", expl_sound_file);
  shot_sound = veAudioFind(shot_sound_file);
  if (!expl_sound)
    veWarning("skyfly","cannot find sound file %s", shot_sound_file);
  for(i = 0; i < MAX_SOUNDS; i++)
    active_sounds[i].instance = NULL;
#endif /* USE_AUDIO */
}

void audio_upd_params(VeAudioParams *p, float *pos, float *camera) {
#ifdef USE_AUDIO
  float d2;
  float vol;
  int i;

  d2 = 0.0;
  for(i = 0; i < 3; i++)
    d2 += (pos[i]-camera[i])*(pos[i]-camera[i]);
  /* d2 is the squared distance between the sound position and the camera */
  if (d2 == 0.0)
    vol = 1.0;
  else { 
    vol = 1/d2;
    if (vol > 1.0)
      vol = 1.0;
    if (vol < 1.0e-6)
      vol = 0.0;
  }
  /* vol = 1.0; */
  veAudioSetVolume(p,vol);
#endif /* USE_AUDIO */
}

void audio_add_sound(VeAudioSound *snd, float *pos) {
#ifdef USE_AUDIO
  int i,j;
  VeAudioParams p;

  if (!use_audio)
    return; /* audio disabled */
  
  if (!snd)
    return; /* no sfx loaded */

  for(i = 0; i < MAX_SOUNDS; i++)
    if (!active_sounds[i].instance)
      break;

  if (i >= MAX_SOUNDS)
    return; /* no slots left */

  veAudioInitParams(&p);
  audio_upd_params(&p,pos,last_audio_camera);
  active_sounds[i].instance = veAudioInstance(NULL,snd,&p);
  for(j = 0; j < 3; j++)
    active_sounds[i].pos[j] = pos[j];
#endif /* USE_AUDIO */
}

void update_sounds(float *camera) {
#ifdef USE_AUDIO
  int i;

  for(i = 0; i < 3; i++)
    last_audio_camera[i] = camera[i];
  for(i = 0; i < MAX_SOUNDS; i++)
    if (active_sounds[i].instance) {
      audio_upd_params(veAudioGetInstParams(active_sounds[i].instance),
		       active_sounds[i].pos,camera);
      veAudioRefreshInst(active_sounds[i].instance);
      if (veAudioIsDone(active_sounds[i].instance)) {
	veAudioClean(active_sounds[i].instance,0);
	active_sounds[i].instance = NULL;
      }
    }
#endif /* USE_AUDIO */
}

int Xgetbutton(int button)
{
        int b;
        if (button < 0 || button >= BUTCOUNT)
                return -1;
        b = buttons[button];
        if (button < LEFTMOUSE)
                buttons[button] = 0;
        return b;
}

int Xgetvaluator(int val)
{
    switch (val) {
                case MOUSEX:
                        return mouse_x;
                case MOUSEY:
                        return mouse_y;
                default:
                        return -1;
        }
}

void setPaletteIndex(int i, GLfloat r, GLfloat g, GLfloat b)
{
	pal[i].red = (255.0F * r);
	pal[i].green = (255.0F * g);
	pal[i].blue = (255.0F * b);      
}                                                           

void init_cmap(void)
{
	int 		ii, jj, color;
    GLfloat 	r0, g0, b0, r1, g1, b1;

    /* Set up color map */
	color = 10;
	memset(pal,0,sizeof(pal));

    /* Sky colors */
    sky_base = color;
    r0 = SKY_R; r1 = 1.0f;
    g0 = SKY_G; g1 = 1.0f;
    b0 = SKY_B; b1 = 1.0f;
    for (ii = 0; ii < SKY_COLORS; ii++) {
        GLfloat p, r, g, b;
        p = (GLfloat) ii / (SKY_COLORS-1);
        r = r0 + p * (r1 - r0);
        g = g0 + p * (g1 - g0);
        b = b0 + p * (b1 - b0);
        for (jj = 0; jj < FOG_LEVELS; jj++) {
            GLfloat fp, fr, fg, fb;
            fp = (FOG_LEVELS > 1) ? (GLfloat) jj / (FOG_LEVELS-1) : 0.0f;
            fr = r + fp * (fog_params[0] - r);
            fg = g + fp * (fog_params[1] - g);
            fb = b + fp * (fog_params[2] - b);
            setPaletteIndex(sky_base + (ii*FOG_LEVELS) + jj, fr, fg, fb);
        }
    }
    color += (SKY_COLORS * FOG_LEVELS);

    /* Terrain colors */
    terr_base = color;
    r0 = TERR_DARK_R;   r1 = TERR_LITE_R;
    g0 = TERR_DARK_G;   g1 = TERR_LITE_G;
    b0 = TERR_DARK_B;   b1 = TERR_LITE_B;
    for (ii = 0; ii < TERR_COLORS; ii++) {
        GLfloat p, r, g, b;
        p = (GLfloat) ii / (TERR_COLORS-1);
        r = r0 + p * (r1 - r0);
        g = g0 + p * (g1 - g0);
        b = b0 + p * (b1 - b0);
        for (jj = 0; jj < FOG_LEVELS; jj++) {
            GLfloat fp, fr, fg, fb;
            fp = (FOG_LEVELS > 1) ? (GLfloat) jj / (FOG_LEVELS-1) : 0.0f;
            fr = r + fp * (fog_params[0] - r);
            fg = g + fp * (fog_params[1] - g);
            fb = b + fp * (fog_params[2] - b);
            setPaletteIndex(terr_base + (ii*FOG_LEVELS) + jj, fr, fg, fb);
        }
	}
    color += (TERR_COLORS * FOG_LEVELS);

    /* Plane colors */
    plane_colors[0] = color;
    plane_colors[1] = color + (PLANE_COLORS/2);
    plane_colors[2] = color + (PLANE_COLORS-1);
    r0 = 0.4; r1 = 0.8;
    g0 = 0.4; g1 = 0.8;
    b0 = 0.1; b1 = 0.1;
    for (ii = 0; ii < PLANE_COLORS; ii++) {
        GLfloat p, r, g, b;
        p = (GLfloat) ii / (PLANE_COLORS);
        r = r0 + p * (r1 - r0);
        g = g0 + p * (g1 - g0);
        b = b0 + p * (b1 - b0);
        setPaletteIndex(plane_colors[0] + ii, r, g, b);
    }
	color += PLANE_COLORS;
#if 0
	GM_setPalette(pal,256,0);
	GM_realizePalette(256,0,true);
#endif
}

void gameLogic(void *v)
{
  static long lasttm = -1;
  if (lasttm < 0) {
    lasttm = veClock();
    time_factor = game_speed;
  } else {
    long t = veClock();
    time_factor = (t-lasttm)*1.0e-3*game_speed;
    lasttm = t;
  }
  
  sim_singlechannel();
  update_view_pos();
  /* draw(); */
  vePostRedisplay();
  if (v) {
    veAddTimerProc(FRAME_INTERVAL,gameLogic,v);
  }
}

int                 lastCount;      /* Timer count for last fps update */
int                 frameCount;     /* Number of frames for timing */
int                 fpsRate;        /* Current frames per second rate */

void draw(VeWindow *w, long tm, VeWallView *wv)
{
    /* Draw the frame */
    handle_draw(w,wv);
    if (draw_stats) {
      veiGlPushTextMode(w);
      glColor3f(1.0,1.0,1.0);
      veiGlDrawStatistics(NULL,0);
      veiGlPopTextMode();
    }
}

void
reshape(int width, int height)
{
    Wxsize = width;
    Wysize = height;

    mouse_x = Wxsize/2;
    mouse_y = Wysize/2;

    glViewport(0, 0, width, height);
}

/* ARGSUSED1 */
int keyboard(VeDeviceEvent *e, void *arg)
{
  VeDeviceE_Keyboard *kbd;
  if (VE_EVENT_TYPE(e) != VE_ELEM_KEYBOARD)
    return 0;
  kbd = VE_EVENT_KEYBOARD(e);
  if (!kbd->state)
    return 0;
  switch (kbd->key) {
  case 'r':
    buttons[RKEY] = 1;
    break;
  case '.':
    buttons[PERIODKEY] = 1;
    break;
  case ' ':
    buttons[SPACEKEY] = 1;
    break;
  case 'f':
    set_fog(!fog);
    break;
  case 'd':
    set_dither(!dither);
    break;
  case 't':
    show_timer = !show_timer;
    break;
  }
  return 0;
}

int valfunc(VeDeviceEvent *e, void *arg) {
  static float x = 0.0, y = 0.0, throttle = 0.5;
  VeDeviceE_Valuator *val;
  float v,min,max;

  if (VE_EVENT_TYPE(e) != VE_ELEM_VALUATOR)
    return 0;
  val = VE_EVENT_VALUATOR(e);
  
  min = (val->min != 0.0) ? val->min : -1.0;
  max = (val->max != 0.0) ? val->max : 1.0;
  v = ((val->value-min)/(max-min)*2.0)-1.0;

  if (strcmp(e->elem,"x") == 0)
    x = v;
  else if (strcmp(e->elem,"y") == 0)
    y = v;
  else if (strcmp(e->elem,"throttle") == 0)
    throttle = v;
  set_controls(x,y,throttle);
  return 0;
}

int shoot(VeDeviceEvent *e, void *arg) {
  if (VE_EVENT_TYPE(e) == VE_ELEM_SWITCH) {
    VeDeviceE_Switch *s;
    s = VE_EVENT_SWITCH(e);
    if (!s->state)
      return 0;
  }
  create_shot();
  return 0;
}

int exitfunc(VeDeviceEvent *e, void *arg) {
  exit(0);
  return 0;
}

int
main(int argc, char **argv)
{
  char *c;

  veInit(&argc, argv);
  if (c = veGetOption("use_audio"))
    use_audio = atoi(c);

#ifdef USE_AUDIO  
  if (use_audio) {
    veAudioInit();
    init_audio();
  }
#endif /* USE_AUDIO */

  veSetOption("doublebuffer", "1");
  veSetOption("depth", "1");
  veSetOption("alpha", "1");

  if (c = veGetOption("use_stick"))
    use_stick = atoi(c);
  if (c = veGetOption("draw_stats"))
    draw_stats = atoi(c);
  if (c = veGetOption("use_tracker"))
    use_tracker = atoi(c);

  veiGlSetDisplayCback(draw);
  veDeviceAddCallback(keyboard,NULL,"keyboard");
  veDeviceAddCallback(valfunc,NULL,"controls");
  veDeviceAddCallback(shoot,NULL,"controls.shoot");
  veDeviceAddCallback(exitfunc,NULL,"exit");
  veAddTimerProc(0,gameLogic,(void *)1);

  init_misc();
  if (!rgbmode)
    init_cmap();
  init_skyfly();

  veRun();
  return 0;             /* ANSI C requires main to return int. */
}

