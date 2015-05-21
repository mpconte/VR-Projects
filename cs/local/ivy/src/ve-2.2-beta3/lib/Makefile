include ../autocfg.mk
include ../Make.config

all :

install :
	@[ "$(VEROOT)" ] || (echo "Install using 'make VEROOT=/path/to/inst install'" ; false)install : 
	[ -d $(VEROOT)/lib ] || mkdir $(VEROOT)/lib
	[ -d $(VEROOT)/lib/ve ] || mkdir $(VEROOT)/lib/ve
	[ -d $(VEROOT)/lib/ve/drivers ] || mkdir $(VEROOT)/lib/ve/drivers
	for x in lib* ve/drivers/*.$(SO); do \
	[ -f $(VEROOT)/lib/$$x ] && $(RM) $(VEROOT)/lib/$$x; \
	[ -f $$x ] && $(CP) $$x $(VEROOT)/include/$$x; done
