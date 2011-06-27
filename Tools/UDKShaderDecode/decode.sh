#!/bin/bash

#srcDir=E:/UDK/Engine/Shaders/Binaries
srcDir=E:/Projects/WarShipBattle/Engine/Shaders/Binaries
dstDir=decoded

mkdir $dstDir

for src in $srcDir/*.bin; do
	dst=${src##*/}
	dst=${dst%.bin}

	./main.exe $src $dstDir/$dst.usf
done
