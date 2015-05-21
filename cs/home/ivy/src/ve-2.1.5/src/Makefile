include ../Make.config

CFLAGS += -I. -I../include

# Choose the right platform here
LIBVE_PLATFORM_SRC = ve_thread_$(THREADARCH).c ve_mp_$(MPARCH).c

LIBVE_SRC = \
ve_core.c \
ve_debug.c \
ve_dev_driver.c \
ve_dev_event.c \
ve_dev_filter.c \
ve_dev_mf.c \
ve_dev_model.c \
ve_dev_spec.c \
ve_dev_intf.c \
ve_device.c \
ve_driver.c \
ve_env.c \
ve_error.c \
ve_htrack.c \
ve_main.c \
ve_math.c \
ve_stats.c \
ve_thread.c \
ve_timer.c \
ve_tlist.c \
ve_util.c \
ve_mp.c \
ve_profiler.c \
$(LIBVE_PLATFORM_SRC)

LIBVE_NULL_SRC = vei_null.c

LIBVE_GLX_SRC = vei_gl.c \
vei_gl_6x10.c \
vei_gl_7x13.c \
vei_gl_8x13.c \
vei_gl_9x15.c \
vei_gl_font.c \
vei_glx.c

LIBVE_AGL_SRC = vei_gl.c \
vei_gl_6x10.c \
vei_gl_7x13.c \
vei_gl_8x13.c \
vei_gl_9x15.c \
vei_gl_font.c \
vei_agl.c \
Carbon_Error_Handler.c \
Carbon_SetupDSp.c \
Carbon_SetupGL.c

LIBVE_OBJ = $(LIBVE_SRC:.c=.o)

LIBVE_NULL_OBJ = $(LIBVE_NULL_SRC:.c=.o)

LIBVE_GLX_OBJ = $(LIBVE_GLX_SRC:.c=.o)

LIBVE_AGL_OBJ = $(LIBVE_AGL_SRC:.c=.o)

TARGETS = libve.a libve.so libvenull.a libvenull.so $(VEITARGETS)

# ------
# Dependencies from here on in
# ------

all : $(TARGETS)

glx : libveglx.a libveglx.so

agl : libveagl.a libveagl.so

libve.so : $(LIBVE_OBJ)
	$(SHLD) -o libve.so $(LIBVE_OBJ) $(LIBPATH) $(VELIBS) $(OSLIBS)
	[ ! -f libve.so ] || cp libve.so ../lib

libve.a : $(LIBVE_OBJ)
	$(AR) cr libve.a $(LIBVE_OBJ) 
	$(RANLIB) libve.a
	cp libve.a ../lib

libvenull.so : $(LIBVE_NULL_OBJ)
	$(SHLD) -o libvenull.so $(LIBVE_NULL_OBJ) $(LIBPATH)
	[ ! -f libvenull.so ] || cp libvenull.so ../lib

libvenull.a : $(LIBVE_NULL_OBJ)
	$(AR) cr libvenull.a $(LIBVE_NULL_OBJ)
	$(RANLIB) libvenull.a
	cp libvenull.a ../lib

libveglx.so : $(LIBVE_GLX_OBJ)
	$(SHLD) -o libveglx.so $(LIBVE_GLX_OBJ) $(LIBPATH) $(VELIBS) $(VEIGLXLIBS) $(OSLIBS)
	[ ! -f libveglx.so ] || cp libveglx.so ../lib

libveglx.a : $(LIBVE_GLX_OBJ)
	$(AR) cr libveglx.a $(LIBVE_GLX_OBJ)
	$(RANLIB) libveglx.a
	cp libveglx.a ../lib

libveagl.so : $(LIBVE_AGL_OBJ)
	$(SHLD) -o libveagl.so $(LIBVE_AGL_OBJ) $(LIBPATH) $(VELIBS) $(VEIAGLLIBS) $(OSLIBS)
	[ ! -f libveagl.so ] || cp libveagl.so ../lib

libveagl.a : $(LIBVE_AGL_OBJ)
	$(AR) cr libveagl.a $(LIBVE_AGL_OBJ)
	$(RANLIB) libveagl.a
	cp libveagl.a ../lib

clean :	
	$(RM) *.a *.so *.o $(JUNK)
