include ../autocfg.mk
include ../Make.config

CFLAGS += -I../include
LDFLAGS += -L../lib

OBJS = calibrate.o viewport.o windowerr.o

all : calibrate

calibrate : $(OBJS)
	$(CC) $(LDFLAGS) -o calibrate $(OBJS) -lve $(ACFG_OPENGL) -lGLU $(OSLIBS) -lm

clean :
	$(RM) calibrate $(OBJS)
