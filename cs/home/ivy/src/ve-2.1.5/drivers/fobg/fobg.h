#ifndef FOB_H
#define FOB_H

#define	FOB_SPORT		"/dev/ttyd7"

#define	POSITION_SCALE		(0.00018310542)      /* sqrt(36) / 32768 ?    */
#define	POSITION_SCALE_36	(0.00109866634113589892) /* 36in / 32767      */
#define	POSITION_SCALE_72	(0.00219733268227179784) /* 72in / 32767      */
#define	MATRIX_SCALE		(0.00003051757) /* .9999 / 32767 or 1/32768 ? */
#define	QUATERNION_SCALE	(0.00003050630207220679) /*.9996 / 32767      */
#define	MAX_LOOPS		10                       /* not used          */
#define	NUM_BIRDS		1                        /* not used          */

/* Bird addresses (used as RS232_TO_FBB commands) */
#define	BIRD0			0xf0	             /* standalone use only!! */
#define	BIRD1			0xf1
#define	BIRD2			0xf2

/* Change/Examine Value Parameters */
#define	BIRD_STATUS		0x00
#define	SOFTWARE_REV		0x01
#define	CRYSTAL_SPEED		0x02
#define	POSITION_SCALING	0x03
#define	FILTER_STATUS		0x04
#define	DC_FILTER_ALPHA		0x05
#define	MEASUREMENT_RATE_COUNT	0x06
#define	MEASUREMENT_RATE	0x07
#define	DATA_READY_CHAR		0x08
#define	DATA_READY_CHANGES	0x09
#define	ERROR_CODE		0x0A
#define	STOP_ON_ERROR		0x0B
#define	DC_FILTER_VM		0x0C
#define	DC_FILTER_ALPHA_MAX	0x0D
#define	SUDDEN_OUTPUT		0x0E
#define	SYSTEM_MODEL_ID		0x0F
#define	EXPANDED_ERROR_CODE	0x10
#define	XYZ_REF_FRAME		0x11
#define	TRANSMITTER_OP_MODE	0x12
#define	FBB_ADDRESSING_MODE	0x13
#define	FILTER_LINE_FREQ	0x14
#define	FBB_ADDRESS		0x15
#define	CHANGE_EX_HEMISPHERE	0x16
#define	CHANGE_EX_ANGLE_ALIGN2	0x17
#define	CHANGE_EX_REF_FRAME2	0x18
#define	BIRD_SERIAL_NO		0x19
#define	SENSOR_SERIAL_NO	0x1A
#define	XMTR_SERIAL_NO		0x1B
#define	FBB_HOST_DELAY		0x20
#define	GROUP_MODE		0x23
#define	SYSTEM_STATUS		0x24
#define	FBB_AUTOCONFIG		0x32

/* Bird Commands */
#define	ANGLE_ALIGN		'J'
#define	BUTTON_MODE		'M'
#define	CHANGE_VALUE		'P'
#define	EXAMINE_VALUE		'O'
#define	HEMISPHERE		'L'
#define	POINT			'B'
#define	POSITION		'V'
#define	POSITION_ANGLES		'Y'
#define	POSITION_MATRIX		'Z'
#define	POSITION_QUATERNION	']'
#define	RUN			'F'
#define	SLEEP			'G'
#define STREAM			'@'

/* Hemisphere Modes */
#define	FORWARD_1		0x00
#define	FORWARD_2		0x00
#define	AFT_1			0x00
#define	AFT_2			0x01
#define	LOWER_1			0x0C
#define	LOWER_2			0x00
#define	UPPER_1			0x0C
#define	UPPER_2			0x01
#define	LEFT_1			0x06
#define	LEFT_2			0x01
#define	RIGHT_1			0x06
#define	RIGHT_2			0x00

/* Mouse Button Modes */
#define	NONE_DOWN		0
#define	LEFT_DOWN		16
#define	MIDDLE_DOWN		48
#define	RIGHT_DOWN		112

/* Angle Alignment (for mickey.vr.clemson.edu) */
#define	SIN_A_1			0x0
#define	SIN_A_2			0x0
#define	COS_A_1			0x0
#define	COS_A_2			0x80
#define	SIN_E_1			0x0
#define	SIN_E_2			0x0
#define	COS_E_1			0xff
#define	COS_E_2 		0x7f
#define	SIN_R_1			0x0
#define	SIN_R_2			0x0
#define	COS_R_1			0xff
#define	COS_R_2			0x7f

/* Status Masks */
#define	MASTER     		0x80
#define	INITIALIZE 		0x40
#define	ERROR      		0x20
#define	RUNMODE    		0x10
#define	HOSTSYNC   		0x08
#define	ADDRESS    		0x04
#define	CRTSYNC    		0x02
#define	SYNC       		0x01
#define	TEST       		0x80
#define	XOFF       		0x40
#define	RUNMODE2   		0x20
#define	POS        		0x02
#define	ANGLE      		0x04
#define	MATRIX     		0x06
#define	POS_ANG    		0x08
#define	POS_MAT    		0x0A
#define	QUATERNION 		0x0E
#define	POS_QUAT   		0x10
#define	POINTMODE  		0x01

#ifndef I
#define I(i,r)                  (((r)+(i))%(r))
#endif
#ifndef J
#define J(j,c)                  (((c)+(j))%(c))
#endif
#ifndef M2A
#define M2A(i,j,r,c)            (I((i),(r))*(c) + J((j),(c)))
#endif


#endif
