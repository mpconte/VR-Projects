#
# Top level autocfg rules which we need everywhere
#
# - define platform
feature set PLATFORM testsh uname
feature set AR value "ar rc" searchpath ar
feature set LD searchpath ld
# - some platforms still need ranlib
feature set RANLIB searchpath ranlib
feature set RANLIB searchpath true
# - best guess at how to make a shared object with the
#   C compiler
feature set SHLD value "ld -dylib" test `uname` = Darwin
feature set SHLD value "$ACFG_CC -shared" ccompile "" "" -shared
feature set SHLD value "$ACFG_CC -Wl,-shared" ccompile "" "" -Wl,-shared
feature set SHLD value "$ACFG_CC -G" ccompile "" "" -G
require SHLD "Shared object support is required to build VE"
feature set MODLD value "$ACFG_CC -bundle" test `uname` = Darwin
feature set MODLD value "$ACFG_SHLD"
feature set SOEXT value ".dylib" test `uname` = Darwin
feature set SOEXT value ".so"
feature set MODEXT value ".bundle" test `uname` = Darwin
feature set MODEXT value ".so"
# Compatibility hacks for 10.4
feature set OSLIBS value "-L/usr/lib -ldylib1.o `gcc -print-file-name=libgcc.a` -lc" test `uname` = Darwin
feature append OSLIBS value "-lSystemStubs" exists /usr/lib/libSystemStubs.a
# look for extra includes
feature append CFLAGS value -I/usr/X11R6/include hasinclude GL/gl.h -I/usr/X11R6/include
feature set OPENGL haslibrary glBegin
feature set OPENGL haslibrary glBegin -lGL
feature set OPENGL haslibrary glBegin -L/usr/X11R6/lib -lGL
# Mac OS X
feature set OPENGL haslibrary glBegin -framework OpenGL
feature set GLUT haslibrary gluErrorString -- $ACFG_OPENGL
feature set GLU haslibrary gluErrorString -lGLU -- $ACFG_OPENGL
feature set GLU haslibrary gluErrorString -L/usr/X11R6/lib -lGLU -- $ACFG_OPENGL
# determine shared library extension
# common tools
feature set CP searchpath cp
feature set CP searchpath copy
feature append CFLAGS value -g
