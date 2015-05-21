#
# You can only build the HTML document if you've got xmlp
#
XMLP = xmlp
XTABLES = -f document.t
XFLAGS =

.SUFFIXES: $(.SUFFIXES) .html .xml

.xml.html:
	$(XMLP) $(XTABLES) $(XFLAGS) $< > $@ || (rm -f $@ && exit 1)

SRCS =  api.xml     event.xml   glx.xml     intro.xml 
TARGETS = api.html     event.html   glx.html     intro.html

all : $(TARGETS)
