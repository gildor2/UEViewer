#!/bin/bash


# Parse the command line
ExeName=

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
		--exe)
			ExeName=$2
			shift 2
			;;
		--file)
			# compile a single file from VSCode, should replace slashes
			single_file=${2//\\//}
			shift 2
			;;
		*)
			echo "Usage: build.sh [--debug] [--profile] [--vc <version>] [--64] [--exe <file.exe>] [--file <cpp file>]"
			exit
			;;
	esac
done


function DebugPrint()
{
#	echo ">> Debug: $*"		# uncomment this line to see debug stuff
	:						# just allow commented 'echo' above
}

#-------------------------------------------------------------
# Setup default project when running this script directly. Used to build UmodelTool.

function SetupDefaultProject()
{
	is_default_project=1
	project="UmodelTool/umodel"
	root="."
	render=1
	if [ -z "$ExeName" ]; then
		ExeName="umodel"
		[ "$debug" ] && ExeName+="-debug"
		[ "$profile" ] && ExeName+="-profile"
		[ "$PLATFORM" == "vc-win64" ] && ExeName+="_64"
	fi
	GENMAKE_OPTIONS+=" EXE_NAME=$ExeName"
}

#-------------------------------------------------------------
# Get revision number from Git

function GetBuildNumber()
{
	local revision="(unversioned)"					# this value will be used in a case of missing git
	local version_file="UmodelTool/Version.h"
	if [ -d .git ]; then
		local git=`type -p git`						# equals to `which git`
		if [ -z "$git" ]; then
			if [ "$OSTYPE" == "msys" ]; then
				# assume Windows, find local git distribution
#				progs="${PROGRAMFILES//\\//}"		# get from environment with slash replacement
#				progs="/${progs//:/}"				# for msys: convert "C:/Program Files" to "/C/Program Files"
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
		DebugPrint "Git revision: $revision"
	fi

	# update tool version
	# read current revision
	[ -f "$version_file" ] && [ "$revision" ] && read last_revision < $version_file
	local last_revision=${last_revision##* }		# cut "#define ..."
	# write back to a file if value differs or if file doesn't exist (only for UModel project, i.e. when $project is empty)
	[ "$is_default_project" ] && [ "$last_revision" != "$revision" ] && echo "#define GIT_REVISION $revision" > $version_file
}

#-------------------------------------------------------------

function DetectBuildPlatform()
{
	# force PLATFORM=linux under Linux OS
	if [ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ]; then
		# PLATFORM="linux" - old case, now we'll recognize 32 or 64 bit OS for proper use of oodle.project
		if [ $(uname -m) == 'x86_64' ]; then
			PLATFORM="linux64"
		else
			PLATFORM="linux32"
		fi
	elif [[ "$OSTYPE" == "darwin"* ]]; then
		PLATFORM=osx
	else
		[ "$PLATFORM" ] || PLATFORM="vc-win32"
	fi
	DebugPrint "Detected platform: $PLATFORM"
}

#-------------------------------------------------------------
# We have some makefile dependency on Visual Studio compiler version, so we should detect it

function DetectVisualStudioVersion()
{
	# setup default compiler version
	[ "$vc_ver" ] || vc_ver=latest
	# Find Visual Studio
	. vc32tools $VC32TOOLS_OPTIONS --version=$vc_ver --check
	[ -z "$found_vc_year" ] && exit 1				# nothing found
	# Adjust vc_ver to found one
	vc_ver=$found_vc_year
	DebugPrint "Found: Visual Studio $found_vc_year at \"$workpath\", Version = $vc_ver"
	GENMAKE_OPTIONS+=" VC_VER=$vc_ver"				# specify compiler for genmake script
}

#-------------------------------------------------------------

function ProcessShaderFiles()
{
	# build shader includes before call to genmake
	if [ $render -eq 1 ]; then
		# 'cd' calls below won't work if we're not calling from the project's root
		if [ "$root" != "." ]; then
			echo "Bad 'root'"
			exit 1
		fi
		# build shaders
		#?? move this command to makefile
		Unreal/Shaders/make.pl
	fi
}

#-------------------------------------------------------------

function GenerateMakefile()
{
	# prepare makefile parameters, store in obj directory
	local projectName=${project##*/}
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
	DebugPrint "Using makefile: $makefile"

	# update makefile when needed
	# [ $makefile -ot $project ] &&
	$root/Tools/genmake $project.project TARGET=$PLATFORM $GENMAKE_OPTIONS > $makefile
}


#-------------------------------------------------------------
# Parse generated makefile to find an obj file which is built from provided c or cpp file $single_file.
# Function parameters are passed via global variables $makefile, $single_file

function FindSingleFileTarget()
{
	# Using perl with HEREDOC for parsing of makefile to find object file matching required target.
	# Code layout: target=`perl << 'EOF'
	# EOF
	# `
	# 1) using quoted 'EOF' to prevent variable expansion
	# 2) passing parameters to a script using command line, return value as output capture
	# 3) putting perl command into `` (inverse quotes)
	# 4) s/// command in perl code has extra quote for '$'

target=`perl -w - "$makefile" "$single_file" <<'EOF'
	my $makefile = $ARGV[0];
	my $single_file = $ARGV[1];
	open(FILE, $makefile) or die;
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
				next if $cpp ne $single_file;			# match with single_file value
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
	DebugPrint "SingleFile target: $target"
	if [ -z "$target" ]; then echo "Error: failed to find a build target for '$single_file'"; exit; fi
}

#-------------------------------------------------------------

[ "$project" ] || SetupDefaultProject
GetBuildNumber
DetectBuildPlatform

[ "${PLATFORM:0:3}" == "vc-" ] && DetectVisualStudioVersion

ProcessShaderFiles

GenerateMakefile

[ "$single_file" ] && FindSingleFileTarget

# Perform a build
# if $target is empty, whole project will be built, otherwise a single file
case "$PLATFORM" in
	"vc-win32"|"vc-win64")
		Make $makefile $target || exit 1
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
