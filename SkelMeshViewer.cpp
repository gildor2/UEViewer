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
	VERIFY_NULL(WeightIndices.Num());
	VERIFY_NULL(BoneInfluences.Num());
	VERIFY_NOT_NULL(VertInfluences.Num());

	for (i = 0; i < Mesh->StaticLODModels.Num(); i++)
	{
		const FStaticLODModel &lod = Mesh->StaticLODModels[i];
//?? (not always)	if (lod.NumDynWedges != lod.Wedges.Num()) appNotify("lod[%d]: NumDynWedges!=wedges.Num()", i);
		if (lod.SkinPoints.Num() != lod.Points.Num() && lod.RigidSections.Num() == 0)
			appNotify("[%d] skinPoints: %d", i,	lod.SkinPoints.Num());
		if (lod.SmoothIndices.Indices.Num() + lod.RigidIndices.Indices.Num() != lod.Faces.Num() * 3)
			appNotify("[%d] strange indices count", i);
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


//!! mesh instance data
static CCoords *BoneCoords    = NULL;
static CCoords *RefBoneCoords = NULL;
static CCoords *BoneTransform = NULL;		// transformation for verts from reference pose to deformed
static CVec3   *MeshVerts     = NULL;
static int     *BoneMap       = NULL;		// remap indices of Mesh.FMeshBone[] to Anim.FNamedBone[]

static void InitBuffers(const USkeletalMesh *Mesh)
{
	if (BoneCoords)
		return;			// already initialized
	//!! should free on exit too
	int NumBones = Mesh->Bones.Num();

	BoneCoords    = new CCoords[NumBones];
	RefBoneCoords = new CCoords[NumBones];
	BoneTransform = new CCoords[NumBones];
	BoneMap       = new int    [NumBones];
	MeshVerts     = new CVec3  [Mesh->Points.Num()];

	const UMeshAnimation *Anim = Mesh->Animation;

	for (int i = 0; i < NumBones; i++)
	{
		const FMeshBone &B = Mesh->Bones[i];
		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);

		// find reference bone in animation track
		BoneMap[i] = -1;
		if (Anim)
		{
			for (int j = 0; j < Anim->RefBones.Num(); j++)
				if (!strcmp(B.Name, Anim->RefBones[j].Name))
				{
					BoneMap[i] = j;
					break;
				}
		}

		// compute reference bone coords
		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = (CVec3&)B.BonePos.Position;
		BO = (CQuat&)B.BonePos.Orientation;
		if (!i) BO.Conjugate();

		CCoords &BC = RefBoneCoords[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			RefBoneCoords[Mesh->Bones[i].ParentIndex].UnTransformCoords(BC, BC);
	}
}


void UpdateSkeleton(const USkeletalMesh *Mesh, const UMeshAnimation *Anim, int Seq, float Time)
{
	InitBuffers(Mesh);		//!! call from constructor

	const MotionChunk *Motion = NULL;
	if (Seq >= 0)
		Motion = &Anim->Moves[Seq];

	for (int i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B = Mesh->Bones[i];

		CVec3 BP;
		CQuat BO;
		int BoneIndex = BoneMap[i];
		if (Motion && BoneIndex >= 0)
		{
			// get bone position from track
			GetBonePosition(Motion->AnimTracks[BoneIndex], Time, BP, BO);
		}
		else
		{
			// get default pose
			BP = (CVec3&)B.BonePos.Position;
			BO = (CQuat&)B.BonePos.Orientation;
		}
		if (!i) BO.Conjugate();

		CCoords &BC = BoneCoords[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			BoneCoords[Mesh->Bones[i].ParentIndex].UnTransformCoords(BC, BC);
		// compute transformation for world-space vertices from reference pose
		// to desired pose
		CCoords tmp;
		InvertCoords(BoneCoords[i], tmp);
		RefBoneCoords[i].UnTransformCoords(tmp, BoneTransform[i]);
	}
}


void DrawSkeleton(USkeletalMesh *Mesh)
{
	glLineWidth(3);
	glEnable(GL_LINE_SMOOTH);

	glBegin(GL_LINES);
	for (int i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B  = Mesh->Bones[i];
		const CCoords   &BC = BoneCoords[i];

		CVec3 v2;
		v2.Set(10, 0, 0);
		BC.UnTransformPoint(v2, v2);

		glColor3f(1,0,0);
		glVertex3fv(BC.origin.v);
		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,0.3);
			glVertex3fv(BoneCoords[B.ParentIndex].origin.v);
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

	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);
}


void DrawBaseSkeletalMesh(const USkeletalMesh *Mesh)
{
	int i;

	memset(MeshVerts, 0, sizeof(CVec3) * Mesh->VertexCount);
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &Inf = Mesh->VertInfluences[i];
		int BoneIndex  = Inf.BoneIndex;
		int PointIndex = Inf.PointIndex;
		const CVec3 &Src = (CVec3&)Mesh->Points[PointIndex];
		CVec3 tmp;
#if 0
		// variant 1: ref pose -> local bone space -> transformed bone to world
		RefBoneCoords[BoneIndex].TransformPoint(Src, tmp);
		BoneCoords[BoneIndex].UnTransformPoint(tmp, tmp);
#else
		// variant 2: use prepared transformation (same result, but faster)
		BoneTransform[BoneIndex].TransformPoint(Src, tmp);
#endif
		VectorMA(MeshVerts[PointIndex], Inf.Weight, tmp);
	}

	glBegin(GL_TRIANGLES);
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &Face = Mesh->Triangles[i];
		for (int j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[Face.WedgeIndex[j]];
			glVertex3fv(&MeshVerts[W.iVertex][0]);
		}
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
}


void CSkelMeshViewer::Draw3D()
{
	CMeshViewer::Draw3D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);

	UpdateSkeleton(Mesh, Mesh->Animation, CurrAnim, AnimTime);
	// show skeleton
	if (ShowSkel)
		DrawSkeleton(Mesh);
	// show mesh
	if (ShowSkel != 2)
	{
		if (LodNum < 0)
			DrawBaseSkeletalMesh(Mesh);
		else
			DrawLodSkeletalMesh(Mesh, &Mesh->StaticLODModels[LodNum]);
	}
}


void CSkelMeshViewer::Draw2D()
{
	CMeshViewer::Draw2D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	int NumAnims = 0;
	if (Mesh->Animation)
		NumAnims = Mesh->Animation->AnimSeqs.Num();
	const char *AnimName = CurrAnim < 0 ? "default" : *Mesh->Animation->AnimSeqs[CurrAnim].Name;
	float AnimRate       = CurrAnim < 0 ? 0         :  Mesh->Animation->AnimSeqs[CurrAnim].Rate;
	int   AnimFrames     = CurrAnim < 0 ? 0         :  Mesh->Animation->AnimSeqs[CurrAnim].NumFrames;

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


void CSkelMeshViewer::ProcessKey(unsigned char key)
{
	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	int NumAnims = 0, NumFrames = 0;
	if (Mesh->Animation)
		NumAnims  = Mesh->Animation->AnimSeqs.Num();
	if (CurrAnim >= 0)
		NumFrames = Mesh->Animation->AnimSeqs[CurrAnim].NumFrames;

	switch (key)
	{
	case 'l':
		if (++LodNum >= Mesh->StaticLODModels.Num())
			LodNum = -1;
		break;
	case '[':
		if (--CurrAnim < -1)
			CurrAnim = NumAnims - 1;
		AnimTime = 0;
		break;
	case ']':
		if (++CurrAnim >= NumAnims)
			CurrAnim = -1;
		AnimTime = 0;
		break;
	case ',':		// '<'
		AnimTime -= 0.2;
		if (AnimTime < 0)
			AnimTime = 0;
		break;
	case '.':		// '>'
		if (NumFrames > 0)
		{
			AnimTime += 0.2;
			if (AnimTime > NumFrames - 1)
				AnimTime = NumFrames - 1;
		}
		break;
	case 's':
		if (++ShowSkel > 2)
			ShowSkel = 0;
		break;
#if 1
	//!! REMOVE
	case 'a':
		if (LodNum >= 0)
		{
			const TArray<unsigned>& A = Mesh->StaticLODModels[LodNum].f0;
			for (int i = 0; i < A.Num(); i++)
				printf("%-4d %08X  /  %d  /  %g\n", i, A[i], A[i], (float&)A[i]);
		}
		break;
#endif
	default:
		CMeshViewer::ProcessKey(key);
	}
}
