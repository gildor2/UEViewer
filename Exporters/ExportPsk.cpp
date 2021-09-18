#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMesh/UnMesh4.h"		// for USkeleton

#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMesh.h"
#include "TypeConvert.h"

#include "Psk.h"
#include "Exporters.h"

#include "UnrealMesh/UnMathTools.h"


// PSK uses right-hand coordinates, but unreal uses left-hand.
// When importing PSK into UnrealEd, it mirrors model.
// Here we performing reverse transformation.
#define MIRROR_MESH				1

static void ExportScript(const CSkeletalMesh *Mesh, FArchive &Ar)
{
	assert(Mesh->OriginalMesh);
	const char *MeshName = Mesh->OriginalMesh->Name;

	// There's a good description of #exec parameters for a mesh:
	// https://unreal.shaungoeppinger.com/skeletal-animation-import-directives/

	// mesh info
	Ar.Printf(
		"class %s extends Actor;\n\n"
		"#exec MESH MODELIMPORT MESH=%s MODELFILE=%s.psk\n"
		"#exec MESH ORIGIN      MESH=%s X=%g Y=%g Z=%g YAW=%d PITCH=%d ROLL=%d\n"
		"// rotator: P=%d Y=%d R=%d\n",
		MeshName,
		MeshName, MeshName,
		MeshName, VECTOR_ARG(Mesh->MeshOrigin),
		Mesh->RotOrigin.Yaw >> 8, Mesh->RotOrigin.Pitch >> 8, Mesh->RotOrigin.Roll >> 8,
		Mesh->RotOrigin.Pitch, Mesh->RotOrigin.Yaw, Mesh->RotOrigin.Roll
	);
	// mesh scale
	Ar.Printf(
		"#exec MESH SCALE       MESH=%s X=%g Y=%g Z=%g\n\n",
		MeshName, VECTOR_ARG(Mesh->MeshScale)
	);
	// sockets
	for (int i = 0; i < Mesh->Sockets.Num(); i++)
	{
		const CSkelMeshSocket& S = Mesh->Sockets[i];
		const CCoords& T = S.Transform;
		FRotator R;
		AxisToRotator(T.axis, R);
		Ar.Printf(
			"#exec MESH ATTACHNAME  MESH=%s BONE=\"%s\" TAG=\"%s\" YAW=%d PITCH=%d ROLL=%d X=%g Y=%g Z=%g\n"
			"// rotator: P=%d Y=%d R=%d\n",
			MeshName, *S.Bone, *S.Name,
			R.Yaw >> 8, R.Pitch >> 8, R.Roll >> 8,
			VECTOR_ARG(T.origin),
			R.Pitch, R.Yaw, R.Roll
		);
	}
}


// Common code for psk format, shared between CSkeletalMesh and CStaticMesh

#define VERT(n)		OffsetPointer(Verts, VertexSize * (n))

static void ExportCommonMeshData
(
	FArchive &Ar,
	const CMeshSection *Sections, int NumSections,
	const CMeshVertex  *Verts,    int NumVerts, int VertexSize,
	const CIndexBuffer &Indices,
	CVertexShare       &Share
)
{
	guard(ExportCommonMeshData);

	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, PtsHdr, WedgHdr, FacesHdr, MatrHdr, NormHdr;
	int i;

#define SECT(n)		(Sections + n)

	// main psk header
	MainHdr.TypeFlag = PSK_VERSION;
	SAVE_CHUNK(MainHdr, "ACTRHEAD");

	guard(Points);
	PtsHdr.DataCount = Share.Points.Num();
	PtsHdr.DataSize  = sizeof(FVector);
	SAVE_CHUNK(PtsHdr, "PNTS0000");
	if (sizeof(FVector) == sizeof(float) * 3)
	{
#if MIRROR_MESH
		// Mirror mesh inplace
		for (CVec3& V : Share.Points)
			V[1] = -V[1]; // V.Y
#endif
		Ar.Serialize(Share.Points.GetData(), Share.Points.Num() * sizeof(CVec3));
	}
	else
	{
		for (i = 0; i < Share.Points.Num(); i++)
		{
			FVector V = (FVector&) Share.Points[i];
#if MIRROR_MESH
			V.Y = -V.Y;
#endif
			Ar << V;
		}
	}
	unguard;

	// get number of faces (some Gears3 meshes may have index buffer larger than needed)
	// get wedge-material mapping
	int numFaces = 0;
	CIndexBuffer::IndexAccessor_t Index = Indices.GetAccessor();

	guard(Wedges);
	TArray<int> WedgeMat;
	WedgeMat.Empty(NumVerts);
	WedgeMat.AddZeroed(NumVerts);
	for (i = 0; i < NumSections; i++)
	{
		const CMeshSection &Sec = *SECT(i);
		numFaces += Sec.NumFaces;
		for (int j = 0; j < Sec.NumFaces * 3; j++)
		{
			int idx = Index(j + Sec.FirstIndex);
			WedgeMat[idx] = i;
		}
	}

	WedgHdr.DataCount = NumVerts;
	WedgHdr.DataSize  = sizeof(VVertex);
	SAVE_CHUNK(WedgHdr, "VTXW0000");
	for (i = 0; i < NumVerts; i++)
	{
		VVertex W;
		const CMeshVertex &S = *VERT(i);
		W.PointIndex = Share.WedgeToVert[i];
		W.U          = S.UV.U;
		W.V          = S.UV.V;
		W.MatIndex   = WedgeMat[i];
		W.Reserved   = 0;
		W.Pad        = 0;
		if (sizeof(VVertex) == sizeof(int32) * 4)
		{
			Ar.Serialize(&W, sizeof(W));
		}
		else
		{
			Ar << W;
		}
	}
	unguard;

	guard(Faces);
	if (NumVerts <= 65536)
	{
		FacesHdr.DataCount = numFaces;
		FacesHdr.DataSize  = sizeof(VTriangle16);
		SAVE_CHUNK(FacesHdr, "FACE0000");
		for (i = 0; i < NumSections; i++)
		{
			const CMeshSection &Sec = *SECT(i);
			for (int j = 0; j < Sec.NumFaces; j++)
			{
				VTriangle16 T;
				for (int k = 0; k < 3; k++)
				{
					int idx = Index(Sec.FirstIndex + j * 3 + k);
					assert((idx & ~0xFFFF) == 0); // (idx >= 0 && idx < 65536);
					T.WedgeIndex[k] = idx;
				}
				T.MatIndex        = i;
				T.AuxMatIndex     = 0;
				T.SmoothingGroups = 1;
#if MIRROR_MESH
				Exchange(T.WedgeIndex[0], T.WedgeIndex[1]);
#endif
				if (sizeof(VTriangle16) == 12)
				{
					Ar.Serialize(&T, sizeof(T));
				}
				else
				{
					Ar << T;
				}
			}
		}
	}
	else
	{
		// pskx extension
		FacesHdr.DataCount = numFaces;
		FacesHdr.DataSize  = 18; // sizeof(VTriangle32) without alignment
		SAVE_CHUNK(FacesHdr, "FACE3200");
		for (i = 0; i < NumSections; i++)
		{
			const CMeshSection &Sec = *SECT(i);
			for (int j = 0; j < Sec.NumFaces; j++)
			{
				VTriangle32 T;
				for (int k = 0; k < 3; k++)
				{
					int idx = Index(Sec.FirstIndex + j * 3 + k);
					T.WedgeIndex[k] = idx;
				}
				T.MatIndex        = i;
				T.AuxMatIndex     = 0;
				T.SmoothingGroups = 1;
#if MIRROR_MESH
				Exchange(T.WedgeIndex[0], T.WedgeIndex[1]);
#endif
				// This structure is not packed, can't use single serialize call for it
				Ar << T;
			}
		}
	}
	unguard;

	guard(Materials);
	MatrHdr.DataCount = NumSections;
	MatrHdr.DataSize  = sizeof(VMaterial);
	SAVE_CHUNK(MatrHdr, "MATT0000");
	for (i = 0; i < NumSections; i++)
	{
		VMaterial M;
		memset(&M, 0, sizeof(M));
		const UUnrealMaterial *Tex = SECT(i)->Material;
		M.TextureIndex = i; // could be required for UT99
		//!! this will not handle (UMaterialWithPolyFlags->Material==NULL) correctly - will make MaterialName=="None"
		//!! (the same valid for md5mesh export)
		if (Tex)
		{
			appStrncpyz(M.MaterialName, Tex->Name, ARRAY_COUNT(M.MaterialName));
			ExportObject(Tex);
		}
		else
			appSprintf(ARRAY_ARG(M.MaterialName), "material_%d", i);
		Ar << M;
	}
	unguard;

	guard(Normals);
	NormHdr.DataCount = Share.Normals.Num();
	NormHdr.DataSize  = sizeof(CVec3);
	SAVE_CHUNK(NormHdr, "VTXNORMS");
	for (i = 0; i < Share.Normals.Num(); i++)
	{
		CVec3 Normal;
		Unpack(Normal, Share.Normals[i]);
		Normal.Normalize();
#if MIRROR_MESH
		Normal.Y = -Normal.Y;
#endif
		Ar << Normal.X << Normal.Y << Normal.Z;
	}
	unguard;

	unguard;
}

static void ExportVertexColors(FArchive &Ar, const FColor* Colors, int NumVerts)
{
	guard(ExportVertexColors);

	if (!Colors) return;

	static VChunkHeader ColorHdr;
	ColorHdr.DataCount = NumVerts;
	ColorHdr.DataSize  = sizeof(FColor);

	SAVE_CHUNK(ColorHdr, "VERTEXCOLOR");
	Ar.Serialize((void*)Colors, sizeof(FColor) * NumVerts);

	unguard;
}

static void ExportExtraUV
(
	FArchive &Ar,
	const CMeshUVFloat* const ExtraUV[],
	int NumVerts,
	int NumTexCoords
)
{
	guard(ExportExtraUV);

	static VChunkHeader UVHdr;
	UVHdr.DataCount = NumVerts;
	UVHdr.DataSize  = sizeof(VMeshUV);

	for (int j = 1; j < NumTexCoords; j++)
	{
		char chunkName[32];
		appSprintf(ARRAY_ARG(chunkName), "EXTRAUVS%d", j-1);
		SAVE_CHUNK(UVHdr, chunkName);
		const CMeshUVFloat* SUV = ExtraUV[j-1];

		if (sizeof(CMeshUVFloat) == sizeof(float) * 2)
		{
			Ar.Serialize((void*)SUV, sizeof(CMeshUVFloat) * NumVerts);
		}
		else
		{
			for (int i = 0; i < NumVerts; i++, SUV++)
			{
				VMeshUV UV;
				UV.U = SUV->U;
				UV.V = SUV->V;
				Ar << UV;
			}
		}
	}

	unguard;
}

static void CopyBoneName(char* Dst, int DstLen, const char* Src)
{
	int NameLength = strlen(Src);
	if (NameLength < DstLen)
	{
		strcpy(Dst, Src);
	}
	else
	{
		int Middle = (DstLen - 4) / 2;
		memcpy(Dst, Src, Middle);
		memcpy(Dst + Middle, "____", 4);
		memcpy(Dst + Middle + 4, Src + NameLength - Middle + 1, Middle); // +1 for keeping null terminating character
		appPrintf("WARNING: bone name \"%s\" is too long, renamed to \"%s\"\n", Src, Dst);
	}
}

static void ExportSkeletalMeshLod(const CSkeletalMesh &Mesh, const CSkelMeshLod &Lod, FArchive &Ar)
{
	guard(ExportSkeletalMeshLod);

	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader BoneHdr, InfHdr;

	int i, j;
	CVertexShare Share;

	// weld vertices
	// The code below differs from similar code for StaticMesh export: it relies on vertex weight
	// information to not perform occasional welding of vertices which has the same position and
	// normal, but belongs to different bones.
//	appResetProfiler();
	guard(WeldVerts);
	Share.Prepare(Lod.Verts, Lod.NumVerts, sizeof(CSkelMeshVertex));
	for (i = 0; i < Lod.NumVerts; i++)
	{
		const CSkelMeshVertex &S = Lod.Verts[i];
		// Here we relies on high possibility that vertices which should be shared between
		// triangles will have the same order of weights and bones (because most likely
		// these vertices were duplicated by copying). Doing more complicated comparison
		// will reduce performance with possibly reducing size of exported mesh by a few
		// more vertices.
		uint32 WeightsHash = S.PackedWeights;
		for (j = 0; j < ARRAY_COUNT(S.Bone); j++)
			WeightsHash ^= S.Bone[j] << j;
		Share.AddVertex(S.Position, S.Normal, WeightsHash);
	}
	unguard;
//	appPrintProfiler();
//	appPrintf("%d wedges were welded into %d verts\n", Lod.NumVerts, Share.Points.Num());

	ExportCommonMeshData
	(
		Ar,
		&Lod.Sections[0], Lod.Sections.Num(),
		Lod.Verts, Lod.NumVerts, sizeof(CSkelMeshVertex),
		Lod.Indices,
		Share
	);

	int numBones = Mesh.RefSkeleton.Num();

	guard(Bones);
	BoneHdr.DataCount = numBones;
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");
	for (i = 0; i < numBones; i++)
	{
		VBone B;
		memset(&B, 0, sizeof(B));
		const CSkelMeshBone &S = Mesh.RefSkeleton[i];
		CopyBoneName(B.Name, sizeof(B.Name), *S.Name);
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
	unguard;

	// count influences
	guard(Influences);
	int NumInfluences = 0;
	for (i = 0; i < Share.Points.Num(); i++)
	{
		int WedgeIndex = Share.VertToWedge[i];
		const CSkelMeshVertex &V = Lod.Verts[WedgeIndex];
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
	for (i = 0; i < Share.Points.Num(); i++)
	{
		int WedgeIndex = Share.VertToWedge[i];
		const CSkelMeshVertex &V = Lod.Verts[WedgeIndex];
		CVec4 UnpackedWeights;
		V.UnpackWeights(UnpackedWeights);
		for (j = 0; j < NUM_INFLUENCES; j++)
		{
			if (V.Bone[j] < 0) break;
			NumInfluences--;				// just for verification

			VRawBoneInfluence I;
			I.Weight     = UnpackedWeights.v[j];
			I.BoneIndex  = V.Bone[j];
			I.PointIndex = i;
			if (sizeof(VRawBoneInfluence) == sizeof(int) * 3)
			{
				Ar.Serialize(&I, sizeof(I));
			}
			else
			{
				Ar << I;
			}
		}
	}
	assert(NumInfluences == 0);
	unguard;

	ExportVertexColors(Ar, Lod.VertexColors, Lod.NumVerts);
	ExportExtraUV(Ar, Lod.ExtraUV, Lod.NumVerts, Lod.NumTexCoords);

/*	if (!GExportPskx)						// nothing more to write
		return;

	// pskx extension
*/
	unguard;
}


void ExportPsk(const CSkeletalMesh *Mesh)
{
	const UObject *OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	int MaxLod = (GExportLods) ? Mesh->Lods.Num() : 1;
	for (int Lod = 0; Lod < MaxLod; Lod++)
	{
		guard(Lod);

		const CSkelMeshLod &MeshLod = Mesh->Lods[Lod];
		if (!MeshLod.Sections.Num()) continue;		// empty mesh

		bool UsePskx = (MeshLod.NumVerts > 65536);

		char filename[512];
		const char *Ext = (UsePskx) ? "pskx" : "psk";
		if (Lod == 0)
			appSprintf(ARRAY_ARG(filename), "%s.%s", OriginalMesh->Name, Ext);
		else
			appSprintf(ARRAY_ARG(filename), "%s_Lod%d.%s", OriginalMesh->Name, Lod, Ext);

		FArchive *Ar = CreateExportArchive(OriginalMesh, EFileArchiveOptions::Default, "%s", filename);
		if (Ar)
		{
			ExportSkeletalMeshLod(*Mesh, MeshLod, *Ar);
			delete Ar;
		}
		else if (Lod == 0)
		{
			// First LOD was failed to be saved, most likely file already exists
			return;
		}

		unguardf("%d", Lod);
	}

	// export script file
	if (GExportScripts)
	{
		FArchive *Ar = CreateExportArchive(OriginalMesh, EFileArchiveOptions::TextFile, "%s.uc", OriginalMesh->Name);
		if (Ar)
		{
			ExportScript(Mesh, *Ar);
			delete Ar;
		}
	}

	if (OriginalMesh->GetTypeinfo()->NumProps)
	{
		FArchive* PropAr = CreateExportArchive(OriginalMesh, EFileArchiveOptions::TextFile, "%s.props.txt", OriginalMesh->Name);
		if (PropAr)
		{
			OriginalMesh->GetTypeinfo()->SaveProps(OriginalMesh, *PropAr);
			delete PropAr;
		}
	}

	// export animation
	if (Mesh->Anim)
	{
		ExportObject(Mesh->Anim->OriginalAnim);
	}
}

// If Skeleton has at most this number of animations, export them as separate psa files.
// This is needed because UAnimSequence4 can refer to other animation sequences in properties
// (e.g. UAnimSequence4::RefPoseSeq).
#define MIN_ANIMSET_SIZE 4

//todo: review if this function could be reused for glTF
const UObject* GetPrimaryAnimObject(const CAnimSet* Anim)
{
	guard(GetPrimaryAnimObject);
#if UNREAL4
	// When AnimSet consists of just 1 animation track, it is possible that we're exporting
	// a separate UE4 AnimSequence. In this case it's worth using that AnimSequence's filename,
	// otherwise we'll have multiple animations mapped to the same exported file.
	if (Anim->Sequences.Num() && Anim->OriginalAnim->IsA("Skeleton"))
	{
		const USkeleton* Skeleton = static_cast<const USkeleton*>(Anim->OriginalAnim);
		assert(Skeleton->OriginalAnims.Num() == Anim->Sequences.Num());
		// Allow up to 3
		if (Skeleton->OriginalAnims.Num() <= MIN_ANIMSET_SIZE)
			return Skeleton->OriginalAnims[0];
	}
#endif // UNREAL4

	// Not a Skeleton, or has different animation track count
	return Anim->OriginalAnim;
	unguard;
}

static void DoExportPsa(const CAnimSet* Anim, const UObject* OriginalAnim)
{
	guard(DoExportPsa);

	FArchive* Ar0 = CreateExportArchive(OriginalAnim, EFileArchiveOptions::Default, "%s.psa", OriginalAnim->Name);
	if (!Ar0) return;
	FArchive &Ar = *Ar0;						// use "Ar << obj" instead of "(*Ar) << obj"

	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, BoneHdr, AnimHdr, KeyHdr, ScaleKeysHdr;
	int i;

	MainHdr.TypeFlag = PSA_VERSION;
	SAVE_CHUNK(MainHdr, "ANIMHEAD");

	int numBones = Anim->TrackBoneNames.Num();
	int numAnims = Anim->Sequences.Num();

	BoneHdr.DataCount = numBones;
	BoneHdr.DataSize  = sizeof(FNamedBoneBinary);
	SAVE_CHUNK(BoneHdr, "BONENAMES");
	for (i = 0; i < numBones; i++)
	{
		FNamedBoneBinary B;
		memset(&B, 0, sizeof(B));
		CopyBoneName(B.Name, sizeof(B.Name), *Anim->TrackBoneNames[i]);
		B.Flags       = 0;						// reserved
		B.NumChildren = 0;						// unknown here
		B.ParentIndex = (i > 0) ? 0 : -1;		// unknown for UAnimSet
		B.BonePos.Length = 1.0f;
		if (Anim->BonePositions.IsValidIndex(i))
		{
			// The AnimSet has bone transform information, store it in psa file (UE4+)
			FQuat Q1;
			CQuat Q2 = CVT(Q1);
			Q1 = CVT(Q2);
			B.BonePos.Position = CVT(Anim->BonePositions[i].Position);
			B.BonePos.Orientation = CVT(Anim->BonePositions[i].Orientation);
		}
		Ar << B;
	}

	int framesCount = 0;

	guard(AnimInfo);
	AnimHdr.DataCount = numAnims;
	AnimHdr.DataSize  = sizeof(AnimInfoBinary);
	SAVE_CHUNK(AnimHdr, "ANIMINFO");
	for (i = 0; i < numAnims; i++)
	{
		AnimInfoBinary A;
		memset(&A, 0, sizeof(A));
		const CAnimSequence &S = *Anim->Sequences[i];
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
	unguard;

	bool requireConfig = false;

	guard(Keys);
	int keysCount = framesCount * numBones;
	KeyHdr.DataCount = keysCount;
	KeyHdr.DataSize  = sizeof(VQuatAnimKey);
	SAVE_CHUNK(KeyHdr, "ANIMKEYS");
	for (i = 0; i < numAnims; i++)
	{
		guard(Sequence);
		const CAnimSequence &S = *Anim->Sequences[i];
		for (int t = 0; t < S.NumFrames; t++)
		{
			for (int b = 0; b < numBones; b++)
			{
				VQuatAnimKey K;
				CVec3 BP;
				CQuat BO;

				BP.Set(0, 0, 0);			// GetBonePosition() will not alter BP and BO when animation tracks are not exists
				BO.Set(0, 0, 0, 1);
				S.Tracks[b]->GetBonePosition(t, S.NumFrames, false, BP, BO);

				K.Position    = (FVector&) BP;
				K.Orientation = (FQuat&)   BO;
				K.Time        = 1;
#if MIRROR_MESH
				K.Orientation.Y *= -1;
				K.Orientation.W *= -1;
				K.Position.Y    *= -1;
#endif

				if (sizeof(VQuatAnimKey) == sizeof(float) * 8)
				{
					// Packed structure, serialize with a single call
					Ar.Serialize(&K, sizeof(K));
				}
				else
				{
					Ar << K;
				}
				keysCount--;

				// check for user error
				if ((S.Tracks[b]->KeyPos.Num() == 0) || (S.Tracks[b]->KeyQuat.Num() == 0))
					requireConfig = true;
			}
		}
		unguard;
	}
	assert(keysCount == 0);
	unguard;

	// UE3 source code reference: UEditorEngine::ImportPSAIntoAnimSet()
	// The function doesn't perform any checks for chunk names etc, so we're very restricted in
	// using very strict order of chunks. If main chunk has version (TypeFlag) at least 20090127,
	// importer will always read "SCALEKEYS" chunk.
	if (PSA_VERSION >= 20090127)
	{
		ScaleKeysHdr.DataCount = 0;
		ScaleKeysHdr.DataSize  = 16; // sizeof(VScaleAnimKey) = FVector + float
		SAVE_CHUNK(ScaleKeysHdr, "SCALEKEYS");
	}

	// psa file is done
	delete Ar0;

	// generate configuration file with extended attributes

	// Get statistics of each bone retargeting mode to see if we need a config or not
	int ModeCounts[(int)EBoneRetargetingMode::Count];
	memset(ModeCounts, 0, sizeof(ModeCounts));
	for (EBoneRetargetingMode Mode : Anim->BoneModes)
	{
		ModeCounts[(int)Mode]++;
	}

	bool bSaveConfig = true;
	if (ModeCounts[(int)EBoneRetargetingMode::Animation] != Anim->BoneModes.Num() && !requireConfig)
	{
		// nothing to write
		bSaveConfig = false;
	}

	FArchive *Ar1 = NULL;
	if (bSaveConfig)
	{
		Ar1 = CreateExportArchive(OriginalAnim, EFileArchiveOptions::TextFile, "%s.config", OriginalAnim->Name);
	}

	if (Ar1)
	{
		// we are using UE3 property names here

		// Just optimization of the config file size: use AnimRotationOnly when most of bones use translation
		// from the mesh.
		bool AnimRotationOnly = (ModeCounts[(int)EBoneRetargetingMode::Animation]
			+ ModeCounts[(int)EBoneRetargetingMode::Mesh]) == numBones;

		if (AnimRotationOnly)
		{
			// Old mode
			// AnimRotationOnly: this will reset all bones to use translation from the mesh
			Ar1->Printf("[AnimSet]\nbAnimRotationOnly=%d\n", AnimRotationOnly);
			// UseTranslationBoneNames: allow animated translation
			Ar1->Printf("\n[UseTranslationBoneNames]\n");
			for (i = 0; i < Anim->BoneModes.Num(); i++)
				if (Anim->BoneModes[i] == EBoneRetargetingMode::Animation)
					Ar1->Printf("%s\n", *Anim->TrackBoneNames[i]);
			// ForceMeshTranslationBoneNames: this will revert a bone back to translation from the mesh.
			// It is no longer used. In UE3, it was possible to set up AnimRotationOnly per mesh, or from
			// AnimTree, so this setting wasn't global.
			/* Ar1->Printf("\n[ForceMeshTranslationBoneNames]\n");
			for (i = 0; i < Anim->ForceMeshTranslation.Num(); i++)
				if (Anim->ForceMeshTranslation[i])
					Ar1->Printf("%s\n", *Anim->TrackBoneNames[i]); */
		}

		if (requireConfig)
		{
			// has removed tracks inside the sequence
			// currently used for Unreal Championship 2 only
			Ar1->Printf("\n[RemoveTracks]\n");
			for (i = 0; i < numAnims; i++)
			{
				const CAnimSequence &S = *Anim->Sequences[i];
				for (int b = 0; b < numBones; b++)
				{
#define FLAG_NO_TRANSLATION		1
#define FLAG_NO_ROTATION		2
					static const char *FlagInfo[] = { "", "trans", "rot", "all" };
					int flag = 0;
					if (S.Tracks[b]->KeyPos.Num() == 0)
						flag |= FLAG_NO_TRANSLATION;
					if (S.Tracks[b]->KeyQuat.Num() == 0)
						flag |= FLAG_NO_ROTATION;
					if (flag)
						Ar1->Printf("%s.%d=%s\n", *S.Name, b, FlagInfo[flag]);
				}
			}
		}

		delete Ar1;
	}

	//todo: .props.txt is not saved when multiple animations are stored in a single .psa file
	if (OriginalAnim->GetTypeinfo()->NumProps)
	{
		FArchive* PropAr = CreateExportArchive(OriginalAnim, EFileArchiveOptions::TextFile, "%s.props.txt", OriginalAnim->Name);
		if (PropAr)
		{
			OriginalAnim->GetTypeinfo()->SaveProps(OriginalAnim, *PropAr);

#if UNREAL4
			//todo: UE3 won't work here
			if (Anim->OriginalAnim->IsA("Skeleton"))
			{
				const USkeleton* Skeleton = static_cast<const USkeleton*>(Anim->OriginalAnim);
				for (const UAnimSequence4* Seq : Skeleton->OriginalAnims)
				{
					PropAr->Printf("\n// Sequence: %s\n", Seq->Name);
					Seq->GetTypeinfo()->SaveProps(Seq, *PropAr);
				}
			}
#endif // UNREAL4

			delete PropAr;
		}
	}

	unguard;
}

void ExportPsa(const CAnimSet* Anim)
{
	if (!Anim->Sequences.Num()) return;			// empty CAnimSet

	// Determine if CAnimSet will save animations as separate psa files, or all at once
	const UObject* OriginalAnim = GetPrimaryAnimObject(Anim);

	if (OriginalAnim == Anim->OriginalAnim || Anim->Sequences.Num() == 1)
	{
		// Export all animations in a single file
		DoExportPsa(Anim, OriginalAnim);
	}
	else
	{
		guard(ExportAnimsByOne);
#if UNREAL4
		assert(Anim->OriginalAnim->IsA("Skeleton"));
		const USkeleton* Skeleton = static_cast<const USkeleton*>(Anim->OriginalAnim);

		// Export animations separately, this will happen only when CAnimSet has
		// a few sequences (but more than one)
		CAnimSet TempAnimSet;
		TempAnimSet.CopyAllButSequences(*Anim);
		// Now we have a copy of AnimSet, let's set up Sequences array to a single
		// item and export one-by-one
		for (int AnimIndex = 0; AnimIndex < Anim->Sequences.Num(); AnimIndex++)
		{
			const CAnimSequence* Seq = Anim->Sequences[AnimIndex];
			const UAnimSequence4* Seq4 = Skeleton->OriginalAnims[AnimIndex];
			assert(strcmp(*Seq->Name, Seq4->Name) == 0);
			TempAnimSet.Sequences.Empty(1);
			TempAnimSet.Sequences.Add(const_cast<CAnimSequence*>(Seq));
			// Do the export, pass UAnimSequence as the "main" object, so it will be
			// used as psa file name.
			DoExportPsa(&TempAnimSet, Seq4);
		}
		// Ensure TempAnimSet destructor will not release Sequences as they are owned by Anim object
		TempAnimSet.Sequences.Empty();
#else
		// Shouldn't happen
		assert(0);
#endif
		unguard;
	}
}


static void ExportStaticMeshLod(const CStaticMeshLod &Lod, FArchive &Ar)
{
	guard(ExportStaticMeshLod);

	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader BoneHdr, InfHdr;

	CVertexShare Share;

	// weld vertices
//	appResetProfiler();
	guard(WeldVerts);
	Share.Prepare(Lod.Verts, Lod.NumVerts, sizeof(CStaticMeshVertex));
	for (int i = 0; i < Lod.NumVerts; i++)
	{
		const CMeshVertex &S = Lod.Verts[i];
		Share.AddVertex(S.Position, S.Normal);
	}
	unguard;
//	appPrintProfiler();
//	appPrintf("%d wedges were welded into %d verts\n", Lod.NumVerts, Share.Points.Num());

	ExportCommonMeshData
	(
		Ar,
		&Lod.Sections[0], Lod.Sections.Num(),
		Lod.Verts, Lod.NumVerts, sizeof(CStaticMeshVertex),
		Lod.Indices,
		Share
	);

	BoneHdr.DataCount = 0;		// dummy ...
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");

	InfHdr.DataCount = 0;		// dummy
	InfHdr.DataSize  = sizeof(VRawBoneInfluence);
	SAVE_CHUNK(InfHdr, "RAWWEIGHTS");

	ExportVertexColors(Ar, Lod.VertexColors, Lod.NumVerts);
	ExportExtraUV(Ar, Lod.ExtraUV, Lod.NumVerts, Lod.NumTexCoords);

	unguard;
}


void ExportStaticMesh(const CStaticMesh *Mesh)
{
	UObject *OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	int MaxLod = (GExportLods) ? Mesh->Lods.Num() : 1;
	for (int Lod = 0; Lod < MaxLod; Lod++)
	{
		guard(Lod);
		if (Mesh->Lods[Lod].Sections.Num() == 0)
		{
			appNotify("Mesh %s Lod %d has no sections\n", OriginalMesh->Name, Lod);
			continue;
		}
		char filename[512];
		if (Lod == 0)
			appSprintf(ARRAY_ARG(filename), "%s.pskx", OriginalMesh->Name);
		else
			appSprintf(ARRAY_ARG(filename), "%s_Lod%d.pskx", OriginalMesh->Name, Lod);

		FArchive *Ar = CreateExportArchive(OriginalMesh, EFileArchiveOptions::Default, "%s", filename);
		if (Ar)
		{
			ExportStaticMeshLod(Mesh->Lods[Lod], *Ar);
			delete Ar;
		}
		else if (Lod == 0)
		{
			// First LOD was failed to be saved, most likely file already exists
			return;
		}

		unguardf("%d", Lod);
	}

	if (OriginalMesh->GetTypeinfo()->NumProps)
	{
		FArchive* PropAr = CreateExportArchive(OriginalMesh, EFileArchiveOptions::TextFile, "%s.props.txt", OriginalMesh->Name);
		if (PropAr)
		{
			OriginalMesh->GetTypeinfo()->SaveProps(OriginalMesh, *PropAr);
			delete PropAr;
		}
	}
 }
