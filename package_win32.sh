#!/bin/bash
archive="umodel_win32.zip"
filelist="umodel.exe umodel_64.exe readme.txt SDL2.dll SDL2_64.dll LICENSE.txt"

# Build 32 and 64 bit executables
./build.sh || exit 1
./build.sh --64 || exit 1

if grep -q -E "(PRIVATE BUILD)" umodel.exe; then
	echo "ERROR: this is a private build"
	exit
fi

if grep -q -E "(DEBUG BUILD)" umodel.exe; then
	echo "ERROR: this is a debug build"
	exit
fi

# Copy SDL2.dll locally
cp libs/SDL2/x86/SDL2.dll .
cp libs/SDL2/x64/SDL2.dll ./SDL2_64.dll

# Verify for presence of all files
for i in $filelist; do
	if [ ! -f $i ]; then
		echo "ERROR: unable to find \"$i\""
		exit 1
	fi
done

# Create an archive
rm -f $archive
pkzipc -add $archive -level=9 $filelist

# Remove SDL2.dll files, these are required only for packaging
rm -f SDL2.dll SDL2_64.dll
