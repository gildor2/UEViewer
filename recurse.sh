#/bin/bash

#type="VertMesh"
#type="SkeletalMesh"
type="VertMesh|SkeletalMesh"
#dump="-dump"
dump="-check"

for package in data/*; do
	case "$package" in
	*.UKX|*.ukx|*.U|*.u)
		echo
		echo "***** Package: $package *****"
		./UnLoader.exe -list $package | grep -E "$type" | {
			# block with redirected stdin from grep
			while read line; do
				# cut number
				cut="${line#* }"
				# split to object name and class name
				obj="${cut#* }"
				class="${cut% *}"
#				echo "    OBJ=$obj   CLS=$class"
				./UnLoader.exe $dump $package $obj $class
			done
		}
		;;
	*)
#		echo "Unknown: $package"
	esac
done
