#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"
#include "UnMathTools.h"
#include "TypeConvert.h"

#include "UnMesh.h"			//??
#include "UnMesh2.h"		// for UMeshAnimation
#include "UnMesh3.h"		// for UAnimSet
#include "SkeletalMesh.h"

#include "Exporters/Exporters.h"


#define TEST_ANIMS			1


CSkelMeshViewer::CSkelMeshViewer(USkeletalMesh *Mesh)
:	CLodMeshViewer(Mesh)
{
	CSkelMeshInstance *SkelInst = new CSkelMeshInstance();
	SkelInst->SetMesh(Mesh);
	if (Mesh->Animation)		//?? UE2 only
		SkelInst->SetAnim(Mesh->Animation->ConvertedAnim);
	Inst = SkelInst;
#if 0
	Initialize();
#else
	// compute bounds for the current mesh
	CVec3 Mins, Maxs;
	ComputeBounds(&CVT(Mesh->Points[0]), Mesh->Points.Num(), sizeof(FVector), Mins, Maxs);
	SkelInst->BaseTransformScaled.TransformPointSlow(Mins, Mins);
	SkelInst->BaseTransformScaled.TransformPointSlow(Maxs, Maxs);
	// extend bounds with additional meshes
	for (int i = 0; i < Meshes.Num(); i++)
	{
		CLodMeshInstance* Inst = Meshes[i];
		if ((Inst->pMesh != SkelInst->pMesh) && Inst->pMesh->IsA("SkeletalMesh"))
		{
			CSkelMeshInstance *SkelInst2 = static_cast<CSkelMeshInstance*>(Inst);
			const USkeletalMesh *Mesh2 = static_cast<const USkeletalMesh*>(SkelInst2->pMesh);
			// the same code for SkelInst2
			CVec3 Bounds2[2];
			ComputeBounds(&CVT(Mesh2->Points[0]), Mesh2->Points.Num(), sizeof(FVector), Bounds2[0], Bounds2[1]);
			SkelInst2->BaseTransformScaled.TransformPointSlow(Bounds2[0], Bounds2[0]);
			SkelInst2->BaseTransformScaled.TransformPointSlow(Bounds2[1], Bounds2[1]);
			ComputeBounds(Bounds2, 2, sizeof(CVec3), Mins, Maxs, true);	// include Bounds2 into Mins/Maxs
		}
	}
	InitViewerPosition(Mins, Maxs);
#endif
}


#if TEST_FILES
void CSkelMeshViewer::Test()
{
	int i;

	CLodMeshViewer::Test();

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
//		if ((lod.SkinningData.Num() == 0) != (lod.NumDynWedges == 0))
//			appNotify("SkinningData=%d  NumDynWedges=%d",lod.SkinningData.Num(), lod.NumDynWedges);
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
}
#endif


void CSkelMeshViewer::Dump()
{
	CLodMeshViewer::Dump();

	const USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	appPrintf(
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
		appPrintf("model # %d\n", i);
		const FStaticLODModel &lod = Mesh->LODModels[i];
		appPrintf(
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
		appPrintf("  smoothIndices: [%d .. %d]\n", i0, i1);
		i0 = 99999999; i1 = -99999999;
		for (j = 0; j < lod.RigidIndices.Indices.Num(); j++)
		{
			int x = lod.RigidIndices.Indices[j];
			if (x < i0) i0 = x;
			if (x > i1) i1 = x;
		}
		appPrintf("  rigidIndices:  [%d .. %d]\n", i0, i1);

		const TArray<FSkelMeshSection> *sec[2];
		sec[0] = &lod.SmoothSections;
		sec[1] = &lod.RigidSections;
		static const char *secNames[] = { "smooth", "rigid" };
		for (int k = 0; k < 2; k++)
		{
			appPrintf("  %s sections: %d\n", secNames[k], sec[k]->Num());
			for (int j = 0; j < sec[k]->Num(); j++)
			{
				const FSkelMeshSection &S = (*sec[k])[j];
				appPrintf("    %d:  mat=%d %d [w=%d .. %d] %d b=%d %d [f=%d + %d]\n", j,
					S.MaterialIndex, S.MinStreamIndex, S.MinWedgeIndex, S.MaxWedgeIndex,
					S.NumStreamIndices, S.BoneIndex, S.fE, S.FirstFace, S.NumFaces);
			}
		}
	}
}


void CSkelMeshViewer::Export()
{
	CLodMeshViewer::Export();
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	const CAnimSet *Anim = MeshInst->GetAnim();
	if (Anim) ExportObject(Anim->OriginalAnim);
}


void CSkelMeshViewer::Draw2D()
{
	CLodMeshViewer::Draw2D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	if (MeshInst->LodNum < 0)
	{
		DrawTextLeft(S_GREEN"LOD  : "S_WHITE"base mesh\n"
					 S_GREEN"Verts: "S_WHITE"%d (%d wedges)\n"
					 S_GREEN"Tris : "S_WHITE"%d\n"
					 S_GREEN"Bones: "S_WHITE"%d",
					 Mesh->Points.Num(), Mesh->Wedges.Num(), Mesh->Triangles.Num(),
					 Mesh->RefSkeleton.Num());
	}
	else
	{
		const FStaticLODModel *Lod = &Mesh->LODModels[MeshInst->LodNum];
		DrawTextLeft(S_GREEN"LOD  : "S_WHITE"%d/%d\n"
					 S_GREEN"Verts: "S_WHITE"%d (%d wedges)\n"
					 S_GREEN"Tris : "S_WHITE"%d\n"
					 S_GREEN"Bones: "S_WHITE"%d",
					 MeshInst->LodNum+1, Mesh->LODModels.Num(),
					 Lod->Points.Num(), Lod->Wedges.Num(), Lod->Faces.Num(),
					 Mesh->RefSkeleton.Num());
	}
}


void CSkelMeshViewer::ShowHelp()
{
	CLodMeshViewer::ShowHelp();
	DrawKeyHelp("L",      "cycle mesh LODs");
	DrawKeyHelp("S",      "show skeleton");
	DrawKeyHelp("B",      "show bone names");
	DrawKeyHelp("I",      "show influences");
	DrawKeyHelp("A",      "show attach sockets");
	DrawKeyHelp("Ctrl+B", "dump skeleton to console");
	DrawKeyHelp("Ctrl+A", "cycle mesh animation sets");
	DrawKeyHelp("Ctrl+T", "tag/untag mesh");
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
	case 'i':
		MeshInst->ShowInfluences = !MeshInst->ShowInfluences;
		break;
	case 'b'|KEY_CTRL:
		MeshInst->DumpBones();
		break;

#if TEST_ANIMS
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
	case 'y'|KEY_CTRL:
		MeshInst->SetBlendParams(0, 1.0f, "b_MF_Forearm_R");
//		MeshInst->SetBlendParams(0, 1.0f, "b_MF_Hand_R");
//		MeshInst->SetBlendParams(0, 1.0f, "b_MF_ForeTwist2_R");
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
		break;
#endif // TEST_ANIMS

	case 'a'|KEY_CTRL:
		{
			const CAnimSet *PrevAnim = MeshInst->GetAnim();
			// find next animation set (code is similar to PAGEDOWN handler)
			int looped = 0;
			int ObjIndex = -1;
			bool found = (PrevAnim == NULL);			// whether previous AnimSet was found; NULL -> any
			while (true)
			{
				ObjIndex++;
				if (ObjIndex >= UObject::GObjObjects.Num())
				{
					ObjIndex = 0;
					looped++;
					if (looped > 1) break;				// no other objects
				}
				const UObject *Obj = UObject::GObjObjects[ObjIndex];
				const CAnimSet *Anim = NULL;
				if (Obj->IsA("MeshAnimation"))			// UE1,UE2
					Anim = static_cast<const UMeshAnimation*>(Obj)->ConvertedAnim;
				else if (Obj->IsA("AnimSet"))			// UE3
					Anim = static_cast<const UAnimSet*>(Obj)->ConvertedAnim;
				else
					continue;

				if (Anim == PrevAnim)
				{
					if (found) break;					// loop detected
					found = true;
					continue;
				}

				if (found && Anim)
				{
					// found desired animation set
					MeshInst->SetAnim(Anim);			// will rebind mesh to new animation set
					for (int i = 0; i < Meshes.Num(); i++)
					{
						CLodMeshInstance* Inst = Meshes[i];
						if (Inst->pMesh->IsA("SkeletalMesh"))
						{
							CSkelMeshInstance *SkelInst = static_cast<CSkelMeshInstance*>(Inst);
							SkelInst->SetAnim(Anim);
						}
					}
					AnimIndex = -1;
					appPrintf("Bound %s'%s' to %s'%s'\n", Mesh->GetClassName(), Mesh->Name, Obj->GetClassName(), Obj->Name);
					break;
				}
			}
		}
		break;

	case 't'|KEY_CTRL:
		{
			CSkelMeshInstance *SkelInst = new CSkelMeshInstance();
			SkelInst->SetMesh(MeshInst->pMesh);
			TagMesh(SkelInst);
		}
		break;

	default:
		CLodMeshViewer::ProcessKey(key);
	}

	unguard;
}

#endif // RENDERING
