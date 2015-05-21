include ../Make.config

OBJS=atlantis.o dolphin.o shark.o swim.o whale.o
LIBS=$(VELIBS)

all : atlantis

atlantis : $(OBJS) 
	$(CC) $(LDFLAGS) -o atlantis $(OBJS) $(LIBS)

clean :
	$(RM) atlantis *.o *~
