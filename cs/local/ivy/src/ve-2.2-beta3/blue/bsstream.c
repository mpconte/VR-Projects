/* Data streams for BlueScript */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bluescript.h>

/*@bsdoc 
package stream {
    longname "Stream Objects"
    desc {
	Objects representing sequences of bytes (e.g. files) supporting
	a simple I/O model.
    }
    doc {
      <p>
	Streams (sequences of bytes that can be read and-or written)
	are not returned by any function in Standard BlueScript.
	[Note:  There are optional functions that do so - see below.]
	However, other modules may make use of stream objects, so the
	common methods for stream objects are denoted below.
      </p>
    }
}
 */

#define CHK(i,x) { if (!(x)) { \
bsSetStringResult((i),"null argument(s)",BS_S_STATIC); \
return BS_ERROR; \
} }

int bsStreamClose(BSInterp *i, BSStream *s) {
  int code;
  CHK(i,s);
  if (s->drv && s->drv->close) {
    /* allow driver to close */
    code = s->drv->close(i,s);
  } else {
    code = BS_OK;
  }
  bsFree(s);
  return code;
}

int bsStreamGetc(BSInterp *i, BSStream *s) {
  int c;
  CHK(i,s);
  if (s->use > s->top) {
    c = s->buf[s->top];
    s->top++;
  } else {
    /* fill buffer */
    int k;
    if (!s->drv || !s->drv->read) {
      /* write-only channel? */
      bsSetStringResult(i,"driver does not support read",BS_S_STATIC);
      return BS_SERROR;
    }
    if ((k = s->drv->read(i,s,s->buf,BS_SBUFSZ)) < 0)
      /* driver sets message */
      return BS_SERROR;
    if (k == 0)
      return BS_EOF; /* nothing left */
    s->top = 0;
    s->use = k;
    c = s->buf[s->top];
    s->top++;
  }
  return c;
}

int bsStreamRead(BSInterp *i, BSStream *s, void *_buf, int nbytes,
		 int exact) {
  char *buf = (char *)_buf;
  int code;
  int sofar = 0;
  CHK(i,s);
  if (nbytes > 0)
    CHK(i,buf);
  /* check read buffer */
  if (s->use > s->top) {
    if (nbytes < (s->top-s->use)) {
      memcpy(buf,s->buf+s->top,nbytes);
      s->top += nbytes;
      return nbytes;
    } else {
      memcpy(buf,s->buf+s->top,s->use-s->top);
      sofar += s->use-s->top;
      buf += s->use-s->top;
      nbytes -= s->use-s->top;
      s->use = s->top = 0; /* reset buffer */
      if (!exact)
	return sofar;
    }
  }

  if (!s->drv || !s->drv->read) {
    /* write-only channel? */
    bsSetStringResult(i,"driver does not support read",BS_S_STATIC);
    return -1;
  }

  while (nbytes > 0) {
    int k;
    k = s->drv->read(i,s,buf,nbytes);
    if (k < 0)
      return -1; /* error set at lower-level */
    if (k == 0)
      break; /* nothing left to read */
    sofar += k;
    nbytes -= k;
    buf += k;
    if (!exact)
      return sofar;
  }
  
  return sofar;
}

int bsStreamWrite(BSInterp *i, BSStream *s, void *_buf, int nbytes,
		  int exact) {
  char *buf = (char *)_buf;
  int sofar = 0, k;

  CHK(i,s);
  if (nbytes > 0)
    CHK(i,buf);
  if (!s->drv || !s->drv->write) {
    bsSetStringResult(i,"driver does not support write",BS_S_STATIC);
    return BS_ERROR;
  }
  while (nbytes > 0) {
    k = s->drv->write(i,s,buf,nbytes);
    if (k < 0)
      return -1;
    if (k == 0)
      break;
    sofar += k;
    buf += k;
    nbytes -= k;
    if (!exact)
      return sofar;
  }
  return sofar;
}

int bsStreamFlush(BSInterp *i, BSStream *s) {
  CHK(i,s);
  if (!s->drv || !s->drv->flush)
    /* allowed - no flushing supported/needed */
    return 0;
  return s->drv->flush(i,s);
}


/* append line to given string */
int bsStreamReadLn(BSInterp *i, BSStream *s, BSString *str) {
  int c;
  CHK(i,s);
  while ((c = bsStreamGetc(i,s)) >= 0) {
    bsStringAppChr(str,c);
    if (c == '\n')
      return 0;
  }
  return (c == BS_EOF ? 1 : -1);
}

int bsStreamPoll(BSInterp *i, BSStream *s, int flags) {
  CHK(i,s);
  if (flags == BS_POLL_READ) {
    /* special case - check buffer */
    if (s->top < s->use)
      return 1; /* yep - ready to read */
  }
  if (!s->drv || !s->drv->poll)
    return -1; /* driver does not support polling */
  return s->drv->poll(i,s,flags);
}

/* write string with optional 'nl' to given stream */
int bsStreamWriteStr(BSInterp *i, BSStream *s, char *buf, int nl) {
  int n = strlen(buf ? buf : ""), k;
  
  if ((k = bsStreamWrite(i,s,buf,n,1)) != n) {
    if (k < 0)
      return -1; /* failure - set elsewhere */
    else {
      bsSetStringResult(i,"string output truncated",BS_S_STATIC);
      return 1; /* truncation */
    }
  }
  if (nl) {
    if ((k = bsStreamWrite(i,s,"\n",1,1)) != 1) {
      if (k < 0)
	return -1; /* failure - set elsewhere */
      else {
	bsSetStringResult(i,"string output truncated",BS_S_STATIC);
	return 1; /* truncation */
      }
    }
  }
  return 0;
}


static int stream_destroy(BSObject *o);
static int stream_proc(BSObject *o, BSInterp *i, 
		       int objc, BSObject *objv[]);
  
static BSOpaqueDriver stream_driver = {
  "stream",
  0,
  NULL,   /* makerep */
  stream_destroy,
  stream_proc
};

static BSStream *getstream(BSObject *o) {
  assert(o != NULL);
  assert(o->opaqueRep != NULL);
  assert(o->opaqueRep->driver == &stream_driver);
  /* note - data field may be NULL if stream has been
     closed */
  return (BSStream *)(o->opaqueRep->data);
}

BSStream *bsStreamGet(BSInterp *i, BSObject *o) {
  if (!o || !(o->repValid & BS_T_OPAQUE) || !o->opaqueRep ||
      o->opaqueRep->driver != &stream_driver) {
    bsSetStringResult(i,"not a valid stream object",BS_S_STATIC);
    return NULL;
  }
  return (BSStream *)(o->opaqueRep->data);
}

static int stream_destroy(BSObject *o) {
  BSStream *s;
  if ((s = getstream(o)))
    bsStreamClose(NULL,s);
  return BS_OK;
}

enum {
  S_POLL,
  S_CLOSE,
  S_READBLOCK,
  S_READ,
  S_WRITE,
  S_FLUSH,
  S_GETC,
  S_READLN,
  S_WRITELN
};

static struct {
  char *name;
  int id;
} stream_proc_names[] = {
  { "poll", S_POLL },
  { "close", S_CLOSE },
  { "readblock", S_READBLOCK },
  { "read", S_READ },
  { "write", S_WRITE },
  { "flush", S_FLUSH },
  { "getc", S_GETC },
  { "readln", S_READLN },
  { "writeln", S_WRITELN },
  { NULL, -1 }
};

/*@bsdoc
object stream {
    procedure poll {
	usage {<stream> poll (read|write)}
	returns {True (1) if the corresponding operation would not
	    block or false (0) if the operation would block.}
	doc {
	  Checks the given stream to see if there is either data
	  available to be read (<code>read</code>) or if data can be
	  written without blocking (<code>write</code>).  Returns a
	  corresponding Boolean value ('1' or '0').  If the underlying
	  implementation does not support polling, an error is generated.
	}
    }
    procedure close {
	usage {<stream> close}
	doc {
	  Explicitly closes the stream.  Nothing further may be
	  written to or read from the stream.  Any buffers are flushed
	  and the underlying stream implementation is notified of the
	  closing.  For example, if <i>stream</i> represents a network
	  socket, the network socket will be shutdown.
	}
    }
    procedure readblock {
	usage {<stream> readblock <n> [<var>]}
	returns {A string containing the data (if <i>var</i> is not
					       specified) or a boolean
	    value (true if read successfully, false if not) (if
							     <i>var</i> is specified).}
	doc {
	    <p>
	    Reads exactly <i>n</i> bytes from the stream.  
	    If the stream is currently in an "end-of-file" condition,
	    then an empty string is returned (regardless of whether or
					      not <i>var</i> is specified).
	    Otherwise, if <i>n</i> bytes cannot be read, then an error
	    is generated.
	    </p>
	    <p>
	    If <i>var</i> is not specified, then the read data is
	    returned or a blank string is returned if an
	    "end-of-file" was encountered.  If <i>var</i> is
	    specified, then the read
	    data is stored in the variable <i>var</i> in the current
	    context and returns a Boolean value indicating whether
	    or not a block was read ('1' if a block was stored in
	    <i>var</i> or '0' if an "end-of-file" was encountered at
            the beginning of the block).
	    </p>
	    <p>
	    Note that an "end-of-file" is only recognized as such if
	    it occurs before any characters are actually read.  If
	    an "end-of-file" is encountered in the middle of a block
	    it is treated as a "short block" error.
	    </p>
	}
    }
    procedure read {
	usage {<stream> read [<n>]}
	returns {A string of bytes read from the stream.}
	doc {
	  <p>
	    Reads data from the stream.  If <i>n</i> is omitted, then
	    this function will read all available data up to an
	    "end-of-file" condition.  If <i>n</i> is specified, then any
	    available data up to <i>n</i> bytes are read.  The read data
	    is returned as a string.
	  </p>
	  <p>
	    Note that given a size to read <i>n</i>, it is valid
	    for this function to return a string that has fewer than
	    <i>n</i> bytes yet not be at the "end-of-file".
	    See <code>readblock</code> below for a method by which to
	    read a precise number of bytes.
	  </p>
	}
    }
    procedure write {
	usage {<stream> write <string>}
	returns {<i>string</i>}
	doc {
	    Writes the given <i>string</i> to the stream.
	}
    }
    procedure flush {
	usage {<stream> flush}
	doc {
	  Forces the stream to flush any output buffers, whether in
	  the stream object itself or its underlying implementation.
	}
    }
    procedure getc {
	usage {<stream> getc}
	returns {The next character or an empty string if no
	    characters are left.}
	doc {
	  Reads the next character from the stream and returns it.
	  If there are no characters left in the stream, then an empty
	  string is returned.
	}
    }
    procedure readln {
	usage {<stream> readln [<var>]}
	returns {A string containing the data (if <i>var</i> is not
					       specified) or a boolean
	    value (true if read successfully, false if not) (if
							     <i>var</i> is specified).}
	doc {
	    Reads the next line from the given stream.  If <i>var</i> is
	    not specified then the line that was read is returned, or an
	    error is generated if an "end-of-file" is encountered.
	    If <i>var</i> is specified then the line is
	    stored in the variable <i>var</i> in the current context and
	    a Boolean value is returned to indicate whether or not a
	    line was read ('1' indicates a line was read and stored in
			   <i>var</i> and '0' indicates that an "end-of-file" was
			   encountered).
	}
    }
    procedure writeln {
	usage {<stream> writeln <string>}
	returns {<i>string</i>}
	doc {
	  Writes the given <i>string</i> to the stream followed
	  by a newline character.	    
	}
    }
}
*/

#define BLOCKSZ 4096
static int stream_proc(BSObject *o, BSInterp *i, 
		       int objc, BSObject *objv[]) {
  BSStream *s = getstream(o);
  int k;

  if ((k = bsObjGetConstant(i,objv[0],stream_proc_names,
			    sizeof(stream_proc_names[0]),"stream proc")) < 0)
    return BS_ERROR;
  /* assume that for any operation, the stream must be open */
  if (!s) {
    bsSetStringResult(i,"stream is closed",BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  switch (stream_proc_names[k].id) {
  case S_POLL:
    {
      char *x;
      int flags, k;
      if (objc != 2) {
	bsSetStringResult(i,"usage: <stream> poll (read|write)",BS_S_STATIC);
	return BS_ERROR;
      }
      x = bsObjGetStringPtr(objv[1]);
      if (strcmp(x,"read") == 0)
	flags = BS_POLL_READ;
      else if (strcmp(x,"write") == 0)
	flags = BS_POLL_WRITE;
      else {
	bsSetStringResult(i,"poll mode must be 'read' or 'write'",BS_S_STATIC);
	return BS_ERROR;
      }
      if ((k = bsStreamPoll(i,s,flags)) < 0)
	return BS_ERROR;
      bsSetIntResult(i,k);
    }
    return BS_OK;

  case S_CLOSE:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <stream> close",BS_S_STATIC);
      return BS_ERROR;
    }
    bsStreamClose(i,s);
    o->opaqueRep->data = NULL;
    return BS_OK;

  case S_READBLOCK:
    {
      BSInt n, k;
      char *blk;

      bsClearResult(i);
      if (objc != 2 && objc != 3) {
	bsSetStringResult(i,"usage: <stream> readblock <n> [<var>]",
			  BS_S_STATIC);
	return BS_ERROR;
      }
      if (bsObjGetInt(i,objv[1],&n) != BS_OK)
	return BS_ERROR;
      if (n < 0) {
	bsSetStringResult(i,"cannot read block negative bytes long",
			  BS_S_STATIC);
	return BS_ERROR;
      }
      if (n == 0) { 
	if (objc == 3) {
	  bsObjInvalidate(bsGet(i,NULL,bsObjGetStringPtr(objv[2]),1),0);
	  bsSetIntResult(i,1);
	} else {
	  bsClearResult(i);
	}
	return BS_OK;
      }
      blk = bsAlloc(n,0);
      k = bsStreamRead(i,s,blk,n,1);
      if (k == 0) { /* eof */
	bsFree(blk);
	if (objc == 3) {
	  bsObjInvalidate(bsGet(i,NULL,bsObjGetStringPtr(objv[2]),1),0);
	  bsSetIntResult(i,0);
	} else
	  bsClearResult(i);
	return BS_OK;
      }
      if (k != n) {
	bsFree(blk);
	if (bsIsResultClear(i))
	  bsSetStringResult(i,"short block",BS_S_STATIC);
	return BS_ERROR;
      }
      if (objc == 3) {
	BSObject *o;
	o = bsGet(i,NULL,bsObjGetStringPtr(objv[2]),1);
	bsSetIntResult(i,1);
      } else {
	o = bsGetResult(i);
      }
      bsObjInvalidate(o,BS_T_STRING);
      bsStringSet(bsObjGetString(o),blk,n,BS_S_VOLATILE);
      bsFree(blk);
    }
    return BS_OK;

  case S_READ:
    {

      if (objc != 1 && objc != 2 && objc != 3) {
	bsSetStringResult(i,"usage: <stream> read [<n>]",BS_S_STATIC);
	return BS_ERROR;
      }
    
      if (objc == 1) {
	/* read everything as a string */
	BSString str;
	char blk[BLOCKSZ];
	int k;
	bsStringInit(&str);
	while ((k = bsStreamRead(i,s,blk,BLOCKSZ,0)) > 0)
	  bsStringAppend(&str,blk,k);
	if (k < 0) {
	  bsStringFreeSpace(&str);
	  return BS_ERROR;
	}
	bsSetObjResult(i,bsObjString(bsStringPtr(&str),bsStringLength(&str),
				     BS_S_VOLATILE));
	bsStringFreeSpace(&str);
	return BS_OK;
      } else {
	BSString *str;
	char *blk;
	BSInt n, k;
	if (bsObjGetInt(i,objv[1],&n) != BS_OK)
	  return BS_ERROR;
	if (n < 0) {
	  bsSetStringResult(i,"cannot read negative bytes",BS_S_STATIC);
	  return BS_ERROR;
	}
	if (n == 0) {
	  bsClearResult(i);
	  return BS_OK;
	}
	blk = bsAlloc(n,0);
	k = bsStreamRead(i,s,blk,n,0);
	if (k < 0) {
	  bsFree(blk);
	  return BS_ERROR;
	}
	bsClearResult(i);
	str = bsObjGetString(bsGetResult(i));
	bsStringAppend(str,blk,k);
	bsFree(blk);
	return BS_OK;
      }
    }
    break;

  case S_WRITE:
  case S_WRITELN:
    if (objc != 2) {
      bsAppendResult(i,"usage: <stream> ",
		     (stream_proc_names[k].id == S_WRITE ? 
		      "write" : "writeln"), " <string>",NULL);
      return BS_ERROR;
    }
    if (bsStreamWriteStr(i,s,bsObjGetStringPtr(objv[1]),
			 (stream_proc_names[k].id == S_WRITE ? 0 : 1)))
      return BS_ERROR;
    bsSetResult(i,objv[1]);
    return BS_OK;

  case S_FLUSH:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <stream> flush",BS_S_STATIC);
      return BS_ERROR;
    }
    if (bsStreamFlush(i,s))
      return BS_ERROR;
    return BS_OK;
    
  case S_GETC:
    {
      int c;
      char str[2];
      if (objc != 1) {
	bsSetStringResult(i,"usage: <stream> getc",BS_S_STATIC);
	return BS_ERROR;
      }
      if ((c = bsStreamGetc(i,s)) < 0) {
	if (c == BS_EOF) { /* eof - special case */
	  bsClearResult(i);
	  return BS_OK;
	}
	return BS_ERROR;
      }
      str[0] = (char)c;
      str[1] = '\0';
      bsSetStringResult(i,str,BS_S_VOLATILE);
    }
    return BS_OK;

  case S_READLN:
    {
      BSString str;
      int k;
      if (objc != 1 && objc != 2) {
	bsSetStringResult(i,"usage: <stream> readln [<var>]",BS_S_STATIC);
	return BS_ERROR;
      }

      bsStringInit(&str);
      
      if ((k = bsStreamReadLn(i,s,&str)) < 0)
	return BS_ERROR;
      else if (k > 0) { /* EOF */
	bsStringFreeSpace(&str);
	if (objc == 1)
	  /* stored in result - throw error */
	  return BS_ERROR;
	else {
	  /* stored in variable - set result code */
	  bsSetIntResult(i,0);
	  return BS_OK;
	}
      }

      /* now store the result */
      if (objc == 1)
	bsSetStringResult(i,bsStringPtr(&str),BS_S_VOLATILE);
      else {
	BSObject *o = bsGet(i,NULL,bsObjGetStringPtr(objv[1]),1);
	if (!o) {
	  bsStringFreeSpace(&str);
	  return BS_ERROR;
	}
	bsObjSetString(o,bsStringPtr(&str),bsStringLength(&str),
		       BS_S_VOLATILE);
	bsSetIntResult(i,1);
      }
      bsStringFreeSpace(&str);
    }
    return BS_OK;

  default:
    BS_FATAL("stream_proc: invalid proc id");
  }
  return BS_ERROR;
}

BSObject *bsStreamMakeOpaque(BSStream *s) {
  BSOpaque *p;
  p = bsAllocObj(BSOpaque);
  if (stream_driver.id == 0)
    stream_driver.id = bsUniqueId();
  p->driver = &stream_driver;
  p->data = (void *)s;
  return bsObjOpaque(p);
}

BSStream *bsStreamMake(void) {
  BSStream *s;
  s = bsAllocObj(BSStream);
  s->use = s->top = 0;
  return s;
}
