include ../Make.examples

all : cube

cube : cube.o
	$(CC) $(LDFLAGS) -o cube cube.o $(LIBPATH) -l$(VELIB) $(OPENGL) $(OSLIBS)

clean :
	$(RM) cube cube.o || true
