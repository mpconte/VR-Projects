/*
 * A rewrite of the old 'fastrak' driver that uses two threads
 * to maintain throughput from the input tracker and to reduce
 * the latency of events delivered to the application
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>

#include <ve.h>

#define MODULE "driver:fastrak"

/* maximum number of stations */
#define MAXSTATION 12

#define DEFAULT_FAS_SPEED "9600"

#define STD_FORMAT "O%d,2,11,1\r\n"
#define TMSTAMP_FORMAT "O%d,21,2,0,11,1\r\n"
#define TMSTAMP_WINDOW 60000

/* st x y z q0 q1 q2 q3 */
#define STD_SIZE 54
#define STD_SCAN "%d %f %f %f %f %f %f %f"
#define STD_SCAN_CNT 8
/* st tm x y z q0 q1 q2 q3 */
#define TMSTAMP_SIZE 69
#define TMSTAMP_SCAN "%d %ld %f %f %f %f %f %f %f"
#define TMSTAMP_SCAN_CNT 9


#define WHT " \r\t\n"

#define SQ(x) ((x)*(x))
#define PDIST2(x,y) (SQ((y).data[0]-(x).data[0])+SQ((y).data[1]-(x).data[1])+SQ((y).data[2]-(x).data[2]))
#define QDIST2(x,y) (SQ((y).data[0]-(x).data[0])+SQ((y).data[1]-(x).data[1])+SQ((y).data[2]-(x).data[2])+SQ((y).data[3]-(x).data[3]))

/* station data record */
typedef struct vei_ft_station {
  int active;         /* do we want data for this station? */
  float p_eps2, q_eps2; /* epsilon values - if 0.0 then no epsilon checking
			   is done (actually - square of epsilon values) */
  long tmstamp;   /* time-stamp for measurement */
  /* position data */
  VeVector3 p;
  VeQuat q;
  int init;       /* have we *ever* gotten data? */
  int updated;    /* is info updated? */
} VeiFTStation;

/* flow control modes */
#define FLOW_NONE  0
#define FLOW_SOFT  1
#define FLOW_HARD  2
 
typedef struct vei_ft {
  int fd; /* file-descriptor from which we are reading */
  /* serial line characteristics */
  char *line; 
  int speed;
  int flow;

  /* tracker configuration/data */
  VeThrMutex *mutex;
  VeThrCond *update; /* in the off-chance we need to wait for an
			 update */
  VeiFTStation data[MAXSTATION];
  int tmstamp; /* use native timestamps? */
  long tmstamp_zero; /* zero point for native timestamps */
  int stream;  /* 0 = polled mode, non-zero = streamed mode */

  /* statistics */
  VeStatistic *update_rate_stat;
  float update_rate;
  VeStatistic *cback_rate_stat;
  float cback_rate;

} VeiFT;


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

static void send_ft(int fd, char *cmd) {
  VE_DEBUG(2,("writing to fasttrak: '%s'",cmd));
  write(fd,cmd,strlen(cmd));
}

static VeiFT *init_ft(char *name, VeDeviceInstance *i) {
  VeiFT *f = NULL;
  char *c, cmd[80];
  float p_eps2 = 0.0, q_eps2 = 0.0;
  int k;

  VE_DEBUG(2,("%s: initializing tracker structure",name));

  /* parse options */
  f = calloc(1,sizeof(VeiFT));
  assert(f != NULL);
  f->fd = -1;
  
  if (c = veDeviceInstOption(i,"line"))
    f->line = veDupString(c);
  else {
    veError(MODULE,"%s: serial line not specified",name);
    goto cleanup;
  }
  
  f->speed = str_to_bps(DEFAULT_FAS_SPEED); /* default */
  if (c = veDeviceInstOption(i,"speed")) {
    if ((f->speed = str_to_bps(c)) < 0) {
      veError(MODULE,"%s: invalid serial speed: %s",name,c);
      goto cleanup;
    }
  }

  f->flow = FLOW_NONE; /* default */
  /* check later to see if flow-control is supported */
  if (c = veDeviceInstOption(i,"flow")) {
    if (strcmp(c,"none") == 0 || strcmp(c,"off") == 0)
      f->flow = FLOW_NONE;
    else if (strcmp(c,"xonxoff") == 0 || strcmp(c,"soft") == 0)
      f->flow = FLOW_SOFT;
    else if (strcmp(c,"rtscts") == 0 || strcmp(c,"hard") == 0)
      f->flow = FLOW_HARD;
    else {
      veError(MODULE,"%s: invalid serial flow control: %s",name,c);
      goto cleanup;
    }
  }
  
  if (c = veDeviceInstOption(i,"pos_epsilon")) {
    p_eps2 = atof(c);
    p_eps2 *= 2.0;
  }
  
  if (c = veDeviceInstOption(i,"quat_epsilon")) {
    q_eps2 = atof(c);
    q_eps2 *= 2.0;
  }
  
  if (c = veDeviceInstOption(i,"tmstamp"))
    f->tmstamp = atoi(c);

  /* default */
  f->stream = 1;
  if (c = veDeviceInstOption(i,"stream"))
    f->stream = atoi(c);

  if (c = veDeviceInstOption(i,"station")) {
    /* turn on specific stations */
    char *work, *s;

    work = veDupString(c);
    s = strtok(work,WHT);
    while (s != NULL) {
      if (sscanf(s,"%d",&k) != 1) {
	veError(MODULE,"%s: invalid station id: %s",name,s);
	goto cleanup;
      }
      if (k < 1 || k > MAXSTATION) {
	veError(MODULE,"%s: station id out of range: %s",name,s);
	goto cleanup;
      }
      f->data[k-1].active = 1;
      s = strtok(NULL,WHT);
    }
  } else
    /* active station 1 (array element 0) by default */
    f->data[0].active = 1;

  VE_DEBUG(2,("%s: opening serial line %s",name,f->line));

  /* okay - now try to open the connection */
  if ((f->fd = open(f->line, O_RDWR|O_NOCTTY)) < 0) {
    veError(MODULE,"%s: could not open serial line %s: %s",name,
	    f->line,strerror(errno));
    goto cleanup;
  }

  VE_DEBUG(2,("%s: setting up serial line %s",name,f->line));

  /* setup serial line */
  {
    struct termios t;
    if (tcgetattr(f->fd,&t)) {
      veError(MODULE,"%s: could not get serial settings for line %s: %s",
	      name,f->line,strerror(errno));
      goto cleanup;
    }
    /* clear flags */
    t.c_iflag = 0;
    t.c_oflag = 0;
    t.c_cflag = 0;
    t.c_lflag = 0;
    /* basic settings */
    t.c_cflag |= (CLOCAL|CREAD|CS8);
    t.c_cc[VMIN] = 1 ;
    t.c_cc[VTIME] = 0;
    
    /* set speeds */
    cfsetispeed(&t,f->speed);
    cfsetospeed(&t,f->speed);

    /* setup flow control */
    switch (f->flow) {
    case FLOW_SOFT:
      t.c_iflag |= (IXON|IXOFF);
      break;

    case FLOW_HARD:
#if defined(__sgi)
      t.c_cflag |= CNEW_RTSCTS;
#elif defined(__sun) || defined(__linux) || defined(CRTSCTS)
      t.c_cflag |= CRTSCTS;
#else
      veError(MODULE,"%s: hardware flow control not support on this platform",
	      name);
      goto cleanup;
#endif      
    }

    /* push settings */
    if (tcsetattr(f->fd,TCSAFLUSH,&t)) {
      veError(MODULE,"%s: could not set serial settings for line %s: %s",
	      name, f->line, strerror(errno));
      goto cleanup;
    }
  }

  VE_DEBUG(2,("%s: setting up tracker",name));

  /* send settings to tracker */
  VE_DEBUG(1,("resetting tracker"));
  send_ft(f->fd,"\030");

  /* wait to sync... */
  tcdrain(f->fd);
  tcflush(f->fd,TCIFLUSH);

  VE_DEBUG(1,("resetting reference frame"));

  VE_DEBUG(1,("setting uints to centimetres"));
  send_ft(f->fd,"u");
  
  VE_DEBUG(1,("requesting ASCII records"));
  send_ft(f->fd,"F");

  {
    char *refang;

    refang = veDeviceInstOption(i,"refang");

    VE_DEBUG(1,("setting up stations"));
    for(k = 0; k < MAXSTATION; k++) {
      if (!f->data[k].active) {
	/* deactivate it */
	sprintf(cmd,"l%d,0\r\n",k+1);
	send_ft(f->fd,cmd);
      } else {
	VE_DEBUG(1,("activating station %d",k+1));

	f->data[k].p_eps2 = p_eps2;
	f->data[k].q_eps2 = q_eps2;

	sprintf(cmd,"l%d,1\r\n",k+1);
	send_ft(f->fd,cmd);
	
	/* reset reference frame */
	sprintf(cmd,"R%d\r\n",k+1);
	send_ft(f->fd,cmd);
	if (refang) {
	  sprintf(cmd,"G%d,%s\r\n",k+1,refang);
	  send_ft(f->fd,cmd);
	  sprintf(cmd,"MB%d\r\n",k+1);
	  send_ft(f->fd,cmd);
	}
	
	/* set format */
	sprintf(cmd,f->tmstamp ? TMSTAMP_FORMAT : STD_FORMAT,k+1);
	send_ft(f->fd,cmd);
      }
    }
  }
    
  if (f->tmstamp) {
    VE_DEBUG(1,("resetting timestamp clock"));
    send_ft(f->fd,"MT\r\n"); /* set milliseconds */
    send_ft(f->fd,"MZ\r\n"); /* zero clock */
    f->tmstamp_zero = veClock(); /* record time */
  }

  if (c = veDeviceInstOption(i,"led")) {
    int led_v = atoi(c) ? 1 : 0;
    VE_DEBUG(1,("turning InterSense leds %s",led_v ? "on" : "off"));
    sprintf(cmd,"ML%d\r\n",led_v);
    send_ft(f->fd,cmd);
  }

  /* allocate remaining structures */
  f->update_rate_stat = veNewStatistic(MODULE, "update_rate", "Hz");
  f->update_rate_stat->type = VE_STAT_FLOAT;
  f->update_rate_stat->data = &f->update_rate;
  f->update_rate = 0.0;
  veAddStatistic(f->update_rate_stat);
  f->cback_rate_stat = veNewStatistic(MODULE, "cback_rate", "Hz");
  f->cback_rate_stat->type = VE_STAT_FLOAT;
  f->cback_rate_stat->data = &f->cback_rate;
  f->cback_rate = 0.0;

  f->mutex = veThrMutexCreate();
  f->update = veThrCondCreate();

  /* allow tracker to settle then throw away old data that
     may still be on the serial port */
  veNotice(MODULE,"sleeping to allow tracker to reset");
  tcdrain(f->fd);
  veMicroSleep(500000); /* wait for it to process (?) */
  tcflush(f->fd,TCIFLUSH);

  /* we should be ready now */
  return f;  

 cleanup:
  if (f) {
    if (f->fd >= 0)
      close(f->fd);
    free(f);
  }
  return NULL;
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

/*
 * read thread - read in values from tracker as quickly as
 * possible and update the data[] array (do *not* generate
 * VE events)
 */
#define MAXLN 1024
static void *read_thread(void *v) {
  VeDevice *d = (VeDevice *)v;
  VeiFT *f = (VeiFT *)(d->instance->idata);
  FILE *logf = NULL;
  char linebuf[MAXLN];
  int lineoffs, k;
  int nupdates;
  long elapsed = 0;
  FILE *inf;
  /* data we read in... */
  int stnum;
  long tmstamp;
  VeVector3 p;
  VeQuat q;

#if 0
  logf = fopen("/tmp/fastrak.log","w");
#endif
  inf = fdopen(f->fd,"r");
  if (!inf)
    veFatalError(MODULE, "%s: failed to open file buffer on input line",
		 d->name);

  /* put ourselves into streaming or polling mode */
  send_ft(f->fd,(f->stream ? "C" : "c"));

  while (1) {
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

    if (!(f->stream)) {
      VE_DEBUG(2,("%s: polling tracker",d->name));
      send_ft(f->fd,"P");
    }

    if (feof(inf) || !fgets(linebuf,MAXLN,inf))
      veFatalError(MODULE,"eof from tracker");

    if (!strchr(linebuf,'\n')) {
      veNotice(MODULE,"%s: mangled input - line is too long",d->name);
      resync_input(inf);
      continue;
    }

#if 0
    if (logf) {
      fprintf(logf,"%d %s\n",strlen(linebuf),linebuf);
      fflush(logf);
    }
#endif

    VE_DEBUG(2,("%s: reading incoming line",d->name));

    if (f->tmstamp) {
      if (sscanf(linebuf,TMSTAMP_SCAN "%n",
		 &stnum,&tmstamp,
		 &(p.data[0]),&(p.data[1]),&(p.data[2]),
		 &(q.data[3]),&(q.data[0]),&(q.data[1]),&(q.data[2]),
		 &lineoffs) < TMSTAMP_SCAN_CNT) {
	veNotice(MODULE,"%s: mangled input - no time-stamp",d->name);
	continue;
      }
      if (strlen(linebuf) != TMSTAMP_SIZE) {
	veNotice(MODULE,"%s: mangled input - incorrect line size",d->name);
	continue;
      }
      tmstamp += f->tmstamp_zero;
    } else {
      /* effects of "%n" on return count are non-standard */
      if (sscanf(linebuf,STD_SCAN "%n",
		 &stnum,
		 &(p.data[0]),&(p.data[1]),&(p.data[2]),
		 &(q.data[3]),&(q.data[0]),&(q.data[1]),&(q.data[2]),
		 &lineoffs) < STD_SCAN_CNT) {
	veNotice(MODULE,"%s: mangled input - no time-stamp",d->name);
	continue;
      }
      if (strlen(linebuf) != STD_SIZE) {
	veNotice(MODULE,"%s: mangled input - incorrect line size",d->name);
	continue;
      }
      tmstamp = veClock();
    }

    if (stnum <= 0 || stnum >= MAXSTATION) {
      veNotice(MODULE,"%s: mangled input - invalid station number: %d",
	      d->name,stnum);
      continue;
    }

    /* is the line clean */
    {
      char *l;
      l = &(linebuf[lineoffs]);
      while (isspace(*l))
	l++;
      if (*l != '\0') {
	veNotice(MODULE,"%s: mangled line - junk after actual data: %s",l);
	continue;
      }
    }

    /* we have read a clean-line */
    /* tweak values */
    p.data[0] *= 0.01;
    p.data[1] *= 0.01;
    p.data[2] *= 0.01;

    VE_DEBUG(2,("%s: successfully parsed line",d->name));

    /* update */
    {
      VeiFTStation *st;
      float pd, qd;

      st = &(f->data[stnum-1]);

      /* do we need to update?  we are the only ones
	 updating so we don't need to lock to check... */
      pd = PDIST2(st->p,p);
      qd = QDIST2(st->q,q);
      /* To update, this must be an active station, plus one 
	 of the following must be true:
	 - init == 0 (have never updated this station)
	 - both epsilons are 0 (implies no filtering)
	 - change in either p or q exceeds its respective
	   epsilon value
	 - tmstamp must be >= current timestamp and 
           <= (current timestamp - TMSTAMP_WINDOW) 
           (helps throw away some bogus entries)
	   (only check if st->init is true
      */
      if (st->active && (!st->init || (st->p_eps2 == 0.0 && 
				       st->q_eps2 == 0.0) ||
			 st->p_eps2 <= pd || st->q_eps2 <= qd)
		&& (!st->init || tmstamp >= st->tmstamp)) {
	/* update data - need to lock to avoid partial updates
	   being passed on, and to get cond. var. right */
	VE_DEBUG(2,("%s: update - [%ld] %2d (%2.4f,%2.4f,%2.4f) (%1.5f,%1.5f,%1.5f,%1.5f)",
		    d->name,tmstamp,stnum,
		    p.data[0],p.data[1],p.data[2],
		    q.data[0],q.data[1],q.data[2],q.data[3]));

	veThrMutexLock(f->mutex);
	st->p = p;
	st->q = q;
	st->tmstamp = tmstamp;
	st->init = 1;
	st->updated = 1;
	veThrCondSignal(f->update);
	veThrMutexUnlock(f->mutex);
      }
    }
  }

  veError(MODULE,"%s: read thread loop broken",d->name);
  return NULL;
}

/*
 * event thread - find updated elements in data structures and
 * generate VE events
 */
static void *event_thread(void *v) {
  VeDevice *d = (VeDevice *)v;
  VeiFT *f = (VeiFT *)(d->instance->idata);
  int ncbacks = 0;
  long cback_elapsed = 0;
  int k = 0;
  /* data we read in... */
  int stnum;
  long tmstamp;
  VeVector3 p;
  VeQuat q;

  /* lots of locking/unlocking here to keep locks fine-grained 
     and to give read_thread lots of time to get in there */
  while (1) {
    int chk, ready;

    veThrMutexLock(f->mutex);
    /* wait for something to do */
    ready = 0;
    
    /* logic here is a little unusual to enforce round-robin
       servicing of multiple stations */
    while (!ready) {
      chk = 0;
      while (chk < MAXSTATION && !f->data[k].updated) {
	k = (k+1)%MAXSTATION;
	chk++;
      }
      if (!f->data[k].updated)
	veThrCondWait(f->update,f->mutex);
      else
	ready = 1;
    }

    /* if we get here, something should be updated */
    assert(f->data[k].updated);
    /* copy data */
    stnum = k+1;
    tmstamp = f->data[k].tmstamp;
    p = f->data[k].p;
    q = f->data[k].q;
    f->data[k].updated = 0;
    
    /* we're done with the critical section */
    veThrMutexUnlock(f->mutex);

    /* outside of the critical section, send a VE event */
    VE_DEBUG(2,("%s: callback for station %d",d->name,stnum));
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

    /* make sure we process both position and quaternion before
       frame updates */
    {
      VeDeviceEvent *ve;
      VeDeviceE_Vector *evec;
      char s[80];

      veLockFrame();

      ve = veDeviceEventCreate(VE_ELEM_VECTOR,3);
      /* update elements */
      sprintf(s, "%s%d", "pos",stnum);
      ve->device = veDupString(d->name);
      ve->elem = veDupString(s);
      evec = (VeDeviceE_Vector *)(ve->content);
      for(k = 0; k < 3; k++)
	evec->value[k] = p.data[k];
      ve->timestamp = tmstamp;
      veDeviceInsertEvent(ve);

      /* right now there is no correction of angles - this is wrong! */
      sprintf(s, "%s%d", "quat",stnum);
      ve = veDeviceEventCreate(VE_ELEM_VECTOR,4);
      ve->device = veDupString(d->name);
      ve->elem = veDupString(s);
      evec = (VeDeviceE_Vector *)(ve->content);
      for(k = 0; k < 4; k++)
	evec->value[k] = q.data[k];
      ve->timestamp = tmstamp;
      veDeviceInsertEvent(ve);

      veUnlockFrame();
    }
  }
  
  veError(MODULE,"%s: event thread loop broken",d->name);
  return NULL;
}

static VeDevice *new_ft_driver(VeDeviceDriver *driver,
				VeDeviceDesc *desc,
				VeStrMap override) {
  VeDevice *d;
  VeiFT *f;
  VeDeviceInstance *i;
  int k;
  
  VE_DEBUG(1,("fastrak: creating new fastrak device"));
  
  i = veDeviceInstanceInit(driver,NULL,desc,override);
  if (!(f = init_ft(desc->name,i))) {
    veError(MODULE,"%s: device initialization failed",desc->name);
    return NULL;
  }
  i->idata = f;
  
  d = veDeviceCreate(desc->name);
  d->instance = i;
  d->model = veDeviceCreateModel();
  for(k = 0; k < MAXSTATION; k++) {
    char s[256];
    sprintf(s,"pos%d vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}",k+1);
    veDeviceAddElemSpec(d->model,s);
    sprintf(s,"quat%d vector 4 {0.0 0.0} {0.0 0.0} {0.0 0.0} {0.0 0.0}",k+1);
    veDeviceAddElemSpec(d->model,s);
  }

  /* two-thread model */
  veThreadInitDelayed(read_thread,d,0,0);
  veThreadInitDelayed(event_thread,d,0,0);
  return d;
}

static VeDeviceDriver fastrak_drv = {
  "fastrak", new_ft_driver
};

void VE_DRIVER_INIT(fastrak) (void) {
  veDeviceAddDriver(&fastrak_drv);
}

void VE_DRIVER_PROBE(fastrak) (void *phdl) {
  veDriverProvide(phdl,"input","fastrak");
}
