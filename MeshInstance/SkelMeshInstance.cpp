#include "Core.h"

#include "UnrealClasses.h"
#include "MeshInstance.h"

#include "GlWindow.h"


struct CMeshBoneData
{
	// static data (computed after mesh loading)
	int			BoneMap;			// index of bone in animation tracks
	CCoords		RefCoords;			// coordinates of bone in reference pose (unused!!)
	CCoords		RefCoordsInv;		// inverse of RefCoords
	int			SubtreeSize;		// count of all children bones (0 for leaf bone)
	// dynamic data
	// skeleton configuration
	float		Scale;				// bone scale; 1=unscaled
	int			FirstChannel;		// first animation channel, affecting this bone
	// current pose
	CCoords		Coords;				// current coordinates of bone, model-space
	CCoords		Transform;			// used to transform vertex from reference pose to current pose
	// data for tweening; bone-space
	CVec3		Pos;				// current position of bone
	CQuat		Quat;				// current orientation quaternion
};


#define ANIM_UNASSIGNED			-2
#define MAX_MESHBONES			512


/*-----------------------------------------------------------------------------
	Create/destroy
-----------------------------------------------------------------------------*/

CSkelMeshInstance::~CSkelMeshInstance()
{
	if (BoneData)
	{
		delete BoneData;
		delete MeshVerts;
		delete MeshNormals;
		delete RefNormals;
	}
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


static void BuildNormals(const USkeletalMesh *Mesh, CVec3 *Normals)
{
	int i;
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &Face = Mesh->Triangles[i];
		// get vertex indices
		int i1 = Mesh->Wedges[Face.WedgeIndex[0]].iVertex;
		int i2 = Mesh->Wedges[Face.WedgeIndex[1]].iVertex;
		int i3 = Mesh->Wedges[Face.WedgeIndex[2]].iVertex;
		// compute edges
		const CVec3 &V1 = (CVec3&)Mesh->Points[i1];
		const CVec3 &V2 = (CVec3&)Mesh->Points[i2];
		const CVec3 &V3 = (CVec3&)Mesh->Points[i3];
		CVec3 D1, D2, D3;
		VectorSubtract(V2, V1, D1);
		VectorSubtract(V3, V2, D2);
		VectorSubtract(V1, V3, D3);
		// compute normal
		CVec3 norm;
		cross(D2, D1, norm);
		norm.Normalize();
		// compute angles
		D1.Normalize();
		D2.Normalize();
		D3.Normalize();
		float angle1 = acos(-dot(D1, D3));
		float angle2 = acos(-dot(D1, D2));
		float angle3 = acos(-dot(D2, D3));
		// add normals for triangle verts
		VectorMA(Normals[i1], angle1, norm);
		VectorMA(Normals[i2], angle2, norm);
		VectorMA(Normals[i3], angle3, norm);
	}
	// normalize normals
	for (i = 0; i < Mesh->Points.Num(); i++)
		Normals[i].Normalize();
}


void CSkelMeshInstance::SetMesh(const ULodMesh *LodMesh)
{
	guard(CSkelMeshInstance::SetMesh);

	CMeshInstance::SetMesh(LodMesh);
	const USkeletalMesh *Mesh = static_cast<const USkeletalMesh*>(LodMesh);

	int NumBones = Mesh->RefSkeleton.Num();
	int NumVerts = Mesh->Points.Num();
	const UMeshAnimation *Anim = Mesh->Animation;

	// allocate some arrays
	if (BoneData)
	{
		delete BoneData;
		delete MeshVerts;
		delete MeshNormals;
		delete RefNormals;
	}
	BoneData    = new CMeshBoneData[NumBones];
	MeshVerts   = new CVec3        [NumVerts];
	MeshNormals = new CVec3        [NumVerts];
	RefNormals  = new CVec3        [NumVerts];

	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < NumBones; i++, data++)
	{
		const FMeshBone &B = Mesh->RefSkeleton[i];
		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);

		// find reference bone in animation track
		data->BoneMap = INDEX_NONE;
		if (Anim)
		{
			for (int j = 0; j < Anim->RefBones.Num(); j++)
				if (!stricmp(B.Name, Anim->RefBones[j].Name))
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
			BoneData[B.ParentIndex].RefCoords.UnTransformCoords(BC, BC);
		// store inverted transformation too
		InvertCoords(data->RefCoords, data->RefCoordsInv);
		// initialize skeleton configuration
		data->Scale = 1.0f;			// default bone scale
	}

	BuildNormals(Mesh, RefNormals);

	// normalize VertInfluences: sum of all influences may be != 1
	// (possible situation, see SkaarjAnims/Skaarj2, SkaarjAnims/Skaarj_Skel, XanRobots/XanF02)
	//!! should be done in USkeletalMesh
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
		FVertInfluences &Inf = const_cast<USkeletalMesh*>(Mesh)->VertInfluences[i]; // to avoid const_cast, implement in mesh
		int PointIndex = Inf.PointIndex;
		float sum = VertSumWeights[PointIndex];
		if (fabs(sum - 1.0f) < 0.01f) continue;
		assert(sum > 0.01f);	// no division by zero
		Inf.Weight = Inf.Weight / sum;
	}

	delete VertSumWeights;
	delete VertInfCount;

	// check bones tree
	//!! should be done in USkeletalMesh
	int treeSizes[MAX_MESHBONES], depth[MAX_MESHBONES];
	int numIndices = 0;
	CheckBoneTree(Mesh->RefSkeleton, 0, treeSizes, depth, numIndices, MAX_MESHBONES);
	assert(numIndices == NumBones);
	for (i = 0; i < numIndices; i++)
		BoneData[i].SubtreeSize = treeSizes[i];	// remember subtree size

	ClearSkelAnims();
	PlayAnim(NULL);

	unguard;
}


void CSkelMeshInstance::DumpBones()
{
#if 1
	const USkeletalMesh *Mesh = static_cast<const USkeletalMesh*>(pMesh);
	int treeSizes[MAX_MESHBONES], depth[MAX_MESHBONES];
	int numIndices = 0;
	CheckBoneTree(Mesh->RefSkeleton, 0, treeSizes, depth, numIndices, MAX_MESHBONES);
	//?? dump tree; separate function (requires depth information)
	for (int i = 0; i < numIndices; i++)
	{
		int parent = Mesh->RefSkeleton[i].ParentIndex;
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
		#if _WIN32
			// pseudographics
			if (j == depth[i]-1)
				printf(found ? "\xC3\xC4\xC4" : "\xC0\xC4\xC4");	// [+--] : [\--]
			else
                printf(found ? "\xB3  " : "   ");					// [|  ] : [   ]
		#else
			// ASCII
			if (j == depth[i]-1)
				printf(found ? "+--" : "\\--");
			else
				printf(found ? "|  " : "   ");
        #endif
	#endif
		}
		printf("%s\n", *Mesh->RefSkeleton[i].Name);
	}
#endif
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

int CSkelMeshInstance::FindBone(const char *BoneName) const
{
	const USkeletalMesh *Mesh = GetMesh();
	for (int i = 0; i < Mesh->RefSkeleton.Num(); i++)
		if (!strcmp(Mesh->RefSkeleton[i].Name, BoneName))
			return i;
	return INDEX_NONE;
}


int CSkelMeshInstance::FindAnim(const char *AnimName) const
{
	const USkeletalMesh *Mesh  = GetMesh();
	const UMeshAnimation *Anim = Mesh->Animation;
	if (!Anim || !AnimName)
		return INDEX_NONE;
	for (int i = 0; i < Anim->AnimSeqs.Num(); i++)
		if (!strcmp(Anim->AnimSeqs[i].Name, AnimName))
			return i;
	return INDEX_NONE;
}


void CSkelMeshInstance::SetBoneScale(const char *BoneName, float scale)
{
	int BoneIndex = FindBone(BoneName);
	if (BoneIndex == INDEX_NONE) return;
	BoneData[BoneIndex].Scale = scale;
}


/*-----------------------------------------------------------------------------
	Skeletal animation itself
-----------------------------------------------------------------------------*/

#define MAX_LINEAR_KEYS		4

//!! DEBUGGING, remove later
#define DEBUG_BIN_SEARCH	1
#if 0
#	define DBG		printf
#else
#	define DBG		if (1) {} else printf
#endif
//!! ^^^^^^


// not 'static', because used in ExportPsa()
//?? place function into UMeshAnimation
void GetBonePosition(const AnalogTrack &A, float Frame, float NumFrames, bool Loop,
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
	int NumKeys = A.KeyTime.Num();
	// *** binary search ***
	int Low = 0, High = NumKeys-1;
	DBG(">>> find %.5f\n", Frame);
	while (Low + MAX_LINEAR_KEYS < High)
	{
		int Mid = (Low + High) / 2;
		DBG("   [%d..%d] mid: [%d]=%.5f", Low, High, Mid, A.KeyTime[Mid]);
		if (Frame < A.KeyTime[Mid])
			High = Mid-1;
		else
			Low = Mid;
		DBG("   d=%f\n", A.KeyTime[Mid]-Frame);
	}

	// *** linear search ***
	DBG("   linear: %d..%d\n", Low, High);
	for (i = Low; i <= High; i++)
	{
		float CurrKeyTime = A.KeyTime[i];
		DBG("   #%d: %.5f\n", i, CurrKeyTime);
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? (CVec3&)A.KeyPos[i]  : (CVec3&)A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? (CQuat&)A.KeyQuat[i] : (CQuat&)A.KeyQuat[0];
			return;
		}
		if (Frame < CurrKeyTime)
		{
			i--;
			break;
		}
	}
	if (i > High)
		i = High;

#if DEBUG_BIN_SEARCH
	//!! --- checker ---
	int i1;
	for (i1 = 0; i1 < NumKeys; i1++)
	{
		float CurrKeyTime = A.KeyTime[i1];
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? (CVec3&)A.KeyPos[i]  : (CVec3&)A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? (CQuat&)A.KeyQuat[i] : (CQuat&)A.KeyQuat[0];
			return;
		}
		if (Frame < CurrKeyTime)
		{
			i1--;
			break;
		}
	}
	if (i1 > NumKeys-1)
		i1 = NumKeys-1;
	if (i != i1)
	{
		appError("i=%d != i1=%d", i, i1);
	}
#endif

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


static int BoneUpdateCounts[MAX_MESHBONES];	//!! remove later

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
		float Time2;
		if (Chn->AnimIndex1 >= 0)		// not INDEX_NONE or ANIM_UNASSIGNED
		{
			Motion1  = &Anim->Moves   [Chn->AnimIndex1];
			AnimSeq1 = &Anim->AnimSeqs[Chn->AnimIndex1];
			if (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
			{
				Motion2  = &Anim->Moves   [Chn->AnimIndex2];
				AnimSeq2 = &Anim->AnimSeqs[Chn->AnimIndex2];
				// compute time for secondary channel; always in sync with primary channel
				Time2 = Chn->Time / AnimSeq1->NumFrames * AnimSeq2->NumFrames;
			}
		}

		// compute bone range, affected by specified animation bone
		int firstBone = Chn->RootBone;
		int lastBone  = firstBone + BoneData[firstBone].SubtreeSize;
		assert(lastBone < Mesh->RefSkeleton.Num());

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
			// check for disabled bone (required for Tribes3)
			if (Motion1 && Motion1->BoneIndices.Num() && BoneIndex != INDEX_NONE &&
				Motion1->BoneIndices[BoneIndex] == INDEX_NONE)
				BoneIndex = INDEX_NONE;		// will use RefSkeleton for this bone
			// compute bone orientation
			if (Motion1 && BoneIndex != INDEX_NONE)
			{
				// get bone position from track
				if (!Motion2 || Chn->SecondaryBlend != 1.0f)
				{
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(Motion1->AnimTracks[BoneIndex], Chn->Time, AnimSeq1->NumFrames,
						Chn->Looped, BP, BO);
				}
				// blend secondary animation
				if (Motion2)
				{
					CVec3 BP2;
					CQuat BO2;
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(Motion2->AnimTracks[BoneIndex], Time2, AnimSeq2->NumFrames,
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
				// get default bone position
				const FMeshBone &B = Mesh->RefSkeleton[i];
				BP = (CVec3&)B.BonePos.Position;
				BO = (CQuat&)B.BonePos.Orientation;
			}
			if (!i) BO.Conjugate();

			// tweening
			if (Chn->TweenTime > 0)
			{
				// interpolate orientation using AnimTweenStep
				// current orientation -> {BP,BO}
				Lerp (data->Pos,  BP, Chn->TweenStep, BP);
				Slerp(data->Quat, BO, Chn->TweenStep, BO);
			}
			// blending with previous channels
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
	for (i = 0, data = BoneData; i < Mesh->RefSkeleton.Num(); i++, data++)
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
			BoneData[Mesh->RefSkeleton[i].ParentIndex].Coords.UnTransformCoords(BC, BC);
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
	for (int i = 0; i < Mesh->RefSkeleton.Num(); i++)
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
			assert(Chn->Time == 0);
		}
		// note: TweenTime may be changed now, check again
		if (!Chn->TweenTime && Chn->AnimIndex1 >= 0)
		{
			// update animation time
			const FMeshAnimSeq *Seq1 = &Anim->AnimSeqs[Chn->AnimIndex1];
			const FMeshAnimSeq *Seq2 = (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
				? &Anim->AnimSeqs[Chn->AnimIndex2]
				: NULL;

			float Rate1 = Chn->Rate * Seq1->Rate;
			if (Seq2)
			{
				// if blending 2 channels, should adjust animation rate
				Rate1 = Lerp(Seq1->Rate / Seq1->NumFrames, Seq2->Rate / Seq2->NumFrames, Chn->SecondaryBlend)
					* Seq1->NumFrames;
			}
			Chn->Time += TimeDelta * Rate1;

			if (Chn->Looped)
			{
				if (Chn->Time >= Seq1->NumFrames)
				{
					// wrap time
					int numSkip = appFloor(Chn->Time / Seq1->NumFrames);
					Chn->Time -= numSkip * Seq1->NumFrames;
				}
			}
			else
			{
				if (Chn->Time >= Seq1->NumFrames-1)
				{
					// clamp time
					Chn->Time = Seq1->NumFrames-1;
					if (Chn->Time < 0)
						Chn->Time = 0;
				}
			}
		}
		// assign bones to channel
		if (Chn->BlendAlpha >= 1.0f && Stage > 0) // stage 0 already set
		{
			// whole subtree will be skipped in UpdateSkeleton(), so - mark root bone only
			BoneData[Chn->RootBone].FirstChannel = Stage;
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
	if (NewAnimIndex == INDEX_NONE)
	{
		// show default pose
		Chn.AnimIndex1     = INDEX_NONE;
		Chn.AnimIndex2     = INDEX_NONE;
		Chn.Time           = 0;
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
	Chn.AnimIndex2     = INDEX_NONE;
	Chn.Time           = 0;
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
		if (Chn.RootBone == INDEX_NONE)	// bone not found -- ignore animation
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
	Frame     = Chn.Time;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimSeq.Rate * Chn.Rate;

	unguard;
}


/*-----------------------------------------------------------------------------
	Drawing
-----------------------------------------------------------------------------*/

void CSkelMeshInstance::DrawSkeleton(bool ShowLabels)
{
	guard(CSkelMeshInstance::DrawSkeleton);

	const USkeletalMesh *Mesh = GetMesh();

	glDisable(GL_TEXTURE_2D);
	glLineWidth(3);
	glEnable(GL_LINE_SMOOTH);

	glBegin(GL_LINES);
	for (int i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
		const FMeshBone &B  = Mesh->RefSkeleton[i];
		const CCoords   &BC = BoneData[i].Coords;

		CVec3 v1;
		if (i > 0)
		{
			glColor3f(1,1,0.3);
			//!! REMOVE LATER:
			int t = BoneUpdateCounts[i];
			glColor3f(t & 1, (t >> 1) & 1, (t >> 2) & 1);
			//!! ^^^^^^^^^^^^^
			v1 = BoneData[B.ParentIndex].Coords.origin;
		}
		else
		{
			glColor3f(1,0,1);
			v1.Zero();
		}
		glVertex3fv(v1.v);
		glVertex3fv(BC.origin.v);

		if (ShowLabels)
		{
			// show bone label
			v1.Add(BC.origin);
			v1.Scale(0.5f);
			DrawText3D(v1, S_YELLOW"(%d)%s", i, *B.Name);
		}
	}
	glColor3f(1,1,1);
	glEnd();

	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);

	unguard;
}

void CSkelMeshInstance::DrawAttachments()
{
	guard(CSkelMeshInstance::DrawAttachments);

	const USkeletalMesh *Mesh = GetMesh();
	if (!Mesh->AttachAliases.Num()) return;

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_LINES);
	for (int i = 0; i < Mesh->AttachAliases.Num(); i++)
	{
		const char *BoneName = Mesh->AttachBoneNames[i];
		int BoneIndex = FindBone(BoneName);
		if (BoneIndex == INDEX_NONE) continue;		// should not happen

		CCoords AC;
		BoneData[BoneIndex].Coords.UnTransformCoords((CCoords&)Mesh->AttachCoords[i], AC);

		for (int j = 0; j < 3; j++)
		{
			float color[3];
			CVec3 point0, point1;

			color[0] = color[1] = color[2] = 0.1f; color[j] = 1.0f;
			point0.Set(0, 0, 0); point0[j] = 10;
			AC.UnTransformPoint(point0, point1);

			glColor3fv(color);
			glVertex3fv(AC.origin.v);
			glVertex3fv(point1.v);
		}

		// show attachment label
		CVec3 labelOrigin;
		static const CVec3 origin0 = { 4, 4, 4 };
		AC.UnTransformPoint(origin0, labelOrigin);
		DrawText3D(labelOrigin, S_GREEN"%s\n(%s)", *Mesh->AttachAliases[i], BoneName);
	}
	glColor3f(1,1,1);
	glEnd();

	unguard;
}


void CSkelMeshInstance::TransformMesh(int NumInfs, const FVertInfluences *Infs, int NumVerts, const FVector *Verts, const CVec3 *Norms)
{
	guard(CSkelMeshInstance::TransformMesh);
	//?? try other tech: compute weighted sum of matrices, and then
	//?? transform vector (+ normal ?; note: normals requires normalized
	//?? transformations ?? (or normalization guaranteed by sum(weights)==1?))
	memset(MeshVerts,   0, sizeof(CVec3) * NumVerts);
	if (Norms)
		memset(MeshNormals, 0, sizeof(CVec3) * NumVerts);

	for (int i = 0; i < NumInfs; i++)
	{
		const FVertInfluences &Inf  = Infs[i];
		const CMeshBoneData   &data = BoneData[Inf.BoneIndex];
		int PointIndex = Inf.PointIndex;
		CVec3 tmp;
		// transform vertex
		data.Transform.UnTransformPoint((CVec3&)Verts[PointIndex], tmp);
		VectorMA(MeshVerts[PointIndex], Inf.Weight, tmp);
		// transform normal
		if (Norms)
		{
			data.Transform.axis.UnTransformVector(Norms[PointIndex], tmp);
			VectorMA(MeshNormals[PointIndex], Inf.Weight, tmp);
		}
	}
	unguard;
}


void CSkelMeshInstance::DrawBaseSkeletalMesh(bool ShowNormals)
{
	guard(CSkelMeshInstance::DrawBaseSkeletalMesh);
	int i;

	const USkeletalMesh *Mesh = GetMesh();

	TransformMesh(Mesh->VertInfluences.Num(), &Mesh->VertInfluences[0], Mesh->Points.Num(), &Mesh->Points[0], RefNormals);

	glEnable(GL_LIGHTING);
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &Face = Mesh->Triangles[i];
		SetMaterial(Face.MatIndex);
		glBegin(GL_TRIANGLES);
		for (int j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[Face.WedgeIndex[j]];
			glTexCoord2f(W.TexUV.U, W.TexUV.V);
			glNormal3fv(MeshNormals[W.iVertex].v);
			glVertex3fv(MeshVerts[W.iVertex].v);
		}
		glEnd();
	}
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);

	// draw mesh normals
	if (ShowNormals)
	{
		glBegin(GL_LINES);
		glColor3f(0.5, 1, 0);
		for (i = 0; i < Mesh->Points.Num(); i++)
		{
			glVertex3fv(MeshVerts[i].v);
			CVec3 tmp;
			VectorMA(MeshVerts[i], 2, MeshNormals[i], tmp);
			glVertex3fv(tmp.v);
		}
		glEnd();
	}

	unguard;
}


void CSkelMeshInstance::DrawLodSkeletalMesh(const FStaticLODModel *lod)
{
	guard(CSkelMeshInstance::DrawLodSkeletalMesh);
	int i, sec;

	//!! Algorithm: similar to DrawBaseSkeletalMesh(): prepare MeshVerts array
	//!! using lod->Points and lod->VertInfluences; should build normals!
	//!! For Lineage should use FLineageWedge structure - it's enough; better -
	//!! - restore Points, Wedges and VertInfluences arrays using 'unk2'
	//!! (problem: should share verts between wedges); also restore mesh by lod#0

	TransformMesh(lod->VertInfluences.Num(), &lod->VertInfluences[0], lod->Points.Num(), &lod->Points[0], NULL);

/*	// skinning stream
	for (i = 0; i < lod->SkinningData.Num(); i++)
	{
		int n = lod->SkinningData[i];
		float &f = (float&)n;
		DrawTextRight("%4d : %08X : %10.3f", i, n, f);
	}
	// influences
	for (i = 0; i < lod->VertInfluences.Num(); i++)
	{
		const FVertInfluences &I = lod->VertInfluences[i];
		DrawTextLeft("b%4d : p%4d : %7.3f", I.BoneIndex, I.PointIndex, I.Weight);
	}
*/

	// smooth sections (influence count >= 2)
	for (sec = 0; sec < lod->SmoothSections.Num(); sec++)
	{
		const FSkelMeshSection &ms = lod->SmoothSections[sec];
		SetMaterial(ms.MaterialIndex);
		glBegin(GL_TRIANGLES);
#if 1
		for (i = 0; i < ms.NumFaces; i++)
		{
			const FMeshFace &F = lod->Faces[ms.FirstFace + i];
			//?? ignore F.MaterialIndex - may be any
//			assert(F.MaterialIndex == ms.MaterialIndex);
			for (int j = 0; j < 3; j++)
			{
				const FMeshWedge &W = lod->Wedges[F.iWedge[j]];
				glTexCoord2f(W.TexUV.U, W.TexUV.V);
//				glVertex3fv(&lod->SkinPoints[W.iVertex].Point.X);
				glVertex3fv(MeshVerts[W.iVertex].v);
			}
		}
#else
		for (i = 0; i < ms.NumFaces; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				const FMeshWedge &W = lod->Wedges[lod->SmoothIndices.Indices[(ms.FirstFace + i) * 3 + j]];
				glTexCoord2f(W.TexUV.U, W.TexUV.V);
				glVertex3fv(&lod->SkinPoints[W.iVertex].Point.X);
			}
		}
#endif
		glEnd();
	}
	// rigid sections (influence count == 1)
	for (sec = 0; sec < lod->RigidSections.Num(); sec++)
	{
		const FSkelMeshSection &ms = lod->RigidSections[sec];
		SetMaterial(ms.MaterialIndex);
		glBegin(GL_TRIANGLES);
#if 1
		for (i = 0; i < ms.NumFaces; i++)
		{
//			const FMeshFace &F = lod->Faces[ms.FirstFace + i];
			for (int j = 0; j < 3; j++)
			{
//				const FMeshWedge &W = lod->Wedges[F.iWedge[j]];
				const FMeshWedge &W = lod->Wedges[lod->RigidIndices.Indices[(ms.FirstFace + i) * 3 + j]];
				glTexCoord2f(W.TexUV.U, W.TexUV.V);
//				glVertex3fv(&lod->Points[W.iVertex].X);
				glVertex3fv(MeshVerts[W.iVertex].v);
			}
		}
#else
		for (i = ms.MinStreamIndex; i < ms.MinStreamIndex + ms.NumStreamIndices; i++)
		{
			const FAnimMeshVertex &V = lod->VertexStream.Verts[lod->RigidIndices.Indices[i]];
			glTexCoord2f(V.Tex.U, V.Tex.V);
			glVertex3fv(&V.Pos.X);
		}
#endif
		glEnd();
	}
	unguard;
}


void CSkelMeshInstance::Draw()
{
	guard(CSkelMeshInstance::Draw);

	const USkeletalMesh *Mesh = GetMesh();

	// show skeleton
	if (ShowSkel)			//?? move this part to CSkelMeshViewer; call Inst->DrawSkeleton() etc
		DrawSkeleton(ShowLabels);
	// show mesh
	if (ShowSkel != 2)
	{
		if (LodNum < 0)
			DrawBaseSkeletalMesh(bShowNormals);
		else
			DrawLodSkeletalMesh(&Mesh->LODModels[LodNum]);
	}
	if (ShowAttach)
		DrawAttachments();

	unguard;
}
