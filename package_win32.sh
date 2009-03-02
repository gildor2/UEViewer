#!/bin/bash
rm -f umodel_win32.zip

if grep umodel.exe -e XMemDecompress > /dev/null; then
	echo "ERROR: this is a private build"
	exit
fi

pkzipc -add umodel_win32.zip -level=9 umodel.exe readme.txt SDL.dll
