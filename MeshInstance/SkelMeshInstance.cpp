#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "SkeletalMesh.h"
#include "MeshInstance.h"

#include "GlWindow.h"
#include "UnMathTools.h"


// debugging
//#define SHOW_INFLUENCES		1
#define SHOW_TANGENTS			1
//#define SHOW_ANIM				1
#define SHOW_BONE_UPDATES		1
//#define PROFILE_MESH			1
//#define TICK_SECTIONS			1

#define SORT_BY_OPACITY			1

#if TICK_SECTIONS
#undef SORT_BY_OPACITY				// to not obfuscate section indices
#endif


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
#if USE_SSE
	CCoords4	Transform4;			// SSE version
#endif
	// data for tweening; bone-space
	CVec3		Pos;				// current position of bone
	CQuat		Quat;				// current orientation quaternion
};


// transformed vertex (cutoff version of CSkelMeshVertex)
struct CSkinVert
{
	CVecT		Position;
	CVec4		Normal;				// force to have 4 components - W is used for binormal decoding
	CVecT		Tangent;
//	CVecT		Binormal;
};


#define MAX_MESHMATERIALS		256


/*-----------------------------------------------------------------------------
	Interface
-----------------------------------------------------------------------------*/

int CSkelMeshInstance::GetAnimCount() const
{
	if (!Animation) return 0;
	return Animation->Sequences.Num();
}


const char *CSkelMeshInstance::GetAnimName(int Index) const
{
	guard(CSkelMeshInstance::GetAnimName);
	if (Index < 0) return "None";
	assert(Animation);
	return Animation->Sequences[Index]->Name;
	unguard;
}


/*-----------------------------------------------------------------------------
	Create/destroy
-----------------------------------------------------------------------------*/

CSkelMeshInstance::CSkelMeshInstance()
:	LodNum(0)
,	UVIndex(0)
,	RotationMode(EARO_AnimSet)
,	LastLodNum(-2)				// differs from LodNum and from all other values
,	MaxAnimChannel(-1)
,	Animation(NULL)
,	DataBlock(NULL)
,	BoneData(NULL)
,	Skinned(NULL)
,	InfColors(NULL)
{
	ClearSkelAnims();
}


CSkelMeshInstance::~CSkelMeshInstance()
{
	if (DataBlock) appFree(DataBlock);
	if (InfColors) delete[] InfColors;
	if (pMesh) pMesh->UnlockMaterials();
}


void CSkelMeshInstance::ClearSkelAnims()
{
	// init 1st animation channel with default pose
	for (int i = 0; i < MAX_SKELANIMCHANNELS; i++)
	{
		Channels[i].Anim1          = NULL;
		Channels[i].Anim2          = NULL;
		Channels[i].SecondaryBlend = 0;
		Channels[i].BlendAlpha     = 1;
		Channels[i].RootBone       = 0;
	}
}


//!! CSkeletalMesh has SortBones() method now!
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
static int CheckBoneTree(const TArray<CSkelMeshBone> &Bones, int Index,
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
//	assert(currIndex == Index);		//??
	if (currIndex != Index) appNotify("Strange skeleton, check childs of bone %d (%s)", currIndex, *Bones[currIndex].Name);
	Sizes[currIndex] = treeSize;
	Depth[currIndex] = depth;
	return treeSize + 1;
}


void CSkelMeshInstance::SetMesh(CSkeletalMesh *Mesh)
{
	guard(CSkelMeshInstance::SetMesh);

	int i;

	assert(pMesh == NULL);
	pMesh = Mesh;
	pMesh->LockMaterials();

	// orientation

	RotatorToAxis(pMesh->RotOrigin, BaseTransformScaled.axis);
	BaseTransformScaled.axis[0].Scale(pMesh->MeshScale[0]);
	BaseTransformScaled.axis[1].Scale(pMesh->MeshScale[1]);
	BaseTransformScaled.axis[2].Scale(pMesh->MeshScale[2]);
	CVec3 tmp;
	VectorNegate(pMesh->MeshOrigin, tmp);
	BaseTransformScaled.axis.UnTransformVector(tmp, BaseTransformScaled.origin);

	int NumBones = pMesh->RefSkeleton.Num();
	int NumVerts = 0;
	for (i = 0; i < pMesh->Lods.Num(); i++)
		if (pMesh->Lods[i].NumVerts > NumVerts)
			NumVerts = pMesh->Lods[i].NumVerts;

	// free old data
	if (DataBlock) appFree(DataBlock);
	if (InfColors)
	{
		delete[] InfColors;
		InfColors = NULL;
	}
	// allocate data arrays in a single block
	int DataSize = sizeof(CMeshBoneData) * NumBones + sizeof(CSkinVert) * NumVerts;
	DataBlock = appMalloc(DataSize, 16);
	BoneData  = (CMeshBoneData*)DataBlock;
	Skinned   = (CSkinVert*)(BoneData + NumBones);

	LastLodNum = -2;

	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < NumBones; i++, data++)
	{
		const CSkelMeshBone &B = Mesh->RefSkeleton[i];
		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);

		// reset animation bone map (will be set by SetAnim())
		data->BoneMap = INDEX_NONE;

		// compute reference bone coords
		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = B.Position;
		BO = B.Orientation;
		if (!i) BO.Conjugate();

		CCoords &BC = data->RefCoords;
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			BoneData[B.ParentIndex].RefCoords.UnTransformCoords(BC, BC);
		// store inverted transformation too
		InvertCoords(data->RefCoords, data->RefCoordsInv);
#if 0
	//!!
if (i == 32 || i == 34)
{
	appNotify("Bone %d (%8.3f %8.3f %8.3f) - (%8.3f %8.3f %8.3f %8.3f)", i, VECTOR_ARG(BP), QUAT_ARG(BO));
#define C data->RefCoords
	appNotify("REF   : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	appNotify("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	appNotify("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	appNotify("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
#define C data->RefCoordsInv
	appNotify("REFIN : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	appNotify("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	appNotify("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	appNotify("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
}
#endif
		// initialize skeleton configuration
		data->Scale = 1.0f;			// default bone scale
	}

	// check bones tree
	if (NumBones)
	{
		//!! should be done in CSkeletalMesh
		guard(VerifySkeleton);
		int treeSizes[MAX_MESHBONES], depth[MAX_MESHBONES];
		int numIndices = 0;
		CheckBoneTree(Mesh->RefSkeleton, 0, treeSizes, depth, numIndices, MAX_MESHBONES);
		assert(numIndices == NumBones);
		for (i = 0; i < numIndices; i++)
			BoneData[i].SubtreeSize = treeSizes[i];	// remember subtree size
		unguard;
	}

	PlayAnim(NULL);

	unguard;
}


void CSkelMeshInstance::SetAnim(const CAnimSet *Anim)
{
	if (!pMesh) return;		// mesh is not set yet

	Animation = Anim;

	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < pMesh->RefSkeleton.Num(); i++, data++)
	{
		const CSkelMeshBone &B = pMesh->RefSkeleton[i];

		// find reference bone in animation track
		data->BoneMap = INDEX_NONE;		// in a case when bone has no corresponding animation track
		if (Animation)
		{
			for (int j = 0; j < Animation->TrackBoneNames.Num(); j++)
				if (!stricmp(B.Name, Animation->TrackBoneNames[j]))
				{
					data->BoneMap = j;
					break;
				}
		}
	}

	ClearSkelAnims();
	PlayAnim(NULL);
}


void CSkelMeshInstance::DumpBones()
{
	int treeSizes[MAX_MESHBONES], depth[MAX_MESHBONES];
	int numIndices = 0;
	CheckBoneTree(pMesh->RefSkeleton, 0, treeSizes, depth, numIndices, MAX_MESHBONES);
	//?? dump tree; separate function (requires depth information)
	for (int i = 0; i < numIndices; i++)
	{
		const CSkelMeshBone &B = pMesh->RefSkeleton[i];
		int parent = B.ParentIndex;
		appPrintf("bone#%3d (parent %3d); tree size: %3d   ", i, parent, treeSizes[i]);
#if 1
		for (int j = 0; j < depth[i]; j++)
		{
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
				appPrintf(found ? "\xC3\xC4\xC4" : "\xC0\xC4\xC4");	// [+--] : [\--]
			else
                appPrintf(found ? "\xB3  " : "   ");				// [|  ] : [   ]
		#else
			// ASCII
			if (j == depth[i]-1)
				appPrintf(found ? "+--" : "\\--");
			else
				appPrintf(found ? "|  " : "   ");
        #endif
		}
		appPrintf("%s\n", *B.Name);
#else
		appPrintf("%s {%g %g %g} {%g %g %g %g}\n", *B.Name, VECTOR_ARG(B.Position), QUAT_ARG(B.Orientation));
#endif
	}
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

int CSkelMeshInstance::FindBone(const char *BoneName) const
{
	for (int i = 0; i < pMesh->RefSkeleton.Num(); i++)
		if (!strcmp(pMesh->RefSkeleton[i].Name, BoneName))
			return i;
	return INDEX_NONE;
}


const CAnimSequence *CSkelMeshInstance::FindAnim(const char *AnimName) const
{
	if (!Animation || !AnimName)
		return NULL;
	for (int i = 0; i < Animation->Sequences.Num(); i++)
	{
		const CAnimSequence &Seq = *Animation->Sequences[i];
		if (!stricmp(Seq.Name, AnimName))
			return &Seq;
	}
	return NULL;
}


void CSkelMeshInstance::SetBoneScale(const char *BoneName, float scale)
{
	int BoneIndex = FindBone(BoneName);
	if (BoneIndex == INDEX_NONE) return;
	BoneData[BoneIndex].Scale = scale;
}


CVec3 CSkelMeshInstance::GetMeshOrigin() const
{
	if (!BoneData)
		return nullVec3;
	// find first animated bone
	int BoneIndex = pMesh->GetRootBone();	//?? cache this value
	if (BoneIndex < 0)
		BoneIndex = 0;
//	appPrintf("Focus on bone %d (%s)\n", BoneIndex, *pMesh->RefSkeleton[BoneIndex].Name);
	return BoneData[BoneIndex].Coords.origin;
}


/*-----------------------------------------------------------------------------
	Skeletal animation itself
-----------------------------------------------------------------------------*/

#if SHOW_BONE_UPDATES
static int BoneUpdateCounts[MAX_MESHBONES];
#endif

void CSkelMeshInstance::UpdateSkeleton()
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	// process all animation channels
	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
#if SHOW_BONE_UPDATES
	memset(BoneUpdateCounts, 0, sizeof(BoneUpdateCounts));
#endif
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && (Chn->Anim1 == NULL || Chn->BlendAlpha <= 0))
			continue;

		const CAnimSequence *AnimSeq1 = Chn->Anim1;
		const CAnimSequence *AnimSeq2 = NULL;
		float Time2;
		if (AnimSeq1)
		{
			if (Chn->Anim2 && Chn->SecondaryBlend)
			{
				AnimSeq2 = Chn->Anim2;
				// compute time for secondary channel; always in sync with primary channel
				Time2 = Chn->Time / AnimSeq1->NumFrames * AnimSeq2->NumFrames;
			}
		}

		// compute bone range, affected by specified animation bone
		int firstBone = Chn->RootBone;
		int lastBone  = firstBone + BoneData[firstBone].SubtreeSize;
		assert(lastBone < pMesh->RefSkeleton.Num());

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
			const CSkelMeshBone &Bone = pMesh->RefSkeleton[i];
			BP = Bone.Position;					// default position - from bind pose
			BO = Bone.Orientation;				// ...

			int BoneIndex = data->BoneMap;

			// compute bone orientation
			if (AnimSeq1 && BoneIndex != INDEX_NONE)
			{
				// get bone position from track
				if (!AnimSeq2 || Chn->SecondaryBlend != 1.0f)
				{
					AnimSeq1->Tracks[BoneIndex].GetBonePosition(Chn->Time, AnimSeq1->NumFrames, Chn->Looped, BP, BO);
//const char *bname = *Bone.Name;
//CQuat BOO = BO;
//if (!strcmp(bname, "b_MF_UpperArm_L")) { BO.Set(-0.225, -0.387, -0.310,  0.839); }
#if SHOW_ANIM
//if (i == 6 || i == 8 || i == 10 || i == 11 || i == 29)	//??
					DrawTextLeft("%s%d Bone (%s) : P{ %8.3f %8.3f %8.3f }  Q{ %6.3f %6.3f %6.3f %6.3f }",
						AnimSeq1->Tracks[BoneIndex].HasKeys() ? S_GREEN : S_BLUE,
						i, *Bone.Name, VECTOR_ARG(BP), QUAT_ARG(BO));
//if (!strcmp(bname, "b_MF_UpperArm_L")) DrawTextLeft("%g %g %g %g [%g %g]", BO.x-BOO.x,BO.y-BOO.y,BO.z-BOO.z,BO.w-BOO.w, BO.w, BOO.w);
#endif
//BO.Normalize();
#if SHOW_BONE_UPDATES
					if (AnimSeq1->Tracks[BoneIndex].HasKeys())
						BoneUpdateCounts[i]++;
#endif
				}
				// blend secondary animation
				if (AnimSeq2)
				{
					CVec3 BP2;
					CQuat BO2;
					BP2 = Bone.Position;		// default position - from bind pose
					BO2 = Bone.Orientation;		// ...
					AnimSeq2->Tracks[BoneIndex].GetBonePosition(Time2, AnimSeq2->NumFrames, Chn->Looped, BP2, BO2);
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
#if SHOW_BONE_UPDATES
					BoneUpdateCounts[i]++;
#endif
				}
				// process AnimRotationOnly
				if (!Animation->ShouldAnimateTranslation(BoneIndex, (EAnimRotationOnly)RotationMode))
					BP = Bone.Position;
			}
			else
			{
				// get default bone position
//				BP = Bone.Position; -- already set above
//				BO = Bone.Orientation;
#if SHOW_ANIM
				DrawTextLeft(S_YELLOW"%d Bone (%s) : P{ %8.3f %8.3f %8.3f }  Q{ %6.3f %6.3f %6.3f %6.3f }",
					i, *Bone.Name, VECTOR_ARG(BP), QUAT_ARG(BO));
#endif
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
	for (i = 0, data = BoneData; i < pMesh->RefSkeleton.Num(); i++, data++)
	{
		CCoords &BC = data->Coords;
		BC.origin = data->Pos;
		data->Quat.ToAxis(BC.axis);

		// move bone position to global coordinate space
		if (!i)
		{
			// root bone - use BaseTransformScaled
			BaseTransformScaled.UnTransformCoords(BC, BC);
		}
		else
		{
			// other bones - rotate around parent bone
			BoneData[pMesh->RefSkeleton[i].ParentIndex].Coords.UnTransformCoords(BC, BC);
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
#if USE_SSE
		data->Transform4.Set(data->Transform);
#endif
#if 0
//!!
if (i == 32 || i == 34)
{
#define C BC
	DrawTextLeft("[%2d] : o=%8.3f %8.3f %8.3f", i, VECTOR_ARG(C.origin ));
	DrawTextLeft("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	DrawTextLeft("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	DrawTextLeft("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
#define C data->Transform
	DrawTextLeft("TRN   : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	DrawTextLeft("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	DrawTextLeft("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	DrawTextLeft("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
#define C data->RefCoordsInv
	DrawTextLeft("REF   : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	DrawTextLeft("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	DrawTextLeft("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	DrawTextLeft("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
}
#endif
	}
	unguard;
}


void CSkelMeshInstance::UpdateAnimation(float TimeDelta)
{
	if (!pMesh->RefSkeleton.Num()) return;	// just in case

	// prepare bone-to-channel map
	//?? optimize: update when animation changed only
	for (int i = 0; i < pMesh->RefSkeleton.Num(); i++)
		BoneData[i].FirstChannel = 0;

	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && Chn->Anim1 == NULL)
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
		if (!Chn->TweenTime && Chn->Anim1)
		{
			// update animation time
			const CAnimSequence *Seq1 = Chn->Anim1;
			const CAnimSequence *Seq2 = Chn->Anim2;
			if (!Chn->SecondaryBlend) Seq2 = NULL;

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

	const CAnimSequence *NewAnim = FindAnim(AnimName);
	if (!NewAnim)
	{
		// show default pose
		Chn.Anim1          = NULL;
		Chn.Anim2          = NULL;
		Chn.Time           = 0;
		Chn.Rate           = 0;
		Chn.Looped         = false;
		Chn.TweenTime      = TweenTime;
		Chn.SecondaryBlend = 0;
		return;
	}

	Chn.Rate   = Rate;
	Chn.Looped = Looped;

	if (NewAnim == Chn.Anim1 && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	Chn.Anim1          = NewAnim;
	Chn.Anim2          = NULL;
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


void CSkelMeshInstance::SetSecondaryAnim(int Channel, const char *AnimName)
{
	guard(CSkelMeshInstance::SetSecondaryAnim);
	CAnimChan &Chn = GetStage(Channel);
	Chn.Anim2          = FindAnim(AnimName);
	Chn.SecondaryBlend = 0;
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


void CSkelMeshInstance::GetAnimParams(int Channel, const char *&AnimName, float &Frame, float &NumFrames, float &Rate) const
{
	guard(CSkelMeshInstance::GetAnimParams);

	const CAnimChan &Chn  = GetStage(Channel);
	if (!Animation || !Chn.Anim1 || Channel > MaxAnimChannel)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const CAnimSequence *Seq = Chn.Anim1;
	AnimName  = Seq->Name;
	Frame     = Chn.Time;
	NumFrames = Seq->NumFrames;
	Rate      = Seq->Rate * Chn.Rate;

	unguard;
}


/*-----------------------------------------------------------------------------
	Drawing
-----------------------------------------------------------------------------*/

static void GetBoneInfColor(int BoneIndex, float *Color)
{
	// most of this code is targetted to make maximal color combinations
	// which are maximally different for adjancent BoneIndex values
	static const float table[] = { 0.9f, 0.3f, 0.6f, 0.0f };
	static const int  table2[] = { 0, 1, 2, 4, 7, 3, 5, 6 };
	BoneIndex = (BoneIndex & 0xFFF8) | table2[BoneIndex & 7] ^ 7;
#if 0
	// this version produces similar colors for adjacent indices - less contrast picture
	Color[0] = table[BoneIndex & 3];
	Color[1] = table[(BoneIndex >> 2) & 3];
	Color[2] = table[(BoneIndex >> 4) & 3];
#else
	// this version produces more color contrast between adjacent indices
	#define C(x)	( (x) & 1 ) | ( ((x) >> 2) & 2 )
	Color[0] = table[C(BoneIndex)];
	Color[1] = table[C(BoneIndex >> 1)];
	Color[2] = table[C(BoneIndex >> 2)];
	#undef C
#endif
}


void CSkelMeshInstance::DrawSkeleton(bool ShowLabels, bool ColorizeBones)
{
	guard(CSkelMeshInstance::DrawSkeleton);

	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);
	glLineWidth(3);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBegin(GL_LINES);
	for (int i = 0; i < pMesh->RefSkeleton.Num(); i++)
	{
		const CSkelMeshBone &B  = pMesh->RefSkeleton[i];
		const CCoords       &BC = BoneData[i].Coords;

		CVec3 v1;
		float Color[4];
		unsigned TextColor;

		Color[3] = 0.5f;
		if (i > 0)
		{
//			Color.Set(1,1,0.3);
#if SHOW_BONE_UPDATES
			int t = BoneUpdateCounts[i];
			Color[0] = t & 1;
			Color[1] = (t >> 1) & 1;
			Color[2] = (t >> 2) & 1;
#endif
			v1 = BoneData[B.ParentIndex].Coords.origin;
		}
		else
		{
			Color[0] = 1; Color[1] = 0; Color[2] = 1;
			v1.Zero();
		}
		TextColor = RGBA(1,1,0,0.7);
		if (ColorizeBones)
		{
			GetBoneInfColor(i, Color);
			TextColor = RGBAS(Color[0], Color[1], Color[2], 0.7);
		}
		glColor4fv(Color);
		glVertex3fv(v1.v);
		glVertex3fv(BC.origin.v);

		if (ShowLabels)
		{
			// show bone label
			v1.Add(BC.origin);
			v1.Scale(0.5f);
			DrawText3D(v1, TextColor, "(%d)%s", i, *B.Name);
		}
	}
	glColor3f(1,1,1);
	glEnd();

	glLineWidth(1);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_DEPTH_TEST);

	unguard;
}

void CSkelMeshInstance::DrawAttachments()
{
	guard(CSkelMeshInstance::DrawAttachments);

	if (!pMesh->Sockets.Num()) return;

	BindDefaultMaterial(true);

	glBegin(GL_LINES);
	for (int i = 0; i < pMesh->Sockets.Num(); i++)
	{
		const CSkelMeshSocket &S = pMesh->Sockets[i];
		const char *BoneName = S.Bone;
		int BoneIndex = FindBone(BoneName);
		if (BoneIndex == INDEX_NONE) continue;		// should not happen

		CCoords AC;
		BoneData[BoneIndex].Coords.UnTransformCoords(S.Transform, AC);

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
		DrawText3D(labelOrigin, RGB(0,1,0), "%s\n(%s)", *S.Name, BoneName);
	}
	glColor3f(1,1,1);
	glEnd();

	unguard;
}


#if !USE_SSE

// Software skinning - FPU version
void CSkelMeshInstance::SkinMeshVerts()
{
	guard(CSkelMeshInstance::SkinMeshVerts);

	const CSkelMeshLod& Mesh = pMesh->Lods[LodNum];
	int NumVerts = Mesh.NumVerts;

	memset(Skinned, 0, sizeof(CSkinVert) * NumVerts);

	for (int i = 0; i < NumVerts; i++)
	{
		const CSkelMeshVertex &V = Mesh.Verts[i];
		CSkinVert             &D = Skinned[i];

		CVec4 UnpackedWeights;
		V.UnpackWeights(UnpackedWeights);

		// compute weighted transform from all influenced bones

		// take a 1st influence
		CCoords transform;
		transform = BoneData[V.Bone[0]].Transform;
		transform.Scale(UnpackedWeights.v[0]);
		// add remaining influences
		for (int j = 1; j < NUM_INFLUENCES; j++)
		{
			int iBone = V.Bone[j];
			if (iBone < 0) break;
			assert(iBone < pMesh->RefSkeleton.Num());	// validate bone index

			const CMeshBoneData &data = BoneData[iBone];
			CoordsMA(transform, UnpackedWeights.v[j], data.Transform);
		}

		// perform transformation

		CVec3 UnpNormal, UnpTangent;
//		CVec3 UnpBinormal;
		Unpack(UnpNormal, V.Normal);
		Unpack(UnpTangent, V.Tangent);
//		Unpack(UnpBinormal, V.Binormal);
		transform.UnTransformPoint(V.Position, D.Position);
		transform.axis.UnTransformVector(UnpNormal, D.Normal);
		transform.axis.UnTransformVector(UnpTangent, D.Tangent);
//		transform.axis.UnTransformVector(UnpBinormal, D.Binormal);
		// Preserve Normal.W to be able to compute binormal correctly
		D.Normal.v[3] = V.Normal.GetW();
	}

	unguard;
}

#else // USE_SSE

// Software skinning - SSE version
void CSkelMeshInstance::SkinMeshVerts()
{
	guard(CSkelMeshInstance::SkinMeshVerts);

	const CSkelMeshLod& Mesh = pMesh->Lods[LodNum];
	int NumVerts = Mesh.NumVerts;

	memset(Skinned, 0, sizeof(CSkinVert) * NumVerts);

	for (int i = 0; i < NumVerts; i++)
	{
		const CSkelMeshVertex &V = Mesh.Verts[i];
		CSkinVert             &D = Skinned[i];

		CVec4 UnpackedWeights;
		V.UnpackWeights(UnpackedWeights);

		// compute weighted transform from all influenced bones

		// take a 1st influence
		const CCoords4 &transform = BoneData[V.Bone[0]].Transform4;
		__m128 x1, x2, x3, x4, x5, x6, x7, x8;
		x1 = transform.mm[0];					// bone transform
		x2 = transform.mm[1];
		x3 = transform.mm[2];
		x4 = transform.mm[3];
		x5 = _mm_load1_ps(&UnpackedWeights.v[0]);// Weight
		x1 = _mm_mul_ps(x1, x5);				// Transform * Weight
		x2 = _mm_mul_ps(x2, x5);
		x3 = _mm_mul_ps(x3, x5);
		x4 = _mm_mul_ps(x4, x5);

		// add remaining influences
		for (int j = 1; j < NUM_INFLUENCES; j++)
		{
			int iBone = V.Bone[j];
			if (iBone < 0) break;
			assert(iBone < pMesh->RefSkeleton.Num());	// validate bone index

			const CMeshBoneData &data = BoneData[iBone];
			x5 = _mm_load1_ps(&UnpackedWeights.v[j]);	// Weight
			// x1..x4 += data.Transform * Weight
			x6 = _mm_mul_ps(data.Transform4.mm[0], x5);
			x1 = _mm_add_ps(x1, x6);
			x6 = _mm_mul_ps(data.Transform4.mm[1], x5);
			x2 = _mm_add_ps(x2, x6);
			x6 = _mm_mul_ps(data.Transform4.mm[2], x5);
			x3 = _mm_add_ps(x3, x6);
			x6 = _mm_mul_ps(data.Transform4.mm[3], x5);
			x4 = _mm_add_ps(x4, x6);
		}

		// perform transformation

		// at this point we have x1..x4 = transform matrix

#define TRANSFORM_POS(value)												\
		x5 = V.value.mm;													\
		x8 = x4;								/* Coords.origin */			\
		x6 = _mm_shuffle_ps(x5, x5, _MM_SHUFFLE(0,0,0,0));	/* X */			\
		x7 = _mm_mul_ps(x1, x6);											\
		x8 = _mm_add_ps(x8, x7);											\
		x6 = _mm_shuffle_ps(x5, x5, _MM_SHUFFLE(1,1,1,1));	/* Y */			\
		x7 = _mm_mul_ps(x2, x6);											\
		x8 = _mm_add_ps(x8, x7);											\
		x6 = _mm_shuffle_ps(x5, x5, _MM_SHUFFLE(2,2,2,2));	/* Z */			\
		x7 = _mm_mul_ps(x3, x6);											\
		D.value.mm = _mm_add_ps(x8, x7);

// version of the code above, but without Transform.origin use
#define TRANSFORM_NORMAL(value)												\
		x5 = UnpackPackedChars(V.value.Data);								\
		x6 = _mm_shuffle_ps(x5, x5, _MM_SHUFFLE(0,0,0,0));	/* X */			\
		x8 = _mm_mul_ps(x1, x6);											\
		x6 = _mm_shuffle_ps(x5, x5, _MM_SHUFFLE(1,1,1,1));	/* Y */			\
		x7 = _mm_mul_ps(x2, x6);											\
		x8 = _mm_add_ps(x8, x7);											\
		x6 = _mm_shuffle_ps(x5, x5, _MM_SHUFFLE(2,2,2,2));	/* Z */			\
		x7 = _mm_mul_ps(x3, x6);											\
		D.value.mm = _mm_add_ps(x8, x7);

		TRANSFORM_POS(Position);
		TRANSFORM_NORMAL(Normal);
		TRANSFORM_NORMAL(Tangent);
//		TRANSFORM_NORMAL(Binormal);

		// Preserve Normal.W to be able to compute binormal correctly
		D.Normal.v[3] = V.Normal.GetW();
	}

	unguard;
}

#endif // USE_SSE


void CSkelMeshInstance::DrawMesh(unsigned flags)
{
	guard(CSkelMeshInstance::DrawMesh);
	int i;

	if (!pMesh->Lods.Num()) return;

	/*const*/ CSkelMeshLod& Mesh = pMesh->Lods[LodNum];	//?? not 'const' because of BuildTangents(); change this?
	int NumSections = Mesh.Sections.Num();
	int NumVerts    = Mesh.NumVerts;
	if (!NumSections || !NumVerts) return;

//	if (!Mesh.HasNormals)  Mesh.BuildNormals();
	if (!Mesh.HasTangents) Mesh.BuildTangents();

#if 0
	SkinMeshVerts();
	glBegin(GL_POINTS);
	for (i = 0; i < Mesh.NumVerts; i++)
	{
//		glVertex3fv(Mesh.Verts[i].Position.v);
		glVertex3fv(Skinned[i].Position.v);
	}
	glEnd();
	return;
#endif

#if PROFILE_MESH
	int timeBeforeTransform = appMilliseconds();
#endif
	SkinMeshVerts();
#if PROFILE_MESH
	int timeAfterTransform = appMilliseconds();
#endif

#if SORT_BY_OPACITY
	// sort sections by material opacity
	int SectionMap[MAX_MESHMATERIALS];
	int secPlace = 0;
	for (int opacity = 0; opacity < 2; opacity++)
	{
		for (i = 0; i < NumSections; i++)
		{
			UUnrealMaterial *Mat = Mesh.Sections[i].Material;
			int op = 0;			// sort value
			if (Mat && Mat->IsTranslucent()) op = 1;
			if (op == opacity) SectionMap[secPlace++] = i;
		}
	}
	assert(secPlace == NumSections);
#endif // SORT_BY_OPACITY

#if TICK_SECTIONS
	int ShowSection = (appMilliseconds() / 2000) % NumSections;
	DrawTextLeft(S_RED"Show section: %d (%d faces)", ShowSection, Sections[ShowSection].NumFaces);
#endif

	// draw mesh
	glEnable(GL_LIGHTING);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glVertexPointer(3, GL_FLOAT, sizeof(CSkinVert), &Skinned[0].Position);
	glNormalPointer(GL_FLOAT, sizeof(CSkinVert), &Skinned[0].Normal);
	if (UVIndex == 0)
	{
		glTexCoordPointer(2, GL_FLOAT, sizeof(CSkelMeshVertex), &Mesh.Verts[0].UV.U);
	}
	else
	{
		glTexCoordPointer(2, GL_FLOAT, sizeof(CMeshUVFloat), &Mesh.ExtraUV[UVIndex-1][0].U);
	}

	if (flags & DF_SHOW_INFLUENCES)
	{
		// in this mode mesh is displayed colorized instead of textured
		if (!InfColors) BuildInfColors();
		assert(InfColors);
#if !SHOW_INFLUENCES
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(3, GL_FLOAT, 0, InfColors);
		BindDefaultMaterial(true);
#else
		BindDefaultMaterial(true);
		glColor4f(1, 1, 1, 0.1f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
	}

	for (i = 0; i < NumSections; i++)
	{
#if TICK_SECTIONS
		if (i != ShowSection) continue;
#endif
#if SORT_BY_OPACITY
		int MaterialIndex = SectionMap[i];
#else
		int MaterialIndex = i;
#endif
		const CMeshSection &Sec = Mesh.Sections[MaterialIndex];
		if (!Sec.NumFaces) continue;

		// select material
		if (!(flags & DF_SHOW_INFLUENCES))
			SetMaterial(Sec.Material, MaterialIndex);
		// check tangent space
		GLint aNormal = -1;
		GLint aTangent = -1;
//		GLint aBinormal = -1;
		const CShader *Sh = GCurrentShader;
		if (Sh)
		{
			aNormal    = Sh->GetAttrib("normal");
			aTangent   = Sh->GetAttrib("tangent");
//			aBinormal  = Sh->GetAttrib("binormal");
		}
		if (aNormal >= 0)
		{
			glEnableVertexAttribArray(aNormal);
			// send 4 components to decode binormal in shader
			glVertexAttribPointer(aNormal, 4, GL_FLOAT, GL_FALSE, sizeof(CSkinVert), &Skinned[0].Normal);
		}
		if (aTangent >= 0)
		{
			glEnableVertexAttribArray(aTangent);
			glVertexAttribPointer(aTangent,  3, GL_FLOAT, GL_FALSE, sizeof(CSkinVert), &Skinned[0].Tangent);
		}
//		if (aBinormal >= 0)
//		{
//			glEnableVertexAttribArray(aBinormal);
//			glVertexAttribPointer(aBinormal, 3, GL_FLOAT, GL_FALSE, sizeof(CSkinVert), &Skinned[0].Binormal);
//		}
		// draw
		//?? place this code into CIndexBuffer?
		//?? (the same code is in CStatMeshInstance)
		if (Mesh.Indices.Is32Bit())
			glDrawElements(GL_TRIANGLES, Sec.NumFaces * 3, GL_UNSIGNED_INT, &Mesh.Indices.Indices32[Sec.FirstIndex]);
		else
			glDrawElements(GL_TRIANGLES, Sec.NumFaces * 3, GL_UNSIGNED_SHORT, &Mesh.Indices.Indices16[Sec.FirstIndex]);
		// disable tangents
		if (aNormal >= 0)
			glDisableVertexAttribArray(aNormal);
		if (aTangent >= 0)
			glDisableVertexAttribArray(aTangent);
//		if (aBinormal >= 0)
//			glDisableVertexAttribArray(aBinormal);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	// display debug information

	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);

#if SHOW_INFLUENCES
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glBegin(GL_LINES);
	for (i = 0; i < NumVerts; i++)
	{
		const CSkelMeshVertex &V = Mesh.Verts[i];
		for (int j = 0; j < NUM_INFLUENCES; j++)
		{
			int iBone = V.Bone[j];
			if (iBone < 0) break;
			const CCoords &BC  = BoneData[iBone].Coords;
			float Color[4];
			GetBoneInfColor(iBone, Color);
			Color[3] = 0.1f;
			glColor4fv(Color);
			glVertex3fv(Skinned[i].Position.v);
			glVertex3fv(BC.origin.v);
		}
	}
	glEnd();
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
#endif // SHOW_INFLUENCES

	// draw mesh normals

	if (flags & DF_SHOW_NORMALS)
	{
		//!! TODO: performance issues, see StatMeshInstance.cpp, DF_SHOW_NORMALS, for more details.
		glBegin(GL_LINES);
		glColor3f(0.5f, 1, 0);
		const float VisualLength = 1.0f;
		for (i = 0; i < NumVerts; i++)
		{
			const CSkinVert& vert = Skinned[i];
			glVertex3fv(vert.Position.v);
			CVecT tmp;
			VectorMA(vert.Position, VisualLength, vert.Normal, tmp);
			glVertex3fv(tmp.v);
		}
#if SHOW_TANGENTS
		glColor3f(0, 0.5f, 1);
		for (i = 0; i < NumVerts; i++)
		{
			const CSkinVert& vert = Skinned[i];
			glVertex3fv(vert.Position.v);
			CVecT tmp;
			VectorMA(vert.Position, VisualLength, vert.Tangent, tmp);
			glVertex3fv(tmp.v);
		}
		glColor3f(1, 0, 0.5f);
		for (i = 0; i < NumVerts; i++)
		{
			const CSkinVert& vert = Skinned[i];
			// compute binormal
			CVecT binormal;
			cross(vert.Normal, vert.Tangent, binormal);
			binormal.Scale(vert.Normal.v[3]);
			// render
			const CVecT &v = vert.Position;
			glVertex3fv(v.v);
			CVecT tmp;
			VectorMA(v, VisualLength, binormal, tmp);
			glVertex3fv(tmp.v);
		}
#endif // SHOW_TANGENTS
		glEnd();
	}

#if PROFILE_MESH
	int timeAfterDraw = appMilliseconds();
	DrawTextRight(
		"Mesh:\n"
		"skinning:  %3d ms\n"
		"rendering: %3d ms\n",
		timeAfterTransform - timeBeforeTransform,
		timeAfterDraw - timeAfterTransform
	);
#endif // PROFILE_MESH

	unguard;
}


void CSkelMeshInstance::Draw(unsigned flags)
{
	guard(CSkelMeshInstance::Draw);

	// switch LOD model
	if (LodNum != LastLodNum)
	{
		// LOD has been changed

		if (InfColors)
		{
			delete[] InfColors;
			InfColors = NULL;
		}
		LastLodNum = LodNum;
	}
	// draw ...
	DrawMesh(flags);

	unguard;
}


void CSkelMeshInstance::BuildInfColors()
{
	guard(CSkelMeshInstance::BuildInfColors);

	int i;

	const CSkelMeshLod &Lod = pMesh->Lods[LodNum];

	if (InfColors) delete[] InfColors;
	InfColors = new CVec3[Lod.NumVerts];

	// get colors for bones
	int NumBones = pMesh->RefSkeleton.Num();
	CVec3 BoneColors[MAX_MESHBONES];
	for (i = 0; i < NumBones; i++)
		GetBoneInfColor(i, BoneColors[i].v);

	// process influences
	for (i = 0; i < Lod.NumVerts; i++)
	{
		const CSkelMeshVertex &V = Lod.Verts[i];
		CVec4 UnpackedWeights;
		V.UnpackWeights(UnpackedWeights);
		for (int j = 0; j < NUM_INFLUENCES; j++)
		{
			if (V.Bone[j] < 0) break;
			VectorMA(InfColors[i], UnpackedWeights.v[j], BoneColors[V.Bone[j]]);
		}
	}

	unguard;
}

#endif // RENDERING
