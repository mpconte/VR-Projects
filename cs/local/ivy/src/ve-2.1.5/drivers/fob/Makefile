include ../../Make.config

OBJS = fob.o
XCFLAGS =
STATIC = fobdrv.o
SHARED = fobdrv.so

include ../Make.driver

#
# Other things to build
#

fob_test : fob_test.o fob.o
	$(CC) $(CFLAGS) -o fob_test fob_test.o fob.o -L../../lib -lveglxmt -lve -lGL -lX11 -lm -lpthread


