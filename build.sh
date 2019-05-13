#!/bin/bash

while [ "$1" ]; do
	case "$1" in
		--debug)
			debug=1
			shift
			;;
		--vc)
			vc_ver=$2
			shift 2
			;;
		--64)
			PLATFORM="vc-win64"
			VC32TOOLS_OPTIONS="--64"
			shift
			;;
		*)
			echo "Usage: build.sh [--debug] [--vc <version>] [--64]"
			exit
			;;
	esac
done

#-------------------------------------------------------------
# Get revision number from Git

revision="(unversioned)"						# this value will be used in a case of missing git
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

[ "$PLATFORM" ] || PLATFORM="vc-win32"

# force PLATFORM=linux under Linux OS
[ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ] && PLATFORM="linux"
#[ "$PLATFORM" == "linux" ] && PLATFORM="linux64"

if [ "${PLATFORM:0:3}" == "vc-" ]; then
	# Visual C++ compiler
	# setup default compiler version
	[ "$vc_ver" ] || vc_ver=latest
	# Find Visual Studio
	. vc32tools $VC32TOOLS_OPTIONS --version=$vc_ver --check
	[ -z "$found_vc_year" ] && exit 1				# nothing found
	# Adjust vc_ver to found one
	vc_ver=$found_vc_year
#	echo "Found: $found_vc_year $workpath [$vc_ver]"
	GENMAKE_OPTIONS=VC_VER=$vc_ver					# specify compiler for genmake script
fi

[ "$project" ] || project="UmodelTool/umodel"		# setup default prohect name
[ "$root"    ] || root="."
[ "$render"  ] || render=1

# build shader includes before call to genmake
if [ $render -eq 1 ]; then
	# 'cd' calls below won't work if we're not calling from the project's root
	if [ "$root" != "." ]; then
		echo "Bad 'root'"
		exit 1
	fi
	# build shaders
	#?? move to makefile
	cd "Unreal/Shaders"
	./make.pl
	cd "../.."
fi

# prepare makefile parameters, store in obj directory
projectName=${project##*/}
makefile="$root/obj/$projectName-$PLATFORM.mak"
if ! [ -d $root/obj ]; then
	mkdir $root/obj
fi
if [ "$debug" ]; then
	makefile="${makefile}-debug"
	GENMAKE_OPTIONS+=" DEBUG=1"
fi

# update makefile when needed
# [ $makefile -ot $project ] &&
$root/Tools/genmake $project.project TARGET=$PLATFORM $GENMAKE_OPTIONS > $makefile

# build
case "$PLATFORM" in
	"vc-win32")
		Make $makefile || exit 1
		cp $root/libs/SDL2/x86/SDL2.dll .
		;;
	"vc-win64")
		Make $makefile || exit 1
		cp $root/libs/SDL2/x64/SDL2.dll .
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
