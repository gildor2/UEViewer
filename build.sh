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
#			progs="${PROGRAMFILES//\\//}"		# get from environment with slash replacement
#			progs="/${progs//:/}"				# for msys: convert "C:/Program Files" to "/C/Program Files"
			[ -d "$PROGRAMFILES/Git" ] && gitpath="$PROGRAMFILES/Git/bin"
			[ -d "$PROGRAMW6432/Git" ] && gitpath="$PROGRAMW6432/Git/bin"
			! [ "$gitpath" ] && [ -d "$PROGRAMFILES/SmartGitHg/git" ] && gitpath="$PROGRAMFILES/SmartGitHg/git/bin"
			! [ "$gitpath" ] && [ -d "$LOCALAPPDATA/Atlassian/SourceTree/git_local" ] && gitpath="$LOCALAPPDATA/Atlassian/SourceTree/git_local/bin"
			[ "$gitpath" ] && PATH="$PATH:$gitpath"
			# find git
			git=`type -p git`
		fi
	fi
	[ "$git" ] && revision=`git rev-list --count HEAD`
fi

# update tool version
# read current revision
[ -f "$version_file" ] && [ "$revision" ] && read last_revision < $version_file
last_revision=${last_revision##* }		# cut "#define ..."
# write back to a file if value differs or if file doesn't exist (only for UModel project, i.e. when $project is empty)
[ -z "$project" ] && [ "$last_revision" != "$revision" ] && echo "#define GIT_REVISION $revision" > $version_file

#-------------------------------------------------------------

PLATFORM="vc-win32"
#PLATFORM="vc-win64"
#PLATFORM="mingw32" - not implemented yet

# force PLATFORM=linux under Linux OS
[ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ] && PLATFORM="linux"
#[ "$PLATFORM" == "linux" ] && PLATFORM="linux64"

# allow platform overriding from command line
[ "$1" ] && PLATFORM=$1

export vc_ver=2010

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
		vc32tools --make $makefile || exit 1
		;;
	"vc-win64")
		vc32tools --64 --make $makefile || exit 1
		;;
	"mingw32"|"cygwin")
		PATH=/bin:/usr/bin:$PATH			# configure paths for Cygwin
		gccfilt make -f $makefile || exit 1
		;;
	linux*)
		make -j 4 -f $makefile || exit 1	# use 4 jobs for build
		;;
	*)
		echo "Unknown PLATFORM=\"$PLATFORM\""
		exit 1
esac
