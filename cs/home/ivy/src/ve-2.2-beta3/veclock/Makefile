include autocfg.mk
include ../Make.config

CFLAGS += -I.
TARGETS = ve_clock.o

SRCS = ve_clock_core.c ve_clock_$(ACFG_IMPL).c
OBJS = $(SRCS:.c=.o)

all : $(TARGETS)

ve_clock.o : $(OBJS)
	$(LD) -r -o ve_clock.o $(OBJS)

clean :
	$(RM) $(TARGETS) $(OBJS) $(JUNK)
