include ../Make.config

TARGETS = rc
LIBS = $(VELIBS)

SRCS = defrc.c matrix.c rc.c
OBJS =  $(SRCS:.c=.o)

all : $(TARGETS)

clean : 
	$(RM) $(TARGETS) $(OBJS)

rc : $(OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

