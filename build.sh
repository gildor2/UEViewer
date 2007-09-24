#!/bin/bash

#export vc_ver=7
project="UnLoader.project"
makefile="makefile.mak"

# update makefile when needed
[ $makefile -ot $project ] &&
	Tools/genmake $project TARGET=vc-win32 > $makefile

# build
vc32tools --make $makefile
