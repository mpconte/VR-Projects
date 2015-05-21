include ../autocfg.mk
include ../Make.config

# nothing to do for default (all)
all :

install : 
	@[ "$(VEROOT)" ] || (echo "Install using 'make VEROOT=/path/to/inst install'" ; false)install : 
	[ -d $(VEROOT)/include ] || mkdir $(VEROOT)/include
	for x in *.h; do \
	[ -f $(VEROOT)/include/$$x ] && $(RM) $(VEROOT)/include/$$x; \
	$(CP) $$x $(VEROOT)/include/$$x; done
