include autocfg.mk
include ../../Make.config

TARGET = linuxjs$(ACFG_MODEXT)
OBJS = linuxjs.o

ifeq ($(HAS_LINUXJS),1)
BUILD = $(TARGET)
endif
include ../Make.driver

xclean:
