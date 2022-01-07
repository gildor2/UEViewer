#!/bin/bash
archive="umodel_linux.tar.gz"
filelist="umodel readme.txt LICENSE.txt"

for i in $filelist; do
	if [ ! -f $i ]; then
		echo "ERROR: unable to find \"$i\""
		exit 1
	fi
done

rm -f $archive
tar -cf - $filelist | gzip -f9 > $archive
