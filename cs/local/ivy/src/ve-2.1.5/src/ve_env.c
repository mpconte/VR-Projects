#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ve_env.h>
#include <ve_error.h>
#include <ve_util.h>
#include <ve_tlist.h>

#define MODULE "ve_env"
#define ARB 1024

/* This is a new parser for the envrionment and user profile.
   It uses the nested-list format rather than XML for simplicity. */

/* Types of entries at the high level:
   name <name-of-environment>
   desc <description of environment>
   option <name> <value>
   wall <name> <wall-contents>
   
   A wall can contain:
   <view-statements>
   window <name> <window-contents>
   option <name> <value>

   view statements are:
   loc x y z
   dir x y z
   up x y z
   width x
   height y

   A window can contain:
   id d
   display disp
   geometry geom
   offset x y
   err x y
   option name value
*/

#define newstruct(x) mallocstruct(sizeof (x))
static void *mallocstruct(int n) {
  void *x = calloc(1,n);
  assert(x != NULL);
  return x;
}
#define free_if(x) { if (x) free(x); }

static int parse_float(char *s, float *f) {
  return (sscanf(s,"%f",f) != 1);
}

static int parse_vector3(char *l, VeVector3 *v) {
  int i;
  char *s;
  for(i = 0; i < 3; i++) {
    if (!(s = veNextLElem(&l))) {
      return -1;
    }
    if (parse_float(s,&(v->data[i]))) {
      return -1;
    }
  }
  return 0;
}

static int parse_int(char *s, int *i) {
  return (sscanf(s,"%i",i) != 1);
}

static VeOption *parse_option(char *l) {
  char *name, *val;
  VeOption *o;

  if (!(name = veNextLElem(&l))) {
    veError(MODULE, "missing name for option");
    return NULL;
  }
  if (!(val = veNextLElem(&l))) {
    veError(MODULE, "missing value for option");
    return NULL;
  }
  o = newstruct(VeOption);
  o->name = veDupString(name);
  o->value = veDupString(val);
  return o;
}

static VeWindow *parse_window(char *l) {
  VeWindow *w;
  char *name, *body, *s;

  if (!(name = veNextLElem(&l))) {
    veError(MODULE, "missing name for window");
    return NULL;
  }
  if (!(body = veNextLElem(&l))) {
    veError(MODULE, "missing body for window");
    return NULL;
  }
  if (veNextLElem(&l))
    veWarning(MODULE, "ignoring extra arguments in window declaration");

  w = newstruct(VeWindow);
  w->name = veDupString(name);
  w->id = veWindowMakeID();
  veMatrixIdentity(&(w->distort));
  w->viewport[0] = w->viewport[1] = w->viewport[2] = w->viewport[3] = 0;

  while ((l = veNextScrElem(&body,NULL))) {
    s = veNextLElem(&l);
    assert(s != NULL);

    if (strcmp(s,"display") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_window: missing argument to display");
	veWindowFreeList(w);
	return NULL;
      }
      free_if(w->display);
      w->display = veDupString(s);
    } else if (strcmp(s,"geometry") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_window: missing argument to geometry");
	veWindowFreeList(w);
	return NULL;
      }
      free_if(w->geometry);
      w->geometry = veDupString(s);
    } else if (strcmp(s,"err") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_window: missing argument to err");
	veWindowFreeList(w);
	return NULL;
      }
      if ((parse_float(s,&(w->width_err)))) {
	veError(MODULE,"parse_window: bad width argument to err");
	veWindowFreeList(w);
	return NULL;
      }
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_window: missing argument to err");
	veWindowFreeList(w);
	return NULL;
      }
      if ((parse_float(s,&(w->height_err)))) {
	veError(MODULE,"parse_window: bad height argument to err");
	veWindowFreeList(w);
	return NULL;
      }
    } else if (strcmp(s,"offset") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_window: missing argument to offset");
	veWindowFreeList(w);
	return NULL;
      }
      if ((parse_float(s,&(w->offset_x)))) {
	veError(MODULE,"parse_window: bad x argument to offset");
	veWindowFreeList(w);
	return NULL;
      }
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_window: missing argument to offset");
	veWindowFreeList(w);
	return NULL;
      }
      if ((parse_float(s,&(w->offset_y)))) {
	veError(MODULE,"parse_window: bad y argument to offset");
	veWindowFreeList(w);
	return NULL;
      }
    } else if (strcmp(s,"option") == 0) {
      VeOption *o;
      if (!(o = parse_option(l))) {
	veError(MODULE, "parse_window: bad option declaration");
	veWindowFreeList(w);
	return NULL;
      }
      o->next = w->options;
      w->options = o;
    } else if (strcmp(s,"eye") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE, "parse_window: missing argument to eye");
	veWindowFreeList(w);
	return NULL;
      }
      if (strcmp(s,"mono") == 0)
	w->eye = VE_WIN_MONO;
      else if (strcmp(s,"left") == 0)
	w->eye = VE_WIN_LEFT;
      else if (strcmp(s,"right") == 0)
	w->eye = VE_WIN_RIGHT;
      else if (strcmp(s,"stereo") == 0)
	w->eye = VE_WIN_STEREO;
      else {
	veError(MODULE, "parse_window: invalid argument to eye: %s", s);
	veWindowFreeList(w);
	return NULL;
      }
    } else if (strcmp(s,"slave") == 0) {
      if ((s = veNextLElem(&l))) {
	w->slave.node = veDupString(s);
	if ((s = veNextLElem(&l))) {
	  w->slave.process = veDupString(s);
	  if ((s = veNextLElem(&l))) {
	    w->slave.thread = veDupString(s);
	  }
	}
      }
    } else if (strcmp(s,"viewport") == 0) {
      int k;
      for(k = 0; k < 4; k++) {
	if (!(s = veNextLElem(&l))) {
	  veError(MODULE, "parse_window: missing argument(s) to viewport (need 4)");
	  veWindowFreeList(w);
	  return NULL;
	}
	if (parse_int(s,&(w->viewport[k]))) {
	  veError(MODULE, "parse_window: invalid argument to viewport: %s",s);
	  veWindowFreeList(w);
	  return NULL;
	}
      }
    } else if (strcmp(s,"distort") == 0) {
      /* distort is a 4x4 matrix, specified row-by-row */
      /* for compatibility a 3x3 matrix is also accepted */
      char *f;
      int j,k,sz = 3;
      /* read the first row (if available) to determine size of matrix */
      if (!(s = veNextLElem(&l))) {
	veError(MODULE, "parse_window: missing arguments to distort");
	veWindowFreeList(w);
	return NULL;
      }

      for(j = 0; j < 3; j++) {
	if (!(f = veNextLElem(&s))) {
	  veError(MODULE, "parse_window: first row in distort argument must have 3 or 4 elements");
	  veWindowFreeList(w);
	  return NULL;
	}
	if (parse_float(f,&(w->distort.data[0][j]))) {
	  veError(MODULE, "parse_window: in distort, expected float got '%s'",f);
	  veWindowFreeList(w);
	  return NULL;
	}
      }
      if ((f = veNextLElem(&s))) {
	sz = 4;
	if (parse_float(f,&(w->distort.data[0][3]))) {
	  veError(MODULE, "parse_window: in distort, expected float got '%s'",f);
	  veWindowFreeList(w);
	  return NULL;
	}
      }
      /* now that we have determined size, parse remaining rows */
      for(k = 1; k < sz; k++) {
	if (!(s = veNextLElem(&l))) {
	  veError(MODULE, "parse_window: missing %d arguments to distort",
		  sz-k);
	  veWindowFreeList(w);
	  return NULL;
	}
	for(j = 0; j < sz; j++) {
	  if (!(f = veNextLElem(&s))) {
	    veError(MODULE, "parse_window: short matrix row for distort - expected %d, got %d",
		    sz, j);
	    veWindowFreeList(w);
	    return NULL;
	  }
	  if (parse_float(f,&(w->distort.data[k][j]))) {
	    veError(MODULE, "parse_window: in distort, expected float got '%s'",f);
	    veWindowFreeList(w);
	    return NULL;
	  }
	}
      }
    } else if (strcmp(s,"async") == 0) {
      w->async = 1;
    } else if (strcmp(s,"sync") == 0) {
      w->async = 0;
    } else {
      veError(MODULE, "parse_window: invalid declaration type: %s", s);
      veWindowFreeList(w);
      return NULL;
    }
  }
  return w;
}

static VeWall *parse_wall(char *l) {
  VeWall *wall;
  char *name, *body, *s;

  if (!(name = veNextLElem(&l))) {
    veError(MODULE, "missing name for wall");
    return NULL;
  }
  if (!(body = veNextLElem(&l))) {
    veError(MODULE, "missing body for wall");
    return NULL;
  }
  if (veNextLElem(&l))
    veWarning(MODULE, "ignoring extra arguments in wall declaration");
  
  wall = newstruct(VeWall);
  wall->name = veDupString(name);
  wall->view = newstruct(VeView);

  /* parse the body */
  while ((l = veNextScrElem(&body,NULL))) {
    s = veNextLElem(&l);
    assert(s != NULL);
    
    if (strcmp(s,"window") == 0) {
      VeWindow *w;
      if (!(w = parse_window(l))) {
	veError(MODULE, "parse_wall: bad window declaration");
	veWallFreeList(wall);
	return NULL;
      }
      w->next = wall->windows;
      wall->windows = w;
      w->wall = wall;
    } else if (strcmp(s,"option") == 0) {
      VeOption *o;
      if (!(o = parse_option(l))) {
	veError(MODULE, "parse_wall: bad option declaration");
	veWallFreeList(wall);
	return NULL;
      }
      o->next = wall->options;
      wall->options = o;
    } else if (strcmp(s,"loc") == 0) {
      if (parse_vector3(l,&(wall->view->frame.loc))) {
	veError(MODULE,"parse_wall: bad vector argument to loc");
	veWallFreeList(wall);
	return NULL;
      }
    } else if (strcmp(s,"dir") == 0) {
      if (parse_vector3(l,&(wall->view->frame.dir))) {
	veError(MODULE,"parse_wall: bad vector argument to dir");
	veWallFreeList(wall);
	return NULL;
      }
    } else if (strcmp(s,"up") == 0) {
      if (parse_vector3(l,&(wall->view->frame.up))) {
	veError(MODULE,"parse_wall: bad vector argument to up");
	veWallFreeList(wall);
	return NULL;
      }
    } else if (strcmp(s,"base") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_wall: missing argument to base");
	veWallFreeList(wall);
	return NULL;
      }
      if (strcmp(s,"origin") == 0)
	wall->view->base = VE_WALL_ORIGIN;
      else if (strcmp(s,"eye") == 0)
	wall->view->base = VE_WALL_EYE;
      else {
	veError(MODULE,"parse_wall: invalid argument to base: %s",s);
	veWallFreeList(wall);
	return NULL;
      }
    } else if (strcmp(s,"size") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_wall: missing width argument to size");
	veWallFreeList(wall);
	return NULL;
      }
      if (parse_float(s,&(wall->view->width))) {
	veError(MODULE,"parse_wall: bad width argument to size (%s)", s);
	veWallFreeList(wall);
	return NULL;
      }
      if (!(s = veNextLElem(&l))) {
	veError(MODULE,"parse_wall: missing height argument to size");
	veWallFreeList(wall);
	return NULL;
      }
      if (parse_float(s,&(wall->view->height))) {
	veError(MODULE,"parse_wall: bad height argument to size (%s)", s);
	veWallFreeList(wall);
	return NULL;
      }
    } else {
      veError(MODULE, "parse_wall: invalid declaration type: %s", s);
      veWallFreeList(wall);
      return NULL;
    }
  }

  return wall;
}


VeEnv *veEnvRead(FILE *stream, int verbose) {
  char *l, *s;
  int lineno = 0;
  VeEnv *env;

  env = newstruct(VeEnv);
  
  while ((l = veNextFScrElem(stream,&lineno))) {
    s = veNextLElem(&l);
    assert(s != NULL);
    
    if (strcmp(s,"name") == 0) {
      s = veNextLElem(&l);
      if (!s) {
	veError(MODULE, "veEnvRead, line %d: missing argument to environment name", lineno);
	veEnvFree(env);
	return NULL;
      }
      free_if(env->name);
      env->name = veDupString(s);
    } else if (strcmp(s,"desc") == 0) {
      s = veNextLElem(&l);
      if (!s) {
	veError(MODULE, "veEnvRead, line %d: missing argument to environment desc", lineno);
	veEnvFree(env);
	return NULL;
      }
      free_if(env->desc);
      env->desc = veDupString(s);
    } else if (strcmp(s,"wall") == 0) {
      VeWall *w;
      if (!(w = parse_wall(l))) {
	veError(MODULE, "veEnvRead, line %d: bad wall declaration", lineno);
	veEnvFree(env);
	return NULL;
      }
      w->next = env->walls;
      w->env = env;
      env->walls = w;
      env->nwalls++;
    } else if (strcmp(s,"option") == 0) {
      VeOption *o;
      if (!(o = parse_option(l))) {
	veError(MODULE, "veEnvRead, line %d: bad option declaration", lineno);
	veEnvFree(env);
	return NULL;
      }
      o->next = env->options;
      env->options = o;
    } else {
      veError(MODULE, "veEnvRead, line %d: invalid declaration type: %s",
	      lineno, s);
      veEnvFree(env);
      return NULL;
    }
  }
  return env;
}


void veViewFree(VeView *v) {
  free_if(v);
}

void veWindowFreeList(VeWindow *w) {
  VeWindow *tmp;

  while(w != NULL) {
    tmp = w->next;
    free_if(w->name);
    free_if(w->display);
    free_if(w->geometry);
    w = tmp;
  }
}

void veOptionFreeList(VeOption *o) {
  VeOption *tmp;
  while(o != NULL) {
    tmp = o->next;
    free_if(o->name);
    free_if(o->value);
    o = tmp;
  }
}

void veWallFreeList(VeWall *w) {
  VeWall *tmp;
  while(w != NULL) {
    tmp = w->next;
    free_if(w->name);
    free_if(w->desc);
    veViewFree(w->view);
    veWindowFreeList(w->windows);
    veOptionFreeList(w->options);
    w = tmp;
  }
}

void veEnvFree(VeEnv *env) {
  if (env != NULL) {
    free_if(env->name);
    free_if(env->desc);
    veWallFreeList(env->walls);
    veOptionFreeList(env->options);
    free(env);
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
    free(d->value);
    d->value = veDupString(value);
  } else {
    d = newstruct(VeProfileDatum);
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
    free_if(p->name);
    free_if(p->fullname);
    m = p->modules;
    while (m) {
      tmp = m->next;
      free_if(m->name);
      d = m->data;
      while (d) {
	dtmp = d->next;
	free_if(d->name);
	free_if(d->value);
	free(d);
	d = dtmp;
      }
      free(m);
      m = tmp;
    }
    free(p);
  }
}

VeProfile *veProfileRead(FILE *stream, int verbose) {
  char *l, *s, *body;
  int lineno = 0;
  VeProfile *pro;
  
  pro = newstruct(VeProfile);
  
  while ((l = veNextFScrElem(stream,&lineno))) {
    s = veNextLElem(&l);
    assert(s != NULL);
    
    if (strcmp(s,"name") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE, "veProfileRead, line %d: missing argument to name",
		lineno);
	veProfileFree(pro);
	return NULL;
      }
      free_if(pro->name);
      pro->name = veDupString(s);
    } else if (strcmp(s,"fullname") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE, "veProfileRead, line %d: missing argument to fullname",
		lineno);
	veProfileFree(pro);
	return NULL;
      }
      free_if(pro->fullname);
      pro->fullname = veDupString(s);
    } else if (strcmp(s,"eyedist") == 0) {
      if (!(s = veNextLElem(&l))) {
	veError(MODULE, "veProfileRead, line %d: missing argument to eyedist",
		lineno);
	veProfileFree(pro);
	return NULL;
      }
      if (parse_float(s,&(pro->stereo_eyedist))) {
	veError(MODULE, "veProfileRead, line %d: invalid argument to eyedist",
		lineno);
	veProfileFree(pro);
	return NULL;
      }
    } else if (strcmp(s,"module") == 0) {
      VeProfileModule *m;
      if (!(s = veNextLElem(&l))) {
	veError(MODULE, "veProfileRead, line %d: missing module name", lineno);
	veProfileFree(pro);
	return NULL;
      }
      m = newstruct(VeProfileModule);
      m->name = veDupString(s);
      m->next = pro->modules;
      pro->modules = m;
      if ((body = veNextLElem(&l))) {
	char *name, *val;
	while ((l = veNextScrElem(&body,NULL))) {
	  name = veNextLElem(&l);
	  val = veNextLElem(&l);
	  if (!name || !val) {
	    veError(MODULE, "veProfileRead, line %d: invalid module option", lineno);
	    veProfileFree(pro);
	    return NULL;
	  }
	  veProfileSetDatum(m,name,val);
	}
      }
    } else {
      veError(MODULE, "veProfileRead, line %d: invalid declaration type: %s",
	      lineno, s);
      veProfileFree(pro);
      return NULL;
    }
  }

  return pro;
}

int veEnvWrite(VeEnv *env, FILE *stream) {
  VeWall *wall;
  VeWindow *win;
  VeOption *o;
  if (env) {
    if (env->name)
      fprintf(stream, "name %s\n", env->name);
    if (env->desc)
      fprintf(stream, "desc \"%s\"\n", env->desc);
    for(wall = env->walls; wall; wall = wall->next) {
      fprintf(stream, "wall %s {\n", wall->name);
      if (wall->desc)
	fprintf(stream, "\tdesc \"%s\"\n", wall->desc);
      /* dump view */
      switch(wall->view->base) {
      case VE_WALL_ORIGIN:  fprintf(stream,"\tbase origin\n"); break;
      case VE_WALL_EYE:     fprintf(stream,"\tbase eye\n"); break;
      default:
	veError(MODULE,"veEnvWrite: base has invalid value: %d\n", 
		wall->view->base);
      }
      fprintf(stream, "\tloc %g %g %g\n", 
	      wall->view->frame.loc.data[0],
	      wall->view->frame.loc.data[1],
	      wall->view->frame.loc.data[2]);
      fprintf(stream, "\tdir %g %g %g\n", 
	      wall->view->frame.dir.data[0],
	      wall->view->frame.dir.data[1],
	      wall->view->frame.dir.data[2]);
      fprintf(stream, "\tup %g %g %g\n", 
	      wall->view->frame.up.data[0],
	      wall->view->frame.up.data[1],
	      wall->view->frame.up.data[2]);
      fprintf(stream, "\tsize %g %g\n",
	      wall->view->width, wall->view->height);
      /* dump windows */
      for(win = wall->windows; win; win = win->next) {
	fprintf(stream, "\twindow %s {\n", win->name);
	if (win->display)
	  fprintf(stream, "\t\tdisplay \"%s\"\n", win->display);
	if (win->geometry)
	  fprintf(stream, "\t\tgeometry \"%s\"\n", win->geometry);
	if (win->width_err != 0.0 || win->height_err != 0.0)
	  fprintf(stream, "\t\terr %g %g\n", win->width_err, win->height_err);
	if (win->offset_x != 0.0 || win->offset_y != 0.0)
	  fprintf(stream, "\t\toffset %g %g\n", win->offset_x, win->offset_y);
	for(o = win->options; o; o = o->next)
	  fprintf(stream, "\t\toption %s \"%s\"\n", o->name, 
		  o->value ? o->value : "");
	switch(win->eye) {
	case VE_WIN_MONO: fprintf(stream, "\t\teye mono\n"); break;
	case VE_WIN_LEFT: fprintf(stream, "\t\teye left\n"); break;
	case VE_WIN_RIGHT: fprintf(stream, "\t\teye right\n"); break;
	case VE_WIN_STEREO: fprintf(stream, "\t\teye stereo\n"); break;
	default:
	  veError(MODULE,"veEnvWrite: invalid value for window eye: %d",
		  win->eye);
	}
	fprintf(stream,"\t\tslave %s %s %s\n",
		win->slave.node ? win->slave.node : "auto",
		win->slave.process ? win->slave.process : "auto",
		win->slave.thread ? win->slave.thread : "auto");
	fprintf(stream,"\t\tviewport %d %d %d %d\n",
		win->viewport[0], win->viewport[1],
		win->viewport[2], win->viewport[3]);
	fprintf(stream,"\t\tdistort {%g %g %g %g} {%g %g %g %g} {%g %g %g %g} {%g %g %g %g}\n",
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
	  fprintf(stream,"\t\tasync\n");
	fprintf(stream, "\t}\n");
      }
      for(o = wall->options; o; o = o->next)
	fprintf(stream, "\toption %s \"%s\"\n", o->name, 
		o->value ? o->value : "");
      fprintf(stream, "}\n");
    }
    for(o = env->options; o; o = o->next)
      fprintf(stream, "option %s \"%s\"\n", o->name, 
	      o->value ? o->value : "");
  }
  return 0;
}

int veProfileWrite(VeProfile *profile, FILE *stream) {
  VeProfileModule *m;
  VeProfileDatum *d;
  if (profile) {
    if (profile->name)
      fprintf(stream, "name %s\n", profile->name);
    if (profile->fullname)
      fprintf(stream, "fullname %s\n", profile->fullname);
    fprintf(stream, "eyedist %g\n", profile->stereo_eyedist);
    for(m = profile->modules; m; m = m->next) {
      fprintf(stream, "module %s {\n", m->name);
      for(d = m->data; d; d = d->next)
	fprintf(stream, "\t%s %s\n", d->name, d->value);
      fprintf(stream, "}\n");
    }
  }
  return 0;
}

int veWindowMakeID(void) {
  static int idcnt = 0;
  idcnt++;
  return idcnt;
}
