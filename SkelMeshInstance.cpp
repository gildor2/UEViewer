#include "ObjectViewer.h"
#include "MeshInstance.h"


CSkelMeshInstance::CSkelMeshInstance(USkeletalMesh *Mesh, CSkelMeshViewer *Viewer)
:	CMeshInstance(Mesh, Viewer)
,	LodNum(-1)
,	CurrAnim(-1)
,	AnimTime(0)
{
	guard(CSkelMeshInstance::CSkelMeshInstance);
	//!! should free on exit too
	int NumBones = Mesh->Bones.Num();

	BoneCoords    = new CCoords[NumBones];
	RefBoneCoords = new CCoords[NumBones];
	BoneTransform = new CCoords[NumBones];
	BoneMap       = new int    [NumBones];
	MeshVerts     = new CVec3  [Mesh->Points.Num()];

	const UMeshAnimation *Anim = Mesh->Animation;

	int i;
	for (i = 0; i < NumBones; i++)
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

	// normalize VertInfluences: sum of all influences may be != 1
	// (possible situation, see SkaarjAnims/Skaarj2, SkaarjAnims/Skaarj_Skel, XanRobots/XanF02)
	float *VertSumWeights = new float[Mesh->Points.Num()];	// zeroed
	int   *VertInfCount   = new int  [Mesh->Points.Num()];	// zeroed
	// count sum of weights for all verts
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &Inf = Mesh->VertInfluences[i];
		int PointIndex = Inf.PointIndex;
		assert(PointIndex < Mesh->Points.Num());
		VertSumWeights[PointIndex] += Inf.Weight;
		VertInfCount  [PointIndex]++;
	}
#if 0
	// notify about wrong weights
	for (i = 0; i < Mesh->Points.Num(); i++)
	{
		if (fabs(VertSumWeights[i] - 1.0f) < 0.01f) continue;
		appNotify("Vert[%d] weight sum=%g (%d weights)", i, VertSumWeights[i], VertInfCount[i]);
	}
#endif
	// normalize weights
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		FVertInfluences &Inf = Mesh->VertInfluences[i];
		int PointIndex = Inf.PointIndex;
		float sum = VertSumWeights[PointIndex];
		if (fabs(sum - 1.0f) < 0.01f) continue;
		assert(sum > 0.01f);	// no division by zero
		Inf.Weight = Inf.Weight / sum;
	}

	delete VertSumWeights;
	delete VertInfCount;
	unguard;
}


CSkelMeshInstance::~CSkelMeshInstance()
{
	delete BoneCoords;
	delete RefBoneCoords;
	delete BoneTransform;
	delete BoneMap;
	delete MeshVerts;
}


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


void CSkelMeshInstance::UpdateSkeleton(int Seq, float Time)	//?? can use local vars instead of Seq/Time
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	const USkeletalMesh  *Mesh = static_cast<USkeletalMesh*>(pMesh);
	const UMeshAnimation *Anim = Mesh->Animation;

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
	unguard;
}


void CSkelMeshInstance::DrawSkeleton()
{
	guard(CSkelMeshInstance::DrawSkeleton);

	const USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(pMesh);

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

	unguard;
}


void CSkelMeshInstance::DrawBaseSkeletalMesh()
{
	guard(CSkelMeshInstance::DrawBaseSkeletalMesh);
	int i;

	const USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(pMesh);

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

	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &Face = Mesh->Triangles[i];
		SetMaterial(Face.MatIndex);
		glBegin(GL_TRIANGLES);
		for (int j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[Face.WedgeIndex[j]];
			glTexCoord2f(W.TexUV.U, W.TexUV.V);
			glVertex3fv(&MeshVerts[W.iVertex][0]);
		}
		glEnd();
	}
	glDisable(GL_TEXTURE_2D);

	unguard;
}


void CSkelMeshInstance::DrawLodSkeletalMesh(const FStaticLODModel *lod)
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


void CSkelMeshInstance::Draw()
{
	guard(CSkelMeshInstance::Draw);

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(pMesh);
	CSkelMeshViewer *Viewer = static_cast<CSkelMeshViewer*>(Viewport);

	UpdateSkeleton(CurrAnim, AnimTime);
	// show skeleton
	if (Viewer->ShowSkel)		//!! move this part to CSkelMeshViewer; call Inst->DrawSkeleton() etc
		DrawSkeleton();
	// show mesh
	if (Viewer->ShowSkel != 2)
	{
		if (LodNum < 0)
			DrawBaseSkeletalMesh();
		else
			DrawLodSkeletalMesh(&Mesh->StaticLODModels[LodNum]);
	}

	unguard;
}
