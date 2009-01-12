#!/bin/bash

PLATFORM="vc-win32"

# force PLATFORM=linux under Linux OS
#?? check this, when cross-compile under wine
[ "$OSTYPE" == "linux-gnu" ] && PLATFORM="linux"

#export vc_ver=7
project="extract.project"
makefile="makefile-$PLATFORM"

# update makefile when needed
# [ $makefile -ot $project ] &&
../genmake $project TARGET=$PLATFORM > $makefile

# build
case "$PLATFORM" in
	"vc-win32")
		vc32tools --make $makefile
		;;
	"linux")
		make -f $makefile
		;;
	*)
		echo "Unknown PLATFORM=\"$PLATFORM\""
		exit 1
esac
