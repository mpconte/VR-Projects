all : _config build_clock build_audio build_nid build_src build_examples build_drivers build_tools \
	build_calibrate

config : _config

_config : 
	[ -f Make.config ] || $(MAKE) reconfig

reconfig :
	rm -f Make.config
	if [ ! -f config/Make.config.`uname` ]; then \
		echo >&2 "*** No configuration for system `uname`" ; \
		exit 1 ; \
	fi
	ln -s config/Make.config.`uname` Make.config

build_clock:
	(cd libveclock ; $(MAKE))

build_nid : build_nid
	(cd libnid ; $(MAKE))

build_audio :
	(cd libveaudio ; $(MAKE))

build_src : build_clock
	(cd src ; $(MAKE))

build_examples : build_src
	(cd examples ; $(MAKE))

build_drivers : build_clock build_nid
	(cd drivers ; $(MAKE))

build_tools : build_clock build_nid
	(cd tools ; $(MAKE))

build_calibrate : build_src
	(cd calibrate ; $(MAKE))

install :
	@[ "$(VEROOT)" ] || (echo "Install using 'make VEROOT=/path/to/inst install'" ; false)
	[ -d $(VEROOT) ] || mkdir $(VEROOT)
	cp -pr doc include lib $(VEROOT)

clean :
	(cd src ; $(MAKE) clean)
	(cd examples ; $(MAKE) clean)
	(cd drivers ; $(MAKE) clean)
	(cd libnid ; $(MAKE) clean)
	(cd libveaudio ; $(MAKE) clean)
	(cd libveclock ; $(MAKE) clean)
	(cd tools ; $(MAKE) clean)
	(cd calibrate ; $(MAKE) clean)
	rm -f lib/lib* lib/drivers/*.so

distclean : config clean
	rm -f Make.config
	find . -name '*~' -print | xargs rm -f
	find . -name '.gdb_history' -print | xargs rm -f
	find . -name '.DS_Store' -print | xargs rm -f

