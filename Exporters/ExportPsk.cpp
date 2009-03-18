#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMesh.h"

#include "Psk.h"
#include "Exporters.h"


// PSK uses right-hand coordinates, but unreal uses left-hand.
// When importing PSK into UnrealEd, it mirrors model.
// Here we performing reverse transformation.
#define MIRROR_MESH			1

void GetBonePosition(const AnalogTrack &A, float Frame, float NumFrames, bool Loop,
	CVec3 &DstPos, CQuat &DstQuat);


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


void ExportPsk(const USkeletalMesh *Mesh, FArchive &Ar)
{
	// export script file
	char filename[64];
	if (GExportScripts)
	{
		appSprintf(ARRAY_ARG(filename), "%s.uc", Mesh->Name);
		FFileReader Ar1(filename, false);
		ExportScript(Mesh, Ar1);
	}
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

	TArray<int> WedgeMat;
	WedgeMat.Empty(Mesh->Wedges.Num());
	WedgeMat.Add(Mesh->Wedges.Num());
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &T = Mesh->Triangles[i];
		WedgeMat[T.WedgeIndex[0]] = T.MatIndex;
		WedgeMat[T.WedgeIndex[1]] = T.MatIndex;
		WedgeMat[T.WedgeIndex[2]] = T.MatIndex;
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

	MatrHdr.DataCount = Mesh->Materials.Num();
	MatrHdr.DataSize  = sizeof(VMaterial);
	SAVE_CHUNK(MatrHdr, "MATT0000");
	for (i = 0; i < Mesh->Materials.Num(); i++)
	{
		VMaterial M;
		memset(&M, 0, sizeof(M));
		appSprintf(ARRAY_ARG(M.MaterialName), "material_%d", i);
		Ar << M;
	}

	BoneHdr.DataCount = Mesh->RefSkeleton.Num();
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");
	for (i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
		VBone B;
		memset(&B, 0, sizeof(B));
		const FMeshBone &S = Mesh->RefSkeleton[i];
		strcpy(B.Name, S.Name);
		B.NumChildren = S.NumChildren;
		B.ParentIndex = S.ParentIndex;
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
		I.BoneIndex  = S.BoneIndex;
		Ar << I;
	}
}


void ExportPsa(const UMeshAnimation *Anim, FArchive &Ar)
{
	// using 'static' here to avoid zero-filling unused fields
	static VChunkHeader MainHdr, BoneHdr, AnimHdr, KeyHdr;
	int i;

	int numBones = Anim->RefBones.Num();
	int numAnims = Anim->AnimSeqs.Num();

	SAVE_CHUNK(MainHdr, "ANIMHEAD");

	BoneHdr.DataCount = numBones;
	BoneHdr.DataSize  = sizeof(FNamedBoneBinary);
	SAVE_CHUNK(BoneHdr, "BONENAMES");
	for (i = 0; i < numBones; i++)
	{
		FNamedBoneBinary B;
		memset(&B, 0, sizeof(B));
		const FNamedBone &S = Anim->RefBones[i];
		strcpy(B.Name, *S.Name);
		B.Flags       = S.Flags;		// reserved, but copy ...
		B.NumChildren = 0;				// unknown here
		B.ParentIndex = S.ParentIndex;
//		B.BonePos     =					// unknown here
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
		const FMeshAnimSeq &S = Anim->AnimSeqs[i];
		strcpy(A.Name,  *S.Name);
		strcpy(A.Group, S.Groups.Num() ? *S.Groups[0] : "None");
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
	for (i = 0; i < numAnims; i++)
	{
		const FMeshAnimSeq &S = Anim->AnimSeqs[i];
		const MotionChunk  &M = Anim->Moves[i];
		for (int t = 0; t < S.NumFrames; t++)
		{
			for (int b = 0; b < numBones; b++)
			{
				VQuatAnimKey K;
				CVec3 BP;
				CQuat BO;
				GetBonePosition(M.AnimTracks[b], t, S.NumFrames, false, BP, BO);
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
			}
		}
	}
	assert(keysCount == 0);
}
