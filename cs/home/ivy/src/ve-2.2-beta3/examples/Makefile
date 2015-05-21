
all:
	for x in *; do [ ! -d $$x ] || (cd $$x ; $(MAKE)); done

clean:
	for x in *; do [ ! -d $$x ] || (cd $$x ; $(MAKE) clean); done
