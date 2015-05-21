#! /bin/sh
VEROOT=/cs/home/ivy/ve
export VEROOT
PROG=./atlantis
${PROG} -ve_env ../env/default.env -ve_user ../env/default.pro -ve_manifest ../env/manifest -ve_devices ./devices $*
