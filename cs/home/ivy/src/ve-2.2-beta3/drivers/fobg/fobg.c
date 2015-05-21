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
#include "fobg.h"

#define MODULE "driver:fobg"

#define DEFAULT_RANGE         0.9144
#define DEFAULT_POS_EPSILON   0.01
#define DEFAULT_ANG_EPSILON   0.01
#define DEFAULT_FOB_SPEED     "9600"
#define ANG_CONV  (180.0/32768.0)
#define QUAT_CONV (1.0/32768.0)

typedef struct vei_flock_of_birds_group {
  int fd;
  char *line;
  int speed;
  int birds;	     /* # of birds in group */
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
} VeiFlockOfBirdsGroup;

static int sync_bird(VeiFlockOfBirdsGroup *b);
static int read_bird_data(int fd, short *wds, int n);
static int read_bird_words(int fd, short *wds, int n, int latest);

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
  case 115200:  res = B115200; break;
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

static VeiFlockOfBirdsGroup *open_serial(char *name, VeDeviceInstance *i) {
  struct termios t;
  VeiFlockOfBirdsGroup *b;
  char *c;

  b = calloc(1,sizeof(VeiFlockOfBirdsGroup));
  assert(b != NULL);

  if (c = veDeviceInstOption(i,"line"))
    b->line = veDupString(c);
  else {
    veError(MODULE,"serial source not specified in fobg input definition");
    return NULL;
  }

  if (c = veDeviceInstOption(i,"birds"))
    b->birds = atoi(c);

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
  sync_bird(b);

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

static int sync_bird(VeiFlockOfBirdsGroup *b) {
  short wds[6];
  int tries = SYNC_ATTEMPTS;
  int i;
  unsigned char buf[3];

  VE_DEBUG(2,("fobg_sync: synchronizing bird"));

  buf[0]= BIRD1;
  buf[1]= 'V';
  buf[2]= 'B';
  write(b->fd,buf,3);   /* force bird into point mode */
  tcdrain(b->fd); /* wait until that command has been written */
  read_bird_words(b->fd,wds,3,0);
  tcflush(b->fd,TCIOFLUSH); /* discard anything pending in either direction */

  while(vePeekFd(b->fd) > 0 && tries > 0) {
    VE_DEBUG(2,("fobg_sync: retrying sync"));
    buf[0]= BIRD1;
    buf[1]= 'B';
    write(b->fd,buf,2); /* force bird into point mode */
    tcdrain(b->fd); /* wait until that command has been written */
    read_bird_words(b->fd,wds,3,0);
    tcflush(b->fd,TCIOFLUSH); /* discard anything pending in either direction */
    tries--;
    usleep(100000);
  }

  VE_DEBUG(2,("fobg_sync: bird believed to be synched"));

  /* now double-check by sending a command and seeing if it starts off
     right */
  buf[0]= BIRD1;
  buf[1]= 'V';
  buf[2]= 'B';
  write(b->fd,buf,3); /* ask for one position */
  /* consume anything else waiting */
  if ((i = read_bird_words(b->fd,wds,3,0)) != 3) {
    if (i < 0) perror("sync_bird");
    else fprintf(stderr, "sync_bird: short read, expected 3 words got %d\n", i);
    return -1;
  }
  tcflush(b->fd,TCIOFLUSH);
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
  VeiFlockOfBirdsGroup *b = (VeiFlockOfBirdsGroup *)(d->instance->idata);

  obuf[0] = EXAMINE_VALUE;

  /* printf("Flock of Birds tracker (%s)\n", d->name); */

  /* get system model */
  obuf[1] = BIRD1;
  obuf[2] = SYSTEM_MODEL_ID;
  send_bird_data(b->fd,obuf,3);
  memset(ibuf,0,11);
  if (forceread(b->fd,ibuf,10) != 10)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  /* printf("Model: %s\n", ibuf); */

  /* get PROM revision */
  obuf[1] = BIRD1;
  obuf[2] = SOFTWARE_REV;
  send_bird_data(b->fd,obuf,3);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  /* printf("PROM Revision: %d.%d\n", (int)(wds[0]>>8)&0xff, 
	 (wds[0]&0xff)); */

  /* get Crystal speed */
  obuf[1] = BIRD1;
  obuf[2] = CRYSTAL_SPEED;
  send_bird_data(b->fd,obuf,3);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  /* printf("Crystal speed: %d MHz\n", (int)(wds[0])); */

  /* check filters */
  obuf[1] = 3;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  /* printf("AC NARROW notch filter is %s\n", wds[0]&0x04 ? "on" : "off" ); */
  /* printf("AC WIDE notch filter is %s\n", wds[0]&0x02 ? "on" : "off");
  printf("DC filter is %s\n", wds[0]&0x01 ? "on" : "off");
*/
  /* measurement rate */
  obuf[1] = 7;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  /* printf("Bird measurement rate: %g Hz\n",
	 (*(unsigned short *)(&wds[0]))/256.0); */
  

  /* get bird status */
  obuf[1] = 0;
  send_bird_data(b->fd,obuf,2);
  if (read_bird_data(b->fd,wds,1) != 1)
    veFatalError(MODULE, "failed to read response from bird %s (%s)",
		 d->name, strerror(errno));
  /* printf("Bird is %s\n", (wds[0] & 0x8000) ? "master" : "slave");
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
*/
}

/* send birds cmd ask for data */
static void fobg_point(VeiFlockOfBirdsGroup *b)
{
  unsigned char obuf[2];
 
  /* get a data block */
  obuf[0] = BIRD1;
  obuf[1] = POINT;
  send_bird_data(b->fd,obuf,2);
  tcdrain(b->fd); // waiting for data to arrive
}

static void *fobg_thread(void *v) {
  /* More datatypes, must have more... */
  VeDevice *d = (VeDevice *)v;
  VeiFlockOfBirdsGroup *b = (VeiFlockOfBirdsGroup *)(d->instance->idata);
  int i,j,num,in=8;
  unsigned char obuf[10];
  short	wds[b->birds * in];
  VeVector3 old_p[b->birds], p[b->birds];
  VeQuat old_q[b->birds], q[b->birds];
  VeFrame f, frame;
  long tm;
  int init, do_call = 0;
  long elapsed = 0;
  int nupdates = 0;
  int ncbacks = 0;
  long cback_elapsed = 0;

  VE_DEBUG(1,("fobg_thread starting"));

#if 0
  /* override with user profile settings, if any */
#endif /* 0 */


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

    /* get a data block , only send cmd to the Master 
     * Later, we will add choice to get get different data according 
     * to initialization
     */
   fobg_point(b);

  /* data format:
   *   buf[ 0] = lsb, buf[ 1] = msb	p[0]	(x)	(bird1)
   *   buf[ 2] = lsb, buf[ 3] = msb	p[1]	(y)	(bird1)
   *   buf[ 4] = lsb, buf[ 5] = msb	p[2]	(z)	(bird1)
   *   buf[ 6] = lsb, buf[ 7] = msb	q[0]	(q0)	(bird1)
   *   buf[ 8] = lsb, buf[ 9] = msb	q[1]	(q1)	(bird1)
   *   buf[10] = lsb, buf[11] = msb	q[2]	(q2)	(bird1)
   *   buf[12] = lsb, buf[13] = msb	q[3] 	(q3)	(bird1)
   *   buf[14] = lsb, buf[15] = msb	but|adr		(bird1)
   *
   *   buf[16] = lsb, buf[17] = msb	p[0]	(x)	(bird2)
   *   buf[18] = lsb, buf[19] = msb	p[1]	(y)	(bird2)
   *   buf[20] = lsb, buf[21] = msb	p[2]	(z)	(bird2)
   *   buf[22] = lsb, buf[23] = msb	q[0]	(q0)	(bird2)
   *   buf[24] = lsb, buf[25] = msb	q[1]	(q1)	(bird2)
   *   buf[26] = lsb, buf[27] = msb	q[2]	(q2)	(bird2)
   *   buf[28] = lsb, buf[29] = msb	q[3] 	(q3)	(bird2)
   *   buf[30] = lsb, buf[31] = msb	but|adr		(bird2)
   *
   * NOTE: if in GROUP mode, the bird returns, for each bird, an extra byte
   *       following each bird's data block which denotes the birds's address
   */

    num = b->birds * in;
    if ((i=read_bird_words(b->fd,wds,num,0)) != num )
      veFatalError(MODULE, "bad read from tracker - %s", strerror(errno));
   /*
    if (!(i = read_bird_words(b->fd,wds,num,0)))
      fobg_point(b);
    */   
    tm = veClock(); /* this is a weak estimate */

    for (i = 0; i < b->birds; i++) {
	/* rearrange the first 12 words of data
	 * (3 pos words followed by 9 matrix words) 
	 */
	for (j=0; j<3; j++) {
	   p[i].data[j] = wds[i*in + j] * b->range_conv;
	}
	for (j=0; j<3; j++) {
	   q[i].data[j] = wds[i*in + j + 4] * QUAT_CONV;
	}
	q[i].data[3] = -wds[i*in+3]*QUAT_CONV;
#if 0
        printf("FOB %d: Pos (%3.2f %3.2f %3.2f) ", i,p[i].data[0],p[i].data[1],p[i].data[2]);
	printf(", Qua (%4.2f %4.2f %4.2f %4.2f) \n", q[i].data[0],q[i].data[1],q[i].data[2],q[i].data[3]);
#endif
    }//endfor

    do_call = 0;
    if (!init) {
      init = 1;
      do_call = 1;
    } else {
      /* don't report anything until we get a significant variation in the
	 data (as defined by the epsilon values - this is done very 
	 intelligently right now (there must be the full value of the variation
	 in at least one axis) but it is a cheap and quick check */
      for(i=0; i < b->birds; i++) {
         for(j = 0; j < 3 && !do_call; j++)
	    if (fabs(p[i].data[j] - old_p[i].data[j]) > b->p_eps)
	        do_call = 1;
         for(j = 0; j < 3 && !do_call; j++)
	    if (fabs(q[i].data[j] - old_q[i].data[j]) > b->q_eps)
	        do_call = 1;
      }
    }//endif

    if (do_call) {
      veFrameIdentity(&frame);
      frame.loc = p[0];
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
      } //endif
      ncbacks++;

      {
	VeDeviceEvent *ve;
	VeDeviceE_Vector *evec;
	char s[6];

	veLockFrame();

	/* insert events (pos, quat) of Flock of Birds to ve devices 
	 */
	for (i=0; i<b->birds; i++) {
	  /* add Master's pos event to Fob device */
	  sprintf(s,"%s%d","pos",i);
	  ve = veDeviceEventCreate(VE_ELEM_VECTOR,3);
	  /* update elements */
	  ve->device = veDupString(d->name);
	  ve->elem = veDupString(s);
	  evec = (VeDeviceE_Vector *)(ve->content);
	  for(j = 0; j < 3; j++)
	    evec->value[j] = p[i].data[j];
	  ve->timestamp = tm;
	  veDeviceInsertEvent(ve);

	  /* add quat event to Fob device */
	  /* right now there is no correction of angles - this is wrong! */
	  sprintf(s,"%s%d","quat",i);
	  ve = veDeviceEventCreate(VE_ELEM_VECTOR,4);
	  ve->device = veDupString(d->name);
	  ve->elem = veDupString(s);
	  evec = (VeDeviceE_Vector *)(ve->content);
	  for(j = 0; j < 4; j++)
	    evec->value[j] = q[i].data[j];
	  ve->timestamp = tm;
	  veDeviceInsertEvent(ve);
	}//endfor

	veUnlockFrame();
	
	/* only update old values when we actually have something to report
	   to avoid "stealthy creeping" (i.e. several successive updates
	   being lost because they all fall under the radar) */
        for (i=0; i<b->birds; i++)
        {
	   old_p[i] = p[i];
	   old_q[i] = q[i];
        }

        tcflush(b->fd,TCIOFLUSH); // discard anything pending in either direction
      }
    }
  }//endwhile
}

static VeDevice *new_fobg_driver(VeDeviceDriver *driver, VeDeviceDesc *desc,
				VeStrMap override) {
  VeDevice *d;
  VeiFlockOfBirdsGroup *fobg;
  VeDeviceInstance *i;
  char *c,s[56];
  int j;

  VE_DEBUG(1,("fobg: creating new flock-of-birds-in-group-mode device"));

  i = veDeviceInstanceInit(driver,NULL,desc,override);
  
  if (!(fobg = open_serial(desc->name,i))) {
    veError(MODULE, "could not open serial port for %s", desc->name);
    return NULL;
  }
  i->idata = fobg;

  VE_DEBUG(1,("fobg: serial port opened"));

  fobg->update_rate_stat = veNewStatistic(MODULE, "update_rate", "Hz");
  fobg->update_rate_stat->type = VE_STAT_FLOAT;
  fobg->update_rate_stat->data = &fobg->update_rate;
  fobg->update_rate = 0.0;
  veAddStatistic(fobg->update_rate_stat);
  fobg->cback_rate_stat = veNewStatistic(MODULE, "cback_rate", "Hz");
  fobg->cback_rate_stat->type = VE_STAT_FLOAT;
  fobg->cback_rate_stat->data = &fobg->cback_rate;
  fobg->cback_rate = 0.0;
  veAddStatistic(fobg->cback_rate_stat);

  d = veDeviceCreate(desc->name);
  d->instance = i;
  d->model = veDeviceCreateModel();

  /* Add elements to ve device. 
   * pos0, quat0 are Master's elements, pos1, quat1 are Slave1's elements, ... etc.  
   * (Note: in the final version, I may want to use quaternions as more stable
   * data-type and then filters to generate angles or matrices as desired) 
   */
  for (j=0; j<fobg->birds; j++) {
     sprintf(s,"%s%d %s","pos",j,"vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
     veDeviceAddElemSpec(d->model,s);
     sprintf(s,"%s%d %s","quat",j,"vector 4 {0.0 0.0} {0.0 0.0} {0.0 0.0} {0.0 0.0}");
     veDeviceAddElemSpec(d->model,s);
  }

  /* initialize device from options now, before starting thread 

  if (!(c = veDeviceInstOption(i,"quiet")) || atoi(c) == 0) {
    VE_DEBUG(1,("fobg: attempting to retrieve bird info"));
    show_bird_info(d);
  }
  */
  veFrameIdentity(&fobg->recvf);

  /* initialize group mode  
   * later, we will add choice to initialize non group mode
   */
  {
    unsigned char obuf[10], parm1, parm2;
    int okay;
    
    /* (auto)configure birds for number of active birds */
    obuf[0] = BIRD1;
    obuf[1] = CHANGE_VALUE;
    obuf[2] = FBB_AUTOCONFIG;
    obuf[3] = fobg->birds;
    send_bird_data(fobg->fd,obuf,4);
    usleep(200000);

    /* start the birds flying */
    obuf[0] = BIRD1;
    obuf[1] = RUN;
    send_bird_data(fobg->fd,obuf,1);
    usleep(200000);

    /* initialize the bird, but don't request data yet */
    if (c = veDeviceInstOption(i,"hemisphere")) {
      okay = 1;
      if (strcmp(c,"forward") == 0) {
	parm1 = 0x0; parm2 = 0x0;
      } else if (strcmp(c,"aft") == 0) {
	parm1 = 0x0; parm2 = 0x1;
      } else if (strcmp(c,"upper") == 0) {
	parm1 = 0xc; parm2 = 0x1;
      } else if (strcmp(c,"lower") == 0) {
	parm1 = 0xc; parm2 = 0x0;
      } else if (strcmp(c,"left") == 0) {
	parm1 = 0x6; parm2 = 0x1;
      } else if (strcmp(c,"right") == 0) {
	parm1 = 0x6; parm2 = 0x0;
      } else {
	veError(MODULE,"%s: invalid hemisphere option: %s",d->name,c);
	okay = 0;
      }
      if (okay) {
	for (j=0; j<fobg->birds; j++) {
	  obuf[0] = BIRD1+j;
	  obuf[1] = HEMISPHERE;
	  obuf[2] = parm1;
	  obuf[3] = parm2;
	  send_bird_data(fobg->fd,obuf,4);
    	  usleep(100000);
	}//endfor
      }//endif
    }//endif
   
    /* set the button mode to return button status(append it to data block) */ 
    for (j=0; j<fobg->birds; j++) {
       obuf[0] = BIRD1+j;
       obuf[1] = BUTTON_MODE;
       obuf[2] = 1;
       send_bird_data(fobg->fd,obuf,3);
       usleep(100000);
    }//endfor

    /* set the data report mode. */ 
    for (j=0; j<fobg->birds; j++) {
      obuf[0] = BIRD1 +j;
      obuf[1] = POSITION_QUATERNION;
      send_bird_data(fobg->fd,obuf,2);
      usleep(100000);
    }//endfor

    /* set the Flock of Birds to group mode */ 
    obuf[0] = BIRD1;
    obuf[1] = CHANGE_VALUE;
    obuf[2] = GROUP_MODE;
    obuf[3] = 1;
    send_bird_data(fobg->fd,obuf,4);
    usleep(100000);

    fobg->range = DEFAULT_RANGE;
    fobg->range_conv = fobg->range/32768.0;
    if (c = veDeviceInstOption(i,"range")) {
      fobg->range = atof(c);
      fobg->range_conv = fobg->range/32768.0;
    }
    if (c = veDeviceInstOption(i,"pos_epsilon"))
      fobg->p_eps = atof(c);
    if (c = veDeviceInstOption(i,"quat_epsilon"))
      fobg->q_eps = atof(c);

    if (c = veDeviceInstOption(i,"sudden_change_lock")) { 
    	for (j=0; j<fobg->birds; j++) {
      	  obuf[0] = BIRD1 +j;
      	  obuf[1] = EXAMINE_VALUE;
      	  obuf[2] = SUDDEN_OUTPUT;
      	  obuf[3] = (atoi(c) ? 1 : 0);
      	  send_bird_data(fobg->fd,obuf,4);
          usleep(100000);
	}//endfor
    }//endif

    /* recvf - the relation between the receiver's true frame and its
       desired frame */
    if (c = veDeviceInstOption(i,"recv_loc"))
      parseVector3(d->name,c,&fobg->recvf.loc);
    if (c = veDeviceInstOption(i,"recv_dir"))
      parseVector3(d->name,c,&fobg->recvf.dir);
    if (c = veDeviceInstOption(i,"recv_up"))
      parseVector3(d->name,c,&fobg->recvf.up);

  }

  /* Build VeDeviceModel for this device */
  veThreadInitDelayed(fobg_thread,d,0,0);
  return d;
}

/* These functions are only looked up by code that tries to calibrate the
   tracker and thus would want to modify the frames of the driver on the
   fly. */
VeFrame *veiFobGetFrameGroup(VeDevice *d) {
  VeiFlockOfBirdsGroup *b = (VeiFlockOfBirdsGroup *)(d->instance->idata);
  return &b->frame;
}

VeFrame *veiFobGetRecvFrameGroup(VeDevice *d) {
  VeiFlockOfBirdsGroup *b = (VeiFlockOfBirdsGroup *)(d->instance->idata);
  return &b->recvf;
}

static VeDeviceDriver fobg_drv = {
  "fobg", new_fobg_driver
};

/* for dynamically loaded driver */
void VE_DRIVER_INIT(fobg) (void) {
  veDeviceAddDriver(&fobg_drv);
}

void VE_DRIVER_PROBE(fobg) (void *phdl) {
  veDriverProvide(phdl,"input","fobg");
}
