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
#define MAX_MESHBONES			256


/*-----------------------------------------------------------------------------
	Create/destroy
-----------------------------------------------------------------------------*/

CSkelMeshInstance::~CSkelMeshInstance()
{
	if (BoneData)  delete BoneData;
	if (MeshVerts) delete MeshVerts;
}


void CSkelMeshInstance::ClearSkelAnims()
{
	// init 1st animation channel with default pose
	for (int i = 0; i < MAX_SKELANIMCHANNELS; i++)
	{
		Channels[i].AnimIndex1     = ANIM_UNASSIGNED;
		Channels[i].AnimIndex2     = ANIM_UNASSIGNED;
		Channels[i].SecondaryBlend = 0;
		Channels[i].BlendAlpha     = 1;
		Channels[i].RootBone       = 0;
	}
}


/* Iterate bone (sub)tree and do following:
 *	- find all direct childs of bone 'Index', check sort order; bones should be
 *	  sorted in hierarchy order (1st child and its children first, other childs next)
 *	  Example of tree:
 *		Bone1
 *		+- Bone11
 *		|  +- Bone111
 *		+- Bone12
 *		|  +- Bone121
 *		|  +- Bone122
 *		+- Bone13
 *	  Sorted order: Bone1, Bone11, Bone111, Bone12, Bone121, Bone122, Bone13
 *	  Note: it seems, Unreal already have sorted bone list (assumed in other code,
 *	  verified by 'assert(currIndex == Index)')
 *	- compute subtree sizes ('Sizes' array)
 *	- compute bone hierarchy depth ('Depth' array, for debugging only)
 */
static int CheckBoneTree(const TArray<FMeshBone> &Bones, int Index,
	int *Sizes, int *Depth, int &numIndices, int maxIndices)
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
			treeSize += CheckBoneTree(Bones, i, Sizes, Depth, numIndices, maxIndices);
			depth--;
		}
	// store gathered information
	assert(currIndex == Index);
	Sizes[currIndex] = treeSize;
	Depth[currIndex] = depth;
	return treeSize + 1;
}


void CSkelMeshInstance::SetMesh(ULodMesh *LodMesh)
{
	guard(CSkelMeshInstance::SetMesh);

	CMeshInstance::SetMesh(LodMesh);
	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(LodMesh);

	int NumBones = Mesh->Bones.Num();
	int NumVerts = Mesh->Points.Num();
	const UMeshAnimation *Anim = Mesh->Animation;

	// allocate some arrays
	if (BoneData)  delete BoneData;
	if (MeshVerts) delete MeshVerts;
	BoneData  = new CMeshBoneData[NumBones];
	MeshVerts = new CVec3        [NumVerts];

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
	float *VertSumWeights = new float[NumVerts];	// zeroed
	int   *VertInfCount   = new int  [NumVerts];	// zeroed
	// count sum of weights for all verts
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &Inf = Mesh->VertInfluences[i];
		int PointIndex = Inf.PointIndex;
		assert(PointIndex < NumVerts);
		VertSumWeights[PointIndex] += Inf.Weight;
		VertInfCount  [PointIndex]++;
	}
#if 0
	// notify about wrong weights
	for (i = 0; i < NumVerts; i++)
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

	// check bones tree
	//?? should be done in USkeletalMesh
	int treeSizes[MAX_MESHBONES], depth[MAX_MESHBONES];
	int numIndices = 0;
	CheckBoneTree(Mesh->Bones, 0, treeSizes, depth, numIndices, MAX_MESHBONES);
	assert(numIndices == NumBones);
	for (i = 0; i < numIndices; i++)
		BoneData[i].SubtreeSize = treeSizes[i];	// remember subtree size

#if 1
	//?? dump tree; separate function (requires depth information)
	for (i = 0; i < numIndices; i++)
	{
		int parent = Mesh->Bones[i].ParentIndex;
		printf("bone#%2d (parent %2d); tree size: %2d   ", i, parent, treeSizes[i]);
		for (int j = 0; j < depth[i]; j++)
		{
	#if 0
			// simple picture
			printf("|  ");
	#else
			// graph-like picture
			bool found = false;
			for (int n = i+1; n < numIndices; n++)
			{
				if (depth[n] >  j+1) continue;
				if (depth[n] == j+1) found = true;
				break;
			}
		#if 0
			// ASCII
			if (j == depth[i]-1)
				printf(found ? "+--" : "\\--");
			else
				printf(found ? "|  " : "   ");
		#else
			// pseudographics
			if (j == depth[i]-1)
				printf(found ? "\xC3\xC4\xC4" : "\xC0\xC4\xC4");	// [+--] : [\--]
			else
                printf(found ? "\xB3  " : "   ");					// [|  ] : [   ]
        #endif
	#endif
		}
		printf("%s\n", *Mesh->Bones[i].Name);
	}
#endif

	ClearSkelAnims();
	PlayAnim(NULL);

	unguard;
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
	if (!Anim || !AnimName)
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
	Skeletal animation itself
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

	// process all animation channels
	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	memset(BoneUpdateCounts, 0, sizeof(BoneUpdateCounts)); //!! remove later
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && (Chn->AnimIndex1 == ANIM_UNASSIGNED || Chn->BlendAlpha <= 0))
			continue;

		const MotionChunk  *Motion1  = NULL, *Motion2  = NULL;
		const FMeshAnimSeq *AnimSeq1 = NULL, *AnimSeq2 = NULL;
		if (Chn->AnimIndex1 >= 0)
		{
			Motion1  = &Anim->Moves   [Chn->AnimIndex1];
			AnimSeq1 = &Anim->AnimSeqs[Chn->AnimIndex1];
			if (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
			{
				Motion2  = &Anim->Moves   [Chn->AnimIndex2];
				AnimSeq2 = &Anim->AnimSeqs[Chn->AnimIndex2];
			}
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
			{
				// this bone position will be overrided in following channel(s); all
				// subhierarchy bones should be overrided too; skip whole subtree
				int skip = data->SubtreeSize;
				// note: 'skip' equals to subtree size; current bone is excluded - it
				// will be skipped by 'for' operator (after 'continue')
				i    += skip;
				data += skip;
				continue;
			}

			CVec3 BP;
			CQuat BO;
			int BoneIndex = data->BoneMap;
			// compute bone orientation
			if (Motion1 && BoneIndex >= 0)
			{
				// get bone position from track
				if (!Motion2 || Chn->SecondaryBlend != 1.0f)
				{
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(Motion1->AnimTracks[BoneIndex], Chn->Time1, AnimSeq1->NumFrames,
						Chn->Looped, BP, BO);
				}
				// blend secondary animation
				if (Motion2)
				{
					CVec3 BP2;
					CQuat BO2;
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(Motion2->AnimTracks[BoneIndex], Chn->Time2, AnimSeq2->NumFrames,
						Chn->Looped, BP2, BO2);
					if (Chn->SecondaryBlend == 1.0f)
					{
						BO = BO2;
						BP = BP2;
					}
					else
					{
						Lerp (BP, BP2, Chn->SecondaryBlend, BP);
						Slerp(BO, BO2, Chn->SecondaryBlend, BO);
					}
				}
			}
			else
			{
				const FMeshBone &B = Mesh->Bones[i];
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
			if (Chn->BlendAlpha < 1.0f)
			{
				Lerp (data->Pos,  BP, Chn->BlendAlpha, BP);
				Slerp(data->Quat, BO, Chn->BlendAlpha, BO);
			}

			data->Quat = BO;
			data->Pos  = BP;
		}
	}

	// transform bones using skeleton hierarchy
	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < Mesh->Bones.Num(); i++, data++)
	{
		CCoords &BC = data->Coords;
		BC.origin = data->Pos;
		data->Quat.ToAxis(BC.axis);

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
		if (Stage > 0 && Chn->AnimIndex1 == ANIM_UNASSIGNED)
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
			assert(Chn->Time1 == 0);
		}
		// note: TweenTime may be changed now, check again
		if (!Chn->TweenTime && Chn->AnimIndex1 >= 0)
		{
			// update animation time
			const FMeshAnimSeq *Seq1 = &Anim->AnimSeqs[Chn->AnimIndex1];
			const FMeshAnimSeq *Seq2 = (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend) ?
				&Anim->AnimSeqs[Chn->AnimIndex2] : NULL;

			float Rate1 = Chn->Rate * Seq1->Rate;
			if (Seq2)
			{
				// if blending 2 channels, should adjust animation rate
				Rate1 = Lerp(Seq1->Rate / Seq1->NumFrames, Seq2->Rate / Seq2->NumFrames, Chn->SecondaryBlend)
					* Seq1->NumFrames;
			}
			Chn->Time1 += TimeDelta * Rate1;

			if (Chn->Looped)
			{
				if (Chn->Time1 >= Seq1->NumFrames)
				{
					// wrap time
					int numSkip = appFloor(Chn->Time1 / Seq1->NumFrames);
					Chn->Time1 -= numSkip * Seq1->NumFrames;
				}
			}
			else
			{
				if (Chn->Time1 >= Seq1->NumFrames-1)
				{
					// clamp time
					Chn->Time1 = Seq1->NumFrames-1;
					if (Chn->Time1 < 0)
						Chn->Time1 = 0;
				}
			}
			// compute time for secondary channel; always in sync with primary channel
			if (Seq2)
				Chn->Time2 = Chn->Time1 / Seq1->NumFrames * Seq2->NumFrames;
		}
		// assign bones to channel
		if (Chn->BlendAlpha >= 1.0f && Stage > 0) // stage 0 already set
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
	Animation setup
-----------------------------------------------------------------------------*/

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
		Chn.AnimIndex1     = -1;
		Chn.AnimIndex2     = -1;
		Chn.Time1          = 0;
		Chn.Rate           = 0;
		Chn.Looped         = false;
		Chn.TweenTime      = TweenTime;
		Chn.SecondaryBlend = 0;
		return;
	}

	Chn.Rate   = Rate;
	Chn.Looped = Looped;

	if (NewAnimIndex == Chn.AnimIndex1 && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	Chn.AnimIndex1     = NewAnimIndex;
	Chn.AnimIndex2     = -1;
	Chn.Time1          = 0;
	Chn.SecondaryBlend = 0;
	Chn.TweenTime      = TweenTime;

	unguard;
}


void CSkelMeshInstance::SetBlendParams(int Channel, float BlendAlpha, const char *BoneName)
{
	guard(CSkelMeshInstance::SetBlendParams);
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
	unguard;
}


void CSkelMeshInstance::SetBlendAlpha(int Channel, float BlendAlpha)
{
	guard(CSkelMeshInstance::SetBlendAlpha);
	GetStage(Channel).BlendAlpha = BlendAlpha;
	unguard;
}


void CSkelMeshInstance::SetSecondaryAnim(int Channel, const char *AnimName)
{
	guard(CSkelMeshInstance::SetSecondaryAnim);
	CAnimChan &Chn = GetStage(Channel);
	Chn.AnimIndex2     = FindAnim(AnimName);
	Chn.SecondaryBlend = 0;
	unguard;
}


void CSkelMeshInstance::SetSecondaryBlend(int Channel, float BlendAlpha)
{
	guard(CSkelMeshInstance::SetSecondaryBlend);
	GetStage(Channel).SecondaryBlend = BlendAlpha;
	unguard;
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
	Chn.Time1 = Time;
	Chn.Rate  = 0;
	unguard;
}


void CSkelMeshInstance::GetAnimParams(int Channel, const char *&AnimName,
	float &Frame, float &NumFrames, float &Rate) const
{
	guard(CSkelMeshInstance::GetAnimParams);

	const USkeletalMesh  *Mesh = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	const CAnimChan      &Chn  = GetStage(Channel);
	if (!Anim || Chn.AnimIndex1 < 0 || Channel > MaxAnimChannel)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const FMeshAnimSeq &AnimSeq = Anim->AnimSeqs[Chn.AnimIndex1];
	AnimName  = AnimSeq.Name;
	Frame     = Chn.Time1;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimSeq.Rate * Chn.Rate;

	unguard;
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
