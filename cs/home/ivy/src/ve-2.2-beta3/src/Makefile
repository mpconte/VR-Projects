include autocfg.mk
include ../Make.config

CFLAGS += -I. -I../include

SO = $(ACFG_SOEXT)
OSLIBS = $(ACFG_OSLIBS) $(ACFG_THREADLIB) -lm

# Choose the right platform here
LIBVE_PLATFORM_SRC = ve_thread_$(ACFG_THREADIMPL).c \
	ve_mp_$(ACFG_MPIMPL).c \
	ve_drv_$(ACFG_DRVIMPL).c

LIBVE_SRC = \
ve_alloc.c \
ve_blue.c \
ve_core.c \
ve_debug.c \
ve_dev_driver.c \
ve_dev_event.c \
ve_dev_filter.c \
ve_dev_mf.c \
ve_dev_model.c \
ve_dev_spec.c \
ve_dev_ctrl.c \
ve_device.c \
ve_driver.c \
ve_env.c \
ve_error.c \
ve_main.c \
ve_math.c \
ve_stats.c \
ve_thread.c \
ve_timer.c \
ve_tlist.c \
ve_util.c \
ve_mp.c \
ve_profiler.c \
ve_version.c \
ve_render.c \
ve_mp_render.c \
ve_render_shim.c \
ve_sound.c \
ve_sndaf.c \
ve_sndmac.c \
ve_sndwav.c \
ve_audio.c \
ve_txm.c \
$(LIBVE_PLATFORM_SRC)

LIBVE_OBJ = $(LIBVE_SRC:.c=.o) ../veclock/ve_clock.o ../blue/libbs.a ../txm/libtxm.o

TARGETS = libve.a libve$(SO)

# ------
# Dependencies from here on in
# ------

all : $(TARGETS)

libve$(SO) : $(LIBVE_OBJ)
	$(SHLD) -o libve$(SO) $(LIBVE_OBJ) $(LIBPATH) $(VELIBS) $(OSLIBS)
	[ ! -f libve$(SO) ] || $(CP) libve$(SO) ../lib

libve.a : $(LIBVE_OBJ)
	$(AR) libve.a $(LIBVE_OBJ) 
	$(RANLIB) libve.a
	cp libve.a ../lib

clean :	
	$(RM) *.a libve$(SO) *.o $(JUNK)
