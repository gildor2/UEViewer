#include "ObjectViewer.h"
#include "MeshInstance.h"


struct CMeshBoneData
{
	// static data (computed after mesh loading)
	int			BoneMap;			// index of bone in animation tracks
	CCoords		RefCoords;			// coordinates of bone in reference pose
	CCoords		RefCoordsInv;		// inverse of RefCoordsInv
	int			SubtreeSize;		// count of all children bones (0 for leaf bone)
	// dynamic data
	// skeleton configuration
	float		Scale;				// bone scale; 1=unscaled
	short		FirstChannel;		// first animation channel, affecting this bone
	// current pose
	CCoords		Coords;				// current coordinates of bone, model-space
	CCoords		Transform;			// used to transform vertex from reference pose to current pose
	// data for tweening; bone-space
	CVec3		Pos;				// current position of bone
	CQuat		Quat;				// current orientation quaternion
};


#define ANIM_UNASSIGNED			-2
#define MAX_BONES				256


/*-----------------------------------------------------------------------------
	Create/destroy
-----------------------------------------------------------------------------*/

//?? rename function
/* Iterate bone (sub)tree and do following:
 *	- find all direct childs of bone 'Index', produce list of bones 'Indices',
 *	  sorted in hierarchy order (1st child and its children first, other childs next)
 *	- compute subtree sizes ('Sizes')
 *	- compute bone hierarchy depth ('Depth', for debugging only)
 */
static int WalkSubtree(const TArray<FMeshBone> &Bones, int Index,
	int *Indices, int *Sizes, int *Depth, int &numIndices, int maxIndices)
{
	assert(numIndices < maxIndices);
	static int depth = 0;
	// remember curerent index, increment for recustion
	int currIndex = numIndices++;
	// find children of Bone[Index]
	int treeSize = 0;
	for (int i = Index + 1; i < Bones.Num(); i++)
		if (Bones[i].ParentIndex == Index)
		{
			// recurse
			depth++;
			treeSize += WalkSubtree(Bones, i, Indices, Sizes, Depth, numIndices, maxIndices);
			depth--;
		}
	// store gathered information
	Indices[currIndex] = Index;
	Sizes  [currIndex] = treeSize;
	Depth  [currIndex] = depth;
	return treeSize + 1;
}


CSkelMeshInstance::CSkelMeshInstance(USkeletalMesh *Mesh, CSkelMeshViewer *Viewer)
:	CMeshInstance(Mesh, Viewer)
,	LodNum(-1)
,	MaxAnimChannel(-1)
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

	// init 1st animation channel with default pose
	PlayAnim("None");
	for (i = 0; i < MAX_SKELANIMCHANNELS; i++)
	{
		Channels[i].AnimIndex  = ANIM_UNASSIGNED;
		Channels[i].BlendAlpha = 1;
		Channels[i].RootBone   = 0;
	}

	//?? check bones tree
	int boneIndices[MAX_BONES], treeSizes[MAX_BONES], depth[MAX_BONES];
	int numIndices = 0;
	WalkSubtree(Mesh->Bones, 0, boneIndices, treeSizes, depth, numIndices, MAX_BONES);
	assert(numIndices == Mesh->Bones.Num());
	for (i = 0; i < numIndices; i++)
	{
		int bone = boneIndices[i];
		assert(bone == i);						// if somewhere failed, should resort/remap bones
		BoneData[i].SubtreeSize = treeSizes[i];	// remember subtree size
	}

#if 1
	//?? dump tree
	for (i = 0; i < numIndices; i++)
	{
		int bone   = boneIndices[i];
		int parent = Mesh->Bones[bone].ParentIndex;
		printf("%2d: bone#%2d (parent %2d [%20s]); tree size: %2d ", i, bone,
			parent, *Mesh->Bones[parent].Name, treeSizes[i]);
		for (int j = 0; j <= depth[i]; j++)
			printf("| ");
		printf("%s\n", *Mesh->Bones[bone].Name);
	}
#endif

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


//!! remove later
static int BoneUpdateCounts[256];

void CSkelMeshInstance::UpdateSkeleton()
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;

	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	memset(BoneUpdateCounts, 0, sizeof(BoneUpdateCounts)); //!! remove later
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && Chn->AnimIndex == ANIM_UNASSIGNED)
			continue;

		const MotionChunk  *Motion  = NULL;
		const FMeshAnimSeq *AnimSeq = NULL;
		if (Chn->AnimIndex >= 0)
		{
			Motion  = &Anim->Moves   [Chn->AnimIndex];
			AnimSeq = &Anim->AnimSeqs[Chn->AnimIndex];
		}

		// compute bone range, affected by specified animation bone
		int firstBone = Chn->RootBone;
		int lastBone  = firstBone + BoneData[firstBone].SubtreeSize;
		assert(lastBone < Mesh->Bones.Num());

		int i;
		CMeshBoneData *data;
		for (i = firstBone, data = BoneData + firstBone; i <= lastBone; i++, data++)
		{
			if (Stage < data->FirstChannel)
				continue;			// bone position will be overrided in following channel(s)

			const FMeshBone &B = Mesh->Bones[i];
			//!! add animation blending
			BoneUpdateCounts[i]++;		//!! remove later

			CVec3 BP;
			CQuat BO;
			int BoneIndex = data->BoneMap;
			// compute bone orientation
			if (Motion && BoneIndex >= 0)
			{
				// get bone position from track
				GetBonePosition(Motion->AnimTracks[BoneIndex], Chn->Time, AnimSeq->NumFrames, Chn->Looped, BP, BO);
			}
			else
			{
				// get default bone position
				BP = (CVec3&)B.BonePos.Position;
				BO = (CQuat&)B.BonePos.Orientation;
			}
			if (!i) BO.Conjugate();

			if (Chn->TweenTime > 0)	// tweening
			{
				// interpolate orientation using AnimTweenStep
				// current orientation -> {BP,BO}
				Lerp (data->Pos,  BP, Chn->TweenStep, BP);
				Slerp(data->Quat, BO, Chn->TweenStep, BO);
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
	}
	unguard;
}


void CSkelMeshInstance::PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, int Channel, bool Looped)
{
	guard(CSkelMeshInstance::PlayAnimInternal);

	CAnimChan &Chn = GetStage(Channel);
	if (Channel > MaxAnimChannel)
		MaxAnimChannel = Channel;

	int NewAnimIndex = FindAnim(AnimName);
	if (NewAnimIndex < 0)
	{
		// show default pose
		Chn.AnimIndex = -1;
		Chn.Time      = 0;
		Chn.Rate      = 0;
		Chn.Looped    = false;
		Chn.TweenTime = TweenTime;
		return;
	}

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	assert(Anim);

	Chn.Rate   = (NewAnimIndex >= 0) ? Anim->AnimSeqs[NewAnimIndex].Rate * Rate : 0;
	Chn.Looped = Looped;

	if (NewAnimIndex == Chn.AnimIndex && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	Chn.AnimIndex = NewAnimIndex;
	Chn.Time      = 0;
	Chn.TweenTime = TweenTime;

	unguard;
}


void CSkelMeshInstance::SetBlendParams(int Channel, float BlendAlpha, const char *BoneName)
{
	CAnimChan &Chn = GetStage(Channel);
	Chn.BlendAlpha = BlendAlpha;
	if (Channel == 0)
		Chn.BlendAlpha = 1;		// force full animation for 1st stage
	Chn.RootBone = 0;
	if (BoneName)
	{
		Chn.RootBone = FindBone(BoneName);
		if (Chn.RootBone < 0)	// bone not found -- ignore animation
			Chn.BlendAlpha = 0;
	}
}


void CSkelMeshInstance::AnimStopLooping(int Channel)
{
	guard(CSkelMeshInstance::AnimStopLooping);
	GetStage(Channel).Looped = false;
	unguard;
}


void CSkelMeshInstance::FreezeAnimAt(float Time, int Channel)
{
	guard(CSkelMeshInstance::FreezeAnimAt);
	CAnimChan &Chn = GetStage(Channel);
	Chn.Time = Time;
	Chn.Rate = 0;
	unguard;
}


void CSkelMeshInstance::GetAnimParams(int Channel, const char *&AnimName,
	float &Frame, float &NumFrames, float &Rate) const
{
	guard(CSkelMeshInstance::GetAnimParams);

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	const CAnimChan      &Chn  = GetStage(Channel);
	if (!Anim || Chn.AnimIndex < 0 || Channel > MaxAnimChannel)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const FMeshAnimSeq &AnimSeq = Anim->AnimSeqs[Chn.AnimIndex];
	AnimName  = AnimSeq.Name;
	Frame     = Chn.Time;
	NumFrames = AnimSeq.NumFrames;
	Rate      = Chn.Rate;

	unguard;
}


void CSkelMeshInstance::UpdateAnimation(float TimeDelta)
{
	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;

	// prepare bone-to-channel map
	//?? optimize: update when animation changed only
	for (int i = 0; i < Mesh->Bones.Num(); i++)
		BoneData[i].FirstChannel = 0;

	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && Chn->AnimIndex == ANIM_UNASSIGNED)
			continue;
		// update tweening
		if (Chn->TweenTime)
		{
			Chn->TweenStep = TimeDelta / Chn->TweenTime;
			Chn->TweenTime -= TimeDelta;
			if (Chn->TweenTime < 0)
			{
				// stop tweening, start animation
				TimeDelta = -Chn->TweenTime;
				Chn->TweenTime = 0;
			}
			assert(Chn->Time == 0);
		}
		// note: TweenTime may be changed now, check again
		if (!Chn->TweenTime && Chn->AnimIndex >= 0)
		{
			// update animation time
			Chn->Time += TimeDelta * Chn->Rate;
			const FMeshAnimSeq &Seq = Anim->AnimSeqs[Chn->AnimIndex];
			if (Chn->Looped)
			{
				if (Chn->Time >= Seq.NumFrames)
				{
					// wrap time
					int numSkip = appFloor(Chn->Time / Seq.NumFrames);
					Chn->Time -= numSkip * Seq.NumFrames;
				}
			}
			else
			{
				if (Chn->Time >= Seq.NumFrames-1)
				{
					// clamp time
					Chn->Time = Seq.NumFrames-1;
					if (Chn->Time < 0)
						Chn->Time = 0;
				}
			}
		}
		// assign bones to channel
		if (Chn->BlendAlpha == 1.0f && Stage > 0) // stage 0 already set
		{
			//?? optimize: update when animation changed only
			int bone, count;
			for (bone = Chn->RootBone, count = BoneData[bone].SubtreeSize; count >= 0; count--, bone++)
				BoneData[bone].FirstChannel = Stage;
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

//		glColor3f(1,0,0);
//		glVertex3fv(BC.origin.v);
//		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,0.3);
			//!! REMOVE LATER:
			int t = BoneUpdateCounts[i];
			glColor3f(t & 1, (t >> 1) & 1, (t >> 2) & 1);
			//!! ^^^^^^^^^^^^^
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
