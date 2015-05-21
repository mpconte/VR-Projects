include ../Make.config

CFLAGS += -I.
LIBSRCS = ve_audio.o mixer.o ve_audio_$(VEARCH).o
LIBOBJS = $(LIBSRCS:.c=.o)
LIBS = -laudiofile

default : 
	if [ "$(BUILDVEAUDIO)" ]; then $(MAKE) all; else true; fi

all:  libveaudio.a libveaudio.so vatest

libveaudio.a : $(LIBOBJS)
	$(AR) rc libveaudio.a $(LIBOBJS)
	$(RANLIB) libveaudio.a
	cp libveaudio.a ../lib

libveaudio.so : $(LIBOBJS)
	$(SHLD) -o libveaudio.so $(LIBOBJS) $(AUDIOLIBS)
	[ -f libveaudio.so ] && cp libveaudio.so ../lib

vatest : vatest.c libveaudio.a
	$(CC) $(CFLAGS) -o vatest vatest.c -L. -lveaudio $(LIBPATH) $(AUDIOLIBS) $(OSLIBS)

clean :
	$(RM) vatest libveaudio.a libveaudio.so *.o $(JUNK)

