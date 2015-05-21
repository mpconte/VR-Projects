include ../Make.examples

all : cube

cube : cube.o $(VELIBS)
	$(CC) $(LDFLAGS) -o cube cube.o $(LIBPATH) -l$(VEGLLIB) -l$(VELIB) -lveclock $(VEIGLLIBS) $(OSLIBS)

clean :
	$(RM) cube cube.o
