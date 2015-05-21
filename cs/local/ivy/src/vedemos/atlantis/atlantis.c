
/* Copyright (c) Mark J. Kilgard, 1994. */

/**
 * (c) Copyright 1993, 1994, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ve.h>
#include <vei_gl.h>
#include "atlantis.h"

/* #define FRAME_INTERVAL 1000/100 */
#define FRAME_INTERVAL 5

struct _progstate progState;
#define TAG_PROGSTATE 1
int draw_stats = 0;

GLboolean moving;

void
InitFishs(void)
{
    int i;

    for (i = 0; i < NUM_SHARKS; i++) {
        progState.sharks[i].x = 70000.0 + rand() % 6000;
        progState.sharks[i].y = rand() % 6000;
        progState.sharks[i].z = rand() % 6000;
        progState.sharks[i].psi = rand() % 360 - 180.0;
        progState.sharks[i].v = 1.0;
    }

    progState.dolph.x = 30000.0;
    progState.dolph.y = 0.0;
    progState.dolph.z = 6000.0;
    progState.dolph.psi = 90.0;
    progState.dolph.theta = 0.0;
    progState.dolph.v = 3.0;

    progState.momWhale.x = 70000.0;
    progState.momWhale.y = 0.0;
    progState.momWhale.z = 0.0;
    progState.momWhale.psi = 90.0;
    progState.momWhale.theta = 0.0;
    progState.momWhale.v = 3.0;

    progState.babyWhale.x = 60000.0;
    progState.babyWhale.y = -2000.0;
    progState.babyWhale.z = -2000.0;
    progState.babyWhale.psi = 90.0;
    progState.babyWhale.theta = 0.0;
    progState.babyWhale.v = 3.0;
}

void
Init(void)
{
    static float ambient[] =
    {0.1, 0.1, 0.1, 1.0};
    static float diffuse[] =
    {1.0, 1.0, 1.0, 1.0};
    static float position[] =
    {0.0, 1.0, 0.0, 0.0};
    static float mat_shininess[] =
    {90.0};
    static float mat_specular[] =
    {0.8, 0.8, 0.8, 1.0};
    static float mat_diffuse[] =
    {0.46, 0.66, 0.795, 1.0};
    static float mat_ambient[] =
    {0.0, 0.1, 0.2, 1.0};
    static float lmodel_ambient[] =
    {0.4, 0.4, 0.4, 1.0};
    static float lmodel_localviewer[] =
    {0.0};

    glFrontFace(GL_CW);

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, lmodel_localviewer);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);

    glClearColor(0.0, 0.5, 0.9, 0.0);
}

void
Animate(void)
{
    int i;

    for (i = 0; i < NUM_SHARKS; i++) {
        SharkPilot(&progState.sharks[i]);
        SharkMiss(i);
    }
    WhalePilot(&progState.dolph);
    progState.dolph.phi++;
    WhalePilot(&progState.momWhale);
    progState.momWhale.phi++;
    WhalePilot(&progState.babyWhale);
    progState.babyWhale.phi++;
    vePostRedisplay();
}

void do_display(VeWindow *w, long tm, VeWallView *wv)
{
    int i;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (i = 0; i < NUM_SHARKS; i++) {
        glPushMatrix();
        FishTransform(&progState.sharks[i]);
        DrawShark(&progState.sharks[i]);
        glPopMatrix();
    }

    glPushMatrix();
    FishTransform(&progState.dolph);
    DrawDolphin(&progState.dolph);
    glPopMatrix();

    glPushMatrix();
    FishTransform(&progState.momWhale);
    DrawWhale(&progState.momWhale);
    glPopMatrix();

    glPushMatrix();
    FishTransform(&progState.babyWhale);
    glScalef(0.45, 0.45, 0.3);
    DrawWhale(&progState.babyWhale);
    glPopMatrix();

    if (draw_stats) {
      glColor3f(1.0,1.0,1.0);
      veiGlPushTextMode(w);
      veiGlDrawStatistics(NULL,0);
      veiGlPopTextMode();
    }
}

void setupwin(VeWindow *win) {
  Init();
}

void Timer(void *v) {
  Animate();
  veAddTimerProc(FRAME_INTERVAL,Timer,v);
}

int exitcback(VeDeviceEvent *e, void *arg) {
  exit(0);
  return -1;
}

int
main(int argc, char **argv)
{
  veInit(&argc,argv);

  InitFishs(); /* do this in all processes */

  veiGlSetWindowCback(setupwin);
  veSetOption("depth","1");
  veSetOption("doublebuffer","1");
  veiGlSetDisplayCback(do_display);

  veAddTimerProc(0,Timer,0);
  veDeviceAddCallback(exitcback,NULL,"exit");
  veMPAddStateVar(TAG_PROGSTATE,&progState,sizeof(progState),VE_MP_AUTO);

  veSetZClip(0.1,1000000.0);

  moving = GL_TRUE;

  veRun();
  return 0;             /* ANSI C requires main to return int. */
}
