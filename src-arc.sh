#!/bin/bash

# This tool will create archive with source code.
# All .svn dirs and "obj", "data", "bak" are excluded.

# Enable extended pattern matching (for (a|b|c)); see bash manpage, "Pattern matching".
shopt -s extglob

arc_name=source-${PWD##*/}.rar
skip_dirs=@(./data|./bak|./obj|./Screenshots)
skip_files=@(./notify.log|./umodel_win32.zip)

arccmd="rar a -r -n@ $arc_name"


function Recurse()
{
	local i j
	for i in $1/*; do
		if [ -f "$i" ]; then
			[[ "$i" == $skip_files ]] && continue;	# skip it
			j=${i//\//\\}							# replace slashes for rar
			echo "$j"
		elif [ -d "$i" ]; then
			[[ "$i" == $skip_dirs ]] && continue;	# skip it
			Recurse "$i"
		fi
	done
}


Recurse "." | $arccmd
