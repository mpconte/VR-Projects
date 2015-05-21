/* A trivial flock-of-birds controller */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>

#include <ve.h>

#define MODULE "driver:fob"

static int sync_bird(int fd);
static int read_bird_data(int fd, short *wds, int n);
static int read_bird_words(int fd, short *wds, int n, int latest);

#define DEFAULT_RANGE         0.9144
#define DEFAULT_POS_EPSILON   0.01
#define DEFAULT_ANG_EPSILON   0.01
#define DEFAULT_FOB_SPEED     "9600"
#define ANG_CONV  (180.0/32768.0)
#define QUAT_CONV (1.0/32768.0)

typedef struct vei_flock_of_birds {
  int fd;
  char *line;
  int speed;
  VeFrame frame;
  int raw;
  VeStatistic *update_rate_stat;
  VeStatistic *cback_rate_stat;
  float update_rate; /* update rate in Hz */
  float cback_rate;
  VeFrame recvf;
  float range;
  float range_conv;
  float p_eps, q_eps;
} VeiFlockOfBirds;

static int str_to_bps(char *c) {
  int b = atoi(c);
  int res = B0;
  switch(b) {
  case 300:     res = B300; break;
  case 1200:    res = B1200; break;
  case 2400:    res = B2400; break;
  case 9600:    res = B9600; break;
  case 19200:   res = B19200; break;
  case 38400:   res = B38400; break;
#ifdef B57600
  case 57600:   res = B57600; break;
#endif /* B57600 */
#ifdef B76800
  case 76800:   res = B76800; break;
#endif /* B76800 */
#ifdef B115200
  case 115200:  res = B115200; break;
#endif /* B115200 */
  default:
    veError(MODULE, "unsupported serial speed: %s", c);
    return -1;
  }
  return res;
}

static int parseVector3(char *name, char *str, VeVector3 *v) {
  if (sscanf(str,"%f %f %f", &(v->data[0]), 
	     &(v->data[1]), &(v->data[2])) != 3) {
    veError(MODULE, "bad option for input %s - should be vector", name);
    return -1;
  }
  return 0;
}

static VeiFlockOfBirds *open_serial(char *name, VeDeviceInstance *i) {
  struct termios t;
  VeiFlockOfBirds *b;
  char *c;

  b = calloc(1,sizeof(VeiFlockOfBirds));
  assert(b != NULL);

  if (c = veDeviceInstOption(i,"line"))
    b->line = veDupString(c);
  else {
    veError(MODULE,"serial source not specified in fob input definition");
    return NULL;
  }

  if (c = veDeviceInstOption(i,"raw"))
    b->raw = atoi(c);

  if (c = veDeviceInstOption(i,"speed"))
    b->speed = str_to_bps(c);
  else
    b->speed = str_to_bps(DEFAULT_FOB_SPEED);
  if (b->speed < 0)
    return NULL;

  b->fd = open(b->line, O_RDWR|O_NOCTTY);
  if (b->fd < 0) {
    veError(MODULE, "could not open serial line %s: %s",
	    b->line, strerror(errno));
    return NULL;
  }

  /* setup serial line, terminal mumbo-jumbo */
  if (tcgetattr(b->fd,&t)) {
    veError(MODULE, "could not get serial attributes for %s: %s",
	    b->line, strerror(errno));
    return NULL;
  }
  
  /* this is "non-portable" or "portability-hostile" but I can't figure
     out which of the many flags was mussing up data from the receiver,
     so we'll just trash them all.  This also seems to kill the 
     slow-startup bug */
  t.c_iflag = 0;
  t.c_oflag = 0;
  t.c_cflag = 0;
  t.c_lflag = 0;
  t.c_cflag |= (CLOCAL|CREAD|CS8);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;

  cfsetispeed(&t,b->speed);
  cfsetospeed(&t,b->speed);

  /* setup flow control */
  if (c = veDeviceInstOption(i, "flow")) {
    if (strcmp(c, "xonxoff") == 0) {
      t.c_iflag |= (IXON|IXOFF);
#if defined(__sgi)
      t.c_cflag &= ~CNEW_RTSCTS;
#elif defined(__sun) || defined(__linux)
      t.c_cflag &= ~CRTSCTS;
#endif
    } else if (strcmp(c, "rtscts") == 0) {
      /* Hmm... RTS/CTS is not standard...great... */
#if defined(__sgi)
      t.c_cflag |= CNEW_RTSCTS;
#elif defined(__sun) || defined(__linux)
      t.c_cflag |= CRTSCTS;
#else
      veError(MODULE, "RTS/CTS not supported on this platform");
      return NULL;
#endif
    }
  } else {
    t.c_iflag &= ~(IXON|IXOFF|IXANY);
#if defined(__sgi)
      t.c_cflag &= ~CNEW_RTSCTS;
#elif defined(__sun) || defined(__linux)
      t.c_cflag &= ~CRTSCTS;
#endif
  }

  if (tcsetattr(b->fd,TCSAFLUSH,&t)) {
    veError(MODULE, "could not set serial attributes for %s: %s",
	    b->line, strerror(errno));
    return NULL;
  }

  /* try to sync things up in case it is already running */
  sync_bird(b->fd);

  /* build NULL frame */
  veFrameIdentity(&b->frame);

  if (c = veDeviceInstOption(i, "loc"))
    parseVector3(name,c,&b->frame.loc);
  if (c = veDeviceInstOption(i, "dir"))
    parseVector3(name,c,&b->frame.dir);
  if (c = veDeviceInstOption(i, "up"))
    parseVector3(name,c,&b->frame.up);

  return b;
}

#define SYNC_ATTEMPTS 10

static int sync_bird(int fd) {
  short wds[6];
  int tries = SYNC_ATTEMPTS;
  int i;

  VE_DEBUG(2,("fob_sync: synchronizing bird"));

  write(fd,"VB",2); /* force bird into point mode */
  tcdrain(fd); /* wait until that command has been written */
  read_bird_words(fd,wds,3,0);
  tcflush(fd,TCIOFLUSH); /* discard anything pending in either direction */

  while(vePeekFd(fd) > 0 && tries > 0) {
    VE_DEBUG(2,("fob_sync: retrying sync"));
    write(fd,"B",1); /* force bird into point mode */
    tcdrain(fd); /* wait until that command has been written */
    read_bird_words(fd,wds,3,0);
    tcflush(fd,TCIOFLUSH); /* discard anything pending in either direction */
    tries--;
    usleep(100);
  }

  VE_DEBUG(2,("fob_sync: bird believed to be synched"));

  /* now double-check by sending a command and seeing if it starts off
     right */
  write(fd,"VB",2); /* ask for one position */
  /* consume anything else waiting */
  if ((i = read_bird_words(fd,wds,3,0)) != 3) {
    if (i < 0) perror("sync_bird");
    else fprintf(stderr, "sync_bird: short read, expected 3 words got %d\n", i);
    return -1;
  }
  tcflush(fd,TCIOFLUSH);
  return 0;
}

/* maximum number of words we can read at a shot */
#define MAXWORDS 4096
static int send_bird_data(int fd, unsigned char *data, int n) {
  int k;
  if (VE_ISDEBUG(4)) {
    int i;
    fprintf(stderr, "writing:");
    for(i = 0; i < n; i++)
      fprintf(stderr, " 0x%02x", data[i]);
    fprintf(stderr, "\n");
  }
  k = write(fd,data,n);
  if (VE_ISDEBUG(4)) {
    sleep(5);
  }
  return k;
}

static int forceread(int fd, unsigned char *buf, int n) {
  int m,sofar;

  sofar = 0;
  while(sofar < n) {
    if ((m = read(fd,&(buf[sofar]),n - sofar)) <= 0)
      return m;
    sofar += m;
  }
  return n;
}

/* beware - read_bird_data and read_bird_words are not the same... */
static int read_bird_data(int fd, short *wds, int n) {
  unsigned char buf[MAXWORDS*2];
  int i;

  if (n > MAXWORDS)
    veFatalError(MODULE, "tried to read too many words from bird: %d", n);

  if ((i = forceread(fd,buf,2*n)) <= 0)
    return i;

  /* return raw words */
  for(i = 0; i < n; i++)
    wds[i] = (short)(buf[2*i]&0xff)|((short)buf[2*i+1]<<8);

  return n;
}

/* like read_bird_data, except that the expected data is in position/etc.
   data format, so we do some extra work */
static int read_bird_words(int fd, short *wds, int n, int latest) {
  unsigned char buf[MAXWORDS*2];
  int i,j,m,sofar,avail;

  if (n > MAXWORDS)
    veFatalError(MODULE, "tried to read too many words from bird: %d", n);

  if (n <= 0)
    return 0; /* no work to do... */

  sofar = 0;
  
  if (!latest) {
    /* look for framing bit in response */
    while((m = read(fd,buf,1)) == 1 && !(buf[0] & 0x80))
      ;
    if (m != 1)
      return m; /* something buggered up... */
    sofar = 1; /* try to read the rest */
    while(sofar < 2*n) {
      if ((m = read(fd,&(buf[sofar]),n*2 - sofar)) <= 0)
	return m; /* error on read */
      sofar += m;
    }
    /* Who wants hacks?  We do! We do! */
    /* Convert screwy FOB data format into something sane */
    for(i = 0; i < n; i++)
      wds[i] = (((short)(buf[2*i]&0x7f)<<1)|((short)buf[2*i+1]<<8))<<1;
  } else {
    /* read all the data in the buffer and take the latest */
    while(1) {
      while ((avail = vePeekFd(fd)) == 0)
	veWaitForData(fd);
      if (avail+sofar > 2*MAXWORDS)
	veFatalError(MODULE, "input overflow - %d chars waiting", avail+sofar);
      if ((m = read(fd,buf,avail)) != avail)
	veFatalError(MODULE, "vePeekFd() lied - expected %d, got %d", avail, m);
      /* look for framing bit */
      for(i = m-1; i >= 0; i--)
	if ((buf[i] & 0x80) && (m-i-1 >= 2*n)) {
	  /* YAY! a framing bit with a full packet after it */
	  for(j = 0; j < n; j++, i += 2)
	    wds[j] = (((short)(buf[i]&0x7f)<<1)|((short)buf[i+1]<<8))<<1;
	  return n;
	}
      sofar += m; /* try again after an infinitessimal nap? */
      usleep(1);
    }
  }
  return n;
}

static void show_bird_info(VeDevice *d) {
  /* get various bits of info from the FOB and spit it out on stdout */
  unsigned char obuf[2], ibuf[11];
  short wds[10];
  VeiFlockOfBirds *b = (VeiFlockOfBirds *)(d->instance->idata);

  obuf[0] = 'O';

  printf("Flock of Birds tracker (%s)\n", d->name);

  /* get system model */
  obuf[1] = 15;
  send_bird_data(b->fd,obuf,2);
  memset(ibuf,0,11);
  if (forceread(b->fd,ibuf,10) != 10)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  printf("Model: %s\n", ibuf);

  /* get PROM revision */
  obuf[1] = 1;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  printf("PROM Revision: %d.%d\n", (int)(wds[0]>>8)&0xff, 
	 (wds[0]&0xff));

  /* get Crystal speed */
  obuf[1] = 2;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  printf("Crystal speed: %d MHz\n", (int)(wds[0]));

  /* check filters */
  obuf[1] = 3;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  printf("AC NARROW notch filter is %s\n", wds[0]&0x04 ? "on" : "off" );
  printf("AC WIDE notch filter is %s\n", wds[0]&0x02 ? "on" : "off");
  printf("DC filter is %s\n", wds[0]&0x01 ? "on" : "off");

  /* measurement rate */
  obuf[1] = 7;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  printf("Bird measurement rate: %g Hz\n",
	 (*(unsigned short *)(&wds[0]))/256.0);
  
  obuf[1] = 0;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  printf("Bird is %s\n", (wds[0] & 0x8000) ? "master" : "slave");
  printf("Bird has %sbeen initialized\n", (wds[0] & 0x4000) ? "" : "not ");
  if (wds[0] & 0x2000)
    printf("Bird has detected error\n");
  printf("Bird is %srunning\n", (wds[0] & 0x1000) ? "" : "not ");
  if (wds[0] & 0x0800)
    printf("Bird is in HOST SYNC mode\n");
  printf("Addressing mode is %s\n", (wds[0] & 0x0400) ? "expanded" : "normal");
  if (wds[0] & 0x0200)
    printf("Bird is in CRT SYNC mode\n");
  printf("Bird is in %s mode\n", (wds[0] & 0x0020) ? "sleep" : "run");
  switch((((unsigned)wds[0])>>1)&0x0f) {
  case 1: printf("POSITION outputs selected\n"); break;
  case 2: printf("ANGLE outputs selected\n"); break;
  case 3: printf("MATRIX outputs selected\n"); break;
  case 4: printf("POSITION/ANGLE outputs selected\n"); break;
  case 5: printf("POSITION/MATRIX outputs selected\n"); break;
  case 6: printf("Output is set to factory mode!\n"); break;
  case 7: printf("QUATERNION outputs selected\n"); break;
  case 8: printf("POSITION/QUATERNION outputs selected\n"); break;
  default:
    printf("Output state is invalid (= %d)\n", (((unsigned)wds[0])>>1)&0x0f);
  }
  printf("Bird is in %s mode\n", (wds[0] & 0x1) ? "stream" : "point");
}

/* size of a packet in words... */
#define PKTSIZE 7
static void *fob_thread(void *v) {
  /* More datatypes, must have more... */
  VeDevice *d = (VeDevice *)v;
  VeiFlockOfBirds *b = (VeiFlockOfBirds *)(d->instance->idata);
  unsigned char obuf[10];
  short wds[10];
  VeVector3 old_p, p;
  VeQuat old_q, q;
  VeFrame f, frame;
  int j, init = 0;
  long tm;
  int do_call = 0;
  long elapsed = 0;
  int nupdates = 0;
  int ncbacks = 0;
  long cback_elapsed = 0;

  VE_DEBUG(1,("fob_thread starting"));

#if 0
  /* override with user profile settings, if any */
#endif /* 0 */

  /* request data */
  obuf[0] = ']';  /* position/quaternion */
#ifdef USE_STREAM
  obuf[1] = '@';  /* stream mode */
  send_bird_data(b->fd,obuf,2);
#else
  send_bird_data(b->fd,obuf,1);
#endif /* USE_STREAM */

  while (1) {
    /* read data agressively - that is, consume all incoming records and only 
       consider the last one, since we are
       running in stream mode... */

    if ((nupdates % 50) == 0) {
      if (nupdates > 0) {
	elapsed = veClock() - elapsed;
	b->update_rate = nupdates/(float)(elapsed/1000.0);
	veUpdateStatistic(b->update_rate_stat);
      }
      nupdates = 0;
      elapsed = veClock();
    }
    nupdates++;

#ifdef USE_STREAM
    /* read at least one... */
    if (read_bird_words(b->fd,wds,PKTSIZE,!b->raw) != PKTSIZE) 
      veFatalError(MODULE, "bad read from tracker - %s", strerror(errno));
    tm = veClock(); /* since this last bit of data came from the tracker
			     very recently (within 1 update cycle) this isn't
			     a bad estimate */
#else
    obuf[0] = 'B';
    send_bird_data(b->fd,obuf,1);
    if (read_bird_words(b->fd,wds,PKTSIZE,0) != PKTSIZE)
      veFatalError(MODULE, "bad read from tracker - %s", strerror(errno));
    tm = veClock(); /* this is a weak estimate */
#endif

    for(j = 0; j < 3; j++)
      p.data[j] = wds[j]*b->range_conv;
    /* the bird's order for quaternions is different from VE's */
    for(j = 0; j < 3; j++)
      q.data[j] = wds[j+4]*QUAT_CONV;
    /* important:  the quaternion should represent the rotation required
       to get from the base to the current orientation - the FOB returns
       the opposite - the rotation to get from the view to the origin,
       so we flip the scalar of the quaternion */
    q.data[3] = -wds[3]*QUAT_CONV;

    VE_DEBUG(2,("FOB: Pos (%3.2f %3.2f %3.2f) Quat (%4.2f %4.2f %4.2f,%4.2f)",
		p.data[0],p.data[1],p.data[2],q.data[0],q.data[1],q.data[2],q.data[3]));

    do_call = 0;
    if (!init) {
      init = 1;
      do_call = 1;
    } else {
      /* don't report anything until we get a significant variation in the
	 data (as defined by the epsilon values - this is done very 
	 intelligently right now (there must be the full value of the variation
	 in at least one axis) but it is a cheap and quick check */
      for(j = 0; j < 3 && !do_call; j++)
	if (fabs(p.data[j] - old_p.data[j]) > b->p_eps)
	  do_call = 1;
      for(j = 0; j < 3 && !do_call; j++)
	if (fabs(q.data[j] - old_q.data[j]) > b->q_eps)
	  do_call = 1;
    }

    if (do_call) {
      veFrameIdentity(&frame);
      frame.loc = p;
      /* now map it into env co-ordinates */
      veMapFrame(&b->frame,&frame,&f);
      /* correct for any receiver orientation problems */
      veMapFrame(&b->recvf,&f,&frame);

      VE_DEBUG(1,("Tracker position in world: (%4.2f %4.2f %4.2f)",
		  frame.loc.data[0], frame.loc.data[1],
		  frame.loc.data[2]));

      if ((ncbacks % 50) == 0) {
	if (ncbacks > 0) {
	  cback_elapsed = veClock() - cback_elapsed;
	  b->cback_rate = ncbacks/(float)(cback_elapsed/1000.0);
	  veUpdateStatistic(b->cback_rate_stat);
	}
	ncbacks = 0;
	cback_elapsed = veClock();
      }
      ncbacks++;

      {
	VeDeviceEvent *ve;
	VeDeviceE_Vector *evec;

	veLockFrame();

	ve = veDeviceEventCreate(VE_ELEM_VECTOR,3);
	/* update elements */
	ve->device = veDupString(d->name);
	ve->elem = veDupString("pos");
	evec = (VeDeviceE_Vector *)(ve->content);
	for(j = 0; j < 3; j++)
	  evec->value[j] = frame.loc.data[j];
	ve->timestamp = tm;
	veDeviceInsertEvent(ve);

	/* right now there is no correction of angles - this is wrong! */
	ve = veDeviceEventCreate(VE_ELEM_VECTOR,4);
	ve->device = veDupString(d->name);
	ve->elem = veDupString("quat");
	evec = (VeDeviceE_Vector *)(ve->content);
	for(j = 0; j < 4; j++)
	  evec->value[j] = q.data[j];
	ve->timestamp = tm;
	veDeviceInsertEvent(ve);

	veUnlockFrame();
	
	/* only update old values when we actually have something to report
	   to avoid "stealthy creeping" (i.e. several successive updates
	   being lost because they all fall under the radar) */
	old_p = p;
	old_q = q;
      }
    }
  }
}

static VeDevice *new_fob_driver(VeDeviceDriver *driver, VeDeviceDesc *desc,
				VeStrMap override) {
  VeDevice *d;
  VeiFlockOfBirds *fob;
  VeDeviceInstance *i;
  char *c;

  VE_DEBUG(1,("fob: creating new flock-of-birds device"));

  i = veDeviceInstanceInit(driver,NULL,desc,override);
  
  if (!(fob = open_serial(desc->name,i))) {
    veError(MODULE, "could not open serial port for %s", desc->name);
    return NULL;
  }
  i->idata = fob;

  VE_DEBUG(1,("fob: serial port opened"));

  fob->update_rate_stat = veNewStatistic(MODULE, "update_rate", "Hz");
  fob->update_rate_stat->type = VE_STAT_FLOAT;
  fob->update_rate_stat->data = &fob->update_rate;
  fob->update_rate = 0.0;
  veAddStatistic(fob->update_rate_stat);
  fob->cback_rate_stat = veNewStatistic(MODULE, "cback_rate", "Hz");
  fob->cback_rate_stat->type = VE_STAT_FLOAT;
  fob->cback_rate_stat->data = &fob->cback_rate;
  fob->cback_rate = 0.0;
  veAddStatistic(fob->cback_rate_stat);

  d = veDeviceCreate(desc->name);
  d->instance = i;
  d->model = veDeviceCreateModel();
  veDeviceAddElemSpec(d->model,"pos vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
  /* Note: in the final version, I may want to use quaternions as more stable
     data-type and then filters to generate angles or matrices as desired */
  veDeviceAddElemSpec(d->model,"quat vector 4 {0.0 0.0} {0.0 0.0} {0.0 0.0} {0.0 0.0}");

  /* initialize device from options now, before starting thread */

  if (!(c = veDeviceInstOption(i,"quiet")) || atoi(c) == 0) {
    VE_DEBUG(1,("fob: attempting to retrieve bird info"));
    show_bird_info(d);
  }
  veFrameIdentity(&fob->recvf);

  {
    unsigned char obuf[10];
    int okay;
    /* initialize the bird, but don't request data yet */
    if (c = veDeviceInstOption(i,"hemisphere")) {
      obuf[0] = 'L';
      okay = 1;
      if (strcmp(c,"forward") == 0) {
	obuf[1] = 0x0; obuf[2] = 0x0;
      } else if (strcmp(c,"aft") == 0) {
	obuf[1] = 0x0; obuf[2] = 0x1;
      } else if (strcmp(c,"upper") == 0) {
	obuf[1] = 0xc; obuf[2] = 0x1;
      } else if (strcmp(c,"lower") == 0) {
	obuf[1] = 0xc; obuf[2] = 0x0;
      } else if (strcmp(c,"left") == 0) {
	obuf[1] = 0x6; obuf[2] = 0x1;
      } else if (strcmp(c,"right") == 0) {
	obuf[1] = 0x6; obuf[2] = 0x0;
      } else {
	veError(MODULE,"%s: invalid hemisphere option: %s",d->name,c);
	okay = 0;
      }
      if (okay)
	send_bird_data(fob->fd,obuf,3);
    }

    fob->range = DEFAULT_RANGE;
    fob->range_conv = fob->range/32768.0;
    if (c = veDeviceInstOption(i,"range")) {
      fob->range = atof(c);
      fob->range_conv = fob->range/32768.0;
    }
    if (c = veDeviceInstOption(i,"pos_epsilon"))
      fob->p_eps = atof(c);
    if (c = veDeviceInstOption(i,"quat_epsilon"))
      fob->q_eps = atof(c);
    if (c = veDeviceInstOption(i,"sudden_change_lock")) {
      obuf[0] = 'O';
      obuf[1] = 14;
      obuf[2] = (atoi(c) ? 1 : 0);
      send_bird_data(fob->fd,obuf,3);
    }
    /* recvf - the relation between the receiver's true frame and its
       desired frame */
    if (c = veDeviceInstOption(i,"recv_loc"))
      parseVector3(d->name,c,&fob->recvf.loc);
    if (c = veDeviceInstOption(i,"recv_dir"))
      parseVector3(d->name,c,&fob->recvf.dir);
    if (c = veDeviceInstOption(i,"recv_up"))
      parseVector3(d->name,c,&fob->recvf.up);
  }

  /* Build VeDeviceModel for this device */
  veThreadInitDelayed(fob_thread,d,0,0);
  return d;
}

/* These functions are only looked up by code that tries to calibrate the
   tracker and thus would want to modify the frames of the driver on the
   fly. */
VeFrame *veiFobGetFrame(VeDevice *d) {
  VeiFlockOfBirds *b = (VeiFlockOfBirds *)(d->instance->idata);
  return &b->frame;
}

VeFrame *veiFobGetRecvFrame(VeDevice *d) {
  VeiFlockOfBirds *b = (VeiFlockOfBirds *)(d->instance->idata);
  return &b->recvf;
}

static VeDeviceDriver fob_drv = {
  "fob", new_fob_driver
};

/* for dynamically loaded driver */
void VE_DRIVER_INIT(fobdrv) (void) {
  veDeviceAddDriver(&fob_drv);
}
