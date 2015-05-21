include autocfg.mk
include ../../Make.config

TARGET = linuxmouse$(ACFG_MODEXT)
OBJS = linuxmouse.o

ifeq ($(HAS_LINUXINPUT),1)
BUILD = $(TARGET)
endif
include ../Make.driver

xclean:
