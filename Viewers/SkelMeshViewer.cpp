#include "Core.h"
#include "UnrealClasses.h"

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"


CSkelMeshViewer::CSkelMeshViewer(USkeletalMesh *Mesh)
:	CMeshViewer(Mesh)
{
	Inst = new CSkelMeshInstance();
	Inst->SetMesh(Mesh);
#if 0
	CSkelMeshInstance* Inst2 = (CSkelMeshInstance*)Inst;
	Inst2->SetBoneScale("Bip01 Pelvis", 1.4);
//	Inst2->SetBoneScale("Bip01 Spine1", 2);
	Inst2->SetBoneScale("Bip01 Head", 0.8);
	Inst2->SetBoneScale("Bip01 L UpperArm", 1.2);
	Inst2->SetBoneScale("Bip01 R UpperArm", 1.2);
	Inst2->SetBoneScale("Bip01 R Thigh", 0.4);
	Inst2->SetBoneScale("Bip01 L Thigh", 0.4);
#endif
}


#if TEST_FILES
void CSkelMeshViewer::Test()
{
	int i;

	CMeshViewer::Test();

	const USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);

	// ULodMesh fields
	if (Mesh->Version > 1)
	{
		VERIFY_NULL(Faces.Num());
	}
	VERIFY_NULL(Verts.Num());

	int NumBones = Mesh->RefSkeleton.Num();
//	TEST_ARRAY(Mesh->CollapseWedge);
//	TEST_ARRAY(Mesh->f1C8);
	VERIFY(AttachBoneNames.Num(), AttachAliases.Num());
	VERIFY(AttachBoneNames.Num(), AttachCoords.Num());
	if (Mesh->Version > 1)
	{
		VERIFY_NULL(WeightIndices.Num());
		VERIFY_NULL(BoneInfluences.Num());
	}
//	VERIFY_NOT_NULL(VertInfluences.Num());
//	VERIFY_NOT_NULL(Wedges.Num());

	for (i = 0; i < Mesh->LODModels.Num(); i++)
	{
		const FStaticLODModel &lod = Mesh->LODModels[i];
//?? (not always)	if (lod.NumDynWedges != lod.Wedges.Num()) appNotify("lod[%d]: NumDynWedges!=wedges.Num()", i);
//		if (lod.SkinPoints.Num() != lod.Points.Num() && lod.RigidSections.Num() == 0)
//			appNotify("[%d] skinPoints: %d", i,	lod.SkinPoints.Num());
//		if (lod.SmoothIndices.Indices.Num() + lod.RigidIndices.Indices.Num() != lod.Faces.Num() * 3)
//			appNotify("[%d] strange indices count", i);
//		if ((lod.f0.Num() != 0 || lod.NumDynWedges != 0) &&
//			(lod.f0.Num() != lod.NumDynWedges * 3 + 1)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
		if ((lod.SkinningData.Num() == 0) != (lod.NumDynWedges == 0))
			appNotify("SkinningData=%d  NumDynWedges=%d",lod.SkinningData.Num(), lod.NumDynWedges);
// (may be empty)	if (lod.VertexStream.Verts.Num() != lod.Wedges.Num()) appNotify("lod%d: bad VertexStream size", i);
//		if (lod.f114 || lod.f118) appNotify("[%d]: f114=%d, f118=%d", lod.f114, lod.f118);

		const TArray<FSkelMeshSection> *sec[2];
		sec[0] = &lod.SmoothSections;
		sec[1] = &lod.RigidSections;
		for (int k = 0; k < 2; k++)
		{
			for (int j = 0; j < sec[k]->Num(); j++)
			{
				const FSkelMeshSection &S = (*sec[k])[j];
				if (k == 1) // rigid sections
				{
					if (S.BoneIndex >= NumBones)
						appNotify("rigid sec[%d,%d]: bad bone link (%d >= %d)", i, j, S.BoneIndex, NumBones);
					if (S.MinStreamIndex + S.NumStreamIndices > lod.RigidIndices.Indices.Num())
						appNotify("rigid sec[%d,%d]: out of index buffer", i, j);
					if (S.NumFaces * 3 != S.NumStreamIndices)
						appNotify("rigid sec[%d,%d]: f8!=NumFaces*3", i, j);
					if (S.fE != 0)
						appNotify("rigid sec[%d,%d]: fE=%d", i, j, S.fE);
				}
			}
		}
	}

	if (Mesh->Animation)
	{
		UMeshAnimation *Anim = Mesh->Animation;
#ifndef SPLINTER_CELL
		if (Anim->Version) appNotify("Anim.Version = %d", Anim->Version);
#endif
		if (Anim->Moves.Num() != Anim->AnimSeqs.Num())
			appNotify("Moves.Num=%d  !=  AnimSeqs.Num=%d", Anim->Moves.Num(), Anim->AnimSeqs.Num());
		for (i = 0; i < Anim->AnimSeqs.Num(); i++)
		{
			const FMeshAnimSeq &S = Anim->AnimSeqs[i];
			if (S.StartFrame != 0) appNotify("Anim[%d=%s]: StartFrame=%d", i, *S.Name, S.StartFrame);
		}
		for (i = 0; i < Anim->Moves.Num(); i++)
		{
			const MotionChunk &M = Anim->Moves[i];
			for (int j = 0; j < M.BoneIndices.Num(); j++)
				if (M.BoneIndices[j] != j && M.BoneIndices[j] != INDEX_NONE)
					appNotify("anim[%d]: idx %d != %d", i, j, M.BoneIndices[j]);
		}
	}
}
#endif


void CSkelMeshViewer::Dump()
{
	CMeshViewer::Dump();

	const USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	printf(
		"\nSkelMesh info:\n==============\n"
		"Bones  # %4d  Points    # %4d  Points2  # %4d\n"
		"Wedges # %4d  Triangles # %4d\n"
		"CollapseWedge # %4d  f1C8      # %4d\n"
		"BoneDepth      %d\n"
		"WeightIds # %d  BoneInfs # %d  VertInfs # %d\n"
		"Attachments #  %d\n"
		"LODModels # %d\n"
		"Animations: %s\n",
		Mesh->RefSkeleton.Num(),
		Mesh->Points.Num(), Mesh->Points2.Num(),
		Mesh->Wedges.Num(),Mesh->Triangles.Num(),
		Mesh->CollapseWedge.Num(), Mesh->f1C8.Num(),
		Mesh->SkeletalDepth,
		Mesh->WeightIndices.Num(), Mesh->BoneInfluences.Num(), Mesh->VertInfluences.Num(),
		Mesh->AttachBoneNames.Num(),
		Mesh->LODModels.Num(),
		Mesh->Animation ? Mesh->Animation->Name : "None"
	);

	int i;

	// check bone sort order (assumed, that child go after parent)
	for (i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
		const FMeshBone &B = Mesh->RefSkeleton[i];
		if (B.ParentIndex >= i + 1) appNotify("bone[%d] has parent %d", i+1, B.ParentIndex);
	}

	for (i = 0; i < Mesh->LODModels.Num(); i++)
	{
		printf("model # %d\n", i);
		const FStaticLODModel &lod = Mesh->LODModels[i];
		printf(
			"  SkinningData=%d  SkinPoints=%d inf=%d  wedg=%d dynWedges=%d faces=%d  points=%d\n"
			"  DistanceFactor=%g  Hysteresis=%g  SharedVerts=%d  MaxInfluences=%d  114=%d  118=%d\n"
			"  smoothInds=%d  rigidInds=%d  vertStream.Size=%d\n",
			lod.SkinningData.Num(),
			lod.SkinPoints.Num(),
			lod.VertInfluences.Num(),
			lod.Wedges.Num(), lod.NumDynWedges,
			lod.Faces.Num(),
			lod.Points.Num(),
			lod.LODDistanceFactor, lod.LODHysteresis, lod.NumSharedVerts, lod.LODMaxInfluences, lod.f114, lod.f118,
			lod.SmoothIndices.Indices.Num(), lod.RigidIndices.Indices.Num(), lod.VertexStream.Verts.Num());

		int i0 = 99999999, i1 = -99999999;
		int j;
		for (j = 0; j < lod.SmoothIndices.Indices.Num(); j++)
		{
			int x = lod.SmoothIndices.Indices[j];
			if (x < i0) i0 = x;
			if (x > i1) i1 = x;
		}
		printf("  smoothIndices: [%d .. %d]\n", i0, i1);
		i0 = 99999999; i1 = -99999999;
		for (j = 0; j < lod.RigidIndices.Indices.Num(); j++)
		{
			int x = lod.RigidIndices.Indices[j];
			if (x < i0) i0 = x;
			if (x > i1) i1 = x;
		}
		printf("  rigidIndices:  [%d .. %d]\n", i0, i1);

		const TArray<FSkelMeshSection> *sec[2];
		sec[0] = &lod.SmoothSections;
		sec[1] = &lod.RigidSections;
		static const char *secNames[] = { "smooth", "rigid" };
		for (int k = 0; k < 2; k++)
		{
			printf("  %s sections: %d\n", secNames[k], sec[k]->Num());
			for (int j = 0; j < sec[k]->Num(); j++)
			{
				const FSkelMeshSection &S = (*sec[k])[j];
				printf("    %d:  mat=%d %d [w=%d .. %d] %d b=%d %d [f=%d + %d] b#%d\n", j,
					S.MaterialIndex, S.MinStreamIndex, S.MinWedgeIndex, S.MaxWedgeIndex,
					S.NumStreamIndices, S.BoneIndex, S.fE, S.FirstFace, S.NumFaces);
			}
		}
	}
}


void CSkelMeshViewer::Draw2D()
{
	CMeshViewer::Draw2D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	if (MeshInst->LodNum < 0)
	{
		DrawTextLeft(S_GREEN"LOD  : "S_WHITE"base mesh\n"
					 S_GREEN"Verts: "S_WHITE"%d (%d wedges)\n"
					 S_GREEN"Tris : "S_WHITE"%d",
					 Mesh->Points.Num(), Mesh->Wedges.Num(), Mesh->Triangles.Num());
	}
	else
	{
		const FStaticLODModel *Lod = &Mesh->LODModels[MeshInst->LodNum];
		int NumFaces = (Lod->SmoothIndices.Indices.Num() + Lod->RigidIndices.Indices.Num()) / 3;
		DrawTextLeft(S_GREEN"LOD  : "S_WHITE"%d/%d\n"
					 S_GREEN"Verts: "S_WHITE"%d (%d wedges)\n"
					 S_GREEN"Tris : "S_WHITE"%d",
					 MeshInst->LodNum+1, Mesh->LODModels.Num(),
					 Lod->Points.Num(), Lod->Wedges.Num(), NumFaces);
	}
}


void CSkelMeshViewer::ShowHelp()
{
	CMeshViewer::ShowHelp();
	DrawTextLeft("L           cycle mesh LODs\n"
				 "S           show skeleton\n"
				 "B           show bone names\n"
				 "A           show attach sockets\n"
				 "Ctrl+B      dump skeleton to console");
}


void CSkelMeshViewer::ProcessKey(int key)
{
	guard(CSkelMeshViewer::ProcessKey);
	static float Alpha = -1.0f; //!!!!

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	switch (key)
	{
	case 'l':
		if (++MeshInst->LodNum >= Mesh->LODModels.Num())
			MeshInst->LodNum = -1;
		break;
	case 's':
		if (++MeshInst->ShowSkel > 2)
			MeshInst->ShowSkel = 0;
		break;
	case 'b':
		MeshInst->ShowLabels = !MeshInst->ShowLabels;
		break;
	case 'a':
		MeshInst->ShowAttach = !MeshInst->ShowAttach;
		break;
	case 'b'|KEY_CTRL:
		MeshInst->DumpBones();
		break;

	//!! testing, remove later
	case 'y':
		if (Alpha >= 0)
		{
			Alpha = -1;
			MeshInst->ClearSkelAnims();
			return;
		}
		Alpha = 0;
		MeshInst->LoopAnim("CrouchF", 1, 0, 2);
//		MeshInst->LoopAnim("WalkF", 1, 0, 2);
//		MeshInst->LoopAnim("RunR", 1, 0, 2);
//		MeshInst->LoopAnim("SwimF", 1, 0, 2);
		MeshInst->SetSecondaryAnim(2, "RunF");
		break;
	case 'c':
		Alpha -= 0.02;
		if (Alpha < 0) Alpha = 0;
		MeshInst->SetSecondaryBlend(2, Alpha);
		break;
	case 'v':
		Alpha += 0.02;
		if (Alpha > 1) Alpha = 1;
		MeshInst->SetSecondaryBlend(2, Alpha);
		break;

	case 'u':
		MeshInst->LoopAnim("Gesture_Taunt02", 1, 0, 1);
		MeshInst->SetBlendParams(1, 1.0f, "Bip01 Spine1");
		MeshInst->LoopAnim("WalkF", 1, 0, 2);
		MeshInst->SetBlendParams(2, 1.0f, "Bip01 R Thigh");
		MeshInst->LoopAnim("AssSmack", 1, 0, 4);
		MeshInst->SetBlendParams(4, 1.0f, "Bip01 L UpperArm");

	default:
		CMeshViewer::ProcessKey(key);
	}

	unguard;
}
