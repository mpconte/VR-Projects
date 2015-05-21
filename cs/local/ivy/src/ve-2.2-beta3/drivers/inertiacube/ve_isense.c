/* ve driver for the inertiacube 
   - lots taken from ismain.c provided with the inertiacube dll
   - extended raw sensor data given under NDA with intersense 

   we will specify this with the following options:

   driver device inertiacube2 inertiacube2.so
   device icube inertiacube2 {
	serialport /dev/ttyS0 
	rawgyros 1
	rawaccels 1
	rawmags 1
	orientationtype quat
	ref 0.0 40.0 0.0
   }

   -- serialport - defines which serialport to use on the pc
   -- rawgyros    - flag 0/1, 1=yes, returns as an event raw gyro rates
   -- rawaccels   - flag 0/1, 1=yes, returns raw accelerometer data
   -- rawmags     - flag 0/1, 1=yes, returns raw magnetometer data
   -- orientationtype - specifies the event type of the orientation output, either quat (quaternion), frame (3 vectors, dir,up,left) 
   -- ref         - 3 vector for boresight reference (yaw, pitch roll)

NOTE: ref is not implemented yet!  Need to figure out how the boresight actually works and how we need to use it.

   use icube

possible events:
   icube.quat      - 4-vector quaternion
   icube.dir       - 3-vector direction vector
   icube.up        - 3-vector up vector
   icube.left      - 3-vector left vector
   icube.rawgyros  - 3-vector x,y,z rates
   icube.rawaccels - 3-vector x,y,z accelerations
   icube.rawmags   - 3-vector x,y,z magnetometer readings
*/
#include <stdio.h> 
#include <stdlib.h> 
#include <memory.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "isense.h"
#include "ve.h"

#define ESC 0X1B

#define MODULE "driver:inertiacube2"
#define CAMERA_TRACKER  0

typedef struct vei_inertiacube2
{
    ISD_TRACKER_HANDLE       handle;
    ISD_EXTENDED_DATA_TYPE   data;
    ISD_STATION_INFO_TYPE    Station[ISD_MAX_STATIONS];
    ISD_CAMERA_DATA_TYPE     cameraData;
    ISD_TRACKER_INFO_TYPE    Tracker;
    WORD station;
    float lastTime;

    /* useful flags */
    Bool rawgyros,rawaccels,rawmags;

    /* data used for events */
    VeVector3 ref[3];
    float quat[4];
    float euler[3];
    float data_rawgyros[3];
    float data_rawaccels[3];
    float data_rawmags[3];
    float data_a[3];
    float data_accel_nav[3];
    float data_mag_yaw;
    char orientationtype[255];
    char serialport[255];
}VeiInertiacube2;

/********************************************************************************/
/* FUNCTIONS BEGIN HERE */
/********************************************************************************/

/* open serialport, set termio properties then close the port... stupid linux */
void fix_serialport_hack(char *name)
{
	int serialport;
	struct termios term;
	VE_DEBUG(2,("inertiacube2: opening serialport for hack"));
	if((serialport = open(name, O_RDWR|O_NDELAY)) < 0)
	{
		veError(MODULE,"error opening serial port");
		return;
	}
	VE_DEBUG(2,("inertiacube2: getting serialport params"));
	if(ioctl(serialport,TCGETA,&term) < 0)
	{
		veError(MODULE,"error getting serialport info");
		close(serialport);
		return;
	}
	term.c_cflag = CLOCAL|CREAD|B115200|CS8;
	term.c_lflag = 0;
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;
	VE_DEBUG(2,("inertiacube2: setting serialport params"));
	if(ioctl(serialport,TCSETA,&term)<0)
	{
		veError(MODULE,"error setting serial port termio");
		close(serialport);
		return;
	}
	VE_DEBUG(2,("inertiacube2: closing serialport hack"));

	/* now close the damn thing again */
	close(serialport);
}

/* parseVector3 parses an option string for 3 values and sets a vector */
static int parseVector3(char *name, char *str, VeVector3 *v) 
{
	if (sscanf(str,"%f %f %f", &(v->data[0]), &(v->data[1]), &(v->data[2])) != 3) 
	{
		veError(MODULE, "bad option for input %s - should be vector", name);
		return -1;
	}
	return 0;
}

/* icube_init_options()
   - initialize inertiacube default options
*/
static void icube_init_options(VeiInertiacube2 *ic)
{
	/* default values for all options */
	strcpy(ic->serialport,"/dev/ttyS0");
	ic->rawgyros = 0;
	ic->rawaccels = 0;
	ic->rawmags = 0;
	ic->ref->data[0] = 0;
	ic->ref->data[1] = 0;
	ic->ref->data[2] = 0;
	strcpy(ic->orientationtype,"quat");
}

/* icube_print_options
   - for debugging, prints out the options on debug priority 2
*/
static void icube_print_options(VeiInertiacube2 *ic)
{
	char s[255];
	sprintf(s,"icube:serialport=%s",ic->serialport);
	VE_DEBUG(2,(s));
	sprintf(s,"icube:orientationtype=%s",ic->orientationtype);
	VE_DEBUG(2,(s));
	sprintf(s,"icube:rawgyros=%d",ic->rawgyros);
	VE_DEBUG(2,(s));
	sprintf(s,"icube:rawaccels=%d",ic->rawaccels);
	VE_DEBUG(2,(s));
	sprintf(s,"icube:rawmags=%d",ic->rawmags);
	VE_DEBUG(2,(s));
	sprintf(s,"icube:ref=%f %f %f",ic->ref->data[0],ic->ref->data[1],ic->ref->data[2]);
	VE_DEBUG(2,(s));
}

/* icube_set_options()
   - sets the user defined options if they exist
*/
static int icube_set_options(VeiInertiacube2 *ic, VeDeviceInstance *i, char *name)
{
	char *c;

	if(c = veDeviceInstOption(i,"serialport")) strcpy(ic->serialport,c);
	else VE_DEBUG(1,("inertiacube2drv: no serial port specified going with default /dev/ttyS0"));

	if(c = veDeviceInstOption(i,"rawgyros"))        ic->rawgyros = atoi(c);
	if(c = veDeviceInstOption(i,"rawaccels"))       ic->rawaccels = atoi(c);
	if(c = veDeviceInstOption(i,"rawmags"))         ic->rawmags = atoi(c);
	if(c = veDeviceInstOption(i,"ref"))             parseVector3(name,c,&(ic->ref[0]));
	if(c = veDeviceInstOption(i,"orientationtype")) strcpy(ic->orientationtype,c);

}

/* icube_setup()
   - setup the inertiacube structure
   - after this function is called, we are ready to initialize the inertiacube using the intersense library
*/
static VeiInertiacube2* icube_setup(char *name, VeDeviceInstance *i)
{
	VeiInertiacube2 *ic;

	/* allocate memory for our data structure */
	VE_DEBUG(2,("icube:setup:allocating memory for VeiInertiacube2 structure"));
	ic = (VeiInertiacube2 *)calloc(1,sizeof(VeiInertiacube2));
	if(ic == NULL)
	{
		veError(MODULE,"could not allocate space for VeiInertiacube2 structure");
		return NULL;
	}

	/* initialize all default options */
	VE_DEBUG(2,("icube:setup:initilizing default options"));
	icube_init_options(ic);

	/* set all options that have been specified */
	VE_DEBUG(2,("icube:setup:setting user specified options"));
	icube_set_options(ic,i,name);

	/* print options */ 
	icube_print_options(ic);

	/* open the serial port and initialize the driver */
	VE_DEBUG(2,("icube:setup:fixing the serialport hack"));
	fix_serialport_hack(ic->serialport);

	/* return the inertiacube pointer */
	VE_DEBUG(2,("icube:setup:ready to call Intersense library"));
	return ic;
}

/* icube_send_ve_event()
   - convenience function
   - sends a vector ve event with passed value
*/
static void icube_send_ve_event(VeiInertiacube2 *ic, VeDevice *d, float v[],int n, char *name)
{
	VeDeviceEvent *ve;
	VeDeviceE_Vector *evec;
	char s[10];
	int k;

	sprintf(s,"%s",name);

	ve = veDeviceEventCreate(VE_ELEM_VECTOR,n);
	ve->device = veDupString(d->name);
	ve->elem = veDupString(s);
	evec = (VeDeviceE_Vector *)(ve->content);
	ve->timestamp = ic->lastTime;

	for(k=0;k<n;k++)
	{
		evec->value[k] = v[k];
	}

	veDeviceInsertEvent(ve);
}

/* icube_start(ic)
   - initialize the inertiacube using the ISD library
 */
static void icube_start(VeiInertiacube2 *ic)
{
    Bool verbose = FALSE;

    VE_DEBUG(2,("icube: calling ISD_OpenTracker()"));
    ic->handle = ISD_OpenTracker( (Hwnd) NULL, 1, FALSE, FALSE);

    if( ic->handle < 1 )
    {
	    veError(MODULE, "couldn't find an inertiacube");
	    return;
    }

    VE_DEBUG(2,("icube: calling ISD_GetTrackerConfig()"));

    /* Get tracker configuration info */
    ISD_GetTrackerConfig( ic->handle, &(ic->Tracker), verbose );

    /* Clear station configuration info to make sure GetAnalogData and other flags are FALSE */
    memset((void *) ic->Station, 0, sizeof(ic->Station));

    /* 
     General procedure for changing any setting is to first retrieve current 
     configuration, make the change, and then apply it. Calling 
     ISD_GetStationConfig is important because you only want to change 
     some of the settings, leaving the rest unchanged. 
    */
    VE_DEBUG(2,("icube: filling in Station data"));
    if( ic->Tracker.TrackerType == ISD_PRECISION_SERIES )
    {
        for( ic->station = 1; ic->station <= 4; ic->station++ )
        {         
            /* fill ISD_STATION_INFO_TYPE structure with current station configuration */
            if( !ISD_GetStationConfig( ic->handle, &(ic->Station[ic->station-1]), ic->station, verbose )) break;
            
            ic->Station[ic->station-1].GetExtendedData = TRUE;
	    if(strcmp(ic->orientationtype,"quat")==0)
	    {
		    ic->Station[ic->station-1].AngleFormat = ISD_QUATERNION;
	    }
	    else if(strcmp(ic->orientationtype,"euler")==0)
	    {
		    ic->Station[ic->station-1].AngleFormat = ISD_EULER;
	    }

            /* apply new configuration to device */
            if( !ISD_SetStationConfig( ic->handle, &(ic->Station[ic->station-1]), ic->station, verbose )) break;
        }
    }
    ic->station = 1;
    ic->lastTime = ISD_GetTime();
    VE_DEBUG(2,("icube: initialized"));
}

/* icube_thread()
   - the main thread for the inertiacube
 */
static void *icube_thread(void *v)
{
	VeDevice *d = (VeDevice*)v;
	VeiInertiacube2 *ic = (VeiInertiacube2*)(d->instance->idata);
	VeVector3 dir,up,left;
	VeQuat quat;
	VeFrame frame;
	char s[10];
	int k;
	ISD_STATION_EXTENDED_DATA_TYPE *data;

	/* if no inertiacube is found, handle < 1 */
	if(ic->handle < 1) return NULL;

	VE_DEBUG(2,("icube:mainthread:entering event loop"));
	/* we do have an inertiacube, so enter event loop */
	while(1)
	{
		/* get data from the inertiacube */
		/* should have a flag for extended data */
		ISD_GetExtendedData(ic->handle, &ic->data);

		if(ISD_GetTime() - ic->lastTime > 0.01f)
		{
			ic->lastTime = ISD_GetTime();

			/* do we need this? */
			ISD_GetCommInfo(ic->handle,&ic->Tracker);

			veLockFrame();
			{
				data = &(ic->data.Station[0]);

				/* send orientation event */
				if(ic->Station->AngleFormat == ISD_QUATERNION)
					icube_send_ve_event(ic,d,data->Orientation,4, "quat");
				else
					icube_send_ve_event(ic,d,data->Orientation,3, "euler");

				/* get extended raw data */
				if(ic->Station->GetExtendedData == TRUE)
				{
					/* copy raw data into our own data structure */
					for(k=0;k<3;k++)
					{
						/* these are the really really raw data from the sensors */
						ic->data_rawgyros[k] = data->GyroRate[k];
						ic->data_rawaccels[k] = data->AccelBody[k];
						ic->data_rawmags[k] = data->MagRaw[k];

						/* this is acceleration after orientation has been applied to it */
						ic->data_accel_nav[k] = data->AccelNav[k];
					}
					/* copy magnetometer yaw, after orientation is applied to it */
					ic->data_mag_yaw = data->MagYaw;

					/* send the events */
					if(ic->rawgyros)
					{
						icube_send_ve_event(ic,d,data->GyroRate,3, "rawgyros");
					}
					if(ic->rawaccels)
					{
						icube_send_ve_event(ic,d,data->GyroRate,3, "rawaccels");
					}
					if(ic->rawmags)
					{
						icube_send_ve_event(ic,d,data->GyroRate,3, "rawmags");
					}
				}
			}
			veUnlockFrame();
		}
	}
}


/* new_inertiacube_driver
   - ve instantiates a new driver by calling this function
   - if null is returned then driver is not instantiated properly
*/
static VeDevice *new_inertiacube_driver(VeDeviceDriver *driver, VeDeviceDesc *desc, VeStrMap override)
{
	VeDevice *d;
	VeiInertiacube2 *ic;
	VeDeviceInstance *i;
	char *c, s[56];
	int j;

	/* instantiate a driver instance */
	VE_DEBUG(1,("inertiacube2:creating new inertiacube driver"));
	i = veDeviceInstanceInit(driver,NULL,desc,override); 

	/* set everything up, i.e. memory and user options */
	ic = icube_setup(desc->name, i);
	i->idata = ic;

	/* create a device and a device model */
	d = veDeviceCreate(desc->name);
	d->instance = i;
	d->model = veDeviceCreateModel();

	/* add elements to ve device model */
	/* direction vector */
	sprintf(s,"%s","dir vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);

	/* up vector */
	sprintf(s,"%s","up vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);
	/* left vector */
	sprintf(s,"%s","left vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);

	/* raw gyros vector */
	sprintf(s,"%s","rawgyros vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);
	/* raw accels vector */
	sprintf(s,"%s","rawaccels vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);
	/* raw mags vector */
	sprintf(s,"%s","rawmags vector 3 {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);

	/* quaternion */
	sprintf(s,"%s","quat vector 4 {0.0 0.0} {0.0 0.0} {0.0 0.0} {0.0 0.0}");
	veDeviceAddElemSpec(d->model,s);

	/* now initialize the actual inertiacube */
	icube_start(ic);

	VE_DEBUG(1,("inertiacube: initialized - starting main thread"));

	/* start the thread */
	veThreadInitDelayed(icube_thread,d,0,0);
	return d;
}

/* ve stuff to create a driver */
static VeDeviceDriver inertiacube2_drv = { "inertiacube2", new_inertiacube_driver};
void VE_DRIVER_INIT(inertiacube2) (void)
{
	veDeviceAddDriver(&inertiacube2_drv);
}

void VE_DRIVER_PROBE(inertiacube2) (void *phdl) {
  veDriverProvide(phdl,"input","inertiacube2");
}

