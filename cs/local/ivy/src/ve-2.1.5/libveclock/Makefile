include ../Make.config

CFLAGS += -I.
OBJS = ve_clock_$(CLOCKARCH).o

all : libveclock.so libveclock.a

clean :
	$(RM) libveclock.so libveclock.a *.o $(JUNK)

libveclock.so : $(OBJS)
	$(SHLD) -o libveclock.so $(OBJS)
	[ ! -f libveclock.so ] || cp libveclock.so ../lib

libveclock.a : $(OBJS)
	$(AR) cr libveclock.a $(OBJS)
	$(RANLIB) libveclock.a
	cp libveclock.a ../lib

