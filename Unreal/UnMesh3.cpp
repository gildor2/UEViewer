#include "Core.h"

#if UNREAL3

#include "UnrealClasses.h"
#include "UnMesh3.h"
#include "UnMeshTypes.h"
#include "UnMathTools.h"			// for FRotator to FCoords
#include "UnMaterial3.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "TypeConvert.h"


//#define DEBUG_SKELMESH		1
//#define DEBUG_STATICMESH		1

#if DEBUG_SKELMESH
#define DBG_SKEL(...)			appPrintf(__VA_ARGS__);
#else
#define DBG_SKEL(...)
#endif

#if DEBUG_STATICMESH
#define DBG_STAT(...)			appPrintf(__VA_ARGS__);
#else
#define DBG_STAT(...)
#endif

//?? move outside?
/*
 * Half = Float16
 * http://www.openexr.com/  source: ilmbase-*.tar.gz/Half/toFloat.cpp
 * http://en.wikipedia.org/wiki/Half_precision
 * Also look at GL_ARB_half_float_pixel
 */
float half2float(uint16 h)
{
	union
	{
		float		f;
		unsigned	df;
	} f;

	int sign = (h >> 15) & 0x00000001;
	int exp  = (h >> 10) & 0x0000001F;
	int mant =  h        & 0x000003FF;

	exp  = exp + (127 - 15);
	f.df = (sign << 31) | (exp << 23) | (mant << 13);
	return f.f;
}


//!! RENAME to CopyNormals/ConvertNormals/PutNormals/RepackNormals etc
void UnpackNormals(const FPackedNormal SrcNormal[3], CMeshVertex &V)
{
	// tangents: convert to FVector (unpack) then cast to CVec3
	V.Tangent = CVT(SrcNormal[0]);
	V.Normal  = CVT(SrcNormal[2]);

	// new UE3 version - binormal is not serialized and restored in vertex shader

	if (SrcNormal[1].Data != 0)
	{
		// pack binormal sign into Normal.W
		FVector Tangent  = SrcNormal[0];
		FVector Binormal = SrcNormal[1];
		FVector Normal   = SrcNormal[2];
		CVec3   ComputedBinormal;
		cross(CVT(Normal), CVT(Tangent), ComputedBinormal);
		float Sign = dot(CVT(Binormal), ComputedBinormal);
		V.Normal.SetW(Sign > 0 ? 1.0f : -1.0f);
	}
}



/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

#define NUM_INFLUENCES_UE3			4
#define NUM_UV_SETS_UE3				4


#if NUM_INFLUENCES_UE3 != NUM_INFLUENCES
#error NUM_INFLUENCES_UE3 and NUM_INFLUENCES are not matching!
#endif

#if NUM_UV_SETS_UE3 > MAX_MESH_UV_SETS
#error NUM_UV_SETS_UE3 too large!
#endif


// Implement constructor in cpp to avoid inlining (it's large enough).
// It's useful to declare TArray<> structures as forward declarations in header file.
USkeletalMesh3::USkeletalMesh3()
:	bHasVertexColors(false)
,	ConvertedMesh(NULL)
#if BATMAN
,	EnableTwistBoneFixers(true)
,	EnableClavicleFixer(true)
#endif
{}


USkeletalMesh3::~USkeletalMesh3()
{
	delete ConvertedMesh;
}


#if MURDERED

struct FSkelMeshSection_MurderedUnk
{
	TArray<byte>		unk1, unk2;

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection_MurderedUnk &V)
	{
		Ar << V.unk1 << V.unk2;
		Ar.Seek(Ar.Tell() + 8 * sizeof(int16) + 2 * sizeof(int32) + 5 * sizeof(int16));
		return Ar;
	}
};

#endif // MURDERED


struct FSkelMeshSection3
{
	int16				MaterialIndex;
	int16				ChunkIndex;
	int					FirstIndex;
	int					NumTriangles;
	byte				unk2;

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection3 &S)
	{
		guard(FSkelMeshSection3<<);
		if (Ar.ArVer < 215)
		{
			// UE2 fields
			int16 FirstIndex;
			int16 unk1, unk2, unk3, unk4, unk5, unk6;
			TArray<int16> unk8;
			Ar << S.MaterialIndex << FirstIndex << unk1 << unk2 << unk3 << unk4 << unk5 << unk6 << S.NumTriangles;
			if (Ar.ArVer < 202) Ar << unk8;	// ArVer<202 -- from EndWar
			S.FirstIndex = FirstIndex;
			S.ChunkIndex = 0;
			return Ar;
		}
#if MURDERED
		if (Ar.Game == GAME_Murdered && Ar.ArLicenseeVer >= 85)
		{
			byte flag;
			Ar << flag;
			if (flag)
			{
				TArray<FSkelMeshSection_MurderedUnk> unk1;
				int unk2;
				Ar << unk1 << unk2;
			}
		}
#endif // MURDERED
		Ar << S.MaterialIndex << S.ChunkIndex << S.FirstIndex;
#if BATMAN
		if (Ar.Game == GAME_Batman3) goto old_section; // Batman1 and 2 has version smaller than 806; Batman3 has outdated code, but Batman4 - new one
#endif
#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 677) goto new_section; // MK X
#endif
		if (Ar.ArVer < 806)
		{
		old_section:
			// NumTriangles is 16-bit here
			uint16 NumTriangles;
			Ar << NumTriangles;
			S.NumTriangles = NumTriangles;
		}
		else
		{
		new_section:
			// NumTriangles is int
			Ar << S.NumTriangles;
		}
#if MCARTA
		if (Ar.Game == GAME_MagnaCarta && Ar.ArLicenseeVer >= 20)
		{
			int fC;
			Ar << fC;
		}
#endif // MCARTA
#if BLADENSOUL
		if (Ar.Game == GAME_BladeNSoul && Ar.ArVer >= 571) goto new_ver;
#endif
		if (Ar.ArVer >= 599)
		{
		new_ver:
			Ar << S.unk2;
		}
#if MASSEFF
		if (Ar.Game == GAME_MassEffect3 && Ar.ArLicenseeVer >= 135)
		{
			byte SomeFlag;
			Ar << SomeFlag;
			assert(SomeFlag == 0);
			// has extra data here in a case of SomeFlag != 0
		}
#endif // MASSEFF
#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3)
		{
			//!! the same code as for MassEffect3, combine?
			byte SomeFlag;
			Ar << SomeFlag;
			assert(SomeFlag == 0);
			// has extra data here in a case of SomeFlag != 0
		}
#endif // BIOSHOCK3
#if REMEMBER_ME
		if (Ar.Game == GAME_RememberMe && Ar.ArLicenseeVer >= 17)
		{
			byte SomeFlag;
			TArray<byte> SomeArray;
			Ar << SomeFlag;
			if (SomeFlag) Ar << SomeArray;
		}
#endif // REMEMBER_ME
#if LOST_PLANET3
		if (Ar.Game == GAME_LostPlanet3 && Ar.ArLicenseeVer >= 75)
		{
			byte SomeFlag;
			Ar << SomeFlag;
			assert(SomeFlag == 0);	// array of 40-byte structures (serialized size = 36)
		}
#endif // LOST_PLANET3
#if XCOM
		if (Ar.Game == GAME_XcomB)
		{
			int SomeFlag;
			Ar << SomeFlag;
			assert(SomeFlag == 0);	// extra data
		}
#endif // XCOM
		return Ar;
		unguard;
	}
};

// This class is used now for UStaticMesh only
struct FIndexBuffer3
{
	TArray<uint16>		Indices;

	friend FArchive& operator<<(FArchive &Ar, FIndexBuffer3 &I)
	{
		guard(FIndexBuffer3<<);

		int unk;						// Revision?
		I.Indices.BulkSerialize(Ar);
		if (Ar.ArVer < 297) Ar << unk;	// at older version compatible with FRawIndexBuffer
		return Ar;

		unguard;
	}
};

// real name (from Android version): FRawStaticIndexBuffer
struct FSkelIndexBuffer3				// differs from FIndexBuffer3 since version 806 - has ability to store int indices
{
	TArray<uint16>		Indices16;
	TArray<uint32>		Indices32;

	FORCEINLINE bool Is32Bit() const
	{
		return (Indices32.Num() != 0);
	}

	friend FArchive& operator<<(FArchive &Ar, FSkelIndexBuffer3 &I)
	{
		guard(FSkelIndexBuffer3<<);

		byte ItemSize = 2;

#if BATMAN
		if ((Ar.Game == GAME_Batman2 || Ar.Game == GAME_Batman3) && Ar.ArLicenseeVer >= 45)
		{
			int unk34;
			Ar << unk34;
			goto old_index_buffer;
		}
#endif // BATMAN

#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
		{
			// MK X
			Ar << I.Indices16 << I.Indices32;
			return Ar;
		}
#endif // MKVSDC

		if (Ar.ArVer >= 806)
		{
			int		f0;
			Ar << f0 << ItemSize;
		}
#if PLA
		if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
		{
			FGuid unk;
			Ar << unk;
		}
#endif // PLA
	old_index_buffer:
		if (ItemSize == 2)
			I.Indices16.BulkSerialize(Ar);
		else if (ItemSize == 4)
			I.Indices32.BulkSerialize(Ar);
		else
			appError("Unknown ItemSize %d", ItemSize);

		int unk;
		if (Ar.ArVer < 297) Ar << unk;	// at older version compatible with FRawIndexBuffer

		return Ar;

		unguard;
	}
};

struct FRigidVertex3
{
	FVector				Pos;
	FPackedNormal		Normal[3];
	FMeshUVFloat		UV[NUM_UV_SETS_UE3];
	byte				BoneIndex;
	int					Color;

	friend FArchive& operator<<(FArchive &Ar, FRigidVertex3 &V)
	{
		int NumUVSets = 1;

#if ENDWAR
		if (Ar.Game == GAME_EndWar)
		{
			// End War uses 4-component FVector everywhere, but here it is 3-component
			Ar << V.Pos.X << V.Pos.Y << V.Pos.Z;
			goto normals;
		}
#endif // ENDWAR
		Ar << V.Pos;
#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft)
		{
			if (Ar.ArLicenseeVer >= 1) Ar.Seek(Ar.Tell() + sizeof(float));
			if (Ar.ArLicenseeVer >= 2) NumUVSets = 4;
		}
#endif // CRIMECRAFT
#if DUNDEF
		if (Ar.Game == GAME_DunDef && Ar.ArLicenseeVer >= 21)
			NumUVSets = 4;
#endif // DUNDEF
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
	normals:
		Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
#if R6VEGAS
		if (Ar.Game == GAME_R6Vegas2 && Ar.ArLicenseeVer >= 63)
		{
			FMeshUVHalf hUV;
			Ar << hUV;
			V.UV[0] = hUV;			// convert
			goto influences;
		}
#endif // R6VEGAS

		// UVs
		if (Ar.ArVer >= 709) NumUVSets = 4;
#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 13) NumUVSets = 3;
#endif
#if MKVSDC || STRANGLE || TRANSFORMERS
		if ((Ar.Game == GAME_MK && Ar.ArLicenseeVer >= 11) ||
			Ar.Game == GAME_Strangle ||	// Stranglehold check MidwayVer >= 17
			(Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55))
			NumUVSets = 2;
#endif
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines)
		{
			if (Ar.ArLicenseeVer >= 3 && Ar.ArLicenseeVer <= 52)	// Frontlines (the code in Homefront uses different version comparison!)
				NumUVSets = 2;
			else if (Ar.ArLicenseeVer > 52)
			{
				byte Num;
				Ar << Num;
				NumUVSets = Num;
			}
		}
#endif // FRONTLINES
		// UV
		for (int i = 0; i < NumUVSets; i++)
			Ar << V.UV[i];

		if (Ar.ArVer >= 710) Ar << V.Color;	// default 0xFFFFFFFF

#if MCARTA
		if (Ar.Game == GAME_MagnaCarta && Ar.ArLicenseeVer >= 5)
		{
			int f20;
			Ar << f20;
		}
#endif // MCARTA
	influences:
		Ar << V.BoneIndex;
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 88)
		{
			int unk24;
			Ar << unk24;
		}
#endif // FRONTLINES
		return Ar;
	}
};


struct FSoftVertex3
{
	FVector				Pos;
	FPackedNormal		Normal[3];
	FMeshUVFloat		UV[NUM_UV_SETS_UE3];
	byte				BoneIndex[NUM_INFLUENCES_UE3];
	byte				BoneWeight[NUM_INFLUENCES_UE3];
	int					Color;

	friend FArchive& operator<<(FArchive &Ar, FSoftVertex3 &V)
	{
		int i;
		int NumUVSets = 1;

#if ENDWAR
		if (Ar.Game == GAME_EndWar)
		{
			// End War uses 4-component FVector everywhere, but here it is 3-component
			Ar << V.Pos.X << V.Pos.Y << V.Pos.Z;
			goto normals;
		}
#endif // ENDWAR
		Ar << V.Pos;
#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft)
		{
			if (Ar.ArLicenseeVer >= 1) Ar.Seek(Ar.Tell() + sizeof(float));
			if (Ar.ArLicenseeVer >= 2) NumUVSets = 4;
		}
#endif // CRIMECRAFT
#if DUNDEF
		if (Ar.Game == GAME_DunDef && Ar.ArLicenseeVer >= 21)
			NumUVSets = 4;
#endif // DUNDEF
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
	normals:
		Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
#if R6VEGAS
		if (Ar.Game == GAME_R6Vegas2 && Ar.ArLicenseeVer >= 63)
		{
			FMeshUVHalf hUV;
			Ar << hUV;
			V.UV[0] = hUV;			// convert
			goto influences;
		}
#endif // R6VEGAS

		// UVs
		if (Ar.ArVer >= 709) NumUVSets = 4;
#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 13) NumUVSets = 3;
#endif
#if MKVSDC || STRANGLE || TRANSFORMERS
		if ((Ar.Game == GAME_MK && Ar.ArLicenseeVer >= 11) ||
			Ar.Game == GAME_Strangle ||	// Stranglehold check MidwayVer >= 17
			(Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55))
			NumUVSets = 2;
#endif
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines)
		{
			if (Ar.ArLicenseeVer >= 3 && Ar.ArLicenseeVer <= 52)	// Frontlines (the code in Homefront uses different version comparison!)
				NumUVSets = 2;
			else if (Ar.ArLicenseeVer > 52)
			{
				byte Num;
				Ar << Num;
				NumUVSets = Num;
			}
		}
#endif // FRONTLINES
		// UV
		for (int i = 0; i < NumUVSets; i++)
			Ar << V.UV[i];

		if (Ar.ArVer >= 710) Ar << V.Color;	// default 0xFFFFFFFF

#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 88)
		{
			int unk24;
			Ar << unk24;
		}
#endif // FRONTLINES

	influences:
		if (Ar.ArVer >= 333)
		{
			for (i = 0; i < NUM_INFLUENCES_UE3; i++) Ar << V.BoneIndex[i];
			for (i = 0; i < NUM_INFLUENCES_UE3; i++) Ar << V.BoneWeight[i];
		}
		else
		{
			for (i = 0; i < NUM_INFLUENCES_UE3; i++)
				Ar << V.BoneIndex[i] << V.BoneWeight[i];
		}
#if MCARTA
		if (Ar.Game == GAME_MagnaCarta && Ar.ArLicenseeVer >= 5)
		{
			int f28;
			Ar << f28;
		}
#endif // MCARTA
		return Ar;
	}
};

#if MKVSDC
struct FMesh3Unk4_MK
{
	uint16				data[4];

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk4_MK &S)
	{
		return Ar << S.data[0] << S.data[1] << S.data[2] << S.data[3];
	}
};
#endif // MKVSDC

struct FSkelMeshChunk3
{
	int					FirstVertex;
	TArray<FRigidVertex3>  RigidVerts;
	TArray<FSoftVertex3>   SoftVerts;
	TArray<int16>		Bones;
	int					NumRigidVerts;
	int					NumSoftVerts;
	int					MaxInfluences;

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshChunk3 &V)
	{
		guard(FSkelMeshChunk3<<);
		Ar << V.FirstVertex << V.RigidVerts << V.SoftVerts;
#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 459)
		{
			TArray<FMesh3Unk4_MK> unk1C, unk28;
			Ar << unk1C << unk28;
		}
#endif // MKVSDC
		Ar << V.Bones;
		if (Ar.ArVer >= 333)
		{
			Ar << V.NumRigidVerts << V.NumSoftVerts;
			// note: NumRigidVerts and NumSoftVerts may be non-zero while corresponding
			// arrays are empty - that's when GPU skin only left
		}
		else
		{
			V.NumRigidVerts = V.RigidVerts.Num();
			V.NumSoftVerts = V.SoftVerts.Num();
		}
#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArLicenseeVer >= 7)
		{
			TArray<FMeshUVFloat> extraUV;
			extraUV.BulkSerialize(Ar);
		}
#endif // ARMYOF2
		if (Ar.ArVer >= 362)
			Ar << V.MaxInfluences;
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55)
		{
			int NumTexCoords;
			Ar << NumTexCoords;
		}
#endif // TRANSFORMERS
		DBG_SKEL("Chunk: FirstVert=%d RigidVerts=%d (%d) SoftVerts=%d (%d) MaxInfs=%d\n",
			V.FirstVertex, V.RigidVerts.Num(), V.NumRigidVerts, V.SoftVerts.Num(), V.NumSoftVerts, V.MaxInfluences);
		return Ar;
		unguard;
	}
};

struct FEdge3
{
	int					iVertex[2];
	int					iFace[2];

	friend FArchive& operator<<(FArchive &Ar, FEdge3 &V)
	{
#if BATMAN
		if (Ar.Game == GAME_Batman && Ar.ArLicenseeVer >= 5)
		{
			int16 iVertex[2], iFace[2];
			Ar << iVertex[0] << iVertex[1] << iFace[0] << iFace[1];
			V.iVertex[0] = iVertex[0];
			V.iVertex[1] = iVertex[1];
			V.iFace[0]   = iFace[0];
			V.iFace[1]   = iFace[1];
			return Ar;
		}
#endif // BATMAN
		return Ar << V.iVertex[0] << V.iVertex[1] << V.iFace[0] << V.iFace[1];
	}
};


// Structure holding normals and bone influeces
struct FGPUVert3Common
{
	FPackedNormal		Normal[3];
	byte				BoneIndex[NUM_INFLUENCES_UE3];
	byte				BoneWeight[NUM_INFLUENCES_UE3];

	void Serialize(FArchive &Ar)
	{
#if AVA
		if (Ar.Game == GAME_AVA) goto new_ver;
#endif
		if (Ar.ArVer < 494)
			Ar << Normal[0] << Normal[1] << Normal[2];
		else
		{
		new_ver:
			Ar << Normal[0] << Normal[2];
		}
#if CRIMECRAFT || FRONTLINES
		if ((Ar.Game == GAME_CrimeCraft && Ar.ArLicenseeVer >= 1) ||
			(Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 88))
			Ar.Seek(Ar.Tell() + sizeof(float)); // pad or vertex color?
#endif
		int i;
		for (i = 0; i < NUM_INFLUENCES_UE3; i++) Ar << BoneIndex[i];
		for (i = 0; i < NUM_INFLUENCES_UE3; i++) Ar << BoneWeight[i];
#if GUILTY
		if (Ar.Game == GAME_Guilty && Ar.ArLicenseeVer >= 1)
		{
			int unk;		// probably vertex color - this was removed from FStaticLODModel3 serializer
			Ar << unk;
		}
#endif // GUILTY
	}
};

static int GNumGPUUVSets = 1;

struct FGPUVert3Half : FGPUVert3Common
{
	FVector				Pos;
	FMeshUVHalf			UV[NUM_UV_SETS_UE3];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Half &V)
	{
		if (Ar.ArVer < 592) Ar << V.Pos;
		V.Serialize(Ar);
		if (Ar.ArVer >= 592) Ar << V.Pos;
		for (int i = 0; i < GNumGPUUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

struct FGPUVert3Float : FGPUVert3Common
{
	FVector				Pos;
	FMeshUVFloat		UV[NUM_UV_SETS_UE3];

	FGPUVert3Float& operator=(const FSoftVertex3 &S)
	{
		int i;
		Pos = S.Pos;
		Normal[0] = S.Normal[0];
		Normal[1] = S.Normal[1];
		Normal[2] = S.Normal[2];
		for (i = 0; i < NUM_UV_SETS_UE3; i++)
			UV[i] = S.UV[i];
		for (i = 0; i < NUM_INFLUENCES_UE3; i++)
		{
			BoneIndex[i]  = S.BoneIndex[i];
			BoneWeight[i] = S.BoneWeight[i];
		}
		return *this;
	}

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Float &V)
	{
		if (Ar.ArVer < 592) Ar << V.Pos;
		V.Serialize(Ar);
		if (Ar.ArVer >= 592) Ar << V.Pos;
		for (int i = 0; i < GNumGPUUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

struct FGPUVert3PackedHalf : FGPUVert3Common
{
	FVectorIntervalFixed32GPU Pos;
	FMeshUVHalf			UV[NUM_UV_SETS_UE3];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3PackedHalf &V)
	{
		V.Serialize(Ar);
		Ar << V.Pos;
		for (int i = 0; i < GNumGPUUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

struct FGPUVert3PackedFloat : FGPUVert3Common
{
	FVectorIntervalFixed32GPU Pos;
	FMeshUVFloat		UV[NUM_UV_SETS_UE3];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3PackedFloat &V)
	{
		V.Serialize(Ar);
		Ar << V.Pos;
		for (int i = 0; i < GNumGPUUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

#if BATMAN
struct FGPUVertBat4_HalfUV_Pos48 : FGPUVert3Common
{
	FVectorIntervalFixed64 Pos;
	FMeshUVHalf			UV[NUM_UV_SETS_UE3];

	friend FArchive& operator<<(FArchive &Ar, FGPUVertBat4_HalfUV_Pos48 &V)
	{
		V.Serialize(Ar);
		Ar << V.Pos;
		for (int i = 0; i < GNumGPUUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

struct FGPUVertBat4_HalfUV_Pos32 : FGPUVert3Common
{
	FVectorIntervalFixed32Bat4 Pos;
	FMeshUVHalf			UV[NUM_UV_SETS_UE3];

	friend FArchive& operator<<(FArchive &Ar, FGPUVertBat4_HalfUV_Pos32 &V)
	{
		V.Serialize(Ar);
		Ar << V.Pos;
		for (int i = 0; i < GNumGPUUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};
#endif // BATMAN

struct FSkeletalMeshVertexBuffer3
{
	int							NumUVSets;
	int							bUseFullPrecisionUVs;		// 0 = half, 1 = float; copy of corresponding USkeletalMesh field
	// compressed position data
	int							bUsePackedPosition;			// 1 = packed FVector (32-bit), 0 = FVector (96-bit)
	FVector						MeshOrigin;
	FVector						MeshExtension;
	// vertex sets
	TArray<FGPUVert3Half>		VertsHalf;					// only one of these vertex sets are used
	TArray<FGPUVert3Float>		VertsFloat;
	TArray<FGPUVert3PackedHalf> VertsHalfPacked;
	TArray<FGPUVert3PackedFloat> VertsFloatPacked;

	inline int GetVertexCount() const
	{
		if (VertsHalf.Num()) return VertsHalf.Num();
		if (VertsFloat.Num()) return VertsFloat.Num();
		if (VertsHalfPacked.Num()) return VertsHalfPacked.Num();
		if (VertsFloatPacked.Num()) return VertsFloatPacked.Num();
		return 0;
	}

	friend FArchive& operator<<(FArchive &Ar, FSkeletalMeshVertexBuffer3 &S)
	{
		guard(FSkeletalMeshVertexBuffer3<<);

		DBG_SKEL("Reading GPU skin\n");

		if (Ar.IsLoading) S.bUsePackedPosition = false;
		bool AllowPackedPosition = false;
		S.NumUVSets = GNumGPUUVSets = 1;

	#if HUXLEY
		if (Ar.Game == GAME_Huxley) goto old_version;
	#endif
	#if AVA
		if (Ar.Game == GAME_AVA)
		{
			// different ArVer to check
			if (Ar.ArVer < 441) goto old_version;
			else				goto new_version;
		}
	#endif // AVA
	#if FRONTLINES
		if (Ar.Game == GAME_Frontlines)
		{
			if (Ar.ArLicenseeVer < 11)
				goto old_version;
			if (Ar.ArVer < 493 )
				S.bUseFullPrecisionUVs = true;
			else
				Ar << S.bUseFullPrecisionUVs;
			int VertexSize, NumVerts;
			Ar << S.NumUVSets << VertexSize << NumVerts;
			GNumGPUUVSets = S.NumUVSets;
			goto serialize_verts;
		}
	#endif // FRONTLINES

	#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArLicenseeVer >= 74)
		{
			int UseNewFormat;
			Ar << UseNewFormat;
			if (UseNewFormat)
			{
				appError("ArmyOfTwo: new vertex format!");
				return Ar;
			}
		}
	#endif // ARMYOF2
		if (Ar.ArVer < 493)
		{
		old_version:
			// old version - FSoftVertex3 array
			TArray<FSoftVertex3> Verts;
			Verts.BulkSerialize(Ar);
			DBG_SKEL("... %d verts in old format\n", Verts.Num());
			// convert verts
			CopyArray(S.VertsFloat, Verts);
			S.bUseFullPrecisionUVs = true;
			return Ar;
		}

		// new version
	new_version:
		// serialize type information
	#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 15) goto get_UV_count;
	#endif
	#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55) goto get_UV_count; // 1 or 2
	#endif
	#if DUNDEF
		if (Ar.Game == GAME_DunDef && Ar.ArLicenseeVer >= 21) goto get_UV_count;
	#endif
		if (Ar.ArVer >= 709)
		{
		get_UV_count:
			Ar << S.NumUVSets;
			GNumGPUUVSets = S.NumUVSets;
		}
		Ar << S.bUseFullPrecisionUVs;
		if (Ar.ArVer >= 592)
			Ar << S.bUsePackedPosition << S.MeshExtension << S.MeshOrigin;

		if (Ar.Platform == PLATFORM_XBOX360 || Ar.Platform == PLATFORM_PS3) AllowPackedPosition = true;

	#if MOH2010
		if (Ar.Game == GAME_MOH2010) AllowPackedPosition = true;
	#endif
	#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 573) GNumGPUUVSets = S.NumUVSets = 2;	// Injustice
	#endif
	#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft && Ar.ArLicenseeVer >= 2) S.NumUVSets = GNumGPUUVSets = 4;
	#endif
	#if LOST_PLANET3
		if (Ar.Game == GAME_LostPlanet3 && Ar.ArLicenseeVer >= 75)
		{
			int NoUV;
			Ar << NoUV;
			assert(!NoUV);	// when NoUV - special vertex format used: FCompNormal[2] + FVector (or something like this?)
		}
	#endif // LOST_PLANET3

		// UE3 PC version ignored bUsePackedPosition - forced !bUsePackedPosition in FSkeletalMeshVertexBuffer3 serializer.
		// Note: in UDK (newer engine) there is no code to serialize GPU vertex with packed position.
		// Working bUsePackedPosition version was found in all XBox360 games. For PC there is only one game -
		// MOH2010, which uses bUsePackedPosition. PS3 also has bUsePackedPosition support (at least TRON)
		DBG_SKEL("... data: packUV:%d packPos:%d%s numUV:%d PackPos:(%g %g %g)+(%g %g %g)\n",
			!S.bUseFullPrecisionUVs, S.bUsePackedPosition, AllowPackedPosition ? "" : " (disabled)", S.NumUVSets,
			FVECTOR_ARG(S.MeshOrigin), FVECTOR_ARG(S.MeshExtension));
		if (!AllowPackedPosition) S.bUsePackedPosition = false;		// not used in games (see comment above)

	#if PLA
		if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
		{
			FGuid unk;
			Ar << unk;
		}
	#endif // PLA

	#if BATMAN
		if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 192)
		{
			S.Serialize_Batman4Verts(Ar);
			goto after_serialize_verts;
		}
	#endif // BATMAN

	serialize_verts:
		// serialize vertex array
		if (!S.bUseFullPrecisionUVs)
		{
			if (!S.bUsePackedPosition)
				S.VertsHalf.BulkSerialize(Ar);
			else
				S.VertsHalfPacked.BulkSerialize(Ar);
		}
		else
		{
			if (!S.bUsePackedPosition)
				S.VertsFloat.BulkSerialize(Ar);
			else
				S.VertsFloatPacked.BulkSerialize(Ar);
		}
	after_serialize_verts:
		DBG_SKEL("... verts: Half[%d] HalfPacked[%d] Float[%d] FloatPacked[%d]\n",
			S.VertsHalf.Num(), S.VertsHalfPacked.Num(), S.VertsFloat.Num(), S.VertsFloatPacked.Num());

	#if BATMAN
		if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 190)
		{
			TArray<FVector> unk1;
			int unk2;
			Ar << unk1 << unk2;
		}
	#endif // BATMAN

		return Ar;
		unguard;
	}

#if BATMAN
	void Serialize_Batman4Verts(FArchive &Ar)
	{
		guard(Serialize_Batman4Verts);
		//!! TODO: that'd be great to move this function to UnMeshBatman.cpp, but it requires vertex declatations.
		//!! Batman4 vertex is derived from FGPUVert3Common.

		int BatmanPackType;
		Ar << BatmanPackType;
		// 0 = FVector
		// 1 = int16[4]
		// 2 = int16[4]
		// 3 = int32 (packed)
		DBG_SKEL("... bat4 position pack: %d\n", BatmanPackType);

		if (!bUseFullPrecisionUVs)
		{
			// half-precision UVs
			switch (BatmanPackType)
			{
			case 1:
			case 2:
				{
					TArray<FGPUVertBat4_HalfUV_Pos48> Verts;
					Verts.BulkSerialize(Ar);
					// copy data
					VertsHalf.AddUninitialized(Verts.Num());
					for (int i = 0; i < Verts.Num(); i++)
					{
						const FGPUVertBat4_HalfUV_Pos48& S = Verts[i];
						FGPUVert3Half& D = VertsHalf[i];
						(FGPUVert3Common&)D = (FGPUVert3Common&)S;
						memcpy(D.UV, S.UV, sizeof(S.UV));
						D.Pos = S.Pos.ToVector(MeshOrigin, MeshExtension);
					}
				}
				return;
			case 3:
				{
					TArray<FGPUVertBat4_HalfUV_Pos32> Verts;
					Verts.BulkSerialize(Ar);
					// copy data
					VertsHalf.AddUninitialized(Verts.Num());
					for (int i = 0; i < Verts.Num(); i++)
					{
						const FGPUVertBat4_HalfUV_Pos32& S = Verts[i];
						FGPUVert3Half& D = VertsHalf[i];
						(FGPUVert3Common&)D = (FGPUVert3Common&)S;
						memcpy(D.UV, S.UV, sizeof(S.UV));
						D.Pos = S.Pos.ToVector(MeshOrigin, MeshExtension);
					}
				}
				return;
			default:
				VertsHalf.BulkSerialize(Ar);
				return;
			}
		}
		else
		{
			// float-precision UVs
			switch (BatmanPackType)
			{
			case 1:
			case 2:
			case 3:
				appError("Batman4 GPU vertex type %d", BatmanPackType);
//				VertsFloatPacked.BulkSerialize(Ar);
				return;
			default:
				VertsFloat.BulkSerialize(Ar);
				return;
			}
		}

		unguard;
	}
#endif // BATMAN

#if MKVSDC
	void Serialize_MK10(FArchive &Ar, int NumVertices)
	{
		guard(FSkeletalMeshVertexBuffer3::Serialize_MK10);

		// MK X vertex data
		// Position stream
		int PositionSize, NumPosVerts;
		Ar << PositionSize << NumPosVerts;
		assert(PositionSize == 12 && NumPosVerts == NumVertices);
		TArray<FVector> Positions;
		Ar << Positions;

		// Normal stream
		int NormalSize, NumNormals;
		Ar << NormalSize << NumNormals;
		assert(NormalSize == 8 && NumNormals == NumVertices);
		TArrayOfArray<FPackedNormal, 2> Normals;
		Ar << Normals;

		// Influence stream
		int InfSize, NumInfs;
		Ar << InfSize << NumInfs;
		assert(InfSize == 8 && NumInfs == NumVertices);
		TArray<int64> Infs;
		Ar << Infs;

		// UVs
		int UVSize, NumUVs;
		Ar << NumUVSets << UVSize << NumUVs;
		TArray<FMeshUVHalf> UVs;
		Ar << UVs;
		assert(NumUVs == NumVertices * NumUVSets);

		// skip everything else, would fail if number of LODs is > 1

		// combine vertex data
		VertsHalf.AddZeroed(NumVertices);
		int UVIndex = 0;
		for (int i = 0; i < NumVertices; i++)
		{
			FGPUVert3Half& V = VertsHalf[i];
			memset(&V, 0, sizeof(V));
			V.Pos = Positions[i];
			V.Normal[0] = Normals[i].Data[0];
			V.Normal[2] = Normals[i].Data[1];
			for (int j = 0; j < NumUVSets; j++)
			{
				V.UV[j] = UVs[UVIndex++];
			}
			// unpack influences
			// MK X allows more than 256 bones per section
			// stored as 10bit BoneIndices[4] + 8bit Weights[3]
			// 4th weight is restored in shader taking into account that sum is 255
			uint64 data = Infs[i];
			int b[4], w[4];
			b[0] = data & 0x3FF;
			b[1] = (data >> 10) & 0x3FF;
			b[2] = (data >> 20) & 0x3FF;
			b[3] = (data >> 30) & 0x3FF;
			w[0] = (data >> 40) & 0xFF;
			w[1] = (data >> 48) & 0xFF;
			w[2] = (data >> 56) & 0xFF;
			w[3] = 255 - w[0] - w[1] - w[2];
			for (int j = 0; j < 4; j++)
			{
				assert(b[j] < 256);
				V.BoneIndex[j] = b[j];
				V.BoneWeight[j] = w[j];
			}
		}

		unguard;
	}
#endif // MKVSDC
};

// real name: FVertexInfluence
struct FVertexInfluence
{
	int					Weights;
	int					Boned;

	friend FArchive& operator<<(FArchive &Ar, FVertexInfluence &S)
	{
		return Ar << S.Weights << S.Boned;
	}
};

SIMPLE_TYPE(FVertexInfluence, int)

// In UE4.0: TMap<FBoneIndexPair, TArray<uint16> >
struct FVertexInfluenceMapOld
{
	// key
	int					f0;
	int					f4;
	// value
	TArray<uint16>		f8;

	friend FArchive& operator<<(FArchive &Ar, FVertexInfluenceMapOld &S)
	{
		Ar << S.f0 << S.f4 << S.f8;
		return Ar;
	}
};

// In UE4.0: TMap<FBoneIndexPair, TArray<uint32> >
struct FVertexInfluenceMap
{
	// key
	int					f0;
	int					f4;
	// value
	TArray<int>			f8;

	friend FArchive& operator<<(FArchive &Ar, FVertexInfluenceMap &S)
	{
		Ar << S.f0 << S.f4 << S.f8;
		return Ar;
	}
};

struct FSkeletalMeshVertexInfluences
{
	TArray<FVertexInfluence>	Influences;
	TArray<FVertexInfluenceMap>	VertexInfluenceMapping;
	TArray<FSkelMeshSection3>	Sections;
	TArray<FSkelMeshChunk3>		Chunks;
	TArray<byte>				RequiredBones;
	byte						Usage;			// default = 0

	friend FArchive& operator<<(FArchive &Ar, FSkeletalMeshVertexInfluences &S)
	{
		guard(FSkeletalMeshVertexInfluences<<);
		DBG_SKEL("Extra vertex influence:\n");
		Ar << S.Influences;
		if (Ar.ArVer >= 609)
		{
			if (Ar.ArVer >= 808)
			{
				Ar << S.VertexInfluenceMapping;
			}
			else
			{
				byte unk1;
				if (Ar.ArVer >= 806) Ar << unk1;
				TArray<FVertexInfluenceMapOld> VertexInfluenceMappingOld;
				Ar << VertexInfluenceMappingOld;
			}
		}
		if (Ar.ArVer >= 700) Ar << S.Sections << S.Chunks;
		if (Ar.ArVer >= 708)
		{
#if BATMAN
			if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 159)
			{
				TArray<int> RequiredBones32;
				Ar << RequiredBones32;
			}
			else
#endif
			{
				Ar << S.RequiredBones;
			}
		}
		if (Ar.ArVer >= 715) Ar << S.Usage;
#if DEBUG_SKELMESH
		for (int i1 = 0; i1 < S.Sections.Num(); i1++)
		{
			FSkelMeshSection3 &Sec = S.Sections[i1];
			appPrintf("Sec[%d]: M=%d, FirstIdx=%d, NumTris=%d Chunk=%d\n", i1, Sec.MaterialIndex, Sec.FirstIndex, Sec.NumTriangles, Sec.ChunkIndex);
		}

#endif // DEBUG_SKELMESH
		return Ar;
		unguard;
	}
};

#if R6VEGAS

struct FMesh3R6Unk1
{
	byte				f[6];

	friend FArchive& operator<<(FArchive &Ar, FMesh3R6Unk1 &S)
	{
		Ar << S.f[0] << S.f[1] << S.f[2] << S.f[3] << S.f[4];
		if (Ar.ArLicenseeVer >= 47) Ar << S.f[5];
		return Ar;
	}
};

#endif // R6VEGAS

#if TRANSFORMERS

struct FTRMeshUnkStream
{
	int					ItemSize;
	int					NumVerts;
	TArray<int>			Data;			// TArray<FPackedNormal>

	friend FArchive& operator<<(FArchive &Ar, FTRMeshUnkStream &S)
	{
		Ar << S.ItemSize << S.NumVerts;
		if (S.ItemSize && S.NumVerts)
			S.Data.BulkSerialize(Ar);
		return Ar;
	}
};

#endif // TRANSFORMERS

// Version references: 180..240 - Rainbow 6: Vegas 2
// Other: GOW PC
struct FStaticLODModel3
{
	//!! field names (from UE4 source): f80 = Size, UsedBones = ActiveBoneIndices, f24 = RequiredBones
	TArray<FSkelMeshSection3> Sections;
	TArray<FSkelMeshChunk3>	Chunks;
	FSkelIndexBuffer3	IndexBuffer;
	TArray<int16>		UsedBones;		// bones, value = [0, NumBones-1]
	TArray<byte>		f24;			// count = NumBones, value = [0, NumBones-1]; note: BoneIndex is 'int16', not 'byte' ...
	TArray<uint16>		f68;			// indices, value = [0, NumVertices-1]
	TArray<byte>		f74;			// count = NumTriangles
	int					f80;
	int					NumVertices;
	TArray<FEdge3>		Edges;			// links 2 vertices and 2 faces (triangles)
	FWordBulkData		BulkData;		// ElementCount = NumVertices
	FIntBulkData		BulkData2;		// used instead of BulkData since version 806, indices?
	FSkeletalMeshVertexBuffer3 GPUSkin;
	TArray<FSkeletalMeshVertexInfluences> ExtraVertexInfluences;	// GoW2+ engine
	int					NumUVSets;
	TArray<int>			VertexColor;	// since version 710

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModel3 &Lod)
	{
		guard(FStaticLODModel3<<);

#if FURY
		if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 8)
		{
			// TLazyArray-like file pointer
			int EndArPos;
			Ar << EndArPos;
		}
#endif // FURY

#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 472 && Ar.Platform == PLATFORM_PS3)	//?? remove this code, doesn't work anyway
		{
			// this platform has no IndexBuffer
			Ar << Lod.Sections;
			goto part1;
		}
		if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
		{
			// MK X
			int unk;
			Ar << unk;			// serialized before FStaticLodModel, inside an array serializer function
		}
#endif // MKVSDC

		Ar << Lod.Sections << Lod.IndexBuffer;

	part1:
#if DEBUG_SKELMESH
		for (int i1 = 0; i1 < Lod.Sections.Num(); i1++)
		{
			FSkelMeshSection3 &S = Lod.Sections[i1];
			appPrintf("Sec[%d]: M=%d, FirstIdx=%d, NumTris=%d Chunk=%d\n", i1, S.MaterialIndex, S.FirstIndex, S.NumTriangles, S.ChunkIndex);
		}
		appPrintf("Indices: %d (16) / %d (32)\n", Lod.IndexBuffer.Indices16.Num(), Lod.IndexBuffer.Indices32.Num());
#endif // DEBUG_SKELMESH

		if (Ar.ArVer < 215)
		{
			TArray<FRigidVertex3>  RigidVerts;
			TArray<FSoftVertex3> SoftVerts;
			Ar << SoftVerts << RigidVerts;
			appNotify("SkeletalMesh: untested code! (ArVer=%d)", Ar.ArVer);
		}

#if ENDWAR || BORDERLANDS
		if (Ar.Game == GAME_EndWar || (Ar.Game == GAME_Borderlands && Ar.ArLicenseeVer >= 57))	//!! Borderlands version unknown
		{
			// refined field set
			Ar << Lod.UsedBones << Lod.Chunks << Lod.f80 << Lod.NumVertices;
			goto part2;
		}
#endif // ENDWAR || BORDERLANDS

#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArVer >= 536)
		{
			// Transformers: Dark of the Moon
			// refined field set + byte bone indices
			assert(Ar.ArLicenseeVer >= 152);		// has mixed version comparisons - ArVer >= 536 and ArLicenseeVer >= 152
			TArray<byte> UsedBones2;
			Ar << UsedBones2;
			CopyArray(Lod.UsedBones, UsedBones2);	// byte -> int
			Ar << Lod.Chunks << Lod.NumVertices;
			goto part2;
		}
#endif // TRANSFORMERS

		if (Ar.ArVer < 686) Ar << Lod.f68;
		Ar << Lod.UsedBones;
		if (Ar.ArVer < 686) Ar << Lod.f74;
		if (Ar.ArVer >= 215)
		{
			Ar << Lod.Chunks << Lod.f80 << Lod.NumVertices;
		}
		DBG_SKEL("%d chunks, %d bones, %d verts\n", Lod.Chunks.Num(), Lod.UsedBones.Num(), Lod.NumVertices);

#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 11)
		{
			int unk84;
			Ar << unk84;	// default is 1
		}
#endif

		if (Ar.ArVer < 686) Ar << Lod.Edges;

		if (Ar.ArVer < 202)
		{
#if 0
			// old version
			TLazyArray<FVertInfluence> Influences;
			TLazyArray<FMeshWedge>     Wedges;
			TLazyArray<FMeshFace>      Faces;
			TLazyArray<FVector>        Points;
			Ar << Influences << Wedges << Faces << Points;
#else
			appError("Old UE3 FStaticLodModel");
#endif
		}

#if STRANGLE
		if (Ar.Game == GAME_Strangle)
		{
			// also check MidwayTag == "WOO " and MidwayVer >= 346
			// f24 has been moved to the end
			Lod.BulkData.Serialize(Ar);
			Ar << Lod.GPUSkin;
			Ar << Lod.f24;
			return Ar;
		}
#endif // STRANGLE

#if DMC
		if (Ar.Game == GAME_DmC && Ar.ArLicenseeVer >= 3) goto word_f24;
#endif

#if BATMAN
		if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 159)
		{
			TArray<int> f24_new;
			Ar << f24_new;
			goto bulk;
		}
#endif // BATMAN

	part2:
		if (Ar.ArVer >= 207)
		{
			Ar << Lod.f24;
		}
		else
		{
		word_f24:
			TArray<int16> f24_a;
			Ar << f24_a;
		}
#if APB
		if (Ar.Game == GAME_APB)
		{
			// skip APB bulk; for details check UTexture3::Serialize()
			Ar.Seek(Ar.Tell() + 8);
			goto after_bulk;
		}
#endif // APB
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 181) // Transformers: Fall of Cybertron, no version in code
			goto after_bulk;
#endif
		/*!! PS3 MK:
			- no Bulk here
			- int NumSections (equals to Sections.Num())
			- int 2 or 4
			- 4 x int unknown
			- int SomeSize
			- byte[SomeSize]
			- uint16 SomeSize (same as above)
			- ...
		*/
	bulk:
		if (Ar.ArVer >= 221)
		{
			if (Ar.ArVer < 806)
				Lod.BulkData.Serialize(Ar);		// Bulk of uint16
			else
				Lod.BulkData2.Serialize(Ar);	// Bulk of int
		}
	after_bulk:
#if R6VEGAS
		if (Ar.Game == GAME_R6Vegas2 && Ar.ArLicenseeVer >= 46)
		{
			TArray<FMesh3R6Unk1> unkA0;
			Ar << unkA0;
		}
#endif // R6VEGAS
#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArLicenseeVer >= 7)
		{
			int unk84;
			TArray<FMeshUVFloat> extraUV;
			Ar << unk84;
			extraUV.BulkSerialize(Ar);
		}
#endif // ARMYOF2
#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3)
		{
			int unkE4;
			Ar << unkE4;
		}
#endif // BIOSHOCK3
#if REMEMBER_ME
		if (Ar.Game == GAME_RememberMe && Ar.ArLicenseeVer >= 19)
		{
			int unkD4;
			Ar << unkD4;
			if (unkD4) appError("RememberMe: new vertex buffer format");
		}
#endif // REMEMBER_ME
#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
		{
			FByteBulkData UnkBulk;
			UnkBulk.Skip(Ar);
			Lod.GPUSkin.Serialize_MK10(Ar, Lod.NumVertices);
			Lod.NumUVSets = Lod.GPUSkin.NumUVSets;
			return Ar;
		}
#endif // MKVSDC

		// UV sets count
		Lod.NumUVSets = 1;
		if (Ar.ArVer >= 709)
			Ar << Lod.NumUVSets;
#if DUNDEF
		if (Ar.Game == GAME_DunDef && Ar.ArLicenseeVer >= 21)
			Ar << Lod.NumUVSets;
#endif
		DBG_SKEL("NumUVSets=%d\n", Lod.NumUVSets);

#if MOH2010
		int RealArVer = Ar.ArVer;
		if (Ar.Game == GAME_MOH2010)
		{
			Ar.ArVer = 592;			// partially upgraded engine, override version (for easier coding)
			if (Ar.ArLicenseeVer >= 42)
				Ar << Lod.ExtraVertexInfluences;	// original code: this field is serialized after GPU Skin
		}
#endif // MOH2010
#if FURY
		if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 34)
		{
			int gpuSkinUnk;
			Ar << gpuSkinUnk;
		}
#endif // FURY
		if (Ar.ArVer >= 333)
			Ar << Lod.GPUSkin;
#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 459)
		{
			TArray<FMesh3Unk4_MK> unkA8;
			Ar << unkA8;
			return Ar;		// no extra fields
		}
#endif // MKVSDC
#if MOH2010
		if (Ar.Game == GAME_MOH2010)
		{
			Ar.ArVer = RealArVer;	// restore version
			if (Ar.ArLicenseeVer >= 42) return Ar;
		}
#endif
#if BLOODONSAND
		if (Ar.Game == GAME_50Cent) return Ar;	// new ArVer, but old engine
#endif
#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 15) return Ar;	// new ArVer, but old engine
#endif // MEDGE
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 73)
		{
			FTRMeshUnkStream unkStream;
			Ar << unkStream;
			return Ar;
		}
#endif // TRANSFORMERS
#if GUILTY
		if (Ar.Game == GAME_Guilty && Ar.ArLicenseeVer >= 1) goto no_vert_color;
#endif
		if (Ar.ArVer >= 710)
		{
			USkeletalMesh3 *LoadingMesh = (USkeletalMesh3*)UObject::GLoadingObj;
			assert(LoadingMesh);
			if (LoadingMesh->bHasVertexColors)
			{
#if PLA
				if (Ar.Game == GAME_PLA && Ar.ArVer >= 900) // this code was guessed, not checked in executable
				{
					FGuid unk;
					Ar << unk;
				}
#endif // PLA
				Lod.VertexColor.BulkSerialize(Ar);
				appPrintf("INFO: SkeletalMesh %s has vertex colors\n", LoadingMesh->Name);
			}
		}
	no_vert_color:
		if (Ar.ArVer >= 534)		// post-UT3 code
			Ar << Lod.ExtraVertexInfluences;
		if (Ar.ArVer >= 841)		// unknown extra index buffer
		{
			FSkelIndexBuffer3 unk;
			Ar << unk;
		}
//		assert(Lod.IndexBuffer.Indices.Num() == Lod.f68.Num()); -- mostly equals (failed in CH_TwinSouls_Cine.upk)
//		assert(Lod.BulkData.ElementCount == Lod.NumVertices); -- mostly equals (failed on some GoW packages)

#if BATMAN
		if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 107)
		{
			TArray<int> unk;
			Ar << unk;
		}
#endif // BATMAN

		return Ar;

		unguard;
	}
};

#if A51 || MKVSDC || STRANGLE || TNA_IMPACT

struct FMaterialBone
{
	int					Bone;
	FName				Param;

	friend FArchive& operator<<(FArchive &Ar, FMaterialBone &V)
	{
		if (Ar.ArVer >= 573) // Injustice, version unknown
		{
			int f1[6], f2[6];
			Ar << f1[0] << f1[1] << f1[2] << f1[3] << f1[4] << f1[5];
			Ar << V.Param;
			Ar << f2[0] << f2[1] << f2[2] << f2[3] << f2[4] << f2[5];
			return Ar;
		}
		return Ar << V.Bone << V.Param;
	}
};

struct FSubSkeleton_MK
{
	FName				Name;
	int					BoneSet[8];			// FBoneSet

	friend FArchive& operator<<(FArchive &Ar, FSubSkeleton_MK &S)
	{
		Ar << S.Name;
		for (int i = 0; i < ARRAY_COUNT(S.BoneSet); i++) Ar << S.BoneSet[i];
		return Ar;
	}
};

struct FBoneMirrorInfo_MK
{
	int					SourceIndex;
	byte				BoneFlipAxis;		// EAxis

	friend FArchive& operator<<(FArchive &Ar, FBoneMirrorInfo_MK &M)
	{
		return Ar << M.SourceIndex << M.BoneFlipAxis;
	}
};

struct FReferenceSkeleton_MK
{
	TArray<VJointPos>	RefPose;
	TArray<int16>		Parentage;
	TArray<FName>		BoneNames;
	TArray<FSubSkeleton_MK> SubSkeletons;
	TArray<FName>		UpperBoneNames;
	TArray<FBoneMirrorInfo_MK> SkelMirrorTable;
	byte				SkelMirrorAxis;		// EAxis
	byte				SkelMirrorFlipAxis;	// EAxis

	friend FArchive& operator<<(FArchive &Ar, FReferenceSkeleton_MK &S)
	{
		Ar << S.RefPose << S.Parentage << S.BoneNames;
		Ar << S.SubSkeletons;				// MidwayVer >= 56
		Ar << S.UpperBoneNames;				// MidwayVer >= 57
		Ar << S.SkelMirrorTable << S.SkelMirrorAxis << S.SkelMirrorFlipAxis;
		return Ar;
	}
};

#endif // MIDWAY ...

#if FURY
struct FSkeletalMeshLODInfoExtra
{
	int					IsForGemini;	// bool
	int					IsForTaurus;	// bool

	friend FArchive& operator<<(FArchive &Ar, FSkeletalMeshLODInfoExtra &V)
	{
		return Ar << V.IsForGemini << V.IsForTaurus;
	}
};
#endif // FURY

#if BATMAN
struct FBoneBounds
{
	int					BoneIndex;
	// FSimpleBox
	FVector				Min;
	FVector				Max;

	friend FArchive& operator<<(FArchive &Ar, FBoneBounds &B)
	{
		return Ar << B.BoneIndex << B.Min << B.Max;
	}
};
#endif // BATMAN

#if LEGENDARY

struct FSPAITag2
{
	UObject				*f0;
	int					f4;

	friend FArchive& operator<<(FArchive &Ar, FSPAITag2 &S)
	{
		Ar << S.f0;
		if (Ar.ArLicenseeVer < 10)
		{
			byte f4;
			Ar << f4;
			S.f4 = f4;
			return Ar;
		}
		int f4[9];		// serialize each bit of S.f4 as separate dword
		Ar << f4[0] << f4[1] << f4[2] << f4[3] << f4[4] << f4[5];
		if (Ar.ArLicenseeVer >= 23) Ar << f4[6];
		if (Ar.ArLicenseeVer >= 31) Ar << f4[7];
		if (Ar.ArLicenseeVer >= 34) Ar << f4[8];
		return Ar;
	}
};

#endif // LEGENDARY

#if MKVSDC

void USkeleton_MK::Serialize(FArchive &Ar)
{
	guard(USkeleton_MK::Serialize);
	assert(Ar.Game == GAME_MK);
	Super::Serialize(Ar);

	Ar << BonePos;
	Ar << BoneParent;
	Ar << BoneName;

	DROP_REMAINING_DATA(Ar);

	unguard;
}

#endif // MKVSDC

void USkeletalMesh3::Serialize(FArchive &Ar)
{
	guard(USkeletalMesh3::Serialize);

#if FRONTLINES
	if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 88)
	{
		int unk320;					// default 1
		Ar << unk320;
	}
#endif // FRONTLINES

	UObject::Serialize(Ar);			// no UPrimitive ...

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		Ar << LODModels;
		// MK X has 1 LODModel and 4 LODInfo
		if (LODInfo.Num() > LODModels.Num())
			LODInfo.RemoveAt(LODModels.Num(), LODInfo.Num() - LODModels.Num());
		DROP_REMAINING_DATA(Ar);
		return;
	}
#endif // MKVSDC

#if MEDGE
	if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 15)
	{
		int unk264;
		Ar << unk264;
	}
#endif // MEDGE
#if FURY
	if (Ar.Game == GAME_Fury)
	{
		int b1, b2, b3, b4;		// bools, serialized as ints; LoadForGemini, LoadForTaurus, IsSceneryMesh, IsPlayerMesh
		TArray<FSkeletalMeshLODInfoExtra> LODInfoExtra;
		if (Ar.ArLicenseeVer >= 14) Ar << b1 << b2;
		if (Ar.ArLicenseeVer >= 8)
		{
			Ar << b3;
			Ar << LODInfoExtra;
		}
		if (Ar.ArLicenseeVer >= 15) Ar << b4;
	}
#endif // FURY
#if DISHONORED
	if (Ar.Game == GAME_Dishonored && Ar.ArVer >= 759)
	{
		// FUserBounds m_UserBounds
		FName   m_BoneName;
		FVector m_Offset;
		float   m_fRadius;
		Ar << m_BoneName;
		if (Ar.ArVer >= 761) Ar << m_Offset;
		Ar << m_fRadius;
	}
#endif // DISHONORED
	Ar << Bounds;
	if (Ar.ArVer < 180)
	{
		UObject *unk;
		Ar << unk;
	}
#if BATMAN
	if (Ar.Game >= GAME_Batman && Ar.Game <= GAME_Batman4 && Ar.ArLicenseeVer >= 15)
	{
		float ConservativeBounds;
		TArray<FBoneBounds> PerBoneBounds;
		Ar << ConservativeBounds << PerBoneBounds;
	}
#endif // BATMAN
	Ar << Materials;
#if DEBUG_SKELMESH
	for (int i1 = 0; i1 < Materials.Num(); i1++)
		appPrintf("Material[%d] = %s\n", i1, Materials[i1] ? Materials[i1]->Name : "None");
#endif
#if BLOODONSAND
	if (Ar.Game == GAME_50Cent && Ar.ArLicenseeVer >= 65)
	{
		TArray<UObject*> OnFireMaterials;		// name is not checked
		Ar << OnFireMaterials;
	}
#endif // BLOODONSAND
#if DARKVOID
	if (Ar.Game == GAME_DarkVoid && Ar.ArLicenseeVer >= 61)
	{
		TArray<UObject*> AlternateMaterials;
		Ar << AlternateMaterials;
	}
#endif // DARKVOID
#if ALPHA_PR
	if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 26)
	{
		TArray<int> SectionDepthBias;
		Ar << SectionDepthBias;
	}
#endif // ALPHA_PR
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 472)	// MK, real version is unknown
	{
		FReferenceSkeleton_MK Skel;
		Ar << Skel << SkeletalDepth;
		MeshOrigin.Set(0, 0, 0);				// not serialized
		RotOrigin.Set(0, 0, 0);
		// convert skeleton
		int NumBones = Skel.RefPose.Num();
		assert(NumBones == Skel.Parentage.Num());
		assert(NumBones == Skel.BoneNames.Num());
		RefSkeleton.AddDefaulted(NumBones);
		for (int i = 0; i < NumBones; i++)
		{
			FMeshBone &B = RefSkeleton[i];
			B.Name        = Skel.BoneNames[i];
			B.BonePos     = Skel.RefPose[i];
			B.ParentIndex = Skel.Parentage[i];
//			appPrintf("BONE: [%d] %s -> %d\n", i, *B.Name, B.ParentIndex);
		}
		goto after_skeleton;
	}
#endif // MKVSDC
#if ALIENS_CM
	if (Ar.Game == GAME_AliensCM && Ar.ArVer >= 21)	// && Ar.ExtraVer >= 1
	{
		TArray<UObject*> unk;
		Ar << unk;
	}
#endif // ALIENS_CM
	Ar << MeshOrigin << RotOrigin;
#if DISHONORED
	if (Ar.Game == GAME_Dishonored && Ar.ArVer >= 775)
	{
		TArray<byte> EdgeSkeleton;
		Ar << EdgeSkeleton;
	}
#endif // DISHONORED
#if SOV
	if (Ar.Game == GAME_SOV && Ar.ArVer >= 850)
	{
		FVector unk;
		Ar << unk;
	}
#endif // SOV
	Ar << RefSkeleton << SkeletalDepth;
after_skeleton:
#if DEBUG_SKELMESH
	appPrintf("RefSkeleton: %d bones, %d depth\n", RefSkeleton.Num(), SkeletalDepth);
	for (int i1 = 0; i1 < RefSkeleton.Num(); i1++)
		appPrintf("  [%d] n=%s p=%d\n", i1, *RefSkeleton[i1].Name, RefSkeleton[i1].ParentIndex);
#endif // DEBUG_SKELMESH
#if A51 || MKVSDC || STRANGLE || TNA_IMPACT
	//?? check GAME_Wheelman
	if (Ar.Engine() == GAME_MIDWAY3 && Ar.ArLicenseeVer >= 15)
	{
		TArray<FMaterialBone> MaterialBones;
		Ar << MaterialBones;
	}
#endif // A51 || MKVSDC || STRANGLE
#if CRIMECRAFT
	if (Ar.Game == GAME_CrimeCraft && Ar.ArLicenseeVer >= 5)
	{
		byte unk8C;
		Ar << unk8C;
	}
#endif
#if LEGENDARY
	if (Ar.Game == GAME_Legendary && Ar.ArLicenseeVer >= 9)
	{
		TArray<FSPAITag2> OATStags2;
		Ar << OATStags2;
	}
#endif
#if SPECIALFORCE2
	if (Ar.Game == GAME_SpecialForce2 && Ar.ArLicenseeVer >= 14)
	{
		byte  unk108;
		FName unk10C;
		Ar << unk108 << unk10C;
	}
#endif
	Ar << LODModels;
#if 0
	//!! also: NameIndexMap (ArVer >= 296), PerPolyKDOPs (ArVer >= 435)
#else
	DROP_REMAINING_DATA(Ar);
#endif

	unguard;
}


void USkeletalMesh3::ConvertMesh()
{
	guard(USkeletalMesh3::ConvertMesh);

	CSkeletalMesh *Mesh = new CSkeletalMesh(this);
	ConvertedMesh = Mesh;

	int ArGame = GetGame();

#if MKVSDC
	if (ArGame == GAME_MK && Skeleton != NULL && RefSkeleton.Num() == 0)
	{
		// convert MK X USkeleton to RefSkeleton
		int NumBones = Skeleton->BoneName.Num();
		assert(Skeleton->BoneParent.Num() == NumBones && Skeleton->BonePos.Num() == NumBones);
		RefSkeleton.AddDefaulted(NumBones);
		for (int i = 0; i < NumBones; i++)
		{
			FMeshBone& B = RefSkeleton[i];
			B.Name        = Skeleton->BoneName[i];
			B.BonePos     = Skeleton->BonePos[i];
			B.ParentIndex = Skeleton->BoneParent[i];
		}
	}
#endif // MKVSDC

	if (!RefSkeleton.Num())
	{
		appNotify("SkeletalMesh with no skeleton");
		return;
	}

	// convert bounds
	Mesh->BoundingSphere.R = Bounds.SphereRadius / 2;		//?? UE3 meshes has radius 2 times larger than mesh
	VectorSubtract(CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Min));
	VectorAdd     (CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Max));

	// MeshScale, MeshOrigin, RotOrigin
	VectorScale(CVT(MeshOrigin), -1, Mesh->MeshOrigin);
	Mesh->RotOrigin = RotOrigin;
	Mesh->MeshScale.Set(1, 1, 1);							// missing in UE3

	// convert LODs
	Mesh->Lods.Empty(LODModels.Num());
	assert(LODModels.Num() == LODInfo.Num());
	for (int lod = 0; lod < LODModels.Num(); lod++)
	{
		guard(ConvertLod);

		const FStaticLODModel3 &SrcLod = LODModels[lod];
		if (!SrcLod.Chunks.Num()) continue;

		int NumTexCoords = max(SrcLod.NumUVSets, SrcLod.GPUSkin.NumUVSets);		// number of texture coordinates is serialized differently for some games
		if (NumTexCoords > MAX_MESH_UV_SETS)
			appError("SkeletalMesh has %d UV sets", NumTexCoords);

		CSkelMeshLod *Lod = new (Mesh->Lods) CSkelMeshLod;
		Lod->NumTexCoords = NumTexCoords;
		Lod->HasNormals   = true;
		Lod->HasTangents  = true;

		guard(ProcessVerts);

		// get vertex count and determine vertex source
		int VertexCount = SrcLod.GPUSkin.GetVertexCount();
		bool UseGpuSkinVerts = (VertexCount > 0);
		if (!VertexCount)
		{
			const FSkelMeshChunk3 &C = SrcLod.Chunks[SrcLod.Chunks.Num() - 1];		// last chunk
			VertexCount = C.FirstVertex + C.NumRigidVerts + C.NumSoftVerts;
		}
		// allocate the vertices
		Lod->AllocateVerts(VertexCount);

		int chunkIndex = 0;
		const FSkelMeshChunk3 *C = NULL;
		int lastChunkVertex = -1;
		const FSkeletalMeshVertexBuffer3 &S = SrcLod.GPUSkin;
		CSkelMeshVertex *D = Lod->Verts;
		int NumReweightedVerts = 0;

		for (int Vert = 0; Vert < VertexCount; Vert++, D++)
		{
			if (Vert >= lastChunkVertex)
			{
				// proceed to next chunk
				C = &SrcLod.Chunks[chunkIndex++];
				lastChunkVertex = C->FirstVertex + C->NumRigidVerts + C->NumSoftVerts;
			}

			if (UseGpuSkinVerts)
			{
				// NOTE: Gears3 has some issues:
				// - chunk may have FirstVertex set to incorrect value (for recent UE3 versions), which overlaps with the
				//   previous chunk (FirstVertex=0 for a few chunks)
				// - index count may be greater than sum of all face counts * 3 from all mesh sections -- this is verified in PSK exporter

				// get vertex from GPU skin
				const FGPUVert3Common *V;		// has normal and influences, but no UV[] and position

				if (!S.bUseFullPrecisionUVs)
				{
					// position
					const FMeshUVHalf *SUV;
					if (!S.bUsePackedPosition)
					{
						const FGPUVert3Half &V0 = S.VertsHalf[Vert];
						D->Position = CVT(V0.Pos);
						V   = &V0;
						SUV = V0.UV;
					}
					else
					{
						const FGPUVert3PackedHalf &V0 = S.VertsHalfPacked[Vert];
						FVector VPos;
						VPos = V0.Pos.ToVector(S.MeshOrigin, S.MeshExtension);
						D->Position = CVT(VPos);
						V   = &V0;
						SUV = V0.UV;
					}
					// UV
					FMeshUVFloat fUV = SUV[0];			// convert half->float
					D->UV = CVT(fUV);
					for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
					{
						Lod->ExtraUV[TexCoordIndex-1][Vert] = CVT(SUV[TexCoordIndex]);
					}
				}
				else
				{
					// position
					const FMeshUVFloat *SUV;
					if (!S.bUsePackedPosition)
					{
						const FGPUVert3Float &V0 = S.VertsFloat[Vert];
						V = &V0;
						D->Position = CVT(V0.Pos);
						SUV = V0.UV;
					}
					else
					{
						const FGPUVert3PackedFloat &V0 = S.VertsFloatPacked[Vert];
						V = &V0;
						FVector VPos;
						VPos = V0.Pos.ToVector(S.MeshOrigin, S.MeshExtension);
						D->Position = CVT(VPos);
						SUV = V0.UV;
					}
					// UV
					FMeshUVFloat fUV = SUV[0];
					D->UV = CVT(fUV);
					for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
					{
						Lod->ExtraUV[TexCoordIndex-1][Vert] = CVT(SUV[TexCoordIndex]);
					}
				}
				// convert Normal[3]
				UnpackNormals(V->Normal, *D);
				// convert influences
				int TotalWeight = 0;
				int i2 = 0;
				unsigned PackedWeights = 0;
				for (int i = 0; i < NUM_INFLUENCES_UE3; i++)
				{
					int BoneIndex  = V->BoneIndex[i];
					byte BoneWeight = V->BoneWeight[i];
					if (BoneWeight == 0) continue;				// skip this influence (but do not stop the loop!)
					PackedWeights |= BoneWeight << (i2 * 8);
					D->Bone[i2]   = C->Bones[BoneIndex];
					i2++;
					TotalWeight += BoneWeight;
				}
				D->PackedWeights = PackedWeights;
				if (TotalWeight != 255 && TotalWeight > 0)
				{
					NumReweightedVerts++;
					float WeightScale = 255.0f / TotalWeight;
					unsigned ScaledWeight = 0;
					for (int i = 0; i < NUM_INFLUENCES_UE3; i++)
					{
						int shift = i * 8;
						unsigned mask = 0xFF << shift;
						unsigned w = (PackedWeights & mask) >> shift;
						w = appRound((float)w * WeightScale);
						if (w == 0) continue;					// this might happen when weights are bad (Dungeon Defenders has w [1 255 255 0] for the same bone
#if 0
						if (w <= 0 || w >= 256)
							printf("w: %d t: %d s: %g b [%d %d %d %d] w [%d %d %d %d] pw: %08X\n", w, TotalWeight, WeightScale,
								V->BoneIndex[0], V->BoneIndex[1], V->BoneIndex[2], V->BoneIndex[3],
								V->BoneWeight[0], V->BoneWeight[1], V->BoneWeight[2], V->BoneWeight[3],
								PackedWeights);
#endif
						assert(w > 0 && w < 256);
						ScaledWeight |= w << shift;
					}
					D->PackedWeights = ScaledWeight;
				}
				if (i2 < NUM_INFLUENCES_UE3) D->Bone[i2] = INDEX_NONE; // mark end of list
			}
			else
			{
				// old UE3 version without a GPU skin
				// get vertex from chunk
				const FMeshUVFloat *SUV;
				if (Vert < C->FirstVertex + C->NumRigidVerts)
				{
					// rigid vertex
					const FRigidVertex3 &V0 = C->RigidVerts[Vert - C->FirstVertex];
					// position and normal
					D->Position = CVT(V0.Pos);
					UnpackNormals(V0.Normal, *D);
					// single influence
					D->PackedWeights = 0xFF;
					D->Bone[0]   = C->Bones[V0.BoneIndex];
					SUV = V0.UV;
				}
				else
				{
					// soft vertex
					const FSoftVertex3 &V0 = C->SoftVerts[Vert - C->FirstVertex - C->NumRigidVerts];
					// position and normal
					D->Position = CVT(V0.Pos);
					UnpackNormals(V0.Normal, *D);
					// influences
//					int TotalWeight = 0;
					int i2 = 0;
					unsigned PackedWeights = 0;
					for (int i = 0; i < NUM_INFLUENCES_UE3; i++)
					{
						int BoneIndex  = V0.BoneIndex[i];
						byte BoneWeight = V0.BoneWeight[i];
						if (BoneWeight == 0) continue;
						PackedWeights |= BoneWeight << (i2 * 8);
						D->Bone[i2]   = C->Bones[BoneIndex];
						i2++;
//						TotalWeight += BoneWeight;
					}
					D->PackedWeights = PackedWeights;
//					assert(TotalWeight == 255);
					if (i2 < NUM_INFLUENCES_UE3) D->Bone[i2] = INDEX_NONE; // mark end of list
					SUV = V0.UV;
				}
				// UV
				FMeshUVFloat fUV = SUV[0];			// convert half->float
				D->UV = CVT(fUV);
				for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
				{
					Lod->ExtraUV[TexCoordIndex-1][Vert] = CVT(SUV[TexCoordIndex]);
				}
			}
		}

		if (NumReweightedVerts > 0)
			appPrintf("LOD %d: udjusted weights for %d vertices\n", lod, NumReweightedVerts);

		unguard;	// ProcessVerts

		// indices
		Lod->Indices.Initialize(&SrcLod.IndexBuffer.Indices16, &SrcLod.IndexBuffer.Indices32);

		// sections
		guard(ProcessSections);
		Lod->Sections.Empty(SrcLod.Sections.Num());
		const FSkeletalMeshLODInfo &Info = LODInfo[lod];

		for (int Sec = 0; Sec < SrcLod.Sections.Num(); Sec++)
		{
			const FSkelMeshSection3 &S = SrcLod.Sections[Sec];
			CMeshSection *Dst = new (Lod->Sections) CMeshSection;

			// remap material for LOD
			int MaterialIndex = S.MaterialIndex;
			if (MaterialIndex >= 0 && MaterialIndex < Info.LODMaterialMap.Num())
				MaterialIndex = Info.LODMaterialMap[MaterialIndex];

			Dst->Material   = (MaterialIndex < Materials.Num()) ? Materials[MaterialIndex] : NULL;
			Dst->FirstIndex = S.FirstIndex;
			Dst->NumFaces   = S.NumTriangles;
		}

		unguard;	// ProcessSections

		unguardf("lod=%d", lod); // ConvertLod
	}

	// copy skeleton
	guard(ProcessSkeleton);
	Mesh->RefSkeleton.Empty(RefSkeleton.Num());
	for (int i = 0; i < RefSkeleton.Num(); i++)
	{
		const FMeshBone &B = RefSkeleton[i];
		CSkelMeshBone *Dst = new (Mesh->RefSkeleton) CSkelMeshBone;
		Dst->Name        = B.Name;
		Dst->ParentIndex = B.ParentIndex;
		Dst->Position    = CVT(B.BonePos.Position);
		Dst->Orientation = CVT(B.BonePos.Orientation);
		// fix skeleton; all bones but 0
		if (i >= 1)
			Dst->Orientation.w *= -1;
	}
	unguard; // ProcessSkeleton

	Mesh->FinalizeMesh();

#if BATMAN
	if (ArGame >= GAME_Batman2 && ArGame <= GAME_Batman4)
		FixBatman2Skeleton();
#endif

	unguard;
}


void USkeletalMesh3::PostLoad()
{
	guard(USkeletalMesh3::PostLoad);

	// MK X has bones serialized in separate USkeleton object, so perform conversion in PostLoad()
	ConvertMesh();
	assert(ConvertedMesh);

	int NumSockets = Sockets.Num();
	if (NumSockets)
	{
		ConvertedMesh->Sockets.Empty(NumSockets);
		for (int i = 0; i < NumSockets; i++)
		{
			USkeletalMeshSocket *S = Sockets[i];
			if (!S) continue;
			CSkelMeshSocket *DS = new (ConvertedMesh->Sockets) CSkelMeshSocket;
			DS->Name = S->SocketName;
			DS->Bone = S->BoneName;
			CCoords &C = DS->Transform;
			C.origin = CVT(S->RelativeLocation);
			RotatorToAxis(S->RelativeRotation, C.axis);
		}
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

// Implement constructor in cpp to avoid inlining (it's large enough).
// It's useful to declare TArray<> structures as forward declarations in header file.
UStaticMesh3::UStaticMesh3()
:	ConvertedMesh(NULL)
{}

UStaticMesh3::~UStaticMesh3()
{
	delete ConvertedMesh;
}

#if MOH2010

struct FMOHStaticMeshSectionUnk
{
	TArray<int>			f4;
	TArray<int>			f10;
	TArray<int16>		f1C;
	TArray<int16>		f28;
	TArray<int16>		f34;
	TArray<int16>		f40;
	TArray<int16>		f4C;
	TArray<int16>		f58;

	friend FArchive& operator<<(FArchive &Ar, FMOHStaticMeshSectionUnk &S)
	{
		return Ar << S.f4 << S.f10 << S.f1C << S.f28 << S.f34 << S.f40 << S.f4C << S.f58;
	}
};

#endif // MOH2010

struct FPS3StaticMeshData
{
	TArray<int>			unk1;
	TArray<int>			unk2;
	TArray<uint16>		unk3;
	TArray<uint16>		unk4;
	TArray<uint16>		unk5;
	TArray<uint16>		unk6;
	TArray<uint16>		unk7;
	TArray<uint16>		unk8;

	friend FArchive& operator<<(FArchive &Ar, FPS3StaticMeshData &S)
	{
		Ar << S.unk1 << S.unk2 << S.unk3 << S.unk4 << S.unk5 << S.unk6 << S.unk7 << S.unk8;
#if DUST514
		if (Ar.Game == GAME_Dust514) // there's no version check, perhaps that code is standard for UE3?
		{
			int16 s1, s2;
			TArray<FVector> va1, va2;
			TArray<int16> sa1;
			Ar << s1 << s2 << va1 << va2;
			if (Ar.ArLicenseeVer >= 32) Ar << sa1;
//			appPrintf("s1=%d s2=%d va1=%d va2=%d sa1=%d\n", s1, s2, va1.Num(), va2.Num(), sa1.Num());
		}
#endif // DUST514
		return Ar;
	}
};


struct FStaticMeshSection3
{
	UMaterialInterface	*Mat;
	int					f10;		//?? bUseSimple...Collision
	int					f14;		//?? ...
	int					bEnableShadowCasting;
	int					FirstIndex;
	int					NumFaces;
	int					f24;		//?? first used vertex
	int					f28;		//?? last used vertex
	int					Index;		//?? index of section
	TArrayOfArray<int, 2> f30;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshSection3 &S)
	{
		guard(FStaticMeshSection3<<);
#if TUROK
		if (Ar.Game == GAME_Turok && Ar.ArLicenseeVer >= 57)
		{
			// no S.Mat
			return Ar << S.f10 << S.f14 << S.FirstIndex << S.NumFaces << S.f24 << S.f28;
		}
#endif // TUROK
		Ar << S.Mat << S.f10 << S.f14;
#if MKVSDC
		if (Ar.Game == GAME_MK)
		{
			TArray<FGuid> unk28;				// 4x32-bit
			// no bEnableShadowCasting and Index fields
			Ar << S.FirstIndex << S.NumFaces << S.f24 << S.f28;
			if (Ar.ArVer >= 409) Ar << unk28;
			return Ar;
		}
#endif // MKVSDC
		if (Ar.ArVer >= 473) Ar << S.bEnableShadowCasting;
		Ar << S.FirstIndex << S.NumFaces << S.f24 << S.f28;
#if MASSEFF
		if (Ar.Game == GAME_MassEffect && Ar.ArVer >= 485)
			return Ar << S.Index;				//?? other name?
#endif // MASSEFF
#if HUXLEY
		if (Ar.Game == GAME_Huxley && Ar.ArVer >= 485)
			return Ar << S.Index;				//?? other name?
#endif // HUXLEY
		if (Ar.ArVer >= 492) Ar << S.Index;		// real version is unknown! This field is missing in GOW1_PC (490), but present in UT3 (512)
#if ALPHA_PR
		if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 13)
		{
			int unk30, unk34;
			Ar << unk30 << unk34;
		}
#endif // ALPHA_PR
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 49)
		{
			Ar << S.f30;
			return Ar;
		}
#endif // TRANSFORMERS
#if FABLE
		if (Ar.Game == GAME_Fable && Ar.ArLicenseeVer >= 1007)
		{
			int unk40;
			Ar << unk40;
		}
#endif // FABLE
		if (Ar.ArVer >= 514) Ar << S.f30;
#if ALPHA_PR
		if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 39)
		{
			int unk38;
			Ar << unk38;
		}
#endif // ALPHA_PR
#if MOH2010
		if (Ar.Game == GAME_MOH2010 && Ar.ArVer >= 575)
		{
			byte flag;
			FMOHStaticMeshSectionUnk unk3C;
			Ar << flag;
			if (flag) Ar << unk3C;
		}
#endif // MOH2010
#if BATMAN
		if ((Ar.Game == GAME_Batman2 || Ar.Game == GAME_Batman3) && (Ar.ArLicenseeVer >= 28)) // && (Ar.ArLicenseeVer < 102)) - this will drop Batman3 support
		{
			UObject *unk4;
			Ar << unk4;
		}
#endif // BATMAN
#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3)
		{
			FString unk4;
			Ar << unk4;
		}
#endif // BIOSHOCK3
		if (Ar.ArVer >= 618)
		{
			byte unk;
			Ar << unk;
			if (unk)
			{
				FPS3StaticMeshData ps3data;
				Ar << ps3data;
				DBG_STAT("PS3 data: %d %d %d %d %d %d %d %d\n",
					ps3data.unk1.Num(), ps3data.unk2.Num(), ps3data.unk3.Num(), ps3data.unk4.Num(),
					ps3data.unk5.Num(), ps3data.unk6.Num(), ps3data.unk7.Num(), ps3data.unk8.Num());
			}
		}
#if XCOM
		if (Ar.Game == GAME_XcomB)
		{
			FString unk;
			Ar << unk;
		}
		if (Ar.Game == GAME_Xcom2 && Ar.ArLicenseeVer >= 83)
		{
			int unk;
			Ar << unk;
		}
#endif // XCOM
		return Ar;
		unguard;
	}
};


struct FStaticMeshVertexStream3
{
	int					VertexSize;		// 0xC
	int					NumVerts;		// == Verts.Num()
	TArray<FVector>		Verts;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexStream3 &S)
	{
		guard(FStaticMeshVertexStream3<<);

		//!! Borderlands3:
		//!! (EngineVersion>>16) >= 29
		//!! int VertexSize, int NumVerts
		//!! byte UseCompressedPositions, always 1 (0==float[3], 1=int16[])
		//!! byte Use3Components, always 0 (0==int16[4], 0=int16[3])
		//!! FVector Mins, Extents

#if BATMAN
		if (Ar.Game >= GAME_Batman && Ar.Game <= GAME_Batman4)
		{
			byte VertexType = 0;		// appeared in Batman 2; 0 -> FVector, 1 -> half[3], 2 -> half[4] (not checked!)
			int unk18;					// default is 1
			if (Ar.ArLicenseeVer >= 41)
				Ar << VertexType;
			Ar << S.VertexSize << S.NumVerts;
			if (Ar.ArLicenseeVer >= 17)
				Ar << unk18;
			DBG_STAT("Batman StaticMesh VertexStream: IS:%d NV:%d VT:%d unk:%d\n", S.VertexSize, S.NumVerts, VertexType, unk18);
			switch (VertexType)
			{
			case 0:
				S.Verts.BulkSerialize(Ar);
				break;
			case 4:
				{
					TArray<FVectorHalf> PackedVerts;
					PackedVerts.BulkSerialize(Ar);
					CopyArray(S.Verts, PackedVerts);
				}
				break;
			default:
				appError("unsupported vertex type %d", VertexType);
			}
			if (Ar.ArLicenseeVer >= 24)
			{
				TArray<FMeshUVFloat> unk28;	// unknown type, TArray<dword,dword>
				Ar << unk28;
			}
			return Ar;
		}
#endif // BATMAN

		Ar << S.VertexSize << S.NumVerts;
		DBG_STAT("StaticMesh Vertex stream: IS:%d NV:%d\n", S.VertexSize, S.NumVerts);

#if AVA
		if (Ar.Game == GAME_AVA && Ar.ArVer >= 442)
		{
			int presence;
			Ar << presence;
			if (!presence)
			{
				appNotify("AVA: StaticMesh without vertex stream");
				return Ar;
			}
		}
#endif // AVA
#if MOH2010
		if (Ar.Game == GAME_MOH2010 && Ar.ArLicenseeVer >= 58)
		{
			int unk28;
			Ar << unk28;
		}
#endif // MOH2010
#if SHADOWS_DAMNED
		if (Ar.Game == GAME_ShadowsDamned && Ar.ArLicenseeVer >= 26)
		{
			int unk28;
			Ar << unk28;
		}
#endif // SHADOWS_DAMNED
#if MASSEFF
		if (Ar.Game == GAME_MassEffect3 && Ar.ArLicenseeVer >= 150)
		{
			int unk28;
			Ar << unk28;
		}
#endif // MASSEFF
#if PLA
		if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
		{
			FGuid unk;
			Ar << unk;
		}
#endif // PLA
#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3)
		{
			byte IsPacked, VectorType;		// VectorType used only when IsPacked != 0
			FVector Mins, Extents;
			Ar << IsPacked << VectorType << Mins << Extents;
			DBG_STAT("... Bioshock3: IsPacked=%d VectorType=%d Mins=%g %g %g Extents=%g %g %g\n",
				IsPacked, VectorType, FVECTOR_ARG(Mins), FVECTOR_ARG(Extents));
			if (IsPacked)
			{
				if (VectorType)
				{
					TArray<FVectorIntervalFixed48Bio> Vecs16x3;
					Vecs16x3.BulkSerialize(Ar);
					S.Verts.AddUninitialized(Vecs16x3.Num());
					for (int i = 0; i < Vecs16x3.Num(); i++)
						S.Verts[i] = Vecs16x3[i].ToVector(Mins, Extents);
					appNotify("type1 - untested");	//?? not found - not used?
				}
				else
				{
					TArray<FVectorIntervalFixed64> Vecs16x4;
					Vecs16x4.BulkSerialize(Ar);
					S.Verts.AddUninitialized(Vecs16x4.Num());
					for (int i = 0; i < Vecs16x4.Num(); i++)
						S.Verts[i] = Vecs16x4[i].ToVector(Mins, Extents);
				}
				return Ar;
			}
			// else - normal vertex stream
		}
#endif // BIOSHOCK3
#if REMEMBER_ME
		if (Ar.Game == GAME_RememberMe && Ar.ArLicenseeVer >= 18)
		{
			int UsePackedPosition, VertexType;
			FVector v1, v2;
			Ar << VertexType << UsePackedPosition;
			if (Ar.ArLicenseeVer >= 20)
				Ar << v1 << v2;
//			appPrintf("VT=%d, PP=%d, V1=%g %g %g, V2=%g %g %g\n", VertexType, UsePackedPosition, FVECTOR_ARG(v1), FVECTOR_ARG(v2));
			if (UsePackedPosition && VertexType != 0)
			{
				appError("Unsupported vertex type!\n");
			}
		}
#endif // REMEMBER_ME
#if THIEF4
		if (Ar.Game == GAME_Thief4 && Ar.ArLicenseeVer >= 15)
		{
			int unk28;
			Ar << unk28;
		}
#endif // THIEF4
#if DUST514
		if (Ar.Game == GAME_Dust514 && Ar.ArLicenseeVer >= 31)
		{
			int bUseFullPrecisionPosition;
			Ar << bUseFullPrecisionPosition;
			if (!bUseFullPrecisionPosition)
			{
				TArray<FVectorHalf> HalfVerts;
				HalfVerts.BulkSerialize(Ar);
				CopyArray(S.Verts, HalfVerts);
				return Ar;
			}
		}
#endif // DUST514
		S.Verts.BulkSerialize(Ar);
		return Ar;

		unguard;
	}
};


static int  GNumStaticUVSets    = 1;
static bool GUseStaticFloatUVs  = true;
static bool GStripStaticNormals = false;

struct FStaticMeshUVItem3
{
	FVector				Pos;			// old version (< 472)
	FPackedNormal		Normal[3];
	int					f10;			//?? VertexColor?
	FMeshUVFloat		UV[NUM_UV_SETS_UE3];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVItem3 &V)
	{
		guard(FStaticMeshUVItem3<<);

#if MKVSDC
		if (Ar.Game == GAME_MK)
		{
			if (Ar.ArVer >= 472) goto uvs;	// normals are stored in FStaticMeshNormalStream_MK
			int unk;
			Ar << unk;
			goto new_ver;
		}
#endif // MKVSDC
#if A51
		if (Ar.Game == GAME_A51)
			goto new_ver;
#endif
#if AVA
		if (Ar.Game == GAME_AVA)
		{
			assert(Ar.ArVer >= 441);
			Ar << V.Normal[0] << V.Normal[2] << V.f10;
			goto uvs;
		}
#endif // AVA

		if (GStripStaticNormals) goto uvs;

		if (Ar.ArVer < 472)
		{
			// old version has position embedded into UVStream (this is not an UVStream, this is a single stream for everything)
			int unk10;					// pad or color ?
			Ar << V.Pos << unk10;
		}
#if FURY
		if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 7)
		{
			int fC;
			Ar << fC;					// really should be serialized before unk10 above (it's FColor)
		}
#endif // FURY
	new_ver:
		if (Ar.ArVer < 477)
			Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
		else
			Ar << V.Normal[0] << V.Normal[2];
#if APB
		if (Ar.Game == GAME_APB && Ar.ArLicenseeVer >= 12) goto uvs;
#endif
#if MOH2010
		if (Ar.Game == GAME_MOH2010 && Ar.ArLicenseeVer >= 58) goto uvs;
#endif
#if UNDERTOW
		if (Ar.Game == GAME_Undertow) goto uvs;
#endif
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 181) goto uvs; // Transformers: Fall of Cybertron, no version in code
#endif
		if (Ar.ArVer >= 434 && Ar.ArVer < 615)
			Ar << V.f10;				// starting from 615 made as separate stream
	uvs:
		if (GUseStaticFloatUVs)
		{
			for (int i = 0; i < GNumStaticUVSets; i++)
				Ar << V.UV[i];
		}
		else
		{
			for (int i = 0; i < GNumStaticUVSets; i++)
			{
				// read in half format and convert to float
				FMeshUVHalf UVHalf;
				Ar << UVHalf;
				V.UV[i] = UVHalf;		// convert
			}
		}
		return Ar;

		unguard;
	}
};

struct FStaticMeshUVStream3
{
	int					NumTexCoords;
	int					ItemSize;
	int					NumVerts;
	int					bUseFullPrecisionUVs;
	TArray<FStaticMeshUVItem3> UV;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVStream3 &S)
	{
		guard(FStaticMeshUVStream3<<);

		Ar << S.NumTexCoords << S.ItemSize << S.NumVerts;
		S.bUseFullPrecisionUVs = true;
#if TUROK
		if (Ar.Game == GAME_Turok && Ar.ArLicenseeVer >= 59)
		{
			int HalfPrecision, unk28;
			Ar << HalfPrecision << unk28;
			S.bUseFullPrecisionUVs = !HalfPrecision;
			assert(S.bUseFullPrecisionUVs);
		}
#endif // TUROK
#if AVA
		if (Ar.Game == GAME_AVA && Ar.ArVer >= 441) goto new_ver;
#endif
#if MOHA
		if (Ar.Game == GAME_MOHA && Ar.ArVer >= 421) goto new_ver;
#endif
#if MKVSDC
		if (Ar.Game == GAME_MK) goto old_ver;	// Injustice has no float/half selection
#endif
		if (Ar.ArVer >= 474)
		{
		new_ver:
			Ar << S.bUseFullPrecisionUVs;
		}
	old_ver:

#if BATMAN
		int HasNormals = 1;
		if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4 && Ar.ArLicenseeVer >= 42)
		{
			Ar << HasNormals;
			if (Ar.ArLicenseeVer >= 194)
			{
				int unk;
				Ar << unk;
			}
		}
		GStripStaticNormals = (HasNormals == 0);
#endif // BATMAN
#if MASSEFF
		if (Ar.Game == GAME_MassEffect3 && Ar.ArLicenseeVer >= 150)
		{
			int unk30;
			Ar << unk30;
		}
#endif // MASSEFF
		DBG_STAT("StaticMesh UV stream: TC:%d IS:%d NV:%d FloatUV:%d\n", S.NumTexCoords, S.ItemSize, S.NumVerts, S.bUseFullPrecisionUVs);
#if MKVSDC
		if (Ar.Game == GAME_MK)
			S.bUseFullPrecisionUVs = false;
#endif
#if A51
		if (Ar.Game == GAME_A51 && Ar.ArLicenseeVer >= 22) // or MidwayVer ?
			Ar << S.bUseFullPrecisionUVs;
#endif
#if MOH2010
		if (Ar.Game == GAME_MOH2010 && Ar.ArLicenseeVer >= 58)
		{
			int unk30;
			Ar << unk30;
		}
#endif // MOH2010
#if FURY
		if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 34)
		{
			int unused;			// useless stack variable, always 0
			Ar << unused;
		}
#endif // FURY
#if SHADOWS_DAMNED
		if (Ar.Game == GAME_ShadowsDamned && Ar.ArLicenseeVer >= 22)
		{
			int unk30;
			Ar << unk30;
		}
#endif // SHADOWS_DAMNED
#if PLA
		if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
		{
			FGuid unk;
			Ar << unk;
		}
#endif // PLA
#if THIEF4
		if (Ar.Game == GAME_Thief4 && Ar.ArLicenseeVer >= 15)
		{
			int unk30;
			Ar << unk30;
		}
#endif
		// prepare for UV serialization
		if (S.NumTexCoords > MAX_MESH_UV_SETS)
			appError("StaticMesh has %d UV sets", S.NumTexCoords);
		GNumStaticUVSets   = S.NumTexCoords;
		GUseStaticFloatUVs = (S.bUseFullPrecisionUVs != 0);
		S.UV.BulkSerialize(Ar);
		return Ar;

		unguard;
	}
};

struct FStaticMeshColorStream3
{
	int					ItemSize;
	int					NumVerts;
	TArray<int>			Colors;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshColorStream3 &S)
	{
		guard(FStaticMeshColorStream3<<);
		Ar << S.ItemSize << S.NumVerts;
		DBG_STAT("StaticMesh ColorStream: IS:%d NV:%d\n", S.ItemSize, S.NumVerts);
		S.Colors.BulkSerialize(Ar);
		return Ar;
		unguard;
	}
};

// new color stream: difference is that data array is not serialized when NumVerts is 0
struct FStaticMeshColorStream3New		// ArVer >= 615
{
	int					ItemSize;
	int					NumVerts;
	TArray<int>			Colors;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshColorStream3New &S)
	{
		guard(FStaticMeshColorStream3New<<);
		Ar << S.ItemSize << S.NumVerts;
		DBG_STAT("StaticMesh ColorStreamNew: IS:%d NV:%d\n", S.ItemSize, S.NumVerts);
#if THIEF4
		if (Ar.Game == GAME_Thief4 && Ar.ArLicenseeVer >= 43)
		{
			byte unk;
			Ar << unk;
		}
#endif
		if (S.NumVerts)
		{
#if PLA
			if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
			{
				FGuid unk;
				Ar << unk;
			}
#endif // PLA
			S.Colors.BulkSerialize(Ar);
		}
		return Ar;
		unguard;
	}
};

struct FStaticMeshVertex3Old			// ArVer < 333
{
	FVector				Pos;
	FPackedNormal		Normal[3];		// packed vector

	operator FVector() const
	{
		return Pos;
	}

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertex3Old &V)
	{
		return Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
	}
};

struct FStaticMeshUVStream3Old			// ArVer < 364; corresponds to UE2 StaticMesh?
{
	TArray<FMeshUVFloat> Data;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVStream3Old &S)
	{
		guard(FStaticMeshUVStream3Old<<);
		int unk;						// Revision?
		Ar << S.Data;					// used BulkSerialize, but BulkSerialize is newer than this version
		if (Ar.ArVer < 297) Ar << unk;
		return Ar;
		unguard;
	}
};

#if MKVSDC

struct FStaticMeshNormal_MK
{
	FPackedNormal		Normal[3];		// packed vector

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshNormal_MK &V)
	{
		if (Ar.ArVer >= 573)
			return Ar << V.Normal[0] << V.Normal[2];	// Injustice
		return Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
	}
};

struct FStaticMeshNormalStream_MK
{
	int					ItemSize;
	int					NumVerts;
	TArray<FStaticMeshNormal_MK> Normals;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshNormalStream_MK &S)
	{
		Ar << S.ItemSize << S.NumVerts;
		S.Normals.BulkSerialize(Ar);
		DBG_STAT("MK NormalStream: ItemSize=%d, Count=%d (%d)\n", S.ItemSize, S.NumVerts, S.Normals.Num());
		return Ar;
	}
};

#endif // MKVSDC

struct FStaticMeshLODModel3
{
	FByteBulkData		BulkData;		// ElementSize = 0xFC for UT3 and 0x170 for UDK ... it's simpler to skip it
	TArray<FStaticMeshSection3> Sections;
	FStaticMeshVertexStream3    VertexStream;
	FStaticMeshUVStream3        UVStream;
	FStaticMeshColorStream3     ColorStream;	//??
	FStaticMeshColorStream3New  ColorStream2;	//??
	FIndexBuffer3		Indices;
	FIndexBuffer3		Indices2;		// wireframe
	int					NumVerts;
	TArray<FEdge3>		Edges;
	TArray<byte>		fEC;			// flags for faces? removed simultaneously with Edges

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshLODModel3 &Lod)
	{
		guard(FStaticMeshLODModel3<<);

		DBG_STAT("Serialize UStaticMesh LOD\n");
#if FURY
		if (Ar.Game == GAME_Fury)
		{
			int EndArPos, unkD0;
			if (Ar.ArLicenseeVer >= 8)	Ar << EndArPos;			// TLazyArray-like file pointer
			if (Ar.ArLicenseeVer >= 18)	Ar << unkD0;
		}
#endif // FURY
#if HUXLEY
		if (Ar.Game == GAME_Huxley && Ar.ArLicenseeVer >= 14)
		{
			// Huxley has different IndirectArray layout: each item
			// has stored data size before data itself
			int DataSize;
			Ar << DataSize;
		}
#endif // HUXLEY

#if APB
		if (Ar.Game == GAME_APB)
		{
			// skip bulk; check UTexture3::Serialize() for details
			Ar.Seek(Ar.Tell() + 8);
			goto after_bulk;
		}
#endif // APB
		if (Ar.ArVer >= 218)
			Lod.BulkData.Skip(Ar);

	after_bulk:

#if TLR
		if (Ar.Game == GAME_TLR && Ar.ArLicenseeVer >= 2)
		{
			FByteBulkData unk128;
			unk128.Skip(Ar);
		}
#endif // TLR
		Ar << Lod.Sections;
#if DEBUG_STATICMESH
		appPrintf("%d sections\n", Lod.Sections.Num());
		for (int i = 0; i < Lod.Sections.Num(); i++)
		{
			FStaticMeshSection3 &S = Lod.Sections[i];
			appPrintf("Mat: %s\n", S.Mat ? S.Mat->Name : "?");
			appPrintf("  %d %d sh=%d i0=%d NF=%d %d %d idx=%d\n", S.f10, S.f14, S.bEnableShadowCasting, S.FirstIndex, S.NumFaces, S.f24, S.f28, S.Index);
		}
#endif // DEBUG_STATICMESH
		// serialize vertex and uv streams
#if A51
		if (Ar.Game == GAME_A51) goto new_ver;
#endif
#if MKVSDC || AVA
		if (Ar.Game == GAME_MK || Ar.Game == GAME_AVA) goto ver_3;
#endif

#if BORDERLANDS
		if (Ar.Game == GAME_Borderlands && Ar.ArLicenseeVer >= 57 && Ar.ArVer < 832)	// Borderlands 1, version unknown; not valid for Borderlands 2
		{
			// refined field set
			Ar << Lod.VertexStream;
			Ar << Lod.UVStream;
			Ar << Lod.NumVerts;
			Ar << Lod.Indices;
			// note: no fEC (smoothing groups?)
			return Ar;
		}
#endif // BORDERLANDS

#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers)
		{
			// code is similar to original code (ArVer >= 472) but has different versioning and a few new fields
			FTRMeshUnkStream unkStream;		// normals?
			int unkD8;						// part of Indices2
			Ar << Lod.VertexStream << Lod.UVStream;
			if (Ar.ArVer >= 516) Ar << Lod.ColorStream2;
			if (Ar.ArLicenseeVer >= 71) Ar << unkStream;
			if (Ar.ArVer < 536) Ar << Lod.ColorStream;
			Ar << Lod.NumVerts << Lod.Indices << Lod.Indices2;
			if (Ar.ArLicenseeVer >= 58) Ar << unkD8;
			if (Ar.ArLicenseeVer >= 181)	// Fall of Cybertron
			{								// pre-Fall of Cybertron has 0 or 1 ints after indices, but Fall of Cybertron has 1 or 2 ints
				int unkIndexStreamField;
				Ar << unkIndexStreamField;
			}
			if (Ar.ArVer < 536)
			{
				Lod.Edges.BulkSerialize(Ar);
				Ar << Lod.fEC;
			}
			return Ar;
		}
#endif // TRANSFORMERS

		if (Ar.ArVer >= 472)
		{
		new_ver:
			Ar << Lod.VertexStream;
			Ar << Lod.UVStream;
#if MOH2010
			if (Ar.Game == GAME_MOH2010 && Ar.ArLicenseeVer >= 55) goto color_stream;
#endif
#if BLADENSOUL
			if (Ar.Game == GAME_BladeNSoul && Ar.ArVer >= 572) goto color_stream;
#endif
			// unknown data in UDK
			if (Ar.ArVer >= 615)
			{
			color_stream:
				Ar << Lod.ColorStream2;
			}
			if (Ar.ArVer < 686) Ar << Lod.ColorStream;	//?? probably this is not a color stream - the same version is used to remove "edges"
			Ar << Lod.NumVerts;
			DBG_STAT("NumVerts: %d\n", Lod.NumVerts);
		}
		else if (Ar.ArVer >= 466)
		{
		ver_3:
#if MKVSDC
			if (Ar.Game == GAME_MK && Ar.ArVer >= 472) // MK9; real version: MidwayVer >= 36
			{
				FStaticMeshNormalStream_MK NormalStream;
				Ar << Lod.VertexStream << Lod.ColorStream << NormalStream << Lod.UVStream;
				if (Ar.ArVer >= 677)
				{
					// MK X
					Ar << Lod.ColorStream2;
				}
				Ar << Lod.NumVerts;
				// copy NormalStream into UVStream
				assert(Lod.UVStream.UV.Num() == NormalStream.Normals.Num());
				for (int i = 0; i < Lod.UVStream.UV.Num(); i++)
				{
					FStaticMeshUVItem3  &UV = Lod.UVStream.UV[i];
					FStaticMeshNormal_MK &N = NormalStream.Normals[i];
					UV.Normal[0] = N.Normal[0];
					UV.Normal[1] = N.Normal[1];
					UV.Normal[2] = N.Normal[2];
				}
				goto duplicate_verts;
			}
#endif // MKVSDC
			Ar << Lod.VertexStream;
			Ar << Lod.UVStream;
			Ar << Lod.NumVerts;
#if MKVSDC || AVA
			if (Ar.Game == GAME_MK || Ar.Game == GAME_AVA)
			{
			duplicate_verts:
				// note: sometimes UVStream has 2 times more items than VertexStream
				// we should duplicate vertices
				int n1 = Lod.VertexStream.Verts.Num();
				int n2 = Lod.UVStream.UV.Num();
				if (n1 * 2 == n2)
				{
					appPrintf("Duplicating MK StaticMesh verts\n");
					Lod.VertexStream.Verts.AddUninitialized(n1);
					for (int i = 0; i < n1; i++)
						Lod.VertexStream.Verts[i+n1] = Lod.VertexStream.Verts[i];
				}
			}
#endif // MKVSDC || AVA
		}
		else if (Ar.ArVer >= 364)
		{
		// ver_2:
			Ar << Lod.UVStream;
			Ar << Lod.NumVerts;
			// create VertexStream
			int NumVerts = Lod.UVStream.UV.Num();
			Lod.VertexStream.Verts.Empty(NumVerts);
//			Lod.VertexStream.NumVerts = NumVerts;
			for (int i = 0; i < NumVerts; i++)
				Lod.VertexStream.Verts.Add(Lod.UVStream.UV[i].Pos);
		}
		else
		{
		// ver_1:
			TArray<FStaticMeshUVStream3Old> UVStream;
			if (Ar.ArVer >= 333)
			{
				appNotify("StaticMesh: untested code! (ArVer=%d)", Ar.ArVer);
				TArray<FQuat> Verts;
				TArray<int>   Normals;	// compressed
				Ar << Verts << Normals << UVStream;	// really used BulkSerialize, but it is too new for this code
				//!! convert
			}
			else
			{
				// oldest version
				TArray<FStaticMeshVertex3Old> Verts;
				Ar << Verts << UVStream;
				// convert vertex stream
				int i;
				int NumVerts     = Verts.Num();
				int NumTexCoords = UVStream.Num();
				if (NumTexCoords > MAX_MESH_UV_SETS)
				{
					appNotify("StaticMesh has %d UV sets", NumTexCoords);
					NumTexCoords = MAX_MESH_UV_SETS;
				}
				Lod.VertexStream.Verts.Empty(NumVerts);
				Lod.VertexStream.Verts.AddZeroed(NumVerts);
				Lod.UVStream.UV.Empty();
				Lod.UVStream.UV.AddDefaulted(NumVerts);
				Lod.UVStream.NumVerts     = NumVerts;
				Lod.UVStream.NumTexCoords = NumTexCoords;
				// resize UV streams
				for (i = 0; i < NumVerts; i++)
				{
					FStaticMeshVertex3Old &V = Verts[i];
					FVector              &DV = Lod.VertexStream.Verts[i];
					FStaticMeshUVItem3   &UV = Lod.UVStream.UV[i];
					DV           = V.Pos;
					UV.Normal[2] = V.Normal[2];
					for (int j = 0; j < NumTexCoords; j++)
						UV.UV[j] = UVStream[j].Data[i];
				}
			}
		}

#if DUST514
		if (Ar.Game == GAME_Dust514 && Ar.ArLicenseeVer >= 32)
		{
			TArray<byte> unk;		// compressed index buffer?
			unk.BulkSerialize(Ar);
		}
#endif // DUST514

		DBG_STAT("Serializing indices ...\n");
#if BATMAN
		if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4 && Ar.ArLicenseeVer >= 45)
		{
			int unk34;		// variable in IndexBuffer, but in 1st only
			Ar << unk34;
		}
#endif // BATMAN
#if PLA
		if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
		{
			FGuid unk;
			Ar << unk;
		}
#endif // PLA
		Ar << Lod.Indices;
#if ENDWAR
		if (Ar.Game == GAME_EndWar) goto after_indices;	// single Indices buffer since version 262
#endif
#if APB
		if (Ar.Game == GAME_APB)
		{
			// serialized FIndexBuffer3 guarded by APB bulk seeker (check UTexture3::Serialize() for details)
			Ar.Seek(Ar.Tell() + 8);
			goto after_indices;				// do not need this data
		}
#endif // APB
#if BORDERLANDS
		if (Ar.Game == GAME_Borderlands && Ar.ArVer >= 832) goto after_indices; // Borderlands 2
#endif
#if METRO_CONF
		if (Ar.Game == GAME_MetroConflict && Ar.ArLicenseeVer >= 8)
		{
			int16 unk;
			Ar << unk;
		}
#endif
		Ar << Lod.Indices2;
		DBG_STAT("Indices: %d %d\n", Lod.Indices.Indices.Num(), Lod.Indices2.Indices.Num());
	after_indices:

		if (Ar.ArVer < 686)
		{
			Lod.Edges.BulkSerialize(Ar);
			Ar << Lod.fEC;
		}
#if ALPHA_PR
		if (Ar.Game == GAME_AlphaProtocol)
		{
			assert(Ar.ArLicenseeVer > 8);	// ArLicenseeVer = [1..7] has custom code
			if (Ar.ArLicenseeVer >= 4)
			{
				TArray<int> unk128;
				unk128.BulkSerialize(Ar);
			}
		}
#endif // ALPHA_PR
#if AVA
		if (Ar.Game == GAME_AVA)
		{
			if (Ar.ArLicenseeVer >= 2)
			{
				int fFC, f100;
				Ar << fFC << f100;
			}
			if (Ar.ArLicenseeVer >= 4)
			{
				FByteBulkData f104, f134, f164, f194, f1C4, f1F4, f224, f254;
				f104.Skip(Ar);
				f134.Skip(Ar);
				f164.Skip(Ar);
				f194.Skip(Ar);
				f1C4.Skip(Ar);
				f1F4.Skip(Ar);
				f224.Skip(Ar);
				f254.Skip(Ar);
			}
		}
#endif // AVA

		if (Ar.ArVer >= 841)
		{
			FIndexBuffer3 Indices3;
#if BATMAN
			if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 45)
			{
				// the same as for indices above
				int unk;
				Ar << unk;
			}
#endif // BATMAN
#if PLA
			if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
			{
				FGuid unk;
				Ar << unk;
			}
#endif // PLA
			Ar << Indices3;
			if (Indices3.Indices.Num())
				appPrintf("LOD has extra index buffer (%d items)\n", Indices3.Indices.Num());
		}

		return Ar;

		unguard;
	}
};

struct FkDOPBounds		// bounds for compressed (quantized) kDOP node
{
	FVector				v1;
	FVector				v2;

	friend FArchive& operator<<(FArchive &Ar, FkDOPBounds &V)
	{
#if ENSLAVED
		if (Ar.Game == GAME_Enslaved)
		{
			// compressed structure
			int16 v1[3], v2[3];
			Ar << v1[0] << v1[1] << v1[2] << v2[0] << v2[1] << v2[2];
			return Ar;
		}
#endif // ENSLAVED
		return Ar << V.v1 << V.v2;
	}
};

struct FkDOPNode3
{
	FkDOPBounds			Bounds;
	int					f18;
	int16				f1C;
	int16				f1E;

	friend FArchive& operator<<(FArchive &Ar, FkDOPNode3 &V)
	{
#if ENSLAVED
		if (Ar.Game == GAME_Enslaved)
		{
			// all data compressed
			byte  fC, fD;
			int16 fE;
			Ar << V.Bounds;		// compressed
			Ar << fC << fD << fE;
			return Ar;
		}
#endif // ENSLAVED
#if DCU_ONLINE
		if (Ar.Game == GAME_DCUniverse && (Ar.ArLicenseeVer & 0xFF00) >= 0xA00)
			return Ar << V.f18 << V.f1C << V.f1E;	// no Bounds field - global for all nodes
#endif // DCU_ONLINE
		Ar << V.Bounds << V.f18;
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 7) goto new_ver;
#endif
#if MOHA
		if (Ar.Game == GAME_MOHA && Ar.ArLicenseeVer >= 8) goto new_ver;
#endif
#if MKVSDC
		if (Ar.Game == GAME_MK) goto old_ver;
#endif
		if ((Ar.ArVer < 209) || (Ar.ArVer >= 468))
		{
		new_ver:
			Ar << V.f1C << V.f1E;	// int16
		}
		else
		{
		old_ver:
			// old version
			assert(Ar.IsLoading);
			int tmp1C, tmp1E;
			Ar << tmp1C << tmp1E;
			V.f1C = tmp1C;
			V.f1E = tmp1E;
		}
		return Ar;
	}
};

struct FkDOPNode3New	// starting from version 770
{
	byte				mins[3];
	byte				maxs[3];

	friend FArchive& operator<<(FArchive &Ar, FkDOPNode3New &V)
	{
		Ar << V.mins[0] << V.mins[1] << V.mins[2] << V.maxs[0] << V.maxs[1] << V.maxs[2];
		return Ar;
	}
};

SIMPLE_TYPE(FkDOPNode3New, byte)


struct FkDOPTriangle3
{
	int16				f0, f2, f4, f6;

	friend FArchive& operator<<(FArchive &Ar, FkDOPTriangle3 &V)
	{
#if FURY
		if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 25) goto new_ver;
#endif
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 7) goto new_ver;
#endif
#if MOHA
		if (Ar.Game == GAME_MOHA && Ar.ArLicenseeVer >= 8) goto new_ver;
#endif
#if MKVSDC
		if (Ar.Game == GAME_MK) goto old_ver;
#endif
		if ((Ar.ArVer < 209) || (Ar.ArVer >= 468))
		{
		new_ver:
			Ar << V.f0 << V.f2 << V.f4 << V.f6;
		}
		else
		{
		old_ver:
			assert(Ar.IsLoading);
			int tmp0, tmp2, tmp4, tmp6;
			Ar << tmp0 << tmp2 << tmp4 << tmp6;
			V.f0 = tmp0;
			V.f2 = tmp2;
			V.f4 = tmp4;
			V.f6 = tmp6;
		}
#if METRO_CONF
		if (Ar.Game == GAME_MetroConflict && Ar.ArLicenseeVer >= 2)
		{
			int16 unk;
			Ar << unk;
		}
#endif // METRO_CONF
		return Ar;
	}
};


#if FURY

struct FFuryStaticMeshUnk	// in other ganes this structure serialized after LOD models, in Fury - before
{
	int					unk0;
	int					fC, f10, f14;
	TArray<int16>		f18;				// old version uses TArray<int>, new - TArray<int16>, but there is no code selection
											// (array size in old version is always 0?)

	friend FArchive& operator<<(FArchive &Ar, FFuryStaticMeshUnk &S)
	{
		if (Ar.ArVer < 297) Ar << S.unk0;	// Version? (like in FIndexBuffer3)
		if (Ar.ArLicenseeVer >= 4)			// Fury-specific
			Ar << S.fC << S.f10 << S.f14 << S.f18;
		return Ar;
	}
};

#endif // FURY


struct FStaticMeshUnk5
{
	int					f0;
	byte				f4[3];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUnk5 &S)
	{
		return Ar << S.f0 << S.f4[0] << S.f4[1] << S.f4[2];
	}
};


void UStaticMesh3::Serialize(FArchive &Ar)
{
	guard(UStaticMesh3::Serialize);

	Super::Serialize(Ar);

#if FURY
	if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 14)
	{
		int unk3C, unk40;
		Ar << unk3C << unk40;
	}
#endif // FURY
#if DARKVOID
	if (Ar.Game == GAME_DarkVoid)
	{
		int unk180, unk18C, unk198;
		if (Ar.ArLicenseeVer >= 5) Ar << unk180 << unk18C;
		if (Ar.ArLicenseeVer >= 6) Ar << unk198;
	}
#endif // DARKVOID
#if TERA
	if (Ar.Game == GAME_Tera && Ar.ArLicenseeVer >= 3)
	{
		FString SourceFileName;
		Ar << SourceFileName;
	}
#endif // TERA
#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 50)
	{
		kDOPNodes.BulkSerialize(Ar);
		kDOPTriangles.BulkSerialize(Ar);
		Ar << Lods;
		// note: Bounds is serialized as property (see UStaticMesh in h-file)
		goto done;
	}
#endif // TRANSFORMERS
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		// MK X
		TArrayOfArray<float, 9> kDOPNodes_MK;
		Ar << kDOPNodes_MK;
		goto kdop_tris;
	}
#endif // MKVSDC

	Ar << Bounds << BodySetup;
#if TUROK
	if (Ar.Game == GAME_Turok && Ar.ArLicenseeVer >= 59)
	{
		int unkFC, unk100;
		Ar << unkFC << unk100;
	}
#endif // TUROK
	if (Ar.ArVer < 315)
	{
		UObject *unk;
		Ar << unk;
	}
#if ENDWAR
	if (Ar.Game == GAME_EndWar) goto version;	// no kDOP since version 306
#endif // ENDWAR
#if SINGULARITY
	if (Ar.Game == GAME_Singularity)
	{
		// serialize kDOP tree
		assert(Ar.ArLicenseeVer >= 112);
		// old serialization code
		kDOPNodes.BulkSerialize(Ar);
		kDOPTriangles.BulkSerialize(Ar);
		// new serialization code
		// bug in Singularity serialization code: serialized the same things twice!
		goto new_kdop;
	}
#endif // SINGULARITY
#if BULLETSTORM
	if (Ar.Game == GAME_Bulletstorm && Ar.ArVer >= 739) goto new_kdop;
#endif
#if MASSEFF
	if (Ar.Game == GAME_MassEffect3 && Ar.ArLicenseeVer >= 153) goto new_kdop;
#endif
#if DISHONORED
	if (Ar.Game == GAME_Dishonored) goto old_kdop;
#endif
#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3)
	{
		FVector v1, v2[2], v3;
		TArray<int> arr4;
		Ar << v1 << v2[0] << v2[1] << v3 << arr4;
		goto version;
	}
#endif
#if THIEF4
	if (Ar.Game == GAME_Thief4 && Ar.ArVer >= 707) goto new_kdop;
#endif
	// kDOP tree
	if (Ar.ArVer < 770)
	{
	old_kdop:
		kDOPNodes.BulkSerialize(Ar);
	}
	else
	{
	new_kdop:
		FkDOPBounds Bounds;
		TArray<FkDOPNode3New> Nodes;
		Ar << Bounds;
		Nodes.BulkSerialize(Ar);
	}
#if FURY
	if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 32)
	{
		int kDopUnk;
		Ar << kDopUnk;
	}
#endif // FURY
kdop_tris:
	kDOPTriangles.BulkSerialize(Ar);
#if DCU_ONLINE
	if (Ar.Game == GAME_DCUniverse && (Ar.ArLicenseeVer & 0xFF00) >= 0xA00)
	{
		// this game stored kDOP bounds only once
		FkDOPBounds Bounds;
		Ar << Bounds;
	}
#endif // DCU_ONLINE
#if DOH
	if (Ar.Game == GAME_DOH && Ar.ArLicenseeVer >= 73)
	{
		FVector			unk18;		// extra computed kDOP field
		TArray<FVector>	unkA0;
		int				unk74;
		Ar << unk18;
		Ar << InternalVersion;		// has InternalVersion = 0x2000F
		Ar << unkA0 << unk74 << Lods;
		goto done;
	}
#endif // DOH
#if THIEF4
	if (Ar.Game == GAME_Thief4)
	{
		if (Ar.ArLicenseeVer >= 7)
			SkipFixedArray(Ar, sizeof(int) * 7);
		if (Ar.ArLicenseeVer >= 5)
			SkipFixedArray(Ar, sizeof(int) * (2+9));	// FName + int[9]
		if (Ar.ArLicenseeVer >= 3)
			SkipFixedArray(Ar, sizeof(int) * 7);
		if (Ar.ArLicenseeVer >= 2)
		{
			SkipFixedArray(Ar, sizeof(int) * 7);
			SkipFixedArray(Ar, sizeof(int) * 10);
			// complex structure
			SkipFixedArray(Ar, sizeof(int) * 3);
			SkipFixedArray(Ar, sizeof(int));
			SkipFixedArray(Ar, sizeof(int));
		}
	}
#endif // THIEF4

version:
	Ar << InternalVersion;

	DBG_STAT("kDOPNodes=%d kDOPTriangles=%d\n", kDOPNodes.Num(), kDOPTriangles.Num());
	DBG_STAT("ver: %d\n", InternalVersion);

#if FURY
	if (Ar.Game == GAME_Fury)
	{
		int unk1, unk2;
		TArray<FFuryStaticMeshUnk> unk50;
		if (Ar.ArLicenseeVer >= 34) Ar << unk1;
		if (Ar.ArLicenseeVer >= 33) Ar << unk2;
		if (Ar.ArLicenseeVer >= 8)  Ar << unk50;
		InternalVersion = 16;		// uses InternalVersion=18
	}
#endif // FURY
#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers) goto lods;	// The Bourne Conspiracy has InternalVersion=17
#endif

	if (InternalVersion >= 17 && Ar.ArVer < 593)
	{
		TArray<FName> unk;			// some text properties; ContentTags ? (switched from binary to properties)
		Ar << unk;
	}
	if (Ar.ArVer >= 823)
	{
		guard(SerializeExtraLOD);

		int unkFlag;
		FStaticMeshLODModel3 unkLod;
		Ar << unkFlag;
		if (unkFlag)
		{
			appPrintf("has extra LOD model\n");
			Ar << unkLod;
		}

		if (Ar.ArVer < 829)
		{
			TArray<int> unk;
			Ar << unk;
		}
		else
		{
			TArray<FStaticMeshUnk5> f178;
			Ar << f178;
		}
		int f74;
		Ar << f74;

		unguard;
	}
#if SHADOWS_DAMNED
	if (Ar.Game == GAME_ShadowsDamned && Ar.ArLicenseeVer >= 26)
	{
		int unk134;
		Ar << unk134;
	}
#endif // SHADOWS_DAMNED

	if (Ar.ArVer >= 859)
	{
		int unk;
		Ar << unk;
	}

lods:
	Ar << Lods;

//	Ar << f48;

done:
	DROP_REMAINING_DATA(Ar);

	ConvertMesh();

	unguard;
}

// convert UStaticMesh3 to CStaticMesh
void UStaticMesh3::ConvertMesh()
{
	guard(UStaticMesh3::ConvertMesh);

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;

	int ArVer  = GetArVer();
	int ArGame = GetGame();

	// convert bounds
	Mesh->BoundingSphere.R = Bounds.SphereRadius / 2;			//?? UE3 meshes has radius 2 times larger than mesh itself
	VectorSubtract(CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Min));
	VectorAdd     (CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Max));

	// convert lods
	Mesh->Lods.Empty(Lods.Num());
	for (int lod = 0; lod < Lods.Num(); lod++)
	{
		guard(ConvertLod);

		const FStaticMeshLODModel3 &SrcLod = Lods[lod];
		if (SrcLod.Sections.Num() == 0)
		{
			// skip empty LODs
			appPrintf("StaticMesh %s.%s lod #%d has no sections\n", GetPackageName(), Name, lod);
			continue;
		}

		CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;

		int NumTexCoords = SrcLod.UVStream.NumTexCoords;
		int NumVerts     = SrcLod.VertexStream.Verts.Num();

		Lod->NumTexCoords = NumTexCoords;
		Lod->HasNormals   = true;
		Lod->HasTangents  = (ArVer >= 364);			//?? check; FStaticMeshUVStream3 is used since this version
#if BATMAN
		if ((ArGame == GAME_Batman2 || ArGame == GAME_Batman3) && CanStripNormalsAndTangents)
			Lod->HasNormals = Lod->HasTangents = false;
#endif
		if (NumTexCoords > MAX_MESH_UV_SETS)
			appError("StaticMesh has %d UV sets", NumTexCoords);

		// sections
		Lod->Sections.AddDefaulted(SrcLod.Sections.Num());
		for (int i = 0; i < SrcLod.Sections.Num(); i++)
		{
			CMeshSection &Dst = Lod->Sections[i];
			const FStaticMeshSection3 &Src = SrcLod.Sections[i];
			Dst.Material   = Src.Mat;
			Dst.FirstIndex = Src.FirstIndex;
			Dst.NumFaces   = Src.NumFaces;
		}

		// vertices
		Lod->AllocateVerts(NumVerts);
		for (int i = 0; i < NumVerts; i++)
		{
			const FStaticMeshUVItem3 &SUV = SrcLod.UVStream.UV[i];
			CStaticMeshVertex &V = Lod->Verts[i];

			V.Position = CVT(SrcLod.VertexStream.Verts[i]);
			UnpackNormals(SUV.Normal, V);
			// copy UV
			const FMeshUVFloat* fUV = &SUV.UV[0];
			V.UV = *CVT(fUV);
			for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
			{
				fUV++;
				Lod->ExtraUV[TexCoordIndex-1][i] = *CVT(fUV);
			}
			//!! also has ColorStream
		}

		// indices
		Lod->Indices.Initialize(&SrcLod.Indices.Indices);			// 16-bit only
		if (Lod->Indices.Num() == 0) appNotify("This StaticMesh doesn't have an index buffer");

		unguardf("lod=%d", lod);
	}

	Mesh->FinalizeMesh();

	unguard;
}


#endif // UNREAL3
