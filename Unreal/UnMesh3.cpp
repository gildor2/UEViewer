#include "Core.h"
#include "UnrealClasses.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"				// for checking game type

#if UNREAL3

struct FSkelMeshSection3
{
	short				MaterialIndex;
	short				unk1;
	int					FirstIndex;
	short				NumTriangles;

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection3 &S)
	{
		return Ar << S.MaterialIndex << S.unk1 << S.FirstIndex << S.NumTriangles;
	}
};

struct FIndexBuffer3
{
	TRawArray<short>	Indices;

	friend FArchive& operator<<(FArchive &Ar, FIndexBuffer3 &I)
	{
		return Ar << I.Indices;
	}
};

static bool CompareCompNormals(int Normal1, int Normal2)
{
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
	int					Normal[3];		// FVectorComp (FVector as 4 bytes)
	float				U, V;
	byte				BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, FRigidVertex3 &V)
	{
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
		Ar << V.U << V.V;
#if MEDGE
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 13)
		{
			float U1, V1, U2, V2;
			Ar << U1 << V1 << U2 << V2;
		}
#endif // MEDGE
		Ar << V.BoneIndex;
		return Ar;
	}
};

struct FSmoothVertex3
{
	FVector				Pos;
	int					Normal[3];		// FVectorComp (FVector as 4 bytes)
	float				U, V;
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FSmoothVertex3 &V)
	{
		int i;
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2] << V.U << V.V;
#if MEDGE
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 13)
		{
			float U1, V1, U2, V2;
			Ar << U1 << V1 << U2 << V2;
		}
#endif // MEDGE
#if MKVSDC
		if (Ar.IsMK && Ar.ArLicenseeVer >= 11)
		{
			float U1, V1;
			Ar << U1 << V1;
		}
#endif // MKVSDC
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
		if (Ar.IsArmyOf2 && Ar.ArLicenseeVer >= 7)
		{
			TRawArray<FMeshUV> extraUV;
			Ar << extraUV;
		}
#endif // ARMYOF2
		if (Ar.ArVer >= 362)
			Ar << V.MaxInfluences;
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
		return Ar << V.iVertex[0] << V.iVertex[1] << V.iFace[0] << V.iFace[1];
	}
};

#if BATMAN
struct FEdge3Bat
{
	short				iVertex[2];
	short				iFace[2];

	friend FArchive& operator<<(FArchive &Ar, FEdge3Bat &V)
	{
		return Ar << V.iVertex[0] << V.iVertex[1] << V.iFace[0] << V.iFace[1];
	}
};
#endif // BATMAN

SIMPLE_TYPE(FEdge3, int)

// Structure holding normals and bone influeces
struct FGPUVert3Common
{
	int					Normal[3];		// FVectorComp (FVector as 4 bytes)
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Common &V)
	{
		int i;
		Ar << V.Normal[0] << V.Normal[1];
		if (Ar.ArVer < 494)
			Ar << V.Normal[2];
		for (i = 0; i < 4; i++) Ar << V.BoneIndex[i];
		for (i = 0; i < 4; i++) Ar << V.BoneWeight[i];
		return Ar;
	}
};

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
		return Ar;
	}
};

struct FGPUVert3Float : FGPUVert3Common
{
	FVector				Pos;
	float				U, V;

	FGPUVert3Float &operator=(const FSmoothVertex3 &S)
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
		return Ar;
	}
};

struct FVectorIntervalFixed32
{
	int					Value;

	friend FArchive& operator<<(FArchive &Ar, FVectorIntervalFixed32 &V)
	{
		return Ar << V.Value;
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
	int							bUseFullPrecisionUVs;		// 0 = half, 1 = float; copy of corresponding USkeletalMesh field
	// compressed position data
	int							bUseFullPrecisionPosition;	// 0 = packed FVector (32-bit), 1 = FVector (96-bit)
	FVector						MeshOrigin;
	FVector						MeshExtension;
	// vertex sets
	TRawArray<FGPUVert3Half>		VertsHalf;				// only one of these vertex sets are used
	TRawArray<FGPUVert3Float>		VertsFloat;
	TRawArray<FGPUVert3PackedFloat>	VertsHalfPacked;		//?? unused
	TRawArray<FGPUVert3PackedHalf>	VertsFloatPacked;		//?? unused

	friend FArchive& operator<<(FArchive &Ar, FGPUSkin3 &S)
	{
		guard(FSkinData3<<);

	#if HUXLEY
		if (Ar.IsHuxley) goto old_version;
	#endif

	#if ARMYOF2
		if (Ar.IsArmyOf2 && Ar.ArLicenseeVer >= 74)
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
			TRawArray<FSmoothVertex3> Verts;
			Ar << Verts;
			// convert verts
			CopyArray(S.VertsFloat, Verts);
			S.bUseFullPrecisionUVs      = true;
			S.bUseFullPrecisionPosition = true;
			return Ar;
		}

		// new version
		// serialize type information
	#if MEDGE
		int NumUVSets = 1;
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 0xF)
			Ar << NumUVSets;
	#endif // MEDGE
		Ar << S.bUseFullPrecisionUVs;
		S.bUseFullPrecisionPosition = true;
		if (Ar.ArVer >= 592)
			Ar << S.bUseFullPrecisionPosition << S.MeshOrigin << S.MeshExtension;

		//?? UE3 ignored this - forced bUseFullPrecisionPosition in FGPUSkin3 serializer ?
//		printf("data: %d %d\n", S.bUseFullPrecisionUVs, S.bUseFullPrecisionPosition);
		S.bUseFullPrecisionPosition = true;

		// serialize vertex array
		if (!S.bUseFullPrecisionUVs)
		{
	#if MEDGE
			if (NumUVSets > 1)
			{
				SkipRawArray(Ar, 0x20 + (NumUVSets - 1) * 4);
				return Ar;
			}
	#endif // MEDGE
			if (S.bUseFullPrecisionPosition)
				Ar << S.VertsHalf;
			else
				Ar << S.VertsHalfPacked;
		}
		else
		{
	#if MEDGE
			if (NumUVSets > 1)
			{
				SkipRawArray(Ar, 0x24 + (NumUVSets - 1) * 8);
				return Ar;
			}
	#endif // MEDGE
			if (S.bUseFullPrecisionPosition)
				Ar << S.VertsFloat;
			else
				Ar << S.VertsFloatPacked;
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

struct FMesh3Unk2
{
	TArray<FMesh3Unk1>	f0;

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk2 &S)
	{
		return Ar << S.f0;
	}
};

struct FStaticLODModel3
{
	TArray<FSkelMeshSection3> Sections;
	TArray<FSkinChunk3>	Chunks;
	FIndexBuffer3		IndexBuffer;
	TArray<short>		UsedBones;		// bones, value = [0, NumBones-1]
	TArray<byte>		f24;			// count = NumBones, value = [0, NumBones-1]; note: BoneIndex is 'short', not 'byte' ...
	TArray<short>		f68;			// indices, value = [0, NumVertices-1]
	TArray<byte>		f74;			// count = NumTriangles
	int					f80;
	int					NumVertices;
	TArray<FEdge3>		Edges;			// links 2 vertices and 2 faces (triangles)
	FWordBulkData		BulkData;		// ElementCount = NumVertices
	FGPUSkin3			GPUSkin;
	TArray<FMesh3Unk2>	fC4;			// unknown, has in GoW2

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModel3 &Lod)
	{
		guard(FStaticLODModel3<<);

		int tmp1;
		Ar << Lod.Sections << Lod.IndexBuffer;
		if (Ar.ArVer < 297)
			Ar << tmp1;
		Ar << Lod.f68 << Lod.UsedBones << Lod.f74 << Lod.Chunks << Lod.f80 << Lod.NumVertices;
#if BATMAN
		if (Ar.IsBatman && Ar.ArLicenseeVer >= 5)
		{
			FEdge3Bat tmpEdges;		// Batman code
			Ar << tmpEdges;
		}
		else
#endif // BATMAN
		{
			Ar << Lod.Edges;
		}
		Ar << Lod.f24;
		Lod.BulkData.Serialize(Ar);
#if ARMYOF2
		if (Ar.IsArmyOf2 && Ar.ArLicenseeVer >= 7)
		{
			int unk84;
			TRawArray<FMeshUV> extraUV;
			Ar << unk84 << extraUV;
		}
#endif // ARMYOF2
		if (Ar.ArVer >= 333)
			Ar << Lod.GPUSkin;
#if MEDGE
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 0xF)
			return Ar;
#endif // MEDGE
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

void USkeletalMesh::SerializeSkelMesh3(FArchive &Ar)
{
	guard(USkeletalMesh::SerializeSkelMesh3);

	UObject::Serialize(Ar);			// no UPrimitive ...

	FBoxSphereBounds	Bounds;
	TArray<UMaterial*>	Materials1;	// MaterialInterface*
	TArray<FStaticLODModel3> Lods;

#if MEDGE
	if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 0xF)
	{
		int unk264;
		Ar << unk264;
	}
#endif // MEDGE
	Ar << Bounds;
#if BATMAN
	if (Ar.IsBatman && Ar.ArLicenseeVer >= 0x0F)
	{
		float ConservativeBounds;
		TArray<FBoneBounds> PerBoneBounds;
		Ar << ConservativeBounds << PerBoneBounds;
	}
#endif // BATMAN
	Ar << Materials1 << MeshOrigin << RotOrigin;
	Ar << RefSkeleton << SkeletalDepth;
#if A51 || MKVSDC
	if ((Ar.IsA51 || Ar.IsMK) && Ar.ArLicenseeVer >= 0xF)
	{
		TArray<FMaterialBone> MaterialBones;
		Ar << MaterialBones;
	}
#endif // A51 || MKVSDC
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

	//!! Optimize search for PointIndex in RestoreMesh3()
	/*!!
	 *	Profile results (GoW2 package): total time = 7.8s, partial:
	 *	load = 1.5s, GPU lods -> CPU = 5.5s, restore mesh = 0.7s
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


static float half2float(word h)
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
	TArray<int> PointNormals;
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

		// when base mesh vertices are missing - try to get information from GPU skin
		if ((C.NumRigidVerts != C.RigidVerts.Num() && C.RigidVerts.Num() == 0) ||
			(C.NumSmoothVerts != C.SmoothVerts.Num() && C.SmoothVerts.Num() == 0))
		{
			guard(GPUVerts);
			if (!Chunk) printf("Restoring LOD verts from GPU skin\n", Mesh.Name);

			const FGPUSkin3 &S = Lod.GPUSkin;
			int LastVertex = C.FirstVertex + C.NumRigidVerts + C.NumSmoothVerts;

			for (Vert = C.FirstVertex; Vert < LastVertex; Vert++)
			{
				const FGPUVert3Common *V;
				FVector VPos;
				float VU, VV;
				//!! have not seen packed position meshes, check FGPUSkin3 serializer for details
				assert(S.bUseFullPrecisionPosition);
				if (!S.bUseFullPrecisionUVs)
				{
					const FGPUVert3Half &V0 = S.VertsHalf[Vert];
					V    = &V0;
					VPos = V0.Pos;
					VU   = half2float(V0.U);
					VV   = half2float(V0.V);
				}
				else
				{
					const FGPUVert3Float &V0 = S.VertsFloat[Vert];
					V    = &V0;
					VPos = V0.Pos;
					VU   = V0.U;
					VV   = V0.V;
				}
				// find the same point in previous items
#if 0
				int PointIndex = INDEX_NONE;
				for (j = 0; j < Points.Num(); j++)
				{
					if (Points[j] == VPos && CompareCompNormals(PointNormals[j], V->Normal[0]))
					{
						PointIndex = j;
						break;
					}
				}
#else
				int PointIndex = -1;	// start with 0, see below
				while (true)
				{
					PointIndex = Points.FindItem(VPos, PointIndex + 1);
					if (PointIndex == INDEX_NONE) break;
					if (CompareCompNormals(PointNormals[PointIndex], V->Normal[0])) break;
				}
#endif
				if (PointIndex == INDEX_NONE)
				{
					// point was not found - create it
					PointIndex = Points.Add();
					Points[PointIndex] = VPos;
					PointNormals.AddItem(V->Normal[0]);
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
			unguard;

			continue;		// process other chunks
		}

		// get information from base (CPU) mesh
		guard(RigidVerts);
		for (Vert = 0; Vert < C.NumRigidVerts; Vert++)
		{
			const FRigidVertex3 &V = C.RigidVerts[Vert];
			// find the same point in previous items
#if 0
			int PointIndex = INDEX_NONE;
			for (j = 0; j < Points.Num(); j++)
			{
				if (Points[j] == V.Pos && CompareCompNormals(PointNormals[j], V.Normal[0]))
				{
					PointIndex = j;
					break;
				}
			}
#else
			int PointIndex = -1;	// start with 0, see below
			while (true)
			{
				PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
				if (PointIndex == INDEX_NONE) break;
				if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
			}
#endif
			if (PointIndex == INDEX_NONE)
			{
				// point was not found - create it
				PointIndex = Points.Add();
				Points[PointIndex] = V.Pos;
				PointNormals.AddItem(V.Normal[0]);
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
#if 0
			int PointIndex = INDEX_NONE;
			for (j = 0; j < Points.Num(); j++)	//!! should compare influences too !
			{
				if (Points[j] == V.Pos && CompareCompNormals(PointNormals[j], V.Normal[0]))
				{
					PointIndex = j;
					break;
				}
			}
#else
			int PointIndex = -1;	// start with 0, see below
			while (true)
			{
				PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
				if (PointIndex == INDEX_NONE) break;
				if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
				//?? should compare influences too !
			}
#endif
			if (PointIndex == INDEX_NONE)
			{
				// point was not found - create it
				PointIndex = Points.Add();
				Points[PointIndex] = V.Pos;
				PointNormals.AddItem(V.Normal[0]);
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
		if (S.unk1 != Sec) appNotify("Sec=%d unk1=%d", Sec, S.unk1);	//??

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
	if (Package->IsMassEffect && !TrackBoneNames.Num() && Sequences.Num())
	{
		// Mass Effect has separated TrackBoneNames from UAnimSet to UBioAnimSetData
		BioData = Sequences[0]->m_pBioAnimSetData;
		if (BioData)
			CopyArray(TrackBoneNames, BioData->TrackBoneNames);
	}
#endif

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
		if (Package->IsTLR) offsetsPerBone = 6;
#endif
#if XMEN
		if (Package->IsXMen) offsetsPerBone = 6;		// has additional CutInfo array
#endif
		if (Seq->CompressedTrackOffsets.Num() != NumTracks * offsetsPerBone)
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
		Reader.ReverseBytes = Package->ReverseBytes;
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
				CopyArray(A->KeyTime, Seq->RawAnimData[j].KeyTimes);
				for (int kk = 0; kk < A->KeyTime.Num(); kk++)
					A->KeyTime[kk] *= Dst->Rate;
				continue;
			}

			// read animations
			int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
			int TransKeys   = Seq->CompressedTrackOffsets[offsetIndex+1];
			int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+2];
			int RotKeys     = Seq->CompressedTrackOffsets[offsetIndex+3];
#if TLR
			int ScaleOffset = 0, ScaleKeys = 0;
			if (Package->IsTLR)
			{
				ScaleOffset  = Seq->CompressedTrackOffsets[offsetIndex+4];
				ScaleKeys    = Seq->CompressedTrackOffsets[offsetIndex+5];
			}
#endif
//			printf("[%d] :  %d[%d]  %d[%d]  %d[%d]\n", TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys);

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
					appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before TransTrack", Name, *Seq->SequenceName, j, hole);
///					findHoles = false;
				}
#endif
				Reader.Seek(TransOffset);
				for (k = 0; k < TransKeys; k++)
				{
					FVector vec;
					Reader << vec;
					A->KeyPos.AddItem(vec);
				}
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
				appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before RotTrack (KeyFormat=%d)",
					Name, *Seq->SequenceName, j, hole, Seq->KeyEncodingFormat);
///				findHoles = false;
			}
#endif
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
				Reader << Mins << Ranges;

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
					//!! other: ACF_Float32NoW
//FQuat qq = Seq->RawAnimData[j].RotKeys[k];
//KeyQuat.AddItem(qq);
//static int execed = 0;
//if (++execed < 10) {
//printf("q: %X : (%d %d %d)  (%g %g %g %g)\n", GET_DWORD(*q), q->X-1023, q->Y-1023, q->Z-511, FQUAT_ARG(qq));
//}
					else
						appError("Unknown compression method: %d", Seq->RotationCompressionFormat);
				}
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
#endif
#if DEBUG_DECOMPRESS
//			printf("[%s : %s] Frames=%d KeyPos.Num=%d KeyQuat.Num=%d KeyFmt=%s\n", *Seq->SequenceName, *TrackBoneNames[j],
//				Seq->NumFrames, A->KeyPos.Num(), A->KeyQuat.Num(), *Seq->KeyEncodingFormat);
			printf("    [%d]: %d - %d + %d - %d (%d/%d)\n", j,
				TransOffset, TransEnd, RotOffset, Reader.Tell(), TransKeys, RotKeys);
#endif
		}
	}

	unguard;
}


#endif // UNREAL3
