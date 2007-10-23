#include "ObjectViewer.h"
#include "MeshInstance.h"


struct CMeshBoneData
{
	// static data (computed after mesh loading)
	int			BoneMap;			// index of bone in animation tracks
	CCoords		RefCoords;			// coordinates of bone in reference pose
	CCoords		RefCoordsInv;		// inverse of RefCoordsInv
	// dynamic data (depends from current pose)
	CCoords		Coords;				// current coordinates of bone, model-space
	CCoords		Transform;			// used to transform vertex from reference pose to current pose
	// data for tweening; bone-space
	CVec3		Pos;				// current position of bone
	CQuat		Quat;				// current orientation quaternion
	// skeleton configuration
	float		Scale;				// bone scale; 1=unscaled
};


/*-----------------------------------------------------------------------------
	Create/destroy
-----------------------------------------------------------------------------*/

CSkelMeshInstance::CSkelMeshInstance(USkeletalMesh *Mesh, CSkelMeshViewer *Viewer)
:	CMeshInstance(Mesh, Viewer)
,	LodNum(-1)
,	AnimIndex(-1)
,	AnimTime(0)
,	AnimTweenTime(0)
{
	guard(CSkelMeshInstance::CSkelMeshInstance);

	int NumBones = Mesh->Bones.Num();
	const UMeshAnimation *Anim = Mesh->Animation;

	// allocate some arrays
	BoneData  = new CMeshBoneData[NumBones];
	MeshVerts = new CVec3[Mesh->Points.Num()];

	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < NumBones; i++, data++)
	{
		const FMeshBone &B = Mesh->Bones[i];
		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);

		// find reference bone in animation track
		data->BoneMap = -1;
		if (Anim)
		{
			for (int j = 0; j < Anim->RefBones.Num(); j++)
				if (!strcmp(B.Name, Anim->RefBones[j].Name))
				{
					data->BoneMap = j;
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

		CCoords &BC = data->RefCoords;
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			BoneData[Mesh->Bones[i].ParentIndex].RefCoords.UnTransformCoords(BC, BC);
		// store inverted transformation too
		InvertCoords(data->RefCoords, data->RefCoordsInv);
		// initialize skeleton configuration
		data->Scale = 1.0f;			// default bone scale
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
	delete BoneData;
	delete MeshVerts;
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

int CSkelMeshInstance::FindBone(const char *BoneName) const
{
	const USkeletalMesh *Mesh = GetMesh();
	for (int i = 0; i < Mesh->Bones.Num(); i++)
		if (!strcmp(Mesh->Bones[i].Name, BoneName))
			return i;
	return -1;
}


int CSkelMeshInstance::FindAnim(const char *AnimName) const
{
	const USkeletalMesh *Mesh  = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	if (!Anim)
		return -1;
	for (int i = 0; i < Anim->AnimSeqs.Num(); i++)
		if (!strcmp(Anim->AnimSeqs[i].Name, AnimName))
			return i;
	return -1;
}


void CSkelMeshInstance::SetBoneScale(const char *BoneName, float scale)
{
	int BoneIndex = FindBone(BoneName);
	if (BoneIndex < 0) return;
	BoneData[BoneIndex].Scale = scale;
}


/*-----------------------------------------------------------------------------
	Skeletal animation support
-----------------------------------------------------------------------------*/

static void GetBonePosition(const AnalogTrack &A, float Frame, float NumFrames, bool Loop,
	CVec3 &DstPos, CQuat &DstQuat)
{
	guard(GetBonePosition);

	int i;

	// fast case: 1 frame only
	if (A.KeyTime.Num() == 1)
	{
		DstPos  = (CVec3&)A.KeyPos[0];
		DstQuat = (CQuat&)A.KeyQuat[0];
		return;
	}

	// find index in time key array
	//!! linear search -> binary search
	int NumKeys = A.KeyTime.Num();
	for (i = 0; i < NumKeys; i++)
	{
		if (Frame == A.KeyTime[i])
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? (CVec3&)A.KeyPos[i]  : (CVec3&)A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? (CQuat&)A.KeyQuat[i] : (CQuat&)A.KeyQuat[0];
			return;
		}
		if (Frame < A.KeyTime[i])
		{
			i--;
			break;
		}
	}
	if (i > NumKeys-1)
		i = NumKeys-1;

	int X = i;
	int Y = i+1;
	float frac;
	if (Y >= NumKeys)
	{
		if (!Loop)
		{
			// clamp animation
			Y = NumKeys-1;
			assert(X == Y);
			frac = 0;
		}
		else
		{
			// loop animation
			Y = 0;
			frac = (Frame - A.KeyTime[X]) / (NumFrames - A.KeyTime[X]);
		}
	}
	else
	{
		frac = (Frame - A.KeyTime[X]) / (A.KeyTime[Y] - A.KeyTime[X]);
	}

	assert(X >= 0 && X < NumKeys);
	assert(Y >= 0 && Y < NumKeys);

	// get position
	if (A.KeyPos.Num() > 1)
		Lerp((CVec3&)A.KeyPos[X], (CVec3&)A.KeyPos[Y], frac, DstPos);
	else
		DstPos = (CVec3&)A.KeyPos[0];
	// get orientation
	if (A.KeyQuat.Num() > 1)
		Slerp((CQuat&)A.KeyQuat[X], (CQuat&)A.KeyQuat[Y], frac, DstQuat);
	else
		DstQuat = (CQuat&)A.KeyQuat[0];

	unguard;
}


void CSkelMeshInstance::UpdateSkeleton()
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;

	const MotionChunk  *Motion  = NULL;
	const FMeshAnimSeq *AnimSeq = NULL;
	if (AnimIndex >= 0)
	{
		Motion  = &Anim->Moves[AnimIndex];
		AnimSeq = &Anim->AnimSeqs[AnimIndex];
	}

	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < Mesh->Bones.Num(); i++, data++)
	{
		const FMeshBone &B = Mesh->Bones[i];

		CVec3 BP;
		CQuat BO;
		int BoneIndex = data->BoneMap;
		// compute bone orientation
		if (Motion && BoneIndex >= 0)
		{
			// get bone position from track
			GetBonePosition(Motion->AnimTracks[BoneIndex], AnimTime, AnimSeq->NumFrames, AnimLooped, BP, BO);
		}
		else
		{
			// get default bone position
			BP = (CVec3&)B.BonePos.Position;
			BO = (CQuat&)B.BonePos.Orientation;
		}
		if (!i) BO.Conjugate();

		if (IsTweening())
		{
			// interpolate orientation using AnimTweenStep
			// current orientation -> {BP,BO}
			Lerp(data->Pos,   BP, AnimTweenStep, BP);
			Slerp(data->Quat, BO, AnimTweenStep, BO);
		}

		CCoords &BC = data->Coords;
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		data->Quat = BO;
		data->Pos  = BP;
		// move bone position to global coordinate space
		if (!i)
		{
			// root bone - use BaseTransform
			// can use inverted BaseTransformScaled to avoid 'slow' operation
			BaseTransformScaled.TransformCoordsSlow(BC, BC);
		}
		else
		{
			// other bones - rotate around parent bone
			BoneData[Mesh->Bones[i].ParentIndex].Coords.UnTransformCoords(BC, BC);
		}
		// deform skeleton according to external settings
		if (data->Scale != 1.0f)
		{
			BC.axis[0].Scale(data->Scale);
			BC.axis[1].Scale(data->Scale);
			BC.axis[2].Scale(data->Scale);
		}
		// compute transformation of world-space model vertices from reference
		// pose to desired pose
		BC.UnTransformCoords(data->RefCoordsInv, data->Transform);
	}
	unguard;
}


void CSkelMeshInstance::PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, bool Looped)
{
	guard(CSkelMeshInstance::PlayAnimInternal);

	int NewAnimIndex = FindAnim(AnimName);
	if (NewAnimIndex < 0)
	{
		// show default pose
		AnimIndex     = -1;
		AnimTime      = 0;
		AnimRate      = 0;
		AnimLooped    = false;
		AnimTweenTime = TweenTime;
		return;
	}

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	assert(Anim);

	AnimRate   = (NewAnimIndex >= 0) ? Anim->AnimSeqs[NewAnimIndex].Rate * Rate : 0;
	AnimLooped = Looped;

	if (NewAnimIndex == AnimIndex && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	AnimIndex     = NewAnimIndex;
	AnimTime      = 0;
	AnimTweenTime = TweenTime;

	unguard;
}


void CSkelMeshInstance::FreezeAnimAt(float Time)
{
	guard(CSkelMeshInstance::FreezeAnimAt);
	AnimTime = Time;
	AnimRate = 0;
	unguard;
}


void CSkelMeshInstance::GetAnimParams(const char *&AnimName,
	float &Frame, float &NumFrames, float &Rate) const
{
	guard(CSkelMeshInstance::GetAnimParams);

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	if (!Anim || AnimIndex < 0)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const FMeshAnimSeq &AnimSeq = Anim->AnimSeqs[AnimIndex];
	AnimName  = AnimSeq.Name;
	Frame     = AnimTime;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimRate;

	unguard;
}


void CSkelMeshInstance::UpdateAnimation(float TimeDelta)
{
	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;

	//?? loop for channels
	{
		// update tweening
		if (AnimTweenTime)
		{
			AnimTweenStep = TimeDelta / AnimTweenTime;
			AnimTweenTime -= TimeDelta;
			if (AnimTweenTime < 0)
			{
				// stop tweening, start animation
				TimeDelta = -AnimTweenTime;
				AnimTweenTime = 0;
			}
			assert(AnimTime == 0);
		}
		// note: AnimTweenTime may be changed now, check again
		if (!AnimTweenTime && AnimIndex >= 0)
		{
			// update animation time
			AnimTime += TimeDelta * AnimRate;
			const FMeshAnimSeq &Seq = Anim->AnimSeqs[AnimIndex];
			if (AnimLooped)
			{
				if (AnimTime >= Seq.NumFrames)
				{
					// wrap time
					int numSkip = appFloor(AnimTime / Seq.NumFrames);
					AnimTime -= numSkip * Seq.NumFrames;
				}
			}
			else
			{
				if (AnimTime >= Seq.NumFrames-1)
				{
					// clamp time
					AnimTime = Seq.NumFrames-1;
					if (AnimTime < 0)
						AnimTime = 0;
				}
			}
		}
	}

	UpdateSkeleton();
}


/*-----------------------------------------------------------------------------
	Drawing
-----------------------------------------------------------------------------*/

void CSkelMeshInstance::DrawSkeleton()
{
	guard(CSkelMeshInstance::DrawSkeleton);

	const USkeletalMesh *Mesh = GetMesh();

	glLineWidth(3);
	glEnable(GL_LINE_SMOOTH);

	glBegin(GL_LINES);
	for (int i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B  = Mesh->Bones[i];
		const CCoords   &BC = BoneData[i].Coords;

		CVec3 v2;
		v2.Set(10, 0, 0);
		BC.UnTransformPoint(v2, v2);

		glColor3f(1,0,0);
		glVertex3fv(BC.origin.v);
		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,0.3);
			glVertex3fv(BoneData[B.ParentIndex].Coords.origin.v);
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

	const USkeletalMesh *Mesh = GetMesh();

	//?? try other tech: compute weighted sum of matrices, and then
	//?? transform vector (+ normal ?; note: normals requires normalized
	//?? transformations ?? (or normalization guaranteed by sum(weights)==1?))
	memset(MeshVerts, 0, sizeof(CVec3) * Mesh->VertexCount);
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &Inf = Mesh->VertInfluences[i];
		const CMeshBoneData &data = BoneData[Inf.BoneIndex];
		int PointIndex = Inf.PointIndex;
		const CVec3 &Src = (CVec3&)Mesh->Points[PointIndex];
		CVec3 tmp;
#if 0
		// variant 1: ref pose -> local bone space -> transformed bone to world
		data.RefCoords.TransformPoint(Src, tmp);
		data.Coords.UnTransformPoint(tmp, tmp);
#else
		// variant 2: use prepared transformation (same result, but faster)
		data.Transform.UnTransformPoint(Src, tmp);
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
	guard(CSkelMeshInstance::DrawLodSkeletalMesh);
	int i, sec;
	// smooth sections (influence count >= 2)
	for (sec = 0; sec < lod->SmoothSections.Num(); sec++)
	{
		const FSkelMeshSection &ms = lod->SmoothSections[sec];
		SetMaterial(ms.MaterialIndex);
		glBegin(GL_TRIANGLES);
		//?? use smooth indices
		for (i = 0; i < ms.NumFaces; i++)
		{
			const FMeshFace &F = lod->Faces[ms.FirstFace + i];
//?? ignore F.MaterialIndex - may be any
//			assert(F.MaterialIndex == 0);
//			assert(F.MaterialIndex == ms.MaterialIndex);
			for (int j = 0; j < 3; j++)
			{
				const FMeshWedge &W = lod->Wedges[F.iWedge[j]];
				glTexCoord2f(W.TexUV.U, W.TexUV.V);
				glVertex3fv(&lod->SkinPoints[W.iVertex].Point.X);
			}
		}
		glEnd();
	}
	// rigid sections (influence count == 1)
	for (sec = 0; sec < lod->RigidSections.Num(); sec++)
	{
		const FSkelMeshSection &ms = lod->RigidSections[sec];
		SetMaterial(ms.MaterialIndex);
		glBegin(GL_TRIANGLES);
		for (i = 0; i < ms.NumFaces; i++)
		{
			const FMeshFace &F = lod->Faces[ms.FirstFace + i];
			for (int j = 0; j < 3; j++)
			{
				const FMeshWedge &W = lod->Wedges[F.iWedge[j]];
				glTexCoord2f(W.TexUV.U, W.TexUV.V);
				glVertex3fv(&lod->VertexStream.Verts[W.iVertex].Pos.X);
			}
		}
		for (i = ms.MinStreamIndex; i < ms.MinStreamIndex + ms.NumStreamIndices; i++)
		{
			const FAnimMeshVertex &V = lod->VertexStream.Verts[lod->RigidIndices.Indices[i]];
			glTexCoord2f(V.Tex.U, V.Tex.V);
			glVertex3fv(&V.Pos.X);
		}
		glEnd();
	}
	unguard;
}


void CSkelMeshInstance::Draw()
{
	guard(CSkelMeshInstance::Draw);

	const USkeletalMesh   *Mesh   = GetMesh();
	const CSkelMeshViewer *Viewer = static_cast<CSkelMeshViewer*>(Viewport);

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
