#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMesh.h"			//?? remove later (when remove USkeletalMesh dependency)

#include "Psk.h"
#include "Exporters.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"


// PSK uses right-hand coordinates, but unreal uses left-hand.
// When importing PSK into UnrealEd, it mirrors model.
// Here we performing reverse transformation.
#define MIRROR_MESH				1

//#define REFINE_SKEL				1
#define MAX_MESHBONES			512


static void ExportScript(const USkeletalMesh *Mesh, FArchive &Ar)
{
	// mesh info
	Ar.Printf(
		"class %s extends Actor;\n\n"
		"#exec MESH MODELIMPORT MESH=%s MODELFILE=%s.psk\n"
		"#exec MESH ORIGIN      MESH=%s X=%g Y=%g Z=%g YAW=%d PITCH=%d ROLL=%d\n",
		Mesh->Name,
		Mesh->Name, Mesh->Name,
		Mesh->Name, FVECTOR_ARG(Mesh->MeshOrigin),
			Mesh->RotOrigin.Yaw >> 8, Mesh->RotOrigin.Pitch >> 8, Mesh->RotOrigin.Roll >> 8
	);
	// mesh scale
	Ar.Printf(
		"#exec MESH SCALE       MESH=%s X=%g Y=%g Z=%g\n\n",
		Mesh->Name, FVECTOR_ARG(Mesh->MeshScale)
	);
}


static void ExportPsk0(const USkeletalMesh *Mesh, FArchive &Ar)
{
	guard(ExportPsk0);

	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, PtsHdr, WedgHdr, FacesHdr, MatrHdr, BoneHdr, InfHdr;
	int i;

	SAVE_CHUNK(MainHdr, "ACTRHEAD");

	PtsHdr.DataCount = Mesh->Points.Num();
	PtsHdr.DataSize  = sizeof(FVector);
	SAVE_CHUNK(PtsHdr, "PNTS0000");
	for (i = 0; i < Mesh->Points.Num(); i++)
	{
		FVector V = Mesh->Points[i];
#if MIRROR_MESH
		V.Y = -V.Y;
#endif
		Ar << V;
	}

	int NumMaterials = Mesh->Materials.Num();	// count USED materials
	TArray<int> WedgeMat;
	WedgeMat.Empty(Mesh->Wedges.Num());
	WedgeMat.Add(Mesh->Wedges.Num());
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &T = Mesh->Triangles[i];
		int Mat = T.MatIndex;
		WedgeMat[T.WedgeIndex[0]] = Mat;
		WedgeMat[T.WedgeIndex[1]] = Mat;
		WedgeMat[T.WedgeIndex[2]] = Mat;
		if (Mat >= NumMaterials) NumMaterials = Mat + 1;
	}

	WedgHdr.DataCount = Mesh->Wedges.Num();
	WedgHdr.DataSize  = sizeof(VVertex);
	SAVE_CHUNK(WedgHdr, "VTXW0000");
	for (i = 0; i < Mesh->Wedges.Num(); i++)
	{
		VVertex W;
		const FMeshWedge &S = Mesh->Wedges[i];
		W.PointIndex = S.iVertex;
		W.U          = S.TexUV.U;
		W.V          = S.TexUV.V;
		W.MatIndex   = WedgeMat[i];
		W.Reserved   = 0;
		W.Pad        = 0;
		Ar << W;
	}

	FacesHdr.DataCount = Mesh->Triangles.Num();
	FacesHdr.DataSize  = sizeof(VTriangle);
	SAVE_CHUNK(FacesHdr, "FACE0000");
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		VTriangle T = Mesh->Triangles[i];
#if MIRROR_MESH
		Exchange(T.WedgeIndex[0], T.WedgeIndex[1]);
#endif
		Ar << T;
	}

	MatrHdr.DataCount = NumMaterials;
	MatrHdr.DataSize  = sizeof(VMaterial);
	SAVE_CHUNK(MatrHdr, "MATT0000");
	for (i = 0; i < NumMaterials; i++)
	{
		VMaterial M;
		memset(&M, 0, sizeof(M));
		const UUnrealMaterial *Tex = NULL;
		if (i < Mesh->Materials.Num())
		{
			int texIdx = Mesh->Materials[i].TextureIndex;
			if (texIdx < Mesh->Textures.Num())
				Tex = MATERIAL_CAST(Mesh->Textures[texIdx]);
		}
		if (Tex)
		{
			appStrncpyz(M.MaterialName, Tex->Name, ARRAY_COUNT(M.MaterialName));
			if (Tex->IsA("UnrealMaterial")) ExportMaterial(Tex);
		}
		else
			appSprintf(ARRAY_ARG(M.MaterialName), "material_%d", i);
		Ar << M;
	}

#if REFINE_SKEL
	assert(Mesh->RefSkeleton.Num() <= MAX_MESHBONES);
	bool usedBones[MAX_MESHBONES];
	memset(usedBones, 0, sizeof(usedBones));
	// walk all influences
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		VRawBoneInfluence I;
		const FVertInfluences &S = Mesh->VertInfluences[i];
		usedBones[S.BoneIndex] = true;
	}
	// mark bone parents
	usedBones[0] = true;							// mark root explicitly
	for (i = 1; i < Mesh->RefSkeleton.Num(); i++)	// skip root bone
	{
		if (!usedBones[i]) continue;
		int parent = i;
		while (true)
		{
			parent = Mesh->RefSkeleton[parent].ParentIndex;
			if (parent <= 0) break;
			usedBones[parent] = true;
		}
	}
	// create bone remap table
	int CurBone = 0;
	int BoneMap[MAX_MESHBONES];
	for (i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
		if (!usedBones[i]) continue;
		BoneMap[i] = CurBone++;
	}
	if (CurBone < Mesh->RefSkeleton.Num())
		appPrintf("... reduced skeleton from %d bones to %d\n", Mesh->RefSkeleton.Num(), CurBone);
#endif // REFINE_SKEL

#if !REFINE_SKEL
	BoneHdr.DataCount = Mesh->RefSkeleton.Num();
#else
	BoneHdr.DataCount = CurBone;
#endif
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");
	for (i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
#if REFINE_SKEL
		if (!usedBones[i]) continue;
#endif
		VBone B;
		memset(&B, 0, sizeof(B));
		const FMeshBone &S = Mesh->RefSkeleton[i];
		strcpy(B.Name, S.Name);
#if !REFINE_SKEL
		B.NumChildren = S.NumChildren;
		B.ParentIndex = S.ParentIndex;
#else
		// compute actual NumChildren (may be changed because of refined skeleton)
		int NumChildren = 0;
		for (int j = 1; j < Mesh->RefSkeleton.Num(); j++)
			if (usedBones[j] && Mesh->RefSkeleton[j].ParentIndex == i)
				NumChildren++;
		B.NumChildren = NumChildren;
		B.ParentIndex = S.ParentIndex > 0 ? BoneMap[S.ParentIndex] : 0;
#endif // REFINE_SKEL
		B.BonePos     = S.BonePos;

#if MIRROR_MESH
		B.BonePos.Orientation.Y *= -1;
		B.BonePos.Orientation.W *= -1;
		B.BonePos.Position.Y    *= -1;
#endif

		Ar << B;
	}

	InfHdr.DataCount = Mesh->VertInfluences.Num();
	InfHdr.DataSize  = sizeof(VRawBoneInfluence);
	SAVE_CHUNK(InfHdr, "RAWWEIGHTS");
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		VRawBoneInfluence I;
		const FVertInfluences &S = Mesh->VertInfluences[i];
		I.Weight     = S.Weight;
		I.PointIndex = S.PointIndex;
#if !REFINE_SKEL
		I.BoneIndex  = S.BoneIndex;
#else
		I.BoneIndex  = BoneMap[S.BoneIndex];
#endif // REFINE_SKEL
		Ar << I;
	}

	unguard;
}


void ExportPsk(const USkeletalMesh *Mesh, FArchive &Ar)
{
	// export script file
	if (GExportScripts)
	{
		char filename[512];
		appSprintf(ARRAY_ARG(filename), "%s/%s.uc", GetExportPath(Mesh), Mesh->Name);
		FFileReader Ar1(filename, false);
		ExportScript(Mesh, Ar1);
	}

	guard(BaseMesh);
	ExportPsk0(Mesh, Ar);
	unguard;

	if (GExportLods)
	{
		for (int Lod = 1; Lod < Mesh->LODModels.Num(); Lod++)
		{
			guard(Lod);
			char filename[512];
			appSprintf(ARRAY_ARG(filename), "%s/%s_Lod%d.psk", GetExportPath(Mesh), Mesh->Name, Lod);
			FFileReader Ar2(filename, false);
			Ar2.ArVer = 128;			// less than UE3 version (required at least for VJointPos structure)
			// copy Lod to mesh and export it
			USkeletalMesh *Mesh2 = const_cast<USkeletalMesh*>(Mesh);	// can also create new USkeletalMesh and restore LOD to it
			Mesh2->RecreateMeshFromLOD(Lod, true);
			ExportPsk0(Mesh2, Ar2);
			unguardf(("%d", Lod));
		}
	}
}


void ExportPsa(const CAnimSet *Anim, FArchive &Ar)
{
	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, BoneHdr, AnimHdr, KeyHdr;
	int i;

	UObject *OriginalAnim = Anim->OriginalAnim;

	int numBones = Anim->TrackBoneNames.Num();
	int numAnims = Anim->Sequences.Num();

	SAVE_CHUNK(MainHdr, "ANIMHEAD");

	BoneHdr.DataCount = numBones;
	BoneHdr.DataSize  = sizeof(FNamedBoneBinary);
	SAVE_CHUNK(BoneHdr, "BONENAMES");
	for (i = 0; i < numBones; i++)
	{
		FNamedBoneBinary B;
		memset(&B, 0, sizeof(B));
		strcpy(B.Name, *Anim->TrackBoneNames[i]);
		B.Flags       = 0;						// reserved
		B.NumChildren = 0;						// unknown here
		B.ParentIndex = (i > 0) ? 0 : -1;		// unknown for UAnimSet
//		B.BonePos     =							// unknown here
		Ar << B;
	}

	AnimHdr.DataCount = numAnims;
	AnimHdr.DataSize  = sizeof(AnimInfoBinary);
	SAVE_CHUNK(AnimHdr, "ANIMINFO");
	int framesCount = 0;
	for (i = 0; i < numAnims; i++)
	{
		AnimInfoBinary A;
		memset(&A, 0, sizeof(A));
		const CAnimSequence &S = Anim->Sequences[i];
		strcpy(A.Name,  *S.Name);
		strcpy(A.Group, /*??S.Groups.Num() ? *S.Groups[0] :*/ "None");
		A.TotalBones          = numBones;
		A.RootInclude         = 0;				// unused
		A.KeyCompressionStyle = 0;				// reserved
		A.KeyQuotum           = S.NumFrames * numBones; // reserved, but fill with keys count
		A.KeyReduction        = 0;				// reserved
		A.TrackTime           = S.NumFrames;
		A.AnimRate            = S.Rate;
		A.StartBone           = 0;				// reserved
		A.FirstRawFrame       = framesCount;	// useless, but used in UnrealEd when importing
		A.NumRawFrames        = S.NumFrames;
		Ar << A;

		framesCount += S.NumFrames;
	}

	int keysCount = framesCount * numBones;
	KeyHdr.DataCount = keysCount;
	KeyHdr.DataSize  = sizeof(VQuatAnimKey);
	SAVE_CHUNK(KeyHdr, "ANIMKEYS");
	bool requirePsax = false;
	for (i = 0; i < numAnims; i++)
	{
		const CAnimSequence &S = Anim->Sequences[i];
		for (int t = 0; t < S.NumFrames; t++)
		{
			for (int b = 0; b < numBones; b++)
			{
				VQuatAnimKey K;
				CVec3 BP;
				CQuat BO;

				BP.Set(0, 0, 0);			// GetBonePosition() will not alter BP and BO when animation tracks are not exists
				BO.Set(0, 0, 0, 1);
				S.Tracks[b].GetBonePosition(t, S.NumFrames, false, BP, BO);

				K.Position    = (FVector&) BP;
				K.Orientation = (FQuat&)   BO;
				K.Time        = 1;
#if MIRROR_MESH
				K.Orientation.Y *= -1;
				K.Orientation.W *= -1;
				K.Position.Y    *= -1;
#endif

				Ar << K;
				keysCount--;

				// check for user error
				if ((S.Tracks[b].KeyPos.Num() == 0) || (S.Tracks[b].KeyQuat.Num() == 0))
					requirePsax = true;
			}
		}
	}
	assert(keysCount == 0);

	//!! get rid of psax format, store additional flags in text file (AnimSetName.psa.config)
	//!! also could remove -pskx command line option
	//!! note: should use pskx format for mesh - remove psax only
	if (!GExportPskx)
	{
		if (requirePsax) appNotify("Exporting %s'%s': psax is recommended", OriginalAnim->GetClassName(), OriginalAnim->Name);
		return;
	}

	// export extra animation information
	static VChunkHeader FlagHdr;
	int numFlags = numAnims * numBones;
	FlagHdr.DataCount = numFlags;
	FlagHdr.DataSize  = sizeof(byte);
	SAVE_CHUNK(FlagHdr, "ANIMFLAGS");
	for (i = 0; i < numAnims; i++)
	{
		const CAnimSequence &S = Anim->Sequences[i];
		for (int b = 0; b < numBones; b++)
		{
			byte flag = 0;
			if (S.Tracks[b].KeyPos.Num() == 0)
				flag |= PSAX_FLAG_NO_TRANSLATION;
			if (S.Tracks[b].KeyQuat.Num() == 0)
				flag |= PSAX_FLAG_NO_ROTATION;
			Ar << flag;
			numFlags--;
		}
	}
	assert(numFlags == 0);
}


static void ExportStaticMeshLod(const CStaticMeshLod &Lod, FArchive &Ar)
{
	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, PtsHdr, WedgHdr, FacesHdr, MatrHdr, BoneHdr, InfHdr, UVHdr;
	int i;

	int numSections = Lod.Sections.Num();
	int numVerts    = Lod.Verts.Num();
	int numIndices  = Lod.Indices.Num();
	int numFaces    = numIndices / 3;

	SAVE_CHUNK(MainHdr, "ACTRHEAD");

	//?? should find common points ? (weld)
	PtsHdr.DataCount = numVerts;
	PtsHdr.DataSize  = sizeof(FVector);
	SAVE_CHUNK(PtsHdr, "PNTS0000");
	for (i = 0; i < numVerts; i++)
	{
		FVector V = (FVector&)Lod.Verts[i].Position;
#if MIRROR_MESH
		V.Y = -V.Y;
#endif
		Ar << V;
	}

	TArray<int> WedgeMat;
	WedgeMat.Empty(numVerts);
	WedgeMat.Add(numVerts);
	CIndexBuffer::IndexAccessor_t Index = Lod.Indices.GetAccessor();
	for (i = 0; i < numSections; i++)
	{
		const CStaticMeshSection &Sec = Lod.Sections[i];
		for (int j = 0; j < Sec.NumFaces * 3; j++)
		{
			int idx = Index(Lod.Indices, j + Sec.FirstIndex);
			WedgeMat[idx] = i;
		}
	}


	WedgHdr.DataCount = numVerts;
	WedgHdr.DataSize  = sizeof(VVertex);
	SAVE_CHUNK(WedgHdr, "VTXW0000");
	for (i = 0; i < numVerts; i++)
	{
		VVertex W;
		const CStaticMeshVertex &S = Lod.Verts[i];
		W.PointIndex = i;
		W.U          = S.UV[0].U;
		W.V          = S.UV[0].V;
		W.MatIndex   = WedgeMat[i];
		W.Reserved   = 0;
		W.Pad        = 0;
		Ar << W;
	}

	FacesHdr.DataCount = numFaces;
	FacesHdr.DataSize  = sizeof(VTriangle);
	SAVE_CHUNK(FacesHdr, "FACE0000");
	for (i = 0; i < numSections; i++)
	{
		const CStaticMeshSection &Sec = Lod.Sections[i];
		for (int j = 0; j < Sec.NumFaces; j++)
		{
			VTriangle T;
			for (int k = 0; k < 3; k++)
			{
				int idx = Index(Lod.Indices, Sec.FirstIndex + j * 3 + k);
				T.WedgeIndex[k] = idx;
			}
			T.MatIndex        = i;
			T.AuxMatIndex     = 0;
			T.SmoothingGroups = 0;
#if MIRROR_MESH
			Exchange(T.WedgeIndex[0], T.WedgeIndex[1]);
#endif
			Ar << T;
		}
	}

	MatrHdr.DataCount = numSections;
	MatrHdr.DataSize  = sizeof(VMaterial);
	SAVE_CHUNK(MatrHdr, "MATT0000");
	for (i = 0; i < numSections; i++)
	{
		VMaterial M;
		memset(&M, 0, sizeof(M));
		const UUnrealMaterial *Tex = Lod.Sections[i].Material;
		if (Tex)
		{
			appStrncpyz(M.MaterialName, Tex->Name, ARRAY_COUNT(M.MaterialName));
			if (Tex->IsA("UnrealMaterial")) ExportMaterial(Tex);
		}
		else
			appSprintf(ARRAY_ARG(M.MaterialName), "material_%d", i);
		Ar << M;
	}

	BoneHdr.DataCount = 0;		// dummy ...
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");

	InfHdr.DataCount = 0;		// dummy
	InfHdr.DataSize  = sizeof(VRawBoneInfluence);
	SAVE_CHUNK(InfHdr, "RAWWEIGHTS");

	// pskx extension
	for (int j = 1; j < Lod.NumTexCoords; j++)
	{
		UVHdr.DataCount = numVerts;
		UVHdr.DataSize  = sizeof(VMeshUV);
		SAVE_CHUNK(WedgHdr, "EXTRAUV0");
		for (i = 0; i < numVerts; i++)
		{
			VMeshUV UV;
			const CStaticMeshVertex &S = Lod.Verts[i];
			UV.U = S.UV[j].U;
			UV.V = S.UV[j].V;
			Ar << UV;
		}
	}
}


void ExportStaticMesh(const CStaticMesh *Mesh, FArchive &Ar)
{
	UObject *OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	guard(Lod0);
	ExportStaticMeshLod(Mesh->Lods[0], Ar);
	unguard;

	if (GExportLods)
	{
		for (int Lod = 1; Lod < Mesh->Lods.Num(); Lod++)
		{
			guard(Lod);
			char filename[512];
			appSprintf(ARRAY_ARG(filename), "%s/%s_Lod%d.pskx", GetExportPath(OriginalMesh), OriginalMesh->Name, Lod);
			FFileReader Ar2(filename, false);
			Ar2.ArVer = 128;			// less than UE3 version (required at least for VJointPos structure)
			ExportStaticMeshLod(Mesh->Lods[Lod], Ar2);
			unguardf(("%d", Lod));
		}
	}
}
