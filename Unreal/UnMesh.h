#ifndef __UNMESH_H__
#define __UNMESH_H__

/*-----------------------------------------------------------------------------
UE1 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		UPrimitive
			UMesh
				ULodMesh
					USkeletalMesh
			USkelModel (Rune)

-----------------------------------------------------------------------------*/

//?? declare separately? place to UnCore?
float half2float(word h);

class UMaterial;
class UMeshAnimation;

//?? move these declarations outside
class CAnimSet;
class CStaticMesh;


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
#if XIII
		if (Ar.Game == GAME_XIII && Ar.ArLicenseeVer >= 19)
		{
			byte	unk1[4];
			float	unk2;
			Ar << unk1[0] << unk1[1] << unk1[2] << unk1[3] << unk2;
		}
#endif // XIII
		unguard;
	}
};


/*-----------------------------------------------------------------------------
	Common mesh structures
-----------------------------------------------------------------------------*/

// corresponds to UT1 FMeshFloatUV
// UE2 name: FMeshUV
struct FMeshUVFloat
{
	float			U;
	float			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUVFloat &M)
	{
		return Ar << M.U << M.V;
	}
};

SIMPLE_TYPE(FMeshUVFloat, float)


// UE1 name: FMeshUV
struct FMeshUV1
{
	byte			U;
	byte			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUV1 &M)
	{
		return Ar << M.U << M.V;
	}

	operator FMeshUVFloat() const
	{
		FMeshUVFloat r;
		r.U = U / 255.0f;
		r.V = V / 255.0f;
		return r;
	}
};

SIMPLE_TYPE(FMeshUV1, byte)


struct FMeshUVHalf
{
	short			U;
	short			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUVHalf &V)
	{
		Ar << V.U << V.V;
		return Ar;
	}

	operator FMeshUVFloat() const
	{
		FMeshUVFloat r;
		r.U = half2float(U);
		r.V = half2float(V);
		return r;
	}
};

SIMPLE_TYPE(FMeshUVHalf, word)


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
// note: UE2 uses FMeshUVFloat
struct FMeshTri
{
	word			iVertex[3];				// Vertex indices.
	FMeshUV1		Tex[3];					// Texture UV coordinates (byte[2]).
	unsigned		PolyFlags;				// Surface flags.
	int				TextureIndex;			// Source texture index.

	friend FArchive& operator<<(FArchive &Ar, FMeshTri &T)
	{
		Ar << T.iVertex[0] << T.iVertex[1] << T.iVertex[2];
		Ar << T.Tex[0] << T.Tex[1] << T.Tex[2];
		Ar << T.PolyFlags << T.TextureIndex;
		return Ar;
	}
};

// the same as FMeshTri, but used float UV
struct FMeshTri2
{
	word			iVertex[3];				// Vertex indices.
	FMeshUVFloat	Tex[3];					// Texture UV coordinates (float[2]).
	unsigned		PolyFlags;				// Surface flags.
	int				TextureIndex;			// Source texture index.

	friend FArchive& operator<<(FArchive &Ar, FMeshTri2 &T)
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
	FMeshUVFloat	TexUV;					// Texture UV coordinates.
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
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArLicenseeVer == 1)
			return Ar << N.Time << N.NotifyObj;
#endif
		Ar << N.Time << N.Function;
#if SPLINTER_CELL
		if (Ar.Game == GAME_SplinterCell)
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

SIMPLE_TYPE(FLineageUnk2, int)


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
		if (Ar.Game == GAME_Tribes3)
		{
			TRIBES_HDR(Ar, 0x17);
		 	if (t3_hdrV == 1)
			{
				int unk;
				Ar << unk;
			}
		}
#endif // TRIBES3
		if (Ar.ArVer >= 115)
			Ar << A.f28;
#if LOCO
		if (Ar.Game == GAME_Loco && Ar.ArVer >= 130)
		{
			int unk2C;
			Ar << unk2C;
		}
#endif // LOCO
		Ar << A.Name;
#if UNREAL1
		if (Ar.Engine() == GAME_UE1)
		{
			// UE1 support
			assert(Ar.IsLoading);
			FName tmpGroup;					// single group
			Ar << tmpGroup;
			if (strcmp(tmpGroup, "None") != 0)
				A.Groups.AddItem(tmpGroup);
		}
		else
#endif // UNREAL1
		{
			Ar << A.Groups;					// array of groups
		}
		Ar << A.StartFrame << A.NumFrames << A.Notifys << A.Rate;
#if SPLINTER_CELL
		if (Ar.Game == GAME_SplinterCell)
		{
			byte unk;
			Ar << unk;
		}
#endif // SPLINTER_CELL
#if LINEAGE2
		if (Ar.Game == GAME_Lineage2 && Ar.ArLicenseeVer >= 1)
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
#endif // LINEAGE2
#if SWRC
		if (Ar.Game == GAME_RepCommando)
		{
			int f18;
			byte f20;
			float f1C, f30;
			Ar << f18 << f1C << f20;
			if (Ar.ArVer >= 144) Ar << f30;	// default = 1.0f
		}
#endif // SWRC
		return Ar;
		unguard;
	}
};


#if LOCO

struct FLocoUnk1
{
	FName		f0;
	int			f4;

	friend FArchive& operator<<(FArchive &Ar, FLocoUnk1 &V)
	{
		return Ar << V.f0 << V.f4;
	}
};

struct FLocoUnk2
{
	FString		f0;
	FName		f1;
	FVector		f2;
	FRotator	f3;
	int			f4, f5;
	float		f6;
	FVector		f7;
	int			f8, f9;

	friend FArchive& operator<<(FArchive &Ar, FLocoUnk2 &V)
	{
		return Ar << V.f0 << V.f1 << V.f2 << V.f3 << V.f4 << V.f5 << V.f6 << V.f7 << V.f8 << V.f9;
	}
};

#endif // LOCO

// Base class for UVertMesh and USkeletalMesh; in Unreal Engine it is derived from
// abstract class UMesh (which is derived from UPrimitive)

/*
 * Possible versions:
 *	1	SplinterCell		LodMesh is same as UT, SkeletalMesh modified
 *	2	Postal 2			same as UT
 *	4	UT2003, UT2004
 *	5	Lineage2, UC2		different extensions
 *	8	Republic Commando, Loco
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

	virtual void Serialize(FArchive &Ar);
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
	FMeshUVFloat	Tex;

	friend FArchive& operator<<(FArchive &Ar, FAnimMeshVertex &V)
	{
		return Ar << V.Pos << V.Norm << V.Tex;
	}
};

SIMPLE_TYPE(FAnimMeshVertex, float)


struct FRawIndexBuffer
{
	TArray<word>	Indices;
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
#endif // TRIBES3

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
		if ((Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4) && t3_hdrSV >= 1)
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

	virtual void Serialize(FArchive &Ar);
#if UNREAL1
	void SerializeVertMesh1(FArchive &Ar);
#endif
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
		{
#if ENDWAR
			if (Ar.Game == GAME_EndWar)
			{
				// End War has W in FVector, but VJointPos will not serialize W for Position since ArVer 295
				Ar << P.Orientation << P.Position.X << P.Position.Y << P.Position.Z;
				return Ar;
			}
#endif // ENDWAR
			Ar << P.Orientation << P.Position;
#if ARMYOF2
			if (Ar.Game == GAME_ArmyOf2 && Ar.ArVer >= 481)
			{
				int pad;
				Ar << pad;
			}
#endif // ARMYOF2
			return Ar;
		}
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
#if XIII
		if (Ar.Game == GAME_XIII && Ar.ArLicenseeVer > 26)					// real version is unknown; beta = old code, retail = new code
			return Ar << B.Name << B.Flags << B.BonePos << B.ParentIndex;	// no NumChildren
#endif // XIII
		Ar << B.Name << B.Flags << B.BonePos << B.NumChildren << B.ParentIndex;
#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArVer >= 459)
		{
			int unk3C;						// FColor?
			Ar << unk3C;
		}
#endif // ARMYOF2
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers) return Ar; // version 537, but really not upgraded
#endif
#if UNREAL3
		if (Ar.ArVer >= 515)
		{
			int unk44;						// byte[4] ? default is 0xFF x 4
			Ar << unk44;
		}
#endif // UNREAL3
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
		if (Ar.Game == GAME_UT2)
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
		if (Ar.Game == GAME_UT2)
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
		if (Ar.Game == GAME_Lineage2 && Ar.ArLicenseeVer >= 0x1C)
		{
			// bone map (local bone index -> mesh bone index)
			Ar << S.LineageBoneMap;
		}
#endif // LINEAGE2
		return Ar;
	}
};


#if LINEAGE2

// FAnimMeshVertex with influence info
struct FLineageWedge
{
	FVector			Point;
	FVector			Normal;				// note: length = 512
	FMeshUVFloat	Tex;
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

SIMPLE_TYPE(FRag2FSkinGPUVertex, float)

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


#if BATTLE_TERR

struct FBTUnk1
{
	FVector			f0;
	FVector			f1;

	friend FArchive& operator<<(FArchive &Ar, FBTUnk1 &S)
	{
		return Ar << S.f0 << S.f1;
	}
};

SIMPLE_TYPE(FBTUnk1, float)

struct FBTUnk2
{
	int				f0;
	int				f1;

	friend FArchive& operator<<(FArchive &Ar, FBTUnk2 &S)
	{
		return Ar << S.f0 << S.f1;
	}
};

SIMPLE_TYPE(FBTUnk2, int)

struct FBTUnk3
{
	float			f[8];

	friend FArchive& operator<<(FArchive &Ar, FBTUnk3 &S)
	{
		return Ar << S.f[0] << S.f[1] << S.f[2] << S.f[3] << S.f[4] << S.f[5] << S.f[6] << S.f[7];
	}
};

SIMPLE_TYPE(FBTUnk3, float)

#endif // BATTLE_TERR


#if UC2

struct FUC2Vector
{
	short			X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FUC2Vector &V)
	{
		if (Ar.ArLicenseeVer == 1)
			return Ar << V.X << V.Y << V.Z;
		// LicenseeVer == 0 -- serialize as int[3] and rescale, using some global scale
		int X, Y, Z;
		return Ar << X << Y << Z;
	}
};

struct FUC2Normal
{
	int				X:11;
	int				Y:10;
	int				Z:11;

	friend FArchive& operator<<(FArchive &Ar, FUC2Normal &V)
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
	FUC2Vector		Pos;
	FUC2Normal		Normal;
	FUC2Int			fA, fC;

	friend FArchive& operator<<(FArchive &Ar, FUC2Unk3 &S)
	{
		Ar << S.Pos << S.Normal << S.fA << S.fC;
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
#if SWRC
		if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 146)
		{
			int unk0;
			Ar << unk0;
		}
#endif // SWRC
		Ar << M.SkinningData << M.SkinPoints << M.NumDynWedges;
		Ar << M.SmoothSections << M.RigidSections << M.SmoothIndices << M.RigidIndices;
		Ar << M.VertexStream;
		Ar << M.VertInfluences << M.Wedges << M.Faces << M.Points;
		Ar << M.LODDistanceFactor << M.LODHysteresis << M.NumSharedVerts;
		Ar << M.LODMaxInfluences << M.f114 << M.f118;
#if TRIBES3
		if ((Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4) && t3_hdrSV >= 1)
		{
			TLazyArray<T3_BasisVector> unk1;
			TArray<T3_BasisVector> unk2;
			Ar << unk1 << unk2;
		}
#endif // TRIBES3
#if LINEAGE2
		if (Ar.Game == GAME_Lineage2 && Ar.ArLicenseeVer >= 0x1C)
		{
			int UseNewWedges;
			Ar << UseNewWedges << M.LineageWedges;
			M.RestoreLineageMesh();
		}
#endif // LINEAGE2
#if RAGNAROK2
		if (Ar.Game == GAME_Ragnarok2 && Ar.ArVer >= 128)
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
#if LOCO
		if (Ar.Game == GAME_Loco)
		{
			// tangent space ?
			if (Ar.ArVer >= 133)
			{
				TArray<FQuat> unk1; // really not FQuat
				Ar << unk1;
			}
			else if (Ar.ArVer == 132)
			{
				TArray<FVector> unk2, unk3;
				Ar << unk2 << unk3;
			}
		}
#endif // LOCO
#if BATTLE_TERR
		if (Ar.Game == GAME_BattleTerr)
		{
			if (Ar.ArLicenseeVer >= 31 || Ar.ArLicenseeVer == 1)
			{
				TArray<FBTUnk1> fA8;
				int     fBC;
				Ar << fA8 << fBC;
			}
			if (Ar.ArLicenseeVer >= 35 || Ar.ArLicenseeVer == 1)
			{
				TArray<FBTUnk2> f98;
				Ar << f98;
			}
			if (Ar.ArLicenseeVer >= 36 || Ar.ArLicenseeVer == 1)
			{
				FSkinVertexStream fCC;
				TArray<FBTUnk3> fFC;
				int     f110, f120, f124;
				Ar << fCC << fFC << f110 << f120 << f124;
			}
		}
#endif // BATTLE_TERR
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			if (Ar.ArVer >= 136)
			{
				FVector f17C, VectorScale, VectorBase;
				Ar << f17C << VectorScale << VectorBase;
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
		PROP_DROP(TriangleSorting)
		PROP_DROP(TriangleSortSettings)
#if FRONTLINES
		PROP_DROP(bExcludeFromConsoles)
		PROP_DROP(bCanRemoveForLowDetail)
#endif
#if MCARTA
		PROP_DROP(LODMaterialDrawOrder)
#endif
	END_PROP_TABLE
};

class USkeletalMeshSocket : public UObject
{
	DECLARE_CLASS(USkeletalMeshSocket, UObject);
public:
	FName					SocketName;
	FName					BoneName;
	FVector					RelativeLocation;
	FRotator				RelativeRotation;
	FVector					RelativeScale;

	USkeletalMeshSocket()
	{
		SocketName.Str = "None";
		BoneName.Str = "None";
		RelativeLocation.Set(0, 0, 0);
		RelativeRotation.Set(0, 0, 0);
		RelativeScale.Set(1, 1, 1);
	}
	BEGIN_PROP_TABLE
		PROP_NAME(SocketName)
		PROP_NAME(BoneName)
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale)
	END_PROP_TABLE
};

#endif // UNREAL3


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
	//?? move outside
	TArray<FSkeletalMeshLODInfo> LODInfo;
	TArray<USkeletalMeshSocket*> Sockets;
	bool					bHasVertexColors;
#endif
#if BIOSHOCK
	TArray<UObject*>		havokObjects;	// wrappers for Havok objects used by this mesh; not used by Bioshock engine (precaching?)
#endif

	void UpgradeFaces();
	void UpgradeMesh();
	void RecreateMeshFromLOD(int LodIndex = 0, bool Force = false);
#if UNREAL1
	void SerializeSkelMesh1(FArchive &Ar);
#endif
#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif
#if UNREAL3
	void SerializeSkelMesh3(FArchive &Ar);
	void PostLoadMesh3();
#endif
#if BIOSHOCK
	void SerializeBioshockMesh(FArchive &Ar);
	void PostLoadBioshockMesh();
#endif

#if UNREAL3
	//!! separate class for UE3 !
	BEGIN_PROP_TABLE
		PROP_ARRAY(LODInfo, FSkeletalMeshLODInfo)
		PROP_ARRAY(Sockets, UObject*)
		PROP_BOOL(bHasVertexColors)
		PROP_DROP(SkelMeshGUID)
		PROP_DROP(SkelMirrorTable)
		PROP_DROP(FaceFXAsset)
		PROP_DROP(bDisableSkeletalAnimationLOD)
		PROP_DROP(bForceCPUSkinning)
		PROP_DROP(bUsePackedPosition)
		PROP_DROP(BoundsPreviewAsset)
		PROP_DROP(PerPolyCollisionBones)
		PROP_DROP(AddToParentPerPolyCollisionBone)
		PROP_DROP(bUseSimpleLineCollision)
		PROP_DROP(bUseSimpleBoxCollision)
		PROP_DROP(LODBiasPS3)
		PROP_DROP(LODBiasXbox360)
		PROP_DROP(ClothToGraphicsVertMap)
		PROP_DROP(ClothWeldingMap)
		PROP_DROP(ClothWeldingDomain)
		PROP_DROP(ClothWeldedIndices)
		PROP_DROP(NumFreeClothVerts)
		PROP_DROP(ClothIndexBuffer)
		PROP_DROP(ClothBones)
		PROP_DROP(bEnableClothPressure)
		PROP_DROP(bEnableClothDamping)
		PROP_DROP(ClothStretchStiffness)
		PROP_DROP(ClothDensity)
		PROP_DROP(ClothFriction)
		PROP_DROP(ClothTearFactor)
		PROP_DROP(SourceFilePath)
		PROP_DROP(SourceFileTimestamp)
#	if MEDGE
		PROP_DROP(NumUVSets)
#	endif
#	if BATMAN
		PROP_DROP(SkeletonName)
		PROP_DROP(Stretches)
#	endif // BATMAN
	END_PROP_TABLE
#endif // UNREAL3

#if UNREAL3
	USkeletalMesh()
	:	bHasVertexColors(false)
	{}
#endif // UNREAL3

	virtual void Serialize(FArchive &Ar);

	virtual void PostLoad()
	{
#if BIOSHOCK
	#if 0
		if (Package->Game == GAME_Bioshock)
	#else
		if (havokObjects.Num())			//?? ... UnPackage is not ready here ...
	#endif
			PostLoadBioshockMesh();		// should be called after loading of all used objects
#endif // BIOSHOCK
#if UNREAL3
		PostLoadMesh3();
#endif
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


/*-----------------------------------------------------------------------------
	Class registration
-----------------------------------------------------------------------------*/

//?? remove this macro (contents should go to REGISTER_MESH_CLASSES_U2)
#define REGISTER_MESH_CLASSES		\
	REGISTER_CLASS(USkeletalMesh)	\
	REGISTER_CLASS(UVertMesh)

// Note: we have registered UVertMesh and UMesh as ULodMesh too for UE1 compatibility
#define REGISTER_MESH_CLASSES_U1_A	\
	REGISTER_CLASS_ALIAS(UVertMesh, UMesh) \
	REGISTER_CLASS_ALIAS(UVertMesh, ULodMesh)

#define REGISTER_MESH_CLASSES_RUNE	\
	REGISTER_CLASS(USkelModel)

// UGolemSkeletalMesh - APB: Reloaded, derived from USkeletalMesh
#define REGISTER_MESH_CLASSES_U3_A	\
	REGISTER_CLASS(USkeletalMeshSocket) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS_ALIAS(USkeletalMesh, UGolemSkeletalMesh)


#endif // __UNMESH_H__
