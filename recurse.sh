#/bin/bash

#type="VertMesh"
#type="SkeletalMesh"
type="VertMesh|SkeletalMesh"
dump="-dump"
#dump="-check"
#path="data"
path="c:/games/unreal/ut2004"

for package in $path/Animations/*; do
	case "$package" in
	*.UKX|*.ukx|*.U|*.u)
		echo
		echo "***** Package: $package *****"
		./umodel.exe -list $package | grep -E "$type" | {
			# block with redirected stdin from grep
			while read line; do
				# cut number
				cut="${line#* }"
				# split to object name and class name
				obj="${cut#* }"
				class="${cut% *}"
#				echo "    OBJ=$obj   CLS=$class"
				./umodel.exe -path=$path $dump $package $obj $class
			done
		}
		;;
	*)
#		echo "Unknown: $package"
	esac
done
