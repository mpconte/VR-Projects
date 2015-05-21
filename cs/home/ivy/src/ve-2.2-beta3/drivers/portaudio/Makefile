include autocfg.mk
include ../../Make.config

PADIR = portaudio_v18_1p1
PALIB = lib/libportaudio.a
.PHONY : $(PADIR)/$(PALIB)

TARGET = portaudio$(ACFG_MODEXT)
OBJS = portaudio_drv.o
XOBJS = $(PADIR)/$(PALIB)

BUILD = $(TARGET)

CFLAGS += -I$(PADIR)/pa_common
LIBS += $(ACFG_AUDIOLIB)

include ../Make.driver

$(PADIR)/$(PALIB) :
	(cd $(PADIR) && \
	([ -f config.status ] || ./configure ) && \
	make $(PALIB))

xclean :
	(cd $(PADIR) ; make clean)
