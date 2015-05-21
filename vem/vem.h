# ifndef _VEM_
# define _VEM_

# include <ve.h>

void VEM_default_bindings();
int VEM_pan_inc(VeDeviceEvent *e, void *arg);
int VEM_pan_dec(VeDeviceEvent *e, void *arg);
int VEM_tilt_inc(VeDeviceEvent *e, void *arg);
int VEM_tilt_dec(VeDeviceEvent *e, void *arg);
int VEM_twist_inc(VeDeviceEvent *e, void *arg);
int VEM_twist_dec(VeDeviceEvent *e, void *arg);
int VEM_forward(VeDeviceEvent *e, void *arg);
int VEM_backward(VeDeviceEvent *e, void *arg);
void VEM_check_collisions(int (*fn)());
void VEM_initial_position(float x, float y, float z);
void VEM_no_collisions();
void VEM_notify(void (*fn)());
void VEM_get_pos(float *x, float *y, float *z);
# endif


