include ../Make.config

TARGETS = skyfly
LIBS = $(VELIBS)

SRCS =	database.c fly.c gm_main.c image.c perfdraw.c skyfly.c
HDRS =	skyfly.h
OBJS =  $(SRCS:.c=.o)

all : $(TARGETS)

clean : 
	$(RM) $(TARGETS) $(OBJS)

skyfly : $(OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

