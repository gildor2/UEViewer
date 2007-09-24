#/bin/bash

#type="VertMesh"
type="SkeletalMesh"
#dump="-dump"
find data -name *.$type -exec ./UnLoader $dump {} $type \;
