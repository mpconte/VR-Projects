VEROOT=~/src/ve-2.0
export VEROOT
LD_LIBRARY_PATH=.:~/src/ve-2.0/lib
export LD_LIBRARY_PATH
env __GL_FSAA_MODE=2 __GL_SYNC_TO_VBLANK=1 ./ve-hallway
