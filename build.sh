#!/bin/bash

PLATFORM="vc-win32"

# force PLATFORM=linux under Linux OS
#?? check this, when cross-compile under wine
[ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ] && PLATFORM="linux"

#export vc_ver=9

[ "$project" ] || project="umodel"		# setup default prohect name
[ "$root"    ] || root="."
[ "$render"  ] || render=1

makefile="makefile-$PLATFORM"

# update makefile when needed
# [ $makefile -ot $project ] &&
#?? add vc_ver here
$root/Tools/genmake $project.project TARGET=$PLATFORM > $makefile

if [ $render -eq 1 ]; then
	# build shaders
	#?? move to makefile
	cd "Unreal/Shaders"
	./make.pl
	cd "../.."
fi

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
