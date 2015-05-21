include Make.config

all : build

config : _config

autocfg.h : autocfg.d
	./autocfg -v || rm -f autocfg.h

# (re)run the autocfg script - this will configure the toplevel
# and all underlying directories...
_config :
	./autocfg -v || rm -f autocfg.h

# needed so make can run make in subdirectories correctly...
_fakeconfig :
	./autocfg -F

build : build_blue build_clock build_nid build_src build_examples \
	build_drivers build_tools build_calibrate

# Note:  blue does not actually need autocfg.h, but it
#  is nice to see autocfg succeed or fail before actually
#  building anything
build_blue: autocfg.h
	(cd blue ; $(MAKE))

build_clock: autocfg.h
	(cd veclock ; $(MAKE))

build_txm : autocfg.h
	(cd txm ; $(MAKE))

build_nid : autocfg.h
	(cd libnid ; $(MAKE))

build_src : build_clock build_blue build_txm autocfg.h
	(cd src ; $(MAKE))

build_examples : build_src autocfg.h
	(cd examples ; $(MAKE))

build_drivers : build_clock build_nid build_src autocfg.h
	(cd drivers ; $(MAKE))

build_tools : build_clock build_nid build_src autocfg.h
	(cd tools ; $(MAKE))

build_calibrate : build_src autocfg.h
	(cd calibrate ; $(MAKE))

install : build
	@[ "$(VEROOT)" ] || (echo "Install using 'make VEROOT=/path/to/inst install'" ; false)
	[ -d $(VEROOT) ] || mkdir $(VEROOT)
	(cd include ; $(MAKE) VEROOT=$(VEROOT) install)
	(cd doc ; $(MAKE) VEROOT=$(VEROOT) install)
	(cd lib ; $(MAKE) VEROOT=$(VEROOT) install)

clean :
	(cd src ; $(MAKE) clean)
	(cd examples ; $(MAKE) clean)
	(cd drivers ; $(MAKE) clean)
	(cd libnid ; $(MAKE) clean)
	(cd veclock ; $(MAKE) clean)
	(cd tools ; $(MAKE) clean)
	(cd calibrate ; $(MAKE) clean)
	(cd blue ; $(MAKE) clean)
	(cd txm ; $(MAKE) clean)
	rm -f lib/lib* lib/drivers/*.$(SO)

distclean : _fakeconfig
	$(MAKE) clean
	(cd blue ; $(MAKE) distclean)
	find . -name '*~' -print | xargs $(RM)
	find . -name '.gdb_history' -print | xargs $(RM)
	find . -name '.DS_Store' -print | xargs $(RM)
	find . -name 'autocfg.mk' -print | xargs $(RM)
	find . -name 'autocfg.h' -print | xargs $(RM)
	find . -name 'autocfg.sh' -print | xargs $(RM)
