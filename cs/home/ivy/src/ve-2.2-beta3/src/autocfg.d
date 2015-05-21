# - include the txm configuration
. ../txm/autocfg.d
# Determine threading interface
feature set THREADIMPL value posix hasinclude pthread.h
require THREADIMPL "No threading implementation has been found"
# Determine libraries for threading interface 
feature set THREADLIB haslibrary pthread_create
feature set THREADLIB haslibrary pthread_create -lpthread
require THREADLIB "Cannot determine thread library (implementation is $ACFG_THREADIMPL)"
# POSIX select
feature set POSIXSELECT hasinclude sys/select.h
# POSIX poll
feature set POSIXPOLL hasinclude sys/poll.h

# Locate components for MP implementation
feature set SOCKETS haslibrary socket
feature set SOCKETS haslibrary socket -lsocket
feature set RSH searchpath rsh
feature set FORK haslibrary fork

# Check for POSIX implementation...
hascfg SOCKETS && hascfg RSH && hascfg FORK && \
	feature set MPIMPL value posix && \
	feature append OSLIBS value "$ACFG_SOCKETS"

hascfg JPEGLIB && feature append OSLIBS value "$ACFG_JPEGLIB"

require MPIMPL "No multi-processing implementation found"

# Determine driver interface
feature set DRVIMPL value darwin hasinclude mach-o/dyld.h
feature set DRVIMPL value posix hasinclude dlfcn.h
require DRVIMPL "No driver loading implementation found"

# Other checks
feature set SNPRINTF haslibrary snprintf
feature set VSNPRINTF haslibrary vsnprintf
feature set USLEEP haslibrary usleep

# Sound file loaders
#
# - the Mac OS X loader doesn't work - we'll provide audiofile
#   through the "overload" package
#
# Mac OS X (CoreAudio - AudioToolbox)
#feature set AUDIOTOOLBOX hasinclude AudioToolbox/AudioToolbox.h && \
#	feature append OSLIBS value "-framework AudioToolbox" &&
#	feature append OSLIBS value "-framework CoreServices"

# IRIX, Linux + other platforms (through OpenAF)
feature set AUDIOFILE haslibrary afOpenFile -laudiofile && \
	feature append OSLIBS value "-laudiofile"
