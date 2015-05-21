-include autocfg.mk

RANLIB = $(ACFG_RANLIB)
SHLD = $(ACFG_SHLD)
SO = $(ACFG_SOEXT)
CFLAGS = $(ACFG_CFLAGS)
LIBS = $(ACFG_GLU) $(ACFG_OPENGL) $(ACFG_JPEGLIB) $(ACFG_OSLIBS)

SRCS = txm.c txmpnm.c txmtga.c txmjpeg.c
OBJS = $(SRCS:.c=.o)

TARGETS = libtxm.o

default: $(TARGETS)

autocfg.mk :
	./autocfg -v

libtxm.o : $(OBJS)
	$(LD) -r -o libtxm.o $(OBJS) $(ACFG_JPEGLIB)

clean :
	rm -f *.o *.so *.dylib *.a *~ so_locations

distclean : clean
	rm -f autocfg.mk autocfg.h autocfg.sh

