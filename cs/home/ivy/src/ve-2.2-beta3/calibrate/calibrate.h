#ifndef CALIBRATE_H
#define CALIBRATE_H

#include <ve.h>
#include <GL/gl.h>
#include <GL/glu.h>

extern VeWall *current_wall;
extern VeWindow *current_window;
extern GLUquadricObj *q;

/* MP data tags - defined here to avoid overlap */
#define DTAG_CURWIN    0 /* current window update */
#define DTAG_CURMODE   1 /* current mode update */
/* other data packets apply to the current window */
#define DTAG_WININFO   2

#endif /* CALIBRATE_H */
