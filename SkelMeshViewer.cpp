#include "ObjectViewer.h"
#include "MeshInstance.h"


CSkelMeshViewer::CSkelMeshViewer(USkeletalMesh *Mesh)
:	CMeshViewer(Mesh)
,	ShowSkel(0)
{
	Inst = new CSkelMeshInstance(Mesh, this);
}


#if TEST_FILES
void CSkelMeshViewer::Test()
{
	int i;

	CMeshViewer::Test();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	int NumBones = Mesh->Bones.Num();
//	TEST_ARRAY(Mesh->CollapseWedge);
//	TEST_ARRAY(Mesh->f1C8);
	VERIFY(AttachBoneNames.Num(), AttachAliases.Num());
	VERIFY(AttachBoneNames.Num(), AttachCoords.Num());
	VERIFY_NULL(WeightIndices.Num());
	VERIFY_NULL(BoneInfluences.Num());
	VERIFY_NOT_NULL(VertInfluences.Num());

	for (i = 0; i < Mesh->StaticLODModels.Num(); i++)
	{
		const FStaticLODModel &lod = Mesh->StaticLODModels[i];
//?? (not always)	if (lod.NumDynWedges != lod.Wedges.Num()) appNotify("lod[%d]: NumDynWedges!=wedges.Num()", i);
		if (lod.SkinPoints.Num() != lod.Points.Num() && lod.RigidSections.Num() == 0)
			appNotify("[%d] skinPoints: %d", i,	lod.SkinPoints.Num());
//		if (lod.SmoothIndices.Indices.Num() + lod.RigidIndices.Indices.Num() != lod.Faces.Num() * 3)
//			appNotify("[%d] strange indices count", i);
//		if ((lod.f0.Num() != 0 || lod.NumDynWedges != 0) &&
//			(lod.f0.Num() != lod.NumDynWedges * 3 + 1)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
		if ((lod.f0.Num() == 0) != (lod.NumDynWedges == 0)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
// (may be empty)	if (lod.VertexStream.Verts.Num() != lod.Wedges.Num()) appNotify("lod%d: bad VertexStream size", i);
		if (lod.f114 || lod.f118) appNotify("[%d]: f114=%d, f118=%d", lod.f114, lod.f118);

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
		if (Anim->f2C) appNotify("Anim.f2C = %d", Anim->f2C);
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
				if (M.BoneIndices[j] != j)
					appNotify("anim[%d]: idx %d != %d", i, j, M.BoneIndices[j]);
		}
	}
}
#endif


void CSkelMeshViewer::Dump()
{
	CMeshViewer::Dump();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	printf(
		"\nSkelMesh info:\n==============\n"
		"f1FC #         %d\n"
		"Bones  # %4d  Points    # %4d\n"
		"Wedges # %4d  Triangles # %4d\n"
		"CollapseWedge # %4d  f1C8      # %4d\n"
		"BoneDepth      %d\n"
		"WeightIds # %d  BoneInfs # %d  VertInfs # %d\n"
		"Attachments #  %d\n"
		"StaticLODModels # %d\n",
		Mesh->f1FC.Num(),
		Mesh->Bones.Num(),
		Mesh->Points.Num(),
		Mesh->Wedges.Num(),Mesh->Triangles.Num(),
		Mesh->CollapseWedge.Num(), Mesh->f1C8.Num(),
		Mesh->BoneDepth,
		Mesh->WeightIndices.Num(), Mesh->BoneInfluences.Num(), Mesh->VertInfluences.Num(),
		Mesh->AttachBoneNames.Num(),
		Mesh->StaticLODModels.Num()
	);

	int i;

	// check bone sort order (assumed, that child go after parent)
	for (i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B = Mesh->Bones[i];
		if (B.ParentIndex >= i + 1) appNotify("bone[%d] has parent %d", i+1, B.ParentIndex);
	}

	for (i = 0; i < Mesh->StaticLODModels.Num(); i++)
	{
		printf("model # %d\n", i);
		const FStaticLODModel &lod = Mesh->StaticLODModels[i];
		printf(
			"  f0=%d  SkinPoints=%d inf=%d  wedg=%d dynWedges=%d faces=%d  points=%d\n"
			"  DistanceFactor=%g  Hysteresis=%g  SharedVerts=%d  MaxInfluences=%d  114=%d  118=%d\n"
			"  smoothInds=%d  rigidInds=%d  vertStream.Size=%d\n",
			lod.f0.Num(),
			lod.SkinPoints.Num(),
			lod.VertInfluences.Num(),
			lod.Wedges.Num(), lod.NumDynWedges,
			lod.Faces.Num(),
			lod.Points.Num(),
			lod.LODDistanceFactor, lod.LODHysteresis, lod.NumSharedVerts, lod.LODMaxInfluences, lod.f114, lod.f118,
			lod.SmoothIndices.Indices.Num(), lod.RigidIndices.Indices.Num(), lod.VertexStream.Verts.Num());

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
				printf("    %d:  mat=%d %d [w=%d .. %d] %d b=%d %d [f=%d + %d]\n", j,
					S.MaterialIndex, S.MinStreamIndex, S.MinWedgeIndex, S.MaxWedgeIndex,
					S.NumStreamIndices, S.BoneIndex, S.fE, S.FirstFace, S.NumFaces);
			}
		}
	}

#if 0
	if (Mesh->Animation)
	{
		UMeshAnimation *Anim = Mesh->Animation;
		printf("\nAnimation: %s\n", Anim->Name);
		printf("f2C: %d  Moves # %d", Anim->f2C, Anim->Moves.Num());
		for (i = 0; i < Anim->AnimSeqs.Num(); i++)
		{
			const FMeshAnimSeq &S = Anim->AnimSeqs[i];
			const MotionChunk  &M = Anim->Moves[i];
			printf("[%d] %s  %d:%d; Bones=%d:%d tracks=%d\n", i,
				*S.Name, S.StartFrame, S.NumFrames,
				M.StartBone, M.BoneIndices.Num(), M.AnimTracks.Num());
		}
	}
#endif
}


void CSkelMeshViewer::Draw2D()
{
	CMeshViewer::Draw2D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	int NumAnims = 0;
	if (Mesh->Animation)
		NumAnims = Mesh->Animation->AnimSeqs.Num();
	const char *AnimName = MeshInst->CurrAnim < 0 ? "default" : *Mesh->Animation->AnimSeqs[MeshInst->CurrAnim].Name;
	float AnimRate       = MeshInst->CurrAnim < 0 ? 0         :  Mesh->Animation->AnimSeqs[MeshInst->CurrAnim].Rate;
	int   AnimFrames     = MeshInst->CurrAnim < 0 ? 0         :  Mesh->Animation->AnimSeqs[MeshInst->CurrAnim].NumFrames;

	if (MeshInst->LodNum < 0)
		GL::textf("LOD : base mesh\n");
	else
		GL::textf("LOD : %d/%d\n", MeshInst->LodNum+1, Mesh->StaticLODModels.Num());
	GL::textf("Anim: %d/%d (%s) rate: %g frames: %d\n", MeshInst->CurrAnim+1, NumAnims, AnimName, AnimRate, AnimFrames);
	GL::textf("Time: %.1f/%d\n", MeshInst->AnimTime, AnimFrames);
#if 0
	if (CurrAnim >= 0)
	{
		// display bone tracks info for selected animation
		for (int i = 0; i < Mesh->Bones.Num(); i++)
		{
			const AnalogTrack &A = Mesh->Animation->Moves[CurrAnim].AnimTracks[i];
			GL::textf("%2d: %-20s: q[%d] p[%d] t[%d](%g .. %g)\n", i, *Mesh->Bones[i].Name,
				A.KeyQuat.Num(), A.KeyPos.Num(), A.KeyTime.Num(), A.KeyTime[0], A.KeyTime[A.KeyTime.Num()-1]);
		}
	}
#endif
}


void CSkelMeshViewer::ProcessKey(unsigned char key)
{
	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	int NumAnims = 0, NumFrames = 0;
	if (Mesh->Animation)
		NumAnims  = Mesh->Animation->AnimSeqs.Num();
	if (MeshInst->CurrAnim >= 0)
		NumFrames = Mesh->Animation->AnimSeqs[MeshInst->CurrAnim].NumFrames;

	switch (key)
	{
	case 'l':
		if (++MeshInst->LodNum >= Mesh->StaticLODModels.Num())
			MeshInst->LodNum = -1;
		break;
	case '[':
		if (--MeshInst->CurrAnim < -1)
			MeshInst->CurrAnim = NumAnims - 1;
		MeshInst->AnimTime = 0;
		break;
	case ']':
		if (++MeshInst->CurrAnim >= NumAnims)
			MeshInst->CurrAnim = -1;
		MeshInst->AnimTime = 0;
		break;
	case ',':		// '<'
		MeshInst->AnimTime -= 0.2;
		if (MeshInst->AnimTime < 0)
			MeshInst->AnimTime = 0;
		break;
	case '.':		// '>'
		if (NumFrames > 0)
		{
			MeshInst->AnimTime += 0.2;
			if (MeshInst->AnimTime > NumFrames - 1)
				MeshInst->AnimTime = NumFrames - 1;
		}
		break;
	case 's':
		if (++ShowSkel > 2)
			ShowSkel = 0;
		break;
#if 1
	//!! REMOVE
	case 'a':
		if (MeshInst->LodNum >= 0)
		{
			const TArray<unsigned>& A = Mesh->StaticLODModels[MeshInst->LodNum].f0;
			for (int i = 0; i < A.Num(); i++)
				printf("%-4d %08X  /  %d  /  %g\n", i, A[i], A[i], (float&)A[i]);
		}
		break;
#endif
	default:
		CMeshViewer::ProcessKey(key);
	}
}
