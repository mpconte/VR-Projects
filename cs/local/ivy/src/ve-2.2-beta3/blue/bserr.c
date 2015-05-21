#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int show_warnings = 1;

int bsShowWarnings(int x) {
  int old = show_warnings;
  show_warnings = x;
  return old;
}

static int die_on_warn(void) {
  static int init = 0;
  static int value = 0;
  
  if (!init) {
    char *c;
    value = ((c = getenv("BS_WARNABORT")) && atoi(c));
    init = 1;
  }
  return value;
}

void bsWarn(char *file, int line, char *msg) {
  if (show_warnings) {
    fprintf(stderr, "bs warning: file %s, line %d: %s\n",
	    (file ? file : "<null>"), line,
	    (msg ? msg : "<null>"));
  }
  if (die_on_warn())
    abort();
}

void bsFatal(char *file, int line, char *msg) {
  fprintf(stderr, "bs fatal error: file %s, line %d: %s\n",
	  (file ? file : "<null>"), line,
	  (msg ? msg : "<null>"));
  abort();
}

    
