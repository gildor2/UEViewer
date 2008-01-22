#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMesh.h"

#include "Psk.h"


// PSK uses right-hand coordinates, but unreal uses left-hand.
// When importing PSK into UnrealEd, it mirrors model.
// Here we performing reverse transformation.
#define MIRROR_MESH			1


void ExportPsk(USkeletalMesh *Mesh, FArchive &Ar)
{
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
		W.MatIndex   = 0;
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
		static VMaterial M;
		Ar << M;
	}

	BoneHdr.DataCount = Mesh->RefSkeleton.Num();
	BoneHdr.DataSize  = sizeof(VBone);
	SAVE_CHUNK(BoneHdr, "REFSKELT");
	for (i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
		VBone B;
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
