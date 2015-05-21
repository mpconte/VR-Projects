include autocfg.mk
include ../../Make.config

TARGET = opengl$(ACFG_MODEXT)
OBJS = opengl.o

ifeq ($(HAS_OPENGL),1)
BUILD = $(TARGET)
LIBS = $(ACFG_GLU) $(ACFG_OPENGL)
endif
include ../Make.driver

xclean:
