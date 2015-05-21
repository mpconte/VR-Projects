#
# Need to include necessary libraries for linking against audio
#
feature set AUDIOLIB value "-framework AudioToolbox -framework CoreAudio" test `uname` = Darwin
