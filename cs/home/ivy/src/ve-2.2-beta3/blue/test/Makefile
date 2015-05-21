CC = cc
OPT = -g
LIBMODE = ar
OBJEXT = .o
EXEEXT =
LIBEXT = .a
CFLAGS = $(OPT)
# in some trees, there's a config.mk a little above us
-include ../../config.mk
CFLAGS += -I..

.PHONY: all tests clean distclean

BS = ../libbs$(LIBEXT)

all : tests exetests

tests : test1$(EXEEXT) 
	@echo Running normal tests
	./runtests
	@echo Running tests with memory optimization
	env BS_OPT_MEMORY=1 ./runtests

exetests : test2$(EXEEXT)
	@echo Running executable tests
	./run1test test2.out ./test2 

test1$(EXEEXT) : test1$(OBJEXT) $(BS)
	$(CC) $(CFLAGS) -o test1$(EXEEXT) test1$(OBJEXT) $(BS)

test2$(EXEEXT) : test2$(OBJEXT) $(BS)
	$(CC) $(CFLAGS) -o test2$(EXEEXT) test2$(OBJEXT) $(BS)

clean : 
	rm -f test1$(EXEEXT) test2$(EXEEXT) *$(OBJEXT) *~

distclean : clean
