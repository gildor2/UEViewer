#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh2.h"
#include "UnPackage.h"			// for Bioshock

#include "UnMaterial2.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "TypeConvert.h"

//#define DEBUG_SKELMESH		1


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

	Ar << Textures;
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

	if (Version <= 1 || Ar.Game == GAME_SplinterCell)
	{
		// skip 2nd obsolete section
		TArray<word> tmp;
		Ar << tmp;
	}

	Ar << FaceLevel << Faces << CollapseWedgeThus << Wedges << Materials;
	Ar << MeshScaleMax << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;

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
	Normals.Add(numVerts);
	TArray<CVec3> tmpVerts, tmpNormals;
	tmpVerts.Add(numVerts);
	tmpNormals.Add(numVerts);
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
		TArray<word> tmp3;
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
		TLazyArray<word>       unk3;
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
	if (Package->Game == GAME_Bioshock)
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
		Triangles.Add(Faces.Num());
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
	VertInfluences.Add(numInfluences);
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
	if (Package->Game == GAME_Tribes3) goto base_mesh;
#endif
#if SWRC
	if (Package->Game == GAME_RepCommando) goto base_mesh;
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
		appPrintf("  Lod %d: Points[%d] Wedges[%d] Influences[%d] Faces[%d]  Rigid(Indices[%d] Verts[%d])  Smooth(Indices[%d] Verts[%d] Stream[%d])\n",
			lod, SrcLod.Points.Num(), SrcLod.Wedges.Num(), SrcLod.VertInfluences.Num(), SrcLod.Faces.Num(),
			SrcLod.RigidIndices.Indices.Num(), SrcLod.VertexStream.Verts.Num(),
			SrcLod.SmoothIndices.Indices.Num(), SrcLod.SkinPoints.Num(), SrcLod.SkinningData.Num()
		);
#endif
//		if (SrcLod.Faces.Num() == 0 && SrcLod.SmoothSections.Num() > 0)
//			continue;

		CSkelMeshLod *Lod = new (Mesh->Lods) CSkelMeshLod;
		Lod->NumTexCoords = 1;
		Lod->HasNormals   = false;
		Lod->HasTangents  = false;

		if (SrcLod.Points.Num() && SrcLod.Wedges.Num() && SrcLod.VertInfluences.Num())
		{
			InitSections(*Lod);
			ConvertWedges(*Lod, SrcLod.Points, SrcLod.Wedges, SrcLod.VertInfluences);
			BuildIndicesForLod(*Lod, SrcLod);
		}
		else
		{
			if (lod == 0)
			{
				appPrintf("WARNING: bad LOD mesh, switching to base\n");
				goto base_mesh;
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

	unguard;
}


void USkeletalMesh::InitSections(CSkelMeshLod &Lod)
{
	// allocate sections and set CMeshSection.Material
	Lod.Sections.Add(Materials.Num());
	for (int sec = 0; sec < Materials.Num(); sec++)
	{
		const FMeshMaterial &M = Materials[sec];
		CMeshSection &Sec = Lod.Sections[sec];
		int TexIndex  = M.TextureIndex;
		UUnrealMaterial *Mat = (TexIndex < Textures.Num()) ? Textures[TexIndex] : NULL;
		Sec.Material = UMaterialWithPolyFlags::Create(Mat, M.PolyFlags);
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
		const FVertInfluence &Inf  = VertInfluences[i];
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
		DW.UV[0] = CVT(SW.TexUV);
		// DW.Normal and DW.Tangent are unset
		// setup Bone[] and Weight[]
		const CVertInfo &V = Verts[SW.iVertex];
		for (j = 0; j < V.NumInfs; j++)
		{
			DW.Bone[j]   = V.Bone[j];
			DW.Weight[j] = V.Weight[j];
		}
		for (/* continue */; j < NUM_INFLUENCES; j++)	// place end marker and zero weight
		{
			DW.Bone[j]   = -1;
			DW.Weight[j] = 0.0f;
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
			Lod.Indices.Indices16.Add(NumIndices);
		}

		for (i = 0; i < Triangles.Num(); i++)
		{
			const VTriangle &Face = Triangles[i];
			int MatIndex = Face.MatIndex;
			CMeshSection &Sec  = Lod.Sections[MatIndex];
			assert(MatIndex < NumSections);

			if (pass == 1)	// on 1st pass count NumFaces, on 2nd pass fill indices
			{
				word *idx = &Lod.Indices.Indices16[Sec.FirstIndex + Sec.NumFaces * 3];
				for (int j = 0; j < 3; j++)
					*idx++ = Face.WedgeIndex[j];
			}

			Sec.NumFaces++;
		}
	}

	unguard;
}


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
			Lod.Indices.Indices16.Add(NumIndices);
		}

		int s;

		// smooth sections (influence count >= 2)
		for (s = 0; s < SrcLod.SmoothSections.Num(); s++)
		{
			const FSkelMeshSection &ms = SrcLod.SmoothSections[s];
			int MatIndex = ms.MaterialIndex;
			CMeshSection &Sec  = Lod.Sections[MatIndex];
			assert(MatIndex < NumSections);

			if (pass == 1)
			{
				word *idx = &Lod.Indices.Indices16[Sec.FirstIndex + Sec.NumFaces * 3];
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
		for (s = 0; s < SrcLod.RigidSections.Num(); s++)
		{
			const FSkelMeshSection &ms = SrcLod.RigidSections[s];
			int MatIndex = ms.MaterialIndex;
			CMeshSection &Sec  = Lod.Sections[MatIndex];
			assert(MatIndex < NumSections);

			if (pass == 1)
			{
				word *idx = &Lod.Indices.Indices16[Sec.FirstIndex + Sec.NumFaces * 3];
				for (i = 0; i < ms.NumFaces; i++)
				{
					for (int j = 0; j < 3; j++)
						*idx++ = SrcLod.RigidIndices.Indices[(ms.FirstFace + i) * 3 + j];
				}
			}

			Sec.NumFaces += ms.NumFaces;
		}
	}

	unguard;
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
		TArray<FVector> unk;
		Ar << unk;
	}
	Ar << RefSkeleton;
	Ar << Animation;
	if (Ar.ArLicenseeVer >= 155)
	{
		TArray<UObject*> unk218;
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
	TArray<word> tmp6;
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
	if (SmoothSections.Num() && RigidSections.Num())
		appNotify("have smooth & rigid sections");

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
	Faces.Empty((SmoothIndices.Indices.Num() + RigidIndices.Indices.Num()) / 3);
	TArray<FVector> PointNormals;
	PointNormals.Empty(NumWedges);

	// remap bones and build faces
	TArray<const FSkelMeshSection*> WedgeSection;
	WedgeSection.Empty(NumWedges);
	for (i = 0; i < NumWedges; i++)
		WedgeSection.AddItem(NULL);
	// smooth sections
	guard(SmoothWedges);
	for (k = 0; k < SmoothSections.Num(); k++)
	{
		const FSkelMeshSection &ms = SmoothSections[k];
		for (i = 0; i < ms.NumFaces; i++)
		{
			FMeshFace *F = new (Faces) FMeshFace;
			F->MaterialIndex = ms.MaterialIndex;
			for (j = 0; j < 3; j++)
			{
				int WedgeIndex = SmoothIndices.Indices[(ms.FirstFace + i) * 3 + j];
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

	// convert LineageWedges (smooth sections)
	guard(BuildSmoothWedges);
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
			PointIndex = Points.Add();
			Points[PointIndex] = LW.Point;
			PointNormals.AddItem(LW.Normal);
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
			PointIndex = Points.Add();
			Points[PointIndex] = LW.Pos;
			PointNormals.AddItem(LW.Norm);
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

UStaticMesh::~UStaticMesh()
{
	delete ConvertedMesh;
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
			dword	itemSize (6 for index stream = 3 verts * sizeof(short))
					(or unknown meanung in a case of trangle strips)
			dword	unk
			data[]
			dword*5	unk (may be include padding?)
	*/

	for (i = 0; i < Sections.Num(); i++)
	{
		FStaticMeshSection &S = Sections[i];
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


void UStaticMesh::ConvertMesh()
{
	guard(UStaticMesh::ConvertMesh);

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;
	Mesh->BoundingBox    = BoundingBox;
	Mesh->BoundingSphere = BoundingSphere;

	CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;
	Lod->HasNormals  = true;
	Lod->HasTangents = false;

	// convert sections
	Lod->Sections.Add(Sections.Num());
	for (int i = 0; i < Sections.Num(); i++)
	{
		CMeshSection &Dst = Lod->Sections[i];
		const FStaticMeshSection &Src = Sections[i];
		Dst.Material   = Materials[i].Material;
		Dst.FirstIndex = Src.FirstIndex;
		Dst.NumFaces   = Src.NumFaces;
	}

	// convert vertices
	int NumVerts = VertexStream.Vert.Num();
	int NumTexCoords = UVStream.Num();
	if (NumTexCoords > NUM_MESH_UV_SETS)
	{
		appNotify("StaticMesh has %d UV sets", NumTexCoords);
		NumTexCoords = NUM_MESH_UV_SETS;
	}
	Lod->NumTexCoords = NumTexCoords;

	Lod->AllocateVerts(NumVerts);
	for (int i = 0; i < NumVerts; i++)
	{
		CStaticMeshVertex &V = Lod->Verts[i];
		const FStaticMeshVertex &SV = VertexStream.Vert[i];
		V.Position = CVT(SV.Pos);
		V.Normal   = CVT(SV.Normal);
		for (int j = 0; j < NumTexCoords; j++)
		{
			const FMeshUVFloat &SUV = UVStream[j].Data[i];
			V.UV[j].U      = SUV.U;
			V.UV[j].V      = SUV.V;
		}
	}

	// copy indices
	Lod->Indices.Initialize(&IndexStream1.Indices);

	unguard;
}
