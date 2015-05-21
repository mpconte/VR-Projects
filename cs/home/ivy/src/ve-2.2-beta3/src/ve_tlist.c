/* Parse TCL list semantics - understands {} and "" */
/* This code is destructive - the input gets mangled */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <ve_alloc.h>

static char *skip_to_end(char *c, char end, int *err) {
  while (*c != '\0' && *c != end) {
    if (*c == '{')
      c = skip_to_end(c+1,'}',err);
    else if (*c == '"')
      c = skip_to_end(c+1,'"',err);
    else if (*c == '\\')
      c++;
    if (*c == '\0')
      break;
    c++;
  }
  if (*c != end && err)
    *err = 1; /* didn't get the expected end character... */
  return c;
}

/* next_lelem() - return ptr to next list element, NULL if nothing left
   "ptr" should contain the start of the string and will be modified to
   point to the character after the last character in the element or
   NULL if there is nothing left) */
char *veNextLElem(char **ptr) {
  char *s = *ptr, *e;

  if (!s)
    return NULL;

  while (isspace(*s))  /* skip leading whitespace */
    s++;
  if (*s == '\0') {
    *ptr = NULL;
    return NULL;
  }
  e = s;
  while (*e != '\0' && !isspace(*e)) {
    if (*e == '{')
      e = skip_to_end(e+1,'}',NULL);
    else if (*e == '"')
      e = skip_to_end(e+1,'"',NULL);
    else if (*e == '\\')
      e++;
    if (*e == '\0')
      break;
    e++;
  }
  if (*e == '\0')
    *ptr = NULL;
  else {
    *ptr = e+1;
    *e = '\0';
  }
  if (*s == '{') {
    s++;
    e--;
    if (*e == '}')
      *e = '\0';
  } else if (*s == '"') {
    s++;
    e--;
    if (*e == '"')
      *e = '\0';
  }
  return s;
}

/* returns 1 if there is a complete list in str (no unclosed brackets) */
/* 0 otherwise */
/* trailing whitespace is ignored */
/* non-destructive */

int veListComplete(char *str) {
  int err = 0;
  (void) skip_to_end(str,'\0',&err);
  return !err; /* complete if we didn't have a syntax error... */
}

/* read in the next list from a line */
/* This doesn't split anything - comments ('#' as first character) are
   skipped */
/* Lines ending with '\' are continued */
static char *rdln(FILE *f, int *lineno) {
  static char *buf = NULL;
  static int bsz = 0;
  char ln[1024];
  int len = 0, n;

  if (!buf) {
    bsz = 1024; /* default sz */
    buf = veAlloc(bsz,0);
  }

  buf[0] = '\0';
  if (!fgets(ln,1024,f))
    return NULL;
  do {
    n = strlen(ln);
    if (n >= 2) {
      if (ln[n-2] == '\\' && ln[n-1] == '\n') {
	if (lineno)
	  (*lineno)++;
	if (n >= 3)
	  ln[n-3] = '\0';
	else
	  ln[0] = '\0';
      }
    }
    len += n;
    while (len > bsz-1) {
      bsz *= 2;
      buf = veRealloc(buf,bsz);
    }
    strcat(buf,ln);
  } while (!strchr(buf,'\n') && fgets(ln,1024,f));
  if (lineno)
    (*lineno)++;

  return buf;
}

static int blank_or_comment(char *s) {
  while (isspace(*s))
    s++;
  return (*s == '\0') || (*s == '#');
}

/* similar to next lelem, but splits up script stuff like:
   {
      foobar f x a 
      fijef  ij ij jjj
      iejfie {
         ifsje ifj se
      } eijfie
   }
   - i.e. elements typically are split by newline
   - we assume that "\\\n" sequences have already been removed
*/
char *veNextScrElem(char **ptr, int *lineno) {
  char *s = *ptr, *e;
  
  if (!s)
    return NULL;

  while (isspace(*s)) {
    while (isspace(*s)) { /* skip leading whitespace */
      if (lineno && *s == '\n')
	(*lineno)++;
      s++;
    }
    if (*s == '#') { /* comment - skip to eol/nul */
      while(*s != '\0' && *s != '\n')
	s++;
    } else if (*s == '\0') { /* nul - nothing left in string */
      *ptr = NULL;
      return NULL;
    }
  }

  if (*s == '\0') {
    *ptr = NULL;
    return NULL;
  }
  /* if we get this far, then there is something which is not a comment
     and which is not the end of the string */
  e = s;

  /* terminate an entry on newline or end-of-string */
  while (*e != '\0' && *e != '\n') {
    if (*e == '{')
      e = skip_to_end(e+1,'}',NULL);
    else if (*e == '"')
      e = skip_to_end(e+1,'"',NULL);
    else if (*e == '\\')
      e++;
    if (*e == '\0')
      break;
    e++;
  }
  if (*e == '\0')
    *ptr = NULL;
  else {
    *ptr = e+1;
    *e = '\0';
  }
  if (lineno) { /* increase line count */
    char *ss;
    ss = s;
    while(*ss) {
      if (*ss == '\n')
	(*lineno)++;
      ss++;
    }
  }
  return s;
} 

char *veNextFScrElem(FILE *f, int *lineno) {
  /* Note:  static buffer - only one call at a time... */
  /* i.e. this is nowhere near re-entrant */
  static char *buf = NULL;
  static int bsz = 0;
  char *ln;
  int len;

  if (!buf) {
    bsz = 1024; /* default sz */
    buf = veAlloc(bsz,0);
  }

  buf[0] = '\0';
  len = 0;

  /* read in lines, until we believe we have a complete one */
  do {
    /* read a whole (non-blank) line */
    do {
      if (!(ln = rdln(f,lineno)))
	return NULL;
    } while (blank_or_comment(ln));
    /* make space for it in the buffer */
    len += strlen(ln);
    while (len > bsz-1) {
      bsz *= 2;
      buf = veRealloc(buf,bsz);
    }
    strcat(buf,ln);
  } while (!veListComplete(buf));
  
  return buf;
}
