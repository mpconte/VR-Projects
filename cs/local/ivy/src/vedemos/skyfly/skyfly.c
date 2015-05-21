
/*
 * skyfly.c     $Revision: 1.5 $
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ve.h>
#include <vei_gl.h>
#include <math.h>
#include "skyfly.h"

#if !defined(GL_VERSION_1_1)
#if defined(GL_EXT_texture_object)
#define glBindTexture(A,B)     glBindTextureEXT(A,B)
#define glGenTextures(A,B)     glGenTexturesEXT(A,B)
#define glDeleteTextures(A,B)  glDeleteTexturesEXT(A,B)
#else
#define glBindTexture(A,B)
#define glGenTextures(A,B)
#define glDeleteTextures(A,B)
#endif
#if defined(GL_EXT_texture)
#define GL_RGB5 GL_RGB5_EXT
#else
#define GL_RGB5 GL_RGB
#endif
#endif

#define ERR_WARNING                     0x1
#define ERR_FATAL                       0x2
#define ERR_SYSERR                      0x4

#define AMALLOC(a, type, num, func)     {                     \
  if((int)(a = (type*)calloc((num),sizeof(type))) <= 0)     \
     err_msg(ERR_FATAL, func, "amalloc failed");           \
}

float   ScaleZ   = 2.3;     /* Terrain height scale factor */
int     CellDim = 4;        /* Terrain cell is CellDim X Celldim quads */
int     NumCells = 36;      /* Terrain grid is NumCells X NumCells cells */
int     GridDim;            /* Terrain grid is GridDim X GridDim quads */
float   XYScale;            /* Conversion from world-space to grid index */
float   CellSize;           /* World-space size of cell */

int     Init_pos;           /* if true, set initial position and kbd mode */
float   Init_x, Init_y, Init_z, Init_azimuth;
int     rgbmode = GL_TRUE;
int     use_dlist = 1;
int     no_ring_buffer = 1; /* if non-zero don't bother filling the ring
			       buffer, just send the data directly */
float control_x = 0.0, control_y = 0.0, control_throttle = 0.0;

/* Color index ramp info */
int sky_base, terr_base;
int plane_colors[3];

/*
 * Data that changes from frame to frame needs to be double-buffered because
 * two processes may be working on two different frames at the same time.
 */
typedef struct buffered_data_struct {

        /* objects */
  perfobj_t       paper_plane_pos_obj[NUM_PLANES];
  perfobj_t       shot_pos_obj[NUM_SHOTS];
  perfobj_t       viewer_pos_obj;

        /* flags */
        unsigned int   paper_plane_pos_flags[2];
        unsigned int   viewer_pos_flags[2];

        /* data */
        float  paper_plane_position[NUM_PLANES][7];
        unsigned int shot_pos_flags[NUM_SHOTS][2];
        float  shot_position[NUM_SHOTS][6];
        float  viewer_position[12]; /* extra space */

} buffered_data;

/*
 * This is the per-pipe data structure which holds pipe id, semaphores,
 * and variable data buffers. Both gfxpipe and buffer structures live in
 * shared memory so the sim can communicate with its forked children.
 */
typedef struct gfxpipe_data_struct {
        int             gfxpipenum;
		buffered_data   **buffers;

} gfxpipe_data;

static gfxpipe_data     *gfxpipe;       /* A processes' gfxpipe struct */
static gfxpipe_data     *gfxpipes[1];   /* Maximum of 1 graphics pipe */
static int              num_pipes;
float           		fog_params[4];  /* Fog and clear color */
static float            fog_density = 0.025*2;
float           		far_cull  = 31.;    /* Far clip distance from eye */
int			mipmap = 0;
static int              texmode = GL_NEAREST;

static int              threecomp = 1;

int 	dither = GL_TRUE, fog = GL_TRUE;
int     Wxsize = 320, Wysize = 240; /* Default to 320x240 window */

/*
 * All non-variable data like geometry is stored in shared memory. This way
 * forked processes avoid duplicating data unnecessarily.
 */
shared_data     *SharedData;

/* //////////////////////////////////////////////////////////////////////// */

void sim_proc(void);
void sim_cyclops(void);
void sim_dualchannel(void);
void sim_singlechannel(void);
void sim_exit(void);
void init_misc(void); 
void init_shmem(void); 
void init_terrain(void);
void init_clouds(void);
void init_paper_planes(void);
void init_positions(void);
void init_gfxpipes(void);
/* void init_gl(int gfxpipenum); */
void init_gl(VeWindow *w);
void err_msg(int type, char* func, char* error);
void fly(perfobj_t *viewer_pos);
void fly_paper_planes(perfobj_t *paper_plane_pos);
void track_shots(perfobj_t *);
float terrain_height(void);
int makeclouds(float *);
void update_sounds(float *);

void init_skyfly(void)
{
    init_shmem();
    init_gfxpipes();
    init_clouds();
    init_terrain();
    init_paper_planes();
    init_positions();

    gfxpipe = gfxpipes[0];
    /* init_gl(gfxpipe->gfxpipenum); */
    veiGlSetWindowCback(init_gl);
}

/*
 * This is a single-channel version of the dual-channel simulation
 * described above.
 */
void sim_singlechannel(void)
{
  extern int use_stick;
	buffered_data  **buffered = gfxpipes[0]->buffers;
	
	if (use_stick)
	  fly_stick(&(buffered[0]->viewer_pos_obj));
	else
	  fly(&(buffered[0]->viewer_pos_obj));
	fly_paper_planes(buffered[0]->paper_plane_pos_obj);
	track_shots(buffered[0]->shot_pos_obj);
}

/*-------------------------------------- Cull ------------------------------*/

/*
 *   The cull and draw processes operate in a classic producer/consumer, 
 * write/read configuration using a ring buffer. The ring consists of pointers
 * to perfobj's instead of actual geometric data. This is important because
 * you want to minimize the amount of data 'shared' between two processes that
 * run on different processors in order to reduce cache invalidations.
 *   enter_in_ring and get_from_ring spin on ring full and ring empty 
 * conditions respectively.
 *   Since cull/draw are shared group processes(sproc), the ring buffer is
 * in the virtual address space of both processes and shared memory is not
 * necessary.
*/

#define RING_SIZE   10000    /* Size of ring */

typedef struct render_ring_struct {
    volatile unsigned int      head, tail;
    perfobj_t                   **ring;
} render_ring;

typedef struct render_context {
  render_ring ringbuffer;
  struct cull {
    perfobj_t       **cells;
    perfobj_t       viewer_pos_obj[2];
    unsigned int   viewer_pos_flags[4];
    float           viewer_position[2][4];
    float           fovx, side, farr, epsilon, plane_epsilon;
  } cull;
  perfobj_t cloud_list, terrain_list;
  int init;
  buffered_data buffered;
} render_context;
void cull_proc(render_context *, VeWallView *);
void draw_proc(render_context *);

void        enter_in_ring(render_context *, perfobj_t *perfobj);
perfobj_t*  get_from_ring(render_context *);


void init_terrain_list(struct render_context *ctx) 
{
  int x,y;
  int l;

  l = glGenLists(1);
  if (l == 0) {
    fprintf(stderr, "Cannot generate list\n");
    exit(1);
  }
  
  ctx->terrain_list.flags = calloc(3,sizeof(int));
  ctx->terrain_list.flags[0] = PD_DRAW_LIST;
  ctx->terrain_list.flags[1] = l;
  ctx->terrain_list.flags[2] = PD_END;

  glNewList(l,GL_COMPILE);
  for (x = 0; x < NumCells; x++)
    for(y = 0; y < NumCells; y++)
      drawperfobj(ctx->cull.cells[x * NumCells + y]);
  glEndList();

  fprintf(stderr, "Created terrain list %d\n", l);
}

int intersect_line_with_z(VeVector3 *p1, VeVector3 *p2, VeVector3 *res) {
  static float ieps = 1.0e-6;
  VeVector3 del;
  float t;
  int i;

  for(i = 0; i < 3; i++)
    del.data[i] = p2->data[i] - p1->data[i];
  if (fabs(del.data[2]) < ieps) {
    /* pretty much coplanar with z */
    if (fabs(p1->data[2]) < ieps || fabs(p2->data[2]) < ieps)
      return 1; /* almost incident with z=0 */
    else
      return -1; /* parallel to but away from z=0 - no intersection */
  }
  t = -p1->data[2]/del.data[2];
  if (t < 0.0 || t > 1.0)
    return -1; /* no intersection in range */
  for(i = 0; i < 3; i++)
    res->data[i] = p1->data[i] + del.data[i]*t;
  return 0;
}

void update_view_pos() {
  float *vdata_ptr = gfxpipe->buffers[0]->viewer_pos_obj.vdata;
  float x = *vdata_ptr, y = *(vdata_ptr + 1),
    z = *(vdata_ptr + 2), azimuth = *(vdata_ptr + 3),
    pitch = *(vdata_ptr + 4), roll = *(vdata_ptr + 5);
  VeVector3 v, in;
  VeMatrix4 a,b,c,d,e;

  in.data[0] = x; in.data[1] = y; in.data[2] = z;
  veGetOrigin()->loc = in;
  update_sounds(in.data);

  veMatrixRotate(&a,VE_X,VE_DEG,roll);
  veMatrixRotate(&b,VE_Y,VE_RAD,pitch);
  veMatrixRotate(&c,VE_Z,VE_RAD,-azimuth + M_PI/2.0);

  veMatrixMult(&c,&b,&d);
  veMatrixMult(&d,&a,&e);
  in.data[0] = in.data[1] = 0.0; in.data[2] = 1.0;
  veMatrixMultPoint(&e,&in,&v,NULL);
  veGetOrigin()->up = v;
  in.data[1] = in.data[2] = 0.0; in.data[0] = 1.0;
  veMatrixMultPoint(&e,&in,&v,NULL);
  veGetOrigin()->dir = v;
}

int is_valid_shot(perfobj_t *o) {
  return (*((float *)o->vdata) != 0.0);
}

void cull_proc(render_context *ctx, VeWallView *wv)
{
	float           *viewer;
	float           minx, maxx, miny, maxy;
	int             i, buffer = 0;
	int             x, y, x0, y0, x1, y1;
	float           vX, vY, vazimuth;
	float           ax, ay, bx, by, cx, cy;
	perfobj_t      *viewer_pos, *paper_plane_pos, *shot_pos;
	buffered_data  *buffered;
	perfobj_t      *terrain_texture = &(SharedData->terrain_texture_obj);
	perfobj_t      *paper_plane = &(SharedData->paper_plane_obj);
	perfobj_t      *paper_plane_start = &(SharedData->paper_plane_start_obj);
	perfobj_t      *paper_plane_end = &(SharedData->paper_plane_end_obj);
	perfobj_t      *clouds_texture = &(SharedData->clouds_texture_obj);
	perfobj_t      *clouds = &(SharedData->clouds_obj);

    if (!ctx->init) {
        int             x, y;

        ctx->cull.fovx = M_PI;
        ctx->cull.side = far_cull / cos(ctx->cull.fovx / 2.);
        ctx->cull.farr = 2.* ctx->cull.side * sin(ctx->cull.fovx / 2.);
        ctx->cull.epsilon = sqrtf(2.) * CellSize / 2.;
        ctx->cull.plane_epsilon = .5;

        ctx->cull.cells = (perfobj_t **) malloc(NumCells * NumCells * sizeof(perfobj_t *));
        for (x = 0; x < NumCells; x++)
            for (y = 0; y < NumCells; y++)
                ctx->cull.cells[x * NumCells + y] =
                    &(SharedData->terrain_cells[x * NumCells + y]);
	
	if (use_dlist)
	  init_terrain_list(ctx);

        ctx->ringbuffer.ring = malloc(RING_SIZE * sizeof(perfobj_t *));
        ctx->ringbuffer.head = ctx->ringbuffer.tail = 0;

        ctx->cull.viewer_pos_obj[0].flags = ctx->cull.viewer_pos_flags;
        ctx->cull.viewer_pos_obj[0].vdata = ctx->cull.viewer_position[0];
        ctx->cull.viewer_pos_obj[1].flags = ctx->cull.viewer_pos_flags;
        ctx->cull.viewer_pos_obj[1].vdata = ctx->cull.viewer_position[1];

        *(ctx->cull.viewer_pos_flags) = PD_VIEWER_POS;
        *(ctx->cull.viewer_pos_flags + 1) = PD_END;

	ctx->cloud_list.flags = calloc(3,sizeof(int));
	ctx->cloud_list.flags[0] = PD_DRAW_CLOUDS;
	ctx->cloud_list.flags[1] = makeclouds(clouds->vdata);
	ctx->cloud_list.flags[2] = PD_END;
	ctx->cloud_list.vdata = NULL;
        ctx->init = 1;
    }

    buffered = gfxpipe->buffers[buffer];

    viewer_pos = &(buffered->viewer_pos_obj);
    paper_plane_pos = buffered->paper_plane_pos_obj;
    shot_pos = buffered->shot_pos_obj;

    vX = *((float *) viewer_pos->vdata + 0);
    vY = *((float *) viewer_pos->vdata + 1);
    vazimuth = *((float *) viewer_pos->vdata + 3);

    viewer = ctx->cull.viewer_position[buffer];

    viewer[0] = vX;
    viewer[1] = vY;
    viewer[2] = *((float *) viewer_pos->vdata + 2);
    viewer[3] = vazimuth;


    enter_in_ring(ctx,&ctx->cull.viewer_pos_obj[buffer]);
    
    if (viewer[2]<SKY_HIGH) {
      /* draw clouds first */
      enter_in_ring(ctx,clouds_texture);
      /* enter_in_ring(ctx,clouds); */
      enter_in_ring(ctx,&ctx->cloud_list);
    }

    enter_in_ring(ctx,terrain_texture);

    enter_in_ring(ctx,&ctx->terrain_list);

    enter_in_ring(ctx,paper_plane_start);
    /*
     * Add visible planes to ring buffer
     */
    /* Don't draw plane 0 - that's the player */
    for (i = 1; i < NUM_PLANES; i++) {
      enter_in_ring(ctx,&paper_plane_pos[i]);
      enter_in_ring(ctx,paper_plane);
    }
    for (i = 0; i < NUM_SHOTS; i++) {
      if (is_valid_shot(&shot_pos[i]))
	enter_in_ring(ctx,&shot_pos[i]);
    }

    enter_in_ring(ctx,paper_plane_end);

    if (viewer[2]>SKY_HIGH) {
      /* draw clouds after everything else */
      enter_in_ring(ctx,clouds_texture);
      /* enter_in_ring(ctx,clouds); */
      enter_in_ring(ctx,&ctx->cloud_list);
    }

    enter_in_ring(ctx,(perfobj_t *) 0);     /* 0 indicates end of frame */
	
    buffer = !buffer;
}

void handle_draw(VeWindow *w, VeWallView *wv) {
  if (w->appdata == NULL) {
    w->appdata = calloc(1,sizeof(render_context));
    assert(w->appdata != NULL);
  }
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  cull_proc((render_context *)w->appdata,wv);
  draw_proc((render_context *)w->appdata);
}

void enter_in_ring(render_context *ctx, perfobj_t *perfobj)
{
  if (no_ring_buffer) {
    if (perfobj)
      drawperfobj(perfobj);
  } else {
    if (ctx->ringbuffer.head == RING_SIZE+ctx->ringbuffer.tail-1)
      veFatalError("skyfly", "no space in ring buffer");
    ctx->ringbuffer.ring[ctx->ringbuffer.head] = perfobj;
    ctx->ringbuffer.head = (ctx->ringbuffer.head+1)%RING_SIZE;
  }
}

perfobj_t* get_from_ring(render_context *ctx)
{
    static perfobj_t *pobj;

    if (ctx->ringbuffer.tail == ctx->ringbuffer.head)
      veFatalError("skyfly", "ring buffer is empty");
    pobj = ctx->ringbuffer.ring[ctx->ringbuffer.tail];
    ctx->ringbuffer.tail = (ctx->ringbuffer.tail+1)%RING_SIZE;
    return pobj;
}

/*-------------------------------------- Draw ------------------------------*/

void draw_proc(render_context *ctx)
{
	perfobj_t      *too_draw;
	int d = 0;

	if (no_ring_buffer)
	  return; /* nop */

	while ((too_draw = get_from_ring(ctx))) {
		drawperfobj(too_draw);
		d++;
	}
}


/*------------------------------- Init -----------------------------------*/

void init_texture_and_lighting(void);
void init_buffered_data(buffered_data *buffered);

void init_misc(void)
{
	float   density;

	threecomp = rgbmode;

	/*
	 * Compute fog and clear color to be linear interpolation between blue
	 * and white.
	 */
	density = 1.- expf(-5.5 * fog_density * fog_density *
							  far_cull * far_cull);
	density = MAX(MIN(density, 1.), 0.);

	fog_params[0] = .23 + density *.57;
	fog_params[1] = .35 + density *.45;
	fog_params[2] = .78 + density *.22;
	fog_params[3] = 1.0f;
}

void init_shmem(void)
{
	int			   i;
    unsigned int  *flagsptr;
    perfobj_vert_t *vertsptr;
    int             nflags, nverts;

    AMALLOC(SharedData, shared_data, 1, "init_shmem");
    AMALLOC(SharedData->terrain_cells, perfobj_t,
            NumCells * NumCells, "init_shmem");
    AMALLOC(SharedData->terrain_cell_flags, unsigned int *,
            NumCells * NumCells, "init_shmem");
    AMALLOC(SharedData->terrain_cell_verts, perfobj_vert_t *,
            NumCells * NumCells, "init_shmem");

    /*
     * Allocate the flags and vertices of all terrain cells in 2 big chunks
     * to improve data locality and consequently, cache hits 
     */
    nflags = 2 * CellDim + 1;
	AMALLOC(flagsptr, unsigned int, nflags * NumCells * NumCells, "init_shmem");
	nverts = (CellDim + 1) * 2 * CellDim;
	AMALLOC(vertsptr, perfobj_vert_t, nverts * NumCells * NumCells, "init_shmem");

	for (i = 0; i < NumCells * NumCells; i++) {
		SharedData->terrain_cell_flags[i] = flagsptr;
		flagsptr += nflags;
		SharedData->terrain_cell_verts[i] = vertsptr;
		vertsptr += nverts;
	}
}

/*
 * Initialize gfxpipe data structures. There is one set of semaphores
 * per pipe.
 */
void init_gfxpipes(void)
{
	int             i, j;

	num_pipes = 1;

	for (i = 0; i < num_pipes; i++) {

		AMALLOC(gfxpipes[i], gfxpipe_data, 1, "initgfxpipes");
		AMALLOC(gfxpipes[i]->buffers, buffered_data *, NBUFFERS,
				"init_gfxpipes");
		gfxpipes[i]->gfxpipenum = i;
	}

	for (j = 0; j < NBUFFERS; j++) {
		AMALLOC(gfxpipes[0]->buffers[j], buffered_data, 1,
				"init_gfxpipes");
		init_buffered_data(gfxpipes[0]->buffers[j]);
		}
}

void init_buffered_data(buffered_data *buffered)
{
    int             i;
    perfobj_t      *pobj;

    pobj = &(buffered->viewer_pos_obj);
    pobj->flags = buffered->viewer_pos_flags;
    pobj->vdata = buffered->viewer_position;

    *(buffered->viewer_pos_flags) = PD_VIEWER_POS;
    *(buffered->viewer_pos_flags + 1) = PD_END;

    for (i = 0; i < NUM_PLANES; i++) {
        pobj = &(buffered->paper_plane_pos_obj[i]);
        pobj->flags = buffered->paper_plane_pos_flags;
        pobj->vdata = buffered->paper_plane_position[i];
    }
    for (i = 0; i < NUM_SHOTS; i++) {
        pobj = &(buffered->shot_pos_obj[i]);
	pobj->flags = buffered->shot_pos_flags[i];
	buffered->shot_pos_flags[i][1] = PD_END;
	pobj->vdata = buffered->shot_position[i];
    }
    
    *(buffered->paper_plane_pos_flags) = PD_PAPER_PLANE_POS;
    *(buffered->paper_plane_pos_flags + 1) = PD_END;
}

/* ARGSUSED */
void init_gl(VeWindow *w)
{
	glClear(GL_COLOR_BUFFER_BIT);
	if (!rgbmode)
		glIndexi(0);

	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	set_fog(fog);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	set_dither(dither);

	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	init_texture_and_lighting();

	glHint(GL_FOG_HINT, GL_FASTEST);
	glFogi(GL_FOG_MODE, GL_EXP2);
	glFogf(GL_FOG_DENSITY, fog_density);

	if (rgbmode) {
		glFogfv(GL_FOG_COLOR, fog_params);
		if (fog && fog_density > 0)
			glEnable(GL_FOG);
	} else if (FOG_LEVELS > 1) {
		glFogi(GL_FOG_INDEX, FOG_LEVELS-1);
		if (fog)
			glEnable(GL_FOG);
	}
}

unsigned char* read_bwimage(char *name, int *w, int *h);

void init_texture_and_lighting(void)
{

    unsigned char  *bwimage256, *bwimage128;
    int             w, h;

    if(!(bwimage256 = (unsigned char*) read_bwimage("terrain.bw", &w, &h)))
       if(!(bwimage256 = (unsigned char *) 
                read_bwimage("/usr/demos/data/textures/terrain.bw", &w, &h)))
		err_msg(ERR_FATAL, "init_texture_and_lighting()",
										"Can't open terrain.bw");

	if(w != 256 || h != 256)
		err_msg(ERR_FATAL, "init_texture_and_lighting()",
										"terrain.bw must be 256x256");

	if (!(bwimage128 = (unsigned char *) read_bwimage("clouds.bw", &w, &h)))
		if (!(bwimage128 = (unsigned char *)
			  read_bwimage("/usr/demos/data/textures/clouds.bw", &w, &h)))
			err_msg(ERR_FATAL, "init_misc()", "Can't open clouds.bw");

	if (w != 128 || h != 128)
		err_msg(ERR_FATAL, "init_misc()", "clouds.bw must be 128x128");

	if (mipmap)
		texmode = GL_LINEAR_MIPMAP_LINEAR;
	else
		texmode = GL_NEAREST;

	if (!threecomp) {
		/*
		 * 1 and 2-component textures offer the highest performance on SkyWriter
		 * so they are the most recommended.
		 */
		glBindTexture(GL_TEXTURE_2D, 1);
		if (!mipmap)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texmode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texmode);
		if (mipmap)
			gluBuild2DMipmaps(GL_TEXTURE_2D, /*0,*/ 1, 256, 256, /*0,*/ GL_LUMINANCE, GL_UNSIGNED_BYTE, bwimage256);
		else if (rgbmode)
			glTexImage2D(GL_TEXTURE_2D, 0, 1, 256, 256, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bwimage256);
		else {
#define TXSIZE 128
			GLubyte buf[TXSIZE*TXSIZE];
			int ii;
			gluScaleImage(GL_LUMINANCE, 256, 256, GL_UNSIGNED_BYTE, bwimage256,
						  TXSIZE, TXSIZE, GL_UNSIGNED_BYTE, buf);
			for (ii = 0; ii < TXSIZE*TXSIZE; ii++) {
				buf[ii] = terr_base +
					FOG_LEVELS * (buf[ii] >> (8-TERR_BITS));
			}
#ifdef GL_COLOR_INDEX8_EXT  /* Requires SGI_index_texture and EXT_paletted_texture */
			glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, TXSIZE, TXSIZE,
						 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, buf);
#endif
#undef TXSIZE
		}

		glBindTexture(GL_TEXTURE_2D, 2);
		if (!mipmap)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texmode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texmode);
		if (mipmap)
			gluBuild2DMipmaps(GL_TEXTURE_2D, /*0,*/ 1, 128, 128, /*0,*/ GL_LUMINANCE, GL_UNSIGNED_BYTE, bwimage128);
		else if (rgbmode)
			glTexImage2D(GL_TEXTURE_2D, 0, 1, 128, 128, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bwimage128);
		else {
#define TXSIZE 64
			GLubyte buf[TXSIZE*TXSIZE];
			int ii;
			gluScaleImage(GL_LUMINANCE, 128, 128, GL_UNSIGNED_BYTE, bwimage128,
						  TXSIZE, TXSIZE, GL_UNSIGNED_BYTE, buf);
			for (ii = 0; ii < TXSIZE*TXSIZE; ii++) {
				buf[ii] = sky_base +
					FOG_LEVELS * (buf[ii] >> (8-SKY_BITS));
			}
#ifdef GL_COLOR_INDEX8_EXT  /* Requires SGI_index_texture and EXT_paletted_texture */
			glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, TXSIZE, TXSIZE,
						 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, buf);
#endif
#undef TXSIZE
		}
	} else {
		float r0, r1, g0, g1, b0, b1;
		int i;
		unsigned char *t256 = (unsigned char *)malloc(256*256*3);

		/* terrain */
		r0 = 0.40f;   r1 = 0.30f;
		g0 = 0.30f;   g1 = 0.70f;
		b0 = 0.15f;   b1 = 0.10f;

		for(i = 0; i < 256*256; i++) {
			float t = bwimage256[i] / 255.0f;
			t256[3*i+0] = (unsigned char) (255.0f * (r0 + t*t * (r1-r0)));
			t256[3*i+1] = (unsigned char) (255.0f * (g0 + t * (g1-g0)));
			t256[3*i+2] = (unsigned char) (255.0f * (b0 + t*t * (b1-b0)));
		}
		glBindTexture(GL_TEXTURE_2D, 1);
		if (!mipmap)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texmode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texmode);
		if (mipmap)
			gluBuild2DMipmaps(GL_TEXTURE_2D, /*0,*/ 3, 256, 256, /*0,*/ GL_RGB, GL_UNSIGNED_BYTE, t256);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, t256);


		/* sky without fog */
		r0 = 0.23;  r1 = 1.0f;
		g0 = 0.35;  g1 = 1.0f;
		b0 = 0.78;  b1 = 1.0f;
		for(i = 0; i < 128*128; i++) {
			float t = bwimage128[i] / 255.0f;
			t256[3*i+0] = (unsigned char) (255.0f * (r0 + t * (r1-r0)));
			t256[3*i+1] = (unsigned char) (255.0f * (g0 + t * (g1-g0)));
			t256[3*i+2] = (unsigned char) (255.0f * (b0 + t * (b1-b0)));
		}
		glBindTexture(GL_TEXTURE_2D, 2);
		if (!mipmap)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texmode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texmode);
		if (mipmap)
			gluBuild2DMipmaps(GL_TEXTURE_2D, /*0,*/ 3, 128, 128, /*0,*/ GL_RGB, GL_UNSIGNED_BYTE, t256);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, t256);

		/* sky with fog */
		r0 = fog_params[0];  r1 = 1.0f;
		g0 = fog_params[1];  g1 = 1.0f;
		b0 = fog_params[2];  b1 = 1.0f;
		for(i = 0; i < 128*128; i++) {
			float t = bwimage128[i] / 255.0f;
			t256[3*i+0] = (unsigned char) (255.0f * (r0 + t * (r1-r0)));
			t256[3*i+1] = (unsigned char) (255.0f * (g0 + t * (g1-g0)));
			t256[3*i+2] = (unsigned char) (255.0f * (b0 + t * (b1-b0)));
		}
		glBindTexture(GL_TEXTURE_2D, 3);
		if (!mipmap)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texmode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texmode);
		if (mipmap)
			gluBuild2DMipmaps(GL_TEXTURE_2D, /*0,*/ 3, 128, 128, /*0,*/ GL_RGB, GL_UNSIGNED_BYTE, t256);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, t256);
		free(t256);
	}

	free(bwimage256);
	free(bwimage128);

	/* both textures use BLEND environment */
	if (rgbmode) {
            if (threecomp) {
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            }
            else {
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
            }
	} else if (FOG_LEVELS > 1) {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
	} else {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}

	{
            GLfloat position[] = { LX, LY, LZ, 0., };
            GLfloat one[] = { 1.0, 1.0, 1.0, 1.0 };

            if (rgbmode)
                glLightfv(GL_LIGHT0, GL_AMBIENT, one);
            glLightfv(GL_LIGHT0, GL_POSITION, position);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, one);
            glLightfv(GL_LIGHT0, GL_SPECULAR, one);
	}

	if (rgbmode) {
		GLfloat ambient[] = { 0.3, 0.3, 0.1, 0.0 };
		GLfloat diffuse[] = { 0.7, 0.7, 0.1, 0.0 };
		GLfloat zero[] = { 0.0, 0.0, 0.0, 0.0 };

		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, zero);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT);
		glEnable(GL_COLOR_MATERIAL);
	} else {
		glMaterialiv(GL_FRONT_AND_BACK, GL_COLOR_INDEXES, plane_colors);
	}

	{
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
		glEnable(GL_LIGHT0);
	}
}

void lightpos(void)
{
    GLfloat position[] = { LX, LY, LZ, 0., };
    glLightfv(GL_LIGHT0, GL_POSITION, position);
}

void texenv(int env)
{
    GLfloat colors[3][4] = { { 0., 0., 0., 0., },
                             { .1, .1, .1, 0., },       /* terrain */
                             { 1., 1., 1., 0., }, };    /* sky */
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colors[env]);
}

/*-------------------------------- Utility ---------------------------------*/

void err_msg(int type, char* func, char* error)
{
	char    msg[512];

	if (type & ERR_WARNING) {
		fprintf(stderr, "Warning:  ");
		sprintf(msg, "Warning:  %s", error);
	}
	else if (type & ERR_FATAL) {
		fprintf(stderr, "FATAL:  ");
		sprintf(msg, "FATAL:  %s", error);
	}

	fprintf(stderr, "%s: %s\n", func, error);
	if (type & ERR_SYSERR) {
		perror("perror() = ");
		fprintf(stderr, "errno = %d\n", errno);
	}
	fflush(stderr);

	if (type & ERR_FATAL) {
		exit(-1);
		}
}

void set_fog(int enable)
{
    fog = enable;
    if (fog) {
        glEnable(GL_FOG);
        if (rgbmode)
            glClearColor(fog_params[0], fog_params[1], fog_params[2], 1.0);
        else {
            glClearIndex(sky_base + FOG_LEVELS - 1);
        }
    } else {
        glDisable(GL_FOG);
        if (rgbmode)
            glClearColor(0.23, 0.35, 0.78, 1.0);
        else {
            glClearIndex(sky_base);
        }
    }
}

void set_dither(int enable)
{
    dither = enable;
    if (dither) {
        glEnable(GL_DITHER);
    } else {
        glDisable(GL_DITHER);
    }
}

