/* Once this driver works at the basic level, support for multiple stations
   will be required */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>

#include <ve.h>

#define MODULE "driver:fastrak"
/* maximum station id we support */
#define MAXSTATION 12

#define DEFAULT_FAS_SPEED "9600"

#define STD_FORMAT "O%s,2,11,1\r\n"
#define TMSTAMP_FORMAT "O%s,21,2,0,11,1\r\n"

/* x y z q0 q1 q2 q3 */
#define STD_SCAN "%f %f %f %f %f %f %f"
#define STD_SCAN_CNT 7

typedef struct vei_fastrak {
  int fd;
  char *line;
  int speed;
  int numStations;         /* the number of stations we use */
  int station[MAXSTATION]; /* store which station we use    */
  char *model;             /* sets model specific info      */
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
  int native_tmstamp;      /* use native timestamp of tracker */
  long tmstamp_zero;       /* time equiv. to tracker's zero time */
} VeiFastrak;

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

static VeiFastrak *open_serial(char *name, VeDeviceInstance *i) {
  struct termios t;
  VeiFastrak *f;
  char *c;

  f = calloc(1,sizeof(VeiFastrak));
  assert(f != NULL);

  if (c = veDeviceInstOption(i,"line"))
    f->line = veDupString(c);
  else {
    veError(MODULE,"serial source not specified in fob input definition");
    return NULL;
  }

  if (c = veDeviceInstOption(i,"raw"))
    f->raw = atoi(c);

  if (c = veDeviceInstOption(i,"speed"))
    f->speed = str_to_bps(c);
  else
    f->speed = str_to_bps(DEFAULT_FAS_SPEED);
  if (f->speed < 0)
    return NULL;

  f->fd = open(f->line, O_RDWR|O_NOCTTY);
  if (f->fd < 0) {
    veError(MODULE, "could not open serial line %s: %s",
	    f->line, strerror(errno));
    return NULL;
  }

  /* setup serial line, terminal mumbo-jumbo */
  if (tcgetattr(f->fd,&t)) {
    veError(MODULE, "could not get serial attributes for %s: %s",
	    f->line, strerror(errno));
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

  /* set baud rates */
  cfsetispeed(&t,f->speed);
  cfsetospeed(&t,f->speed);

  /* setup flow control */
  /* Hmm... RTS/CTS is not standard...great... */
  if (c = veDeviceInstOption(i, "flow")) {
    if (strcmp(c, "xonxoff") == 0) {
      t.c_iflag |= (IXON|IXOFF);
#if defined(__sgi)
      t.c_cflag &= ~CNEW_RTSCTS;
#elif defined(__sun) || defined(__linux)
      t.c_cflag &= ~CRTSCTS;
#endif
    } else if (strcmp(c, "rtscts") == 0) {
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

  if (tcsetattr(f->fd,TCSAFLUSH,&t)) {
    veError(MODULE, "could not set serial attributes for %s: %s",
	    f->line, strerror(errno));
    return NULL;
  }

  /* build NULL frame */
  veFrameIdentity(&f->frame);
  veFrameIdentity(&f->recvf);

  if (c = veDeviceInstOption(i, "loc"))
    parseVector3(name,c,&f->frame.loc);
  if (c = veDeviceInstOption(i, "dir"))
    parseVector3(name,c,&f->frame.dir);
  if (c = veDeviceInstOption(i, "up"))
    parseVector3(name,c,&f->frame.up);
  /* recvf - the relation between the receiver's true frame and its
     desired frame */
  if (c = veDeviceInstOption(i,"recv_loc"))
    parseVector3(name,c,&f->recvf.loc);
  if (c = veDeviceInstOption(i,"recv_dir"))
    parseVector3(name,c,&f->recvf.dir);
  if (c = veDeviceInstOption(i,"recv_up"))
    parseVector3(name,c,&f->recvf.up);
  return f;
}

/* We are going to read exactly number bytes from buffer,
 * otherwise we wait here
 */
static int readline(int fd, char *buf, int n) {
  int m,sofar = 0;
  *buf = '\0';
  /*  n--;  leave space for '\0' at end */
  while (n > 0) {
    /* waiting here until get something */
    while ((m = read(fd,buf,n)) <= 0);
    *(buf+m) = '\0'; /* terminate it again */
    sofar += m;
    /* not yet...keep reading */
    buf += m;
    n -= m;
  }
  return sofar;
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

static void send_fastrak(int fd, char *cmd) {
  VE_DEBUG(2,("writing to fasttrak: '%s'",cmd));
  write(fd,cmd,strlen(cmd));
}

/* A useful debugging/info function - queries a bunch of
   values from the tracker */
static void show_fastrak_info(VeDevice *d) {
  VeiFastrak *f = (VeiFastrak *)(d->instance->idata);
  unsigned char buf[256];
  char cmd[256];

  printf("Fastrak tracker (%s)\n", d->name);

  /* query configuration data */
  send_fastrak(f->fd,"S");
  if (forceread(f->fd,buf,37) != 37) {
    veError(MODULE,"failed to read response from fastrak %s (%s)",
	    d->name, strerror(errno));
    return;
  }
  if (buf[0] != '2' || buf[1] != ' ' || buf[2] != 'X') {
    veError(MODULE,"invalid record header: '%c%c%c'",buf[0],buf[1],buf[2]);
    return;
  }
  printf("Config: ");
  fwrite(buf+3,1,32,stdout);
  putchar('\n');
  
  sprintf(cmd,"A%d",f->station);
  send_fastrak(f->fd,cmd);
  if (forceread(f->fd,buf,68) != 68) {
    veError(MODULE,"failed to read response from fastrat %s (%s)",
	    d->name, strerror(errno));
    return;
  }
}

static char *station_to_code(int station) {
  static char stations[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static char st[2];
  if (station < 0 || station >= (sizeof(stations)-1))
    return "0"; /* a good default? */
  st[0] = stations[station];
  st[1] = '\0';
  return st;
}

static void init_fastrak(VeDevice *d) {
  VeiFastrak *f = (VeiFastrak *)(d->instance->idata);
  unsigned char buf[256];
  char cmd[256];
  int i, j, status;
  char *refang, *led;

  refang = veDeviceInstOption(d->instance,"refang");
  led = veDeviceInstOption(d->instance,"led");

  VE_DEBUG(1,("resetting fastrak"));
  /* reset the fastrak to power-up state */
  send_fastrak(f->fd,"\030");

  /* reset reference frame to the default */
  VE_DEBUG(1,("resetting reference frame"));
  for (j = 0; j < f->numStations; j++)
    {
      sprintf(cmd,"R%d\r\n",station_to_code(f->station[j]));
      send_fastrak(f->fd,cmd);
    }

  VE_DEBUG(1,("setting units to centimetres"));
  /* request measurements in centimetres */
  send_fastrak(f->fd,"u");
  
  VE_DEBUG(1,("requesting ASCII records"));
  /* request ascii records */
  send_fastrak(f->fd,"F");

  VE_DEBUG(1,("disabling other stations, activating mine"));
  /* disable other stations except ours */
  for(i = 1; i <= MAXSTATION; i++) 
    {
      status = 0;

      /* find out which station we have used */
      for (j = 0; j< f->numStations; j++)
	if (i == f->station[j])
          status = 1;

      sprintf(cmd,"l%s,%d\r\n",station_to_code(i),status);
      send_fastrak(f->fd,cmd);
      if (status && refang) {
	VE_DEBUG(1,("setting reference angles on station %d",i));
	sprintf(cmd,"G%s,%s\r\n",station_to_code(i),refang);
	send_fastrak(f->fd,cmd);
	sprintf(cmd,"MB%s\r\n",station_to_code(i));
	send_fastrak(f->fd,cmd);
	sprintf(cmd,f->native_tmstamp ? TMSTAMP_FORMAT : STD_FORMAT,
		station_to_code(i));
	send_fastrak(f->fd,cmd);
      }
    }

  if (f->native_tmstamp) {
    VE_DEBUG(1,("resetting timestamp clock"));
    send_fastrak(f->fd,"MT\r\n"); /* set milliseconds */
    send_fastrak(f->fd,"MZ\r\n"); /* zero clock */
    f->tmstamp_zero = veClock(); /* record time */
  }

  /* led control - only really valid on InterSense devices */
  if (led) {
    int led_v = atoi(led) ? 1 : 0;
    VE_DEBUG(1,("turning InterSense leds %s",led_v ? "on" : "off"));
    sprintf(cmd,"ML%d\r\n",led_v);
    send_fastrak(f->fd,cmd);
  }
}

static void resync_input(FILE *f) {
  int c;
  while ((c = getc(f)) != EOF && c != '\n')
    ;
}

static int line_is_clean(FILE *f) {
  int c;
  while ((c = getc(f)) != EOF && c != '\n' && isspace(c))
    ;
  if (c == '\n')
    return 1; /* it's clean */
  return 0; /* it's not clean */
}

static void *fastrak_thread(void *v) {
  VeDevice *d = (VeDevice *)v;
  VeiFastrak *f = (VeiFastrak *)(d->instance->idata);
  VeVector3 old_p[MAXSTATION], p[MAXSTATION];
  VeQuat old_q[MAXSTATION], q[MAXSTATION];
  int init[MAXSTATION];
  VeFrame frame,fr;
  int nupdates = 0;
  int ncbacks = 0;
  long cback_elapsed = 0, elapsed = 0;
  int do_call = 0, k, c, stnum;
  long tm;
  FILE *inf;

  /* run in continuous mode, but be prepared for bogus records */
  inf = fdopen(f->fd,"r");
  send_fastrak(f->fd,"C");

  while (1) {
    /* this statistic is likely to be less meaningless now...
       (need to deal with multiple stations...) */
    if ((nupdates % 50) == 0) {
      if (nupdates > 0) {
        elapsed = veClock() - elapsed;
        f->update_rate = nupdates/(float)(elapsed/1000.0);
        veUpdateStatistic(f->update_rate_stat);
      }
      nupdates = 0;
      elapsed = veClock();
    }
    nupdates++;


    if (feof(inf))
      veFatalError(MODULE,"eof from tracker");

    /* parse an input line, being careful to watch out for mangled lines */
    VE_DEBUG(2,("fastrak: parsing line"));

    if (fscanf(inf,"%d",&stnum) != 1) {
      veError(MODULE,"mangled input - no station number");
      resync_input(inf);
      continue;
    }

    if (stnum < 0 || stnum >= MAXSTATION) {
      veError(MODULE,"mangled input - invalid station number: %d\n",
	      stnum);
      resync_input(inf);
      continue;
    }

    /* NOTE: check later - do we actually care about this station? */

    if (f->native_tmstamp) {
      if (fscanf(inf,"%ld",&tm) != 1) {
	veError(MODULE,"mangled input - no time-stamp");
	resync_input(inf);
	continue;
      }
      tm += f->tmstamp_zero; /* correct offset to VE's clock */
    } else {
      tm = veClock(); /* approximate tmstamp */
    }

    /* parse remaining data... */
    /* note quaternion ordering */
    if (fscanf(inf,STD_SCAN,
	       &(p[stnum].data[0]), &(p[stnum].data[1]), &(p[stnum].data[2]),
	       &(q[stnum].data[3]),
	       &(q[stnum].data[0]), &(q[stnum].data[1]), &(q[stnum].data[2]))
	!= STD_SCAN_CNT) {
      veError(MODULE,"mangled line - failed to read all data fields");
      resync_input(inf);
      continue;
    }

    /* is the line too long...? */
    if (!line_is_clean(inf)) {
      veError(MODULE,"mangled line - junk after actual data");
      resync_input(inf);
    }

    /* Okay - we have read a clean line... */
    
    /* convert position to metres (should be in centimetres) */
    p[stnum].data[0] *= 0.01;
    p[stnum].data[1] *= 0.01;
    p[stnum].data[2] *= 0.01;

    VE_DEBUG(2,("successfully parsed line"));

    do_call = 0;
    if (!init[stnum]) {
      init[stnum] = 1;
      do_call = 1;
    } else if (f->p_eps == 0.0 && f->q_eps == 0.0) {
      /* if epsilon is 0 for both values, always pass an
	 event through */
      do_call = 1;
    } else {
      /* don't report anything until we get a significant variation in the
	 data (as defined by the epsilon values - this is done very 
	 intelligently right now (there must be the full value of the variation
	 in at least one axis) but it is a cheap and quick check */
      for(k = 0; k < 3 && !do_call; k++)
	if (fabs(p[stnum].data[k] - old_p[stnum].data[k]) > f->p_eps)
	  do_call = 1;
      for(k = 0; k < 3 && !do_call; k++)
	if (fabs(q[stnum].data[k] - old_q[stnum].data[k]) > f->q_eps)
	  do_call = 1;
    }
    
    if (!do_call) {
      VE_DEBUG(2,("skipping call (no change?)"));
    } else {
      veFrameIdentity(&frame);
      frame.loc = p[stnum];
      veMapFrame(&f->frame,&frame,&fr);
      veMapFrame(&f->recvf,&fr,&frame);
      VE_DEBUG(1,("Tracker position in world: (%4.2f %4.2f %4.2f)",
		  frame.loc.data[0], frame.loc.data[1],
		  frame.loc.data[2]));

      if ((ncbacks % 50) == 0) {
	if (ncbacks > 0) {
	  cback_elapsed = veClock() - cback_elapsed;
	  f->cback_rate = ncbacks/(float)(cback_elapsed/1000.0);
	  veUpdateStatistic(f->cback_rate_stat);
	}
	ncbacks = 0;
	cback_elapsed = veClock();
      }
      ncbacks++;
    
      {
	VeDeviceEvent *ve;
	VeDeviceE_Vector *evec;
	char s[6];
      
	veLockFrame();

	ve = veDeviceEventCreate(VE_ELEM_VECTOR,3);
	/* update elements */
	sprintf(s, "%s%d", "pos",stnum);
	ve->device = veDupString(d->name);
	ve->elem = veDupString(s);
	evec = (VeDeviceE_Vector *)(ve->content);
	for(k = 0; k < 3; k++)
	  evec->value[k] = p[stnum].data[k];
	ve->timestamp = tm;
	veDeviceInsertEvent(ve);

	/* right now there is no correction of angles - this is wrong! */
	sprintf(s, "%s%d", "quat",stnum);
	ve = veDeviceEventCreate(VE_ELEM_VECTOR,4);
	ve->device = veDupString(d->name);
	ve->elem = veDupString(s);
	evec = (VeDeviceE_Vector *)(ve->content);
	for(k = 0; k < 4; k++)
	  evec->value[k] = q[stnum].data[k];
	ve->timestamp = tm;
	veDeviceInsertEvent(ve);
      }

      veUnlockFrame();
        
      /* only update old values when we actually have something to report
	 to avoid "stealthy creeping" (i.e. several successive updates
	 being lost because they all fall under the radar) */
      old_p[stnum] = p[stnum];
      old_q[stnum] = q[stnum];
    }

    /* tcflush(f->fd,TCIOFLUSH); */
  }
}

/* read station num from string,and convert to decimal 
 * return number of stations
 */
int getStationNum(char *c, int *n)
{
  int i,count=0;
  char s[3];

  if (!c || !*c)  /* s is NULL or empty */
    return 0;
  while (*c == ' ' || *c == '\t') 
    c++;  /* skip while space */
  while (*c != '\0')  /* read token */
    {  
      i = 0; 
      while (isdigit(*c))
	{  
	  s[i] = *c;
	  c++;
	  i++;
	}
      if (*c == ' ' || *c == '\t' || *c == '\0')
	{  
	  *n = atoi(s);
	  if ( *n < 0 || *n > MAXSTATION) 
	    {  
	      veError(MODULE,"invalid station: must be between 1 and %d",
		      MAXSTATION);
	      return 0;
	    }
	  n++;
	  count++;
	}
      while (*c == ' ' || *c == '\t') c++; /* skip while space again */
    }

  if ( *c == '\0')
    return count;
  return 0;
}

static VeDevice *new_fastrak_driver(VeDeviceDriver *driver,
				    VeDeviceDesc *desc,
				    VeStrMap override) {
  VeDevice *d;
  VeiFastrak *f;
  VeDeviceInstance *i;
  char *c, s[56];
  int j;

  VE_DEBUG(1,("fastrak: creating new fastrak device"));
  
  i = veDeviceInstanceInit(driver,NULL,desc,override);
  if (!(f = open_serial(desc->name,i))) {
    veError(MODULE, "could not open serial port for %s", desc->name);
    return NULL;
  }
  i->idata = f;

  if (c = veDeviceInstOption(i,"station")) 
    {
      if (!(f->numStations = getStationNum(c,f->station)))
	{  veError(MODULE, "fail to get station option");
	return NULL;  
	}
    } else
      {
	f->numStations = 1;
	f->station[0] = 1;
      }

  VE_DEBUG(1,("fastrak: serial port opened"));

  f->update_rate_stat = veNewStatistic(MODULE, "update_rate", "Hz");
  f->update_rate_stat->type = VE_STAT_FLOAT;
  f->update_rate_stat->data = &f->update_rate;
  f->update_rate = 0.0;
  veAddStatistic(f->update_rate_stat);
  f->cback_rate_stat = veNewStatistic(MODULE, "cback_rate", "Hz");
  f->cback_rate_stat->type = VE_STAT_FLOAT;
  f->cback_rate_stat->data = &f->cback_rate;
  f->cback_rate = 0.0;
  veAddStatistic(f->cback_rate_stat);

  d = veDeviceCreate(desc->name);
  d->instance = i;
  d->model = veDeviceCreateModel();
  /* Add elements to ve device */
  for (j = 0; j < f->numStations; j++)
    {
      sprintf(s, "%s%d %s","pos",f->station[j],"vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
      veDeviceAddElemSpec(d->model,s);
      sprintf(s, "%s%d %s","quat",f->station[j],"vector 4 {0.0 0.0} {0.0 0.0} {0.0 0.0} {0.0 0.0}");
      veDeviceAddElemSpec(d->model,s);
    }
  if ((c = veDeviceInstOption(i,"info")) && atoi(c)) {
    VE_DEBUG(1,("fastrak: attempting to retrieve tracker info"));
    show_fastrak_info(d);
  }
  
  if (c = veDeviceInstOption(i,"tmstamp"))
    f->native_tmstamp = 1;

  /* initialize it */
  init_fastrak(d);

  if (c = veDeviceInstOption(i,"pos_epsilon"))
    f->p_eps = atof(c);
  if (c = veDeviceInstOption(i,"quat_epsilon"))
    f->q_eps = atof(c);

  
  veThreadInitDelayed(fastrak_thread,d,0,0);
  return d;
}

static VeDeviceDriver fastrak_drv = {
  "fastrak", new_fastrak_driver
};

void VE_DRIVER_INIT(fastrakdrv) (void) {
  veDeviceAddDriver(&fastrak_drv);
}
