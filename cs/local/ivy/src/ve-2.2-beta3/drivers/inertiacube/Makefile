include autocfg.mk
include ../../Make.config

TARGET = inertiacube2$(ACFG_MODEXT)
OBJS = ve_isense.o isense.o 
XOBJS = libisense.so
XLIBS = libisense.so

# right now this driver only builds on Linux
# (due to binary object)
ifeq ($(ACFG_PLATFORM),Linux)
BUILD = $(TARGET)
endif
include ../Make.driver

xclean:
