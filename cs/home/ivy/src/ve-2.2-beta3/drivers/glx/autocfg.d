feature set X11 haslibrary XOpenDisplay -lX11
feature set X11 haslibrary XOpenDisplay -L/usr/X11R6/lib -lX11
feature set GLX haslibrary glXWaitGL
feature set GLX haslibrary glXWaitGL -lGL
feature set GLX haslibrary glXWaitGL -L/usr/X11R6/lib -lGL
feature set SWAPGROUP haslibrary glXJoinSwapGroupSGIX -- $ACFG_GLX
# Unsetting a window is harmless on 10.3 and deadlocks on 10.4
# - but needed on Linux (and possibly elsewhere)
feature set GLX_NOUNSET test `uname` = Darwin
