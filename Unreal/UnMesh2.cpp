#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh2.h"

#include "UnMaterial2.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "TypeConvert.h"

//#define DEBUG_SKELMESH		1
//#define DEBUG_STATICMESH		1


/*-----------------------------------------------------------------------------
	ULodMesh class
-----------------------------------------------------------------------------*/

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

#if VANGUARD

struct FVanguardSkin
{
	TArray<UMaterial*> Textures;
	FName		Name;

	friend FArchive& operator<<(FArchive &Ar, FVanguardSkin &S)
	{
		return Ar << S.Textures << S.Name;
	}
};

#endif // VANGUARD

void ULodMesh::Serialize(FArchive &Ar)
{
	guard(ULodMesh::Serialize);
	Super::Serialize(Ar);

	Ar << Version << VertexCount << Verts;

	if (Version <= 1 || Ar.Game == GAME_SplinterCell)
	{
		// skip FMeshTri2 section
		TArray<FMeshTri2> tmp;
		Ar << tmp;
	}

#if VANGUARD
	if (Ar.Game == GAME_Vanguard && Ar.ArLicenseeVer >= 9)
	{
		TArray<FVanguardSkin> Skins;
		int unk74;
		Ar << Skins << unk74;
		if (Skins.Num())
			CopyArray(Textures, Skins[0].Textures);
		goto after_textures;
	}
#endif // VANGUARD
	Ar << Textures;
after_textures:

#if DEBUG_SKELMESH
	for (int i = 0; i < Textures.Num(); i++) appPrintf("Tex[%d] = %s\n", i, Textures[i] ? Textures[i]->Name : "None");
#endif

#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell && Version >= 3)
	{
		TArray<UObject*> unk80;
		Ar << unk80;
	}
#endif // SPLINTER_CELL
#if LOCO
	if (Ar.Game == GAME_Loco)
	{
		int               unk7C;
		TArray<FName>     unk80;
		TArray<FLocoUnk2> unk8C;
		TArray<FLocoUnk1> unk98;
		if (Version >= 5) Ar << unk98;
		if (Version >= 6) Ar << unk80;
		if (Version >= 7) Ar << unk8C << unk7C;
	}
#endif // LOCO
	Ar << MeshScale << MeshOrigin << RotOrigin;

#if DEBUG_SKELMESH
	appPrintf("Scale: %g %g %g\nOrigin: %g %g %g\nRotation: %d %d %d\n", FVECTOR_ARG(MeshScale), FVECTOR_ARG(MeshOrigin), FROTATOR_ARG(RotOrigin));
#endif

	if (Version <= 1 || Ar.Game == GAME_SplinterCell)
	{
		// skip 2nd obsolete section
		TArray<uint16> tmp;
		Ar << tmp;
	}

	Ar << FaceLevel << Faces << CollapseWedgeThus << Wedges << Materials;
	Ar << MeshScaleMax;

#if EOS
	if (Ar.Game == GAME_EOS && Ar.ArLicenseeVer >= 42) goto lod_fields2;
#endif
	Ar << LODHysteresis;
lod_fields2:
	Ar << LODStrength << LODMinVerts << LODMorph << LODZDisplace;

#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell) return;
#endif

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
	if (Ar.Game == GAME_Lineage2 && Version >= 5)
	{
		int unk;
		Ar << unk;
	}
#endif // LINEAGE2
#if BATTLE_TERR
	if (Ar.Game == GAME_BattleTerr && Version >= 5)
	{
		TArray<int> unk;
		Ar << unk;
	}
#endif // BATTLE_TERR
#if SWRC
	if (Ar.Game == GAME_RepCommando && Version >= 7)
	{
		int unkD4, unkD8, unkDC, unkE0;
		Ar << unkD4 << unkD8 << unkDC << unkE0;
	}
#endif // SWRC

	unguard;
}


/*-----------------------------------------------------------------------------
	UVertMesh class
-----------------------------------------------------------------------------*/

void UVertMesh::BuildNormals()
{
	// UE1 meshes have no stored normals, should build them
	// This function is similar to BuildNormals() from SkelMeshInstance.cpp
	int numVerts = Verts.Num();
	int i;
	Normals.Empty(numVerts);
	Normals.AddZeroed(numVerts);
	TArray<CVec3> tmpVerts, tmpNormals;
	tmpVerts.AddZeroed(numVerts);
	tmpNormals.AddZeroed(numVerts);
	// convert verts
	for (i = 0; i < numVerts; i++)
	{
		const FMeshVert &SV = Verts[i];
		CVec3           &DV = tmpVerts[i];
		DV[0] = SV.X * MeshScale.X;
		DV[1] = SV.Y * MeshScale.Y;
		DV[2] = SV.Z * MeshScale.Z;
	}
	// iterate faces
	for (i = 0; i < Faces.Num(); i++)
	{
		const FMeshFace &F = Faces[i];
		// get vertex indices
		int i1 = Wedges[F.iWedge[0]].iVertex;
		int i2 = Wedges[F.iWedge[2]].iVertex;		// note: reverse order in comparison with SkeletalMesh
		int i3 = Wedges[F.iWedge[1]].iVertex;
		// iterate all frames
		for (int j = 0; j < FrameCount; j++)
		{
			int base = VertexCount * j;
			// compute edges
			const CVec3 &V1 = tmpVerts[base + i1];
			const CVec3 &V2 = tmpVerts[base + i2];
			const CVec3 &V3 = tmpVerts[base + i3];
			CVec3 D1, D2, D3;
			VectorSubtract(V2, V1, D1);
			VectorSubtract(V3, V2, D2);
			VectorSubtract(V1, V3, D3);
			// compute normal
			CVec3 norm;
			cross(D2, D1, norm);
			norm.Normalize();
			// compute angles
			D1.Normalize();
			D2.Normalize();
			D3.Normalize();
			float angle1 = acos(-dot(D1, D3));
			float angle2 = acos(-dot(D1, D2));
			float angle3 = acos(-dot(D2, D3));
			// add normals for triangle verts
			VectorMA(tmpNormals[base + i1], angle1, norm);
			VectorMA(tmpNormals[base + i2], angle2, norm);
			VectorMA(tmpNormals[base + i3], angle3, norm);
		}
	}
	// normalize and convert computed normals
	for (i = 0; i < numVerts; i++)
	{
		CVec3 &SN     = tmpNormals[i];
		FMeshNorm &DN = Normals[i];
		SN.Normalize();
		DN.X = appRound(SN[0] * 511 + 512);
		DN.Y = appRound(SN[1] * 511 + 512);
		DN.Z = appRound(SN[2] * 511 + 512);
	}
}


void UVertMesh::Serialize(FArchive &Ar)
{
	guard(UVertMesh::Serialize);

#if UNREAL1
	if (Ar.Engine() == GAME_UE1)
	{
		SerializeVertMesh1(Ar);
		RotOrigin.Roll = -RotOrigin.Roll;	//??
		return;
	}
#endif // UNREAL1

	Super::Serialize(Ar);
	RotOrigin.Roll = -RotOrigin.Roll;		//??

	Ar << AnimMeshVerts << StreamVersion; // FAnimMeshVertexStream: may skip this (simply seek archive)
	Ar << Verts2 << f150;
	Ar << AnimSeqs << Normals;
	Ar << VertexCount << FrameCount;
	Ar << BoundingBoxes << BoundingSpheres;

	unguard;
}


/*-----------------------------------------------------------------------------
	USkeletalMesh class
-----------------------------------------------------------------------------*/

// Implement constructor in cpp to avoid inlining (it's large enough).
// It's useful to declare TArray<> structures as forward declarations in header file.
USkeletalMesh::USkeletalMesh()
{}


USkeletalMesh::~USkeletalMesh()
{
	delete ConvertedMesh;
}


#if SWRC

struct FAttachSocketSWRC
{
	FName		Alias;
	FName		BoneName;
	FMatrix		Matrix;

	friend FArchive& operator<<(FArchive &Ar, FAttachSocketSWRC &S)
	{
		return Ar << S.Alias << S.BoneName << S.Matrix;
	}
};

struct FMeshAnimLinkSWRC
{
	int			Flags;
	UMeshAnimation *Anim;

	friend FArchive& operator<<(FArchive &Ar, FMeshAnimLinkSWRC &S)
	{
		if (Ar.ArVer >= 151)
			Ar << S.Flags;
		else
			S.Flags = 1;
		return Ar << S.Anim;
	}
};

#endif // SWRC


void USkeletalMesh::Serialize(FArchive &Ar)
{
	guard(USkeletalMesh::Serialize);

	assert(Ar.Game < GAME_UE3);

#if UNREAL1
	if (Ar.Engine() == GAME_UE1)
	{
		SerializeSkelMesh1(Ar);
		return;
	}
#endif
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		SerializeBioshockMesh(Ar);
		return;
	}
#endif

	Super::Serialize(Ar);
#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell)
	{
		SerializeSCell(Ar);
		return;
	}
#endif // SPLINTER_CELL
#if TRIBES3
	TRIBES_HDR(Ar, 4);
#endif
	Ar << Points2;
#if BATTLE_TERR
	if (Ar.Game == GAME_BattleTerr && Ar.ArVer >= 134)
	{
		TArray<FVector> Points3;
		Ar << Points3;
	}
#endif // BATTLE_TERR

	Ar << RefSkeleton;
#if DEBUG_SKELMESH
	appPrintf("RefSkeleton: %d bones\n", RefSkeleton.Num());
	for (int i1 = 0; i1 < RefSkeleton.Num(); i1++)
		appPrintf("  [%d] n=%s p=%d\n", i1, *RefSkeleton[i1].Name, RefSkeleton[i1].ParentIndex);
#endif // DEBUG_SKELMESH

#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 142)
	{
		for (int i = 0; i < RefSkeleton.Num(); i++)
		{
			FMeshBone &B = RefSkeleton[i];
			B.BonePos.Orientation.X *= -1;
			B.BonePos.Orientation.Y *= -1;
			B.BonePos.Orientation.Z *= -1;
		}
	}
	if (Ar.Game == GAME_RepCommando && Version >= 5)
	{
		TArray<FMeshAnimLinkSWRC> Anims;
		Ar << Anims;
		if (Anims.Num() >= 1) Animation = Anims[0].Anim;
	}
	else
#endif // SWRC
		Ar << Animation;
#if AA2
	if (Ar.Game == GAME_AA2 && Ar.ArLicenseeVer >= 22)
	{
		TArray<UObject*> unk230;
		Ar << unk230;
	}
#endif // AA2
	Ar << SkeletalDepth << WeightIndices << BoneInfluences;
#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 140)
	{
		TArray<FAttachSocketSWRC> Sockets;
		Ar << Sockets;	//?? convert
	}
	else
#endif // SWRC
	{
		Ar << AttachAliases << AttachBoneNames << AttachCoords;
	}
	if (Version <= 1)
	{
//		appNotify("SkeletalMesh of version %d\n", Version);
		TArray<FLODMeshSection> tmp1, tmp2;
		TArray<uint16> tmp3;
		Ar << tmp1 << tmp2 << tmp3;
		// copy and convert data from old mesh format
		UpgradeMesh();
	}
	else
	{
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 136)
		{
			int f338;
			Ar << f338;
		}
#endif // UC2
#if SWRC
		if (Ar.Game == GAME_RepCommando)
		{
			int f1C4;
			if (Version >= 6) Ar << f1C4;
			Ar << LODModels;
			if (Version < 5) Ar << f224;
			Ar << Points << Wedges << Triangles << VertInfluences;
			Ar << CollapseWedge << f1C8;
			goto skip_remaining;
		}
#endif // SWRC
#if EOS
		if (Ar.Game == GAME_EOS)
		{
			int unk1;
			UObject* unk2;
			UObject* unk3;
			if (Version >= 6) Ar << unk1 << unk2;
			if (Version >= 7) Ar << unk3;
			Ar << LODModels;
			goto skip_remaining;
		}
#endif // EOS
#if 0
		// Shui Hu Q Zhuan 2 Online
		if (Ar.ArVer == 126 && Ar.ArLicenseeVer == 1)
		{
			// skip LOD models
			int Num;
			Ar << AR_INDEX(Num);
			for (int i = 0; i < Num; i++)
			{
				int Pos;
				Ar << Pos;
				Ar.Seek(Ar.Tell() + Pos - 4);
			}
			goto after_lods;
		}
#endif
		Ar << LODModels;
	after_lods:
		Ar << f224 << Points;
#if BATTLE_TERR
		if (Ar.Game == GAME_BattleTerr && Ar.ArVer >= 134)
		{
			TLazyArray<int>	unk15C;
			Ar << unk15C;
		}
#endif // BATTLE_TERR
		Ar << Wedges << Triangles << VertInfluences;
		Ar << CollapseWedge << f1C8;
	}

#if TRIBES3
	if ((Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4) && t3_hdrSV >= 3)
	{
	#if 0
		// it looks like format of following data was chenged sinse
		// data was prepared, and game executeble does not load these
		// LazyArrays (otherwise error should occur) -- so we are
		// simply skipping these arrays
		TLazyArray<FT3Unk1>    unk1;
		TLazyArray<FMeshWedge> unk2;
		TLazyArray<uint16>     unk3;
		Ar << unk1 << unk2 << unk3;
	#else
		SkipLazyArray(Ar);
		SkipLazyArray(Ar);
		SkipLazyArray(Ar);
	#endif
		// nothing interesting below ...
		goto skip_remaining;
	}
#endif // TRIBES3
#if BATTLE_TERR
	if (Ar.Game == GAME_BattleTerr) goto skip_remaining;
#endif
#if UC2
	if (Ar.Engine() == GAME_UE2X) goto skip_remaining;
#endif

#if LINEAGE2
	if (Ar.Game == GAME_Lineage2)
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
		ConvertMesh();
		return;
	}
#endif // LINEAGE2

	if (Ar.ArVer >= 120)
	{
		Ar << AuthKey;
	}

#if LOCO
	if (Ar.Game == GAME_Loco) goto skip_remaining;	// Loco codepath is similar to UT2004, but sometimes has different version switches
#endif

#if UT2
	if (Ar.Game == GAME_UT2)
	{
		// UT2004 has branched version of UE2, which is slightly different
		// in comparison with generic UE2, which is used in all other UE2 games.
		if (Ar.ArVer >= 122)
			Ar << KarmaProps << BoundingSpheres << BoundingBoxes << f32C;
		if (Ar.ArVer >= 127)
			Ar << CollisionMesh;
		ConvertMesh();
		return;
	}
#endif // UT2

	// generic UE2 code
	if (Ar.ArVer >= 124)
		Ar << KarmaProps << BoundingSpheres << BoundingBoxes;
	if (Ar.ArVer >= 125)
		Ar << f32C;

#if XIII
	if (Ar.Game == GAME_XIII) goto skip_remaining;
#endif
#if RAGNAROK2
	if (Ar.Game == GAME_Ragnarok2 && Ar.ArVer >= 131)
	{
		float unk1, unk2;
		Ar << unk1 << unk2;
	}
#endif // RAGNAROK2

	if (Ar.ArLicenseeVer && (Ar.Tell() != Ar.GetStopper()))
	{
		appPrintf("Serializing SkeletalMesh'%s' of unknown game: %d unreal bytes\n", Name, Ar.GetStopper() - Ar.Tell());
	skip_remaining:
		DROP_REMAINING_DATA(Ar);
	}

	ConvertMesh();

	unguard;
}


void USkeletalMesh::PostLoad()
{
#if BIOSHOCK
	if (GetGame() == GAME_Bioshock)
		PostLoadBioshockMesh();		// should be called after loading of all used objects
#endif // BIOSHOCK
}


void USkeletalMesh::UpgradeFaces()
{
	guard(UpgradeFaces);
	// convert 'FMeshFace Faces' to 'VTriangle Triangles'
	if (Faces.Num() && !Triangles.Num())
	{
		Triangles.Empty(Faces.Num());
		Triangles.AddUninitialized(Faces.Num());
		for (int i = 0; i < Faces.Num(); i++)
		{
			const FMeshFace &F = Faces[i];
			VTriangle &T = Triangles[i];
			T.WedgeIndex[0] = F.iWedge[0];
			T.WedgeIndex[1] = F.iWedge[1];
			T.WedgeIndex[2] = F.iWedge[2];
			T.MatIndex      = F.MaterialIndex;
		}
	}
	unguard;
}

void USkeletalMesh::UpgradeMesh()
{
	guard(USkeletalMesh.UpgradeMesh);

	int i;
	CopyArray(Points, Points2);
	CopyArray(Wedges, Super::Wedges);
	UpgradeFaces();
	// convert VBoneInfluence and VWeightIndex to FVertInfluence
	// count total influences
	int numInfluences = 0;
	for (i = 0; i < WeightIndices.Num(); i++)
		numInfluences += WeightIndices[i].BoneInfIndices.Num() * (i + 1);
	VertInfluences.Empty(numInfluences);
	VertInfluences.AddZeroed(numInfluences);
	int vIndex = 0;
	for (i = 0; i < WeightIndices.Num(); i++)				// loop by influence count per vertex
	{
		const VWeightIndex &WI = WeightIndices[i];
		int index = WI.StartBoneInf;
		for (int j = 0; j < WI.BoneInfIndices.Num(); j++)	// loop by vertices
		{
			int iVertex = WI.BoneInfIndices[j];
			for (int k = 0; k <= i; k++)					// enumerate all bones per vertex
			{
				const VBoneInfluence &BI = BoneInfluences[index++];
				FVertInfluence &I = VertInfluences[vIndex++];
				I.Weight     = BI.BoneWeight / 65535.0f;
				I.BoneIndex  = BI.BoneIndex;
				I.PointIndex = iVertex;
			}
		}
	}

	unguard;
}


void USkeletalMesh::ConvertMesh()
{
	guard(USkeletalMesh::ConvertMesh);

	CSkeletalMesh *Mesh = new CSkeletalMesh(this);
	ConvertedMesh = Mesh;
	Mesh->BoundingBox    = BoundingBox;
	Mesh->BoundingSphere = BoundingSphere;

	Mesh->RotOrigin  = RotOrigin;
	Mesh->MeshScale  = CVT(MeshScale);
	Mesh->MeshOrigin = CVT(MeshOrigin);

	Mesh->Lods.Empty(LODModels.Num());

#if DEBUG_SKELMESH
	appPrintf("  Base : Points[%d] Wedges[%d] Influences[%d] Faces[%d]\n",
		Points.Num(), Wedges.Num(), VertInfluences.Num(), Triangles.Num()
	);
#endif

	// some games has troubles with LOD models ...
#if TRIBES3
	if (GetGame() == GAME_Tribes3) goto base_mesh;
#endif
#if SWRC
	if (GetGame() == GAME_RepCommando) goto base_mesh;
#endif

	if (!LODModels.Num())
	{
	base_mesh:
		guard(ConvertBaseMesh);

		// create CSkelMeshLod from base mesh
		CSkelMeshLod *Lod = new (Mesh->Lods) CSkelMeshLod;
		Lod->NumTexCoords = 1;
		Lod->HasNormals   = false;
		Lod->HasTangents  = false;

		if (Points.Num() && Wedges.Num() && VertInfluences.Num())
		{
			InitSections(*Lod);
			ConvertWedges(*Lod, Points, Wedges, VertInfluences);
			BuildIndices(*Lod);
		}
		else
		{
			appPrintf("ERROR: bad base mesh\n");
		}
		goto skeleton;

		unguard;
	}

	// convert LODs
	for (int lod = 0; lod < LODModels.Num(); lod++)
	{
		guard(ConvertLod);

		const FStaticLODModel &SrcLod = LODModels[lod];

#if DEBUG_SKELMESH
		appPrintf("  Lod %d: Points[%d] Wedges[%d] Influences[%d] Faces[%d]  Rigid(Sec[%d] Indices[%d] Verts[%d])  Soft(Sec[%d] Indices[%d] Verts[%d] Stream[%d])\n",
			lod, SrcLod.Points.Num(), SrcLod.Wedges.Num(), SrcLod.VertInfluences.Num(), SrcLod.Faces.Num(),
			SrcLod.RigidSections.Num(), SrcLod.RigidIndices.Indices.Num(), SrcLod.VertexStream.Verts.Num(),
			SrcLod.SoftSections.Num(), SrcLod.SoftIndices.Indices.Num(), SrcLod.SkinPoints.Num(), SrcLod.SkinningData.Num()
		);
#endif
//		if (SrcLod.Faces.Num() == 0 && SrcLod.SoftSections.Num() > 0)
//			continue;

		CSkelMeshLod *Lod = new (Mesh->Lods) CSkelMeshLod;
		Lod->NumTexCoords = 1;
		Lod->HasNormals   = false;
		Lod->HasTangents  = false;

		if (IsCorrectLOD(SrcLod))
		{
			InitSections(*Lod);
			ConvertWedges(*Lod, SrcLod.Points, SrcLod.Wedges, SrcLod.VertInfluences);
			BuildIndicesForLod(*Lod, SrcLod);
		}
		else
		{
			appPrintf("WARNING: bad LOD#%d mesh, switching to base\n", lod);
			if (lod == 0)
			{
				Mesh->Lods.Empty();
				goto base_mesh;
			}
			else
			{
				Mesh->Lods.RemoveAt(lod);
				break;
			}
		}

		unguard;
	}

skeleton:
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
	}
	unguard; // ProcessSkeleton

	// copy sockets
	int NumSockets = AttachAliases.Num();
	Mesh->Sockets.Empty(NumSockets);
	for (int i = 0; i < NumSockets; i++)
	{
		CSkelMeshSocket *DS = new (Mesh->Sockets) CSkelMeshSocket;
		DS->Name      = AttachAliases[i];
		DS->Bone      = AttachBoneNames[i];
		DS->Transform = CVT(AttachCoords[i]);
	}

	Mesh->FinalizeMesh();

	unguard;
}


void USkeletalMesh::InitSections(CSkelMeshLod &Lod)
{
	// allocate sections and set CMeshSection.Material
	Lod.Sections.AddZeroed(Materials.Num());
	for (int sec = 0; sec < Materials.Num(); sec++)
	{
		const FMeshMaterial &M = Materials[sec];
		CMeshSection &Sec = Lod.Sections[sec];
		int TexIndex  = M.TextureIndex;
		UUnrealMaterial *Mat = (TexIndex < Textures.Num()) ? Textures[TexIndex] : NULL;
#if RENDERING
		Sec.Material = UMaterialWithPolyFlags::Create(Mat, M.PolyFlags);
#endif
	}
}

void USkeletalMesh::ConvertWedges(CSkelMeshLod &Lod, const TArray<FVector> &MeshPoints, const TArray<FMeshWedge> &MeshWedges, const TArray<FVertInfluence> &VertInfluences)
{
	guard(USkeletalMesh::ConvertWedges);

	struct CVertInfo
	{
		int		NumInfs;		// may be higher than NUM_INFLUENCES
		int		Bone[NUM_INFLUENCES];
		float	Weight[NUM_INFLUENCES];
	};

	int i, j;

	CVertInfo *Verts = new CVertInfo[MeshPoints.Num()];
	memset(Verts, 0, MeshPoints.Num() * sizeof(CVertInfo));

	// collect influences per vertex
	for (i = 0; i < VertInfluences.Num(); i++)
	{
		const FVertInfluence &Inf = VertInfluences[i];
		CVertInfo &V = Verts[Inf.PointIndex];
		int NumInfs = V.NumInfs++;
		int idx = NumInfs;
		if (NumInfs >= NUM_INFLUENCES)
		{
			// overflow
			// find smallest weight smaller than current
			float w = Inf.Weight;
			idx = -1;
			for (j = 0; j < NUM_INFLUENCES; j++)
			{
				if (V.Weight[j] < w)
				{
					w = V.Weight[j];
					idx = j;
					// continue - may be other weight will be even smaller
				}
			}
			if (idx < 0) continue;	// this weight is smaller than other
		}
		// add influence
		V.Bone[idx]   = Inf.BoneIndex;
		V.Weight[idx] = Inf.Weight;
	}

	// normalize influences
	for (i = 0; i < MeshPoints.Num(); i++)
	{
		CVertInfo &V = Verts[i];
		if (V.NumInfs == 0)
		{
			appPrintf("WARNING: Vertex %d has 0 influences\n", i);
			V.NumInfs = 1;
			V.Bone[0] = 0;
			V.Weight[0] = 1.0f;
			continue;
		}
		if (V.NumInfs <= NUM_INFLUENCES) continue;	// no normalization is required
		float s = 0;
		for (j = 0; j < NUM_INFLUENCES; j++)		// count sum
			s += V.Weight[j];
		s = 1.0f / s;
		for (j = 0; j < NUM_INFLUENCES; j++)		// adjust weights
			V.Weight[j] *= s;
	}

	// create vertices
	Lod.AllocateVerts(MeshWedges.Num());
	for (i = 0; i < MeshWedges.Num(); i++)
	{
		const FMeshWedge &SW = MeshWedges[i];
		CSkelMeshVertex  &DW = Lod.Verts[i];
		DW.Position = CVT(MeshPoints[SW.iVertex]);
		DW.UV = CVT(SW.TexUV);
		// DW.Normal and DW.Tangent are unset
		// setup Bone[] and Weight[]
		const CVertInfo &V = Verts[SW.iVertex];
		unsigned PackedWeights = 0;
		for (j = 0; j < V.NumInfs; j++)
		{
			DW.Bone[j]   = V.Bone[j];
			PackedWeights |= appRound(V.Weight[j] * 255) << (j * 8);
		}
		DW.PackedWeights = PackedWeights;
		for (/* continue */; j < NUM_INFLUENCES; j++)	// place end marker and zero weight
		{
			DW.Bone[j] = -1;
		}
	}

	delete Verts;

	unguard;
}


void USkeletalMesh::BuildIndices(CSkelMeshLod &Lod)
{
	guard(USkeletalMesh::BuildIndices);

	int i;
	int NumSections = Lod.Sections.Num();

	// 1st pass: count Lod.Sections[i].NumFaces
	// 2nd pass: set Lod.Sections[i].FirstIndex, allocate and fill indices array
	for (int pass = 0; pass < 2; pass++)
	{
		int NumIndices = 0;
		for (i = 0; i < NumSections; i++)
		{
			CMeshSection &Sec = Lod.Sections[i];
			if (pass == 1)
			{
				int SecIndices = Sec.NumFaces * 3;
				Sec.FirstIndex = NumIndices;
				NumIndices += SecIndices;
			}
			Sec.NumFaces = 0;
		}

		// allocate index buffer
		if (pass == 1)
		{
			Lod.Indices.Indices16.AddZeroed(NumIndices);
		}

		for (i = 0; i < Triangles.Num(); i++)
		{
			const VTriangle &Face = Triangles[i];
			int MatIndex = Face.MatIndex;
			// if section does not exist - add it
			if (MatIndex >= NumSections)
			{
				Lod.Sections.AddZeroed(MatIndex - NumSections + 1);
				NumSections = MatIndex + 1;
			}
			CMeshSection &Sec = Lod.Sections[MatIndex];

			if (pass == 1)	// on 1st pass count NumFaces, on 2nd pass fill indices
			{
				uint16 *idx = &Lod.Indices.Indices16[Sec.FirstIndex + Sec.NumFaces * 3];
				for (int j = 0; j < 3; j++)
					*idx++ = Face.WedgeIndex[j];
			}

			Sec.NumFaces++;
		}
	}

	unguard;
}


/* TODO:
 * The most correct way of converting FStaticLODModel to CSkelMeshLod is using soft and rigid sections.
 *   RigidSection: RigidIndices -> VertexStream (contains position and UV)
 *   SoftSection: SoftIndices -> SkinningData (encoded: contains index in SkinPoints plus UV and influences)
 * NOTE: there's no common vertex stream here, these vertex and index buffers are entirely separate.
 *
 * Current implementation is not correct, and it is failed when mesh has both rigid and soft sections (or
 * when TLazyArray part of FStaticLODModel erased). Should be fixed ConvertWedges (separate function for LOD
 * model) and BuildIndicesForLod. ConvertWedges should process SkinningData, BuildIndicesForLod should
 * use index buffers for all sections (i.e. do not use Faces for soft sections).
 * Possible reason of this: it seems when SkinningData generated by the engine, it reorders vertices so
 * soft vertices goes before rigid.
 */
void USkeletalMesh::BuildIndicesForLod(CSkelMeshLod &Lod, const FStaticLODModel &SrcLod)
{
	guard(USkeletalMesh::BuildIndicesForLod);

	int i;
	int NumSections = Lod.Sections.Num();

	// 1st pass: count Lod.Sections[i].NumFaces
	// 2nd pass: set Lod.Sections[i].FirstIndex, allocate and fill indices array
	for (int pass = 0; pass < 2; pass++)
	{
		int NumIndices = 0;
		for (i = 0; i < NumSections; i++)
		{
			CMeshSection &Sec = Lod.Sections[i];
			if (pass == 1)
			{
				int SecIndices = Sec.NumFaces * 3;
				Sec.FirstIndex = NumIndices;
				NumIndices += SecIndices;
			}
			Sec.NumFaces = 0;
		}

		// allocate index buffer
		if (pass == 1)
		{
			Lod.Indices.Indices16.AddZeroed(NumIndices);
		}

		int s;

		// soft sections (influence count >= 2)
		for (s = 0; s < SrcLod.SoftSections.Num(); s++)
		{
			const FSkelMeshSection &ms = SrcLod.SoftSections[s];
			int MatIndex = ms.MaterialIndex;
			// if section does not exist - add it
			if (MatIndex >= NumSections)
			{
				Lod.Sections.AddZeroed(MatIndex - NumSections + 1);
				NumSections = MatIndex + 1;
			}
			CMeshSection &Sec = Lod.Sections[MatIndex];

			if (pass == 1)
			{
				uint16 *idx = &Lod.Indices.Indices16[Sec.FirstIndex + Sec.NumFaces * 3];
//				printf("sidx[%d]: %d + %d (nf=%d) -> %d\n", MatIndex, Sec.FirstIndex + Sec.NumFaces * 3, ms.FirstFace * 3, ms.NumFaces, Sec.FirstIndex + Sec.NumFaces * 3 + ms.NumFaces * 3);
				for (i = 0; i < ms.NumFaces; i++)
				{
					const FMeshFace &F = SrcLod.Faces[ms.FirstFace + i];
					//?? ignore F.MaterialIndex - may be any
//					assert(F.MaterialIndex == ms.MaterialIndex);
					for (int j = 0; j < 3; j++)
						*idx++ = F.iWedge[j];
				}
			}

			Sec.NumFaces += ms.NumFaces;
		}
		// rigid sections (influence count == 1)
		// code is similar to the block above
		for (s = 0; s < SrcLod.RigidSections.Num(); s++)
		{
			const FSkelMeshSection &ms = SrcLod.RigidSections[s];
			int MatIndex = ms.MaterialIndex;
			// if section does not exist - add it
			if (MatIndex >= NumSections)
			{
				Lod.Sections.AddZeroed(MatIndex - NumSections + 1);
				NumSections = MatIndex + 1;
			}
			CMeshSection &Sec  = Lod.Sections[MatIndex];

			if (pass == 1)
			{
				uint16 *idx = &Lod.Indices.Indices16[Sec.FirstIndex + Sec.NumFaces * 3];
				uint16 firstIndex = ms.FirstFace * 3;
//				printf("ridx[%d]: %d + %d * 3 (nf=%d) -> %d\n", MatIndex, Sec.FirstIndex + Sec.NumFaces * 3, ms.FirstFace * 3, ms.NumFaces, Sec.FirstIndex + Sec.NumFaces * 3 + ms.NumFaces * 3);
				for (i = 0; i < ms.NumFaces * 3; i++)
					*idx++ = SrcLod.RigidIndices.Indices[firstIndex + i];
			}

			Sec.NumFaces += ms.NumFaces;
		}
	}

	unguard;
}


bool USkeletalMesh::IsCorrectLOD(const FStaticLODModel &Lod) const
{
	if (!Lod.Points.Num() || !Lod.Wedges.Num() || !Lod.VertInfluences.Num())
		return false;
	if (!(Lod.RigidIndices.Indices.Num() + Lod.SoftIndices.Indices.Num()) && !Lod.Faces.Num())
		return false;

	if ((Lod.RigidSections.Num() != 0) && (Lod.SoftSections.Num() != 0))
		return false; //!! we can't reliably support this kind of mesh - see note before BuildIndicesForLod()

	//?? really any mesh with rigid sections should be considered as "bad" (we can't work with such LODs)

	int i;
	int NumPoints = Lod.Points.Num();
	int NumWedges = Lod.Wedges.Num();

	// verify influences
	for (i = 0; i < Lod.VertInfluences.Num(); i++)
	{
		int PointIndex = Lod.VertInfluences[i].PointIndex;
		if (PointIndex < 0 || PointIndex >= NumPoints) return false;
	}

	// verify indices (only RigidIndices, SoftIndices aren't used)
	for (i = 0; i < Lod.RigidIndices.Indices.Num(); i++)
	{
		int WedgeIndex = Lod.RigidIndices.Indices[i];
		if (WedgeIndex < 0 || WedgeIndex >= NumWedges) return false;
	}

	int TotalFaces = 0;

	// soft sections (influence count >= 2)
	int s;
	for (s = 0; s < Lod.SoftSections.Num(); s++)
	{
		const FSkelMeshSection &ms = Lod.SoftSections[s];
		TotalFaces += ms.NumFaces;
//		int MatIndex = ms.MaterialIndex;
//		if (MatIndex < 0 || MatIndex >= ... //??
	}
	// rigid sections (influence count == 1)
	for (s = 0; s < Lod.RigidSections.Num(); s++)
	{
		const FSkelMeshSection &ms = Lod.RigidSections[s];
		TotalFaces += ms.NumFaces;
//		int MatIndex = ms.MaterialIndex; ... //??
	}

	if (!TotalFaces) return false;

	return true;
}


#if SPLINTER_CELL

struct FSCellUnk1
{
	int				f0, f4, f8, fC;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk1 &S)
	{
		return Ar << S.f0 << S.f4 << S.f8 << S.fC;
	}
};

SIMPLE_TYPE(FSCellUnk1, int)

struct FSCellUnk2
{
	int				f0, f4, f8, fC, f10;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk2 &S)
	{
		return Ar << S.f0 << S.f4 << S.f10 << S.f8 << S.fC;
	}
};

struct FSCellUnk3
{
	int				f0, f4, f8, fC;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk3 &S)
	{
		return Ar << S.f0 << S.fC << S.f4 << S.f8;
	}
};

struct FSCellUnk4a
{
	FVector			f0;
	FVector			fC;
	int				f18;					// float?

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk4a &S)
	{
		return Ar << S.f0 << S.fC << S.f18;
	}
};

SIMPLE_TYPE(FSCellUnk4a, float)

struct FSCellUnk4
{
	int				Size;
	int				Count;
	TArray<FString>	BoneNames;				// BoneNames.Num() == Count
	FString			f14;
	FSCellUnk4a**	Data;					// (FSCellUnk4a*)[Count][Size]

	FSCellUnk4()
	:	Data(NULL)
	{}
	~FSCellUnk4()
	{
		// cleanup data
		if (Data)
		{
			for (int i = 0; i < Count; i++)
				delete Data[i];
			delete Data;
		}
	}

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk4 &S)
	{
		int i, j;

		// serialize scalars
		Ar << S.Size << S.Count << S.BoneNames << S.f14;

		if (Ar.IsLoading)
		{
			// allocate array of pointers
			S.Data = new FSCellUnk4a* [S.Count];
			// allocate arrays
			for (i = 0; i < S.Count; i++)
				S.Data[i] = new FSCellUnk4a [S.Size];
		}
		// serialize arrays
		for (i = 0; i < S.Count; i++)
		{
			FSCellUnk4a* Ptr = S.Data[i];
			for (j = 0; j < S.Size; j++, Ptr++)
				Ar << *Ptr;
		}
		return Ar;
	}
};

void USkeletalMesh::SerializeSCell(FArchive &Ar)
{
	if (Version >= 2) Ar << Version;		// interesting code
	Ar << Points2;
	if (Ar.ArLicenseeVer >= 48)
	{
		TArray<FVector> unk1;
		Ar << unk1;
	}
	if (Ar.ArLicenseeVer >= 49 && Ar.ArLicenseeVer < 67)
	{
		TArray<byte> unk2;
		Ar << unk2;
	}
	Ar << RefSkeleton;
	Ar << Animation;
	if (Ar.ArLicenseeVer >= 155)
	{
		UObject* unk218;
		Ar << unk218;
	}
	Ar << SkeletalDepth << WeightIndices << BoneInfluences;
	Ar << AttachAliases << AttachBoneNames << AttachCoords;
	DROP_REMAINING_DATA(Ar);
	UpgradeMesh();

	ConvertMesh();

/*	TArray<FSCellUnk1> tmp1;
	TArray<FSCellUnk2> tmp2;
	TArray<FSCellUnk3> tmp3;
	TArray<FLODMeshSection> tmp4, tmp5;
	TArray<uint16> tmp6;
	FSCellUnk4 complex;
	Ar << tmp1 << tmp2 << tmp3 << tmp4 << tmp5 << tmp6 << complex; */
}

#endif // SPLINTER_CELL


#if LINEAGE2

void FStaticLODModel::RestoreLineageMesh()
{
	guard(FStaticLODModel::RestoreLineageMesh);

	if (Wedges.Num()) return;			// nothing to restore
	appPrintf("Converting Lineage2 LODModel to standard LODModel ...\n");
	if (SoftSections.Num() && RigidSections.Num())
		appNotify("have soft & rigid sections");

	int i, j, k;
	int NumWedges = LineageWedges.Num() + VertexStream.Verts.Num(); // one of them is zero (ensured by assert below)
	if (!NumWedges)
	{
		appNotify("Cannot restore mesh: no wedges");
		return;
	}
	assert(LineageWedges.Num() == 0 || VertexStream.Verts.Num() == 0);

	Wedges.Empty(NumWedges);
	Points.Empty(NumWedges);			// really, should be a smaller count
	VertInfluences.Empty(NumWedges);	// min count = NumVerts
	Faces.Empty((SoftIndices.Indices.Num() + RigidIndices.Indices.Num()) / 3);
	TArray<FVector> PointNormals;
	PointNormals.Empty(NumWedges);

	// remap bones and build faces
	TArray<const FSkelMeshSection*> WedgeSection;
	WedgeSection.Empty(NumWedges);
	WedgeSection.AddZeroed(NumWedges);
	// soft sections
	guard(SoftWedges);
	for (k = 0; k < SoftSections.Num(); k++)
	{
		const FSkelMeshSection &ms = SoftSections[k];
		for (i = 0; i < ms.NumFaces; i++)
		{
			FMeshFace *F = new (Faces) FMeshFace;
			F->MaterialIndex = ms.MaterialIndex;
			for (j = 0; j < 3; j++)
			{
				int WedgeIndex = SoftIndices.Indices[(ms.FirstFace + i) * 3 + j];
				assert(WedgeSection[WedgeIndex] == NULL || WedgeSection[WedgeIndex] == &ms);
				WedgeSection[WedgeIndex] = &ms;
				F->iWedge[j] = WedgeIndex;
			}
		}
	}
	unguard;
	guard(RigidWedges);
	// and the same code for rigid sections
	for (k = 0; k < RigidSections.Num(); k++)
	{
		const FSkelMeshSection &ms = RigidSections[k];
		for (i = 0; i < ms.NumFaces; i++)
		{
			FMeshFace *F = new (Faces) FMeshFace;
			F->MaterialIndex = ms.MaterialIndex;
			for (j = 0; j < 3; j++)
			{
				int WedgeIndex = RigidIndices.Indices[(ms.FirstFace + i) * 3 + j];
				assert(WedgeSection[WedgeIndex] == NULL || WedgeSection[WedgeIndex] == &ms);
				WedgeSection[WedgeIndex] = &ms;
				F->iWedge[j] = WedgeIndex;
			}
		}
	}
	unguard;

	// process wedges

	// convert LineageWedges (soft sections)
	guard(BuildSoftWedges);
	for (i = 0; i < LineageWedges.Num(); i++)
	{
		const FLineageWedge &LW = LineageWedges[i];
		FVector VPos = LW.Point;
		// find the same point in previous items
		int PointIndex = -1;
		while (true)
		{
			PointIndex = Points.FindItem(VPos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (PointNormals[PointIndex] == LW.Normal) break;
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.AddUninitialized();
			Points[PointIndex] = LW.Point;
			PointNormals.Add(LW.Normal);
			// build influences
			const FSkelMeshSection *ms = WedgeSection[i];
			assert(ms);
			for (j = 0; j < 4; j++)
			{
				if (LW.Bones[j] == 255) continue;	// no bone assigned
				float Weight = LW.Weights[j];
				if (Weight < 0.000001f) continue;	// zero weight
				FVertInfluence *Inf = new (VertInfluences) FVertInfluence;
				Inf->Weight     = Weight;
				Inf->BoneIndex  = ms->LineageBoneMap[LW.Bones[j]];
				Inf->PointIndex = PointIndex;
			}
		}
		// create wedge
		FMeshWedge *W = new (Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = LW.Tex;
	}
	unguard;
	// similar code for VertexStream (rigid sections)
	guard(BuildRigidWedges);
	for (i = 0; i < VertexStream.Verts.Num(); i++)
	{
		const FAnimMeshVertex &LW = VertexStream.Verts[i];
		FVector VPos = LW.Pos;
		// find the same point in previous items
		int PointIndex = -1;
		while (true)
		{
			PointIndex = Points.FindItem(VPos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (LW.Norm == PointNormals[PointIndex]) break;
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.AddUninitialized();
			Points[PointIndex] = LW.Pos;
			PointNormals.Add(LW.Norm);
			// build influences
			const FSkelMeshSection *ms = WedgeSection[i];
			assert(ms);
			FVertInfluence *Inf = new (VertInfluences) FVertInfluence;
			Inf->Weight     = 1.0f;
			Inf->BoneIndex  = /*VertexStream.Revision; //??*/ ms->BoneIndex; //-- equals 0 in Lineage2 ...
			Inf->PointIndex = PointIndex;
		}
		// create wedge
		FMeshWedge *W = new (Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = LW.Tex;
	}
	unguard;

	unguard;
}


#endif // LINEAGE2


/*-----------------------------------------------------------------------------
	UStaticMesh class
-----------------------------------------------------------------------------*/

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
	int16					unk4;
	int16					unk5;

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
	int16					v[4];

	friend FArchive& operator<<(FArchive &Ar, FkDOPCollisionTriangle &T)
	{
		return Ar << T.v[0] << T.v[1] << T.v[2] << T.v[3];
	}
};

SIMPLE_TYPE(FkDOPCollisionTriangle, int16)

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


// Implement constructor in cpp to avoid inlining (it's large enough).
// It's useful to declare TArray<> structures as forward declarations in header file.
UStaticMesh::UStaticMesh()
{}

UStaticMesh::~UStaticMesh()
{
	delete ConvertedMesh;
}

void UStaticMesh::Serialize(FArchive &Ar)
{
	guard(UStaticMesh::Serialize);

	assert(Ar.Game < GAME_UE3);

#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		SerializeBioshockMesh(Ar);
		return;
	}
#endif // BIOSHOCK

#if VANGUARD
	if (Ar.Game == GAME_Vanguard && Ar.ArVer >= 128 && Ar.ArLicenseeVer >= 25)
	{
		SerializeVanguardMesh(Ar);
		return;
	}
#endif // VANGUARD

	if (Ar.ArVer < 112)
	{
		appNotify("StaticMesh of old version %d/%d has been found", Ar.ArVer, Ar.ArLicenseeVer);
	skip_remaining:
		DROP_REMAINING_DATA(Ar);
		ConvertMesh();
		return;
	}

	//!! copy common part inside BIOSHOCK and UC2 code paths
	//!! separate BIOSHOCK and UC2 code paths to cpps
	//!! separate specific data type declarations into cpps
	//!! UC2 code: can integrate LoadExternalUC2Data() into serializer
	Super::Serialize(Ar);
#if TRIBES3
	TRIBES_HDR(Ar, 3);
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard) GUseNewVanguardStaticMesh = false; // in game code InternalVersion is analyzed before serialization
#endif
	Ar << Sections;
	Ar << BoundingBox;			// UPrimitive field, serialized twice ...
#if UC2
	if (Ar.Engine() == GAME_UE2X)
	{
		FVector f120, VectorScale, VectorBase;	// defaults: vec(1.0), Scale=vec(1.0), Base=vec(0.0)
		int     f154, f158, f15C, f160;
		if (Ar.ArVer >= 135)
		{
			Ar << f120 << VectorScale << f154 << f158 << f15C << f160;
			if (Ar.ArVer >= 137) Ar << VectorBase;
		}
		GUC2VectorScale = VectorScale;
		GUC2VectorBase  = VectorBase;
		Ar << VertexStream << ColorStream1 << ColorStream2 << UVStream << IndexStream1;
		if (Ar.ArLicenseeVer != 1) Ar << IndexStream2;
		//!!!!!
//		appPrintf("v:%d c1:%d c2:%d uv:%d idx1:%d\n", VertexStream.Vert.Num(), ColorStream1.Color.Num(), ColorStream2.Color.Num(),
//			UVStream.Num() ? UVStream[0].Data.Num() : -1, IndexStream1.Indices.Num());
		Ar << f108;

		LoadExternalUC2Data();

		// skip collision information
		goto skip_remaining;
	}
#endif // UC2
#if SWRC
	if (Ar.Game == GAME_RepCommando)
	{
		int f164, f160;
		Ar << VertexStream;
		if (Ar.ArVer >= 155) Ar << f164;
		if (Ar.ArVer >= 149) Ar << f160;
		Ar << ColorStream1 << ColorStream2 << UVStream << IndexStream1 << IndexStream2 << f108;
		goto skip_remaining;
	}
#endif // SWRC
	Ar << VertexStream << ColorStream1 << ColorStream2 << UVStream << IndexStream1 << IndexStream2 << f108;

#if 1
	// UT2 and UE2Runtime has very different collision structures
	// We don't need it, so don't bother serializing it
	goto skip_remaining;
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
	Ar << InternalVersion;

#if UT2
	if (Ar.Game == GAME_UT2)
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

	ConvertMesh();

	unguard;
}

#if UC2

void UStaticMesh::LoadExternalUC2Data()
{
	guard(UStaticMesh::LoadExternalUC2Data);

	int i, Size;
	void *Data;

	//?? S.NumFaces is used as NumIndices, but it is not a multiply of 3
	//?? (sometimes is is N*3, sometimes = N*3+1, sometimes - N*3+2 ...)
	//?? May be UC2 uses triangle strips instead of trangles?
	assert(IndexStream1.Indices.Num() == 0);	//???
/*	int NumIndices = 0;
	for (i = 0; i < Sections.Num(); i++)
	{
		FStaticMeshSection &S = Sections[i];
		int idx = S.FirstIndex + S.NumFaces;
	} */
	/*
		FindXprData will return block in following format:
			dword	unk
			dword	itemSize (6 for index stream = 3 verts * sizeof(int16))
					(or unknown meanung in a case of trangle strips)
			dword	unk
			data[]
			dword*5	unk (may be include padding?)
	*/

	for (i = 0; i < Sections.Num(); i++)
	{
//		FStaticMeshSection &S = Sections[i];
		Data = FindXprData(va("%s_%d_pb", Name, i), &Size);
		if (!Data)
		{
			appNotify("Missing external index stream for mesh %s", Name);
			return;
		}
		//!! use
//		appPrintf("...[%d] f4=%d FirstIndex=%d FirstVertex=%d LastVertex=%d fE=%d NumFaces=%d\n", i, S.f4, S.FirstIndex, S.FirstVertex, S.LastVertex, S.fE, S.NumFaces);
		appFree(Data);
	}

	if (!VertexStream.Vert.Num())
	{
		Data = FindXprData(va("%s_VS", Name), &Size);
		if (!Data)
		{
			appNotify("Missing external vertex stream for mesh %s", Name);
			return;
		}
		//!! use
		appFree(Data);
	}

	// other streams:
	//	ColorStream1 = CS
	//	ColorStream2 = AS

	for (i = 0; i < UVStream.Num(); i++)
	{
		if (UVStream[i].Data.Num()) continue;
		Data = FindXprData(va("%s_UV%d", Name, i), &Size);
		if (!Data)
		{
			appNotify("Missing external UV stream for mesh %s", Name);
			return;
		}
		//!! use
		appFree(Data);
	}

	unguard;
}

#endif // UC2


#if VANGUARD

struct FVanguardBasisVector
{
	FVector					v1, v2;

	friend FArchive& operator<<(FArchive &Ar, FVanguardBasisVector &V)
	{
		return Ar << V.v1 << V.v2;
	}
};

SIMPLE_TYPE(FVanguardBasisVector, float)

struct FVanguardUTangentStream
{
	TArray<FVanguardBasisVector> Data;
	int						Version;

	friend FArchive& operator<<(FArchive &Ar, FVanguardUTangentStream &S)
	{
		return Ar << S.Data << S.Version;
	}
};


bool GUseNewVanguardStaticMesh;

void UStaticMesh::SerializeVanguardMesh(FArchive &Ar)
{
	guard(UStaticMesh::SerializeVanguardMesh);

	Super::Serialize(Ar);

	Ar.Seek(Ar.Tell() + 236);		// skip header
	Ar << InternalVersion;
	GUseNewVanguardStaticMesh = (InternalVersion >= 13);

	int		unk1CC, unk134;
	UObject	*unk198, *unk1DC;
	float	unk194, unk19C, unk1A0;
	byte	unk1A4;
	TArray<FVanguardUTangentStream> BasisStream;
	TArray<int> unk144, unk150;
	TArray<byte> unk200;

	Ar << unk1CC << unk134 << f108 << unk198 << unk194 << unk19C;
#if DEBUG_STATICMESH
	appPrintf("Version: %d\n", InternalVersion);
#endif
	if (InternalVersion > 11)
		Ar << unk1A0;
	Ar << unk1A4;
	Ar << unk1DC;

	Ar << BoundingBox;
#if DEBUG_STATICMESH
	appPrintf("Bounds: %g %g %g - %g %g %g (%d)\n", FVECTOR_ARG(BoundingBox.Min), FVECTOR_ARG(BoundingBox.Max), BoundingBox.IsValid);
#endif

	Ar << Sections;

	TArray<FVanguardSkin> Skins;
	Ar << Skins;
	if (Skins.Num() && !Materials.Num())
	{
		const FVanguardSkin &S = Skins[0];
		Materials.AddZeroed(S.Textures.Num());
		for (int i = 0; i < S.Textures.Num(); i++)
			Materials[i].Material = S.Textures[i];
	}

	Ar << Faces << UVStream << BasisStream;
	Ar << unk144 << unk150 << unk200;

	Ar << VertexStream << ColorStream1 << ColorStream2 << IndexStream1 << IndexStream2;

	// skip the remaining data
	Ar.Seek(Ar.GetStopper());

	ConvertMesh();

	unguard;
}

#endif // VANGUARD

void UStaticMesh::ConvertMesh()
{
	guard(UStaticMesh::ConvertMesh);

	int i;

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;
	Mesh->BoundingBox    = BoundingBox;
	Mesh->BoundingSphere = BoundingSphere;

	CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;
	Lod->HasNormals  = true;
	Lod->HasTangents = false;

	// convert sections
	Lod->Sections.AddZeroed(Sections.Num());
	for (i = 0; i < Sections.Num(); i++)
	{
		CMeshSection &Dst = Lod->Sections[i];
		const FStaticMeshSection &Src = Sections[i];
		Dst.Material   = (i < Materials.Num()) ? Materials[i].Material : NULL;
		Dst.FirstIndex = Src.FirstIndex;
		Dst.NumFaces   = Src.NumFaces;
	}

	// convert vertices
	int NumVerts = VertexStream.Vert.Num();
	int NumTexCoords = UVStream.Num();
	if (NumTexCoords > MAX_MESH_UV_SETS)
	{
		appNotify("StaticMesh has %d UV sets", NumTexCoords);
		NumTexCoords = MAX_MESH_UV_SETS;
	}
	Lod->NumTexCoords = NumTexCoords;

	Lod->AllocateVerts(NumVerts);
	bool PrintedWarning = false;
	for (i = 0; i < NumVerts; i++)
	{
		CStaticMeshVertex &V = Lod->Verts[i];
		const FStaticMeshVertex &SV = VertexStream.Vert[i];
		V.Position = CVT(SV.Pos);
		Pack(V.Normal, CVT(SV.Normal));
		for (int j = 0; j < NumTexCoords; j++)
		{
			if (i < UVStream[j].Data.Num())		// Lineage2 has meshes with UVStream[i>1].Data size less than NumVerts
			{
				const FMeshUVFloat &SUV = UVStream[j].Data[i];
				if (j == 0)
				{
					V.UV = CVT(SUV);
				}
				else
				{
					Lod->ExtraUV[j-1][i] = CVT(SUV);
				}
			}
			else if (!PrintedWarning)
			{
				appPrintf("WARNING: StaticMesh UV#%d has %d vertices (should be %d)\n", j, UVStream[j].Data.Num(), NumVerts);
				PrintedWarning = true;
			}
		}
	}

	// copy indices
	Lod->Indices.Initialize(&IndexStream1.Indices);

	Mesh->FinalizeMesh();

	unguard;
}
