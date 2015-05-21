# defaults (posix)
CC = cc
OPT = -g
LIBMODE = ar
OBJEXT = .o
EXEEXT =
LIBEXT = .a
CFLAGS = $(OPT)
# in some trees, there's a config.mk above us...
-include ../config.mk

CFLAGS += -I.

.PHONY: default all test clean distclean objlist docs

CORESRCS = bsalloc.c bscore.c bserr.c bsexpr.c bsexprp.c bshash.c \
	bsinterp.c bslist.c bsobj.c bsparse.c bsstring.c \
	bsvar.c bssproc.c bslproc.c bshproc.c bsstream.c bsstreamstd.c

COREOBJS = $(CORESRCS:.c=$(OBJEXT))

default : all 

all : libbs$(LIBEXT) bssh$(EXEEXT) test

objonly : $(COREOBJS)

objlist :
	echo $(COREOBJS)

libbs$(LIBEXT) : $(COREOBJS)
	./mklib $(LIBMODE) libbs$(LIBEXT) $(COREOBJS)

bssh$(EXEEXT) : bssh$(OBJEXT) $(COREOBJS)
	$(CC) $(CFLAGS) -o bssh$(EXEEXT) bssh$(OBJEXT) $(COREOBJS)

docs : util/bsdoc.tcl util/doctmpl.tcl
	rm -f doc/ref/pkg_*.html
	util/bsdoc.tcl util/doctmpl.tcl doc/ref $(CORESRCS)

test : libbs$(LIBEXT) bssh$(EXEEXT)
	(cd test ; $(MAKE))

clean :
	rm -f $(COREOBJS) *~ bs$(OBJEXT) bssh$(OBJEXT) bssh$(EXEEXT) lemon$(EXEEXT) libbs$(LIBEXT)
	(cd test ; $(MAKE) clean)

distclean : clean
	rm -f *.o
	(cd test ; $(MAKE) distclean)

#
# dependencies
#

lemon$(EXEEXT) : lemon.c
	$(CC) $(CFLAGS) -o lemon$(EXEEXT) lemon.c

# check - is there a way to combine the following rules?
bsexprp.c : bsexprp.y lemon
	./lemon$(EXEEXT) -q bsexprp.y

bsexprp.h : bsexprp.y lemon
	./lemon$(EXEEXT) -q bsexprp.y

bsexpr$(OBJEXT) : bsexpr.c bsexpr.h

bsexpr.h : bsexprp.h

