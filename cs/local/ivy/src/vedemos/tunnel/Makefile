include ../Make.config

# Only need to link in libraries we actually reference
CFLAGS += -I/cs/home/matt/src/txm
LIBS=$(VELIBS)

all : tunnel

tunnel : tunnel.o matrix.o 
	$(CC) $(LDFLAGS) -o tunnel tunnel.o matrix.o -ltxm $(LIBS)

clean :
	$(RM) tunnel tunnel.o matrix.o
