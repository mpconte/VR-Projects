#
# Rebuild diagrams
#
DOT = dot

SRCS = volatile.dot
TARGETS = $(SRCS:.dot=.ps)

%.ps : %.dot
	$(DOT) -Tps $< -o $@

all : $(TARGETS)

clean :
	rm -f $(TARGETS) *~
