#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "matrix.h"

#include <ve.h>
#include <vei_gl.h>

#include <txm.h> /* texture manager support */

/* #define FRAME_INTERVAL (long)(1000/100) */
#define FRAME_INTERVAL ((long)(1000/100))
float STEP_SIZE = 0.025;
float speed = 2.0;
#define INVIEW 30
int draw_axes = 0;
int draw_stats = 0;
int do_fog = 0;
char *texture_name[2] = { "green.marble.2.ppm", "abrozzo.ppm" };
int texid[2] = { -1, -1 }; /* our texture id */
VeVector3 f_oloc, f_oup, f_odir;
VeVector3 f_loc, f_up, f_dir;
#define ANG_EPS 0.1

/* number of accumulator samples */
int accum_on = 0; /* accumulator buffer on/off? */
#define ACCUM_SMP 5

float r_ang[3] = { 0.0, 0.0, 0.0 }; /* pan, tilt, twist */
float r_tgt[3] = { 0.0, 0.0, 0.0 }; /* pan, tilt, twist */

struct {
    int current_cell;
    VeFrame org[ACCUM_SMP]; /* scaling for each position */
    float sc;            /* scaling for sum */
    int n;               /* how many to actually do (must be <= ACCUM_SMP)  */
} appstate;

typedef float vect3[3];

#define X 0
#define Y 1
#define Z 2
/* 
 *   generate a normalized vector of length 1.0
 */
void normalize(float *v) 
{
    float r;


    r = sqrt( v[X]*v[X] + v[Y]*v[Y] + v[Z]*v[Z] );

    v[X] /= r;
    v[Y] /= r;
    v[Z] /= r;
}


void sum(float *acc, float asc, float *a) {
    int i;
    for(i = 0; i < 3; i++)
        acc[i] += a[i]*asc;
}

void catmull_rom(float **p, float t, float *res) {
    float t2 = t*t, t3 = t*t*t;
    res[0] = res[1] = res[2] = 0.0;
    sum(res,3*t3,p[1]);
    sum(res,-3*t3,p[2]);
    sum(res,t3,p[3]);
    sum(res,-t3,p[0]);
    sum(res,2*t2,p[0]);
    sum(res,-5*t2,p[1]);
    sum(res,4*t2,p[2]);
    sum(res,-t2,p[3]);
    sum(res,t,p[2]);
    sum(res,-t,p[0]);
    sum(res,2,p[1]);
    res[0] /= 2; res[1] /= 2; res[2] /= 2;
}

/*
 *   make a cross product of the two vectors passed in via v1 and v2,
 *   and return the resulting perpendicular-to-both vector in result
 */
void cross(float *result, float *v1, float *v2) 
{
    result[X] = v1[Y]*v2[Z] - v1[Z]*v2[Y];
    result[Y] = v1[Z]*v2[X] - v1[X]*v2[Z];
    result[Z] = v1[X]*v2[Y] - v1[Y]*v2[X];
}

struct tunnelElement {
    vect3 v[8];
    vect3 b1, b2, b3, cp;
};

/* Simple texture loader for P6 PPM files */
int load_and_def_texture(char *file) {
    /* textures should be in raw (P6) PPM format*/
    FILE *f = NULL;
    int width, height, maxval, i;
    unsigned char *data = NULL;

    if (!file) {
        fprintf(stderr, "load_and_def_texture: no texture file defined\n");
        goto load_and_def_err;
    }

    if (!(f = fopen(file,"r"))) {
        fprintf(stderr, "load_and_def_texture: could not load texture file %s\n",
                file);
        goto load_and_def_err;
    }
    /* check for header*/
    if (fgetc(f) != 'P' || fgetc(f) != '6' || fgetc(f) != '\n') {
        fprintf(stderr, "load_and_def_texture: file %s does not have 'P6' PPM header\n",
                file);
        goto load_and_def_err;
    }
  
    if (fscanf(f, " %d %d %d", &width, &height, &maxval) != 3) {
        fprintf(stderr, "load_and_def_texture: file %s has corrupt or missing PPM header\n", file);
        goto load_and_def_err;
    }
    if (!isspace(fgetc(f))) {
        fprintf(stderr, "load_and_def_texture: file %s is missing newline/whitespace after PPM header\n",file
        );
        goto load_and_def_err;
    }
    data = malloc(width*height*3);
    assert(data != NULL);
    if (fread(data,3*sizeof(unsigned char),width*height,f) != width*height) {
        fprintf(stderr, "load_and_def_texture: truncated PPM file %s\n", file);
        goto load_and_def_err;
    }
    fclose(f);
    f = NULL;
    if (maxval != 255) {
        /* scale it to fit in a byte */
        for(i = 0; i < width*height*3; i++)
            data[i] = (unsigned char)((float)data[i]*255.0/(float)maxval);
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGB,
                 GL_UNSIGNED_BYTE,data);
    gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA,width,height,GL_RGB,
                      GL_UNSIGNED_BYTE,data);
    free(data);
    data = NULL;
    return 0;

    load_and_def_err:
    if (f)
        fclose(f);
    if (data)
        free(data);
    return -1;
}

float curr_color[3] = { 0.2, 1.0, 0.4 };
float col_vel[3] = { 0.02, -0.02, 0.01 };

void updateColor( void )
{
    int i;
    
    for( i = 0; i < 3; i++ )
    {
	curr_color[i] += col_vel[i];
	if ( curr_color[i] > 1.0 )
	{
	    curr_color[i] = 1.0;
	    col_vel[i] = -col_vel[i];
	}
	if ( curr_color[i] < 0.0 )
	{
	    curr_color[i] = 0.0;
	    col_vel[i] = -col_vel[i];
	}
    }
    glColor3fv(curr_color);
}

#define TUNNELVERT(i,x,y) ce->v[i][0] = x*b1[0] + y*b2[0] + delta*b3[0] + cp[0]; \
ce->v[i][1] = x*b1[1] + y*b2[1] + delta*b3[1] + cp[1]; \
ce->v[i][2] = x*b1[2] + y*b2[2] + delta*b3[2] + cp[2];

#define TUNNEL_X 4
#define TUNNEL_Y 4

void newCElem( struct tunnelElement *ce, vect3 b1, vect3 b2, 
               vect3 b3, vect3 cp, float delta )
{
    /* here we go: */
    int i;

    TUNNELVERT(0,-TUNNEL_X,-TUNNEL_Y/2)
    TUNNELVERT(1,-TUNNEL_X/2,-TUNNEL_Y)
    TUNNELVERT(2,TUNNEL_X/2,-TUNNEL_Y)
    TUNNELVERT(3,TUNNEL_X,-TUNNEL_Y/2)
    TUNNELVERT(4,TUNNEL_X,TUNNEL_Y/2)
    TUNNELVERT(5,TUNNEL_X/2,TUNNEL_Y)
    TUNNELVERT(6,-TUNNEL_X/2,TUNNEL_Y)
    TUNNELVERT(7,-TUNNEL_X,TUNNEL_Y/2)
    for( i = 0; i < 3; i++ )
    {
	ce->b1[i] = b1[i]; 
	ce->b2[i] = b2[i];
	ce->b3[i] = b3[i];
	ce->cp[i] = cp[i];
    }
}

void xformBases( matrix m, vect3 b1, vect3 b2, vect3 b3, vect3 cp, 
                 float delta )
{
    vect3 zeroV = { 0.0, 0.0, 0.0 };

    /* try translating first */

    mult_point(m,zeroV);
    mult_point(m,b1);
    mult_point(m,b2);
    mult_point(m,b3);
    /* mult_point(m,cp); */
    b1[0] -= zeroV[0];
    b1[1] -= zeroV[1];
    b1[2] -= zeroV[2];
    b2[0] -= zeroV[0];
    b2[1] -= zeroV[1];
    b2[2] -= zeroV[2];
    b3[0] -= zeroV[0];
    b3[1] -= zeroV[1];
    b3[2] -= zeroV[2];
    normalize(b1);
    normalize(b2);
    normalize(b3);

    cp[0] += b3[0]*delta;
    cp[1] += b3[1]*delta;
    cp[2] += b3[2]*delta;

    /*
    cp[0] += b3[0]*delta;
    cp[1] += b3[1]*delta;
    cp[2] += b3[2]*delta;
    */
}

#define TURN_LEFT   0
#define TURN_RIGHT  1
#define TILT_UP     2
#define TILT_DOWN   3
#define STRAIGHT    4

#define RAD_DELTA   15*M_PI/180.0

void randomMatrix( matrix m, int countLimit )
{
    static int counter = -1;

    if ( counter >= countLimit || counter == -1 )
    {
	/* get new matrix */
	switch( rand()%5 )
	{
            case TURN_LEFT: ;
                rotmtrx_y(m,RAD_DELTA);
                break;
            case TURN_RIGHT: ;
                rotmtrx_y(m,-RAD_DELTA);
                break;
            case TILT_UP: ;
                rotmtrx_x(m,RAD_DELTA);
                break;
            case TILT_DOWN: ;
                rotmtrx_x(m,-RAD_DELTA);
                break;
            case STRAIGHT: ; 
            default: ;
                identity_mtrx(m);
                break;
	}
	counter = 0;
    }
    else
	counter++;
}

#define cQueueSize 2000
int cQueueHead = 0;
int cQueueTail = 0;
struct tunnelElement cQueue[cQueueSize];

vect3 b1 = {1.0,0.0,0.0}, b2 = {0.0,1.0,0.0}, 
    b3 = {0.0,0.0,1.0}, cp = {0.0,0.0,0.0};
    matrix currentMatrix;

    void fillCQueue( void )
    {
        int i;
        static int called = 0;
        matrix m;

        if (called)
            return;

        for( i = 0; i < cQueueSize; i++ )
        {
            newCElem( &(cQueue[i]), b1, b2, b3, cp, 2.0 );
            randomMatrix(m,2);
            xformBases(m,b1,b2,b3,cp,10.0);
        }
        called = 1;
    }

#define colour1 0xff0000
#define colour2 0xc00000
#define colour3 0xa00000
#define colour4 0xc00000

    extern void cross( float *, float *, float * );
    extern void normalize(float *);

    float dotprod( float *v1, float *v2 )
    {
        return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
    }

    float frandom( void )
    {
        /* return random value from 0.0 - 1.0 */
        return (rand()&0xffffff)/(float)0xffffff;
    }

#define CV(i,k) (cQueue[i].v[k])

    void drawElement( int i )
{
    int k;
    float v[3];
    static float t00[2] = { 0.0, 0.0 };
    static float t10[2] = { 1.0, 0.0 };
    static float t01[2] = { 0.0, 1.0 };
    static float t11[2] = { 1.0, 1.0 };
    if ( i >= 0 && i < cQueueSize-1 )
    {
        /* do each triangulation */
        txmBindTexture(NULL,texid[i%2]);
        glColor3fv(curr_color);
        glBegin(GL_TRIANGLE_STRIP);
        for( k = 0; k < 8; k++ ) {
            glTexCoord2f((float)k,0);
            glVertex3fv(CV(i,k));
            glTexCoord2f((float)k,1.0);
            glVertex3fv(CV(i+1,k));
        }
        glTexCoord2f(8.0,0);
        glVertex3fv(CV(i,0));
        glTexCoord2f(8.0,1.0);
        glVertex3fv(CV(i+1,0));
        glEnd();

        if (draw_axes) {
            /* draw bases */
            glColor3f(1.0,1.0,1.0);
            glBegin(GL_LINES);
            glVertex3fv(cQueue[i].cp);
            glVertex3fv(cQueue[i+1].cp);
            glEnd();
            glColor3f(1.0,1.0,0.0);
            glBegin(GL_LINES);
            glVertex3fv(cQueue[i].cp);
            for(k = 0; k < 3; k++)
                v[k] = cQueue[i].cp[k] + cQueue[i].b3[k];
            glVertex3fv(v);
            glColor3f(0.0,1.0,0.0);
            glVertex3fv(cQueue[i].cp);
            for(k = 0; k < 3; k++)
                v[k] = cQueue[i].cp[k] + cQueue[i].b2[k];
            glVertex3fv(v);
            glColor3f(1.0,0.0,0.0);
            glVertex3fv(cQueue[i].cp);
            for(k = 0; k < 3; k++)
                v[k] = cQueue[i].cp[k] + cQueue[i].b1[k];
            glVertex3fv(v);
            glEnd();
        }
    }
}
 
int win_id;

float headlight_amb[] = { 0.2, 0.2, 0.2, 0.0 };
float headlight_spec[] = { 1.0, 1.0, 1.0, 0.0 };
float headlight_diff[] = { 1.0, 1.0, 1.0, 0.0 };
float headlight_pos[] = { 0.0, 0.0, 0.0, 1.0 };

float mat_amb[] = { 0.5, 0.3, 0.05, 0.0 };
float mat_diff[] = { 0.9333, 0.5569, 0.0, 0.0 };
float mat_spec[] = { 0.3, 0.18, 0.0, 0.0 };
float mat_shininess = 10.0;

void winsetup(VeWindow *w)
{
    float lmvar[5];

    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
  
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  
    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
#if 0
    /* do lighting model */
    glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, 1.0);
    lmvar[0] = lmvar[1] = lmvar[2] = lmvar[3] = 0.0;
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmvar);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.0);
  
    /* light */
    glLightfv(GL_LIGHT0, GL_AMBIENT, headlight_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, headlight_diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, headlight_spec);
    glLightfv(GL_LIGHT0, GL_POSITION, headlight_pos);
    /*
    lmvar[0] = 0.0;
    glLightfv(GL_LIGHT0, GL_CONSTANT_ATTENUATION, lmvar);

    lmvar[0] = 0.03;
    glLightfv(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, lmvar);
    */

        /* setup material 1 */
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diff);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
#endif

    if (do_fog) {
        glEnable(GL_FOG);
        glFogf(GL_FOG_DENSITY,0.02);
    }

glMatrixMode(GL_TEXTURE);
glScalef(2.0,2.0,2.0);
glMatrixMode(GL_MODELVIEW);
}

void RenderTunnel()
{
    int i,min = 0,max = cQueueSize;

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);


    if (appstate.current_cell >= 0) {
        min = appstate.current_cell - INVIEW;
        max = appstate.current_cell + INVIEW + 1;
        if (min < 0)
            min = 0;
        if (max > cQueueSize)
            max = cQueueSize;
    }

    for( i = min; i < max; i++ )
        drawElement(i);
}


#define GHOST_SC 0.8
void display(VeWindow *w, long tm, VeWallView *wv) {
    if (!accum_on || appstate.n <= 1) {
        /* trust current setup */
        RenderTunnel();
    } else {
        /* use accumulation buffer for motion blurring */
        /* this means we need to setup matrices ourselves */
        VeFrame *eye_p, *org_p;
        int i,j,k, steye;
        float viewm[16], projm[16];
        
        eye_p = veGetDefaultEye();
        org_p = veGetOrigin();
        steye = wv->eye;
        
        /* Blurring could be done with the accumulation buffer but
           h/w support is rare in the PC world and without h/w
           support it is *painfully* slow. */
        /* glClear(GL_ACCUM_BUFFER_BIT); */
        /* draw ghosts (1..n) */
        glColorMask(1,1,1,1);
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        
        for(k = appstate.n-1; k >= 0; k--) {
            VeFrame e;

            glColorMask(0,0,0,1); /* only clear alpha */
            glClearColor(0,0,0,GHOST_SC);
            glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
            glColorMask(1,1,1,0); /* disable writing to alpha buffer */
            glBlendFunc(GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);
            glEnable(GL_BLEND);
            
            *org_p = appstate.org[k];
            veCalcStereoEye(eye_p,&e,veGetProfile(),steye);
            veGetWindowView(w,eye_p,&e,wv);
            for(i = 0; i < 4; i++)
                for(j = 0; j < 4; j++) {
                    viewm[i+j*4] = wv->view.data[i][j];
                    projm[i+j*4] = wv->proj.data[i][j];
                }
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(projm);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(viewm);
            
            /* do the actual drawing */
            RenderTunnel();
            
            /* glAccum(GL_ACCUM,1.0/appstate.n); */
        }
        /* glAccum(GL_RETURN,1.0); */
    }
    if (draw_stats) {
        glColor3f(1.0,1.0,1.0);
        veiGlPushTextMode(w);
        veiGlDrawStatistics(NULL,0);
        veiGlPopTextMode();
    }
}

void moveview(void) {
    static long last;
    static int init = 0;
    static float spd = 30.0; /* deg/sec */
    int i;
    long tm = veClock();
    float del = spd*(tm-last)*1.0e-3;
    last = tm;
    if (!init) {
        init = 1;
        return;
    }

    for(i = 0; i < 3; i++) {
        if (fabs(r_ang[i]-r_tgt[i]) > ANG_EPS) {
            int sgn1 = r_tgt[i] > r_ang[i] ? 1 : -1;
            int sgn2 = r_tgt[i] > (r_ang[i]+del) ? 1 : -1;
            if (sgn1 != sgn2) { /* we'll pass the target this time */
                r_ang[i] = r_tgt[i];
                continue;
            }
            r_ang[i] += del*sgn1;
        }
    }
}

void move(void) {
    /* follow catmull-rom spline through centerpoints for trajectory
    "up" vector is roughly estimated */
    static int init = 0;
    static float where = 1.0; /* need to start 1 element in */
    int i,j;
    float offset;
    VeVector3 u,v,w;
    float *cm[5];

    /* updateColor(); */
    f_oloc = f_loc;
    f_odir = f_dir;
    f_oup = f_up;
    
    i = floor(where);
    offset = (where - (float)i);
    if (i >= cQueueSize-3)
        exit(0);
    appstate.current_cell = i;
    cm[0] = cQueue[i-1].cp; cm[1] = cQueue[i].cp; cm[2] = cQueue[i+1].cp;
    cm[3] = cQueue[i+2].cp; cm[4] = cQueue[i+3].cp;
    catmull_rom(cm,offset,v.data);
    f_loc = v;
    if (offset < 0.8)
        catmull_rom(cm,offset+0.2,u.data);
    else
        catmull_rom(&(cm[1]),offset-0.8,u.data);
    /* calculate up */
    for(j = 0; j < 3; j++) {
        u.data[j] -= v.data[j];
        v.data[j] = (1-offset)*cQueue[i].b1[j] + offset*cQueue[i+1].b1[j];
    }
    f_dir = u;
    veVectorCross(&u,&v,&w);
    f_up = w;

    vePostRedisplay();
    {
        static long last;
        static int init = 0;
        long tm = veClock();
        if (init) {
            where += speed*(tm-last)*1.0e-3;
        } else {
            /* f_oloc, etc. are bogus */
            f_oloc = f_loc;
            f_odir = f_dir;
            f_oup = f_up;
        }
        last = tm;
        init = 1;
    }
    

    /* where += STEP_SIZE; */
}

void nmove(float dt) {
    /* follow catmull-rom spline through centerpoints for trajectory
    "up" vector is roughly estimated */
    static float where = 1.0; /* need to start 1 element in */
    int i,j;
    float offset;
    VeVector3 u,v,w;
    float *cm[5];

    /* updateColor(); */
    f_oloc = f_loc;
    f_odir = f_dir;
    f_oup = f_up;
    
    i = floor(where);
    offset = (where - (float)i);
    if (i >= cQueueSize-3)
        exit(0);
    appstate.current_cell = i;
    cm[0] = cQueue[i-1].cp; cm[1] = cQueue[i].cp; cm[2] = cQueue[i+1].cp;
    cm[3] = cQueue[i+2].cp; cm[4] = cQueue[i+3].cp;
    catmull_rom(cm,offset,v.data);
    f_loc = v;
    if (offset < 0.8)
        catmull_rom(cm,offset+0.2,u.data);
    else
        catmull_rom(&(cm[1]),offset-0.8,u.data);
    /* calculate up */
    for(j = 0; j < 3; j++) {
        u.data[j] -= v.data[j];
        v.data[j] = (1-offset)*cQueue[i].b1[j] + offset*cQueue[i+1].b1[j];
    }
    f_dir = u;
    veVectorCross(&u,&v,&w);
    f_up = w;

    vePostRedisplay();
    {
        if (dt > 0.0) {
	  where += speed*dt;
        } else {
            /* f_oloc, etc. are bogus */
            f_oloc = f_loc;
            f_odir = f_dir;
            f_oup = f_up;
        }
    }
    

    /* where += STEP_SIZE; */
}

static void mkframe(VeFrame *o, VeVector3 *loc, VeVector3 *dir, VeVector3 *up) {
    VeVector3 v,v2,r;
    VeMatrix4 m;
    
    o->loc = f_loc;
    /* rotate view acc. to pan, tilt and twist */
    veMatrixRotateArb(&m,up,VE_DEG,r_ang[0]);
    veMatrixMultPoint(&m,dir,&v,NULL);
    veVectorCross(&v,up,&r);
    veMatrixRotateArb(&m,&r,VE_DEG,r_ang[1]);
    veMatrixMultPoint(&m,&v,&(o->dir),NULL);
    veMatrixMultPoint(&m,up,&v,NULL);
    veMatrixRotateArb(&m,&(o->dir),VE_DEG,r_ang[2]);
    veMatrixMultPoint(&m,&v,&(o->up),NULL);
}

void predisplay(void) {
    mkframe(veGetOrigin(),&f_loc,&f_dir,&f_up);
    if (!accum_on || ACCUM_SMP <= 1)
        appstate.n = 0;
    else {
        /* make other origins */
        int j,k;
        VeVector3 loc,dir,up;
        for(k = 0; k < ACCUM_SMP; k++) {
            float s = k/(float)ACCUM_SMP;
            for(j = 0; j < 3; j++) {
                loc.data[j] = f_loc.data[j]*s+f_oloc.data[j]*(1-s);
                up.data[j] = f_up.data[j]*s+f_oup.data[j]*(1-s);
                dir.data[j] = f_dir.data[j]*s+f_odir.data[j]*(1-s);
            }
            veVectorNorm(&up);
            veVectorNorm(&dir);
            mkframe(&(appstate.org[k]),&loc,&dir,&up);
        }
        appstate.n = ACCUM_SMP;
        appstate.sc = 1.0/ACCUM_SMP;
    }
}

#if 0
void spec_func(VeInput *i, VeWindow *w, long tm, int key, int x, int y)
{
    switch(key) {
        case XK_Up:
        case XK_KP_Up:
            STEP_SIZE *= 1.1;
            break;
        case XK_Down:
        case XK_KP_Down:
            STEP_SIZE /= 1.1;
            break;
        case XK_Left:
        case XK_KP_Left:
            move();
            break;
        case XK_Right:
        case XK_KP_Right:
            move();
            break;
        case XK_Escape:
            exit(0);
    }
    veImplPostRedisplay();
}
#endif /* 0 */

void timer_proc(void *x) {
    move();
    moveview();
    veAddTimerProc(FRAME_INTERVAL,timer_proc,x);
}

void idle_proc(void) {
    move();
    moveview();
}

void anim_proc(float t, float dt) {
  nmove(dt);
  moveview();
}

int exitcback(VeDeviceEvent *e, void *arg) {
    exit(0);
    return -1;
}

static unsigned random_seed = 0;

#define DATA_SEED 0
#define DATA_CELL 1

void recv_data(int tag, void *data, int dlen) {
    switch (tag) {
        case DATA_SEED:
        {
            unsigned *k = (unsigned *)data;
            fprintf(stderr, "[%d] filling queue with seed %u\n",
                    veMPId(),*k);
            srand(*k);
            fillCQueue();
        }
        break;
    }
}

int keyboard(VeDeviceEvent *e, void *arg) {
    if (!veDeviceToSwitch(e))
        return 0;
    switch (e->elem[0]) {
        case 'a': accum_on = !accum_on; break;
        case 'w': r_tgt[0] = 90.0; break;
        case 's': r_tgt[0] = 0.0; break;
        case 'x': r_tgt[0] = -90.0; break;
        case 'e': r_tgt[1] = 90.0; break;
        case 'd': r_tgt[1] = 0.0; break;
        case 'c': r_tgt[1] = -90.0; break;
        case 'r': r_tgt[2] = 90.0; break;
        case 'f': r_tgt[2] = 0.0; break;
        case 'v': r_tgt[2] = -90.0; break;
    }
}

int main( int argc, char **argv )
{
    int i;
    char *c;
    veInit(&argc,argv);

    veSetZClip(0.01,200.0);
    veSetOption("doublebuffer", "1");
    veSetOption("depth", "1");
    if (c = veGetOption("draw_axes"))
        draw_axes = atoi(c);
    if (c = veGetOption("draw_stats"))
        draw_stats = atoi(c);
    if (c = veGetOption("fog"))
        do_fog = atoi(c);
#if 0
    if (c = veGetOption("texture"))
        texture_name = c;
#endif

    /* initialize appstate */
    appstate.current_cell = -1;
    appstate.n = 0;
    
    veiGlSetWindowCback(winsetup);
    veiGlSetPreDisplayCback(predisplay);
    veiGlSetDisplayCback(display);

    veMPDataAddHandler(VE_DTAG_ANY,recv_data);
    veMPInit(); /* initialize by hand so we can send messages before entering
    the loop */

    if (veMPIsMaster()) {
        if (argc > 1)
            random_seed = atoi(argv[1]);
        else
            random_seed = (unsigned)time(NULL);
        srand(random_seed);
        fillCQueue(); /* we need this data ahead of time */
        veMPDataPush(DATA_SEED,&random_seed,sizeof(random_seed));
    }

    veDeviceAddCallback(exitcback,NULL,"exit");
    veDeviceAddCallback(keyboard,NULL,"keyboard");

    if (!(c = veGetOption("manual")) || atoi(c) == 0) {
      veAddTimerProc(0,timer_proc,0);
      /* veSetIdleProc(idle_proc); */
      /* veSetAnimProc(anim_proc); */
    }


    if (veMPIsMaster())
      nmove(0.0);

    veMPAddStateVar(DATA_CELL,&appstate,sizeof(appstate),VE_MP_AUTO);

    /* load texture */
    txmSetRenderer(NULL,txmOpenGLRenderer());
    for(i = 0; i < 2; i++) {
        if ((texid[i] = txmAddTexFile(NULL,texture_name[i],NULL,0)) <= 0) {
            fprintf(stderr, "failed to load texture %s\n",texture_name[i]);
            exit(-1);
        }
        txmSetOption(NULL,texid[i],TXM_OPTSET_TXM,
                     TXM_AUTOMIPMAP, 1);
        txmSetOption(NULL,texid[i],TXM_OPTSET_OPENGL,
                     GL_TEXTURE_WRAP_S, GL_REPEAT);
        txmSetOption(NULL,texid[i],TXM_OPTSET_OPENGL,
                     GL_TEXTURE_WRAP_T, GL_REPEAT);
        txmSetOption(NULL,texid[i],TXM_OPTSET_OPENGL,
                     GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    veRun();

    exit(0);
}
