-------------------------
PREAMBLE:
- Author: Andrew Hogue
- Date: April 2004
-------------------------
This directory contains the ve driver for the inertiacube2 Pro from Intersense.  It currently uses the Extended Data library given under an NDA to Andrew Hogue and Michael Jenkin in 2004.  This driver will work for the inertiacube connected to a serial port which is user definable in the manifest and devices file.  It also works using the Free2Move F2M01 bluetooth serial plugs transparently if one is set as Master and the other as a Slave and they are paired (important).  This allows the inertiacube to be wireless.

--------------
INSTALLATION:
--------------
In order to install this driver into your VE distribution, 
unpack this directory into the $VEROOT/drivers/ directory 
(all of these files should be in a directory called inertiacube) 
and type:

% make 

this should make the driver and copy the files to the appropriate location.

then simply modify the default manifest file to contain the following:

driver device inertiacube2 inertiacube2drv.so
device icube inertiacube2 {
   <OPTIONS DETAILED HERE, SEE BELOW>
}

--------------
QUICK USAGE:
--------------
For a program to use the default settings for this device, simply add the following to the devices file:

use icube


This will allow the program to get events like icube.quat denoting a quaternion representing an orientation.

-------------------
Events:
-------------------
- several events can be output from this driver, they are:

icube.quat      - 4-vector quaternion x,y,z,w
icube.euler     - 3-vector euler angles x,y,z
icube.rawgyros  - 3-vector gyro rates x,y,z
icube.rawaccels - 3-vector accelerations x,y,z
icube.rawmags   - 3-vector magnetometer readings x,y,z

-------------------
DETAILED USAGE:
-------------------
There are several options you can specify in the driver.
Currently, the following options are suppported:
- serialport 
- rawgyros
- rawaccels
- rawmags
- orientationtype

Default values for these options are:
 serialport /dev/ttyS0
 rawgyros 0
 rawaccels 0
 rawmags 0
 orientationtype quat

Explanation of options:
------------------------
 - The "serialport" option allows you to specify which serialport to look on.  This should be /dev/ttyS0 however it will work for any serialport.  However, the inertiacube library requires the "isports.ini" file to match this option since this is the only way the inertiacube can be initialized.

- The "rawgyros" option is a flag (0 or 1) which specifies whether the raw gyro rates are to be output as an event

- The "rawaccels" option is a flag (0 or 1) which specifies whether the raw accelerations from the accelerometers are to be output as an event

- The "rawmags" option is a flag (0 or 1) which specifies whether the raw magnetometer readings are to be output as an event

- The "orientationtype" option is either euler or quat.  This changes the name of the event to icube.quat or icube.euler.  It simply changes the type of orientation data to be output either a quaternion (quat) or euler angles (euler).


