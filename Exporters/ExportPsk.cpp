#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"

#include "Psk.h"
#include "Exporters.h"


// PSK uses right-hand coordinates, but unreal uses left-hand.
// When importing PSK into UnrealEd, it mirrors model.
// Here we performing reverse transformation.
#define MIRROR_MESH				1

#define MAX_MESHBONES			512

/*??

- ExportSkeletalMeshLod() and ExportStaticMeshLod() has a lot of common code, difference is only
  use of CSkelMeshSomething vs CStaticMeshSomething, can use a pointer to variable (Section.Material),
  Stride and NumSections for this; the same for vertices
- may weld vertices when write PNTS0000 chunk

*/

static void ExportScript(const CSkeletalMesh *Mesh, FArchive &Ar)
{
	assert(Mesh->OriginalMesh);
	const char *MeshName = Mesh->OriginalMesh->Name;

	// mesh info
	Ar.Printf(
		"class %s extends Actor;\n\n"
		"#exec MESH MODELIMPORT MESH=%s MODELFILE=%s.psk\n"
		"#exec MESH ORIGIN      MESH=%s X=%g Y=%g Z=%g YAW=%d PITCH=%d ROLL=%d\n",
		MeshName,
		MeshName, MeshName,
		MeshName, VECTOR_ARG(Mesh->MeshOrigin),
			Mesh->RotOrigin.Yaw >> 8, Mesh->RotOrigin.Pitch >> 8, Mesh->RotOrigin.Roll >> 8
	);
	// mesh scale
	Ar.Printf(
		"#exec MESH SCALE       MESH=%s X=%g Y=%g Z=%g\n\n",
		MeshName, VECTOR_ARG(Mesh->MeshScale)
	);
}


static void ExportSkeletalMeshLod(const CSkeletalMesh &Mesh, const CSkelMeshLod &Lod, FArchive &Ar)
{
	guard(ExportSkeletalMeshLod);

	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, PtsHdr, WedgHdr, FacesHdr, MatrHdr, BoneHdr, InfHdr;
	int i, j;

	int numSections = Lod.Sections.Num();
	int numVerts    = Lod.NumVerts;
	int numIndices  = Lod.Indices.Num();
	int numFaces    = numIndices / 3;
	int numBones    = Mesh.RefSkeleton.Num();

	SAVE_CHUNK(MainHdr, "ACTRHEAD");

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
		const CSkelMeshSection &Sec = Lod.Sections[i];
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
		const CSkelMeshVertex &S = Lod.Verts[i];
		W.PointIndex = i;
		W.U          = S.UV[0].U;
		W.V          = S.UV[0].V;
		W.MatIndex   = WedgeMat[i];
		W.Reserved   = 0;
		W.Pad        = 0;
		Ar << W;
	}

	FacesHdr.DataCount = numFaces;
	FacesHdr.DataSize  = sizeof(VTriangle16);
	SAVE_CHUNK(FacesHdr, "FACE0000");
	for (i = 0; i < numSections; i++)
	{
		const CSkelMeshSection &Sec = Lod.Sections[i];
		for (int j = 0; j < Sec.NumFaces; j++)
		{
			VTriangle16 T;
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
	for (i = 0; i < Lod.Sections.Num(); i++)
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

	BoneHdr.DataCount = numBones;
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");
	for (i = 0; i < numBones; i++)
	{
		VBone B;
		memset(&B, 0, sizeof(B));
		const CSkelMeshBone &S = Mesh.RefSkeleton[i];
		strcpy(B.Name, S.Name);
		// count NumChildren
		int NumChildren = 0;
		for (j = 0; j < numBones; j++)
			if ((j != i) && (Mesh.RefSkeleton[j].ParentIndex == i))
				NumChildren++;
		B.NumChildren = NumChildren;
		B.ParentIndex = S.ParentIndex;
		B.BonePos.Position    = (FVector&) S.Position;
		B.BonePos.Orientation = (FQuat&)   S.Orientation;

#if MIRROR_MESH
		B.BonePos.Orientation.Y *= -1;
		B.BonePos.Orientation.W *= -1;
		B.BonePos.Position.Y    *= -1;
#endif

		Ar << B;
	}

	// count influences
	int NumInfluences = 0;
	for (i = 0; i < numVerts; i++)
	{
		const CSkelMeshVertex &V = Lod.Verts[i];
		for (j = 0; j < NUM_INFLUENCES; j++)
		{
			if (V.Bone[j] < 0) break;
			NumInfluences++;
		}
	}
	// write influences
	InfHdr.DataCount = NumInfluences;
	InfHdr.DataSize  = sizeof(VRawBoneInfluence);
	SAVE_CHUNK(InfHdr, "RAWWEIGHTS");
	for (i = 0; i < numVerts; i++)
	{
		const CSkelMeshVertex &V = Lod.Verts[i];
		for (j = 0; j < NUM_INFLUENCES; j++)
		{
			if (V.Bone[j] < 0) break;
			NumInfluences--;				// just for verification

			VRawBoneInfluence I;
			I.Weight     = V.Weight[j];
			I.BoneIndex  = V.Bone[j];
			I.PointIndex = i;				//?? remap
			Ar << I;
		}
	}
	assert(NumInfluences == 0);

	unguard;
}


void ExportPsk(const CSkeletalMesh *Mesh, FArchive &Ar)
{
	UObject *OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	// export script file
	if (GExportScripts)
	{
		char filename[512];
		appSprintf(ARRAY_ARG(filename), "%s/%s.uc", GetExportPath(OriginalMesh), OriginalMesh->Name);
		FFileReader Ar1(filename, false);
		ExportScript(Mesh, Ar1);
	}

	int MaxLod = (GExportLods) ? Mesh->Lods.Num() : 1;
	for (int Lod = 0; Lod < MaxLod; Lod++)
	{
		guard(Lod);
		char filename[512];
		const char *Ext = (GExportPskx) ? "pskx" : "psk";
		if (Lod == 0)
			appSprintf(ARRAY_ARG(filename), "%s/%s.%s", GetExportPath(OriginalMesh), OriginalMesh->Name, Ext);
		else
			appSprintf(ARRAY_ARG(filename), "%s/%s_Lod%d.%s", GetExportPath(OriginalMesh), OriginalMesh->Name, Lod, Ext);
		FFileReader Ar2(filename, false);
		Ar2.ArVer = 128;			// less than UE3 version; not required anymore (were required for VJointPos before)
		ExportSkeletalMeshLod(*Mesh, Mesh->Lods[Lod], Ar2);
		unguardf(("%d", Lod));
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
	bool requireConfig = false;
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
					requireConfig = true;
			}
		}
	}
	assert(keysCount == 0);

	// generate configuration file with extended attributes
	if (!Anim->AnimRotationOnly && !Anim->UseAnimTranslation.Num() && !Anim->ForceMeshTranslation.Num() && !requireConfig)
		return;		// nothing to write

	char filename[512];
	appSprintf(ARRAY_ARG(filename), "%s/%s.config", GetExportPath(OriginalAnim), OriginalAnim->Name);
	FFileReader Ar1(filename, false);

	// we are using UE3 property names here

	// AnimRotationOnly
	Ar1.Printf("[AnimSet]\nbAnimRotationOnly=%d\n", Anim->AnimRotationOnly);
	// UseTranslationBoneNames
	Ar1.Printf("\n[UseTranslationBoneNames]\n");
	for (i = 0; i < Anim->UseAnimTranslation.Num(); i++)
		if (Anim->UseAnimTranslation[i])
			Ar1.Printf("%s\n", *Anim->TrackBoneNames[i]);
	// ForceMeshTranslationBoneNames
	Ar1.Printf("\n[ForceMeshTranslationBoneNames]\n");
	for (i = 0; i < Anim->ForceMeshTranslation.Num(); i++)
		if (Anim->ForceMeshTranslation[i])
			Ar1.Printf("%s\n", *Anim->TrackBoneNames[i]);

	if (requireConfig)
	{
		// has removed tracks inside the sequence
		// currently used for Unreal Championship 2 only
		Ar1.Printf("\n[RemoveTracks]\n");
		for (i = 0; i < numAnims; i++)
		{
			const CAnimSequence &S = Anim->Sequences[i];
			for (int b = 0; b < numBones; b++)
			{
#define FLAG_NO_TRANSLATION		1
#define FLAG_NO_ROTATION		2
				static const char *FlagInfo[] = { "", "trans", "rot", "all" };
				int flag = 0;
				if (S.Tracks[b].KeyPos.Num() == 0)
					flag |= FLAG_NO_TRANSLATION;
				if (S.Tracks[b].KeyQuat.Num() == 0)
					flag |= FLAG_NO_ROTATION;
				if (flag)
					Ar1.Printf("%s.%d=%s\n", *S.Name, b, FlagInfo[flag]);
			}
		}
	}
}


static void ExportStaticMeshLod(const CStaticMeshLod &Lod, FArchive &Ar)
{
	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, PtsHdr, WedgHdr, FacesHdr, MatrHdr, BoneHdr, InfHdr, UVHdr;
	int i;

	int numSections = Lod.Sections.Num();
	int numVerts    = Lod.NumVerts;
	int numIndices  = Lod.Indices.Num();
	int numFaces    = numIndices / 3;

	SAVE_CHUNK(MainHdr, "ACTRHEAD");

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
	FacesHdr.DataSize  = sizeof(VTriangle16);
	SAVE_CHUNK(FacesHdr, "FACE0000");
	for (i = 0; i < numSections; i++)
	{
		const CStaticMeshSection &Sec = Lod.Sections[i];
		for (int j = 0; j < Sec.NumFaces; j++)
		{
			VTriangle16 T;
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
			Ar2.ArVer = 128;			// less than UE3 version; not required anymore (were required for VJointPos before)
			ExportStaticMeshLod(Mesh->Lods[Lod], Ar2);
			unguardf(("%d", Lod));
		}
	}
}
