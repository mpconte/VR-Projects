include autocfg.mk
include ../../Make.config

TARGET = macjs$(ACFG_MODEXT)
OBJS = macjs.o
LIBS = -L. -lHIDUtilities
XLIBS = libHIDUtilities.dylib

ifeq ($(HAS_MACJS),1)
BUILD = $(TARGET)
endif
include ../Make.driver

xclean:
