#ifndef __PSK_H__
#define __PSK_H__


/******************************************************************************
 *	Common structures
 *****************************************************************************/

struct VChunkHeader
{
	char			ChunkID[20];			// text identifier
	int				TypeFlag;				// version: 1999801 or 2003321
	int				DataSize;				// sizeof(type)
	int				DataCount;				// number of array elements

	friend FArchive& operator<<(FArchive &Ar, VChunkHeader &H)
	{
		Ar.Serialize(ARRAY_ARG(H.ChunkID));
		return Ar << H.TypeFlag << H.DataSize << H.DataCount;
	}
};


#define LOAD_CHUNK(var, name)	\
	VChunkHeader var;			\
	Ar << var;					\
	if (strcmp(var.ChunkID, name) != 0) \
		appError("%08X: LoadChunk: expecting header \"%s\", but found \"%s\"", Ar.Tell(), name, var.ChunkID);
//	appPrintf("%s: type=%d / count=%d / size=%d\n", var.ChunkID, var.TypeFlag, var.DataCount, var.DataSize);

#define SAVE_CHUNK(var, name)	\
	strcpy(var.ChunkID, name);	\
	var.TypeFlag = 1999801;		\
	Ar << var;
//	appPrintf("%08X: %s: type=%d / count=%d / size=%d\n", Ar.Tell(), var.ChunkID, var.TypeFlag, var.DataCount, var.DataSize);


/******************************************************************************
 *	PSK file format structures
 *****************************************************************************/

// NOTE: VJointPos and VTriangle were duplicated here to remove UnMesh.h and archive version dependency

struct VVertex
{
	int				PointIndex;				// int16, padded to int; used as int for large meshes
	float			U, V;
	byte			MatIndex;
	byte			Reserved;
	int16			Pad;					// not used

	friend FArchive& operator<<(FArchive &Ar, VVertex &V)
	{
		Ar << V.PointIndex << V.U << V.V << V.MatIndex << V.Reserved << V.Pad;
		if (Ar.IsLoading)
			V.PointIndex &= 0xFFFF;			// clamp padding bytes
		return Ar;
	}
};


// Textured triangle.
// This is a copy of UnMesh.h VTriangle
struct VTriangle16
{
	uint16			WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte			MatIndex;				// Materials can be anything.
	byte			AuxMatIndex;			// Second material (unused).
	unsigned		SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend FArchive& operator<<(FArchive &Ar, VTriangle16 &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
	}
};


struct VMaterial
{
	char			MaterialName[64];
	int				TextureIndex;
	unsigned		PolyFlags;
	int				AuxMaterial;
	unsigned		AuxFlags;
	int				LodBias;
	int				LodStyle;

	friend FArchive& operator<<(FArchive &Ar, VMaterial &M)
	{
		Ar.Serialize(ARRAY_ARG(M.MaterialName));
		return Ar << M.TextureIndex << M.PolyFlags << M.AuxMaterial <<
					 M.AuxFlags << M.LodBias << M.LodStyle;
	}
};


// A bone: an orientation, and a position, all relative to their parent.
// This is a copy of UnMesh.h VJointPos with removed UE3 serializing code
struct VJointPosPsk
{
	FQuat			Orientation;
	FVector			Position;
	float			Length;
	FVector			Size;

	friend FArchive& operator<<(FArchive &Ar, VJointPosPsk &P)
	{
		return Ar << P.Orientation << P.Position << P.Length << P.Size;
	}
};


struct VBone
{
	char			Name[64];
	unsigned		Flags;
	int				NumChildren;
	int				ParentIndex;			// 0 if this is the root bone.
	VJointPosPsk	BonePos;

	friend FArchive& operator<<(FArchive &Ar, VBone &B)
	{
		Ar.Serialize(ARRAY_ARG(B.Name));
		return Ar << B.Flags << B.NumChildren << B.ParentIndex << B.BonePos;
	}
};


struct VRawBoneInfluence
{
	float			Weight;
	int				PointIndex;
	int				BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, VRawBoneInfluence &I)
	{
		return Ar << I.Weight << I.PointIndex << I.BoneIndex;
	}
};


struct VMeshUV
{
	float			U;
	float			V;

	friend FArchive& operator<<(FArchive &Ar, VMeshUV &V)
	{
		return Ar << V.U << V.V;
	}
};


/******************************************************************************
 *	PSKX extended information
 *****************************************************************************/

// The same as VTriangle16 but with 32-bit vertex indices.
// NOTE: this structure has different on-disk and in-memory layout and size (due to alignment).
struct VTriangle32
{
	int				WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte			MatIndex;				// Materials can be anything.
	byte			AuxMatIndex;			// Second material (unused).
	unsigned		SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend FArchive& operator<<(FArchive &Ar, VTriangle32 &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
	}
};


/******************************************************************************
 *	PSA file format structures
 *****************************************************************************/

// Binary bone format to deal with raw animations as generated by various exporters.
struct FNamedBoneBinary
{
	char			Name[64];				// Bone's name
	unsigned		Flags;					// reserved
	int				NumChildren;
	int				ParentIndex;			// 0/NULL if this is the root bone.
	VJointPosPsk	BonePos;

	friend FArchive& operator<<(FArchive &Ar, FNamedBoneBinary &B)
	{
		Ar.Serialize(ARRAY_ARG(B.Name));
		return Ar << B.Flags << B.NumChildren << B.ParentIndex << B.BonePos;
	}
};


// Binary animation info format - used to organize raw animation keys into FAnimSeqs on rebuild
// Similar to MotionChunkDigestInfo.
struct AnimInfoBinary
{
	char			Name[64];				// Animation's name
	char			Group[64];				// Animation's group name

	int				TotalBones;				// TotalBones * NumRawFrames is number of animation keys to digest.

	int				RootInclude;			// 0 none 1 included (unused)
	int				KeyCompressionStyle;	// Reserved: variants in tradeoffs for compression.
	int				KeyQuotum;				// Max key quotum for compression; ActorX sets this to numFrames*numBones
	float			KeyReduction;			// desired
	float			TrackTime;				// explicit - can be overridden by the animation rate
	float			AnimRate;				// frames per second.
	int				StartBone;				// Reserved: for partial animations
	int				FirstRawFrame;			// global number of first animation frame
	int				NumRawFrames;			// NumRawFrames and AnimRate dictate tracktime...

	friend FArchive& operator<<(FArchive &Ar, AnimInfoBinary &A)
	{
		Ar.Serialize(ARRAY_ARG(A.Name));
		Ar.Serialize(ARRAY_ARG(A.Group));
		return Ar << A.TotalBones << A.RootInclude << A.KeyCompressionStyle <<
					 A.KeyQuotum << A.KeyReduction << A.TrackTime << A.AnimRate <<
					 A.StartBone << A.FirstRawFrame << A.NumRawFrames;
	}
};


struct VQuatAnimKey
{
	FVector			Position;				// Relative to parent
	FQuat			Orientation;			// Relative to parent
	float			Time;					// The duration until the next key (end key wraps to first ...)

	friend FArchive& operator<<(FArchive &Ar, VQuatAnimKey &K)
	{
		return Ar << K.Position << K.Orientation << K.Time;
	}
};


#endif // __PSK_H__
