/*
 * Unix-specific support
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>      /* for gethostbyname() */
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h> /* for socket()/connect() */
#include <netinet/in.h> /* for htonl(),etc. */

#include <nid.h>

int nidOsOpenSocket(char *host, int port) {
  struct hostent *hp;
  struct sockaddr_in addr;
  int fd;

  if ((hp = gethostbyname(host)) == NULL) {
    _nidLogMsg("OpenSocket: gethostbyname(%s): host not found", host);
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  memcpy(&(addr.sin_addr.s_addr),hp->h_addr,hp->h_length);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    _nidLogMsg("OpenSocket: socket: %s", strerror(errno));
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    _nidLogMsg("OpenSocket: connect(%s:%d): %s", host, port, strerror(errno));
    close(fd);
    return -1;
  }

  return fd;
}

int nidOsCloseSocket(int fd) {
  close(fd);
  return 0;
}

int nidOsWritePacket(int tport, int fd, char *data, int len) {
  if (tport == NID_TPORT_UDP) {
    /* two stage write */
    if (write(fd,data,sizeof(nid_uint32)) != sizeof(nid_uint32))
      return 1;
    if (write(fd,data+sizeof(nid_uint32),len-sizeof(nid_uint32)) !=
	len-sizeof(nid_uint32))
      return 1;
    return 0;
  } else {
    if (write(fd,data,len) == len)
      return 0;
    else
      return 1;
  }
}

char *nidOsNextPacket(int tport, int fd, int *len, int wait) {
  int sofar,k,i;
  nid_uint32 sz;
  char *b;

  if (!wait)
    fcntl(fd, F_SETFL, fcntl(fd,F_GETFL)|O_NONBLOCK);
  errno = 0;
  i = read(fd,&sz,sizeof(sz));
  if (!wait)
    fcntl(fd, F_SETFL, fcntl(fd,F_GETFL)&~O_NONBLOCK);
  
  if (i != sizeof(sz)) {
    if (!wait && i <= 0)
      return NULL;
    _nidLogMsg("NextPacket: read(packet_size): %s",strerror(errno));
    return NULL;
  }
  sz = nidOs_n2h_uint32(sz);
  b = malloc(sz + sizeof(sz));
  assert(b != NULL);
  *(nid_uint32 *)b = nidOs_h2n_uint32(sz);
  sofar = 0;
  while (sofar < sz) {
    if ((k = read(fd,b+sizeof(sz)+sofar,sz-sofar)) <= 0) {
      /* error or EOF */
      free(b);
      _nidLogMsg("NextPacket: read(packet) - expected %d, got %d - %s",
		 sz-sofar,k,strerror(errno));
      return NULL;
    }
    sofar += k;
  }
  return b;
}

static int open_ip(char *proto, char *host, char *portstr) {
  int fd;
  struct hostent *hp;
  struct sockaddr_in addr;
  int port;

  assert(proto != NULL);
  assert(host != NULL);
  assert(portstr != NULL);
  if ((port = atoi(portstr)) == 0)
    return -1;
  if ((hp = gethostbyname(host)) == NULL)
    return -1;
  memset(&addr,0,sizeof(addr));
  memcpy(&(addr.sin_addr.s_addr),hp->h_addr,hp->h_length);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (strcmp(proto,"tcp") == 0 || strcmp(proto,"udp") == 0) {
    if ((fd = socket(AF_INET, 
		     (strcmp(proto,"tcp") == 0 ? SOCK_STREAM : SOCK_DGRAM), 
		     0)) < 0)
      return -1;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      close(fd);
      return -1;
    }
  } else {
    return -1;
  }
  return fd;
}

/* do the actual work of receiving a reconnect */
#define DELIM " \r\t\n"
int nidOSHandleReconnect(char *tport) {
  char *proto, *host, *port;
  int fd;
  if (!(proto = strtok(tport,DELIM)))
    return -1;
  if ((strcmp(proto,"tcp") != 0) && (strcmp(proto,"udp") != 0))
    return -1;
  if (!(host = strtok(NULL,DELIM)) || 
      !(port = strtok(NULL,DELIM)))
    return -1;
  if ((fd = open_ip(proto,host,port)) >= 0) {
    NidHandshake h;
    h.protocol_major = NID_PROTO_MAJOR;
    h.protocol_minor = NID_PROTO_MINOR;
    if (write(fd,&h,sizeof(h)) != sizeof(h)) {
      close(fd);
      return -1;
    }
  }
  return fd;
}

/* do the actual work of requesting a new reconnection */
int nidOSReconnect(int conn, int fd, char *tport) {
  static int tcp_reconnect_fd = -1;
  char connect_str[256];
  char host[80];

  if (strcmp(tport,"tcp") == 0) {
    /* create new local port to accept reconnect on if it does not
       already exist */
    struct sockaddr_in server;
    int i;
    if (tcp_reconnect_fd < 0) {
      int nfd;
      if ((nfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1;
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = INADDR_ANY;
      server.sin_port = 0;
      if (bind(nfd, (struct sockaddr *)&server, sizeof(server)) < 0)
	return -1;
      if (listen(nfd,32) < 0)
	return -1;
      tcp_reconnect_fd = nfd;
    }
    i = sizeof(server);
    if (getsockname(tcp_reconnect_fd,(struct sockaddr *)&server,&i) < 0)
      return -1;
    if (gethostname(host,80) < 0)
      return -1;
    sprintf(connect_str,"tcp %s %d",host,ntohs(server.sin_port));
    if (nidSetEventSink(conn,connect_str))
      return -1;
    /* the server ACK'ed our SET_EVENT_SINK request, so it should have
       connected to the TCP port we gave it */
    {
      int nfd;
      struct sockaddr_in inaddr;
      int alen;
      alen = sizeof(struct sockaddr_in);
      while (((nfd = accept(tcp_reconnect_fd,
			    (struct sockaddr *)&inaddr,&alen)) < 0) &&
	     errno == EINTR)
	;
      if (nfd < 0)
	return -1; /* where did our connection go? */
      /* there should be a handshake payload from the server */
      {
	NidHandshake h;
	if (read(nfd,&h,sizeof(h)) != sizeof(h))
	  return -1;
      }
      return nfd; /* this is our new connection */
    }
  } else if (strcmp(tport,"udp") == 0) {
    struct sockaddr_in inaddr;
    int alen, nfd;
    alen = sizeof(inaddr);

    if ((nfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      return -1;
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = INADDR_ANY;
    inaddr.sin_port = 0;
    if (bind(nfd, (struct sockaddr *)&inaddr, sizeof(inaddr)) < 0)
      return -1;
    alen = sizeof(inaddr);
    if (getsockname(nfd,(struct sockaddr *)&inaddr,&alen) < 0)
      return -1;
    if (gethostname(host,80) < 0)
      return -1;
    sprintf(connect_str,"udp %s %d",host,ntohs(inaddr.sin_port));
    if (nidSetEventSink(conn,connect_str)) {
      _nidLogMsg("nidSetEventSink failed");
      return -1;
    }
    /* there should be a handshake payload from the server */
    {
      NidHandshake h;
      if (recvfrom(nfd,&h,sizeof(h),0,(struct sockaddr *)&inaddr,&alen) != sizeof(h))
	return -1;
      if (connect(nfd,(struct sockaddr *)&inaddr,alen) < 0)
	return -1;
      return nfd;
    }
  } else
    return -1; /* I don't know how to handle this */
}

nid_uint32 nidOs_h2n_uint32(nid_uint32 i) { return htonl(i); }
nid_uint32 nidOs_n2h_uint32(nid_uint32 i) { return ntohl(i); }
nid_int32 nidOs_n2h_int32(nid_int32 i) { 
  nid_uint32 x = ntohl(*(nid_uint32 *)&i);
  return *(nid_int32 *)&x;
}
nid_int32 nidOs_h2n_int32(nid_int32 i) {
  return nidOs_n2h_int32(i);
}
nid_real nidOs_n2h_real(nid_real r) {
  nid_uint32 x = htonl(*(nid_uint32 *)&r);
  return *(nid_real *)&x;
}
nid_real nidOs_h2n_real(nid_real r) {
  return nidOs_n2h_real(r);
}
