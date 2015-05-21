/* Standard C streams */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#endif /* _WIN32 */

#include <bluescript.h>

/*@bsdoc
package streamstd {
    longname {Standard Streams}
    desc {
	Common functions for creating streams (files, commands, stdio).
    }
    doc {
	These functions are not included in a standard BlueScript
	interpreter by default.  Check the documentation for your
	specific interpreter implementation to see if they are included.
    }
}
 */

struct stdio_priv {
  FILE *f_read, *f_write;
  int is_popen;
};

static int stdio_close(BSInterp *i, BSStream *s) {
  struct stdio_priv *p;
  int res = 0;
  assert(s != NULL);
  p = (struct stdio_priv *)(s->priv);
  assert(p != NULL);
  if (p->f_read && p->f_read != stdin) {
    if (p->is_popen)
      res += pclose(p->f_read);
    else
      res += fclose(p->f_read);
  }
  if (p->f_write && p->f_write != p->f_read) {
    if (p->f_write == stdout || p->f_write == stderr)
      res += fflush(p->f_write); /* don't try to close */
    else if (p->is_popen)
      res += pclose(p->f_write);
    else
      res += fclose(p->f_write);
  }
  bsFree(p);
  return res;
}

static int stdio_poll(BSInterp *i, BSStream *s, int flags) {
  /* there's no standard interface for polling a stdio descriptor
     given that it may be buffered...so we always assume it is 
     ready */
  return 1;
}

static int stdio_read(BSInterp *i, BSStream *s, void *buf, int nbytes) {
  struct stdio_priv *p;
  int k;
  assert(s != NULL);
  p = s->priv;
  assert(p != NULL);
  if (!p->f_read) {
    bsSetStringResult(i,"no read channel",BS_S_STATIC);
    return -1;
  }
  k = fread(buf,1,nbytes,p->f_read);
  if (ferror(p->f_read)) {
    bsSetStringResult(i,strerror(errno),BS_S_STATIC);
    return -1;
  }
  return k;
}

static int stdio_write(BSInterp *i, BSStream *s, void *buf, int nbytes) {
  struct stdio_priv *p;
  int k;
  assert(s != NULL);
  p = s->priv;
  assert(p != NULL);
  if (!p->f_write) {
    bsSetStringResult(i,"no write channel",BS_S_STATIC);
    return -1;
  }
  k = fwrite(buf,1,nbytes,p->f_write);
  if (ferror(p->f_write)) {
    bsSetStringResult(i,strerror(errno),BS_S_STATIC);
    return -1;
  }
  return k;
}

static int stdio_flush(BSInterp *i, BSStream *s) {
  struct stdio_priv *p;
  assert(s != NULL);
  p = s->priv;
  assert(p != NULL);
  if (!p->f_write) {
    bsSetStringResult(i,"no write channel",BS_S_STATIC);
    return -1;
  }
  return fflush(p->f_write);
}

static BSStreamDriver stdio_stream_driver = {
  stdio_close,
  stdio_read,
  stdio_write,
  stdio_flush,
  stdio_poll
};

BSStream *bsStream_File(char *fname, char *mode) {
  BSStream *s;
  struct stdio_priv *p;
  FILE *f;

  if (!(f = fopen(fname,mode)))
    return NULL;
  if (!(s = bsStreamMake()))
    return NULL;
  p = bsAllocObj(struct stdio_priv);
  p->f_read = p->f_write = f;
  s->drv = &stdio_stream_driver;
  s->priv = p;
  return s;
}

BSStream *bsStream_Stdio(FILE *read_src, FILE *write_dst) {
  BSStream *s;
  struct stdio_priv *p;
  if (!(s = bsStreamMake()))
    return NULL;
  p = bsAllocObj(struct stdio_priv);
  p->f_read = read_src;
  p->f_write = write_dst;
  s->drv = &stdio_stream_driver;
  s->priv = p;
  return s;
}

/*@bsdoc
procedure fopen {
    usage {fopen <filename> [<mode>]}
    returns {A new stream object which points at the contents of the
	given file.}
    doc {
	Opens a file for reading or writing as a stream object.  The
	<i>mode</i> corresponds to the modes used in the Standard C
	<code>fopen()</code> function:
	<ul>
	<li><code>r</code> - opens a file for reading at the
	beginning.</li>
	<li><code>r+</code> - opens a file for reading and
	writing at the beginning.</li>
	<li><code>w</code> - truncates or creates a file for writing.</li>
	<li><code>w+</code> - truncates or creates a file for
	reading and writing.</li>
	<li><code>a</code> - opens or creates a file for writing
	at the end.</li>
	<li><code>a+</code> - opens or creates a file for reading
	and writing at the end.</li>
	</ul>
	If <i>mode</i> is omitted then the mode defaults to <code>r</code>.
    }
    example {Opening a file for reading} {fopen /etc/passwd r} {<i>stream</i>}
}
 */
static int f_fopen(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  BSStream *s;
  BSObject *o;
  char *fn, *m;
  FILE *f;
  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,"usage: fopen <filename> [<mode>]",BS_S_STATIC);
    return BS_ERROR;
  }
  fn = bsObjGetStringPtr(objv[1]);
  if (objc >= 3)
    m = bsObjGetStringPtr(objv[2]);
  else
    m = "r"; /* default */
  if (!(s = bsStream_File(fn,m)))
    return BS_ERROR;
  if (!(o = bsStreamMakeOpaque(s))) {
    bsStreamClose(i,s);
    return BS_ERROR;
  }
  bsSetResult(i,o);
  bsObjDelete(o);
  return BS_OK;
}

#ifndef OMIT_POPEN
/*@bsdoc
procedure popen {
    usage {popen <cmd> [<mode>]}
    doc {
	Executes a command and allows the script to read from its
	standard output and/or write to its standard intput.  The
	<i>mode</i> parameter determines what kind of input/output is
	supported:
	<ul>
	<li><code>r</code> - allow reading from command's standard
	output</li>
	<li><code>w</code> - allow writing to command's standard
	input</li>
	</ul>
	If <i>mode</i> is omitted then it defaults to <code>r</code>.
	<p>
	This command depends upon the presence and functionality of
	the underlying <code>popen()</code> C library function.  On
	some platforms (e.g. Windows) the availability and
	functionality of this procedure may be constrained.
    }
    example {Reading a command's output} {popen {grep ^root
	/etc/passwd} r} {<i>stream</i>}
}
*/
static int f_popen(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  BSStream *s;
  BSObject *o;
  char *fn, *m;
  struct stdio_priv *p;
  FILE *x;
  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,"usage: popen <cmd> [<mode>]",BS_S_STATIC);
    return BS_ERROR;
  }
  fn = bsObjGetStringPtr(objv[1]);
  if (objc >= 3)
    m = bsObjGetStringPtr(objv[2]);
  else
    m = "r"; /* default */
  if (!(x = popen(fn,m))) {
    bsSetStringResult(i,strerror(errno),BS_S_STATIC);
    return BS_ERROR;
  }
  if (strcmp(m,"r") == 0)
    s = bsStream_Stdio(x,NULL);
  else
    s = bsStream_Stdio(NULL,x);
  if (!s) {
    pclose(x);
    return BS_ERROR;
  }
  p = (struct stdio_priv *)(s->priv);
  p->is_popen = 1;
  if (!(o = bsStreamMakeOpaque(s))) {
    bsStreamClose(i,s);
    return BS_ERROR;
  }
  bsSetResult(i,o);
  bsObjDelete(o);
  return BS_OK;
}
#endif /* OMIT_POPEN */

/* return a standard stream */
/*@bsdoc
procedure stdin {
    usage {stdin}
    returns {A read-only stream object corresponding to standard input.}
}
procedure stdout {
    usage {stdout}
    returns {A write-only stream object corresponding to standard output.}
}
procedure stderr {
    usage {stderr}
    returns {A write-only stream object corresponding to standard error.}
}
procedure stdio {
    usage {stdio}
    returns {A read-write stream object where input is taken from
	standard input and output is written to standard output.}
}
*/
static int f_stdio(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static BSObject *o_stdin = NULL, *o_stdout = NULL, *o_stderr = NULL,
    *o_stdio = NULL;

  char *fn;
  fn = bsObjGetStringPtr(objv[0]);
  if (objc != 1) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",bsObjGetStringPtr(objv[0]),NULL);
    return BS_ERROR;
  }

  if (strcmp(fn,"stdio") == 0) {
    if (!(o_stdio)) {
      o_stdio = bsStreamMakeOpaque(bsStream_Stdio(stdin,stdout));
      assert(o_stdio != NULL);
    }
    bsSetResult(i,o_stdio);
  } else if (strcmp(fn,"stdin") == 0) {
    if (!(o_stdin)) {
      o_stdin = bsStreamMakeOpaque(bsStream_Stdio(stdin,NULL));
      assert(o_stdin != NULL);
    }
    bsSetResult(i,o_stdin);
  } else if (strcmp(fn,"stdout") == 0) {
    if (!(o_stdout)) {
      o_stdout = bsStreamMakeOpaque(bsStream_Stdio(NULL,stdout));
      assert(o_stdout != NULL);
    }
    bsSetResult(i,o_stdout);
  } else if (strcmp(fn,"stderr") == 0) {
    if (!(o_stderr)) {
      o_stderr = bsStreamMakeOpaque(bsStream_Stdio(NULL,stderr));
      assert(o_stderr != NULL);
    }
    bsSetResult(i,o_stderr);
  } else {
    BS_FATAL("f_stdio aliased to unknown stream");
  }
  return BS_OK;
}

void bsStream_FileProcs(BSInterp *i) {
  bsSetExtProc(i,"fopen",f_fopen,NULL);
}

void bsStream_StdioProcs(BSInterp *i) {
  bsSetExtProc(i,"stdio",f_stdio,NULL);
  bsSetExtProc(i,"stdin",f_stdio,NULL);
  bsSetExtProc(i,"stdout",f_stdio,NULL);
  bsSetExtProc(i,"stderr",f_stdio,NULL);
}

void bsStream_ExecProcs(BSInterp *i) {
#ifndef OMIT_POPEN
  bsSetExtProc(i,"popen",f_popen,NULL);
#endif /* OMIT_POPEN */
}
