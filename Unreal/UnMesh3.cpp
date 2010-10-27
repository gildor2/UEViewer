#include "Core.h"
#include "UnrealClasses.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"				// for checking game type
#include "UnMathTools.h"			// for FRotator to FCoords

#if UNREAL3

/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

struct FSkelMeshSection3
{
	short				MaterialIndex;
	short				unk1;
	int					FirstIndex;
	word				NumTriangles;
	byte				unk2;

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection3 &S)
	{
		if (Ar.ArVer < 215)
		{
			// UE2 fields
			short FirstIndex;
			short unk1, unk2, unk3, unk4, unk5, unk6, unk7;
			TArray<short> unk8;
			Ar << S.MaterialIndex << FirstIndex << unk1 << unk2 << unk3 << unk4 << unk5 << unk6 << S.NumTriangles;
			Ar << unk8;
			S.FirstIndex = FirstIndex;
			S.unk1 = 0;
			return Ar;
		}
		Ar << S.MaterialIndex << S.unk1 << S.FirstIndex << S.NumTriangles;
#if MCARTA
		if (Ar.Game == GAME_MagnaCarta && Ar.ArLicenseeVer >= 20)
		{
			int fC;
			Ar << fC;
		}
#endif // MCARTA
		if (Ar.ArVer >= 599) Ar << S.unk2;
		return Ar;
	}
};

struct FIndexBuffer3
{
	TArray<word>		Indices;

	friend FArchive& operator<<(FArchive &Ar, FIndexBuffer3 &I)
	{
		int unk;						// Revision?
		Ar << RAW_ARRAY(I.Indices);
		if (Ar.ArVer < 297) Ar << unk;	// at older version compatible with FRawIndexBuffer
		return Ar;
	}
};

static bool CompareCompNormals(const FPackedNormal &N1, const FPackedNormal &N2)
{
	int Normal1 = N1.Data;
	int Normal2 = N2.Data;
	for (int i = 0; i < 3; i++)
	{
		char b1 = Normal1 & 0xFF;
		char b2 = Normal2 & 0xFF;
		if (abs(b1 - b2) > 10) return false;
		Normal1 >>= 8;
		Normal2 >>= 8;
	}
	return true;
}

struct FRigidVertex3
{
	FVector				Pos;
	FPackedNormal		Normal[3];
	float				U, V;
	byte				BoneIndex;
	int					Color;

	friend FArchive& operator<<(FArchive &Ar, FRigidVertex3 &V)
	{
		int NumUVSets;

#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft)
		{
			// uses FVector4 for position and 4 UV sets
			int pad;
			Ar << V.Pos;
			if (Ar.ArLicenseeVer >= 1) Ar.Seek(Ar.Tell() + sizeof(float));		// FVector4 ?
			Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
			Ar << V.U << V.V;
			if (Ar.ArLicenseeVer >= 2) Ar.Seek(Ar.Tell() + sizeof(float) * 6);	// 4 UV sets
			Ar << V.BoneIndex;
			return Ar;
		}
#endif // CRIMECRAFT
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
#if R6VEGAS
		if (Ar.Game == GAME_R6Vegas2 && Ar.ArLicenseeVer >= 63)
		{
			short hU, hV;
			Ar << hU << hV;
			V.U = half2float(hU);
			V.V = half2float(hV);
			goto influences;
		}
#endif // R6VEGAS

		// UVs
		NumUVSets = 1;
		if (Ar.ArVer >= 709) NumUVSets = 4;
#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 13) NumUVSets = 3;
#endif
#if MKVSDC || STRANGLE || FRONTLINES || TRANSFORMERS
		if ((Ar.Game == GAME_MK && Ar.ArLicenseeVer >= 11) || Ar.Game == GAME_Strangle ||	// Stranglehold check MidwayVer >= 17
			(Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 3) ||
			(Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55))
			NumUVSets = 2;
#endif
		Ar << V.U << V.V;
		if (NumUVSets > 1) Ar.Seek(Ar.Tell() + sizeof(float) * 2 * (NumUVSets - 1));

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
		return Ar;
	}
};

struct FSmoothVertex3
{
	FVector				Pos;
	FPackedNormal		Normal[3];
	float				U, V;
	byte				BoneIndex[4];
	byte				BoneWeight[4];
	int					Color;

	friend FArchive& operator<<(FArchive &Ar, FSmoothVertex3 &V)
	{
		int i, NumUVSets;

#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft)
		{
			// uses FVector4 for position and 4 UV sets
			int pad;
			Ar << V.Pos;
			if (Ar.ArLicenseeVer >= 1) Ar.Seek(Ar.Tell() + sizeof(float));		// FVector4 ?
			Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
			Ar << V.U << V.V;
			if (Ar.ArLicenseeVer >= 2) Ar.Seek(Ar.Tell() + sizeof(float) * 6);	// 4 UV sets
			goto influences;
		}
#endif // CRIMECRAFT
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
#if R6VEGAS
		if (Ar.Game == GAME_R6Vegas2 && Ar.ArLicenseeVer >= 63)
		{
			short hU, hV;
			Ar << hU << hV;
			V.U = half2float(hU);
			V.V = half2float(hV);
			goto influences;
		}
#endif // R6VEGAS

		// UVs
		NumUVSets = 1;
		if (Ar.ArVer >= 709) NumUVSets = 4;
#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 13) NumUVSets = 3;
#endif
#if MKVSDC || STRANGLE || FRONTLINES || TRANSFORMERS
		if ((Ar.Game == GAME_MK && Ar.ArLicenseeVer >= 11) || Ar.Game == GAME_Strangle ||	// Stranglehold check MidwayVer >= 17
			(Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 3) ||
			(Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55))
			NumUVSets = 2;
#endif
		Ar << V.U << V.V;
		if (NumUVSets > 1) Ar.Seek(Ar.Tell() + sizeof(float) * 2 * (NumUVSets - 1));

		if (Ar.ArVer >= 710) Ar << V.Color;	// default 0xFFFFFFFF

	influences:
		if (Ar.ArVer >= 333)
		{
			for (i = 0; i < 4; i++) Ar << V.BoneIndex[i];
			for (i = 0; i < 4; i++) Ar << V.BoneWeight[i];
		}
		else
		{
			for (i = 0; i < 4; i++)
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

struct FSkinChunk3
{
	int					FirstVertex;
	TArray<FRigidVertex3>  RigidVerts;
	TArray<FSmoothVertex3> SmoothVerts;
	TArray<short>		Bones;
	int					NumRigidVerts;
	int					NumSmoothVerts;
	int					MaxInfluences;

	friend FArchive& operator<<(FArchive &Ar, FSkinChunk3 &V)
	{
		guard(FSkinChunk3<<);
		Ar << V.FirstVertex << V.RigidVerts << V.SmoothVerts << V.Bones;
		if (Ar.ArVer >= 333)
		{
			Ar << V.NumRigidVerts << V.NumSmoothVerts;
			// note: NumRigidVerts and NumSmoothVerts may be non-zero while corresponding
			// arrays are empty - that's when GPU skin only left
		}
		else
		{
			V.NumRigidVerts  = V.RigidVerts.Num();
			V.NumSmoothVerts = V.SmoothVerts.Num();
		}
#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArLicenseeVer >= 7)
		{
			TArray<FMeshUV> extraUV;
			Ar << RAW_ARRAY(extraUV);
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
			short iVertex[2], iFace[2];
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
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Common &V)
	{
#if AVA
		if (Ar.Game == GAME_AVA) goto new_ver;
#endif
		if (Ar.ArVer < 494)
			Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
		else
		{
		new_ver:
			Ar << V.Normal[0] << V.Normal[2];
		}
#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft && Ar.ArVer >= 1) Ar.Seek(Ar.Tell() + sizeof(float)); // pad ?
#endif
		int i;
		for (i = 0; i < 4; i++) Ar << V.BoneIndex[i];
		for (i = 0; i < 4; i++) Ar << V.BoneWeight[i];
		return Ar;
	}
};

static int GNumGPUUVSets = 1;

/*
 * Half = Float16
 * http://www.openexr.com/  source: ilmbase-*.tar.gz/Half/toFloat.cpp
 * http://en.wikipedia.org/wiki/Half_precision
 * Also look GL_ARB_half_float_pixel
 */
struct FGPUVert3Half : FGPUVert3Common
{
	FVector				Pos;
	word				U, V;			//?? create class float16 ?

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Half &V)
	{
		if (Ar.ArVer < 592)
			Ar << V.Pos << *((FGPUVert3Common*)&V);
		else
			Ar << *((FGPUVert3Common*)&V) << V.Pos;
		Ar << V.U << V.V;
		if (GNumGPUUVSets > 1) Ar.Seek(Ar.Tell() + (GNumGPUUVSets - 1) * 2 * sizeof(word));	// for some game titles
		return Ar;
	}
};

struct FGPUVert3Float : FGPUVert3Common
{
	FVector				Pos;
	float				U, V;

	FGPUVert3Float& operator=(const FSmoothVertex3 &S)
	{
		Pos = S.Pos;
		U   = S.U;
		V   = S.V;
		for (int i = 0; i < 4; i++)
		{
			BoneIndex[i]  = S.BoneIndex[i];
			BoneWeight[i] = S.BoneWeight[i];
		}
		return *this;
	}

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Float &V)
	{
		if (Ar.ArVer < 592)
			Ar << V.Pos << *((FGPUVert3Common*)&V);
		else
			Ar << *((FGPUVert3Common*)&V) << V.Pos;
		Ar << V.U << V.V;
		if (GNumGPUUVSets > 1) Ar.Seek(Ar.Tell() + (GNumGPUUVSets - 1) * 2 * sizeof(float));	// for some game titles
		return Ar;
	}
};

//?? move to UnMeshTypes.h ?
//?? checked with Enslaved and MOH2010
struct FVectorIntervalFixed32
{
	int X:11; int Y:11; int Z:10;

	FVector ToVector(const FVector &Mins, const FVector &Ranges) const
	{
		FVector r;
		r.X = (X / 1023.0f) * Ranges.X + Mins.X;
		r.Y = (Y / 1023.0f) * Ranges.Y + Mins.Y;
		r.Z = (Z / 511.0f)  * Ranges.Z + Mins.Z;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FVectorIntervalFixed32 &V)
	{
		return Ar << GET_DWORD(V);
	}
};

struct FGPUVert3PackedFloat : FGPUVert3Common
{
	FVectorIntervalFixed32 Pos;
	float				U, V;

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3PackedFloat &V)
	{
		Ar << *((FGPUVert3Common*)&V) << V.Pos << V.U << V.V;
		return Ar;
	}
};

struct FGPUVert3PackedHalf : FGPUVert3Common
{
	FVectorIntervalFixed32 Pos;
	word				U, V;			//?? create class float16 ?

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3PackedHalf &V)
	{
		Ar << *((FGPUVert3Common*)&V) << V.Pos << V.U << V.V;
		return Ar;
	}
};

struct FGPUSkin3
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

	friend FArchive& operator<<(FArchive &Ar, FGPUSkin3 &S)
	{
		guard(FGPUSkin3<<);

		if (Ar.IsLoading) S.bUsePackedPosition = false;
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
			S.bUseFullPrecisionUVs = true;
			int VertexSize, NumVerts;
			Ar << S.NumUVSets << VertexSize << NumVerts;
			GNumGPUUVSets = S.NumUVSets;
			Ar << RAW_ARRAY(S.VertsFloat);	// serialized as ordinary array anyway
			return Ar;
		}
	#endif // FRONTLINES

	#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArLicenseeVer >= 74)
		{
			int UseNewFormat;
			Ar << UseNewFormat;
			if (UseNewFormat)
			{
				appError("ArmyOfTwo: new vertex format!");	//!!!
				return Ar;
			}
		}
	#endif // ARMYOF2
		if (Ar.ArVer < 493)
		{
		old_version:
			// old version - FSmoothVertex3 array
			TArray<FSmoothVertex3> Verts;
			Ar << RAW_ARRAY(Verts);
			// convert verts
			CopyArray(S.VertsFloat, Verts);
			S.bUseFullPrecisionUVs = true;
			return Ar;
		}

		// new version
	new_version:
		// serialize type information
	#if MEDGE
		if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 15)
		{
			Ar << S.NumUVSets;
			GNumGPUUVSets = 1;			// serialized in a different way - as separate stream
		}
	#endif // MEDGE
	#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55) goto get_UV_count; // 1 or 2
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

		bool AllowPackedPosition = false;
	#if ENSLAVED
		if (Ar.Game == GAME_Enslaved)	AllowPackedPosition = true;
	#endif
	#if MOH2010
		if (Ar.Game == GAME_MOH2010)	AllowPackedPosition = true;
	#endif
		//?? UE3 ignored this - forced !bUsePackedPosition in FGPUSkin3 serializer ?
		//?? Note: in UDK (newer engine) there is no code to serialize GPU vertex with packed position
		//?? working bUsePackedPosition was found in Enslaved and MOH2010 only
//		printf("data: %d %d (%g %g %g)+(%g %g %g)\n", S.bUseFullPrecisionUVs, S.bUsePackedPosition, FVECTOR_ARG(S.MeshOrigin), FVECTOR_ARG(S.MeshExtension));
		if (!AllowPackedPosition) S.bUsePackedPosition = false;		// not used in games (see comment above)

	#if CRIMECRAFT
		if (Ar.Game == GAME_CrimeCraft && Ar.ArLicenseeVer >= 2) S.NumUVSets = GNumGPUUVSets = 4;
	#endif

		// serialize vertex array
		if (!S.bUseFullPrecisionUVs)
		{
			if (!S.bUsePackedPosition)
				Ar << RAW_ARRAY(S.VertsHalf);
			else
				Ar << RAW_ARRAY(S.VertsHalfPacked);
		}
		else
		{
			if (!S.bUsePackedPosition)
				Ar << RAW_ARRAY(S.VertsFloat);
			else
				Ar << RAW_ARRAY(S.VertsFloatPacked);
		}

		return Ar;
		unguard;
	}
};

struct FMesh3Unk1
{
	int					f0;
	int					f4;

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk1 &S)
	{
		return Ar << S.f0 << S.f4;
	}
};

SIMPLE_TYPE(FMesh3Unk1, int)

struct FMesh3Unk3
{
	int					f0;
	int					f4;
	TArray<word>		f8;

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk3 &S)
	{
		Ar << S.f0 << S.f4 << S.f8;
		return Ar;
	}
};

struct FMesh3Unk2
{
	TArray<FMesh3Unk1>	f0;
	TArray<FMesh3Unk3>	fC;				//?? SparseArray or Map

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk2 &S)
	{
		Ar << S.f0;
		if (Ar.ArVer >= 609) Ar << S.fC;
		return Ar;
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
	TArray<int>			Data;

	friend FArchive& operator<<(FArchive &Ar, FTRMeshUnkStream &S)
	{
		Ar << S.ItemSize << S.NumVerts;
		if (S.ItemSize && S.NumVerts)
			Ar << RAW_ARRAY(S.Data);
		return Ar;
	}
};

#endif // TRANSFORMERS

// Version references: 180..240 - Rainbow 6: Vegas 2
// Other: GOW PC
struct FStaticLODModel3
{
	TArray<FSkelMeshSection3> Sections;
	TArray<FSkinChunk3>	Chunks;
	FIndexBuffer3		IndexBuffer;
	TArray<short>		UsedBones;		// bones, value = [0, NumBones-1]
	TArray<byte>		f24;			// count = NumBones, value = [0, NumBones-1]; note: BoneIndex is 'short', not 'byte' ...
	TArray<word>		f68;			// indices, value = [0, NumVertices-1]
	TArray<byte>		f74;			// count = NumTriangles
	int					f80;
	int					NumVertices;
	TArray<FEdge3>		Edges;			// links 2 vertices and 2 faces (triangles)
	FWordBulkData		BulkData;		// ElementCount = NumVertices
	FGPUSkin3			GPUSkin;
	TArray<FMesh3Unk2>	fC4;			// unknown, has in GoW2
	int					f6C;			// unknown, default 1

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModel3 &Lod)
	{
		guard(FStaticLODModel3<<);

		Ar << Lod.Sections << Lod.IndexBuffer;

		if (Ar.ArVer < 215)
		{
			TArray<FRigidVertex3>  RigidVerts;
			TArray<FSmoothVertex3> SmoothVerts;
			Ar << RigidVerts << SmoothVerts;
			appNotify("SkeletalMesh: untested code! (ArVer=%d)", Ar.ArVer);
		}

#if BORDERLANDS
		if (Ar.Game == GAME_Borderlands)
		{
			// refined field set
			Ar << Lod.UsedBones << Lod.Chunks << Lod.f80 << Lod.NumVertices;
			goto part2;
		}
#endif // BORDERLANDS

		if (Ar.ArVer < 686) Ar << Lod.f68;
		Ar << Lod.UsedBones;
		if (Ar.ArVer < 686) Ar << Lod.f74;
		if (Ar.ArVer >= 215)
		{
			Ar << Lod.Chunks << Lod.f80 << Lod.NumVertices;
		}

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
			// old version
			TLazyArray<FVertInfluences> Influences;
			TLazyArray<FMeshWedge>      Wedges;
			TLazyArray<FMeshFace>       Faces;
			TLazyArray<FVector>         Points;
			Ar << Influences << Wedges << Faces << Points;
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

	part2:
		if (Ar.ArVer >= 207)
		{
			Ar << Lod.f24;
		}
		else
		{
			TArray<short> f24_a;
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
		if (Ar.ArVer >= 221)
			Lod.BulkData.Serialize(Ar);
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
			TArray<FMeshUV> extraUV;
			Ar << unk84 << RAW_ARRAY(extraUV);
		}
#endif // ARMYOF2
		if (Ar.ArVer >= 709)
			Ar << Lod.f6C;
#if MOH2010
		int RealArVer = Ar.ArVer;
		if (Ar.Game == GAME_MOH2010)
		{
			Ar.ArVer = 592;			// partially upgraded engine, change version (for easier coding)
			if (Ar.ArLicenseeVer >= 42)
				Ar << Lod.fC4;		// original code: this field is serialized after GPU Skin
		}
#endif
		if (Ar.ArVer >= 333)
			Ar << Lod.GPUSkin;
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
		}
#endif // TRANSFORMERS
		if (Ar.ArVer >= 710)
		{
			//!! if Mesh.bHasVertexColors -> serialize extra stream
			//!! serialize: RAW_ARRAY<int>
			//!! problem: cannot access USkeletalMesh property from nested structure
			//?? store global "UObject* GSerializedObject"?
		}
		if (Ar.ArVer >= 534)		// post-UT3 code
			Ar << Lod.fC4;
//		assert(Lod.IndexBuffer.Indices.Num() == Lod.f68.Num()); -- mostly equals (failed in CH_TwinSouls_Cine.upk)
//		assert(Lod.BulkData.ElementCount == Lod.NumVertices); -- mostly equals (failed on some GoW packages)
		return Ar;

		unguard;
	}
};

#if A51
struct FMaterialBone
{
	int					Bone;
	FName				Param;

	friend FArchive& operator<<(FArchive &Ar, FMaterialBone &V)
	{
		return Ar << V.Bone << V.Param;
	}
};
#endif // A51

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

void USkeletalMesh::SerializeSkelMesh3(FArchive &Ar)
{
	guard(USkeletalMesh::SerializeSkelMesh3);

	UObject::Serialize(Ar);			// no UPrimitive ...

	FBoxSphereBounds	Bounds;
	TArray<UMaterial*>	Materials1;	// MaterialInterface*
	TArray<FStaticLODModel3> Lods;

#if MEDGE
	if (Ar.Game == GAME_MirrorEdge && Ar.ArLicenseeVer >= 15)
	{
		int unk264;
		Ar << unk264;
	}
#endif // MEDGE
	Ar << Bounds;
	if (Ar.ArVer < 180)
	{
		UObject *unk;
		Ar << unk;
	}
#if BATMAN
	if (Ar.Game == GAME_Batman && Ar.ArLicenseeVer >= 0x0F)
	{
		float ConservativeBounds;
		TArray<FBoneBounds> PerBoneBounds;
		Ar << ConservativeBounds << PerBoneBounds;
	}
#endif // BATMAN
	Ar << Materials1;
#if BLOODONSAND
	if (Ar.Game == GAME_50Cent && Ar.ArLicenseeVer >= 65)
	{
		TArray<UObject*> OnFireMaterials; // name is not checked
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
	Ar << MeshOrigin << RotOrigin;
	Ar << RefSkeleton << SkeletalDepth;
#if A51 || MKVSDC || STRANGLE
	//?? check GAME_Wheelman
	if (Ar.Engine() == GAME_MIDWAY3 && Ar.ArLicenseeVer >= 0xF)
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
	Ar << Lods;
#if 0
	//!! also: NameIndexMap (ArVer >= 296), PerPolyKDOPs (ArVer >= 435)
#else
	Ar.Seek(Ar.GetStopper());
#endif

	guard(ConvertMesh);
	int i;

	// convert LODs
	assert(Lods.Num() == LODInfo.Num());
	LODModels.Empty(Lods.Num());
	LODModels.Add(Lods.Num());
	for (i = 0; i < Lods.Num(); i++)
		LODModels[i].RestoreMesh3(*this, Lods[i], LODInfo[i]);

	/*
	 *	Optimized search for PointIndex in RestoreMesh3()
	 *	Profile results (GoW2 package): total time = 7.8s, partial:
	 *	  load = 1.5s, GPU lods -> CPU = 5.5s, restore mesh = 0.7s
	 *	Loading w/o seraching for good PointIndex = 1.6s, with search = 7.7s (not depends on restore mesh!)
	 *	With disabled CompareCompNormals() = 6.3s
	 *	New version (find PointIndex using TArray::FindItem()) = 4.4s
	 */
	// convert LOD 0 to mesh
	RecreateMeshFromLOD();

	MeshOrigin.Scale(-1);

	// fix skeleton; all bones but 0
	for (i = 1; i < RefSkeleton.Num(); i++)
		RefSkeleton[i].BonePos.Orientation.W *= -1;

	// materials
	Textures.Add(Materials1.Num());
	Materials.Add(Materials1.Num());
	for (i = 0; i < Materials.Num(); i++)
	{
		Textures[i] = Materials1[i];
		Materials[i].TextureIndex = i;
	}

	// setup missing properties
	MeshScale.Set(1, 1, 1);
	BoundingSphere.R = Bounds.SphereRadius / 2;		//?? UE3 meshes has radius 2 times larger than mesh
	VectorSubtract((CVec3&)Bounds.Origin, (CVec3&)Bounds.BoxExtent, (CVec3&)BoundingBox.Min);
	VectorAdd     ((CVec3&)Bounds.Origin, (CVec3&)Bounds.BoxExtent, (CVec3&)BoundingBox.Max);

	unguard;

	unguard;
}


void USkeletalMesh::PostLoadMesh3()
{
	guard(USkeletalMesh::PostLoadMesh3);

	// sockets
	int NumSockets = Sockets.Num();
	if (NumSockets)
	{
		AttachAliases.Empty(NumSockets);
		AttachAliases.Add(NumSockets);
		AttachBoneNames.Empty(NumSockets);
		AttachBoneNames.Add(NumSockets);
		AttachCoords.Empty(NumSockets);
		AttachCoords.Add(NumSockets);
		for (int i = 0; i < NumSockets; i++)
		{
			USkeletalMeshSocket *S = Sockets[i];
			if (!S) continue;
			AttachAliases[i]       = S->SocketName;
			AttachBoneNames[i]     = S->BoneName;
			CCoords &C = (CCoords&)AttachCoords[i];
			C.origin = (CVec3&)S->RelativeLocation;
			SetAxis(S->RelativeRotation, C.axis);
		}
	}

	unguard;
}


float half2float(word h)
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
	mant = mant << 13;
	f.df = (sign << 31) | (exp << 23) | mant;
	return f.f;
}


void FStaticLODModel::RestoreMesh3(const USkeletalMesh &Mesh, const FStaticLODModel3 &Lod, const FSkeletalMeshLODInfo &Info)
{
	guard(FStaticLODModel::RestoreMesh3);

	// prepare arrays
	TArray<FPackedNormal> PointNormals;
	Points.Empty        (Lod.NumVertices);
	PointNormals.Empty  (Lod.NumVertices);
	Wedges.Empty        (Lod.NumVertices);
	VertInfluences.Empty(Lod.NumVertices * 4);

	// create wedges, vertices and influences
	guard(ProcessChunks);
	for (int Chunk = 0; Chunk < Lod.Chunks.Num(); Chunk++)
	{
		int Vert, j;
		const FSkinChunk3 &C = Lod.Chunks[Chunk];
		const FGPUSkin3   &S = Lod.GPUSkin;

		// when base mesh vertices are missing - try to get information from GPU skin
		if ((C.NumRigidVerts != C.RigidVerts.Num() && C.RigidVerts.Num() == 0) ||
			(C.NumSmoothVerts != C.SmoothVerts.Num() && C.SmoothVerts.Num() == 0))
		{
			guard(GPUVerts);
			if (!Chunk) printf("Restoring LOD verts from GPU skin\n", Mesh.Name);

			int LastVertex = C.FirstVertex + C.NumRigidVerts + C.NumSmoothVerts;

			for (Vert = C.FirstVertex; Vert < LastVertex; Vert++)
			{
				const FGPUVert3Common *V;
				FVector VPos;
				float VU, VV;

				if (!S.bUseFullPrecisionUVs)
				{
					if (!S.bUsePackedPosition)
					{
						const FGPUVert3Half &V0 = S.VertsHalf[Vert];
						V    = &V0;
						VPos = V0.Pos;
						VU   = half2float(V0.U);
						VV   = half2float(V0.V);
					}
					else
					{
						const FGPUVert3PackedHalf &V0 = S.VertsHalfPacked[Vert];
						V    = &V0;
						VPos = V0.Pos.ToVector(S.MeshOrigin, S.MeshExtension);
						VU   = half2float(V0.U);
						VV   = half2float(V0.V);
					}
				}
				else
				{
					if (!S.bUsePackedPosition)
					{
						const FGPUVert3Float &V0 = S.VertsFloat[Vert];
						V    = &V0;
						VPos = V0.Pos;
						VU   = V0.U;
						VV   = V0.V;
					}
					else
					{
						const FGPUVert3PackedFloat &V0 = S.VertsFloatPacked[Vert];
						V    = &V0;
						VPos = V0.Pos.ToVector(S.MeshOrigin, S.MeshExtension);
						VU   = V0.U;
						VV   = V0.V;
					}
				}
				// find the same point in previous items
				int PointIndex = -1;	// start with 0, see below
				while (true)
				{
					PointIndex = Points.FindItem(VPos, PointIndex + 1);
					if (PointIndex == INDEX_NONE) break;
					if (CompareCompNormals(PointNormals[PointIndex], V->Normal[2])) break;
				}
				if (PointIndex == INDEX_NONE)
				{
					// point was not found - create it
					PointIndex = Points.Add();
					Points[PointIndex] = VPos;
					PointNormals.AddItem(V->Normal[2]);
					// add influences
//					int TotalWeight = 0;
					for (int i = 0; i < 4; i++)
					{
						int BoneIndex  = V->BoneIndex[i];
						int BoneWeight = V->BoneWeight[i];
						if (BoneWeight == 0) continue;
						FVertInfluences *Inf = new(VertInfluences) FVertInfluences;
						Inf->PointIndex = PointIndex;
						Inf->Weight     = BoneWeight / 255.0f;
						Inf->BoneIndex  = C.Bones[BoneIndex];
//						TotalWeight += BoneWeight;
					}
//					assert(TotalWeight = 255);
				}
				// create wedge
				FMeshWedge *W = new(Wedges) FMeshWedge;
				W->iVertex = PointIndex;
				W->TexUV.U = VU;
				W->TexUV.V = VV;
			}
			unguardf(("vh=%d vf=%d ph=%d pf=%d [%d+%d+%d]",
				S.VertsHalf.Num(), S.VertsFloat.Num(), S.VertsHalfPacked.Num(), S.VertsFloatPacked.Num(),
                C.FirstVertex, C.NumRigidVerts, C.NumSmoothVerts));

			continue;		// process other chunks (not from CPU mesh)
		}

		// get information from base (CPU) mesh
		guard(RigidVerts);
		for (Vert = 0; Vert < C.NumRigidVerts; Vert++)
		{
			const FRigidVertex3 &V = C.RigidVerts[Vert];
			// find the same point in previous items
			int PointIndex = -1;	// start with 0, see below
			while (true)
			{
				PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
				if (PointIndex == INDEX_NONE) break;
				if (CompareCompNormals(PointNormals[PointIndex], V.Normal[2])) break;
			}
			if (PointIndex == INDEX_NONE)
			{
				// point was not found - create it
				PointIndex = Points.Add();
				Points[PointIndex] = V.Pos;
				PointNormals.AddItem(V.Normal[2]);
				// add influence
				FVertInfluences *Inf = new(VertInfluences) FVertInfluences;
				Inf->PointIndex = PointIndex;
				Inf->Weight     = 1.0f;
				Inf->BoneIndex  = C.Bones[V.BoneIndex];
			}
			// create wedge
			FMeshWedge *W = new(Wedges) FMeshWedge;
			W->iVertex = PointIndex;
			W->TexUV.U = V.U;
			W->TexUV.V = V.V;
		}
		unguard;

		guard(SmoothVerts);
		for (Vert = 0; Vert < C.NumSmoothVerts; Vert++)
		{
			const FSmoothVertex3 &V = C.SmoothVerts[Vert];
			// find the same point in previous items
			int PointIndex = -1;	// start with 0, see below
			while (true)
			{
				PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
				if (PointIndex == INDEX_NONE) break;
				if (CompareCompNormals(PointNormals[PointIndex], V.Normal[2])) break;
				//?? should compare influences too !
			}
			if (PointIndex == INDEX_NONE)
			{
				// point was not found - create it
				PointIndex = Points.Add();
				Points[PointIndex] = V.Pos;
				PointNormals.AddItem(V.Normal[2]);
				// add influences
				for (int i = 0; i < 4; i++)
				{
					int BoneIndex  = V.BoneIndex[i];
					int BoneWeight = V.BoneWeight[i];
					if (BoneWeight == 0) continue;
					FVertInfluences *Inf = new(VertInfluences) FVertInfluences;
					Inf->PointIndex = PointIndex;
					Inf->Weight     = BoneWeight / 255.0f;
					Inf->BoneIndex  = C.Bones[BoneIndex];
				}
			}
			// create wedge
			FMeshWedge *W = new(Wedges) FMeshWedge;
			W->iVertex = PointIndex;
			W->TexUV.U = V.U;
			W->TexUV.V = V.V;
		}
		unguard;
	}
	unguard;

	// count triangles (speed optimization for TArray allocations)
	int NumTriangles = 0;
	int Sec;
	for (Sec = 0; Sec < Lod.Sections.Num(); Sec++)
		NumTriangles += Lod.Sections[Sec].NumTriangles;
	Faces.Empty(NumTriangles);
	// create faces
	for (Sec = 0; Sec < Lod.Sections.Num(); Sec++)
	{
		const FSkelMeshSection3 &S = Lod.Sections[Sec];
//		if (S.unk1 != Sec) appNotify("Mesh %s: Sec=%d unk1=%d", Mesh.Name, Sec, S.unk1);	//??

		FSkelMeshSection *Dst = new (SmoothSections) FSkelMeshSection;
		int MaterialIndex = S.MaterialIndex;
		if (Info.LODMaterialMap.Num())
			MaterialIndex = Info.LODMaterialMap[MaterialIndex];
		Dst->MaterialIndex = MaterialIndex;
		Dst->FirstFace     = Faces.Num();
		Dst->NumFaces      = S.NumTriangles;

		int Index = S.FirstIndex;
		for (int i = 0; i < S.NumTriangles; i++)
		{
			FMeshFace *Face = new (Faces) FMeshFace;
			Face->MaterialIndex = MaterialIndex;
			Face->iWedge[0] = Lod.IndexBuffer.Indices[Index++];
			Face->iWedge[1] = Lod.IndexBuffer.Indices[Index++];
			Face->iWedge[2] = Lod.IndexBuffer.Indices[Index++];
		}
	}

	//!! recreate SmoothSections and RigidSections

	unguard;

	return;
}


/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

// following defines will help finding new undocumented compression schemes
#define FIND_HOLES			1
//#define DEBUG_DECOMPRESS	1

static void ReadTimeArray(FArchive &Ar, int NumKeys, TArray<float> &Times, int NumFrames)
{
	guard(ReadTimeArray);
	if (NumKeys <= 1) return;

//	printf("  pos=%4X keys (max=%X)[ ", Ar.Tell(), NumFrames);
	if (NumFrames < 256)
	{
		for (int k = 0; k < NumKeys; k++)
		{
			byte v;
			Ar << v;
			Times.AddItem(v);
//			if (k < 4 || k > NumKeys - 5) printf(" %02X ", v);
//			else if (k == 4) printf("...");
		}
	}
	else
	{
		for (int k = 0; k < NumKeys; k++)
		{
			word v;
			Ar << v;
			Times.AddItem(v);
//			if (k < 4 || k > NumKeys - 5) printf(" %04X ", v);
//			else if (k == 4) printf("...");
		}
	}
//	printf(" ]\n");

	// align to 4 bytes
	Ar.Seek(Align(Ar.Tell(), 4));

	unguard;
}


void UAnimSet::ConvertAnims()
{
	guard(UAnimSet::ConvertAnims);

	int i;

#if MASSEFF
	UBioAnimSetData *BioData = NULL;
	if ((Package->Game == GAME_MassEffect || Package->Game == GAME_MassEffect2) && !TrackBoneNames.Num() && Sequences.Num())
	{
		// Mass Effect has separated TrackBoneNames from UAnimSet to UBioAnimSetData
		BioData = Sequences[0]->m_pBioAnimSetData;
		if (BioData)
			CopyArray(TrackBoneNames, BioData->TrackBoneNames);
	}
#endif // MASSEFF

	RefBones.Empty(TrackBoneNames.Num());
	for (i = 0; i < TrackBoneNames.Num(); i++)
	{
		FNamedBone *Bone = new (RefBones) FNamedBone;
		Bone->Name = TrackBoneNames[i];
		// Flags, ParentIndex unused
	}

#if FIND_HOLES
	bool findHoles = true;
#endif
	int NumTracks = TrackBoneNames.Num();

	for (i = 0; i < Sequences.Num(); i++)
	{
		const UAnimSequence *Seq = Sequences[i];
		if (!Seq)
		{
			printf("WARNING: %s: no sequence %d\n", Name, i);
			continue;
		}
#if DEBUG_DECOMPRESS
	for (i = 0; i < Seq->CompressedTrackOffsets.Num(); i += 4)
	{
		int TransOffset = Seq->CompressedTrackOffsets[i  ];
		int TransKeys   = Seq->CompressedTrackOffsets[i+1];
		int RotOffset   = Seq->CompressedTrackOffsets[i+2];
		int RotKeys     = Seq->CompressedTrackOffsets[i+3];
		printf("[%d] = %d+%d ; %d+%d\n", i/4, TransOffset, TransKeys, RotOffset, RotKeys);
	}
#endif
#if MASSEFF
		if (Seq->m_pBioAnimSetData != BioData)
		{
			appNotify("Mass Effect AnimSequence %s/%s has different BioAnimSetData object, removing track",
				Name, *Seq->SequenceName);
			continue;
		}
#endif // MASSEFF
		// some checks
		int offsetsPerBone = 4;
#if TLR
		if (Package->Game == GAME_TLR) offsetsPerBone = 6;
#endif
#if XMEN
		if (Package->Game == GAME_XMen) offsetsPerBone = 6;		// has additional CutInfo array
#endif
		if (Seq->CompressedTrackOffsets.Num() != NumTracks * offsetsPerBone && !Seq->RawAnimData.Num())
		{
			appNotify("AnimSequence %s/%s has wrong CompressedTrackOffsets size (has %d, expected %d), removing track",
				Name, *Seq->SequenceName, Seq->CompressedTrackOffsets.Num(), NumTracks * offsetsPerBone);
			continue;
		}

		// create FMeshAnimSeq
		FMeshAnimSeq *Dst = new (AnimSeqs) FMeshAnimSeq;
		Dst->Name      = Seq->SequenceName;
		Dst->NumFrames = Seq->NumFrames;
		Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;
		// create MotionChunk
		MotionChunk *M = new (Moves) MotionChunk;

#if DEBUG_DECOMPRESS
		printf("ComprTrack: %d bytes, %d frames\n", Seq->CompressedByteStream.Num(), Seq->NumFrames);
#endif

		// bone tracks ...
		M->AnimTracks.Empty(NumTracks);

		FMemReader Reader(Seq->CompressedByteStream.GetData(), Seq->CompressedByteStream.Num());
		Reader.SetupFrom(*Package);
		bool hasTimeTracks = (Seq->KeyEncodingFormat == AKF_VariableKeyLerp);

		int offsetIndex = 0;
		for (int j = 0; j < NumTracks; j++, offsetIndex += offsetsPerBone)
		{
			AnalogTrack *A = new (M->AnimTracks) AnalogTrack;

			int k;

			if (!Seq->CompressedTrackOffsets.Num())	//?? or if RawAnimData.Num() != 0
			{
				// using RawAnimData array
				assert(Seq->RawAnimData.Num() == NumTracks);
				CopyArray(A->KeyPos,  Seq->RawAnimData[j].PosKeys);
				CopyArray(A->KeyQuat, Seq->RawAnimData[j].RotKeys);
				CopyArray(A->KeyTime, Seq->RawAnimData[j].KeyTimes);	// may be empty
				int k;
/*				if (!A->KeyTime.Num())
				{
					int numKeys = max(A->KeyPos.Num(), A->KeyQuat.Num());
					A->KeyTime.Empty(numKeys);
					for (k = 0; k < numKeys; k++)
						A->KeyTime.AddItem(k);
				} */
				for (k = 0; k < A->KeyTime.Num(); k++)	//??
					A->KeyTime[k] *= Dst->Rate;
				continue;
			}

			// read animations
			int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
			int TransKeys   = Seq->CompressedTrackOffsets[offsetIndex+1];
			int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+2];
			int RotKeys     = Seq->CompressedTrackOffsets[offsetIndex+3];
#if TLR
			int ScaleOffset = 0, ScaleKeys = 0;
			if (Package->Game == GAME_TLR)
			{
				ScaleOffset  = Seq->CompressedTrackOffsets[offsetIndex+4];
				ScaleKeys    = Seq->CompressedTrackOffsets[offsetIndex+5];
			}
#endif // TLR
//			printf("[%d:%d:%d] :  %d[%d]  %d[%d]  %d[%d]\n", j, Seq->RotationCompressionFormat, Seq->TranslationCompressionFormat, TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys);

			A->KeyPos.Empty(TransKeys);
			A->KeyQuat.Empty(RotKeys);
			if (hasTimeTracks)
			{
				A->KeyPosTime.Empty(TransKeys);
				A->KeyQuatTime.Empty(RotKeys);
			}

			// read translation keys
			if (TransKeys)
			{
#if FIND_HOLES
				int hole = TransOffset - Reader.Tell();
				if (findHoles && hole/** && abs(hole) > 4*/)	//?? should not be holes at all
				{
					appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before TransTrack (KeyFormat=%d/%d)",
						Name, *Seq->SequenceName, j, hole, Seq->KeyEncodingFormat, Seq->TranslationCompressionFormat);
///					findHoles = false;
				}
#endif // FIND_HOLES
				Reader.Seek(TransOffset);

#if TRANSFORMERS
				if (Package->Game == GAME_Transformers && TransKeys >= 4)
				{
					assert(Package->ArLicenseeVer >= 100);
					FVector v1, v2;
					Reader << v1.X;
					if (v1.X != -1)
					{
						Reader << v1.Y << v1.Z << v2;
//						printf("  trans: %g %g %g -- %g %g %g\n", FVECTOR_ARG(v1), FVECTOR_ARG(v2));
						for (k = 0; k < TransKeys; k++)
						{
#if 0
							unsigned int pos;
							Reader << pos;
							printf("  %08X\n", pos);
//!!						A->KeyPos.AddItem(vec); -- decode!
#else
							FPackedVectorTrans pos;
							Reader << pos;
							A->KeyPos.AddItem(pos.ToVector(v1, v2));
#endif
						}
						goto trans_keys_done;
					}
				}
#endif // TRANSFORMERS

				for (k = 0; k < TransKeys; k++)
				{
					if (Seq->TranslationCompressionFormat == ACF_None)
					{
						FVector vec;
						Reader << vec;
						A->KeyPos.AddItem(vec);
					}
/*!!#if BORDERLANDS
					//!! not implemented
					// should serialize 9 floats before keys, then short[3] keys
					// number of keys may be 1 less then specified (?)
					else if (Seq->TranslationCompressionFormat == ACF_Delta48NoW)
					{
						short v[3];
						Reader << v[0] << v[1] << v[2];
						FVector vec;
						vec.X = v[0];
						vec.Y = v[1];
						vec.Z = v[2];
						A->KeyPos.AddItem(vec);
					}
#endif // BORDERLANDS */
					else
						appError("Unknown translation compression method: %d", Seq->TranslationCompressionFormat);
				}

			trans_keys_done:
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (hasTimeTracks)
					ReadTimeArray(Reader, TransKeys, A->KeyPosTime, Seq->NumFrames);
			}
			else
			{
				static FVector zero;
				A->KeyPos.AddItem(zero);
				appNotify("No translation keys!");
			}

#if DEBUG_DECOMPRESS
			int TransEnd = Reader.Tell();
#endif
#if FIND_HOLES
			int hole = RotOffset - Reader.Tell();
			if (findHoles && hole/** && abs(hole) > 4*/)	//?? should not be holes at all
			{
				appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before RotTrack (KeyFormat=%d/%d)",
					Name, *Seq->SequenceName, j, hole, Seq->KeyEncodingFormat, Seq->RotationCompressionFormat);
///				findHoles = false;
			}
#endif // FIND_HOLES
			// read rotation keys
			Reader.Seek(RotOffset);
			if (RotKeys == 1)
			{
				FQuatFloat96NoW q;
				Reader << q;
				A->KeyQuat.AddItem(q);
			}
			else
			{
				// read mins/ranges
				FVector Mins, Ranges;
				if (Seq->RotationCompressionFormat == ACF_IntervalFixed32NoW || Package->ArVer < 761)
				{
					// starting with version 761 Mins/Ranges are read only when needed - i.e. for ACF_IntervalFixed32NoW
					Reader << Mins << Ranges;
				}
#if TRANSFORMERS
				if (Package->Game == GAME_Transformers)
				{
					FQuat Unk;
					Reader << Unk;
//					printf("  rot   %g %g %g %g\n", FQUAT_ARG(Unk));
				}
#endif // TRANSFORMERS

				for (k = 0; k < RotKeys; k++)
				{
					if (Seq->RotationCompressionFormat == ACF_None)
					{
						FQuat q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (Seq->RotationCompressionFormat == ACF_Float96NoW)
					{
						FQuatFloat96NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (Seq->RotationCompressionFormat == ACF_Fixed48NoW)
					{
						FQuatFixed48NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (Seq->RotationCompressionFormat == ACF_Fixed32NoW)
					{
						FQuatFixed32NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (Seq->RotationCompressionFormat == ACF_IntervalFixed32NoW)
					{
						FQuatIntervalFixed32NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q.ToQuat(Mins, Ranges));
					}
					else if (Seq->RotationCompressionFormat == ACF_Float32NoW)
					{
						FQuatFloat32NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
#if BATMAN
					else if (Seq->RotationCompressionFormat == ACF_Fixed48Max)
					{
						FQuatFixed48Max q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
#endif // BATMAN
#if MASSEFF
					// Mass Effect 2 animation compression
					else if (Seq->RotationCompressionFormat == ACF_BioFixed48)
					{
						FQuatBioFixed48 q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
#endif // MASSEFF
/*!!#if BORDERLANDS
					//!! not implemented
					// should serialize 4 floats before keys, then short[3] keys
					// number of keys may be 1 less then specified (?)
					else if (Seq->RotationCompressionFormat == ACF_Delta48NoW)
					{
						short v[3];
						Reader << v[0] << v[1] << v[2];
						FQuat q;
						q.Set(0, 0, 0, 1);
						A->KeyQuat.AddItem(q);
					}
#endif // BORDERLANDS */
#if TRANSFORMERS
					else if (Seq->RotationCompressionFormat == ACF_IntervalFixed48NoW)
					{
						FQuatIntervalFixed48NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q.ToQuat(Mins, Ranges));
					}
#endif // TRANSFORMERS
					else
						appError("Unknown rotation compression method: %d", Seq->RotationCompressionFormat);
				}

			rot_keys_done:
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (hasTimeTracks)
					ReadTimeArray(Reader, RotKeys, A->KeyQuatTime, Seq->NumFrames);
			}
#if TLR
			if (ScaleKeys)
			{
				//?? no ScaleKeys support, simply drop data
				Reader.Seek(ScaleOffset + ScaleKeys * 12);
				Reader.Seek(Align(Reader.Tell(), 4));
			}
#endif // TLR
#if DEBUG_DECOMPRESS
//			printf("[%s : %s] Frames=%d KeyPos.Num=%d KeyQuat.Num=%d KeyFmt=%s\n", *Seq->SequenceName, *TrackBoneNames[j],
//				Seq->NumFrames, A->KeyPos.Num(), A->KeyQuat.Num(), *Seq->KeyEncodingFormat);
			printf("    [%d]: %d - %d + %d - %d (%d/%d)\n", j,
				TransOffset, TransEnd, RotOffset, Reader.Tell(), TransKeys, RotKeys);
#endif // DEBUG_DECOMPRESS
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

#if TRANSFORMERS

struct FTRStaticMeshSectionUnk
{
	int					f0;
	int					f4;

	friend FArchive& operator<<(FArchive &Ar, FTRStaticMeshSectionUnk &S)
	{
		return Ar << S.f0 << S.f4;
	}
};

SIMPLE_TYPE(FTRStaticMeshSectionUnk, int)

#endif // TRANSFORMERS

#if MOH2010

struct FMOHStaticMeshSectionUnk
{
	TArray<int>			f4;
	TArray<int>			f10;
	TArray<short>		f1C;
	TArray<short>		f28;
	TArray<short>		f34;
	TArray<short>		f40;
	TArray<short>		f4C;
	TArray<short>		f58;

	friend FArchive& operator<<(FArchive &Ar, FMOHStaticMeshSectionUnk &S)
	{
		return Ar << S.f4 << S.f10 << S.f1C << S.f28 << S.f34 << S.f40 << S.f4C << S.f58;
	}
};

#endif // MOH2010


struct FStaticMeshSection3
{
	UMaterial			*Mat;
	int					f10;		//?? bUseSimple...Collision
	int					f14;		//?? ...
	int					bEnableShadowCasting;
	int					FirstIndex;
	int					NumFaces;
	int					f24;
	int					f28;
	int					Index;		//?? index of section
	TArray<FMesh3Unk1>	f30;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshSection3 &S)
	{
		guard(FStaticMeshSection3<<);
		Ar << S.Mat << S.f10 << S.f14;
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
		if (Ar.ArVer >= 492) Ar << S.Index;		//?? real version is unknown! No field in GOW1_PC (490), has in UT3 (512)
#if ALPHA_PR
		if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 13)
		{
			int unk30, unk34;
			Ar << unk30 << unk34;
		}
#endif // ALPHA_PR
		if (Ar.ArVer >= 514) Ar << S.f30;
#if ALPHA_PR
		if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 39)
		{
			int unk38;
			Ar << unk38;
		}
#endif // ALPHA_PR
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 49)
		{
			TArray<FTRStaticMeshSectionUnk> f30;
			Ar << f30;
		}
#endif // TRANSFORMERS
#if MOH2010
		if (Ar.Game == GAME_MOH2010 && Ar.ArVer >= 575)
		{
			byte flag;
			FMOHStaticMeshSectionUnk unk3C;
			Ar << flag;
			if (flag) Ar << unk3C;
		}
#endif // MOH2010
		if (Ar.ArVer >= 618)
		{
			byte unk;
			Ar << unk;
			assert(unk == 0);
		}
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
		Ar << S.VertexSize << S.NumVerts;
#if BATMAN
		if (Ar.Game == GAME_Batman && Ar.ArLicenseeVer >= 0x11)
		{
			int unk18;					// default is 1
			Ar << unk18;
		}
#endif // BATMAN
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
		Ar << RAW_ARRAY(S.Verts);
		return Ar;
	}
};


static int  GNumStaticUVSets   = 1;
static bool GUseStaticFloatUVs = true;

struct FStaticMeshUVItem3
{
	FVector				Pos;			// old version (< 472)
	FPackedNormal		Normal[3];
	int					f10;			//?? VertexColor?
	float				U, V;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVItem3 &V)
	{
		guard(FStaticMeshUVItem3<<);

#if MKVSDC
		if (Ar.Game == GAME_MK)
		{
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

		if (Ar.ArVer < 472)
		{
			// old version has position embedded into UVStream (this is not an UVStream, this is a single stream for everything)
			int unk10;					// pad or color ?
			Ar << V.Pos << unk10;
		}
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
		if (Ar.ArVer >= 434 && Ar.ArVer < 615)
			Ar << V.f10;				// starting from 615 made as separate stream
	uvs:
		if (GUseStaticFloatUVs)
		{
			Ar << V.U << V.V;
		}
		else
		{
			// read in half format and convert to float
			word U0, V0;
			Ar << U0 << V0;
			V.U = half2float(U0);
			V.V = half2float(V0);
		}
		// skip extra UV sets
		int UVSize = GUseStaticFloatUVs ? sizeof(float) : sizeof(short);
		if (GNumStaticUVSets > 1) Ar.Seek(Ar.Tell() + (GNumStaticUVSets - 1) * 2 * UVSize);
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
		Ar << S.NumTexCoords << S.ItemSize << S.NumVerts;
		S.bUseFullPrecisionUVs = true;
#if AVA
		if (Ar.Game == GAME_AVA && Ar.ArVer >= 441) goto new_ver;
#endif
		if (Ar.ArVer >= 474)
		{
		new_ver:
			Ar << S.bUseFullPrecisionUVs;
		}
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
		// prepare for UV serialization
		GNumStaticUVSets   = S.NumTexCoords;
		GUseStaticFloatUVs = S.bUseFullPrecisionUVs;
		Ar << RAW_ARRAY(S.UV);
		return Ar;
	}
};

struct FStaticMeshColorStream3
{
	int					ItemSize;
	int					NumVerts;
	TArray<int>			Colors;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshColorStream3 &S)
	{
		return Ar << S.ItemSize << S.NumVerts << RAW_ARRAY(S.Colors);
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

struct FStaticMeshUVStream3Old			// ArVer < 364
{
	TArray<FStaticMeshUV> Data;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVStream3Old &S)
	{
		int unk;						// Revision?
		Ar << S.Data;					// used RAW_ARRAY, but RAW_ARRAY is newer than this version
		if (Ar.ArVer < 297) Ar << unk;
		return Ar;
	}
};

struct FStaticMeshLODModel
{
	FByteBulkData		BulkData;		// ElementSize = 0xFC for UT3 and 0x170 for UDK ... it's simpler to skip it
	TArray<FStaticMeshSection3> Sections;
	FStaticMeshVertexStream3 VertexStream;
	FStaticMeshUVStream3     UVStream;
	FStaticMeshColorStream3  ColorStream;	//??
	FIndexBuffer3		Indices;
	FIndexBuffer3		Indices2;		//?? wireframe?
	int					f80;
	TArray<FEdge3>		Edges;			//??
	TArray<byte>		fEC;			//?? flags for faces?

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshLODModel &Lod)
	{
		guard(FStaticMeshLODModel<<);

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
#if 0
		printf("%d sections\n", Lod.Sections.Num());
		for (int i = 0; i < Lod.Sections.Num(); i++)
		{
			FStaticMeshSection3 &S = Lod.Sections[i];
			printf("Mat: %s\n", S.Mat ? S.Mat->Name : "?");
			printf("  %d %d sh=%d i0=%d NF=%d %d %d idx=%d\n", S.f10, S.f14, S.bEnableShadowCasting, S.FirstIndex, S.NumFaces, S.f24, S.f28, S.Index);
		}
#endif
		// serialize vertex and uv streams
#if A51
		if (Ar.Game == GAME_A51) goto new_ver;
#endif
#if MKVSDC || AVA
		if (Ar.Game == GAME_MK || Ar.Game == GAME_AVA) goto ver_3;
#endif
#if BORDERLANDS
		if (Ar.Game == GAME_Borderlands)
		{
			// refined field set
			Ar << Lod.VertexStream;
			Ar << Lod.UVStream;
			Ar << Lod.f80;
			Ar << Lod.Indices;
			// note: no fEC (smoothing groups?)
			return Ar;
		}
#endif // BORDERLANDS
		if (Ar.ArVer >= 472)
		{
		new_ver:
			Ar << Lod.VertexStream;
			Ar << Lod.UVStream;
#if TRANSFORMERS
			if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 71)
			{
				FTRMeshUnkStream unkStream;
				Ar << unkStream;
			}
#endif // TRANSFORMERS
#if MOH2010
			if (Ar.Game == GAME_MOH2010 && Ar.ArLicenseeVer >= 55) goto color_stream;
#endif
			// unknown data in UDK
			if (Ar.ArVer >= 615)
			{
			color_stream:
				// new color stream: difference is that data array is not serialized when NumVerts is 0
				int ColorItemSize, ColorNumVerts;
				Ar << ColorItemSize << ColorNumVerts;
				TArray<int> ColorData;
				if (ColorNumVerts) Ar << RAW_ARRAY(ColorData);
			}
			if (Ar.ArVer < 686) Ar << Lod.ColorStream;
			Ar << Lod.f80;
		}
		else if (Ar.ArVer >= 466)
		{
		ver_3:
			Ar << Lod.VertexStream;
			Ar << Lod.UVStream;
			Ar << Lod.f80;
#if MKVSDC || AVA
			if (Ar.Game == GAME_MK || Ar.Game == GAME_AVA)
			{
				// note: sometimes UVStream has 2 times more items than VertexStream
				// we should duplicate vertices
				int n1 = Lod.VertexStream.Verts.Num();
				int n2 = Lod.UVStream.UV.Num();
				if (n1 * 2 == n2)
				{
					printf("Duplicating MK StaticMesh verts\n");
					Lod.VertexStream.Verts.Add(n1);
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
			Ar << Lod.f80;
			// create VertexStream
			int NumVerts = Lod.UVStream.UV.Num();
			Lod.VertexStream.Verts.Empty(NumVerts);
//			Lod.VertexStream.NumVerts = NumVerts;
			for (int i = 0; i < NumVerts; i++)
				Lod.VertexStream.Verts.AddItem(Lod.UVStream.UV[i].Pos);
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
				Ar << Verts << Normals << UVStream;	// really used RAW_ARRAY, but it is too new for this code
				//!! convert
			}
			else
			{
				// oldest version
				TArray<FStaticMeshVertex3Old> Verts;
				Ar << Verts << UVStream;
				// convert vertex stream
				int i;
				int NumVerts = Verts.Num();
//				int numUVs   = UVStream.Num();
				Lod.VertexStream.Verts.Empty(NumVerts);
				Lod.VertexStream.Verts.Add(NumVerts);
				Lod.UVStream.UV.Empty();
				Lod.UVStream.UV.Add(NumVerts);
				Lod.UVStream.NumVerts = NumVerts;
				// resize UV streams
				for (i = 0; i < NumVerts; i++)
				{
					FStaticMeshVertex3Old &V = Verts[i];
					FStaticMeshUV       &SUV = UVStream[0].Data[i];
					FVector              &DV = Lod.VertexStream.Verts[i];
					FStaticMeshUVItem3   &UV = Lod.UVStream.UV[i];
					DV           = V.Pos;
					UV.Normal[2] = V.Normal[2];
					UV.U = SUV.U;
					UV.V = SUV.V;
				}
			}
			//!! note: this code will crash in RestoreMesh3() because of empty data
		}
		Ar << Lod.Indices;
#if APB
		if (Ar.Game == GAME_APB)
		{
			// serialized FIndexBuffer3 guarded by APB bulk seeker (check UTexture3::Serialize() for details)
			Ar.Seek(Ar.Tell() + 8);
			goto after_indices;				// do not need this data
		}
#endif // APB
		Ar << Lod.Indices2;
	after_indices:

#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 58)
		{
			int unkD8;
			Ar << unkD8;
		}
#endif // TRANSFORMERS

		if (Ar.ArVer < 686)
		{
			Ar << RAW_ARRAY(Lod.Edges);
			Ar << Lod.fEC;
		}
#if ALPHA_PR
		if (Ar.Game == GAME_AlphaProtocol)
		{
			assert(Ar.ArLicenseeVer > 8);	// ArLicenseeVer = [1..7] has custom code
			if (Ar.ArLicenseeVer >= 4)
			{
				TArray<int> unk128;
				Ar << RAW_ARRAY(unk128);
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

		return Ar;

		unguard;
	}
};

struct FStaticMeshUnk1
{
	FVector				v1;
	FVector				v2;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUnk1 &V)
	{
#if ENSLAVED
		if (Ar.Game == GAME_Enslaved)
		{
			// compressed structure
			short v1[3], v2[3];
			Ar << v1[0] << v1[1] << v1[2] << v2[0] << v2[1] << v2[2];
			return Ar;
		}
#endif // ENSLAVED
		return Ar << V.v1 << V.v2;
	}
};

struct FkDOPNode3
{
	FStaticMeshUnk1		f0;
	int					f18;
	short				f1C;
	short				f1E;

	friend FArchive& operator<<(FArchive &Ar, FkDOPNode3 &V)
	{
#if ENSLAVED
		if (Ar.Game == GAME_Enslaved)
		{
			// all data compressed
			byte  fC, fD;
			short fE;
			Ar << V.f0;		// compressed
			Ar << fC << fD << fE;
			return Ar;
		}
#endif // ENSLAVED
		Ar << V.f0 << V.f18;
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 7)
			goto new_ver;
#endif
		if (Ar.ArVer >= 468)
		{
		new_ver:
			Ar << V.f1C << V.f1E;	// short
		}
		else
		{
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

struct FkDOPTriangle3
{
	short				f0, f2, f4, f6;

	friend FArchive& operator<<(FArchive &Ar, FkDOPTriangle3 &V)
	{
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 7)
			goto new_ver;
#endif
		if (Ar.ArVer >= 468)
		{
		new_ver:
			Ar << V.f0 << V.f2 << V.f4 << V.f6;
		}
		else
		{
			assert(Ar.IsLoading);
			int tmp0, tmp2, tmp4, tmp6;
			Ar << tmp0 << tmp2 << tmp4 << tmp6;
			V.f0 = tmp0;
			V.f2 = tmp2;
			V.f4 = tmp4;
			V.f6 = tmp6;
		}
		return Ar;
	}
};

/*struct FStaticMeshUnk4
{
	TArray<UObject*>	f0;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUnk4 &V)
	{
		return Ar << V.f0;
	}
};*/

void UStaticMesh::SerializeStatMesh3(FArchive &Ar)
{
	guard(UStaticMesh::SerializeStatMesh3);

	UObject::Serialize(Ar);					// no UPrimitive ...

	// NOTE: UStaticMesh is not mirrored by script with exception of Transformers game
	FBoxSphereBounds	Bounds;
	UObject				*BodySetup;			// URB_BodySetup
	int					InternalVersion;	// GOW1_PC: 15, UT3: 16, UDK: 18-...
	TArray<FStaticMeshLODModel> Lods;
//	TArray<FStaticMeshUnk4> f48;
	// kDOP tree
	TArray<FkDOPNode3>     kDOPNodes;
	TArray<FkDOPTriangle3> kDOPTriangles;

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
		Ar << RAW_ARRAY(kDOPNodes) << RAW_ARRAY(kDOPTriangles) << Lods;
		// Bounds serialized as property (see UStaticMesh in h-file)
		Bounds = this->Bounds;
		goto done;
	}
#endif // TRANSFORMERS

	Ar << Bounds << BodySetup;
	if (Ar.ArVer < 315)
	{
		UObject *unk;
		Ar << unk;
	}
	// kDOP tree
	Ar << RAW_ARRAY(kDOPNodes) << RAW_ARRAY(kDOPTriangles);
	Ar << InternalVersion;
//	printf("kDOPNodes=%d kDOPTriangles=%d\n", kDOPNodes.Num(), kDOPTriangles.Num());
//	printf("ver: %d\n", InternalVersion);
	if (InternalVersion >= 17 && Ar.ArVer < 593)
	{
		TArray<FName> unk;			// some text properties; ContentTags ? (switched from binary to properties)
		Ar << unk;
	}
	Ar << Lods;

//	Ar << f48;

done:
	// ignore remaining part
	Ar.Seek(Ar.GetStopper());
	if (!Lods.Num()) return;

	RestoreMesh3(Lods[0]);

	// setup missing properties
	BoundingSphere.R = Bounds.SphereRadius / 2;		//?? UE3 meshes has radius 2 times larger than mesh
	VectorSubtract((CVec3&)Bounds.Origin, (CVec3&)Bounds.BoxExtent, (CVec3&)BoundingBox.Min);
	VectorAdd     ((CVec3&)Bounds.Origin, (CVec3&)Bounds.BoxExtent, (CVec3&)BoundingBox.Max);

	unguard;
}

void UStaticMesh::RestoreMesh3(const FStaticMeshLODModel &Lod)
{
	guard(UStaticMesh::RestoreMesh3);

	int i;

	// materials and sections
	int NumSections = Lod.Sections.Num();
	Sections.Empty(NumSections);
	Materials.Empty(NumSections);
	for (i = 0; i < NumSections; i++)
	{
		// source
		const FStaticMeshSection3 &Sec = Lod.Sections[i];
		// destination
		FStaticMeshMaterial *DM = new (Materials) FStaticMeshMaterial;
		FStaticMeshSection  *DS = new (Sections)  FStaticMeshSection;
		DM->Material   = Sec.Mat;
		DS->FirstIndex = Sec.FirstIndex;
		DS->NumFaces   = Sec.NumFaces;
	}

	// vertices and UVs
	int NumVerts = Lod.VertexStream.Verts.Num();
	assert(Lod.UVStream.NumVerts == NumVerts);
	VertexStream.Vert.Empty(NumVerts);
	FStaticMeshUVStream *UVS = new (UVStream) FStaticMeshUVStream;
	UVS->Data.Empty(NumVerts);
	for (i = 0; i < NumVerts; i++)
	{
		// source
		const FStaticMeshUVItem3 &SUV = Lod.UVStream.UV[i];
		// destination
		FStaticMeshUV    *UV = new (UVS->Data) FStaticMeshUV;
		FStaticMeshVertex *V = new (VertexStream.Vert) FStaticMeshVertex;
		V->Pos = Lod.VertexStream.Verts[i];
		UV->U = SUV.U;
		UV->V = SUV.V;
		// normal
		V->Normal = SUV.Normal[2];
	}
	//?? also has ColorStream

	// indices
	CopyArray(IndexStream1.Indices, Lod.Indices.Indices);

	unguard;
}


#endif // UNREAL3
