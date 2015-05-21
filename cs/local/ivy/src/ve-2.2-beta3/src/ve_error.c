#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ve_error.h"
#include "ve_util.h"

#define MODULE "ve_error"

#define MAX_ERROR_MSG_SIZE 4096
#define MAX_ADD_SIZE 80

void veDefaultErrorHandler(int type, char *module, char *msg) {
  fprintf(stderr, "%s: %s\n", module, msg);
}

static VeErrorHandler ve_error_handler = veDefaultErrorHandler;
static int show_notices = 0;
static int show_warnings = 1;

void veShowNotices(int i) {
  show_notices = i ? 1 : 0;
}

void veShowWarnings(int i) {
  show_warnings = i ? 1 : 0;
}

void veNotice(char *module, char *fmt, ...) {
  va_list ap;
  char str1[MAX_ERROR_MSG_SIZE], str2[MAX_ERROR_MSG_SIZE+MAX_ADD_SIZE];
  
  if (!show_notices)
    return;
  
  va_start(ap,fmt);
  veVsnprintf(str1,MAX_ERROR_MSG_SIZE,fmt,ap);
  va_end(ap);
  sprintf(str2,"notice: %s", str1);
  ve_error_handler(VE_ERR_NOTICE,module,str2);
}

void veWarning(char *module, char *fmt, ...) {
  va_list ap;
  char str1[MAX_ERROR_MSG_SIZE], str2[MAX_ERROR_MSG_SIZE+MAX_ADD_SIZE];

  if (!show_warnings)
    return;

  va_start(ap,fmt);
  veVsnprintf(str1,MAX_ERROR_MSG_SIZE,fmt,ap);
  va_end(ap);
  sprintf(str2,"warning: %s", str1);
  ve_error_handler(VE_ERR_WARNING,module,str2);
}

void veError(char *module, char *fmt, ...) {
  va_list ap;
  char str1[MAX_ERROR_MSG_SIZE], str2[MAX_ERROR_MSG_SIZE+MAX_ADD_SIZE];
  va_start(ap,fmt);
  veVsnprintf(str1,MAX_ERROR_MSG_SIZE,fmt,ap);
  va_end(ap);
  sprintf(str2,"error: %s", str1);
  ve_error_handler(VE_ERR_ERROR,module,str2);
}

void veFatalError(char *module, char *fmt, ...) {
  va_list ap;
  char str1[MAX_ERROR_MSG_SIZE], str2[MAX_ERROR_MSG_SIZE+MAX_ADD_SIZE];
  va_start(ap,fmt);
  veVsnprintf(str1,MAX_ERROR_MSG_SIZE,fmt,ap);
  va_end(ap);
  sprintf(str2,"fatal error: %s", str1);
  ve_error_handler(VE_ERR_FATAL,module,str2);
  /* exit on a fatal error */
  if (getenv("VE_ABORT_ON_FATAL") && atoi(getenv("VE_ABORT_ON_FATAL")))
    abort();
  else
    exit(1);
}

void veSetErrorHandler(VeErrorHandler e) {
  if (e)
    ve_error_handler = e;
  else
    ve_error_handler = veDefaultErrorHandler;
}
