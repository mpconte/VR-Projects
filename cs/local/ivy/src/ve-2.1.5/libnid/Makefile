include ../Make.config

CFLAGS += -I. -I../include
OBJS = nid.o nid_$(NIDARCH).o

default :	
	if [ "$(BUILDNID)" ]; then $(MAKE) all; else true; fi

all : libnid.a libnid.so

clean :
	$(RM) libnid.a libnid.so nid_snoop nid_server *.o $(JUNK)

libnid.a : $(OBJS)
	$(AR) cr libnid.a $(OBJS)
	$(RANLIB) libnid.a
	cp libnid.a ../lib

libnid.so : $(OBJS)
	$(SHLD) -o libnid.so $(OBJS) $(LIBPATH) -L../lib -lveclock $(NIDLIBS) 
	[ ! -f libnid.so ] || cp libnid.so ../lib
