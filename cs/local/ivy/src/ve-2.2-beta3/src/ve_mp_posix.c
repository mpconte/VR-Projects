/** misc

<p>A POSIX implementation for MP support.</p>
   
<p>For remote processes, "rsh" is used to spawn a remote process.
The remote machine must have the same filesystem setup (i.e. the
binary must exist in the same directory at the same path as the
master node).  <code>VE_MP_RELIABLE</code> uses the rsh connection
for communication while <code>VE_MP_FAST</code> uses a UDP connection
which is negotiated during the "prepare" stage.</p>

<p>
For local processes and threads, a standard Unix pipe is used for
communicaiton.  Both <code>VE_MP_RELIABLE</code> and
<code>VE_MP_FAST</code> are implemented using the pipe.</p>

*/
#include "autocfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/uio.h> /* for writev */
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#ifdef HAS_POSIXSELECT
/* POSIX interface to select */
#include <sys/select.h>
#endif /* HAS_POSIXSELECT */
#ifdef HAS_POSIXPOLL
#include <sys/poll.h>
#endif /* HAS_POSIXPOLL */

/* POSIX socket interface */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* necessary VE interfaces */
#include <ve_alloc.h>
#include <ve_error.h>
#include <ve_clock.h>
#include <ve_debug.h>
#include <ve_mp.h>
#include <ve_main.h>
#include <ve_util.h>
#include <ve_thread.h>

#define MODULE "ve_mp_posix"

/** misc
    To help with buggy pthread implementations, the POSIX MP
    implementation uses a "spawn helper" to create other processes.
    This uses a process forked off early on to fork all other
    processes.
 */

/* SPAWN CODE STARTS HERE */
typedef struct spawn_pipe {
  /* file-descriptor to read on, and file-descriptor to
     write on */
  int rfd, wfd;
} spawn_pipe_t;

static void spawn_init(void); /* initialize spawning... */
static pid_t spawn_v(spawn_pipe_t *, char *prog, int argc, char **argv);
static pid_t spawn_vp(spawn_pipe_t *, char *prog, int argc, char **argv);

static VeThrMutex *spawn_mutex = NULL;

#define FIFODIRSZ 128
#define FIFOPATHSZ (FIFODIRSZ+64)

static FILE *spawn_read = NULL;
static FILE *spawn_write = NULL;
static int spawn_rfd = -1;
static int spawn_wfd = -1;
static pid_t helper_pid = (pid_t)-1;
static char fifodir[FIFODIRSZ];

/* messages */
static int read_str(FILE *f, char **buf, int *use_r) {
  int c;
  char *x = NULL;
  int sz = 0;
  int use = 0;
  
  if (buf && use_r) {
    x = *buf;
    use = *use_r;
  }

  while ((c = fgetc(f)) != EOF && c != '\0') {
    if (use >= sz) {
      if (use == 0)
	use = 32;
      while (use <= sz)
	use *= 2;
      x = realloc(x,use+1);
      assert(x != NULL);
    }
    x[sz++] = c;
  }
  if (sz == 0) {
    x = malloc(1);
    assert(x != NULL);
  }
  x[sz] = '\0';
  if (c == EOF) {
    free(x);
    return -1;
  }
  if (buf)
    *buf = x;
  else
    free(x);
  if (use_r)
    *use_r = use;
  return 0;
}

static void sigchld(int x) {
  while (waitpid(-1,NULL,WNOHANG) > 0)
    ;
}

static void spawn_helper(int rfd, int wfd) {
  FILE *in;
  int c;
  char **argv = NULL;
  int *argsz = NULL;
  int maxargc = 0;
  int argc;
  char *prog = NULL;
  int progsz = 0;
  char *argc_s = NULL;
  int argc_sz = 0;
  char *inpid_s = NULL;
  int inpidsz = 0;
  static int rcnt = 0;
  static int wcnt = 0;

  signal(SIGCHLD,sigchld);

  if (!(in = fdopen(rfd,"r"))) {
    perror("fdopen");
    exit(-1);
  }

  while ((c = getc(in)) != EOF) {
    switch (c) {
    case 'E':  /* execv */
    case 'P':  /* execvp */
      {
	char rfifo[FIFOPATHSZ], wfifo[FIFOPATHSZ];
	int infd = -1, outfd = -1;
	pid_t pid = (pid_t)-1;
	int k;

	if (read_str(in,&prog,&progsz)) {
	  write(wfd,"1",1);
	  continue;
	}
	if (read_str(in,&argc_s,&argc_sz)) {
	  write(wfd,"1",1);
	  continue;
	}
	if (sscanf(argc_s,"%d",&argc) != 1) {
	  write(wfd,"1",1);
	  continue;
	}
	if (argc < 0) {
	  write(wfd,"1",1);
	  continue;
	}
	if (argc >= maxargc) {
	  int omaxargc = maxargc;
	  maxargc = argc+1;
	  argv = realloc(argv,sizeof(char *)*(maxargc));
	  assert(argv != NULL);
	  argsz = realloc(argsz,sizeof(int)*(maxargc));
	  assert(argsz != NULL);
	  while (omaxargc < maxargc) {
	    argv[omaxargc] = NULL;
	    argsz[omaxargc] = 0;
	    omaxargc++;
	  }
	}
	for (k = 0; k < argc; k++) {
	  if (read_str(in,&(argv[k]),&(argsz[k]))) {
	    break;
	  }
	}
	if (k < argc) {
	  write(wfd,"1",1);
	  continue;
	}
	argv[k] = NULL;
	
	/* create rfifo */
	snprintf(rfifo,FIFOPATHSZ,"%s/rfifo.%d",fifodir,rcnt);
	rcnt++;
	while (mkfifo(rfifo,0600)) {
	  snprintf(rfifo,FIFOPATHSZ,"%s/rfifo.%d",fifodir,rcnt);
	  rcnt++;
	}
	/* create wfifo */
	snprintf(wfifo,FIFOPATHSZ,"%s/wfifo.%d",fifodir,wcnt);
	wcnt++;
	while (mkfifo(wfifo,0600)) {
	  snprintf(wfifo,FIFOPATHSZ,"%s/wfifo.%d",fifodir,wcnt);
	  wcnt++;
	}
	/* now fork */
	if ((pid = fork()) == 0) {
	  /* open fifos */
	  /* Ordering is subtle and important here... */
	  if ((infd = open(wfifo,O_RDONLY)) < 0 ||
	      (outfd = open(rfifo,O_WRONLY)) < 0) {
	    exit(-1);
	  }
	  close(0);
	  dup2(infd,0);
	  close(1);
	  dup2(outfd,1);
	  close(infd);
	  close(outfd);
	  if (c == 'P')
	    execvp(prog,argv);
	  else
	    execv(prog,argv);
	  perror("exec");
	  exit(-1);
	}
	close(infd);
	close(outfd);
	if (pid < 0) {
	  unlink(rfifo);
	  unlink(wfifo);
	  write(wfd,"1",1);
	  continue;
	}
	write(wfd,"0",1);
	{
	  char str[80];
	  snprintf(str,80,"%d",(int)pid);
	  write(wfd,str,strlen(str)+1);
	}
	write(wfd,rfifo,strlen(rfifo)+1);
	write(wfd,wfifo,strlen(wfifo)+1);
      }
      break;

    case 'K':
      exit(0); /* die */
     
    default:
      fprintf(stderr,"spawn_helper: unexepcted message: '%c' (%d)",c,c);
    }
  }
}

static void spawn_cleanup(void) {
  char p[FIFOPATHSZ];
  write(spawn_wfd,"K",1);
  kill(helper_pid,SIGTERM);
  veSnprintf(p,FIFOPATHSZ,"/bin/rm -rf %s >/dev/null 2>&1",fifodir); 
  system(p);
}

static void spawn_init(void) {
  /* create pipe */
  int rfd[2], wfd[2];
  
  if (pipe(rfd)) {
    perror("pipe(rfd)");
    exit(-1);
  }
  if (pipe(wfd)) {
    perror("pipe(wfd)");
    exit(-1);
  }
  /* create a temporary directory for FIFOs */
  {
    int k = 0;
    pid_t p = getpid();
    snprintf(fifodir,FIFODIRSZ,"/tmp/spawn.fifo.%d.%d",p,k);
    while (mkdir(fifodir,0700)) {
      if (errno != EEXIST) {
	fprintf(stderr,"failed to create fifo dir: %s",strerror(errno));
	exit(-1);
      }
      k++;
      snprintf(fifodir,FIFODIRSZ,"/tmp/spawn.fifo.%d.%d",p,k);
    }
  }
  if ((helper_pid = fork()) == 0) {
    close(wfd[1]);
    close(rfd[0]);
    spawn_helper(wfd[0],rfd[1]);
    exit(-1);
  }
  if (helper_pid < 0) {
    perror("fork");
    rmdir(fifodir);
    exit(-1);
  }
  spawn_rfd = rfd[0];
  close(rfd[1]);
  spawn_wfd = wfd[1];
  close(wfd[0]);
  if (!(spawn_read = fdopen(spawn_rfd,"r"))) {
    perror("fdopen(read)");
    rmdir(fifodir);
    exit(-1);
  }
  if (!(spawn_write = fdopen(spawn_wfd,"w"))) {
    perror("fdopen(write)");
    rmdir(fifodir);
    exit(-1);
  }
  atexit(spawn_cleanup);
}

static pid_t spawn_it(int code,
		      spawn_pipe_t *p, char *prog,
		      int argc, char **argv) {
  pid_t result = (pid_t)-1;
  int k, c;

  veThrMutexLock(spawn_mutex);

  if (!prog && argv && argv[0])
    prog = argv[0];

  if (argc < 0) {
    for (argc = 0; argv && argv[argc]; argc++)
      ;
  }

  fprintf(spawn_write,"%c%s%c%d%c",code,prog,'\0',argc,'\0');
  for (k = 0; k < argc; k++) {
    fprintf(spawn_write,"%s%c",argv[k],'\0');
  }
  fflush(spawn_write);

  if ((c = getc(spawn_read)) == EOF) {
    errno = EIO;
    goto end;
  }
  if (c == '1') {
    /* failure */
    errno = EINVAL;
    goto end;
  }
  if (c != '0') {
    errno = EIO;
    goto end;
  }
  {
    char *pid_s = NULL, *rfifo_s = NULL, *wfifo_s = NULL;
    unsigned long l;
    /* valid - read: pid_t, rfifo_s, wfifo_s */
    if (read_str(spawn_read,&pid_s,NULL) ||
	read_str(spawn_read,&rfifo_s,NULL) ||
	read_str(spawn_read,&wfifo_s,NULL)) {
      if (pid_s)
	free(pid_s);
      if (rfifo_s)
	free(rfifo_s);
      if (wfifo_s)
	free(wfifo_s);
      goto end;
    }
    if (sscanf(pid_s,"%lu",&l) != 1) {
      free(pid_s);
      free(rfifo_s);
      free(wfifo_s);
      goto end;
    }
    if (p) {
      /* Ordering is subtle and important here... */
      p->wfd = open(wfifo_s,O_WRONLY);
      p->rfd = open(rfifo_s,O_RDONLY);
    }
    (void) unlink(rfifo_s);
    (void) unlink(wfifo_s);
    free(pid_s);
    free(rfifo_s);
    free(wfifo_s);
    if (p) {
      if (p->rfd < 0 || p->wfd < 0) {
	close(p->rfd);
	close(p->wfd);
	goto end;
      }
    }
    result = (pid_t)l;
  }
  
 end:
  veThrMutexUnlock(spawn_mutex);
  return result;
}

/* searches path */
static pid_t spawn_vp(spawn_pipe_t *p, char *prog, int argc, char **argv) {
  return spawn_it('P',p,prog,argc,argv);
}

/* does not search path */
static pid_t spawn_v(spawn_pipe_t *p, char *prog, int argc, char **argv) {
  return spawn_it('E',p,prog,argc,argv);
}

/** misc
    <p>A compile-time constant (<code>MAX_PAYLOAD</code>) in this
    module determines the size of the largest message that will
    be sent without fragmenting on connection methods that are marked
    as being "limited".  Currently this only applies to UDP connections.
    Any value higher than about 1400 or so will sometimes have the
    TCP/IP stack fragment things.</p>
*/
#define MAX_PAYLOAD 30000
#define FBUFSZ (2*MAX_PAYLOAD)

typedef struct ve_mp_posix_fbuffer {
  char buf[FBUFSZ];
  int top, use;
} VeMPPosixFBuffer;

/* a posix connection */
typedef struct ve_mp_posix_conn {
  /* list - so we can keep track of what has/has-not
     been created */
  struct ve_mp_posix_conn *prev, *next;

  /* reliable connection 
     - 2 fds are required for pipes
     (also used for rsh)
   */
  int rel_recv_fd, rel_send_fd;
  VeMPPosixFBuffer rel_buf;  /* receive buffer for reliable transport */

  /* fast connection
     - if this is -1 then there is no "fast"
       connection (just use the reliable ones).
     - currently only one fd is needed here.
  */
  int fast_fd;
  VeMPPosixFBuffer fast_buf; /* receive buffer for fast transport */

  int rel_last;  /* if non-zero then the last fd we serviced was the
		    reliable one, otherwise (if 0) then we last serviced
		    the fast fd.  This allows us to fairly process packets
		    from both sources */

  /* connection info */
  char *method;  /* what method was requested */

  pid_t pid;  /* pid of child (if > 0) */

} VeMPPosixConn;

static VeMPPosixConn *conn_list = NULL;

/* Note return value:
   0  --> success
   -1 --> error
   1  --> timeout
*/
static int fillbuf(VeMPPosixFBuffer *f, int fd, long tmout) {
  int k;
  struct timeval tv;
  fd_set st;

  VE_DEBUGM(9,("filling buffer for fd %d",fd));

  if (f->use > 0)
    return 0;  /* "filled" - already something there */
  /* do select here... */
  FD_ZERO(&st);
  FD_SET(fd,&st);
  if (tmout >= 0) {
    tv.tv_sec = tmout/1000000;
    tv.tv_usec = tmout%1000000;
  }
  k = select(fd+1,&st,NULL,NULL,(tmout < 0 ? NULL : &tv));
  if (k < 0) {
    perror("select");
    return -1; /* something bad happened */
  }
  if (k == 0)
    return 1;  /* timeout */
  /* now try to actually read */
  errno = 0;
  while ((k = read(fd,f->buf,FBUFSZ)) <= 0 && errno == EINTR)
    ;
  if (k <= 0) {
    perror("read");
    return -1; /* error or eof */
  }
  f->top = 0;
  f->use = k;
  return 0;
}

/* buffered reading */
/* note results:
   - <0 --> error
   - 0  --> timeout
   - >0 --> num bytes read
*/
static int bufread(VeMPPosixFBuffer *f, int fd, void *buf, int n,
		   long tmout) {
  int left;
  /* does buffer need filling? */
  /* note: semantics of fillbuf() are different from bufread() -
     beware return value */
  VE_DEBUGM(8,("buffered read on fd %d (%d bytes, tmout = %ld)",fd,n,tmout));

  if (f->use == 0) {
    int k;
    if ((k = fillbuf(f,fd,tmout)) < 0)
      return -1;
    else if (k > 0)
      return 0;
  }
  left = f->use - f->top;
  assert(left > 0);
  if (left < n)
    n = left;
  memcpy(buf,f->buf+f->top,n);
  f->top += n;
  if (f->top >= f->use)
    f->top = f->use = 0;
  if (VE_ISDEBUG2(MODULE,25)) {
    int z = n;
    char *b = (char *)buf;
    while (z > 8) {
      fprintf(stderr, "data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
              b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
      z -= 8;
      b += 8;
    }
    if (z) {
      fprintf(stderr,"data:");
      while (z > 0) {
        fprintf(stderr," 0x%x",*b);
        b++;
        z--;
      }
      fprintf(stderr,"\n");
    }
  }
  return n; /* success */
}

int veMPImplWait(VeMPImplConn *c_v, VeMPImplConn *c_res_v, int n) {
  int ready = 0;
  int k;
  VeMPPosixConn **c = (VeMPPosixConn **)c_v;
  VeMPPosixConn **c_res = (VeMPPosixConn **)c_res_v;

  /* if a buffer is non-full and something is waiting, then
     fill that buffer... */
  VE_DEBUGM(6,("veMPImplWait enter"));
  ready = 0;
  for(k = 0; k < n; k++) {
    if (!c[k]) 
      c_res[k] = NULL;
    else {
      VE_DEBUGM(6,("filling reliable buffer %d",k));
      if (fillbuf(&(c[k]->rel_buf),c[k]->rel_recv_fd,0) < 0)
	veFatalError(MODULE,"failure filling reliable slave buffer");
      if (c[k]->fast_fd >= 0) {
	VE_DEBUGM(6,("filling fast buffer"));
	if (fillbuf(&(c[k]->fast_buf),c[k]->fast_fd,0) < 0)
	  veFatalError(MODULE,"failure filling fast slave buffer");
      }
      VE_DEBUGM(6,("finishing filling buffers: reliable=%d fast=%d",
		   c[k]->rel_buf.use, (c[k]->fast_fd >= 0 ? 
				       c[k]->fast_buf.use : -1)));
      /* if either buffer is non-empty, this one is ready */
      if (c[k]->rel_buf.use > 0 || (c[k]->fast_fd >= 0 && 
				    c[k]->fast_buf.use > 0)) {
	c_res[k] = c[k];
	ready = 1;
      } else {
	c_res[k] = NULL;
      }
    }
  }

  if (ready)
    return 0; /* something has data waiting */

  VE_DEBUGM(6,("wait: all buffers are empty"));

  /* otherwise, wait for data */
  {
    fd_set fs;
    int max = 0;
    FD_ZERO(&fs);
    for(k = 0; k < n; k++) {
      if (c[k]) {
	FD_SET(c[k]->rel_recv_fd,&fs);
	if (c[k]->rel_recv_fd >= max)
	  max = c[k]->rel_recv_fd+1;
	if (c[k]->fast_fd >= 0) {
	  FD_SET(c[k]->fast_fd,&fs);
	  if (c[k]->fast_fd >= max)
	    max = c[k]->fast_fd + 1;
	}
      }
    }
    VE_DEBUGM(6,("waiting on file descriptors..."));
    k = select(max,&fs,NULL,NULL,NULL);
    VE_DEBUGM(6,("...wait complete"));
    if (k <= 0)
      return -1; /* error */
  }
  /* we can't rely on select to tell us what got updated, so
     check them all again */
  VE_DEBUGM(6,("attempting to fill buffers again"));
  for(k = 0; k < n; k++) {
    if (!c[k]) {
      c_res[k] = NULL;
    } else {
      VE_DEBUGM(6,("filling reliable buffer %d",k));
      if (fillbuf(&(c[k]->rel_buf),c[k]->rel_recv_fd,0) < 0)
	veFatalError(MODULE,"failure filling reliable slave buffer");
      if (c[k]->fast_fd >= 0) {
	VE_DEBUGM(6,("filling fast buffer"));
	if (fillbuf(&(c[k]->fast_buf),c[k]->fast_fd,0) < 0)
	  veFatalError(MODULE,"failure filling fast slave buffer");
      }
      VE_DEBUGM(6,("finishing filling buffers: reliable=%d fast=%d",
		   c[k]->rel_buf.use, (c[k]->fast_fd >= 0 ? 
				       c[k]->fast_buf.use : -1)));
      /* if either buffer is non-empty, this one is ready */
      if (c[k]->rel_buf.use > 0 || (c[k]->fast_fd >= 0 && 
				    c[k]->fast_buf.use > 0)) {
	c_res[k] = c[k];
	ready = 1;
      } else {
	c_res[k] = NULL;
      }
    }
  }
  VE_DEBUGM(6,("wait complete after retry"));
  return 0; /* done */
}

static int wait_for_input(VeMPPosixConn *c, long tmout) {
  /* if a buffer is non-full and something is waiting, then
     fill that buffer... */
  VE_DEBUGM(6,("wait_for_input enter"));
  VE_DEBUGM(6,("filling reliable buffer"));
  if (fillbuf(&(c->rel_buf),c->rel_recv_fd,0) < 0)
    veFatalError(MODULE,"failure filling reliable slave buffer");
  if (c->fast_fd >= 0) {
    VE_DEBUGM(6,("filling fast buffer"));
    if (fillbuf(&(c->fast_buf),c->fast_fd,0) < 0)
      veFatalError(MODULE,"failure filling fast slave buffer");
  }
  VE_DEBUGM(6,("finishing filling buffers: reliable=%d fast=%d",
	       c->rel_buf.use, (c->fast_fd >= 0 ? c->fast_buf.use : -1)));
  /* if either buffer is non-empty, we're done */
  if (c->rel_buf.use > 0 || (c->fast_fd >= 0 && c->fast_buf.use > 0))
    return 0; /* done */
  /* if tmout is 0, then we don't try any further... */
  VE_DEBUGM(6,("wait: both buffers are empty"));
  if (tmout == 0) {
    VE_DEBUGM(6,("wait: timing out"));
    return 1; /* timeout */
  }
  /* otherwise, wait for timeout */
  {
    fd_set fs;
    int max, k;
    struct timeval tv;
    FD_ZERO(&fs);
    FD_SET(c->rel_recv_fd,&fs);
    max = c->rel_recv_fd + 1;
    if (c->fast_fd >= 0) {
      FD_SET(c->fast_fd,&fs);
      if (c->fast_fd >= max)
	max = c->fast_fd + 1;
    }
    if (tmout >= 0) {
      tv.tv_sec = tmout/1000000;
      tv.tv_usec = tmout%1000000;
    }
    VE_DEBUGM(6,("waiting on file descriptors..."));
    k = select(max,&fs,NULL,NULL,(tmout < 0 ? NULL : &tv));
    VE_DEBUGM(6,("...wait complete"));
    if (k < 0)
      return -1; /* error */
    else if (k == 0)
      return 1; /* timeout */
  }
  /* select says that somebody has work to do - there's only
     two fds so try filling their buffers again */
  VE_DEBUGM(6,("retry filling reliable buffer"));
  if (fillbuf(&(c->rel_buf),c->rel_recv_fd,0) < 0)
    veFatalError(MODULE,"failure filling reliable slave buffer");
  if (c->fast_fd >= 0) { 
    VE_DEBUGM(6,("retry filling fast buffer"));
    if (fillbuf(&(c->fast_buf),c->fast_fd,0) < 0)
      veFatalError(MODULE,"failure filling fast slave buffer");
  }
  /* either we filled a buffer or we got an error (which would have
     tripped one of the above fatal errors - so at least one of the
     buffers must be non-empty at this point */
  assert(c->rel_buf.use > 0 || (c->fast_fd >= 0 && c->fast_buf.use > 0));
  VE_DEBUGM(6,("wait complete after retry"));
  return 0; /* done */
}

/* assume we don't like partial reads.
   Note return value:
   0  --> success
   -1 --> partial read
   1  --> timeout
*/
/* Subtle note:  timeout here is applied to each individual
   buffered read, not the total time spent waiting in forceread.
   Fix this later on...
*/
static int forceread(VeMPPosixFBuffer *f, int fd, void *buf, int n,
		     long tmout) {
  char *b = (char *)buf;
  int k,sofar = 0;
  VE_DEBUGM(7,("forceread on fd %d (%d bytes, tmout = %ld)",fd,n,tmout));
  while ((k = bufread(f,fd,b,n,tmout)) > 0) {
    sofar += k;
    b += k;
    n -= k;
    if (n <= 0)
      return 0; /* read it */
  }
  if (k < 0)
    return -1; /* failure all round */
  else
    return 1; /* timeout of some kind */
}

#define MAX_KEY_BUF 256
#define MAX_CHECK_BUF 256

static void mkkey(char *key, char *check) {
  /* this is not meant to be particularly rigorous - just something
     to be reasonably sure that we are talking to the box we expect
     to be talking two - the handshake is two way - the slave sends
     the master the key, the master sends the slave the check in
     response */
  static int keylen = 32, init = 0;
  static unsigned long next = 0;
  int k;
  /* yes - this is like the rand() function from the K+R book */
  if (init == 0) {
    next = (unsigned long)time(NULL);
    init = 1;
  }
  for(k = 0; k < keylen; k++) {
    next = next * 1103515245 + 12345;
    key[k] = ((next>>6)%26) + 'a';
    next = next * 1103515245 + 12345;
    check[k] = ((next>>6)%26) + 'a';
  }
  key[k] = '\0';
  check[k] = '\0';
}


/* Start the remote process with a similar environment to ours.
   This is done over rsh currently and requires some trickery to
   do everything the way we want. 
 */
#define MAXDIRLEN 1024
#define wrstr(f,z) write((f),(z),strlen(z))
static pid_t create_remote(spawn_pipe_t *p,
			   char *host, int argc, char **argv) {
  char **nargv;
  int k;
  static char exec_path[256],str[2048];
  pid_t pid;
  int fd[2];
  char *rsh = "ssh"; /* rely on path to find it */
  char *largv[10];

  /* quick-hack to allow overriding of "rsh" command
     - note that we cannot support a command and arguments via
     this method - that would require more work...
  */
  {
    char *alt;
    if ((alt = getenv("VERSH")))
      rsh = alt;
  }

  /* setup the exec script on the other side */
  sprintf(exec_path,"/tmp/ve_mp_exec.%d",(int)getpid());

  VE_DEBUGM(4,("rsh_exec on %s",host));

  k = 0;
  largv[k++] = rsh;
  largv[k++] = host;
  largv[k++] = "/bin/sh";
  largv[k++] = NULL;
  if ((pid = spawn_vp(p,NULL,-1,largv)) <= 0) {
    veError(MODULE, "failed while setting up rsh_exec script");
    return -1;
  }

  {
    char dir[MAXDIRLEN];
    if (!getcwd(dir,MAXDIRLEN)) {
      veError(MODULE, "present directory is too long or unreadable");
      return -1;
    }
    sprintf(str,"cd \"%s\"\n",dir);
    wrstr(p->wfd,str);
  }
  {
    /* export any potentially useful environment variables */
    char *x, *envs[] = {
      "VEROOT", "VEENV", "LD_LIBRARY_PATH", "DISPLAY", "PATH",
      "__GL_SYNC_TO_VBLANK", "__GL_FSAA_MODE", "__GL_DEFAULT_LOG_ANISO",
      NULL
    };
    int k;
    for(k = 0; envs[k]; k++) {
      if ((x = getenv(envs[k]))) {
	wrstr(p->wfd,envs[k]);
	wrstr(p->wfd,"=");
	wrstr(p->wfd,x);
	wrstr(p->wfd," ; export ");
	wrstr(p->wfd,envs[k]);
	wrstr(p->wfd,"\n");
      }
    }
    sprintf(str,"VEDEBUG=%s ; export VEDEBUG\n",veGetDebugStr());
    wrstr(p->wfd,str);
  }

  wrstr(p->wfd,"exec");
  for (k = 0; k < argc; k++) {
    wrstr(p->wfd," '");
    wrstr(p->wfd,argv[k]);
    wrstr(p->wfd,"'");
  }
  wrstr(p->wfd,"\n");
  
  return pid;
}

static int open_ip(char *proto, char *host, char *portstr) {
  int fd;
  struct hostent *hp;
  struct sockaddr_in addr;
  int port;

  assert(proto != NULL);
  assert(host != NULL);
  assert(portstr != NULL);
  VE_DEBUGM(4,("open_ip(%s,%s,%s)",proto,host,portstr));
  if ((port = atoi(portstr)) == 0) {
    veError(MODULE, "invalid port for TCP connection: %s", portstr);
    return -1;
  }
  memset(&addr,0,sizeof(addr));
  if ((hp = gethostbyname(host)) != NULL)
    memcpy(&(addr.sin_addr.s_addr),hp->h_addr,hp->h_length);
  else {
    addr.sin_addr.s_addr = inet_addr(host);
  }
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (strcmp(proto,"tcp") == 0 || strcmp(proto,"udp") == 0) {
    if ((fd = socket(AF_INET, 
		     (strcmp(proto,"tcp") == 0 ? SOCK_STREAM : SOCK_DGRAM), 
		     0)) < 0) {
      veError(MODULE, "failed to create socket: %s", strerror(errno));
      return -1;
    }
    /* set up a nice big receive buffer */
    {
      int x = 65536;
      setsockopt(fd,SOL_SOCKET,SO_RCVBUF,&x,sizeof(x));
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      veError(MODULE, "failed to connect socket: %s", strerror(errno));
      close(fd);
      return -1;
    }
  } else {
    veError(MODULE,"unsupported IP protocol: %s",proto);
    return -1;
  }
  return fd;
}

void veMPImplCleanup(void) {
  /* nothing to do - exit_cback is deprecated */
}

static VeMPPosixConn *create_conn(void) {
  VeMPPosixConn *c;
  c = veAllocObj(VeMPPosixConn);
  c->rel_recv_fd = c->rel_send_fd = c->fast_fd = -1;
  /* add to list */
  c->next = conn_list;
  if (conn_list)
    conn_list->prev = c;
  conn_list = c;
  return c;
}

VeMPImplConn veMPImplSlaveCreate(int *args, char **argv) {
  VeMPPosixConn *c;
  int fd;

  VE_DEBUGM(3,("creating slave-end for communications"));

  c = create_conn();
  /* save file descriptors to avoid collisions with
     printf/scanf/etc. in application */
  c->rel_recv_fd = dup(0); /* stdin */
  c->rel_send_fd = dup(1); /* stdout */

  /* set stdin to be /dev/null */
  fd = open("/dev/null",O_RDONLY);
  close(0);
  dup2(fd,0);
  close(fd);

  /* set stdout to be stderr */
  close(1);
  dup2(2,1);

  return (VeMPImplConn)c;
}

/* Sysdep tags for the POSIX layer */
#define SYSDEP_BESTADDR 0x1   /* request appropriate IP address */
#define SYSDEP_CONNUDP  0x2   /* start UDP-based connection (FAST) */
#define SYSDEP_CONNKEY  0x3   /* simple sanity check initialization */
#define ADDRSIZE 80
#define DELIM " \r\t\n"
int veMPImplSysdep(VeMPImplConn c_v, VeMPPkt *p) {
  VeMPPosixConn *c = (VeMPPosixConn *)(c_v);

  VE_DEBUGM(4,("received SYSDEP message: %d",p->tag));

  switch (p->tag) {
  case SYSDEP_CONNUDP:
    /* remote side has asked us to connect up a UDP socket for
       fast transport */
    {
      /* 
	 argument is a string of the format:
	 <address> <port> <key> <check>
      */
      char *addr, *port, *key, *check;
      char checkbuf[MAX_CHECK_BUF];
      int fd, l;

      VE_DEBUGM(2,("handling SYSDEP_CONNUDP"));
      if (!p->data) {
	veError(MODULE, "missing payload for SYSDEP_CONNUDP");
	return -1;
      }
      if (!(addr = strtok(p->data,DELIM))) {
	veError(MODULE, "missing address in SYSDEP_CONNUDP");
	return -1;
      }
      if (!(port = strtok(NULL,DELIM))) {
	veError(MODULE, "missing port in SYSDEP_CONNUDP");
	return -1;
      }
      if (!(key = strtok(NULL,DELIM))) {
	veError(MODULE, "missing key in SYSDEP_CONNUDP");
	return -1;
      }
      if (!(check = strtok(NULL,DELIM))) {
	veError(MODULE, "missing check in SYSDEP_CONNUDP");
	return -1;
      }
      VE_DEBUGM(2,("making udp connection to %s:%s",addr,port));
      if ((fd = open_ip("udp",addr,port)) < 0)
	return -1;
      /* write our key */
      l = strlen(key)+1;
      VE_DEBUGM(2,("sending key (%d bytes): %s",l,key));
      if (write(fd,key,l) != l) {
	veError(MODULE, "failed to write key: %s",strerror(errno));
	return -1;
      }
      /* look for check value */
      l = strlen(check)+1;
      VE_DEBUGM(2,("receiving check value (%d bytes)",l));
      /* should come in one packet */
      if (read(fd,checkbuf,l) != l) {
	veError(MODULE, "failed to read check value: %s", strerror(errno));
	return -1;
      }
      VE_DEBUGM(2,("comparing check value from master"));
      if (memcmp(checkbuf,check,l)) {
	veError(MODULE,"check value from master does not match");
	return -1;
      }
      VE_DEBUGM(2,("udp connection established"));
      /* update connection to use this for fast transport */
      if (c->fast_fd >= 0)
	close(c->fast_fd);
      c->fast_fd = fd;
      c->fast_buf.top = c->fast_buf.use = 0;
    }
    break;

  case SYSDEP_BESTADDR:
    {
      /* look at existing address to determine which address we should be
	 talking on - blank if we can't tell */
      char str[ADDRSIZE] = "";
      char *x;

      VE_DEBUGM(2,("handling SYSDEP_BESTADDR"));

      /* Use SSH info if available... */
      if ((x = getenv("SSH_CLIENT"))) {
	strncpy(str,x,ADDRSIZE);
	x = str;
	while (*x && !isspace(*x))
	  x++;
	if (isspace(*x))
	  *x = '\0';
      } else {
	struct sockaddr_in server;
	socklen_t i;

	i = sizeof(server);
	/* look at the address of our reliable receive fd */
	if (getpeername(c->rel_recv_fd,(struct sockaddr *)&server,&i) == 0 &&
	    server.sin_family == AF_INET)
	  strcpy(str,inet_ntoa(server.sin_addr));
      }
      VE_DEBUGM(2,("bestaddr is '%s'",str));
      veMPImplSendv(c,0,VE_MP_RELIABLE,VE_MPMSG__SYSDEP,SYSDEP_BESTADDR,
		    (void *)str,ADDRSIZE);
    }
    break;
  default:
    veFatalError(MODULE,"got SYSDEP, tag %d that I do not know how to handle",
		 p->tag);
  }
  return 0;
}

int veMPImplPrepare(VeMPImplConn c_v, int flags) {
  VeMPPosixConn *c = (VeMPPosixConn *)(c_v);
  assert(c != NULL);

  VE_DEBUGM(2,("preparing slave connection"));

  if (c->method && (strcmp(c->method,VE_MP_REMOTE) == 0)) {
    /* this is a remote connection */
    /* attempt to reconnect */
    char str[MAX_KEY_BUF+MAX_CHECK_BUF+256];
    char key[MAX_KEY_BUF], keyr[MAX_KEY_BUF];
    char check[MAX_CHECK_BUF];
    VeMPPkt *p;
    struct sockaddr_in inaddr;
    socklen_t alen;
    int l;
    int fd;
    VE_DEBUGM(2,("creating udp connection"));
    if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
      veError(MODULE, "failed to create udp socket: %s", strerror(errno));
      return -1;
    }
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = INADDR_ANY;
    inaddr.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&inaddr, sizeof(inaddr)) < 0) {
      veError(MODULE, "failed to bind udp socket: %s", strerror(errno));
      return -1;
    }
    /* build the reconnect string */
    /* probe the other side to see which IP address we're talking to
       them from */
    VE_DEBUGM(2,("asking for best address"));
    if (veMPImplSendv(c,0,VE_MP_RELIABLE,VE_MPMSG__SYSDEP,
		      SYSDEP_BESTADDR,NULL,0)) {
      veError(MODULE,"failed to send SYSDEP_BESTADDR: %s",strerror(errno));
      return -1;
    }
    if (veMPImplRecv(c,&p,-1)) {
      veError(MODULE,"failed to receive response to SYSDEP_BESTADDR");
      return -1;
    }
    if (!p || !(p->data)) {
      veError(MODULE,"invalid response to SYSDEP_BESTADDR");
      return -1;
    }
    VE_DEBUGM(2,("best address is '%s'",p->data));
    alen = sizeof(inaddr);
    if (getsockname(fd,(struct sockaddr *)&inaddr,&alen) < 0) {
      veError(MODULE, "cannot determine name of socket: %s", strerror(errno));
      return -1;
    }
    mkkey(key,check);
    sprintf(str,"%s %d %s %s",p->data,ntohs(inaddr.sin_port),key,check);
    veMPPktDestroy(p);
    /* now send the request */
    VE_DEBUGM(2,("sending reconnect to slave: %s",str));
    if (veMPImplSendv(c,0,VE_MP_RELIABLE,VE_MPMSG__SYSDEP,SYSDEP_CONNUDP,str,
		      strlen(str)+1)) {
      veError(MODULE,"failed to send SYSDEP_CONNUDP: %s",strerror(errno));
      return -1;
    }
    /* now wait for packet */
    alen = sizeof(inaddr);
    l = strlen(key)+1;
    errno = 0;
    VE_DEBUGM(2,("waiting for key from slave"));
    if (recvfrom(fd,keyr,l,0,(struct sockaddr *)&inaddr,&alen) != l) {
      veError(MODULE,"failed to read key from slave: %s",
	      strerror(errno));
      return -1;
    }
    VE_DEBUGM(2,("connecting udp ports"));
    if (connect(fd,(struct sockaddr *)&inaddr,alen) < 0) {
      veError(MODULE,"could not connect udp port to slave: %s",
	      strerror(errno));
      return -1;
    }
    VE_DEBUGM(2,("checking key"));
    if (memcmp(keyr,key,l) != 0) {
      veError(MODULE,"udp connect failed - key mismatch ('%s' vs '%s')",
	      key,keyr);
      return -1;
    }
    /* key is okay - send check */
    l = strlen(check)+1;
    errno = 0;
    VE_DEBUGM(2,("sending check value"));
    if (write(fd,check,l) != l) {
      veError(MODULE,"failed to send check to slave: %s",strerror(errno));
      return -1;
    }
    VE_DEBUGM(2,("udp connection to slave established"));
    /* update connection to use this for fast transport */
    if (c->fast_fd >= 0)
      close(c->fast_fd);
    c->fast_fd = fd;
    c->fast_buf.top = c->fast_buf.use = 0;
  }
  VE_DEBUGM(2,("preparation complete"));
  return 0;
}

int veMPImplRecv(VeMPImplConn c_v, VeMPPkt **p_r, long tmout) {
  VeMPPosixConn *c = (VeMPPosixConn *)(c_v);
  VeMPPkt ptmp, *p;
  int k;
  int rel_first;
  
  if (p_r)
    *p_r = NULL;

  VE_DEBUGM(3,("receiving packet"));

  /* read in a "pkt" */
  if ((k = wait_for_input(c,tmout))) {
    VE_DEBUGM(3,("receive failed: %s",(k < 0 ? "error" : "timeout")));
    return k; /* either error or tmout */
  }

  /* at least one pipe has a packet to read and it is in the buffer */
  rel_first = !(c->rel_last); /* did we read the reliable stream last? */
  if (rel_first) {
    /* check reliable stream first */
    VE_DEBUGM(3,("receive: checking reliable first"));
    if (c->rel_buf.use > 0) {
      /* read in a packet */
      if (k = forceread(&(c->rel_buf),c->rel_recv_fd,&ptmp,
			sizeof(ptmp),tmout)) {
	if (k < 0) {
	  VE_DEBUGM(3,("receive: read failed: %s", strerror(errno)));
	  return -1; /* that's bad */
	}
	goto skip_pipe1;
      }
      p = veMPPktCreate(0);
      *p = ptmp;
      p->data = NULL;
      if (p->dlen > 0) {
	p->data = veAlloc(p->dlen,0);
	/* we're reading the payload now - note that we switch over to
	   a "-1" timeout at this point to avoid reading a payload
	   fragment. */
	if (forceread(&(c->rel_buf),c->rel_recv_fd,p->data,p->dlen,-1)) {
	  VE_DEBUGM(3,("receive: payload failed: %s",strerror(errno)));
	  return -1;
	}
      }
      if (p_r)
	*p_r = p;
      else
	veMPPktDestroy(p);
      c->rel_last = 1; /* last read from reliable stream */
      VE_DEBUGM(3,("receive: returning packet from reliable"));
      return 0; /* successful */
    }
  }
  
 skip_pipe1:
  /* try the fast stream */
  if (c->fast_fd >= 0 && c->fast_buf.use > 0) {
    /* read in a packet */
    VE_DEBUGM(3,("receive: checking fast pipe"));
    if (k = forceread(&(c->fast_buf),c->fast_fd,&ptmp,
		      sizeof(ptmp),tmout)) {
      if (k < 0) {
	VE_DEBUGM(3,("receive: bad read on fast pipe: %s",strerror(errno)));
	return -1; /* that's bad */
      }
      goto skip_pipe2;
    }
    p = veMPPktCreate(0);
    *p = ptmp;
    p->data = NULL;
    if (p->dlen > 0) {
      p->data = veAlloc(p->dlen,0);
      /* we're reading the payload now - note that we switch over to
	 a "-1" timeout at this point to avoid reading a payload
	 fragment. */
      if (forceread(&(c->fast_buf),c->fast_fd,p->data,p->dlen,-1)) {
	VE_DEBUGM(3,("receive: failed on fast pipe payload: %s",strerror(errno)));
	return -1;
      }
    }
    if (p_r)
      *p_r = p;
    else
      veMPPktDestroy(p);
    c->rel_last = 0; /* last read from fast stream */
    VE_DEBUGM(3,("receive: returning packet from fast pipe"));
    return 0; /* successful */
  }
  
 skip_pipe2:

  if (!rel_first) {
    /* check reliable stream last */
    VE_DEBUGM(3,("receive: checking reliable last"));
    if (c->rel_buf.use > 0) {
      /* read in a packet */
      if (k = forceread(&(c->rel_buf),c->rel_recv_fd,&ptmp,
			sizeof(ptmp),tmout)) {
	if (k < 0) {
	  VE_DEBUGM(3,("receive: read failed: %s", strerror(errno)));
	  return -1; /* that's bad */
	}
	goto skip_pipe3;
      }
      p = veMPPktCreate(0);
      *p = ptmp;
      p->data = NULL;
      if (p->dlen > 0) {
	p->data = veAlloc(p->dlen,0);
	assert(p->data != NULL);
	/* we're reading the payload now - note that we switch over to
	   a "-1" timeout at this point to avoid reading a payload
	   fragment. */
	if (forceread(&(c->rel_buf),c->rel_recv_fd,p->data,p->dlen,-1)) {
	  VE_DEBUGM(3,("receive: payload failed: %s",strerror(errno)));
	  return -1;
	}
      }
      if (p_r)
	*p_r = p;
      else
	veMPPktDestroy(p);
      c->rel_last = 1; /* last read from reliable stream */
      VE_DEBUGM(3,("receive: returning packet from reliable"));
      return 0; /* successful */
    }
  }

 skip_pipe3:
  /* we've skipped all pipes which means we've timedout everywhere */
  VE_DEBUGM(3,("receive: timeout - no packets available"));
  return 1;
}

int veMPImplSendv(VeMPImplConn c, unsigned long seq, int ch,
		  int msg, int tag, void *data, int dlen) {
  VeMPPkt p;
  p.seq = seq;
  p.ch = ch;
  p.msg = msg;
  p.tag = tag;
  p.data = data;
  p.dlen = data ? dlen : 0;
  return veMPImplSend(c,&p);
}

int veMPImplSend(VeMPImplConn c_v, VeMPPkt *pp) {
  VeMPPosixConn *c = (VeMPPosixConn *)c_v;
  VeMPPkt p;
  struct iovec v[2];
  int n = 0, tot = 0;
  p = *pp; /* make a copy to tweak */
  /* If channel is bogus, or payload is too big, or there is
     no fast channel, then force communication to be over the
     reliable pipe.
  */
  memset(v,0,sizeof(v));
  if ((p.ch != VE_MP_RELIABLE && p.ch != VE_MP_FAST) || 
      p.dlen > MAX_PAYLOAD || c->fast_fd < 0)
    p.ch = VE_MP_RELIABLE; /* default to reliable */
  p.data = NULL; /* pointer is always bogus anyways in packet */
  if ((!pp->data && p.dlen != 0) || p.dlen < 0) {
    veWarning(MODULE,"correcting bogus payload properties: p.dlen = %d",p.dlen);
    p.dlen = 0; /* no data to send */
  }

  v[n].iov_base = (void *)&p;
  v[n].iov_len = sizeof(p);
  tot += v[n].iov_len;
  n++;
  if (p.dlen > 0) {
    v[n].iov_base = pp->data;
    v[n].iov_len = p.dlen;
    tot += v[n].iov_len;
    n++;
  }
  errno = 0;

  VE_DEBUGM(3,("send: sending %d bytes on %s", tot,
	       (p.ch == VE_MP_RELIABLE ? "reliable" : "fast")));
  if (writev((p.ch == VE_MP_RELIABLE ? c->rel_send_fd : c->fast_fd),
	     v,n) != tot) {
    veError(MODULE,"writev failed: %s",strerror(errno));
  }
  return 0;
}

void veMPImplDestroy(VeMPImplConn c_v) {
  VeMPPosixConn *c = (VeMPPosixConn *)c_v;
  if (c) {
    /* remove from list */
    if (c->next)
      c->next->prev = c->prev;
    if (c->prev)
      c->prev->next = c->next;
    if (conn_list == c)
      conn_list = c->next;
    /* actually free the structure */
    veFree(c->method);
    if (c->fast_fd >= 0)
      close(c->fast_fd);
    if (c->rel_recv_fd >= 0)
      close(c->rel_recv_fd);
    if (c->rel_send_fd >= 0)
      close(c->rel_send_fd);
    if (c->pid > 0)
      kill(c->pid,SIGTERM);
    veFree(c);
  }
}

/* create a connection */
VeMPImplConn veMPImplCreate(char *type, int id, char *node, 
			    int argc, char **argv) {
  int sendp[2], recvp[2]; /* pipes: 0 - rd, 1 - wr */
  pid_t p;
  VeMPPosixConn *c;

  if (!type)
    type = VE_MP_THREAD;

  VE_DEBUGM(2,("creating slave %d - %s (%s)", id, type,
	       node ? node : "<null>"));

  c = create_conn();
  c->method = veDupString(type);
  
  /* what type of connection do we want? */
  if (strcmp(type,VE_MP_THREAD) == 0) {
    /* create "other" connection side */
    VeMPPosixConn *sl;
    int sendp[2], recv[2];
    
    if (pipe(sendp) != 0 || pipe(recvp) != 0) {
      veError(MODULE,"failed to create pipes: %s",strerror(errno));
      return NULL;
    }
    c->rel_recv_fd = recvp[0];
    c->rel_send_fd = sendp[1];
    sl = create_conn();
    sl->rel_recv_fd = sendp[0];
    sl->rel_send_fd = recvp[1];
    VE_DEBUGM(2,("init: creating local thread slave"));
    veMPCreateSlave(sl); /* done */
  } else if (strcmp(type,VE_MP_LOCAL) == 0 || 
	     strcmp(type,VE_MP_REMOTE) == 0) {
    /* fork... */
    spawn_pipe_t p;
    VE_DEBUGM(2,("init: forking process"));
    /* use spawn helper */
    if (strcmp(type,VE_MP_REMOTE) == 0) {
      /* remote process */
      c->pid = create_remote(&p,node,argc,argv);
    } else {
      /* local process */
      c->pid = spawn_vp(&p,NULL,-1,argv);
    }
    if (c->pid <= 0) {
      veError(MODULE,"failed to spawn: %s",strerror(errno));
      veMPImplDestroy(c);
      return NULL;
    }
    c->rel_recv_fd = p.rfd;
    c->rel_send_fd = p.wfd;
  } else {
    veError(MODULE,"unsupported mp type: %s",type);
    veMPImplDestroy(c);
    return NULL;
  }

  VE_DEBUGM(2,("init: returning connection handle"));
  return c;
}

void veMPImplEarlyInit(void) {
  /* set up spawning helper */
  spawn_mutex = veThrMutexCreate();
  spawn_init();
}
