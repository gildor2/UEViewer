#/bin/bash

#type="VertMesh"
#type="SkeletalMesh"
type="VertMesh|SkeletalMesh"
dump="-dump"

for package in data/*; do
	case "$package" in
	*.UKX|*.ukx|*.U|*.u)
		echo
		echo "***** Package: $package *****"
		./UnLoader.exe -list $package | grep -E "$type" | {
			# block with redirected stdin from grep
			while read line; do
				obj="${line##* }"
				./UnLoader.exe $dump $package $obj
			done
		}
		;;
	*)
#		echo "Unknown: $package"
	esac
done
