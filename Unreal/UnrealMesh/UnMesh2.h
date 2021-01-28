#ifndef __UNMESH2_H__
#define __UNMESH2_H__


/*-----------------------------------------------------------------------------

UE2 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		UPrimitive
			ULodMesh
				USkeletalMesh
				UVertMesh
		UStaticMesh

-----------------------------------------------------------------------------*/

#include "UnMesh.h"			// common types

// forwards
class UMaterial;
class USkeletalMesh;
class UMeshAnimation;


//?? move this function from UnTexture.cpp to UnCore.cpp?
byte *FindXprData(const char *Name, int *DataSize);


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
	ULodMesh class and common data structures
-----------------------------------------------------------------------------*/

// Packed mesh vertex point for vertex meshes
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
	uint16			iWedge[3];				// Textured Vertex indices.
	uint16			MaterialIndex;			// Source Material (= texture plus unique flags) index.

	friend FArchive& operator<<(FArchive &Ar, FMeshFace &F)
	{
		Ar << F.iWedge[0] << F.iWedge[1] << F.iWedge[2];
		Ar << F.MaterialIndex;
		return Ar;
	}
};

SIMPLE_TYPE(FMeshFace, uint16)

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


// temp structure, skipped while mesh loading; used by UE1 UMesh only
// note: UE2 uses FMeshUVFloat
struct FMeshTri
{
	uint16			iVertex[3];				// Vertex indices.
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
	uint16			iVertex[3];				// Vertex indices.
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
	uint16			iVertex;				// Vertex index.
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
#if LINEAGE2
		if (Ar.Game == GAME_Lineage2 && Ar.ArVer >= 131)
		{
			int StringLen;
			Ar << StringLen;
			Ar.Seek(Ar.Tell() + StringLen * 2);
		}
#endif // LINEAGE2
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
				A.Groups.Add(tmpGroup);
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
	TArray<uint16>		FaceLevel;
	TArray<FMeshFace>	Faces;				// empty for USkeletalMesh
	TArray<uint16>		CollapseWedgeThus;	// ...
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
	TArray<uint16>	Indices;
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

// similar to VRawBoneInfluence, but used 'uint16' instead of 'int32'
struct FVertInfluence
{
	float			Weight;
	uint16			PointIndex;
	uint16			BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, FVertInfluence &I)
	{
		return Ar << I.Weight << I.PointIndex << I.BoneIndex;
	}
};

RAW_TYPE(FVertInfluence)


struct VWeightIndex
{
	TArray<uint16>	BoneInfIndices;			// array of vertex indices (in Points array)
	int				StartBoneInf;			// start index in BoneInfluences array

	friend FArchive& operator<<(FArchive &Ar, VWeightIndex &I)
	{
		return Ar << I.BoneInfIndices << I.StartBoneInf;
	}
};


struct VBoneInfluence						// Weight and bone number
{
	uint16			BoneWeight;				// 0..65535 == 0..1
	uint16			BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, VBoneInfluence &V)
	{
		return Ar << V.BoneWeight << V.BoneIndex;
	}
};

SIMPLE_TYPE(VBoneInfluence, uint16)


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
	uint16			WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte			MatIndex;				// Materials can be anything.
	byte			AuxMatIndex;			// Second material (unused).
	unsigned		SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend FArchive& operator<<(FArchive &Ar, VTriangle &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
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
	uint16			MaterialIndex;
	uint16			MinStreamIndex;			// rigid section only
	uint16			MinWedgeIndex;
	uint16			MaxWedgeIndex;
	uint16			NumStreamIndices;		// rigid section only
//	uint16			fA;						// not serialized, not used
	uint16			BoneIndex;				// rigid section only
	uint16			fE;						// serialized, not used
	uint16			FirstFace;
	uint16			NumFaces;
#if LINEAGE2
	TArray<int>		LineageBoneMap;			// for soft sections only
#endif
	// Rigid sections:
	//	MinWedgeIndex/MaxWedgeIndex -> FStaticLODModel.Wedges
	//	NumStreamIndices = NumFaces*3 == MaxWedgeIndex-MinWedgeIndex+1
	//	MinStreamIndex/NumStreamIndices -> FStaticLODModel.StaticIndices.Indices
	//	MinWedgeIndex -> FStaticLODModel.VertexStream.Verts
	// Soft sections:
	//	NumStreamIndices should contain MaxWedgeIndex-MinWedgeIndex+1 (new imported meshes?),
	//	but older package versions contains something different (should not rely on this field)
	//	Other fields are uninitialized

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
	uint16			f0;
	TArray<uint16>	f2;

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
	int16			X, Y, Z;

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
	int16			V;

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
	uint16			f0;
	TArray<uint16>	f4;

	friend FArchive& operator<<(FArchive &Ar, FUC2Unk2 &S)
	{
		return Ar << S.f0 << S.f4;
	}
};


#endif // UC2


struct FStaticLODModel
{
	TArray<uint32>			SkinningData;		// floating stream format, contains encoded UV and weights for each of SkinPoints
	TArray<FSkinPoint>		SkinPoints;			// soft surface points
	int						NumSoftWedges;		// number of wedges in soft sections
	TArray<FSkelMeshSection> SoftSections;
	TArray<FSkelMeshSection> RigidSections;
	FRawIndexBuffer			SoftIndices;
	FRawIndexBuffer			RigidIndices;
	FSkinVertexStream		VertexStream;		// for rigid parts
	TLazyArray<FVertInfluence> VertInfluences;
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
		Ar << M.SkinningData << M.SkinPoints << M.NumSoftWedges;
#if EOS
		if (Ar.Game == GAME_EOS && Ar.ArLicenseeVer >= 42)
		{
			TArray<int> unk;
			Ar << unk;
		}
#endif // EOS
		Ar << M.SoftSections << M.RigidSections << M.SoftIndices << M.RigidIndices;
		Ar << M.VertexStream;
		Ar << M.VertInfluences << M.Wedges << M.Faces << M.Points;
		Ar << M.LODDistanceFactor;
#if EOS
		if (Ar.Game == GAME_EOS && Ar.ArLicenseeVer >= 42) goto no_lod_hysteresis;
#endif
		Ar << M.LODHysteresis;
	no_lod_hysteresis:
		Ar << M.NumSharedVerts;
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
			// TArray of uint16[9]
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
				TArray<uint16>		fB0;
				int					fBC;
				FUC2Unk1			fC0;
				TArray<FUC2Unk2>	fEC;
				Ar << f98 << fB0 << fBC << fC0;
				if (Ar.ArVer >= 128)
					Ar << fEC;
			}
		}
#endif // UC2
#if VANGUARD
		if (Ar.Game == GAME_Vanguard && Ar.ArLicenseeVer >= 1)
		{
			TArray<unsigned> unkC;
			Ar << unkC;
		}
#endif // VANGUARD
		return Ar;

		unguard;
	}
};


// old USkeletalMesh
struct FLODMeshSection
{
	int16					f0, f2, f4;
	int16					LastIndex;
	int16					f8, fA, fC;
	int16					iFace;
	int16					f10;
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
	int16		f0[3];
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
	TLazyArray<FVertInfluence> VertInfluences;
	TLazyArray<uint16>		CollapseWedge;	// Num == Wedges.Num; used to automatically build StaticLODModels
	TLazyArray<uint16>		f1C8;			// Num == Triangles.Num, element = index in Points array; not used?
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
#if BIOSHOCK
	TArray<UObject*>		havokObjects;	// wrappers for Havok objects used by this mesh; not used by Bioshock engine (precaching?)
#endif

	CSkeletalMesh			*ConvertedMesh;

	USkeletalMesh();
	virtual ~USkeletalMesh();

	void ConvertMesh();
	void ConvertWedges(CSkelMeshLod &Lod, const TArray<FVector> &MeshPoints, const TArray<FMeshWedge> &MeshWedges, const TArray<FVertInfluence> &VertInfluences);
	void InitSections(CSkelMeshLod &Lod);
	void BuildIndices(CSkelMeshLod &Lod);
	void BuildIndicesForLod(CSkelMeshLod &Lod, const FStaticLODModel &SrcLod);
	bool IsCorrectLOD(const FStaticLODModel &Lod) const;

	void UpgradeFaces();
	void UpgradeMesh();
#if UNREAL1
	void SerializeSkelMesh1(FArchive &Ar);
#endif
#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif
#if BIOSHOCK
	void SerializeBioshockMesh(FArchive &Ar);
	void PostLoadBioshockMesh();
#endif

	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad();
};


#if RUNE

class USkelModel : public UPrimitive
{
	DECLARE_CLASS(USkelModel, UPrimitive);
public:
	// transient data
	TArray<USkeletalMesh*>	Meshes;
	UMeshAnimation			*Anim;
	virtual ~USkelModel();

	virtual void Serialize(FArchive &Ar);
};

#endif // RUNE


/*-----------------------------------------------------------------------------
	UMeshAnimation class
-----------------------------------------------------------------------------*/

// Additions: KeyPos array may be empty - in that case bone will be rotated only, no translation will be performed

struct AnalogTrack
{
	unsigned		Flags;					// reserved
	TArray<FQuat>	KeyQuat;				// Orientation key track (count = 1 or KeyTime.Count)
	TArray<FVector>	KeyPos;					// Position key track (count = 1 or KeyTime.Count)
	TArray<float>	KeyTime;				// For each key, time when next key takes effect (measured from start of track)

#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar);
#endif
#if SWRC
	void SerializeSWRC(FArchive &Ar);
#endif
#if UC1
	void SerializeUC1(FArchive &Ar);
#endif

	friend FArchive& operator<<(FArchive &Ar, AnalogTrack &A)
	{
		guard(AnalogTrack<<);
#if SPLINTER_CELL
		if (Ar.Game == GAME_SplinterCell && Ar.ArLicenseeVer >= 13)	// compressed Quat and Time tracks
		{
			A.SerializeSCell(Ar);
			return Ar;
		}
#endif // SPLINTER_CELL
#if SWRC
		if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 141)
		{
			A.SerializeSWRC(Ar);
			return Ar;
		}
#endif // SWRC
#if UC1
		if (Ar.Game == GAME_UC1 && Ar.ArLicenseeVer >= 28)
		{
			A.SerializeUC1(Ar);
			return Ar;
		}
#endif
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 147)
		{
			Ar << A.Flags; // other data serialized in a different way
			return Ar;
		}
#endif // UC2
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
#if SPLINTER_CELL || LINEAGE2
		// possibly M.Flags != 0, skip FlexTrack serializer
		if (Ar.Game == GAME_SplinterCell || Ar.Game == GAME_Lineage2)
			return Ar;
#endif
#if UNREAL25
		if (M.Flags >= 3)
			SerializeFlexTracks(Ar, M);		//!! make a method
#endif
#if TRIBES3
		if (Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4)
			FixTribesMotionChunk(M);		//!! make a method
#endif
		return Ar;
		unguard;
	}
};


// Named bone for the animating skeleton data.
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
		if (Ar.Engine() == GAME_UE2X)
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
 *	4			UE2Runtime, UC2, Harry Potter and the Prisoner of Azkaban
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

	CAnimSet				*ConvertedAnim;

	virtual ~UMeshAnimation();

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
#if SWRC
	void SerializeSWRCAnims(FArchive &Ar);
#endif
#if UC2
	bool SerializeUE2XMoves(FArchive &Ar);
#endif

	virtual void Serialize(FArchive &Ar)
	{
		guard(UMeshAnimation.Serialize);
		Super::Serialize(Ar);
		if (Ar.Game >= GAME_UE2)
			Ar << Version;					// no such field in UE1
		Ar << RefBones;
#if SWRC
		if (Ar.Game == GAME_RepCommando)
		{
			SerializeSWRCAnims(Ar);
			ConvertAnims();
			return;
		}
#endif // SWRC
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			if (!SerializeUE2XMoves(Ar))
			{
				// avoid assert
				DROP_REMAINING_DATA(Ar);
				ConvertAnims();
				return;
			}
		}
		else
#endif // UC2
#if LINEAGE2
		if (Ar.Game == GAME_Lineage2)
			SerializeLineageMoves(Ar);
		else
#endif // LINEAGE2
			Ar << Moves;
		Ar << AnimSeqs;
#if SPLINTER_CELL
		if (Ar.Game == GAME_SplinterCell)
			SerializeSCell(Ar);
#endif
#if UNREAL1
		if (Ar.Engine() == GAME_UE1) Upgrade();		// UE1 code
#endif
		ConvertAnims();

		unguard;
	}

	void ConvertAnims();
};


/*-----------------------------------------------------------------------------
	UStaticMesh class
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FkDOPNode;
struct FkDOPCollisionTriangle;
struct FStaticMeshTriangle;


struct FStaticMeshSection
{
	int						f4;				// always 0 ??
	uint16					FirstIndex;		// first index
	uint16					FirstVertex;	// first used vertex
	uint16					LastVertex;		// last used vertex
	uint16					fE;				// ALMOST always equals to f10
	uint16					NumFaces;		// number of faces in section
	// Note: UE2X uses "NumFaces" as "LastIndex"

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshSection &S)
	{
		guard(FStaticMeshSection<<);
		assert(Ar.ArVer >= 112);
		Ar << S.f4 << S.FirstIndex << S.FirstVertex << S.LastVertex << S.fE << S.NumFaces;
//		appPrintf("... f4=%d FirstIndex=%d FirstVertex=%d LastVertex=%d fE=%d NumFaces=%d\n", S.f4, S.FirstIndex, S.FirstVertex, S.LastVertex, S.fE, S.NumFaces); //!!!
		return Ar;
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

#if UC2

static FVector GUC2VectorScale, GUC2VectorBase;

struct FStaticMeshVertexUC2
{
	FUC2Vector				Pos;
	FUC2Normal				Normal;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexUC2 &V)
	{
		return Ar << V.Pos << V.Normal;
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos.X = Pos.X / 32767.0f / GUC2VectorScale.X + GUC2VectorBase.X;
		r.Pos.Y = Pos.Y / 32767.0f / GUC2VectorScale.Y + GUC2VectorBase.Y;
		r.Pos.Z = Pos.Z / 32767.0f / GUC2VectorScale.Z + GUC2VectorBase.Z;
		r.Normal.Set(0, 0, 0);		//?? decode
		return r;
	}
};

RAW_TYPE(FStaticMeshVertexUC2)

#endif // UC2

#if VANGUARD

extern bool GUseNewVanguardStaticMesh;

struct FStaticMeshVertexVanguard
{
	FVector					Pos;
	FVector					Normal;
	float					unk[8];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexVanguard &V)
	{
		Ar << V.Pos << V.Normal;
		for (int i = 0; i < 8; i++) Ar << V.unk[i];
		return Ar;
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos    = Pos;
		r.Normal = Normal;
		return r;
	}
};

SIMPLE_TYPE(FStaticMeshVertexVanguard, float)

#endif // VANGUARD

struct FStaticMeshVertexStream
{
	TArray<FStaticMeshVertex> Vert;
	int						Revision;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexStream &S)
	{
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			TArray<FStaticMeshVertexUC2> UC2Verts;
			int Flag;
			Ar << Flag;
			if (!Flag)
			{
				Ar << UC2Verts;
				CopyArray(S.Vert, UC2Verts);
			}
			return Ar << S.Revision;
		}
#endif // UC2
#if VANGUARD
		if (Ar.Game == GAME_Vanguard && GUseNewVanguardStaticMesh)
		{
			TArray<FStaticMeshVertexVanguard> VangVerts;
			Ar << VangVerts;
			CopyArray(S.Vert, VangVerts);
			return Ar << S.Revision;
		}
#endif // VANGUARD
#if TRIBES3
		TRIBES_HDR(Ar, 0xD);
#endif
		Ar << S.Vert << S.Revision;
#if TRIBES3
		if ((Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4) && t3_hdrSV >= 1)
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
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			int Flag;
			Ar << Flag;
			if (!Flag) Ar << S.Color;
			return Ar << S.Revision;
		}
#endif // UC2
		return Ar << S.Color << S.Revision;
	}
};


struct FStaticMeshUVStream
{
	TArray<FMeshUVFloat>	Data;
	int						f10;
	int						f1C;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVStream &S)
	{
#if BIOSHOCK
		if (Ar.Game == GAME_Bioshock)
		{
			if (Ar.ArLicenseeVer < 59)
			{
				// Bioshock 1 & Bioshock 2 MP
				Ar << S.Data;
			}
			else
			{
				// Bioshock 2 SP; real version detection code is unknown
				TArray<FMeshUVHalf> UV;
				Ar << UV;
				CopyArray(S.Data, UV);
			}
			return Ar << S.f10;
		}
#endif // BIOSHOCK
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			int Flag;
			Ar << Flag;
			if (!Flag) Ar << S.Data;
			return Ar << S.f10 << S.f1C;
		}
#endif // UC2
		return Ar << S.Data << S.f10 << S.f1C;
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
	FRawColorStream			ColorStream;	// for Verts
	FRawColorStream			AlphaStream;
	TArray<FStaticMeshUVStream> UVStream;
	FRawIndexBuffer			IndexStream1;
	FRawIndexBuffer			IndexStream2;
	TArray<FkDOPNode>		kDOPNodes;
	TArray<FkDOPCollisionTriangle> kDOPCollisionFaces;
	TLazyArray<FStaticMeshTriangle> Faces;
	UObject					*f108;			//?? collision?
	int						f124;			//?? FRotator?
	int						f128;
	int						f12C;
	int						InternalVersion;		// set to 11 when saving mesh
	TArray<float>			f150;
	int						f15C;
	UObject					*f16C;
	bool					UseSimpleLineCollision;
	bool					UseSimpleBoxCollision;
	bool					UseSimpleKarmaCollision;
	bool					UseVertexColor;

	CStaticMesh				*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, "FStaticMeshMaterial")
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
#if DECLARE_VIEWER_PROPS
		PROP_INT(InternalVersion)
#endif // DECLARE_VIEWER_PROPS
	END_PROP_TABLE

	UStaticMesh();
	virtual ~UStaticMesh();
	virtual void Serialize(FArchive &Ar);

#if UC2
	void LoadExternalUC2Data();
#endif
#if BIOSHOCK
	void SerializeBioshockMesh(FArchive &Ar);
#endif
#if VANGUARD
	void SerializeVanguardMesh(FArchive &Ar);
#endif
	void ConvertMesh();
};


/*-----------------------------------------------------------------------------
	Other games
-----------------------------------------------------------------------------*/

#if BIOSHOCK

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



/*-----------------------------------------------------------------------------
	Class registration
-----------------------------------------------------------------------------*/

#define REGISTER_MESH_CLASSES_U2	\
	REGISTER_CLASS(UVertMesh)		\
	REGISTER_CLASS(USkeletalMesh)	\
	REGISTER_CLASS(UMeshAnimation)	\
	REGISTER_CLASS(UStaticMesh)		\
	REGISTER_CLASS(FStaticMeshMaterial)

// Note: are have registered UMesh and ULodMesh as UVertMesh for UE1 compatibility
#define REGISTER_MESH_CLASSES_U1	\
	REGISTER_CLASS_ALIAS(UVertMesh, UMesh) \
	REGISTER_CLASS_ALIAS(UVertMesh, ULodMesh) \
	REGISTER_CLASS_ALIAS(UMeshAnimation, UAnimation)

#define REGISTER_MESH_CLASSES_RUNE	\
	REGISTER_CLASS(USkelModel)

#define REGISTER_MESH_CLASSES_BIO	\
	/*REGISTER_CLASS(USharedSkeletonDataMetadata)*/ \
	REGISTER_CLASS(UAnimationPackageWrapper)


#endif // __UNMESH2_H__
