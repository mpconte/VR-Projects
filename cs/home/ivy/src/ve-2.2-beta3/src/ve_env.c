#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_alloc.h>
#include <ve_env.h>
#include <ve_error.h>
#include <ve_util.h>
#include <ve_tlist.h>

#define MODULE "ve_env"
#define ARB 1024

void veViewFree(VeView *v) {
  veFree(v);
}

void veWindowFreeList(VeWindow *w) {
  VeWindow *tmp;

  while(w != NULL) {
    tmp = w->next;
    veFree(w->name);
    veFree(w->display);
    veFree(w->geometry);
    w = tmp;
  }
}

void veOptionFreeList(VeOption *o) {
  VeOption *tmp;
  while(o != NULL) {
    tmp = o->next;
    veFree(o->name);
    veFree(o->value);
    o = tmp;
  }
}

void veWallFreeList(VeWall *w) {
  VeWall *tmp;
  while(w != NULL) {
    tmp = w->next;
    veFree(w->name);
    veFree(w->desc);
    veViewFree(w->view);
    veWindowFreeList(w->windows);
    veOptionFreeList(w->options);
    w = tmp;
  }
}

void veEnvFree(VeEnv *env) {
  if (env != NULL) {
    veFree(env->name);
    veFree(env->desc);
    veWallFreeList(env->walls);
    veOptionFreeList(env->options);
    veFree(env);
  }
}

VeWall *veFindWall(VeEnv *env, char *name) {
  VeWall *w;
  
  for(w = env->walls; w != NULL; w = w->next)
    if (strcmp(w->name,name) == 0)
      break;
  return w;
}

VeWindow *veFindWindow(VeEnv *env, char *name) {
  /* beware - name may be "win" or "wall.win" */
  char wall[ARB], win[ARB], *c;
  VeWall *wl;
  VeWindow *w;

  /* try "wall.win" first */
  if ((c = strchr(name,'.'))) {
    *c = '\0'; /* temporary */
    strcpy(wall,name);
    strcpy(win,c+1);
    *c = '.';
    if ((wl = veFindWall(env,wall)) && 
	(w = veFindWindowInWall(wl,win)))
      return w;
  }
  
  /* if not "x.y" or not found, try looking for the first match in all
     walls */
  w = NULL;
  for(wl = env->walls; wl != NULL; wl = wl->next)
    if ((w = veFindWindowInWall(wl,name)))
      break;
  return w;
}

VeWindow *veFindWindowInWall(VeWall *wl, char *name) {
  VeWindow *w;
  for(w = wl->windows; w != NULL; w = w->next)
    if (strcmp(w->name, name) == 0)
      break;
  return w;
}

char *veEnvGetOption(VeOption *l, char *name) {
  while(l != NULL) {
    if (strcmp(l->name,name) == 0)
      return l->value;
    l = l->next;
  }
  return NULL;
}

VeProfileModule *veProfileFindModule(VeProfile *p, char *name) {
  VeProfileModule *m;
  for(m = p->modules; m; m = m->next)
    if (strcmp(m->name,name) == 0)
      return m;
  return NULL;
}

VeProfileDatum *veProfileFindDatum(VeProfileModule *m, char *name) {
  VeProfileDatum *d;
  for(d = m->data; d; d = d->next)
    if (strcmp(d->name,name) == 0)
      return d;
  return NULL;
}

int veProfileSetDatum(VeProfileModule *m, char *name, char *value) {
  VeProfileDatum *d;
  for(d = m->data; d; d = d->next)
    if (strcmp(d->name,name) == 0)
      break;
  if (d) {
    /* exists already */
    veFree(d->value);
    d->value = veDupString(value);
  } else {
    d = veAllocObj(VeProfileDatum);
    d->name = veDupString(name);
    d->value = veDupString(value);
    d->next = m->data;
    m->data = d;
  }
  return 0;
}

void veProfileFree(VeProfile *p) {
  VeProfileModule *m, *tmp;
  VeProfileDatum *d, *dtmp;
  if (p) {
    veFree(p->name);
    veFree(p->fullname);
    m = p->modules;
    while (m) {
      tmp = m->next;
      veFree(m->name);
      d = m->data;
      while (d) {
	dtmp = d->next;
	veFree(d->name);
	veFree(d->value);
	veFree(d);
	d = dtmp;
      }
      veFree(m);
      m = tmp;
    }
    veFree(p);
  }
}

/* The following need to be reworked somewhat to create valid
   BlueScript files. */
static int escape_write(FILE *stream, char *s) {
  putc('"',stream);
  while (*s != '\0') {
    if (*s == '"' || *s == '$' || *s == '[' || *s == '\\')
      putc('\\',stream);
    putc(*s,stream);
    s++;
  }
  putc('"',stream);
}

static int write_strval(FILE *stream, char *name, char *value, int eol) {
  fprintf(stream,"%s ",name);
  escape_write(stream,value ? value : "");
  if (eol)
    putc('\n',stream);
}

int veEnvWrite(VeEnv *env, FILE *stream) {
  VeWall *wall;
  VeWindow *win;
  VeOption *o;
  if (env) {
    write_strval(stream,"env",env->name,0);
    fprintf(stream, " {\n");
    if (env->desc)
      write_strval(stream,"\tdesc",env->desc,1);
    for(wall = env->walls; wall; wall = wall->next) {
      write_strval(stream,"\twall",wall->name,0);
      fprintf(stream, " {\n");
      if (wall->desc)
	write_strval(stream,"\t\tdesc",wall->desc,1);

      /* dump view */
      switch(wall->view->base) {
      case VE_WALL_ORIGIN:  fprintf(stream,"\t\tbase origin\n"); break;
      case VE_WALL_EYE:     fprintf(stream,"\t\tbase eye\n"); break;
      default:
	veError(MODULE,"veEnvWrite: base has invalid value: %d\n", 
		wall->view->base);
      }
      fprintf(stream, "\t\tloc %g %g %g\n", 
	      wall->view->frame.loc.data[0],
	      wall->view->frame.loc.data[1],
	      wall->view->frame.loc.data[2]);
      fprintf(stream, "\t\tdir %g %g %g\n", 
	      wall->view->frame.dir.data[0],
	      wall->view->frame.dir.data[1],
	      wall->view->frame.dir.data[2]);
      fprintf(stream, "\t\tup %g %g %g\n", 
	      wall->view->frame.up.data[0],
	      wall->view->frame.up.data[1],
	      wall->view->frame.up.data[2]);
      fprintf(stream, "\t\tsize %g %g\n",
	      wall->view->width, wall->view->height);
      /* dump windows */
      for(win = wall->windows; win; win = win->next) {
	write_strval(stream,"\t\twindow",win->name,0);
	fprintf(stream, " {\n");
	if (win->display)
	  write_strval(stream,"\t\t\tdisplay",win->display,1);
	if (win->geometry)
	  write_strval(stream, "\t\t\tgeometry", win->geometry,1);
	if (win->width_err != 0.0 || win->height_err != 0.0)
	  fprintf(stream, "\t\t\terr %g %g\n", win->width_err, win->height_err);
	if (win->offset_x != 0.0 || win->offset_y != 0.0)
	  fprintf(stream, "\t\t\toffset %g %g\n", win->offset_x, win->offset_y);
	for(o = win->options; o; o = o->next) {
	  fprintf(stream,"\t\t\toption ");
	  write_strval(stream,o->name,o->value,1);
	}
	switch(win->eye) {
	case VE_WIN_MONO: fprintf(stream, "\t\t\teye mono\n"); break;
	case VE_WIN_LEFT: fprintf(stream, "\t\t\teye left\n"); break;
	case VE_WIN_RIGHT: fprintf(stream, "\t\t\teye right\n"); break;
	case VE_WIN_STEREO: fprintf(stream, "\t\t\teye stereo\n"); break;
	default:
	  veError(MODULE,"veEnvWrite: invalid value for window eye: %d",
		  win->eye);
	}
	fprintf(stream,"\t\t\tslave ");
	escape_write(stream,win->slave.node ? 
		     win->slave.node : "auto");
	putc(' ',stream);
	escape_write(stream,win->slave.process ?
		     win->slave.process : "auto");
	putc(' ',stream);
	escape_write(stream,win->slave.thread ?
		     win->slave.thread : "auto");
	putc('\n',stream);
	fprintf(stream,"\t\t\tviewport %d %d %d %d\n",
		win->viewport[0], win->viewport[1],
		win->viewport[2], win->viewport[3]);
	fprintf(stream,"\t\t\tdistort {%g %g %g %g} {%g %g %g %g} {%g %g %g %g} {%g %g %g %g}\n",
		win->distort.data[0][0],
		win->distort.data[0][1],
		win->distort.data[0][2],
		win->distort.data[0][3],
		win->distort.data[1][0],
		win->distort.data[1][1],
		win->distort.data[1][2],
		win->distort.data[1][3],
		win->distort.data[2][0],
		win->distort.data[2][1],
		win->distort.data[2][2],
		win->distort.data[2][3],
		win->distort.data[3][0],
		win->distort.data[3][1],
		win->distort.data[3][2],
		win->distort.data[3][3]);
	if (win->async)
	  fprintf(stream,"\t\t\tasync\n");
	fprintf(stream, "\t\t}\n");
      }
      for(o = wall->options; o; o = o->next) {
	fprintf(stream, "\t\toption ");
	write_strval(stream,o->name,o->value,1);
      }
      fprintf(stream,"\t}\n");
    }
    for(o = env->options; o; o = o->next) {
      fprintf(stream, "\toption ");
      write_strval(stream,o->name,o->value,1);
    }
    fprintf(stream,"}\n");
  }
  return 0;
}

int veProfileWrite(VeProfile *profile, FILE *stream) {
  VeProfileModule *m;
  VeProfileDatum *d;
  if (profile) {
    fprintf(stream,"profile ");
    escape_write(stream,profile->name);
    fprintf(stream," {\n");
    if (profile->fullname)
      write_strval(stream,"\tfullname",profile->fullname,1);
    fprintf(stream, "\teyedist %g\n", profile->stereo_eyedist);
    for(m = profile->modules; m; m = m->next) {
      fprintf(stream, "\tmodule ");
      escape_write(stream,m->name);
      fprintf(stream, " {\n");
      for(d = m->data; d; d = d->next) {
	fprintf(stream,"\t\t");
	write_strval(stream,d->name,d->value,1);
      }
      fprintf(stream, "\t}\n");
    }
    fprintf(stream,"}\n");
  }
  return 0;
}

int veWindowMakeID(void) {
  static int idcnt = 0;
  idcnt++;
  return idcnt;
}
