include ../Make.examples

all : audio

audio : audio.o
	$(CC) $(LDFLAGS) -o audio audio.o $(LIBPATH) -l$(VELIB) $(OPENGL) $(OSLIBS)

clean :
	$(RM) audio audio.o || true
