#!/bin/bash


# Parse command line parameters

while [ "$1" ]; do
	case "$1" in
		--debug)
			# enable debug build
			debug=1
			shift
			;;
		--profile)
			profile=1
			PLATFORM="vc-win64"			# force 64 bit build for profiler
			VC32TOOLS_OPTIONS="--64"
			shift
			;;
		--vc)
			vc_ver=$2
			shift 2
			;;
		--64)
			# switch to 64 bit platform (Win64)
			PLATFORM="vc-win64"
			VC32TOOLS_OPTIONS="--64"
			shift
			;;
		--file)
			# compile a single file from VSCode, should replace slashes
			single_file=${2//\\//}
			shift 2
			;;
		*)
			echo "Usage: build.sh [--debug] [--profile] [--vc <version>] [--64] [--file <cpp file>]"
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
[[ "$OSTYPE" == "darwin"* ]] && PLATFORM=osx
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
	Unreal/Shaders/make.pl
fi

# prepare makefile parameters, store in obj directory
projectName=${project##*/}
makefile="$root/obj/$projectName-$PLATFORM"
if ! [ -d $root/obj ]; then
	mkdir $root/obj
fi
# debugging options
if [ "$debug" ]; then
	makefile="${makefile}-debug"
	GENMAKE_OPTIONS+=" DEBUG=1"
elif [ "$profile" ]; then
	makefile="${makefile}-profile"
	GENMAKE_OPTIONS+=" TRACY=1"
fi
makefile="${makefile}.mak"

# update makefile when needed
# [ $makefile -ot $project ] &&
$root/Tools/genmake $project.project TARGET=$PLATFORM $GENMAKE_OPTIONS > $makefile

if [ "$single_file" ]; then
# Using perl with HEREDOC for parsing of makefile to find object file matching required target.
# Code layout: target=`perl << 'EOF'
# EOF
# `
# 1) using quoted 'EOF' to prevent variable expansion
# 2) passing parameters to a script using 'export <variable', return value - as output capture
# 3) putting perl command into `` (inverse quotes)
# 4) s/// command in perl code has extra quote for '$'
export makefile
export single_file
target=`perl <<'EOF'
	open(FILE, $ENV{"makefile"}) or die;
	$defines = ();
	while ($line = <FILE>)
	{
		next if $line !~ /^\S+/;	# we're interested only in lines starting without indent
		next if $line =~ /^\#/;		# no comments
		$line =~ s/(\r|\n)//;		# string end of line
#		print($line."\n");
		# parse assignment
		($var, $val) = $line =~ /^(\w+)\s*\=\s*(.*)$/;
		if (defined($var) && defined($val)) {
			$defines{$var} = $val;
		} else {
			# parse target
			($target, $cpp) = $line =~ /^(\S+)\s*\:\s*(\S+)(\s|$)/;
			if (defined($target) && defined($cpp)) {
				next if $cpp ne $ENV{"single_file"};	# match with single_file value
#				print("$cpp -> $target\n");
				for my $key (keys(%defines)) {
					my $value = $defines{$key};
					$target =~ s/\\$\($key\)/$value/g;	# replace $(var) with value
				}
#				print("$cpp -> $target\n");
				print("$target");
				exit;
			}
		}
	}
EOF
`
#echo "[$target]"
if [ -z "$target" ]; then echo "Error: failed to find build target for '$single_file'"; exit; fi
# end of parsing
fi

# build
# if $target is empty, whole project will be built, otherwise a single file
case "$PLATFORM" in
	"vc-win32")
		Make $makefile $target || exit 1
		[ $render -eq 1 ] && cp $root/libs/SDL2/x86/SDL2.dll .
		;;
	"vc-win64")
		Make $makefile $target || exit 1
		[ $render -eq 1 ] && cp $root/libs/SDL2/x64/SDL2.dll .
		;;
	"mingw32"|"cygwin")
		PATH=/bin:/usr/bin:$PATH					# configure paths for Cygwin
		gccfilt make -f $makefile $target || exit 1
		;;
	linux*)
		make -j 4 -f $makefile $target || exit 1	# use 4 jobs for build
		;;
	osx)
		make -f $makefile $target || exit 1
		;;
	*)
		echo "Unknown PLATFORM=\"$PLATFORM\""
		exit 1
esac
