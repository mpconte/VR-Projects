/* A simple Unix-based back-end to ve_mp - uses pipes and rsh for
   spawning programs.  Assumes that filesystem is shared between
   master and slave such that paths and directories (wrt to the
   VE application) are consistent.  This is easily achieved with
   NFS or by copying all files to the slaves.

   To help in communication latency, if we use rsh to spawn a
   slave, we use the VE_MPMSG_RECONNECT immediately talking to the
   slave to get it to reconnect to us directly over a socket.
*/
/* Note that we know have to handle UDP messages in a single read.
   There are platforms (some Linux kernels) where two UDP messages
   sent from the same thread in succession get sent out in reverse order.
   To get around this we cannot send out the header and the payload separately
   or we will get very easily confused.  This means that we need to know
   which fd's are packet-based and which aren't because the read semantics
   will be very different in each (packet-based reads an entire message in
   one-shot, others use forceread to piece it together).  The flags that used
   to be used for signalling whether writev was "okay" for an fd, are not used
   to signal whether or not that stream is made up of packets (and has been
   renamed appropriately).
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/uio.h>
#include <time.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ve_error.h>
#include <ve_clock.h>
#include <ve_debug.h>
#include <ve_mp.h>
#include <ve_main.h>

#define MODULE "ve_mp_unix"

/* In theory, the maximum payload may be as much as 65516 or as little 
   as 556.  Without fragmentation every system we care about ahouls be able to
   handle ~1480.  With fragmentation, it should be higher.  The following
   is a "guesstimate".
 */
#define MAX_PAYLOAD 30000

/* TCP is more "reliable" in general than UDP, but
   UDP is much faster in this case (we care more about
   latency than about throughput so we'll use UDP by
   default.  Note that if we drop a packet, the current
   effect will be that the system hangs.  This may be
   fixable with the timer stuff in the future but will
   require a fair bit of work.  It is not clear at this point
   if such work is really worthwhile.
*/
#define DEFAULT_PROTO "udp"

static char *default_proto(void) {
  static char *proto = NULL;
  if (!proto) {
    proto = veGetOption("mp_proto");
    if (!proto)
      proto = DEFAULT_PROTO;
  }
  return proto;
}

static char *find_rsh() {
  return "ssh";
}

/* we use the following instead of read/close to hide
   buffering issues which vary from connection to connection
   (e.g. UDP vs. TCP vs. pipes) */
#define FBUFSZ (2*MAX_PAYLOAD)
typedef struct ve_mp_fbuffer {
  char buf[FBUFSZ];
  int top, use;
} ve_mp_fbuffer;
ve_mp_fbuffer **fbufs = NULL;
int fbufsz = 0;

static int bufread(int fd, void *buf, int n) {
  int left;
  ve_mp_fbuffer *f;
  if (!fbufs || fbufsz <= fd) {
    int nsz = 2*(fd+1);
    fbufs = realloc(fbufs,nsz*sizeof(ve_mp_fbuffer));
    assert(fbufs != NULL);
    while (fbufsz < nsz)
      fbufs[fbufsz++] = NULL;
  }
  if (!fbufs[fd]) {
    fbufs[fd] = calloc(1,sizeof(ve_mp_fbuffer));
    assert(fbufs[fd] != NULL);
  }
  f = fbufs[fd];
  /* does buffer need filling? */
  if (f->use == 0) {
    int k;
    if ((k = read(fd,f->buf,FBUFSZ)) <= 0)
      return k;
    f->top = 0;
    f->use = k;
  }
  left = f->use - f->top;
  assert(left > 0);
  if (left < n)
    n = left;
  memcpy(buf,f->buf+f->top,n);
  f->top += n;
  if (f->top >= f->use)
    f->top = f->use = 0;
  return n;
}

static int bufclose(int fd) {
  if (fbufsz > fd && fbufs[fd]) {
    free(fbufs[fd]);
    fbufs[fd] = NULL;
  }
  return close(fd);
}

/* if we're dealing with a network connection our stream may get
   "packet-ized" which may mean that data comes in chunks - be wary */
static int forceread(int fd, void *buf, int n) {
  char *b = (char *)buf;
  int k,sofar = 0;
  while ((k = bufread(fd,b,n)) > 0) {
    sofar += k;
    b += k;
    n -= k;
    if (n <= 0)
      return sofar;
  }
  if (k < 0)
    return -1; /* failure all round */
  else
    return sofar; /* eof */
}

#define MAX_KEY_BUF 256
#define MAX_CHECK_BUF 256

static int reconnect_fd = -1;

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

/* Sysdep tags for the POSIX layer */
#define SYSDEP_BESTADDR 0x1
#define ADDRSIZE 80

int veMP_OSSysdep(int tag, void *data, int dlen, int recv_h, int send_h) {
  switch (tag) {
  case SYSDEP_BESTADDR:
    {
      /* look at existing address to determine which address we should be
	 talking on - blank if we can't tell */
      char str[ADDRSIZE];
      struct sockaddr_in server;
      int i;
      memset(str,'\0',ADDRSIZE);
      i = sizeof(server);
      if (getpeername(recv_h,(struct sockaddr *)&server,&i) >= 0)
	strcpy(str,inet_ntoa(server.sin_addr));
      veMP_OSSend(send_h,VE_MPMSG_RESPONSE,0,(void *)str,ADDRSIZE);
    }
    break;
  default:
    veFatalError(MODULE,"got MPMSG_SYSDEP, tag %d that I do not know how to handle",tag);
  }
  return 0;
}

static int get_best_addr(int recv_h, int send_h, char *str) {
  char *data;
  veMP_OSSend(send_h,VE_MPMSG__SYSDEP,SYSDEP_BESTADDR,NULL,0);
  veMP_OSRecv(recv_h,NULL,NULL,(void **)&data,NULL);
  if (data) {
    strcpy(str,data);
    free(data);
  } else {
    str[0] = '\0';
  }
  return 0;
}

static int init_reconnect(int recv_h, int send_h, char *proto, char *base) {
  int fd;
  struct sockaddr_in server;
  int i;
  char host[ADDRSIZE];

  /* query other side for the address we should use to talk to them */
  get_best_addr(recv_h,send_h,host);
  if (host[0] == '\0' && (gethostname(host,ADDRSIZE) < 0)) {
    veError(MODULE, "cannot determine best address or host name: %s",
	    strerror(errno));
    return -1;
  }

  if (strcmp(proto,"tcp") == 0) {
    /* reuse incoming socket */
    if (reconnect_fd < 0) {
      if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	veError(MODULE, "failed to create socket: %s", strerror(errno));
	return -1;
      }
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = INADDR_ANY;
      server.sin_port = 0;
      if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
	veError(MODULE, "failed to bind socket: %s", strerror(errno));
	return -1;
      }
      if (listen(fd,32) < 0) {
	veError(MODULE, "cannot listen on socket: %s", strerror(errno));
	return -1;
      }
      reconnect_fd = fd;
    }
    /* now build the reconnect string */
    i = sizeof(server);
    if (getsockname(reconnect_fd,(struct sockaddr *)&server,&i) < 0) {
      veError(MODULE, "cannot determine name of socket: %s", strerror(errno));
      return -1;
    }
    if (base)
      sprintf(base,"tcp %s %d",host,ntohs(server.sin_port));
    return reconnect_fd;
  } else if (strcmp(proto,"udp") == 0) {
    /* make a return UDP port for each client */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      veError(MODULE, "failed to create socket: %s", strerror(errno));
      return -1;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
      veError(MODULE, "failed to bind socket: %s", strerror(errno));
      return -1;
    }
    /* now build the reconnect string */
    i = sizeof(server);
    if (getsockname(fd,(struct sockaddr *)&server,&i) < 0) {
      veError(MODULE, "cannot determine name of socket: %s", strerror(errno));
      return -1;
    }
    if (base)
      sprintf(base,"udp %s %d",host,ntohs(server.sin_port));
    return fd;
  } else {
    veError(MODULE, "init_reconnect: unsupported protocol %s", proto);
    return -1;
  }
}

static int reconnect_slave(char *proto, int id, int fd, char *key, char *check) {
  int l;
  char keyr[MAX_KEY_BUF];

  if (strcmp(proto,"tcp") == 0) {
    int nfd;
    struct sockaddr_in inaddr;
    int alen;
    VE_DEBUG(2,("waiting for connection from slave %d",id));
    while (((nfd = accept(reconnect_fd,(struct sockaddr *)&inaddr,&alen)) < 0) &&
	   errno == EINTR)
      ;
    if (nfd < 0)
      veFatalError(MODULE,"reconnect on slave %d failed - accept - %s",
		   id, strerror(errno));
    VE_DEBUG(2,("connection received for slave %d - verifying connection",id));
    fd = nfd;
    l = strlen(key)+1;
    errno = 0;
    if (forceread(fd,keyr,l) != l)
      veFatalError(MODULE,"reconnect on slave %d failed - read failed (%s)", id, strerror(errno));
    if (memcmp(keyr,key,l) != 0)
      veFatalError(MODULE,"reconnect on slave %d failed - bogus key - expected '%s' got '%s'",id,key,keyr);
    VE_DEBUG(3,("reconnect key is okay - sending check"));

    /* okay - we got the right key, now send the check */
    l = strlen(check)+1;
    errno = 0;
    if (write(fd,check,l) != l)
      veFatalError(MODULE,"reconnect failed to send check: %s",strerror(errno));
  } else if (strcmp(proto,"udp") == 0) {
    struct sockaddr_in inaddr;
    int alen;
    VE_DEBUG(2,("udp proto - no connection needed for slave %d",id));

    /* use recvfrom() first time to get an address to connect to */
    alen = sizeof(inaddr);
    l = strlen(key)+1;
    errno = 0;
    if (recvfrom(fd,keyr,l,0,(struct sockaddr *)&inaddr,&alen) != l)
      veFatalError(MODULE,"reconnect on slave %d failed - recvfrom failed (%s)",strerror(errno));
    /* connect this socket for future use... */
    if (connect(fd,(struct sockaddr *)&inaddr,alen) < 0)
      veFatalError(MODULE,"reconnect on slave %d failed - failed to connect UDP socket (%s)",strerror(errno));

    if (memcmp(keyr,key,l) != 0)
      veFatalError(MODULE,"reconnect on slave %d failed - bogus key - expected '%s' got '%s'",id,key,keyr);

    /* okay - we got the right key, now send the check */
    l = strlen(check)+1;
    errno = 0;
    if (write(fd,check,l) != l)
      veFatalError(MODULE,"reconnect failed to send check: %s",strerror(errno));
  } else {
    veError(MODULE,"reconnect_slave: unknown protocol %s",proto);
    return -1;
  }

  return fd;
}



/* This function does an rsh call, but keeps the wd (and other bits and
   pieces) consistent between this machine and the remote one.  It does
   this by constructing a shell script on the other side and then using
   that shell script to execute the real program.  The arguments still
   get interpreted by the shell once more than may be intuitive from the
   user's point-of-view.  Unfortunately, it takes 3 rsh calls to do so.
   (Although a little more hacking could eliminate one of them.)  It should
   clean up after itself as well.
*/
#define wrstring(f,z) write(f,z,strlen(z))
static int rsh_exec(char *host, int argc, char **argv) {
  char **nargv;
  int k;
  static char exec_path[256],str[2048];
  pid_t chld,p;
  int fd[2];
  char *rsh = find_rsh();

  /* setup the exec script on the other side */
  sprintf(exec_path,"/tmp/ve_mp_exec.%d",(int)getpid());

  VE_DEBUG(4,("rsh_exec on %s",host));

  pipe(fd);
  if ((chld = fork()) == 0) {
    close(0);
    close(1);
    dup2(fd[0],0);
    close(fd[0]);
    close(fd[1]);
    VE_DEBUG(4,("execing: %s %s %s",rsh,rsh,host));
    execlp(rsh,rsh,host,"cat",">",exec_path,NULL);
    exit(-1);
  }
  if (chld < 0) {
    veError(MODULE, "failed while setting up rsh_exec script");
    return -1;
  }
  VE_DEBUG(4,("creating rsh exec script on remote host"));
  close(fd[0]);
  wrstring(fd[1],"#!/bin/sh\n");

  sprintf(str,"cd \"%s\"\n",veMP_OSGetWD());
  wrstring(fd[1],str);
  sprintf(str,"rm -f %s\n",exec_path);
  wrstring(fd[1],str);
  {
    /* export any potentially useful environment variables */
    char *x, *envs[] = {
      "VEROOT", "VEENV", "LD_LIBRARY_PATH", "DISPLAY", 
      "__GL_SYNC_TO_VBLANK", "__GL_FSAA_MODE", "__GL_DEFAULT_LOG_ANISO",
      NULL
    };
    int k;
    for(k = 0; envs[k]; k++) {
      if ((x = getenv(envs[k]))) {
	wrstring(fd[1],envs[k]);
	wrstring(fd[1],"=");
	wrstring(fd[1],x);
	wrstring(fd[1]," ; export ");
	wrstring(fd[1],envs[k]);
	wrstring(fd[1],"\n");
      }
    }
    sprintf(str,"VEDEBUG=%d ; export VEDEBUG\n",veGetDebugLevel());
    wrstring(fd[1],str);
  }
  sprintf(str,"prog=\"$1\"\nshift\n");
  wrstring(fd[1],str);
  sprintf(str,"exec \"$prog\" \"$@\"\n");
  wrstring(fd[1],str);
  close(fd[1]);
  while ((p = waitpid(chld,NULL,0)) < 0)
    ;

  if ((chld = fork()) == 0) {
    close(0);
    close(1);
    execlp(rsh,rsh,host,"chmod","700",exec_path,NULL);
    exit(-1);
  }
  if (chld < 0) {
    veError(MODULE, "failed while setting permission on rsh_exec script");
    return -1;
  }
  while ((p = waitpid(chld,NULL,0)) != chld && p > 0)
    ;

  VE_DEBUG(4,("rsh exec script created - starting real child"));
  nargv = malloc(sizeof(char)*(argc+4));
  assert(nargv != NULL);
  nargv[0] = rsh;
  nargv[1] = host;
  nargv[2] = exec_path;
  for(k = 0; k < argc; k++)
    nargv[k+3] = argv[k];
  nargv[k+3] = NULL;
  execvp(nargv[0],nargv);
  return -1;
}

/* could grow this dynamically but realistically, the number is likely
   to be well under this limit */
#define MAX_SLAVES 256
static pid_t slave_pids[MAX_SLAVES];

int veMP_OSSetWD(char *dir) {
  return chdir(dir);
}

static int open_ip(char *proto, char *host, char *portstr) {
  int fd;
  struct hostent *hp;
  struct sockaddr_in addr;
  int port;

  assert(proto != NULL);
  assert(host != NULL);
  assert(portstr != NULL);
  VE_DEBUG(4,("open_ip(%s,%s,%s)",proto,host,portstr));
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

#define DELIM " \r\t\n"
int veMP_OSReconnect(int tag, void *data, int dlen, int *recv_h,
		     int *send_h) {
  /* Received a "reconnect" message from the server - attempt to 
     connect via the given data */
  char *dstr = data; /* treat data as a NULL-terminated string */
  char *proto;
  int l;
  VE_DEBUG(2,("[%2x] OSReconnect(%s)",veMPId(),dstr));
  if (!(proto = strtok(dstr,DELIM))) {
    veError(MODULE, "invalid reconnect request: %s",dstr);
    return -1;
  }

  if (strcmp(proto,"tcp") == 0 || strcmp(proto,"udp") == 0) {
    /* open a new TCP socket to the given host and port */
    char *host, *port, *key, *check;
    char checkbuf[MAX_CHECK_BUF];
    int fd;
    if (!(host = strtok(NULL,DELIM))) {
      veError(MODULE, "missing host in %s reconnect request",proto);
      return -1;
    }
    if (!(port = strtok(NULL,DELIM))) {
      veError(MODULE, "missing port in %s reconnect request",proto);
      return -1;
    }
    if (!(key = strtok(NULL,DELIM))) {
      veError(MODULE, "missing key in %s reconnect request",proto);
      return -1;
    }
    if (!(check = strtok(NULL,DELIM))) {
      veError(MODULE, "missing check in %s reconnect request",proto);
      return -1;
    }
    VE_DEBUG(4,("reconnect: connecting to (%s,%s)",host,port));
    if ((fd = open_ip(proto,host,port)) < 0)
      return -1;
    /* okay we've connected - send our key */
    l = strlen(key)+1;
    VE_DEBUG(4,("reconnect: sending key (%d bytes)",l));
    if (write(fd,key,l) != l) {
      veError(MODULE, "failed to write key on reconnect: %s",
	      strerror(errno));
      return -1;
    }
    /* look for our check value */
    l = strlen(check)+1;
    VE_DEBUG(4,("reconnect: receiving check (%d bytes)",l));
    if (forceread(fd,checkbuf,l) != l) {
      veError(MODULE, "failed to read check on reconnect: %s",
	      strerror(errno));
      return -1;
    }
    VE_DEBUG(4,("reconnect: comparing check"));
    if (memcmp(checkbuf,check,l)) {
      veError(MODULE, "check does not match on reconnect");
      return -1;
    }
    VE_DEBUG(4,("reconnect: finished"));
    /* okay - check has succeeded - we now expect further communication
       on this port */
    if (recv_h)
      *recv_h = fd;
    if (send_h)
      *send_h = fd;
    return 0;
  } else {
    veError(MODULE, "unsupported reconnect type: %s", proto);
    return -1;
  }
}

int veMP_OSSlaveInit(int *argc, char **argv, int *recv_h, int *send_h) {
  /* Assume that we got here via rsh or some other mechanism, such that
     we receive on stdin (fd = 0), and write on stdout (fd = 1).
     
     Now the clever bit - we cannot be assured that the application or 
     other bits of code won't try to write on stdout, so we'll hide it
     from them. */
  int send_fd;
  send_fd = dup(1); /* make a copy of the existing stdout */
  close(1);  /* close fd 1 */
  dup2(2,1); /* make the new fd 1 point to stderr (fd 2) */
  /* now, no other piece of code can muck us up by writing to fd 1 */
  if (recv_h)
    *recv_h = 0; /* data comes in on stdin */
  if (send_h)
    *send_h = send_fd; /* data goes out on our copy of the original stdout */
  return 0;
}

int veMP_OSSlavePrepare(int id, char *proto, char *node, char *process,
			int *recv_h, int *send_h) {
  int isrsh;

  VE_DEBUG(4,("preparing slave process"));

  assert(recv_h != NULL && send_h != NULL);
  
  if (!proto)
    proto = default_proto();

  isrsh = (node && (strcmp(node,"auto") != 0));

  if (isrsh) {
    /* negotiate better connection than rsh default */
    /* note that we have filled in the fd's temporarily in the pointers
       so we should be able to send a message now */
    char base_str[256], reconnect_str[256], key[MAX_KEY_BUF], 
      check[MAX_CHECK_BUF];
    int sfd,nfd;
    VE_DEBUG(2,("reconnecting to slave %d proto %s",id,proto));
    if ((sfd = init_reconnect(*recv_h,*send_h,proto,base_str)) < 0)
      return -1;
    mkkey(key,check);
    sprintf(reconnect_str,"%s %s %s",base_str,key,check);
    VE_DEBUG(2,("reconnect string is '%s'",reconnect_str));
    veMPIntPush(id,VE_MPMSG_RECONNECT,0,reconnect_str,strlen(reconnect_str)+1);

    if ((nfd = reconnect_slave(proto,id,sfd,key,check)) < 0)
      return -1;

    /* okay - we're alright */
    /* don't close the old pipe - if it's an rsh pipe, then we could
       kill off the remote program accidentally... */
    *recv_h = nfd;
    *send_h = nfd;
  }

  return 0;
}

int veMP_OSSlaveSpawn(int id, char *proto, char *node, char *process,
		      int argc, char **argv, 
		      int *recv_h, int *send_h) {
  /* create a new slave on the given node */
  /* sigh - if only pipes were bidirectional everywhere... */
  int sendp[2], recvp[2]; /* 0 = rd, 1 = wr */
  int k, isrsh;
  pid_t p;

  VE_DEBUG(4,("spawning slave"));

  if (!proto)
    proto = default_proto();

  if (pipe(sendp) != 0 || pipe(recvp) != 0)
    veFatalError(MODULE,"failed to create pipes: %s",strerror(errno));
  
  if ((!node || strcmp(node,"auto") == 0) &&
      (!process || strcmp(process,"auto") == 0)) {
    /* special case - this node, this process - i.e. don't fork */
    VE_DEBUG(4,("creating internal slave"));
    veMPSetSlaveProp(atoi(argv[2]),sendp[0],recvp[1]);
    if (recv_h)
      *recv_h = recvp[0];
    if (send_h)
      *send_h = sendp[1];
    return 0;
  }

  VE_DEBUG(4,("starting slave process"));

  isrsh = (node && (strcmp(node,"auto") != 0));

  /* if we get here, we need a new process (remote or local) */
  if ((p = fork()) == 0) {
    /* child */
    /* setup pipes as our stdin/stdout */
    VE_DEBUG(4,("in slave child"));
    close(sendp[1]);
    close(recvp[0]);
    close(0);
    close(1);
    dup2(sendp[0],0);
    dup2(recvp[1],1);
    close(sendp[0]);
    close(recvp[1]);
    /* now try to fork off other process */
    if (!isrsh) {
      /* local host - just exec */
      VE_DEBUG(4,("starting local slave"));
      execvp(argv[0],argv);
      veFatalError(MODULE,"failed to exec %s: %s",argv[0],strerror(errno));
    } else {
      /* remote host - try to rsh there */
      VE_DEBUG(4,("starting remote slave"));
      rsh_exec(node,argc,argv);
      veFatalError(MODULE,"failed to rsh exec %s: %s",argv[0],strerror(errno));
    }
    abort(); /* should never get here... */
  }
  
  if (p < 0)
    veFatalError(MODULE,"failed to fork: %s",strerror(errno));

  close(sendp[0]);
  close(recvp[1]);
  if (recv_h)
    *recv_h = recvp[0];
  if (send_h)
    *send_h = sendp[1];

  for(k = 0; k < MAX_SLAVES; k++)
    if (slave_pids[k] == 0) {
      slave_pids[k] = p;
      break;
    }
  if (k >= MAX_SLAVES)
    veFatalError(MODULE,"too many slaves - max = %d",MAX_SLAVES);

  return 0;
}

/* I expect that most implementations will use a stream with the following
   header, but not necessarily - hence the hiding it in the OS implementation
*/
/* We assume that all nodes are running the same architecture... */
typedef struct vemp_msgheader {
  int serial; /* serial number for this message (not fragment) */
  int fragid;
  int fragcnt; /* 0 --> no fragmentation */
  int fraglen;
  int src;
  int msg;
  int tag;
  int dlen;
} VeMPMsgHeader;

int veMP_OSSend(int h, int msg, int tag, void *data, int dlen) {
  static int serial = 1;
  VeMPMsgHeader hdr;
  memset(&hdr,0,sizeof(hdr));
  hdr.serial = serial++;
  hdr.src = veMPId();
  hdr.msg = msg;
  hdr.tag = tag;
  hdr.dlen = dlen;

  VE_DEBUG(4,("OSSend(%d,%d,%d,%d) [%d bytes]",h,hdr.serial,msg,tag,dlen));

  /* lock the actual sending to avoid getting interleaving on packets */
  if (dlen <= MAX_PAYLOAD) {
    /* send as a single packet */
    struct iovec iov[2];
    VE_DEBUG(4,("OSSend - sending as one packet"));
    hdr.fraglen = dlen;
    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = data;
    iov[1].iov_len = dlen;
    if (writev(h,iov,2) != iov[0].iov_len+iov[1].iov_len)
      veFatalError(MODULE,"failed to write packet: %s",strerror(errno));
  } else {
    /* send as multiple packets */
    struct iovec iov[2];
    int k;
    hdr.fragcnt = dlen / MAX_PAYLOAD;
    if (dlen % MAX_PAYLOAD)
      hdr.fragcnt++;
    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof(hdr);

    VE_DEBUG(4,("OSSend - sending as %d fragments",hdr.fragcnt));

    for(k = 0; k < hdr.fragcnt; k++) {
      iov[1].iov_base = ((char *)data)+k*MAX_PAYLOAD;
      iov[1].iov_len = dlen-k*MAX_PAYLOAD;
      if (iov[1].iov_len > MAX_PAYLOAD)
	iov[1].iov_len = MAX_PAYLOAD;
      hdr.fragid = k;
      hdr.fraglen = iov[1].iov_len;
      VE_DEBUG(4,("OSSend - sending fragment %d",k));
      if (writev(h,iov,2) != iov[0].iov_len+iov[1].iov_len)
	veFatalError(MODULE,"failed to write packet: %s",strerror(errno));
    }
  }
  return 0;
}

static int recv_msg(int h, VeMPMsgHeader *hdr, void **data) {
  int k;
  char *buf;

  VE_DEBUG(4,("recv_msg - trying to read header"));
  if ((k = forceread(h,hdr,sizeof(VeMPMsgHeader))) != sizeof(VeMPMsgHeader)) {
    veError(MODULE,"failed to read header: %s (k = %d)",strerror(errno),k);
    return -1;
  }
  VE_DEBUG(4,("recv_msg - header read - %d (%d/%d %d) (dlen=%d)",
	      hdr->serial, hdr->fragid, hdr->fragcnt, hdr->fraglen, 
	      hdr->dlen));
  /* fragment or header? */
  if (hdr->fraglen > MAX_PAYLOAD)
    veFatalError(MODULE,"recv_msg - payload in header is too large dlen=%d max=%d",
		 hdr->fraglen, MAX_PAYLOAD);
  if (hdr->fraglen <= 0) {
    if (data)
      *data = NULL;
  } else {
    buf = malloc(hdr->fraglen);
    assert(buf != NULL);
    if (forceread(h,buf,hdr->fraglen) != hdr->fraglen) {
      veError(MODULE,"failed to read message body: %s",strerror(errno));
      return -1;
    }
    if (data)
      *data = buf;
    else
      free(buf);
  }
  VE_DEBUG(4,("recv_msg - read payload"));
  return 0;
}

int veMP_OSRecv(int h, int *msg, int *tag, void **data, int *dlen) {
  VeMPMsgHeader hdr;
  int k;
  char *buf, *dbuf;
  int *frags;
  int fragcnt, len;
  int serial;

  VE_DEBUG(4,("OSRecv - trying to read initial message"));
  buf = NULL;
  if (recv_msg(h,&hdr,(void **)&buf)) {
    veError(MODULE,"OSRecv - failed to read message: %s",strerror(errno));
    return -1;
  }

  /* deal with simple non-fragmented case */
  if (hdr.fragcnt == 0) {
    VE_DEBUG(4,("OSRecv - reading coherent message"));
    if (hdr.fraglen != hdr.dlen) {
      veError(MODULE,"OSRecv - invalid message (no fragments, but fraglen=%d and dlen=%d",
	      hdr.fraglen,hdr.dlen);
      if (buf)
	free(buf);
      return -1;
    }
    if (msg)
      *msg = hdr.msg;
    if (tag)
      *tag = hdr.tag;
    if (dlen)
      *dlen = hdr.dlen;
    if (data)
      *data = buf;
    else if (buf)
      free(buf);
    VE_DEBUG(4,("OSRecv - coherent complete"));
    return 0;
  }

  VE_DEBUG(4,("OSRecv - reading fragmented message: %d fragments (%d bytes)",
	      hdr.fragcnt, hdr.dlen));
  serial = hdr.serial;
  fragcnt = hdr.fragcnt;
  len = hdr.dlen;
  if (fragcnt <= 0) {
    veError(MODULE,"OSRecv - invalid fragcnt in message: %d",
	    fragcnt);
    if (buf)
      free(buf);
    return -1;
  }
  frags = calloc(fragcnt,sizeof(int));
  assert(frags != NULL);
  for(k = 0; k < fragcnt; k++)
    frags[k] = 0;
  dbuf = malloc(len);
  assert(dbuf != NULL);

  /* process packets until we have all fragments */
  do {
    if (hdr.serial != serial) {
      veError(MODULE,"OSRecv - incomplete message (missing fragments)");
      goto os_recv_err;
    }
    if (hdr.fragid < 0 || hdr.fragid >= fragcnt) {
      veError(MODULE,"OSRecv - invalid fragment id %d must be (0..%d)",
	      hdr.fragid, fragcnt-1);
      goto os_recv_err;
    }
    if (frags[hdr.fragid]) {
      veError(MODULE,"OSRecv - duplicate fragment %d of %d",
	      hdr.fragid, fragcnt);
      goto os_recv_err;
    }
    if ((hdr.fragid*MAX_PAYLOAD+hdr.fraglen) > len) {
      veError(MODULE,"OSRecv - fragment is too big for dlen");
      goto os_recv_err;
    }
    if (hdr.fraglen > 0)
      memcpy(dbuf+hdr.fragid*MAX_PAYLOAD,buf,hdr.fraglen);
    frags[hdr.fragid] = 1;
    if (buf) {
      free(buf);
      buf = NULL;
    }
    /* check to see if we've finished */
    for(k = 0; k < fragcnt; k++)
      if (!frags[k])
	break;
    if (k == fragcnt) {
      /* complete message */
      VE_DEBUG(4,("OSRecv - complete message from fragments"));
      if (msg)
	*msg = hdr.msg;
      if (tag)
	*tag = hdr.tag;
      if (dlen)
	*dlen = len;
      if (data)
	*data = dbuf;
      else if (dbuf)
	free(dbuf);
      free(frags);
      return 0;
    }
  } while (recv_msg(h,&hdr,(void **)&buf) == 0);
  
  veError(MODULE,"OSRecv - ran out of fragments");

 os_recv_err:
  if (buf)    
    free(buf);
  if (dbuf)   
    free(dbuf);
  if (frags)  
    free(frags);
  return -1;
}

int veMP_OSClose(int h) {
  return bufclose(h);
}

void veMP_OSCleanup(void) {
  int k;
  for(k = 0; k < MAX_SLAVES; k++)
    if (slave_pids[k] != 0)
      kill(slave_pids[k],SIGTERM);
}

char *veMP_OSGetWD(void) {
  static char *wd = NULL;
  if (!wd)
    wd = getcwd(NULL,2048);
  return wd;
}
