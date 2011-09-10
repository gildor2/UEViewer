#!/bin/bash

#srcDir=E:/UDK/UDK-2011-06/Engine/Shaders/Binaries
#srcDir=E:/Projects/WarShipBattle/Engine/Shaders/Binaries
srcDir=binaries
dstDir=decoded

mkdir $dstDir

for src in $srcDir/*.bin; do
	dst=${src##*/}
	dst=${dst%.bin}

	./main.exe $src $dstDir/$dst.usf
done
