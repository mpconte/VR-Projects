include ../Make.config

all:
	if [ "$(BUILDDRIVERS)" ]; then \
	for x in *; do [ -d $$x ] && (cd $$x ; $(MAKE)); done ; \
	fi

clean:
	for x in *; do [ -d $$x ] && (cd $$x ; $(MAKE) clean); done

