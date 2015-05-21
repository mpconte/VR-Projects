include autocfg.mk
include ../../Make.config

TARGET = glx$(ACFG_MODEXT)
OBJS = glx.o

ifeq ($(HAS_GLX),1)
BUILD = $(TARGET)
LIBS = $(ACFG_GLX) $(ACFG_GLU) $(ACFG_GL) $(ACFG_X11)
endif
include ../Make.driver

xclean:
