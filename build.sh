#!/bin/bash

#-------------------------------------------------------------
# Get revision number from Git

revision="unknown"								# this value will be used in a case of missing git
version_file="UmodelTool/Version.h"
if [ -d .git ]; then
	git=`type -p git`							# equals to `which git`
	if [ -z "$git" ]; then
		if [ "$OSTYPE" == "msys" ]; then
			# assume Windows, find local git distribution
			progs="${PROGRAMFILES//\\//}"		# get from environment with slash replacement
			progs="/${progs//:/}"				# for msys: convert "C:/Program Files" to "/C/Program Files"
			if [ -d "$progs/Git" ]; then
				PATH=$PATH:"$progs/Git/bin"
			elif [ -d "$progs/SmartGitHg/git" ]; then
				PATH=$PATH:"$progs/SmartGitHg/git/bin"
			elif [ -d "$LOCALAPPDATA/Atlassian/SourceTree/git_local" ]; then
				PATH=$PATH:"$LOCALAPPDATA/Atlassian/SourceTree/git_local"
			fi
			# find gi
			git=`type -p git`
		fi
	fi
	if [ "$git" ]; then
		revision=`git rev-list --count HEAD`
		revision=$((revision+1))				# increment revision, because we're advanced from recent git commit
	fi
fi

# update tool version
# read current revision
[ -f "$version_file" ] && [ "$revision" ] && read last_revision < $version_file
last_revision=${last_revision##* }		# cut "#define ..."
# write back to a file if value differs or if file doesn't exist
[ "$last_revision" != "$revision" ] && echo "#define GIT_REVISION $revision" > $version_file

#-------------------------------------------------------------

PLATFORM="vc-win32"
#PLATFORM="mingw32"

# force PLATFORM=linux under Linux OS
#?? check this, when cross-compile under wine
[ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ] && PLATFORM="linux"

export vc_ver=10

[ "$project" ] || project="UmodelTool/umodel"		# setup default prohect name
[ "$root"    ] || root="."
[ "$render"  ] || render=1

makefile="makefile-$PLATFORM"

# build shader includes before call to genmake
if [ $render -eq 1 ]; then
	# build shaders
	#?? move to makefile
	cd "Unreal/Shaders"
	./make.pl
	cd "../.."
fi

# update makefile when needed
# [ $makefile -ot $project ] &&
$root/Tools/genmake $project.project TARGET=$PLATFORM > $makefile

# build
case "$PLATFORM" in
	"vc-win32")
		vc32tools --make $makefile
		;;
	"mingw32"|"cygwin")
		PATH=/bin:/usr/bin:$PATH		# configure paths for Cygwin
		gccfilt make -f $makefile
		;;
	"linux")
		make -j 4 -f $makefile			# use 4 jobs for build
		;;
	*)
		echo "Unknown PLATFORM=\"$PLATFORM\""
		exit 1
esac
