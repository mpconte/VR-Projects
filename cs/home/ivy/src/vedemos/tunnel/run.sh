#! /bin/sh
VEROOT=/cs/home/matt/src/ve-2.0
export VEROOT
LD_LIBRARYN32_PATH=/cs/home/matt/src/txm:$VEROOT/lib:/cs/local/lib
export LD_LIBRARYN32_PATH
PROG=./tunnel
${PROG} $*
# -ve_env ../env/default.env -ve_user ../env/default.pro -ve_manifest ../env/manifest -ve_devices ./devices
