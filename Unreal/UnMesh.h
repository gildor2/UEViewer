#ifndef __UNMESH_H__
#define __UNMESH_H__

//?? either remove this line or make analogous lines for other targets
#if DEUS_EX && !UNREAL1
#	error DEUS_EX requires UNREAL1
#endif

/*-----------------------------------------------------------------------------
UE1 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		UPrimitive
			UMesh
				ULodMesh
					USkeletalMesh
			USkelModel (Rune)

UE2 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		UPrimitive
			ULodMesh
				USkeletalMesh
				UVertMesh

UE3 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		USkeletalMesh

-----------------------------------------------------------------------------*/


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

#if UNREAL3

struct FBoxSphereBounds
{
	FVector			Origin;
	FVector			BoxExtent;
	float			SphereRadius;

	friend FArchive& operator<<(FArchive &Ar, FBoxSphereBounds &B)
	{
		return Ar << B.Origin << B.BoxExtent << B.SphereRadius;
	}
};

#endif // UNREAL3


/*-----------------------------------------------------------------------------
	ULodMesh class and common data structures
-----------------------------------------------------------------------------*/

// Packed mesh vertex point for vertex meshes
#define GET_DWORD(v) (*(unsigned*)&(v))

struct FMeshVert
{
	int X:11; int Y:11; int Z:10;

	friend FArchive& operator<<(FArchive &Ar, FMeshVert &V)
	{
		return Ar << GET_DWORD(V);
	}
};

SIMPLE_TYPE(FMeshVert, unsigned)


struct FMeshNorm
{
	// non-normalized vector, to convert to float should use
	// (value - 512) / 512
	unsigned X:10; unsigned Y:10; unsigned Z:10;

	friend FArchive& operator<<(FArchive &Ar, FMeshNorm &V)
	{
		return Ar << GET_DWORD(V);
	}
};

SIMPLE_TYPE(FMeshNorm, unsigned)


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

SIMPLE_TYPE(FMeshUV, float)


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

SIMPLE_TYPE(FMeshUV1, byte)


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

SIMPLE_TYPE(FMeshFace, word)


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

//RAW_TYPE(FMeshWedge) -- not raw type because of alignment between iVertex and TexUV


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
		if (Ar.ArLicenseeVer == 0x1A)
			return Ar << S.f4;
		else if (Ar.ArLicenseeVer >= 0x1B)
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
		if (Ar.ArVer < PACKAGE_V2)
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
 *	5	Lineage2, UC2		different extensions
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

SIMPLE_TYPE(FAnimMeshVertex, float)


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
#endif // TRIBES3
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
		if (Ar.ArVer < PACKAGE_V2)
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
#if UNREAL3
	TArray<float>	KeyQuatTime;			// not serialized, used for UE3 animations with separate time tracks for
	TArray<float>	KeyPosTime;				//	position and rotation channels
#endif

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
#if SPLINTER_CELL
		if (Ar.IsSplinterCell)				// possibly M.Flags != 0, skip FlexTrack serializer
			return Ar;
#endif
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
		Ar << F.Name << F.Flags << F.ParentIndex;
#if UC2
		if (Ar.ArVer >= 145) // IsUC2()
		{
			FVector unused1;				// strange code: serialized into stack and dropped
			byte    unused2;
			if (Ar.ArVer >= 130) Ar << unused1;
			if (Ar.ArVer >= 132) Ar << unused2;
		}
#endif // UC2
		return Ar;
	}
};

/*
 * Possible versions:
 *	0			UT2003, UT2004
 *	1			Lineage2
 *	4			UE2Runtime, Harry Potter and the Prisoner of Azkaban
 *	6			Tribes3, Bioshock
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
		if (Ar.ArVer >= PACKAGE_V2)
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
		if (Ar.ArVer < PACKAGE_V2) Upgrade();		// UE1 code
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
#if UNREAL3
		if (Ar.ArVer >= 224)
			return Ar << P.Orientation << P.Position;
#endif
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
		Ar << B.Name << B.Flags << B.BonePos << B.NumChildren << B.ParentIndex;
#if UNREAL3
		if (Ar.ArVer >= 515 && Ar.ArVer != 522)	//?? note: 522 check is in Mirror's Edge, no in GoW2
		{
			int unk44;						// byte[4] ? default is 0xFF x 4
			Ar << unk44;
		}
#endif
		return Ar;
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

RAW_TYPE(FVertInfluences)


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

SIMPLE_TYPE(VBoneInfluence, word)


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
		return *this;
	}
};

RAW_TYPE(VTriangle)

struct FSkinPoint
{
	FVector			Point;
	FMeshNorm		Normal;

	friend FArchive& operator<<(FArchive &Ar, FSkinPoint &P)
	{
		return Ar << P.Point << P.Normal;
	}
};

RAW_TYPE(FSkinPoint)

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
	TArray<int>		LineageBoneMap;			// for smooth sections only
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

// FAnimMeshVertex with influence info
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

RAW_TYPE(FLineageWedge)

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


#if UC2

struct FUC2Vector1
{
	short			X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FUC2Vector1 &V)
	{
		if (Ar.ArLicenseeVer == 1)
			return Ar << V.X << V.Y << V.Z;
		// LicenseeVer == 0 -- serialize as int[3] and rescale, using some global scale
		int X, Y, Z;
		return Ar << X << Y << Z;
	}
};

struct FUC2Vector2
{
	int				X:11;
	int				Y:10;
	int				Z:11;

	friend FArchive& operator<<(FArchive &Ar, FUC2Vector2 &V)
	{
		if (Ar.ArLicenseeVer == 1)
			return Ar << GET_DWORD(V);
		// LicenseeVer == 0 -- serialize as int[3] and rescale by consts
		int X, Y, Z;
		return Ar << X << Y << Z;
	}
};

struct FUC2Int
{
	short			V;

	friend FArchive& operator<<(FArchive &Ar, FUC2Int &V)
	{
		if (Ar.ArLicenseeVer == 1)
			return Ar << V.V;
		// LicenseeVer == 0 -- serialize as int and rescale by consts
		int X;
		return Ar << X;
	}
};


struct FUC2Unk3
{
	FUC2Vector1		f0;
	FUC2Vector2		f6;
	FUC2Int			fA, fC;

	friend FArchive& operator<<(FArchive &Ar, FUC2Unk3 &S)
	{
		Ar << S.f0 << S.f6 << S.fA << S.fC;
		if (Ar.ArLicenseeVer == 1)
		{
			byte b[8];
			Ar << b[0] << b[1] << b[2] << b[3] << b[4] << b[5] << b[6] << b[7];
		}
		else
		{
			int i[8];
			Ar << i[0] << i[1] << i[2] << i[3] << i[4] << i[5] << i[6] << i[7];
		}
		return Ar;
	}
};

struct FUC2Unk1
{
	int				f14;
	int				f18;
	int				f1C;
	// some of TArray<> variations, allocated in GPU memory (?)
	int				SomeFlag;
	TArray<FUC2Unk3> f20;

	friend FArchive& operator<<(FArchive &Ar, FUC2Unk1 &S)
	{
		Ar << S.f14 << S.f18 << S.f1C;
		S.SomeFlag = 0;
		if (Ar.ArLicenseeVer == 1)
			Ar << S.SomeFlag;
		if (S.SomeFlag == 0)
			Ar << S.f20;
		return Ar;
	}
};


struct FUC2Unk2
{
	word			f0;
	TArray<word>	f4;

	friend FArchive& operator<<(FArchive &Ar, FUC2Unk2 &S)
	{
		return Ar << S.f0 << S.f4;
	}
};


#endif // UC2


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
#if UNREAL3
	void RestoreMesh3(const class USkeletalMesh &Mesh, const class FStaticLODModel3 &Lod, const struct FSkeletalMeshLODInfo &Info);	//?? forward declarations for classes
#endif
#if BIOSHOCK
	void RestoreMeshBio(const USkeletalMesh &Mesh, const struct FStaticLODModelBio &Lod);
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
#endif // TRIBES3
#if LINEAGE2
		if (Ar.IsLineage2 && Ar.ArLicenseeVer >= 0x1C)
		{
			int UseNewWedges;
			Ar << UseNewWedges << M.LineageWedges;
			M.RestoreLineageMesh();
		}
#endif // LINEAGE2
#if RAGNAROK2
		if (Ar.IsRagnarok2 && Ar.ArVer >= 128)
		{
			int tmp;
			FRawIndexBuffer unk2;
			FRag2FSkinGPUVertexStream unk3;
			TArray<FRag2Unk1> unk4;
			// TArray of word[9]
			Ar << AR_INDEX(tmp);
			Ar.Seek(Ar.Tell() + tmp*9*2);
			// other ...
			Ar << unk2 << unk3 << unk4;
		}
#endif // RAGNAROK2
#if UC2
		if (Ar.ArVer >= 145) //?? IsUC2
		{
			if (Ar.ArVer >= 136)
			{
				FVector f17C, f188, f194;
				Ar << f17C << f188 << f194;
			}
			if (Ar.ArVer >= 127)
			{
				TArray<FSkelMeshSection> f98;
				TArray<word>		fB0;
				int					fBC;
				FUC2Unk1			fC0;
				TArray<FUC2Unk2>	fEC;
				Ar << f98 << fB0 << fBC << fC0;
				if (Ar.ArVer >= 128)
					Ar << fEC;
			}
		}
#endif // UC2
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

#if UNREAL3
struct FSkeletalMeshLODInfo
{
	DECLARE_STRUCT(FSkeletalMeshLODInfo);
	float					DisplayFactor;
	float					LODHysteresis;
	TArray<int>				LODMaterialMap;
	TArray<bool>			bEnableShadowCasting;

	BEGIN_PROP_TABLE
		PROP_FLOAT(DisplayFactor)
		PROP_FLOAT(LODHysteresis)
		PROP_ARRAY(LODMaterialMap, int)
		PROP_ARRAY(bEnableShadowCasting, bool)
	END_PROP_TABLE
};
#endif

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
#if UNREAL3
	TArray<FSkeletalMeshLODInfo> LODInfo;	//?? move outside
#endif
#if BIOSHOCK
	TArray<UObject*>		havokObjects;	// wrappers for Havok objects used by this mesh; not used by Bioshock engine (precaching?)
#endif

	void UpgradeFaces();
	void UpgradeMesh();
	void RecreateMeshFromLOD();
#if UNREAL1
	void SerializeSkelMesh1(FArchive &Ar);
#endif
#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif
#if UNREAL3
	void SerializeSkelMesh3(FArchive &Ar);
#endif
#if BIOSHOCK
	void SerializeBioshockMesh(FArchive &Ar);
	void PostLoadBioshockMesh();
#endif

#if UNREAL3
	//!! separate class for UE3 !
	BEGIN_PROP_TABLE
		PROP_ARRAY(LODInfo, FSkeletalMeshLODInfo)
		PROP_DROP(Sockets)
		PROP_DROP(SkelMeshGUID)
		PROP_DROP(SkelMirrorTable)
		PROP_DROP(FaceFXAsset)
		PROP_DROP(bDisableSkeletalAnimationLOD)
		PROP_DROP(BoundsPreviewAsset)
		PROP_DROP(PerPolyCollisionBones)
		PROP_DROP(AddToParentPerPolyCollisionBone)
#	if MEDGE
		PROP_DROP(NumUVSets)
#	endif
	END_PROP_TABLE
#endif // UNREAL3

	virtual void Serialize(FArchive &Ar)
	{
		guard(USkeletalMesh::Serialize);

#if UNREAL1
		if (Ar.ArVer < PACKAGE_V2)
		{
			SerializeSkelMesh1(Ar);
			return;
		}
#endif
#if UNREAL3
		if (Ar.ArVer >= PACKAGE_V3)
		{
			SerializeSkelMesh3(Ar);
			return;
		}
#endif
#if BIOSHOCK
		if (Ar.IsBioshock)
		{
			SerializeBioshockMesh(Ar);
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
//				appNotify("SkeletalMesh of version %d\n", Version);
				TArray<FLODMeshSection> tmp1, tmp2;
				TArray<word> tmp3;
				Ar << tmp1 << tmp2 << tmp3;
			}
			// copy and convert data from old mesh format
			UpgradeMesh();
		}
		else
		{
#if UC2
			if (Ar.ArVer >= 136)
			{
				int f338;
				Ar << f338;
			}
#endif // UC2
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
			Ar.Seek(Ar.GetStopper());
			return;
		}
#endif // TRIBES3
#if UC2
		if (Ar.ArVer >= 145) // IsUC2
		{
			Ar.Seek(Ar.GetStopper());
			return;
		}
#endif // UC2

#if LINEAGE2
		if (Ar.IsLineage2)
		{
			int unk1, unk3, unk4;
			TArray<float> unk2;
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

		if (Ar.ArLicenseeVer && (Ar.Tell() != Ar.GetStopper()))
		{
			appNotify("Serializing SkeletalMesh'%s' of unknown game: %d unreal bytes", Name, Ar.GetStopper() - Ar.Tell());
			Ar.Seek(Ar.GetStopper());
		}

		unguard;
	}
#if BIOSHOCK
	virtual void PostLoad()
	{
#if 0
		if (Package->IsBioshock)
#else
		if (havokObjects.Num())			//?? ... UnPackage is not ready here ...
#endif
			PostLoadBioshockMesh();		// should be called after loading of all used objects
	}
#endif
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


#if BIOSHOCK

struct FBioshockUnk6
{
	FName					f0, f8, f10, f18;
	FVector					f20;
	FVector					f2C;
	float					f38, f3C;
	FVector					f40;
	int						f4C, f50, f54;
	int						f58, f5C, f60;
	int						f64, f68, f6C;

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk6 &S)
	{
		TRIBES_HDR(Ar, 0);
		Ar << S.f0 << S.f8 << S.f10 << S.f20 << S.f2C << S.f38 << S.f3C;
		Ar << S.f40 << S.f4C << S.f50 << S.f54 << S.f58 << S.f5C << S.f60;
		Ar << S.f64 << S.f68 << S.f6C;
		if (t3_hdrSV >= 2)
			Ar << S.f18;
		return Ar;
	}
};

#if 0
class USharedSkeletonDataMetadata : public UObject
{
	DECLARE_CLASS(USharedSkeletonDataMetadata, UObject);
public:
	int						f44, f48;
	TArray<float>			f88;
	int						f94, f98, f9C;
	float					fA0, fA4, fA8, fAC, fB0, fB4;
	TArray<FBioshockUnk6>	fB8;
	float					fC4, fC8;

	virtual void Serialize(FArchive &Ar)
	{
		guard(USharedSkeletonDataMetadata::Serialize);

		Super::Serialize(Ar);
		Ar << f88;
		if (Ar.ArLicenseeVer < 34) return;
		TRIBES_HDR(Ar, 0);
		Ar << f94 << f98 << f9C;
		if (t3_hdrSV >= 2)
			Ar << fA0 << fA4 << fA8 << fAC << fB0 << fB4;
		if (t3_hdrSV >= 3)
			Ar << fB8;
		if (t3_hdrSV >= 4)
		{
			Ar << f44;
			if (t3_hdrSV < 8)
			{
				int tmp1, tmp2;
				Ar << tmp1 << tmp2;
			}
			Ar << f48 << fC4 << fC8;
		}
/*
		//!!
		int i;
		printf("f88[]=%d f94=%d %d %d\n"
			   "fA0=%g %g %g %g %g %g\n"
			   "fB8[]=%d\n",
			   f88.Num(), f94, f98, f9C,
			   fA0, fA4, fA8, fAC, fB0, fB4,
			   fB8.Num());
		for (i = 0; i < fB8.Num(); i++)
		{
			const FBioshockUnk6 &S = fB8[i];
			printf("  %d: %s %s %s %s\n"
				   "    {%g %g %g} {%g %g %g}\n"
				   "    %g %g\n"
				   "    {%g %g %g} {%X %X %X}\n"
				   "    {%X %X %X} {%X %X %X}\n",
				   i, *S.f0, *S.f8, *S.f10, *S.f18,
				   FVECTOR_ARG(S.f2C), S.f38, S.f3C,
				   FVECTOR_ARG(S.f40), S.f4C, S.f50, S.f54,
				   S.f58, S.f5C, S.f60, S.f64, S.f68, S.f6C);
		}
*/
		Ar.Seek(Ar.GetStopper());

		unguard;
	}
};
#endif // 0

class UAnimationPackageWrapper : public UObject
{
	DECLARE_CLASS(UAnimationPackageWrapper, UObject);
public:
	TArray<byte>			HavokData;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UAnimationPackageWrapper::Serialize);
		Super::Serialize(Ar);
		TRIBES_HDR(Ar, 0);
		Ar << HavokData;
		Process();
		unguard;
	}

	void Process();
};

#endif // BIOSHOCK


#if UNREAL3

struct FRawAnimSequenceTrack
{
	DECLARE_STRUCT(FRawAnimSequenceTrack);
	TArray<FVector>			PosKeys;
	TArray<FQuat>			RotKeys;
	TArray<float>			KeyTimes;

	BEGIN_PROP_TABLE
		PROP_ARRAY(PosKeys, FVector)
		PROP_ARRAY(RotKeys, FQuat)
		PROP_ARRAY(KeyTimes, float)
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive &Ar, FRawAnimSequenceTrack &T)
	{
		return Ar << T.PosKeys << T.RotKeys << T.KeyTimes;
	}
};

enum AnimationCompressionFormat
{
	ACF_None,
	ACF_Float96NoW,
	ACF_Fixed48NoW,
	ACF_IntervalFixed32NoW,
	ACF_Fixed32NoW,
	ACF_Float32NoW,
};

_ENUM(AnimationCompressionFormat)
{
	_E(ACF_None),
	_E(ACF_Float96NoW),
	_E(ACF_Fixed48NoW),
	_E(ACF_IntervalFixed32NoW),
	_E(ACF_Fixed32NoW),
	_E(ACF_Float32NoW),
};

enum AnimationKeyFormat
{
	AKF_ConstantKeyLerp,									// animation keys are placed on evenly-spaced intervals
	AKF_VariableKeyLerp										// animation keys have explicit key times
};

_ENUM(AnimationKeyFormat)
{
	_E(AKF_ConstantKeyLerp),
	_E(AKF_VariableKeyLerp)
};


#if MASSEFF
// Bioware has separated some common UAnimSequence settings

class UBioAnimSetData : public UObject
{
	DECLARE_CLASS(UBioAnimSetData, UObject);
public:
	bool					bAnimRotationOnly;
	TArray<FName>			TrackBoneNames;
	TArray<FName>			UseTranslationBoneNames;

	BEGIN_PROP_TABLE
		PROP_BOOL(bAnimRotationOnly)
		PROP_ARRAY(TrackBoneNames, FName)
		PROP_ARRAY(UseTranslationBoneNames, FName)
	END_PROP_TABLE
};

#endif // MASSEFF


class UAnimSequence : public UObject
{
	DECLARE_CLASS(UAnimSequence, UObject);
public:
	FName					SequenceName;
//	TArray<FAnimNotifyEvent> Notifies;	// analogue of FMeshAnimNotify
	float					SequenceLength;
	int						NumFrames;
	float					RateScale;
	bool					bNoLoopingInterpolation;
	TArray<FRawAnimSequenceTrack> RawAnimData;
	AnimationCompressionFormat TranslationCompressionFormat;
	AnimationCompressionFormat RotationCompressionFormat;
	AnimationKeyFormat		KeyEncodingFormat;				// GoW2+
	TArray<int>				CompressedTrackOffsets;
	TArray<byte>			CompressedByteStream;
#if MASSEFF
	UBioAnimSetData			*m_pBioAnimSetData;
#endif

	UAnimSequence()
	:	RateScale(1.0f)
	,	TranslationCompressionFormat(ACF_None)
	,	RotationCompressionFormat(ACF_None)
	,	KeyEncodingFormat(AKF_ConstantKeyLerp)
	{}

	BEGIN_PROP_TABLE
		PROP_NAME(SequenceName)
		PROP_FLOAT(SequenceLength)
		PROP_INT(NumFrames)
		PROP_FLOAT(RateScale)
		PROP_BOOL(bNoLoopingInterpolation)
		PROP_ARRAY(RawAnimData, FRawAnimSequenceTrack)
		PROP_ENUM2(TranslationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(RotationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(KeyEncodingFormat, AnimationKeyFormat)
		PROP_ARRAY(CompressedTrackOffsets, int)
#if MASSEFF
		PROP_OBJ(m_pBioAnimSetData)
#endif
		// unsupported
		PROP_DROP(Notifies)
		PROP_DROP(CompressionScheme)
		PROP_DROP(bDoNotOverrideCompression)
		//!! additive animations
		PROP_DROP(bIsAdditive)
		PROP_DROP(AdditiveRefName)
#if TLR
		PROP_DROP(ActionID)
		PROP_DROP(m_ExtraData)
#endif
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		guard(UAnimSequence::Serialize);
		assert(Ar.ArVer >= 372);		// older version is not yet ready
		Super::Serialize(Ar);
#if TUROK
		if (Ar.IsTurok)
		{
			Ar << RawAnimData;
			return;
		}
#endif // TUROK
		Ar << CompressedByteStream;
		unguard;
	}
};

class UAnimSet : public UMeshAnimation // real parent is UObject
{
	DECLARE_CLASS(UAnimSet, UMeshAnimation);
public:
	bool					bAnimRotationOnly;
	TArray<FName>			TrackBoneNames;
	TArray<UAnimSequence*>	Sequences;
	TArray<FName>			UseTranslationBoneNames;
	FName					PreviewSkelMeshName;
#if MASSEFF
	UBioAnimSetData			*m_pBioAnimSetData;
#endif

	UAnimSet()
	:	bAnimRotationOnly(true)
	{
		PreviewSkelMeshName.Str = "None";
	}

	BEGIN_PROP_TABLE
		PROP_BOOL(bAnimRotationOnly)
		PROP_ARRAY(TrackBoneNames, FName)
		PROP_ARRAY(Sequences, UObject*)
		PROP_ARRAY(UseTranslationBoneNames, FName)
		PROP_NAME(PreviewSkelMeshName)
#if MASSEFF
		PROP_OBJ(m_pBioAnimSetData)
#endif
		//!! unsupported
		PROP_DROP(ForceMeshTranslationBoneNames)
	END_PROP_TABLE

	void ConvertAnims();

	virtual void Serialize(FArchive &Ar)
	{
		guard(UAnimSet::Serialize);
		UObject::Serialize(Ar);
		unguard;
	}

	virtual void PostLoad()
	{
		ConvertAnims();		// should be called after loading of all used objects
	}
};

#endif // UNREAL3


/*-----------------------------------------------------------------------------
	UStaticMesh class
-----------------------------------------------------------------------------*/

struct FStaticMeshSection
{
	int						f4;				// always 0 ??
	word					FirstIndex;		// first index
	word					FirstVertex;	// first used vertex
	word					LastVertex;		// last used vertex
	word					fE;				// ALMOST always equals to f10
	word					NumFaces;		// number of faces in section

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshSection &S)
	{
		guard(FStaticMeshSection<<);
		assert(Ar.ArVer >= 112);
		return Ar << S.f4 << S.FirstIndex << S.FirstVertex << S.LastVertex << S.fE << S.NumFaces;
		unguard;
	}
};

struct FStaticMeshVertex
{
	FVector					Pos;
	FVector					Normal;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertex &V)
	{
		assert(Ar.ArVer >= 112);
		return Ar << V.Pos << V.Normal;
		// Ver < 112 -- +2 floats
		// Ver < 112 && LicVer >= 5 -- +2 floats
		// Ver == 111 -- +4 bytes (FColor?)
		//!! check SIMPLE_TYPE() possibility when modifying this function
	}
};

SIMPLE_TYPE(FStaticMeshVertex, float)		//?? check each version

#if BIOSHOCK

struct FStaticMeshVertexBio
{
	FVector					Pos;
	unsigned				Normal[3];		// Bioshock has different normal format (dword{00XXYYZZ} x 3)

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexBio &V)
	{
		return Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos = Pos;
		union PackedNorm
		{
			unsigned I;
			byte     C[4];
		};
		PackedNorm N;
		N.I = Normal[2];		//?? use other indices too?
		r.Normal.X = N.C[0] / 128.0f - 1;
		r.Normal.Y = N.C[1] / 128.0f - 1;
		r.Normal.Z = N.C[2] / 128.0f - 1;
		return r;
	}
};

SIMPLE_TYPE(FStaticMeshVertexBio, int)

#endif // BIOSHOCK

struct FStaticMeshVertexStream
{
	TArray<FStaticMeshVertex> Vert;
	int						Revision;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexStream &S)
	{
#if BIOSHOCK
		if (Ar.IsBioshock)
		{
			TArray<FStaticMeshVertexBio> BioVerts;
			Ar << BioVerts;
			CopyArray(S.Vert, BioVerts);
			return Ar;
		}
#endif
#if TRIBES3
		TRIBES_HDR(Ar, 0xD);
#endif
		Ar << S.Vert << S.Revision;
#if TRIBES3
		if (Ar.IsTribes3 && t3_hdrSV >= 1)
		{
			TArray<T3_BasisVector> unk1C;
			Ar << unk1C;
		}
#endif // TRIBES3
		return Ar;
	}
};

struct FRawColorStream
{
	TArray<FColor>			Color;
	int						Revision;

	friend FArchive& operator<<(FArchive &Ar, FRawColorStream &S)
	{
		return Ar << S.Color << S.Revision;
	}
};

struct FStaticMeshUV
{
	float					U;
	float					V;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUV &V)
	{
		return Ar << V.U << V.V;
	}
};

SIMPLE_TYPE(FStaticMeshUV, float)

struct FStaticMeshUVStream
{
	TArray<FStaticMeshUV>	Data;
	int						f10;
	int						f1C;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVStream &S)
	{
#if BIOSHOCK
		if (Ar.IsBioshock)
			return Ar << S.Data << S.f10;
#endif
		return Ar << S.Data << S.f10 << S.f1C;
	}
};

struct FStaticMeshTriangleUnk
{
	float					unk1[2];
	float					unk2[2];
	float					unk3[2];
};

// complex FStaticMeshTriangle structure
struct FStaticMeshTriangle
{
	FVector					f0;
	FVector					fC;
	FVector					f18;
	FStaticMeshTriangleUnk	f24[8];
	byte					fE4[12];
	int						fF0;
	int						fF4;
	int						fF8;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshTriangle &T)
	{
		guard(FStaticMeshTriangle<<);
		int i;

		assert(Ar.ArVer >= 112);
		Ar << T.f0 << T.fC << T.f18;
		Ar << T.fF8;
		assert(T.fF8 <= ARRAY_COUNT(T.f24));
		for (i = 0; i < T.fF8; i++)
		{
			FStaticMeshTriangleUnk &V = T.f24[i];
			Ar << V.unk1[0] << V.unk1[1] << V.unk2[0] << V.unk2[1] << V.unk3[0] << V.unk3[1];
		}
		for (i = 0; i < 12; i++)
			Ar << T.fE4[i];			// UT2 has strange order of field serialization: [2,1,0,3] x 3 times
		Ar << T.fF0 << T.fF4;
		// extra fields for older version (<= 111)

		return Ar;
		unguard;
	}
};

struct FkDOPNode
{
	float					unk1[3];
	float					unk2[3];
	int						unk3;		//?? index * 32 ?
	short					unk4;
	short					unk5;

	friend FArchive& operator<<(FArchive &Ar, FkDOPNode &N)
	{
		guard(FkDOPNode<<);
		int i;
		for (i = 0; i < 3; i++)
			Ar << N.unk1[i];
		for (i = 0; i < 3; i++)
			Ar << N.unk2[i];
		Ar << N.unk3 << N.unk4 << N.unk5;
		return Ar;
		unguard;
	}
};

RAW_TYPE(FkDOPNode)

struct FkDOPCollisionTriangle
{
	short					v[4];

	friend FArchive& operator<<(FArchive &Ar, FkDOPCollisionTriangle &T)
	{
		return Ar << T.v[0] << T.v[1] << T.v[2] << T.v[3];
	}
};

SIMPLE_TYPE(FkDOPCollisionTriangle, short)

struct FStaticMeshCollisionNode
{
	int						f1[4];
	float					f2[6];
	byte					f3;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshCollisionNode &N)
	{
		int i;
		for (i = 0; i < 4; i++) Ar << AR_INDEX(N.f1[i]);
		for (i = 0; i < 6; i++) Ar << N.f2[i];
		Ar << N.f3;
		return Ar;
	}
};

struct FStaticMeshCollisionTriangle
{
	float					f1[16];
	int						f2[4];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshCollisionTriangle &T)
	{
		int i;
		for (i = 0; i < 16; i++) Ar << T.f1[i];
		for (i = 0; i <  4; i++) Ar << AR_INDEX(T.f2[i]);
		return Ar;
	}
};

struct FStaticMeshMaterial
{
	DECLARE_STRUCT(FStaticMeshMaterial);
	UMaterial				*Material;
	bool					EnableCollision;

	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
		PROP_BOOL(EnableCollision)
#if LINEAGE2
		PROP_DROP(EnableCollisionforShadow)
		PROP_DROP(bNoDynamicShadowCast)
#endif // LINEAGE2
	END_PROP_TABLE
};

class UStaticMesh : public UPrimitive
{
	DECLARE_CLASS(UStaticMesh, UPrimitive);
public:
	TArray<FStaticMeshMaterial> Materials;
	TArray<FStaticMeshSection>  Sections;
	FStaticMeshVertexStream	VertexStream;
	FRawColorStream			ColorStream1;	// for Verts
	FRawColorStream			ColorStream2;
	TArray<FStaticMeshUVStream> UVStream;
	FRawIndexBuffer			IndexStream1;
	FRawIndexBuffer			IndexStream2;
	TArray<FkDOPNode>		kDOPNodes;
	TArray<FkDOPCollisionTriangle> kDOPCollisionFaces;
	TLazyArray<FStaticMeshTriangle> Faces;
	UObject					*f108;
	int						f124;			//?? FRotator?
	int						f128;
	int						f12C;
	int						Version;		// set to 11 when saving mesh
	TArray<float>			f150;
	int						f15C;
	UObject					*f16C;
	bool					UseSimpleLineCollision;
	bool					UseSimpleBoxCollision;
	bool					UseSimpleKarmaCollision;
	bool					UseVertexColor;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, FStaticMeshMaterial)
		PROP_BOOL(UseSimpleLineCollision)
		PROP_BOOL(UseSimpleBoxCollision)
		PROP_BOOL(UseSimpleKarmaCollision)
		PROP_BOOL(UseVertexColor)
#if LINEAGE2
		PROP_DROP(bMakeTwoSideMesh)
#endif
#if BIOSHOCK
		PROP_DROP(HavokCollisionTypeStatic)
		PROP_DROP(HavokCollisionTypeDynamic)
		PROP_DROP(UseSimpleVisionCollision)
		PROP_DROP(UseSimpleFootIKCollision)
		PROP_DROP(NeverCollide)
		PROP_DROP(SortShape)
		PROP_DROP(LightMapCoordinateIndex)
		PROP_DROP(LightMapScale)
#endif // BIOSHOCK
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		guard(UStaticMesh::Serialize);

		if (Ar.ArVer >= PACKAGE_V3)
		{
			//!! not yet supported
			Ar.Seek(Ar.GetStopper());
			return;
		}

		if (Ar.ArVer < 112)
		{
			appNotify("StaticMesh of old version %d/%d has been found", Ar.ArVer, Ar.ArLicenseeVer);
			Ar.Seek(Ar.GetStopper());
			return;
		}

		Super::Serialize(Ar);
#if TRIBES3
		TRIBES_HDR(Ar, 3);
#endif
		Ar << Sections;
		Ar << BoundingBox;			// UPrimitive field, serialized twice ...
#if BIOSHOCK
		if (Ar.IsBioshock)
		{
			Ar << VertexStream << UVStream << IndexStream1;
			// also: t3_hdrSV < 2 => IndexStream2
			Ar.Seek(Ar.GetStopper());
			return;
		}
#endif // BIOSHOCK
		Ar << VertexStream << ColorStream1 << ColorStream2 << UVStream << IndexStream1 << IndexStream2 << f108;

#if 1
		// UT2 and UE2Runtime has very different collision structures
		// We don't need it, so don't bother serializing it
		Ar.Seek(Ar.GetStopper());
		return;
#else
		if (Ar.ArVer < 126)
		{
			assert(Ar.ArVer >= 112);
			TArray<FStaticMeshCollisionTriangle> CollisionFaces;
			TArray<FStaticMeshCollisionNode>     CollisionNodes;
			Ar << CollisionFaces << CollisionNodes;
		}
		else
		{
			// this is an UT2 code, UE2Runtime has different structures
			Ar << kDOPNodes << kDOPCollisionFaces;
		}
		if (Ar.ArVer < 114)
			Ar << f124 << f128 << f12C;

		Ar << Faces;					// can skip this array
		Ar << Version;

#if UT2
		if (Ar.IsUT2)
		{
			//?? check for generic UE2
			if (Ar.ArLicenseeVer == 22)
			{
				float unk;				// predecessor of f150
				Ar << unk;
			}
			else if (Ar.ArLicenseeVer >= 23)
				Ar << f150;
			Ar << f16C;
			if (Ar.ArVer >= 120)
				Ar << f15C;
		}
#endif // UT2
#endif // 0

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	Class registration
-----------------------------------------------------------------------------*/

#define REGISTER_MESH_CLASSES		\
	REGISTER_CLASS(USkeletalMesh)	\
	REGISTER_CLASS(UVertMesh)		\
	REGISTER_CLASS(UMeshAnimation)	\
	REGISTER_CLASS(UStaticMesh)		\
	REGISTER_CLASS(FStaticMeshMaterial)

// Note: we have registered UVertMesh and UMesh as ULodMesh too for UE1 compatibility
#define REGISTER_MESH_CLASSES_U1	\
	REGISTER_CLASS_ALIAS(UVertMesh, UMesh) \
	REGISTER_CLASS_ALIAS(UVertMesh, ULodMesh) \
	REGISTER_CLASS_ALIAS(UMeshAnimation, UAnimation)

#define REGISTER_MESH_CLASSES_RUNE	\
	REGISTER_CLASS(USkelModel)

#define REGISTER_MESH_CLASSES_BIO	\
	/*REGISTER_CLASS(USharedSkeletonDataMetadata)*/ \
	REGISTER_CLASS(UAnimationPackageWrapper)

#define REGISTER_MESH_CLASSES_MASSEFF \
	REGISTER_CLASS(UBioAnimSetData)

// UTdAnimSet - Mirror's Edge, derived from UAnimSet
#define REGISTER_MESH_CLASSES_U3	\
	REGISTER_CLASS(FRawAnimSequenceTrack) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS(UAnimSequence)	\
	REGISTER_CLASS(UAnimSet)		\
	REGISTER_CLASS_ALIAS(UAnimSet, UTdAnimSet)


#define REGISTER_MESH_ENUMS_U3		\
	REGISTER_ENUM(AnimationCompressionFormat) \
	REGISTER_ENUM(AnimationKeyFormat)

#endif // __UNMESH_H__
