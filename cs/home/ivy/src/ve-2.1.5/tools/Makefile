include ../Make.config

CFLAGS += -I. -I../include
NID = -lnid
NIDPATH = -L../lib

VE = -lvenull -lve
VECLOCK = -lveclock
VEPATH = ../lib

# Choose the right server objects for your platform
SCANOBJS = bsdscan.o
NIDSERVEROBJS = nid_server_$(VEARCH).o

default :
	if [ "$(BUILDNID)" ]; then $(MAKE) all; else true; fi

all : nid_snoop ve_nid_server
	if [ "$(BUILDNIDSERVER)" ]; then $(MAKE) nid_server; fi

nid_snoop : nid_snoop.o
	$(CC) $(CFLAGS) -o nid_snoop nid_snoop.o $(LIBPATH) $(NIDPATH) $(NID) $(VECLOCK)  $(OSLIBS)

nid_server : nid_server.o $(SCANOBJS) $(NIDSERVEROBJS)
	$(CC) $(CFLAGS) -o nid_server nid_server.o $(SCANOBJS) $(NIDSERVEROBJS) $(LIBPATH) $(NIDPATH) $(NID) $(VECLOCK) $(OSLIBS)

ve_nid_server : ve_nid_server.o $(SCANOBJS)
	$(CC) $(CFLAGS) -o ve_nid_server ve_nid_server.o $(SCANOBJS) $(LIBPATH) $(NIDPATH) $(NID) $(VE) $(VECLOCK) $(OSLIBS)

clean : 
	rm -f nid_snoop ve_nid_server nid_server *.o *~
