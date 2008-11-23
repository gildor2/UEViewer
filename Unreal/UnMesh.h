#ifndef __UNMESH_H__
#define __UNMESH_H__

#if DEUS_EX && !UNREAL1
#	error DEUS_EX requires UNREAL1
#endif


/*-----------------------------------------------------------------------------
	UPrimitive class
-----------------------------------------------------------------------------*/

class UPrimitive : public UObject
{
	DECLARE_CLASS(UPrimitive, UObject);
public:
	FBox			BoundingBox;
	FSphere			BoundingSphere;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UPrimitive::Serialize);
		Super::Serialize(Ar);
		Ar << BoundingBox << BoundingSphere;
		unguard;
	}
};


/*-----------------------------------------------------------------------------
	ULodMesh class and common data structures
-----------------------------------------------------------------------------*/

// Packed mesh vertex point for vertex meshes
#define GET_MESHVERT_DWORD(mv) (*(unsigned*)&(mv))

struct FMeshVert
{
	int X:11; int Y:11; int Z:10;

	friend FArchive& operator<<(FArchive &Ar, FMeshVert &V)
	{
		return Ar << GET_MESHVERT_DWORD(V);
	}
};


// Packed mesh vertex point for skinned meshes.
#define GET_MESHNORM_DWORD(mv) (*(unsigned*)&(mv))

struct FMeshNorm
{
	// non-normalized vector, to convert to float should use
	// (value - 512) / 512
	unsigned X:10; unsigned Y:10; unsigned Z:10;

	friend FArchive& operator<<(FArchive &Ar, FMeshNorm &V)
	{
		return Ar << GET_MESHNORM_DWORD(V);
	}
};


// corresponds to UT1 FMeshFloatUV
struct FMeshUV
{
	float			U;
	float			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUV &M)
	{
		return Ar << M.U << M.V;
	}
};


// UE1 FMeshUV
struct FMeshUV1
{
	byte			U;
	byte			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUV1 &M)
	{
		return Ar << M.U << M.V;
	}

	operator FMeshUV() const
	{
		FMeshUV r;
		r.U = U / 255.0f;
		r.V = V / 255.0f;
		return r;
	}
};


// LOD-style triangular polygon in a mesh, which references three textured vertices.
struct FMeshFace
{
	word			iWedge[3];				// Textured Vertex indices.
	word			MaterialIndex;			// Source Material (= texture plus unique flags) index.

	friend FArchive& operator<<(FArchive &Ar, FMeshFace &F)
	{
		Ar << F.iWedge[0] << F.iWedge[1] << F.iWedge[2];
		Ar << F.MaterialIndex;
		return Ar;
	}
};


// temp structure, skipped while mesh loading; used by UE1 UMesh only
struct FMeshTri
{
	word			iVertex[3];				// Vertex indices.
	FMeshUV1		Tex[3];					// Texture UV coordinates.
	unsigned		PolyFlags;				// Surface flags.
	int				TextureIndex;			// Source texture index.

	friend FArchive& operator<<(FArchive& Ar, FMeshTri &T)
	{
		Ar << T.iVertex[0] << T.iVertex[1] << T.iVertex[2];
		Ar << T.Tex[0] << T.Tex[1] << T.Tex[2];
		Ar << T.PolyFlags << T.TextureIndex;
		return Ar;
	}
};


// LOD-style Textured vertex struct. references one vertex, and
// contains texture U,V information.
// One triangular polygon in a mesh, which references three vertices,
// and various drawing/texturing information.
// Corresponds to UT1 FMeshExtWedge.
struct FMeshWedge
{
	word			iVertex;				// Vertex index.
	FMeshUV			TexUV;					// Texture UV coordinates.
	friend FArchive& operator<<(FArchive &Ar, FMeshWedge &T)
	{
		Ar << T.iVertex << T.TexUV;
		return Ar;
	}
};


// LOD-style mesh material.
struct FMeshMaterial
{
	unsigned		PolyFlags;				// Surface flags.
	int				TextureIndex;			// Source texture index.
	friend FArchive& operator<<(FArchive &Ar, FMeshMaterial &M)
	{
		return Ar << M.PolyFlags << M.TextureIndex;
	}
};


// An actor notification event associated with an animation sequence.
struct FMeshAnimNotify
{
	float			Time;					// Time to occur, 0.0-1.0.
	FName			Function;				// Name of the actor function to call.
	UObject			*NotifyObj;				//?? UAnimNotify

	friend FArchive& operator<<(FArchive &Ar, FMeshAnimNotify &N)
	{
		guard(FMeshAnimNotify<<);
		Ar << N.Time << N.Function;
#if SPLINTER_CELL
		if (Ar.IsSplinterCell)
		{
			FName Obj;
			Ar << Obj;						// instead of UObject*
		}
#endif
		if (Ar.ArVer >= 112)
			Ar << N.NotifyObj;
		return Ar;
		unguard;
	}
};


#if LINEAGE2

struct FLineageUnk2
{
	int						f0;
	int						f4;

	friend FArchive& operator<<(FArchive &Ar, FLineageUnk2 &S)
	{
		return Ar << S.f0 << S.f4;
	}
};


struct FLineageUnk3
{
	int						f0;
	TArray<FLineageUnk2>	f4;

	friend FArchive& operator<<(FArchive &Ar, FLineageUnk3 &S)
	{
		return Ar << S.f0 << S.f4;
	}
};


struct FLineageUnk4
{
	byte					f0;
	TArray<FLineageUnk2>	f4;
	TArray<FLineageUnk3>	f10;
	int						f1C;
	int						f20;
	TArray<FLineageUnk2>	f24;

	friend FArchive& operator<<(FArchive &Ar, FLineageUnk4 &S)
	{
		if (Ar.ArVer == 0x1A)
			return Ar << S.f4;
		else if (Ar.ArVer >= 0x1B)
			return Ar << S.f0 << S.f4 << S.f10 << S.f1C << S.f20 << S.f24;
		else
			return Ar;
	}
};

#endif


// Information about one animation sequence associated with a mesh,
// a group of contiguous frames.
struct FMeshAnimSeq
{
	FName					Name;			// Sequence's name.
	TArray<FName>			Groups;			// Group.
	int						StartFrame;		// Starting frame number.
	int						NumFrames;		// Number of frames in sequence.
	float					Rate;			// Playback rate in frames per second.
	TArray<FMeshAnimNotify> Notifys;		// Notifications.
	float					f28;			//?? unknown; default=0

	friend FArchive& operator<<(FArchive &Ar, FMeshAnimSeq &A)
	{
		guard(FMeshAnimSeq<<);
#if TRIBES3
		TRIBES_HDR(Ar, 0x17);
		if (Ar.IsTribes3 && t3_hdrV == 1)
		{
			int unk;
			Ar << unk;
		}
#endif
		if (Ar.ArVer >= 115)
			Ar << A.f28;
		Ar << A.Name;
#if UNREAL1
		if (Ar.ArVer < 100)
		{
			// UE1 support
			assert(Ar.IsLoading);
			FName tmpGroup;					// single group
			Ar << tmpGroup;
			if (strcmp(tmpGroup, "None") != 0)
				A.Groups.AddItem(tmpGroup);
		}
		else
#endif
		{
			Ar << A.Groups;					// array of groups
		}
		Ar << A.StartFrame << A.NumFrames << A.Notifys << A.Rate;
#if SPLINTER_CELL
		if (Ar.IsSplinterCell)
		{
			byte unk;
			Ar << unk;
		}
#endif
#if LINEAGE2
		if (Ar.IsLineage2 && Ar.ArLicenseeVer >= 1)
		{
			int				f2C, f30, f34, f3C, f40;
			UObject			*f38;
			FLineageUnk4	f44;
			Ar << f2C << f30;
			if (Ar.ArLicenseeVer >= 2)    Ar << f34;
			if (Ar.ArLicenseeVer >= 1)    Ar << f38;
			if (Ar.ArLicenseeVer >= 0x14) Ar << f3C;
			if (Ar.ArLicenseeVer >= 0x19) Ar << f40;
			if (Ar.ArLicenseeVer >= 0x1A) Ar << f44;
		}
#endif
		return Ar;
		unguard;
	}
};


// Base class for UVertMesh and USkeletalMesh; in Unreal Engine it is derived from
// abstract class UMesh (which is derived from UPrimitive)

/*
 * Possible versions:
 *	1	SplinterCell		LodMesh is same as UT, SkeletalMesh modified
 *	2	Postal 2			same as UT
 *	4	UT2003, UT2004
 *	5	Lineage2
 */

class ULodMesh : public UPrimitive
{
	DECLARE_CLASS(ULodMesh, UPrimitive);
public:
	unsigned			AuthKey;			// used in USkeletalMesh only?
	int					Version;			// see table above
	int					VertexCount;
	TArray<FMeshVert>	Verts;				// UVertMesh: NumFrames*NumVerts, USkeletalMesh: empty
	TArray<UMaterial*>	Textures;			// skins of mesh parts
	FVector				MeshScale;
	FVector				MeshOrigin;
	FRotator			RotOrigin;
	TArray<word>		FaceLevel;
	TArray<FMeshFace>	Faces;				// empty for USkeletalMesh
	TArray<word>		CollapseWedgeThus;	// ...
	TArray<FMeshWedge>	Wedges;				// ...
	TArray<FMeshMaterial> Materials;
	float				MeshScaleMax;
	float				LODStrength;
	int					LODMinVerts;
	float				LODMorph;
	float				LODZDisplace;
	float				LODHysteresis;
	// version 3 fields
	int					HasImpostor;		// sprite object to replace mesh when distance too far
	UMaterial*			SpriteMaterial;
	FVector				ImpScale;			// Impostor.Scale3D
	FRotator			ImpRotation;		// Impostor.RelativeRotation
	FVector				ImpLocation;		// Impostor.RelativeLocation
	FColor				ImpColor;
	int					ImpSpaceMode;		// EImpSpaceMode
	int					ImpDrawMode;		// EImpDrawMode
	int					ImpLightMode;		// EImpLightMode
	// version 4 fields
	float				SkinTesselationFactor; // LOD.SkinTesselationFactor

	virtual void Serialize(FArchive &Ar)
	{
		guard(ULodMesh::Serialize);
		Super::Serialize(Ar);

		Ar << Version << VertexCount << Verts;

		if (Version <= 1)
		{
			// skip FMeshTri section
			TArray<FMeshTri> tmp;
			Ar << tmp;
		}

		Ar << Textures << MeshScale << MeshOrigin << RotOrigin;

		if (Version <= 1)
		{
			// skip 2nd obsolete section
			TArray<word> tmp;
			Ar << tmp;
		}

		Ar << FaceLevel << Faces << CollapseWedgeThus << Wedges << Materials;
		Ar << MeshScaleMax << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;

		if (Version >= 3)
		{
			Ar << HasImpostor << SpriteMaterial;
			Ar << ImpLocation << ImpRotation << ImpScale << ImpColor;
			Ar << ImpSpaceMode << ImpDrawMode << ImpLightMode;
		}

		if (Version >= 4)
		{
			Ar << SkinTesselationFactor;
		}
#if LINEAGE2
		if (Version >= 5 && Ar.IsLineage2)
		{
			int unk;
			Ar << unk;
		}
#endif

		unguard;
	}

#if UNREAL1
	void SerializeLodMesh1(FArchive &Ar, TArray<FMeshAnimSeq> &AnimSeqs, TArray<FBox> &BoundingBoxes,
		TArray<FSphere> &BoundingSpheres, int &FrameCount);
#endif
};


/*-----------------------------------------------------------------------------
	Vertex streams
-----------------------------------------------------------------------------*/

struct FAnimMeshVertex
{
	// geometry data
	FVector			Pos;
	FVector			Norm;
	// texturing info
	FMeshUV			Tex;

	friend FArchive& operator<<(FArchive &Ar, FAnimMeshVertex &V)
	{
		return Ar << V.Pos << V.Norm << V.Tex;
	}
};


struct FRawIndexBuffer
{
	TArray<short>	Indices;
//	int64			CacheId;
	int				Revision;

	friend FArchive& operator<<(FArchive &Ar, FRawIndexBuffer &Buf)
	{
		return Ar << Buf.Indices << Buf.Revision;
	}
};


#if TRIBES3
struct T3_BasisVector
{
	FVector			v1;
	FVector			v2;

	friend FArchive& operator<<(FArchive &Ar, T3_BasisVector &V)
	{
		return Ar << V.v1 << V.v2;
	}
};
#endif

struct FSkinVertexStream
{
	//?? unknown
//	UObject*				f4, f8;
//	int64					CacheId;
	int						Revision;
	int						f18;
	int						f1C;
	TArray<FAnimMeshVertex>	Verts;

	friend FArchive& operator<<(FArchive &Ar, FSkinVertexStream &S)
	{
#if TRIBES3
		TRIBES_HDR(Ar, 0x11);
#endif
		Ar << S.Revision << S.f18 << S.f1C << S.Verts;
#if TRIBES3
		if (Ar.IsTribes3 && t3_hdrSV >= 1)
		{
			int unk1;
			TArray<T3_BasisVector> unk2;
			Ar << unk1 << unk2;
		}
#endif
		return Ar;
	}
};


#if 0
// old USkeletalMesh format
struct FAnimMeshVertexStream
{
	int						f4;			//?? unk
	TArray<FAnimMeshVertex>	Verts;
	int						f14, f18;	//?? unk
	int						Revision;
	int						f20, f24;	//?? unk
	int						PartialSize;
};
#endif


/*-----------------------------------------------------------------------------
	UVertMesh class
-----------------------------------------------------------------------------*/

class UVertMesh : public ULodMesh
{
	DECLARE_CLASS(UVertMesh, ULodMesh);
public:
	TArray<FMeshVert>		Verts2;			// empty; used ULodMesh.Verts
	TArray<FMeshNorm>		Normals;		// [NumFrames * NumVerts]
	TArray<float>			f150;			// empty?
	TArray<FMeshAnimSeq>	AnimSeqs;
	TArray<FBox>			BoundingBoxes;	// [NumFrames]
	TArray<FSphere>			BoundingSpheres;// [NumFrames]
	int						VertexCount;	// same as ULodMesh.VertexCount
	int						FrameCount;
	// FAnimVertexStream fields; used internally in UT, but serialized
	TArray<FAnimMeshVertex> AnimMeshVerts;	// empty; generated after loading
	int						StreamVersion;	// unused

	void BuildNormals();
#if UNREAL1
	void SerializeVertMesh1(FArchive &Ar);
#endif

	virtual void Serialize(FArchive &Ar)
	{
		guard(UVertMesh::Serialize);

#if UNREAL1
		if (Ar.ArVer < 100)
		{
			SerializeVertMesh1(Ar);
			RotOrigin.Roll = -RotOrigin.Roll;	//??
			return;
		}
#endif

		Super::Serialize(Ar);
		RotOrigin.Roll = -RotOrigin.Roll;		//??

		Ar << AnimMeshVerts << StreamVersion; // FAnimMeshVertexStream: may skip this (simply seek archive)
		Ar << Verts2 << f150;
		Ar << AnimSeqs << Normals;
		Ar << VertexCount << FrameCount;
		Ar << BoundingBoxes << BoundingSpheres;

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	UMeshAnimation class
-----------------------------------------------------------------------------*/

struct AnalogTrack
{
	unsigned		Flags;					// reserved
	TArray<FQuat>	KeyQuat;				// Orientation key track (count = 1 or KeyTime.Count)
	TArray<FVector>	KeyPos;					// Position key track (count = 1 or KeyTime.Count)
	TArray<float>	KeyTime;				// For each key, time when next key takes effect (measured from start of track)

#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif

	friend FArchive& operator<<(FArchive &Ar, AnalogTrack &A)
	{
		guard(AnalogTrack<<);
#if SPLINTER_CELL
		if (Ar.IsSplinterCell && Ar.ArLicenseeVer >= 0x0D)	// compressed Quat and Time tracks
		{
			A.SerializeSCell(Ar);
			return Ar;
		}
#endif // SPLINTER_CELL
		return Ar << A.Flags << A.KeyQuat << A.KeyPos << A.KeyTime;
		unguard;
	}
};


#if UNREAL25
void SerializeFlexTracks(FArchive &Ar, struct MotionChunk &M);
#endif
#if TRIBES3
void FixTribesMotionChunk(struct MotionChunk &M);
#endif

// Individual animation; subgroup of bones with compressed animation.
// Note: SplinterCell uses MotionChunkFixedPoint and MotionChunkFloat structures
struct MotionChunk
{
	FVector					RootSpeed3D;	// Net 3d speed.
	float					TrackTime;		// Total time (Same for each track.)
	int						StartBone;		// If we're a partial-hierarchy-movement, this is the lowest bone.
	unsigned				Flags;			// Reserved; equals to UMeshAnimation.Version in UE2.5

	TArray<int>				BoneIndices;	// Refbones number of Bone indices (-1 or valid one) to fast-find tracks for a particular bone.
	// Frame-less, compressed animation tracks. NumBones times NumAnims tracks in total
	TArray<AnalogTrack>		AnimTracks;		// Compressed key tracks (one for each bone)
	AnalogTrack				RootTrack;		// May or may not be used; actual traverse-a-scene root tracks for use
	// with cutscenes / special physics modes, in addition to the regular skeletal root track.

	friend FArchive& operator<<(FArchive &Ar, MotionChunk &M)
	{
		guard(MotionChunk<<);
		Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.BoneIndices << M.AnimTracks << M.RootTrack;
#if UNREAL25
		if (M.Flags >= 3)
			SerializeFlexTracks(Ar, M);
#endif
#if TRIBES3
		if (Ar.IsTribes3)
			FixTribesMotionChunk(M);
#endif
		return Ar;
		unguard;
	}
};


// Named bone for the animating skeleton data.
// Note: bone set may slightly differ in USkeletalMesh and UMeshAnimation objects (missing bones, different order)
// - should compute a map from one bone set to another; skeleton hierarchy should be taken from mesh (may differ
// too)
struct FNamedBone
{
	FName			Name;					// Bone's name (== single 32-bit index to name)
	unsigned		Flags;					// reserved
	int				ParentIndex;			// same meaning as FMeshBone.ParentIndex; when drawing model, should
											// use bone info from mesh, not from animation (may be different)

	friend FArchive& operator<<(FArchive &Ar, FNamedBone &F)
	{
		return Ar << F.Name << F.Flags << F.ParentIndex;
	}
};

/*
 * Possible versions:
 *	0			UT2003, UT2004
 *	1			Lineage2
 *	4			UE2Runtime, Harry Potter and the Prisoner of Azkaban
 *	6			Tribes3
 *	1000		SplinterCell
 *	2000		SplinterCell2
 */

class UMeshAnimation : public UObject
{
	DECLARE_CLASS(UMeshAnimation, UObject);
public:
	int						Version;		// always zero?
	TArray<FNamedBone>		RefBones;
	TArray<MotionChunk>		Moves;
	TArray<FMeshAnimSeq>	AnimSeqs;

#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif
#if UNREAL1
	void Upgrade();
#endif

#if LINEAGE2
	// serialize TRoughArray<MotionChunk> into TArray<MotionChunk>
	void SerializeLineageMoves(FArchive &Ar);
#endif

	virtual void Serialize(FArchive &Ar)
	{
		guard(UMeshAnimation.Serialize);
		Super::Serialize(Ar);
		if (Ar.ArVer >= 100)
			Ar << Version;					// no such field in UE1
#if LINEAGE2
		Ar << RefBones;
		if (!Ar.IsLineage2)
			Ar << Moves;
		else
			SerializeLineageMoves(Ar);
		Ar << AnimSeqs;
#else
		Ar << RefBones << Moves << AnimSeqs;
#endif
#if SPLINTER_CELL
		if (Ar.IsSplinterCell)
			SerializeSCell(Ar);
#endif
#if UNREAL1
		if (Ar.ArVer < 100) Upgrade();		// UE1 code
#endif
		unguard;
	}
};


/*-----------------------------------------------------------------------------
	USkeletalMesh class
-----------------------------------------------------------------------------*/

//
//	Bones
//

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat			Orientation;
	FVector			Position;
	float			Length;
	FVector			Size;

	friend FArchive& operator<<(FArchive &Ar, VJointPos &P)
	{
		return Ar << P.Orientation << P.Position << P.Length << P.Size;
	}
};


struct FMeshBone
{
	FName			Name;
	unsigned		Flags;
	VJointPos		BonePos;
	int				ParentIndex;			// 0 if this is the root bone.
	int				NumChildren;

	friend FArchive& operator<<(FArchive &Ar, FMeshBone &B)
	{
		return Ar << B.Name << B.Flags << B.BonePos << B.NumChildren << B.ParentIndex;
	}
};


// similar to VRawBoneInfluence, but used 'word' instead of 'int'
struct FVertInfluences
{
	float			Weight;
	word			PointIndex;
	word			BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, FVertInfluences &I)
	{
		return Ar << I.Weight << I.PointIndex << I.BoneIndex;
	}
};


struct VWeightIndex
{
	TArray<word>	BoneInfIndices;			// array of vertex indices (in Points array)
	int				StartBoneInf;			// start index in BoneInfluences array

	friend FArchive& operator<<(FArchive &Ar, VWeightIndex &I)
	{
		return Ar << I.BoneInfIndices << I.StartBoneInf;
	}
};


struct VBoneInfluence						// Weight and bone number
{
	word			BoneWeight;				// 0..65535 == 0..1
	word			BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, VBoneInfluence &V)
	{
		return Ar << V.BoneWeight << V.BoneIndex;
	}
};


//
//	Collision data
//

// MEPBonePrimSphere from MeshEditProps.uc
struct FSkelBoneSphere
{
	FName			BoneName;
	FVector			Offset;
	float			Radius;
#if UT2
	int				bBlockKarma;
	int				bBlockNonZeroExtent;
	int				bBlockZeroExtent;
#endif

	friend FArchive& operator<<(FArchive &Ar, FSkelBoneSphere &B)
	{
		Ar << B.BoneName << B.Offset << B.Radius;
#if UT2
		if (Ar.IsLoading)
		{
			B.bBlockKarma = B.bBlockNonZeroExtent = B.bBlockZeroExtent = 1;
		}
		if (Ar.IsUT2)
		{
			if (Ar.ArVer > 123) Ar << B.bBlockKarma;
			if (Ar.ArVer > 124)	Ar << B.bBlockZeroExtent << B.bBlockNonZeroExtent;
		}
#endif // UT2
		return Ar;
	}
};


// MEPBonePrimBox from MeshEditProps.uc
struct FSkelBoneBox
{
	FName			BoneName;
	FVector			Offset;
	FVector			Radii;
#if UT2
	int				bBlockKarma;
	int				bBlockNonZeroExtent;
	int				bBlockZeroExtent;
#endif

	friend FArchive& operator<<(FArchive &Ar, FSkelBoneBox &B)
	{
		Ar << B.BoneName << B.Offset << B.Radii;
#if UT2
		if (Ar.IsLoading)
		{
			B.bBlockKarma = B.bBlockNonZeroExtent = B.bBlockZeroExtent = 1;
		}
		if (Ar.IsUT2)
		{
			if (Ar.ArVer > 123) Ar << B.bBlockKarma;
			if (Ar.ArVer > 124) Ar << B.bBlockZeroExtent << B.bBlockNonZeroExtent;
		}
#endif // UT2
		return Ar;
	}
};


//
//	Mesh
//

// Textured triangle.
// This is an extended FMeshFace structure
struct VTriangle
{
	word			WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte			MatIndex;				// Materials can be anything.
	byte			AuxMatIndex;			// Second material (unused).
	unsigned		SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend FArchive& operator<<(FArchive &Ar, VTriangle &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
	}

	VTriangle& operator=(const FMeshFace &F)
	{
		WedgeIndex[0] = F.iWedge[0];
		WedgeIndex[1] = F.iWedge[1];
		WedgeIndex[2] = F.iWedge[2];
		MatIndex      = F.MaterialIndex;
	}
};


struct FSkinPoint
{
	FVector			Point;
	FMeshNorm		Normal;

	friend FArchive& operator<<(FArchive &Ar, FSkinPoint &P)
	{
		return Ar << P.Point << P.Normal;
	}
};


struct FSkelMeshSection
{
	word			MaterialIndex;
	word			MinStreamIndex;			// rigid section only
	word			MinWedgeIndex;
	word			MaxWedgeIndex;
	word			NumStreamIndices;		// rigid section only
//	word			fA;						// not serialized, not used
	word			BoneIndex;				// rigid section only
	word			fE;						// serialized, not used
	word			FirstFace;
	word			NumFaces;
#if LINEAGE2
	TArray<int>		LineageBoneMap;
#endif
	// Rigid sections:
	//	MinWedgeIndex/MaxWedgeIndex -> FStaticLODModel.Wedges
	//	NumStreamIndices = NumFaces*3 == MaxWedgeIndex-MinWedgeIndex+1
	//	MinStreamIndex/NumStreamIndices -> FStaticLODModel.StaticIndices.Indices
	//	MinWedgeIndex -> FStaticLODModel.VertexStream.Verts
	// Smooth sections:
	//	NumStreamIndices should contain MaxWedgeIndex-MinWedgeIndex+1 (new imported meshes?),
	//	but older package versions contains something different (should not rely on this field)
	//	Other fields are unitialized

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection &S)
	{
		Ar << S.MaterialIndex << S.MinStreamIndex << S.MinWedgeIndex << S.MaxWedgeIndex << S.NumStreamIndices;
		Ar << S.BoneIndex << S.fE << S.FirstFace << S.NumFaces;
#if LINEAGE2
		if (Ar.IsLineage2 && Ar.ArLicenseeVer >= 0x1C)
		{
			// bone map (local bone index -> mesh bone index)
			Ar << S.LineageBoneMap;
		}
#endif
		return Ar;
	}
};


#if LINEAGE2

struct FLineageWedge
{
	FVector			Point;
	FVector			Normal;				// note: length = 512
	FMeshUV			Tex;
	byte			Bones[4];
	float			Weights[4];

	friend FArchive& operator<<(FArchive &Ar, FLineageWedge &S)
	{
		return Ar << S.Point << S.Normal << S.Tex
				  << S.Bones[0] << S.Bones[1] << S.Bones[2] << S.Bones[3]
				  << S.Weights[0] << S.Weights[1] << S.Weights[2] << S.Weights[3];
	}
};

#endif


#if RAGNAROK2

struct FRag2FSkinGPUVertex
{
	FVector			f0;
	FVector			fC;
	float			f18[10];

	friend FArchive& operator<<(FArchive &Ar, FRag2FSkinGPUVertex &S)
	{
		Ar << S.f0 << S.fC;
		for (int i = 0; i < 10; i++) Ar << S.f18[i];
		return Ar;
	}
};

struct FRag2FSkinGPUVertexStream
{
	int				f14, f18, f1C;
	TArray<FRag2FSkinGPUVertex> f20;

	friend FArchive& operator<<(FArchive &Ar, FRag2FSkinGPUVertexStream &S)
	{
		return Ar << S.f14 << S.f18 << S.f1C << S.f20;
	}
};


struct FRag2Unk1
{
	word			f0;
	TArray<word>	f2;

	friend FArchive& operator<<(FArchive &Ar, FRag2Unk1 &S)
	{
		return Ar << S.f0 << S.f2;
	}
};

#endif // RAGNAROK2


struct FStaticLODModel
{
	TArray<unsigned>		SkinningData;		// floating stream format, contains U/V, weights etc
	TArray<FSkinPoint>		SkinPoints;			// smooth surface points
	int						NumDynWedges;		// number of wedges in smooth sections
	TArray<FSkelMeshSection> SmoothSections;
	TArray<FSkelMeshSection> RigidSections;
	FRawIndexBuffer			SmoothIndices;
	FRawIndexBuffer			RigidIndices;
	FSkinVertexStream		VertexStream;		// for rigid parts
	TLazyArray<FVertInfluences> VertInfluences;
	TLazyArray<FMeshWedge>	Wedges;
	TLazyArray<FMeshFace>	Faces;
	TLazyArray<FVector>		Points;				// all surface points
	float					LODDistanceFactor;
	float					LODHysteresis;
	int						NumSharedVerts;		// number of verts, which is used in more than one wedge
	int						LODMaxInfluences;
	int						f114;
	int						f118;
	// have another params: float ReductionError, int Coherence

#if LINEAGE2
	TArray<FLineageWedge>	LineageWedges;

	void RestoreLineageMesh();
#endif

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModel &M)
	{
		guard(FStaticLODModel<<);

#if TRIBES3
		TRIBES_HDR(Ar, 9);
#endif
		Ar << M.SkinningData << M.SkinPoints << M.NumDynWedges;
		Ar << M.SmoothSections << M.RigidSections << M.SmoothIndices << M.RigidIndices;
		Ar << M.VertexStream;
		Ar << M.VertInfluences << M.Wedges << M.Faces << M.Points;
		Ar << M.LODDistanceFactor << M.LODHysteresis << M.NumSharedVerts;
		Ar << M.LODMaxInfluences << M.f114 << M.f118;
#if TRIBES3
		if (Ar.IsTribes3 && t3_hdrSV >= 1)
		{
			TLazyArray<T3_BasisVector> unk1;
			TArray<T3_BasisVector> unk2;
			Ar << unk1 << unk2;
		}
#endif
#if LINEAGE2
		if (Ar.IsLineage2 && Ar.ArLicenseeVer >= 0x1C)
		{
			int UseNewWedges;
			Ar << UseNewWedges << M.LineageWedges;
			M.RestoreLineageMesh();
		}
#endif
#if RAGNAROK2
		if (Ar.IsRagnarok2 && Ar.ArVer >= 0x80)
		{
			int tmp;
			FRawIndexBuffer unk2;
			FRag2FSkinGPUVertexStream unk3;
			TArray<FRag2Unk1> unk4;
			// TArray of word[9]
			Ar << AR_INDEX(tmp);
			Ar.Seek(Ar.ArPos + tmp*9*2);
			// other ...
			Ar << unk2 << unk3 << unk4;
		}
#endif // RAGNAROK2
		return Ar;

		unguard;
	}
};


// old USkeletalMesh
struct FLODMeshSection
{
	short					f0, f2, f4;
	short					LastIndex;
	short					f8, fA, fC;
	short					iFace;
	short					f10;
//	FAnimMeshVertexStream	VertStream;
//	FRawIndexBuffer			IndexBuffer;

	friend FArchive& operator<<(FArchive &Ar, FLODMeshSection &S)
	{
		return Ar << S.f0 << S.f2 << S.f4 << S.LastIndex <<
					 S.f8 << S.fA << S.fC << S.iFace << S.f10;
	}
};


#if TRIBES3

struct FT3Unk1
{
	short		f0[3];
	byte		f1[2];
	int			f2;

	friend FArchive& operator<<(FArchive &Ar, FT3Unk1 &V)
	{
		return Ar << V.f0[0] << V.f0[1] << V.f0[2] << V.f1[0] << V.f1[1] << V.f2;
	}
};

#endif // TRIBES3


class USkeletalMesh : public ULodMesh
{
	DECLARE_CLASS(USkeletalMesh, ULodMesh);
public:
	TLazyArray<FVector>		Points;			// note: have ULodMesh.Verts
	TLazyArray<FMeshWedge>	Wedges;			// note: have ULodMesh.Wedges
	TLazyArray<VTriangle>	Triangles;
	TLazyArray<FVertInfluences> VertInfluences;
	TLazyArray<word>		CollapseWedge;	// Num == Wedges.Num; used to automatically build StaticLODModels
	TLazyArray<word>		f1C8;			// Num == Triangles.Num, element = index in Points array; not used?
	TArray<FMeshBone>		RefSkeleton;
	int						SkeletalDepth;
	TArray<FStaticLODModel>	LODModels;
	TArray<FVector>			Points2;		// used for Version=1 only
	TArray<VWeightIndex>	WeightIndices;	// empty
	TArray<VBoneInfluence>	BoneInfluences;
	UMeshAnimation*			Animation;
	UObject*				f224;			//?? always NULL (seen in IDA)
	// AttachSocket data
	TArray<FName>			AttachAliases;
	TArray<FName>			AttachBoneNames;
	TArray<FCoords>			AttachCoords;
	// collision data
	TArray<FSkelBoneSphere>	BoundingSpheres;
	TArray<FSkelBoneBox>	BoundingBoxes;
	TArray<UObject*>		f32C;			// TArray<UModel*>; collision models??
	UObject*				CollisionMesh;	// UStaticMesh*
	UObject*				KarmaProps;		// UKMeshProps*

	void UpgradeFaces();
	void UpgradeMesh();
#if UNREAL1
	void SerializeSkelMesh1(FArchive &Ar);
#endif
#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif
#if LINEAGE2
	void RecreateMeshFromLOD();
#endif

	virtual void Serialize(FArchive &Ar)
	{
		guard(USkeletalMesh::Serialize);

#if UNREAL1
		if (Ar.ArVer < 100)
		{
			SerializeSkelMesh1(Ar);
			return;
		}
#endif

		Super::Serialize(Ar);
#if TRIBES3
		TRIBES_HDR(Ar, 4);
#endif
		Ar << Points2 << RefSkeleton << Animation;
		Ar << SkeletalDepth << WeightIndices << BoneInfluences;
		Ar << AttachAliases << AttachBoneNames << AttachCoords;
		if (Version <= 1)
		{
#if SPLINTER_CELL
			if (Ar.IsSplinterCell)
			{
				SerializeSCell(Ar);
			}
			else
#endif
			{
				appNotify("SkeletalMesh of version %d\n", Version);
				TArray<FLODMeshSection> tmp1, tmp2;
				TArray<word> tmp3;
				Ar << tmp1 << tmp2 << tmp3;
			}
			// copy and convert data from old mesh format
			UpgradeMesh();
		}
		else
		{
			Ar << LODModels << f224 << Points << Wedges << Triangles << VertInfluences;
			Ar << CollapseWedge << f1C8;
		}

#if TRIBES3
		if (Ar.IsTribes3 && t3_hdrSV >= 3)
		{
	#if 0
			// it looks like format of following data was chenged sinse
			// data was prepared, and game executeble does not load these
			// LazyArrays (otherwise error should occur) -- so we are
			// simply skipping these arrays
			TLazyArray<FT3Unk1>    unk1;
			TLazyArray<FMeshWedge> unk2;
			TLazyArray<word>       unk3;
			Ar << unk1 << unk2 << unk3;
	#else
			SkipLazyArray(Ar);
			SkipLazyArray(Ar);
			SkipLazyArray(Ar);
	#endif
			// nothing interesting below ...
			Ar.Seek(Ar.ArStopper);
			return;
		}
#endif

#if LINEAGE2
		if (Ar.IsLineage2)
		{
			int unk1, unk3, unk4;
			TArray<int> unk2;
			if (Ar.ArVer >= 118 && Ar.ArLicenseeVer >= 3)
				Ar << unk1;
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x12)
				Ar << unk2;
			if (Ar.ArVer >= 120)
				Ar << unk3;		// AuthKey ?
			if (Ar.ArLicenseeVer >= 0x23)
				Ar << unk4;
			RecreateMeshFromLOD();
			return;
		}
#endif

		if (Ar.ArVer >= 120)
		{
			Ar << AuthKey;
		}

#if UT2
		if (Ar.IsUT2)
		{
			// UT2004 has branched version of UE2, which is slightly different
			// in comparison with generic UE2, which is used in all other UE2 games.
			if (Ar.ArVer >= 122)
				Ar << KarmaProps << BoundingSpheres << BoundingBoxes << f32C;
			if (Ar.ArVer >= 127)
				Ar << CollisionMesh;
			return;
		}
#endif // UT2

		// generic UE2 code
		if (Ar.ArVer >= 124)
			Ar << KarmaProps << BoundingSpheres << BoundingBoxes;
		if (Ar.ArVer >= 125)
			Ar << f32C;

#if RAGNAROK2
		if (Ar.IsRagnarok2 && Ar.ArVer >= 131)
		{
			float unk1, unk2;
			Ar << unk1 << unk2;
		}
#endif

		unguard;
	}
};


#if RUNE

class USkelModel : public UPrimitive
{
	DECLARE_CLASS(USkelModel, UPrimitive);
public:
	// transient data
	TArray<USkeletalMesh*>	Meshes;
	TArray<char*>			MeshNames;
	UMeshAnimation			*Anim;
	virtual ~USkelModel();

	virtual void Serialize(FArchive &Ar);
};

#endif // RUNE


#define REGISTER_MESH_CLASSES		\
	REGISTER_CLASS(USkeletalMesh)	\
	REGISTER_CLASS(UVertMesh)		\
	REGISTER_CLASS(UMeshAnimation)

// Note: we have registered UVertMesh and UMesh as ULodMesh too for UE1 compatibility
#define REGISTER_MESH_CLASSES_U1	\
	REGISTER_CLASS_ALIAS(UVertMesh, UMesh) \
	REGISTER_CLASS_ALIAS(UVertMesh, ULodMesh) \
	REGISTER_CLASS_ALIAS(UMeshAnimation, UAnimation)

#define REGISTER_MESH_CLASSES_RUNE	\
	REGISTER_CLASS(USkelModel)


#endif // __UNMESH_H__
