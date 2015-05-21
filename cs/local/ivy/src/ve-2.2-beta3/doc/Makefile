include ../autocfg.mk
include ../Make.config

all :

.PHONY: blueref

blueref :
	../blue/util/bsdoc.tcl ../blue/util/doctmpl.tcl blueref ../blue/*.c ../src/ve_blue.c

cdoc :
	$(HOME)/src/cdoc/cdoc -o ref ../include/*.h

install :
	@[ "$(VEROOT)" ] || (echo "Install using 'make VEROOT=/path/to/inst install'" ; false)install : 
	[ -d $(VEROOT)/doc ] || mkdir $(VEROOT)/doc
	[ -d $(VEROOT)/doc/drivers ] || mkdir $(VEROOT)/doc/drivers
	[ -d $(VEROOT)/doc/ref ] || mkdir $(VEROOT)/doc/ref
	for x in *.html *.gif *.jpg drivers/*.html drivers/*.gif \
	drivers/*.jpg ref/*.html ref/*.gif ref/*.jpg; do \
	[ -f $(VEROOT)/doc/$$x ] && $(RM) $(VEROOT)/doc/$$x; \
	[ -f $$x ] && $(CP) $$x $(VEROOT)/include/$$x; done
