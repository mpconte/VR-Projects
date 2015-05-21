include ../Make.config

CFLAGS += -I$(VEROOT)/include
LDFLAGS += -L$(VEROOT)/lib

OBJS = calibrate.o viewport.o windowerr.o

all : calibrate

calibrate : $(OBJS)
	$(CC) $(LDFLAGS) -o calibrate $(OBJS) -L$(VEROOT)/lib -l$(VEGLLIB) -l$(VELIB) -lGLU -lGL -lveclock $(OSLIBS) -lm

clean :
	$(RM) calibrate $(OBJS)
