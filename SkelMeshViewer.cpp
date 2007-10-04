#include "ObjectViewer.h"


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

	for (i = 0; i < Mesh->StaticLODModels.Num(); i++)
	{
		const FStaticLODModel &lod = Mesh->StaticLODModels[i];
//?? (not always)	if (lod.NumDynWedges != lod.Wedges.Num()) appNotify("lod[%d]: NumDynWedges!=wedges.Num()", i);
		if (lod.SkinPoints.Num() != lod.Points.Num() && lod.RigidSections.Num() == 0)
			appNotify("[%d] skinPoints: %d", i,	lod.SkinPoints.Num());
//		if ((lod.f0.Num() != 0 || lod.NumDynWedges != 0) &&
//			(lod.f0.Num() != lod.NumDynWedges * 3 + 1)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
		if ((lod.f0.Num() == 0) != (lod.NumDynWedges == 0)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
// (may be empty)	if (lod.VertexStream.Verts.Num() != lod.Wedges.Num()) appNotify("lod%d: bad VertexStream size", i);

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
		if (Anim->Moves.Num() != Anim->AnimSeqs.Num())
			appNotify("Moves.Num=%d  !=  AnimSeqs.Num=%d", Anim->Moves.Num(), Anim->AnimSeqs.Num());
		for (i = 0; i < Anim->AnimSeqs.Num(); i++)
		{
			const FMeshAnimSeq &S = Anim->AnimSeqs[i];
			if (S.StartFrame != 0) appNotify("Anim[%d=%s]: StartFrame=%d", i, *S.Name, S.StartFrame);
		}
		// check bone count
		if (Anim->RefBones.Num() != NumBones)
			appNotify("AnimBones != MeshBones");
		// compare mesh and animation bones
		for (i = 0; i < Anim->RefBones.Num(); i++)
		{
			const FMeshBone  &MB = Mesh->Bones[i];
			const FNamedBone &AB = Anim->RefBones[i];
			if (strcmp(AB.Name, MB.Name) != 0)
				appNotify("bone[%d]: different names: %s / %s", i, *AB.Name, *MB.Name);
/* -- bone parents may be different, should use parent from mesh (not from animation)
			if (AB.ParentIndex != MB.ParentIndex)
				appNotify("bone[%d,%s]: different parent: %d / %d", i, *AB.Name, AB.ParentIndex, MB.ParentIndex);
*/
		}
		for (i = 0; i < Anim->Moves.Num(); i++)
		{
			const MotionChunk &M = Anim->Moves[i];
			if (M.AnimTracks.Num() != NumBones)
				appNotify("anim[%d]: AnalogTracks=%d Bones=%d", i, M.AnimTracks.Num(), NumBones);
/*			int NumFrames = Anim->AnimSeqs[i].NumFrames;
			for (int j = 0; j < M.AnimTracks.Num(); j++)
			{
				const AnalogTrack &A = M.AnimTracks[j];
			} */
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
		"WeightIds #    %d\n"
		"BoneInfs # %d 	VertInfs # %d\n"
		"Attachments #  %d\n"
		"StaticLODModels # %d\n",
		Mesh->f1FC.Num(),
		Mesh->Bones.Num(),
		Mesh->Points.Num(),
		Mesh->Wedges.Num(),Mesh->Triangles.Num(),
		Mesh->CollapseWedge.Num(), Mesh->f1C8.Num(),
		Mesh->BoneDepth,
		Mesh->WeightIndices.Num(),
		Mesh->BoneInfluences.Num(),
		Mesh->VertInfluences.Num(),
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
			"  DistanceFactor=%g  Hysteresis=%g  10C=%d  MaxInfluences=%d  114=%d  118=%d\n"
			"  smoothInds=%d  rigidInds=%d  vertStream.Size=%d\n",
			lod.f0.Num(),
			lod.SkinPoints.Num(),
			lod.VertInfluences.Num(),
			lod.Wedges.Num(), lod.NumDynWedges,
			lod.Faces.Num(),
			lod.Points.Num(),
			lod.LODDistanceFactor, lod.LODHysteresis, lod.f10C, lod.LODMaxInfluences, lod.f114, lod.f118,
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
}


//!!

void GetBonePosition(const AnalogTrack &A, float time, CVec3 &DstPos, CQuat &DstQuat)
{
	int i;
	float frac;

	// fast case: 1 frame only
	if (A.KeyTime.Num() == 1)
	{
		DstPos  = (CVec3&)A.KeyPos[0];
		DstQuat = (CQuat&)A.KeyQuat[0];
		return;
	}

	// find index in time key array
	//!! linear search -> binary search
	for (i = 0; i < A.KeyTime.Num(); i++)
	{
		if (time == A.KeyTime[i])
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? (CVec3&)A.KeyPos[i]  : (CVec3&)A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? (CQuat&)A.KeyQuat[i] : (CQuat&)A.KeyQuat[0];
			return;
		}
		if (time < A.KeyTime[i])
		{
			i--;
			break;
		}
	}
	//?? clamp time, should wrap
	if (i >= A.KeyTime.Num() - 1)
	{
		i = A.KeyTime.Num() - 1;
		frac = 0;
	}
	else
	{
		//!! when last frame, looping should use NumFrames instead of 2nd time
		frac = (time - A.KeyTime[i]) / (A.KeyTime[i+1] - A.KeyTime[i]);
	}
	// get position
	if (A.KeyPos.Num() > 1)
		Lerp((CVec3&)A.KeyPos[i], (CVec3&)A.KeyPos[i+1], frac, DstPos);
	else
		DstPos = (CVec3&)A.KeyPos[0];
	// get orientation
	if (A.KeyQuat.Num() > 1)
		Slerp((CQuat&)A.KeyQuat[i], (CQuat&)A.KeyQuat[i+1], frac, DstQuat);
	else
		DstQuat = (CQuat&)A.KeyQuat[0];
}


void DrawSkeleton(const USkeletalMesh *Mesh, const UMeshAnimation *Anim, int Seq, float Time)
{
	CCoords	BonePos[256];
	glBegin(GL_LINES);

	const MotionChunk &M = Anim->Moves[Seq];

	for (int i = 0; i < Anim->RefBones.Num(); i++)
	{
		const FNamedBone  &B = Anim->RefBones[i];
		const AnalogTrack &A = M.AnimTracks[i];

		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);
		assert(strcmp(B.Name, Mesh->Bones[i].Name) == 0);
//--		assert(B.ParentIndex == Mesh->Bones[i].ParentIndex);

#if 0
		const CVec3 &BP = (CVec3&)A.KeyPos[0];
//		const CQuat &BO = (CQuat&)B.BonePos.Orientation;
		CQuat BO; BO = (CQuat&)A.KeyQuat[0];
#else
		CVec3 BP;
		CQuat BO;
		GetBonePosition(A, Time, BP, BO);
#endif
		if (!i) BO.Conjugate();

		// convert VJointPos to CCoords
		CCoords &BC = BonePos[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
#if 0
		if (i)	// do not rotate root bone
			BonePos[B.ParentIndex].UnTransformCoords(BC, BC);
#else
		if (i)	// do not rotate root bone
			BonePos[Mesh->Bones[i].ParentIndex].UnTransformCoords(BC, BC);
#endif

		CVec3 v2;
		v2.Set(10/*B.BonePos.Length*/, 0, 0);
		BC.UnTransformPoint(v2, v2);

		glColor3f(1,0,0);
		glVertex3fv(BC.origin.v);
		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,1);
			glVertex3fv(BonePos[B.ParentIndex].origin.v);
		}
		else
		{
			glColor3f(1,0,1);
			glVertex3f(0, 0, 0);
		}
		glVertex3fv(BC.origin.v);
	}
	glColor3f(1,1,1);
	glEnd();
}

void DrawBaseSkeletalMesh(const USkeletalMesh *Mesh)
{
	int i;

	glBegin(GL_POINTS);
	for (i = 0; i < Mesh->VertexCount; i++)
	{
		const FVector &V = Mesh->Points[i];
		CVec3 v;
		v[0] = V.X * Mesh->MeshScale.X /*+ Mesh->MeshOrigin.X*/;
		v[1] = V.Y * Mesh->MeshScale.Y /*+ Mesh->MeshOrigin.Y*/;
		v[2] = V.Z * Mesh->MeshScale.Z /*+ Mesh->MeshOrigin.Z*/;
		glVertex3fv(&V.X);
	}
	glEnd();
}


void DrawLodSkeletalMesh(const USkeletalMesh *Mesh, const FStaticLODModel *lod)
{
	int i;
	glBegin(GL_POINTS);
	for (i = 0; i < lod->SkinPoints.Num(); i++)
	{
		const FVector &V = lod->SkinPoints[i].Point;
		glVertex3fv(&V.X);
	}
	glEnd();
	//!! wrong
	CCoords	BonePos[256];
	glBegin(GL_LINES);
	for (i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B = Mesh->Bones[i];
		const CVec3 &BP = (CVec3&)B.BonePos.Position;
//		const CQuat &BO = (CQuat&)B.BonePos.Orientation;
		CQuat BO; BO = (CQuat&)B.BonePos.Orientation;
		if (!i) BO.Conjugate();

		// convert VJointPos to CCoords
		CCoords &BC = BonePos[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			BonePos[B.ParentIndex].UnTransformCoords(BC, BC);

		CVec3 v2;
		v2.Set(10/*B.BonePos.Length*/, 0, 0);
		BC.UnTransformPoint(v2, v2);

		glColor3f(1,0,0);
		glVertex3fv(BC.origin.v);
		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,1);
			glVertex3fv(BonePos[B.ParentIndex].origin.v);
		}
		else
		{
			glColor3f(1,0,1);
			glVertex3f(0, 0, 0);
		}
		glVertex3fv(BC.origin.v);
	}
	glColor3f(1,1,1);
	glEnd();
}


void CSkelMeshViewer::Draw3D()
{
	CMeshViewer::Draw3D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	if (LodNum < 0)
	{
		DrawBaseSkeletalMesh(Mesh);
		//!!
		if (Mesh->Animation && CurrAnim >= 0)
			DrawSkeleton(Mesh, Mesh->Animation, CurrAnim, AnimTime);
	}
	else
		DrawLodSkeletalMesh(Mesh, &Mesh->StaticLODModels[LodNum]);
}


void CSkelMeshViewer::Draw2D()
{
	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	int NumAnims = 0;
	if (Mesh->Animation)
		NumAnims = Mesh->Animation->AnimSeqs.Num();
	const char *AnimName = CurrAnim < 0 ? "default" : *Mesh->Animation->AnimSeqs[CurrAnim].Name;
	float AnimRate       = CurrAnim < 0 ? 0         :  Mesh->Animation->AnimSeqs[CurrAnim].Rate;
	int   AnimFrames     = CurrAnim < 0 ? 0         :  Mesh->Animation->AnimSeqs[CurrAnim].NumFrames;

	CMeshViewer::Draw2D();
	if (LodNum < 0)
		GL::textf("LOD : base mesh\n");
	else
		GL::textf("LOD : %d/%d\n", LodNum+1, Mesh->StaticLODModels.Num());
	GL::textf("Anim: %d/%d (%s) rate: %g frames: %d\n", CurrAnim+1, NumAnims, AnimName, AnimRate, AnimFrames);
	GL::textf("Time: %.1f/%d\n", AnimTime, AnimFrames);
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
