#!/bin/bash
archive="umodel_win32.zip"
filelist="umodel.exe readme.txt SDL2.dll"

for i in $filelist; do
	if [ ! -f $i ]; then
		echo "ERROR: unable to find \"$i\""
		exit 1
	fi
done

if grep -q -E "(PRIVATE BUILD)" umodel.exe; then
	echo "ERROR: this is a private build"
	exit
fi

rm -f $archive
pkzipc -add $archive -level=9 $filelist
