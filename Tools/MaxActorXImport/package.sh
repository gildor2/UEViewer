#!/bin/bash
archive="ActorXImporter.zip"
filelist="*.ms"

for i in $filelist; do
	if [ ! -f $i ]; then
		echo "ERROR: unable to find \"$i\""
		exit 1
	fi
done

rm -f $archive
pkzipc -add $archive -level=9 $filelist
