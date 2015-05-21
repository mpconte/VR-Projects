/*
 * NOTE:  At what point does the '\' '<newline>' sequence get
 *        interpreted?  It should be considered as white-space
 *        while parsing a list (right)?
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "bluescript.h"

static void source_init(BSParseSource *p) {
  int k;
  p->lineno = 1;
  p->charno = 0;
  p->eol = 1;
  for(k = 0; k < BS_UNGET_DEPTH; k++)
    p->ungotten[k] = BS_EOF;
}

BSParseSource *bsStringSource(BSParseSource *p, char *s) {
  int k;
  p->type = BS_P_STRING;
  p->data.string = s;
  source_init(p);
  return p;
}

BSParseSource *bsFileSource(BSParseSource *p, FILE *f) {
  p->type = BS_P_FILE;
  p->data.file = f;
  source_init(p);
  return p;
}

void bsUngetc(BSParseSource *p, int c) {
  if (p) {
    int k;
    for(k = 0; k < BS_UNGET_DEPTH; k++) {
      if (p->ungotten[k] == BS_EOF) {
	p->ungotten[k] = c;
	break;
      }
    }
    /* make sure we could unget... */
    assert(k < BS_UNGET_DEPTH);
  }
}

#define my_isspace(c) ((c) == ' ' || (c) == '\n' || (c) == '\t' || (c) == '\r')

void bsSkipSpace(BSParseSource *p) {
  int c;
  while ((c = bsGetc(p)) != BS_EOF) {
    if (c == BS_C_ESCAPE) {
      if ((c = bsGetc(p)) != '\n') {
	bsUngetc(p,c);
	bsUngetc(p,BS_C_ESCAPE);
	return;
      }
    } else if (!my_isspace(c)) {
      bsUngetc(p,c);
      return;
    }
  }
  /* get here on EOF */
}

int bsGetc(BSParseSource *p) {
  int c;

  if (p->ungotten[0] != BS_EOF) {
    int k;
    for (k = BS_UNGET_DEPTH-1; k >= 0; k--) {
      if (p->ungotten[k] != BS_EOF) {
	c = p->ungotten[k];
	p->ungotten[k] = BS_EOF;
	return c;
      }
    }
    BS_FATAL("bsGetc: ungotten is not empty but empty?");
  }

  switch (p->type) {
  case BS_P_STRING:
    {
      if (!p->data.string)
	return BS_EOF;
      if ((c = *(p->data.string)) == '\0')
	return BS_EOF;
      p->data.string++;
      break;
    }

  case BS_P_FILE:
    {
      if (!p->data.file)
	return BS_EOF;
      if ((c = getc(p->data.file)) == EOF)
	return BS_EOF;
      break;
    }
			
  default:
    return BS_EOF;
  }
	
  if (c == '\n') {
    p->lineno++;
    p->charno++;
    p->eol = 1;
  } else {
    if (p->eol)
      p->charno = 0;
    else
      p->charno++;
  }
  return c;
}

int bsEof(BSParseSource *p) {
  if (!p)
    return 1;
  switch (p->type) {
  case BS_P_STRING:
    {
      if (!p->data.string)
	return 1;
      if (*(p->data.string) == '\0')
	return 1;
      return 0;
    }
			
  case BS_P_FILE:
    {
      if (!p->data.file)
	return 1;
      return feof(p->data.file);
    }
  }
  return 1; /* by default */
}

/* need to think a bit about how syntax works, e.g.
   what does {a}{b} mean vs {a} {b}? In Tcl {a}{b} is not
   a valid string */

/* generic string parser - used to skip over common pieces and
   to store the "unused" portion somewhere */
/* Return: 1 == EOF, 0 == terminal reached (*but* not appended), -1 == error */
static int parser(BSInterp *i, BSString *buffer, BSParseSource *p, 
		  int flags, int terminal) {
  BSObject *o;
  int c, k;
  while ((c = bsGetc(p)) != terminal && c != BS_EOF) {
    /* always append this character */
    bsStringAppChr(buffer,c);
    /* special cases */
    if (c == '\\') {
      /* escape */
      if ((c = bsGetc(p)) == BS_EOF) {
	bsSetStringResult(i,"escape ('\\') followed by end-of-input",
			  BS_S_STATIC);
	return -1;
      }
      bsStringAppChr(buffer,c);
    } else if (c == BS_C_VARIABLE && (flags & BS_Q_VARIABLE)) {
      BSString *s;
      bsUngetc(p,c);
      if (!(o = bsParseVariable(i,p)))
	return -1;
      s = bsObjGetString(o);
      if (o->quote == BS_Q_QVARIABLE)
	bsStringAppChr(buffer,'{');
      bsStringAppend(buffer,bsStringPtr(s),bsStringLength(s));
      if (o->quote == BS_Q_QVARIABLE)
	bsStringAppChr(buffer,'}');
      bsObjDelete(o);
    } else if (c == BS_C_LISTOPEN && (flags & BS_Q_LIST)) {
      if ((k = parser(i,buffer,p,flags,BS_C_LISTCLOSE)) != 0) {
	if (k > 0)
	  bsSetStringResult(i,"unterminated list",BS_S_STATIC);
	return -1; /* EOF is not valid here... */
      }
      bsStringAppChr(buffer,BS_C_LISTCLOSE);
    } else if (c == BS_C_ELISTOPEN && (flags & BS_Q_ELIST)) {
      if ((k = parser(i,buffer,p,flags,BS_C_ELISTCLOSE)) != 0) {
	if (k > 0)
	  bsSetStringResult(i,"unterminated eval-list",BS_S_STATIC);
	return -1;
      }
      bsStringAppChr(buffer,BS_C_ELISTCLOSE);
    } else if (c == BS_C_STRINGOPEN && (flags & BS_Q_STRING)) {
      /* special case - only terminated by unescaped BS_C_STRINGCLOSE */
      /* nothing else is considered special */
      while ((c = bsGetc(p)) != BS_C_STRINGCLOSE && c != BS_EOF) {
	bsStringAppChr(buffer,c);
	if (c == '\\') {
	  if ((c = bsGetc(p)) == BS_EOF)
	    break;
	  bsStringAppChr(buffer,c);
	}
      }
      if (c == BS_EOF) {
	bsSetStringResult(i,"unterminated string quote",BS_S_STATIC);
	return -1; /* unterminated quote */
      }
      bsStringAppChr(buffer,c);
    }
  }
  return (c == terminal ? 0 : 1);
}

/* 
   Parses:  '$<name>' and '${<name>}'
   Returns: A variable object
*/
BSObject *bsParseVariable(BSInterp *i, BSParseSource *p) {
  BSObject *o;
  int c, k;
  bsClearResult(i);
  if ((c = bsGetc(p)) != BS_C_VARIABLE) {
    bsAppendResult(i,"invalid variable reference");
    return NULL;
  }
  c = bsGetc(p);
  o = bsObjString(NULL,0,BS_S_VOLATILE);
  if (c == BS_C_LISTOPEN) {
    if ((k = parser(i,&(o->stringRep),p,BS_Q__NONE,BS_C_LISTCLOSE))) {
      if (k > 0)
	bsSetStringResult(i,"unterminated string for variable name",
			  BS_S_STATIC);
      bsObjDelete(o);
      return NULL; /* signal error...? */
    }
    o->quote = BS_Q_QVARIABLE;
  } else if (isalpha(c) || c == '_') {
    bsStringAppChr(&(o->stringRep),c);
    c = bsGetc(p);
    while (isalnum(c) || c == '_') {
      bsStringAppChr(&(o->stringRep),c);
      c = bsGetc(p);
    }
    bsUngetc(p,c); /* put back unwanted character */
    o->quote = BS_Q_VARIABLE;
  } else {
    bsSetStringResult(i,"invalid variable name",BS_S_STATIC);
    bsObjDelete(o);
    return NULL;
  }
  return o;
}

BSObject *bsParseQList(BSInterp *i, BSParseSource *p) {
  BSObject *o;
  int k, c;
  bsClearResult(i);
  if ((c = bsGetc(p)) != BS_C_LISTOPEN) {
    bsSetStringResult(i,"invalid list",BS_S_STATIC);
    return NULL;
  }
  o = bsObjString(NULL,0,BS_S_VOLATILE);
  if ((k = parser(i,&(o->stringRep),p,BS_Q__ALL,BS_C_LISTCLOSE))) {
    if (k > 0)
      bsSetStringResult(i,"unterminated list",BS_S_STATIC);
    bsObjDelete(o);
    return NULL;
  }
  o->quote = BS_Q_LIST;
  return o;
}

BSObject *bsParseEList(BSInterp *i, BSParseSource *p) {
  BSObject *o;
  int c, k;
  bsClearResult(i);
  if ((c = bsGetc(p)) != BS_C_ELISTOPEN) {
    bsSetStringResult(i,"invalid eval-list",BS_S_STATIC);
    return NULL;
  }
  o = bsObjString(NULL,0,BS_S_VOLATILE);
  if ((k = parser(i,&(o->stringRep),p,BS_Q__ALL,BS_C_ELISTCLOSE))) {
    if (k > 0)
      bsSetStringResult(i,"unterminated eval-list",BS_S_STATIC);
    bsObjDelete(o);
    return NULL;
  }
  o->quote = BS_Q_ELIST;
  return o;
}

/* Remember - 'escape' 'newline' is whitespace */
BSObject *bsParseName(BSInterp *i, BSParseSource *p) {
  BSObject *o;
  int c, k;
  bsClearResult(i);
  if ((c = bsGetc(p)) == BS_EOF || my_isspace(c)) {
    bsSetStringResult(i,"empty name",BS_S_STATIC);
    return NULL;
  }
  if (c == BS_C_ESCAPE) {
    /* peek ahead */
    if ((c = bsGetc(p)) == '\n') {
      bsSetStringResult(i,"empty name",BS_S_STATIC);
      return NULL;
    }
    bsUngetc(p,c);
    c = BS_C_ESCAPE;
  }
  o = bsObjString(NULL,0,BS_S_VOLATILE);
  while (!my_isspace(c) && c != BS_EOF) {
    if (c == BS_C_ESCAPE) {
      if ((c = bsGetc(p)) == BS_EOF) {
	bsSetStringResult(i,"escape followed by end-of-input",BS_S_STATIC);
	bsObjDelete(o);
	return NULL;
      }
      if (c == '\n') {
	/* this is white-space - need unget depth of *2* here! */
	bsUngetc(p,c);
	c = BS_C_ESCAPE;
	break;
      }
      bsStringAppChr(&(o->stringRep),BS_C_ESCAPE);
      bsStringAppChr(&(o->stringRep),c);
    } else
      bsStringAppChr(&(o->stringRep),c);
    c = bsGetc(p);
  }
  bsUngetc(p,c);
  return o;
}

BSObject *bsParseString(BSInterp *i, BSParseSource *p) {
  int c, k;
  BSObject *o;
  bsClearResult(i);
  if ((c = bsGetc(p)) != BS_C_STRINGOPEN) {
    bsSetStringResult(i,"invalid string",BS_S_STATIC);
    return NULL;
  }
  o = bsObjString(NULL,0,BS_S_VOLATILE);
  while ((c = bsGetc(p)) != BS_C_STRINGCLOSE && 
	 c != BS_EOF) {
    if (c != BS_C_ESCAPE) {
      bsStringAppChr(&(o->stringRep),c);
    } else {
      /* deal with special escapes */
      if ((c = bsGetc(p)) == BS_EOF) {
	bsSetStringResult(i,"escape followed by end-of-input",BS_S_STATIC);
	bsObjDelete(o);
	return NULL;
      }
      switch (c) {
      case 'n':  
	bsStringAppChr(&(o->stringRep),'\n');
	break;
      case 't':
	bsStringAppChr(&(o->stringRep),'\t');
	break;
      case 'v':
	bsStringAppChr(&(o->stringRep),'\v');
	break;
      case 'r':
	bsStringAppChr(&(o->stringRep),'\r');
	break;
      default:
	/* all other characters are interpreted later */
	bsStringAppChr(&(o->stringRep),BS_C_ESCAPE);
	bsStringAppChr(&(o->stringRep),c);
      }
    }
  }
  o->quote = BS_Q_STRING;
  return o;
}

static BSObject *next_list_elem(BSInterp *i, BSParseSource *p) {
  int c, k;

  /* skip any leading unquoted white-space */
  while ((c = bsGetc(p)) != BS_EOF) {
    /* look out for special 'escape' 'newline' case
       - that's white-space */
    if (c == BS_C_ESCAPE) {
      if ((c = bsGetc(p)) == '\n') {
	/* special case */
	continue;
      } else {
	bsUngetc(p,c);
	c = BS_C_ESCAPE;
	break;
      }
    } else if (!my_isspace(c)) {
      break;
    }
  }

  if (c == BS_EOF)
    return NULL; /* nothing left */

  /* put it back - our parsers will read it again... */
  bsUngetc(p,c);
  
  /* deal with quoted cases */
  if (c == BS_C_VARIABLE)
    return bsParseVariable(i,p);
  else if (c == BS_C_LISTOPEN)
    return bsParseQList(i,p);
  else if (c == BS_C_ELISTOPEN)
    return bsParseEList(i,p);
  else if (c == BS_C_STRINGOPEN)
    return bsParseString(i,p);
  else
    return bsParseName(i,p);
}  

static int comment_or_blank(char *s) {
  while (my_isspace(*s))
    s++;
  if (*s == '#' || *s == '\0')
    return 1;
  return 0;
}

int bsParseList(BSInterp *i, BSParseSource *p, BSList *l) {
  BSObject *o;
  bsClearResult(i);
  while (o = next_list_elem(i,p))
    bsListPush(l,o,BS_TAIL);
  /* check for error */
  return (bsIsResultClear(i) ? 0 : -1);
}

/* simplistic parsing - just tell us whether all brackets are closed or
   not (roughly) */
int bsScriptComplete(char *str) {
  int depth = 0;

  while (*str != '\0') {
    switch (*str) {
    case '{':
      depth++;
      break; 
    case '}':
      depth--;
      break;
    case '[':
      depth++;
      break;
    case ']':
      depth--;
      break;
    case '"':
      while (*str && *str != '"') {
	if (*str == '\\') {
	  if (!*(str+1))
	    return 0;
	  str++;
	}
	str++;
      }
      if (*str != '"')
	return 0; /* nope */
      break;
    case '\\':
      if (!*(str+1))
	return 0; /* nope */
    }
    str++;
  }
  return (depth <= 0);
}

/* return a list only containing non-comment script elements */
int bsParseScript(BSInterp *i, BSParseSource *p, BSList *l) {
  BSString s;
  BSParseSource sp;
  int k;

  bsClearResult(i);
  /* append to l */
  bsStringInit(&s);
  bsStringClear(&s);
  while ((k = parser(i,&s,p,BS_Q__ALL,'\n')) == 0) {
    if (!comment_or_blank(bsStringPtr(&s))) {
      BSObject *o;
      o = bsObjList(0,NULL);
      if (bsParseList(i,bsStringSource(&sp,bsStringPtr(&s)),&(o->listRep))) {
	bsStringFreeSpace(&s);
	return -1; /* error has been set by bsParseList() */
      }
      bsListPush(l,o,BS_TAIL);
    }
    bsStringClear(&s);
  }
  if (k < 0) {
    bsStringFreeSpace(&s);
    return -1; /* error has been set by parser() */
  }

  /* eof */
  if (!comment_or_blank(bsStringPtr(&s))) {
    /* there's something non-trivial left... */
    BSObject *o;
    o = bsObjList(0,NULL);
    if (bsParseList(i,bsStringSource(&sp,bsStringPtr(&s)),&(o->listRep))) {
      bsStringFreeSpace(&s);
      return -1; /* error has been set by bsParseList() */
    }
    bsListPush(l,o,BS_TAIL);
  }
  bsStringFreeSpace(&s);
  return 0;
}

/*
 * "substitution" - convert a string with possibly variable declarations
 * to a string with values substituted.
 */
int bsSubsIsStatic(char *s) {
  while (*s) {
    if (*s == BS_C_VARIABLE || *s == BS_C_VARIABLE)
      return 0; /* nope */
    if (*s == BS_C_ESCAPE) {
      s++;
      if (!*s)
	continue;
    }
    s++;
    continue;
  }
  return 1; /* yep - it's static alright */
}

static BSSubsElem *make_string(char *s) {
  BSSubsElem *e;
  e = bsAllocObj(BSSubsElem);
  e->type = BS_SUBS_STRING;
  bsStringInit(&(e->data.string));
  bsStringAppend(&(e->data.string),s,-1);
  return e;
}

static BSSubsElem *make_var(char *s) {
  BSSubsElem *e;
  e = bsAllocObj(BSSubsElem);
  e->type = BS_SUBS_VAR;
  bsStringInit(&(e->data.var));
  bsStringAppend(&(e->data.var),s,-1);
  return e;
}

int bsSubsParse(BSInterp *i, BSSubsList *sl, char *s, int flags) {
  BSParseSource p;
  int c, k;
  int code = BS_ERROR;
  
  bsStringSource(&p,s);
  bsSubsClear(sl);
  
  while ((c = bsGetc(&p)) != BS_EOF) {
    if ((c == BS_C_VARIABLE) && !(flags & BS_SUBS_NOVARS)) {
      BSSubsElem *e = make_var(NULL);
      if (sl->tail)
	sl->tail->next = e;
      else
	sl->head = e;
      sl->tail = e;

      if ((c = bsGetc(&p)) == BS_EOF) {
	bsSetStringResult(i,"missing variable name",BS_S_STATIC);
	code = BS_ERROR;
	goto cleanup;
      } else if (c == BS_C_LISTOPEN) {
	k = parser(i,&(e->data.var),&p,BS_Q__NONE,BS_C_LISTCLOSE);
	if (k) {
	  if (k > 0)
	    bsSetStringResult(i,"unterminated variable name",BS_S_STATIC);
	  code = BS_ERROR;
	  goto cleanup;
	}
      } else if (isalpha(c) || c == '_') {
	bsStringAppChr(&(e->data.var),c);
	c = bsGetc(&p);
	while (isalnum(c) || c == '_') {
	  bsStringAppChr(&(e->data.var),c);
	  c = bsGetc(&p);
	}
	bsUngetc(&p,c);
      }
    } else if ((c == BS_C_ELISTOPEN) && !(flags & BS_SUBS_NOPROCS)) {
      BSParseSource lp;
      BSString lstr;
      BSSubsElem *e = NULL;
      bsStringInit(&lstr);
      k = parser(i,&(lstr),&p,BS_Q__ALL,BS_C_ELISTCLOSE);
      if (k) {
	if (k > 0)
	  bsSetStringResult(i,"unterminated eval-list",BS_S_STATIC);
	code = BS_ERROR;
	goto cleanup;
      }
      e = bsAllocObj(BSSubsElem);
      e->type = BS_SUBS_PROC;
      if (sl->tail)
	sl->tail->next = e;
      else
	sl->head = e;
      sl->tail = e;
      bsListInit(&(e->data.proc));
      if (bsParseList(i,bsStringSource(&lp,bsStringPtr(&lstr)),
		      &(e->data.proc)) != BS_OK) {
	bsStringFreeSpace(&lstr);
	code = BS_ERROR;
	goto cleanup;
      }
      bsStringFreeSpace(&lstr);
    } else {
      BSSubsElem *e = make_string(NULL);
      if (sl->tail)
	sl->tail->next = e;
      else
	sl->head = e;
      sl->tail = e;

      while (c != BS_EOF && 
	     (c != BS_C_VARIABLE || (flags & BS_SUBS_NOVARS)) &&
	     (c != BS_C_ELISTOPEN || (flags & BS_SUBS_NOPROCS))) {
	if (c == BS_C_ESCAPE && !(flags & BS_SUBS_NOESCAPES)) {
	  /* skip to next char */
	  if ((c = bsGetc(&p)) == BS_EOF) {
	    bsSetStringResult(i,"escape followed by end-of-input",BS_S_STATIC);
	    code = BS_ERROR;
	    goto cleanup;
	  }
	}
	bsStringAppChr(&(e->data.string),c);
	c = bsGetc(&p);
      }
      bsUngetc(&p,c);
    }
  }
  code = BS_OK;
  sl->flags = flags;
  /* fall-through */

 cleanup:
  if (code != BS_OK)
    bsSubsClear(sl);
  return code;
}

int bsSubsList(BSInterp *i, BSString *dst, BSSubsList *l) {
  BSSubsElem *e;

  bsClearResult(i);
  for (e = l->head; e; e = e->next) {
    switch (e->type) {
    case BS_SUBS_STRING:
      bsStringAppend(dst,bsStringPtr(&(e->data.string)),
		     bsStringLength(&(e->data.string)));
      break;
    case BS_SUBS_VAR:
      {
	BSObject *o;
	BSString *s;
	if (!(o = bsGet(i,NULL,bsStringPtr(&(e->data.var)),0))) {
	  bsAppendResult(i,"unknown variable: ",
			 bsStringPtr(&(e->data.var)),NULL);
	  return BS_ERROR;
	}
	s = bsObjGetString(o);
	bsStringAppend(dst,bsStringPtr(s),bsStringLength(s));
      }
      break;
    case BS_SUBS_PROC:
      {
	BSString *s;
	int k;
	bsClearResult(i); /* *should* be clear... */
	if ((k = bsProcessLine(i,&(e->data.proc))) != BS_OK) {
	  if (k != BS_ERROR && bsIsResultClear(i))
	    bsAppendResult(i,"unsupported result from calllist",NULL);
	  return BS_ERROR;
	}
	s = bsObjGetString(bsGetResult(i));
	bsStringAppend(dst,bsStringPtr(s),bsStringLength(s));
	bsClearResult(i); /* don't save this at all... */
      }
      break;
    default:
      BS_FATAL("invalid elem type in bsSubsList");
    }
  }
  return BS_OK;
}

void bsSubsElemFree(BSSubsElem *e) {
  if (e) {
    switch (e->type) {
    case BS_SUBS_STRING:
      bsStringFreeSpace(&(e->data.string));
      break;
    case BS_SUBS_VAR:
      bsStringFreeSpace(&(e->data.var));
      break;
    case BS_SUBS_PROC:
      bsListClear(&(e->data.proc));
      break;
    default:
      BS_FATAL("invalid elem type in bsSubsElemFree");
    }
    bsFree(e);
  }
}

void bsSubsClear(BSSubsList *sl) {
  BSSubsElem *e, *en;
  if (sl) {
    e = sl->head;
    while (e) {
      en = e->next;
      bsSubsElemFree(e);
      e = en;
    }
    sl->head = sl->tail = NULL;
    sl->flags = 0;
  }
}

void bsSubsInit(BSSubsList *sl) {
  if (sl) {
    sl->head = sl->tail = NULL;
    sl->flags = 0;
  }
}

int bsSubs(BSInterp *i, BSString *dst, char *s, int flags) {
  BSSubsList l;
  int code;
  bsClearResult(i);
  bsSubsInit(&l);
  if ((code = bsSubsParse(i,&l,s,flags)) != BS_OK)
    return code;
  code = bsSubsList(i,dst,&l);
  bsSubsClear(&l);
  return code;
}

typedef struct bs_subs_priv {
  BSSubsList l;
  int flags;
} BSSubsPriv;

static void subs_freeproc(void *priv, void *cdata) {
  BSSubsPriv *p;
    
  if ((p = (BSSubsPriv *)priv)) {
    bsSubsClear(&(p->l));
    bsFree(p);
  }
}

#if 0
static void *subs_copyproc(void *priv, void *cdata) {
  BSSubsPriv *p, *p2;
  if (!(p = (BSSubsPriv *)priv))
    return NULL;
  p2 = bsAllocObj(BSSubsPriv);
  bsSubsInit(&(p2->l));
  bsSubsCopy(&(p2->l),&(p->l));
  p2->flags = p->flags;
  return p2;
}
#endif 

static BSCacheDriver subs_driver = {
  "subs",                /* name */
  0,                     /* id - assign later */
  subs_freeproc,         /* freeproc */
  NULL,                  /* copyproc */
  NULL                   /* cdata */
};

BSCacheDriver *bsGetSubsDriver(void) {
  if (subs_driver.id == 0)
    subs_driver.id = bsUniqueId();
  return &subs_driver;
}

/* using an object allows us to cache the broken-down substitution */
int bsSubsObj(BSInterp *i, BSString *dst, BSObject *o, int flags) {
  BSCacheDriver *d;
  BSSubsPriv *p;
  int code, free_subs = 0;

  bsClearResult(i);

  if (!o)
    return BS_OK; /* nothing to substitute */

  d = bsGetSubsDriver();
  assert(d != NULL);
  assert(d->id != (BSId)0);

  if (bsObjGetCache(o,d->id,(void **)&p)) {
    /* rebuild and add */
    p = bsAllocObj(BSSubsPriv);
    bsSubsInit(&(p->l));
    p->flags = flags;
    if ((code = bsSubsParse(i,&(p->l),bsObjGetStringPtr(o),
			    flags)) != BS_OK) {
      subs_freeproc(p,NULL);
      return code;
    }
    if (i->opt & BS_OPT_MEMORY)
      free_subs = 1;
    else
      bsObjAddCache(o,d,(void *)p);
  } else if (p->flags != flags) {
    /* rebuild */
    bsSubsClear(&(p->l));
    p->flags = flags;
    if ((code = bsSubsParse(i,&(p->l),bsObjGetStringPtr(o),
			    flags)) != BS_OK) {
      bsObjDelCache(o,d->id); /* invalidate cache */
      return code;
    }
    /* already in cache */
  }

  bsClearResult(i);
  code = bsSubsList(i,dst,&(p->l));
  
  if (free_subs)
    subs_freeproc(p,NULL);

  return code;
}

