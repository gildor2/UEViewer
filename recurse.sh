#/bin/bash

#type="VertMesh"
type="SkeletalMesh"
#type="VertMesh|SkeletalMesh|LodMesh"
#dump="-dump"
dump="-check"
path="data"

# UT1
#path="data/UT1"

# UT2004
#path="c:/games/Unreal/ut2004"
#path="c:/games/Unreal Anthology/ut2004"

# SplinterCell
#path="data/SplinterCell"

# SplinterCell2
#path="data/SplinterCell2"

path2=$path
[ -d "$path/Animations" ] && path2="$path/Animations"

for package in "$path2/"*; do
	case "$package" in
	*.UKX|*.ukx|*.U|*.u)
		echo
		echo "***** Package: $package *****"
		./umodel.exe -list "$package" | grep -E "$type" | {
			# block with redirected stdin from grep
			while read line; do
				# cut number
				cut="${line#* }"
				# split to object name and class name
				obj="${cut#* }"
				class="${cut% *}"
#				echo "    OBJ=$obj   CLS=$class"
				./umodel.exe -path="$path" $dump "$package" $obj $class
			done
		}
		;;
	*)
#		echo "Unknown: $package"
	esac
done
