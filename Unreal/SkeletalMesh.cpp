#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"		// for typeinfo
#include "SkeletalMesh.h"


/*-----------------------------------------------------------------------------
	CSkeletalMesh
-----------------------------------------------------------------------------*/

struct CBoneProxy
{
	CBoneProxy		*Parent;
	CSkelMeshBone	*Bone;

/*	bool IsChildOf(const CBoneProxy *Other) const
	{
		if (Other == this) return false;

		int i = 0;
		for (const CBoneProxy *Bone = Parent; Bone; Bone = Bone->Parent)
		{
			if (Bone == Other) return true;
			if (++i >= MAX_MESHBONES) appError("Loop in skeleton");
		}
		return false;
	} */
};


static CBoneProxy Bones[MAX_MESHBONES];		//!! rename or pass to SortBones()
static CBoneProxy *SortedBones[MAX_MESHBONES];
static int NumSortedBones;

static void SortBoneArray(CBoneProxy *Parent, int NumBones)
{
	for (int i = 0; i < NumBones; i++)
	{
		CBoneProxy *Bone = &Bones[i];
		if (Bone->Parent == Parent)
		{
			if (NumSortedBones >= NumBones) appError("Loop in skeleton");
			SortedBones[NumSortedBones++] = Bone;
			SortBoneArray(Bone, NumBones);
		}
	}
}


void CSkeletalMesh::SortBones()
{
	int NumBones = RefSkeleton.Num();
	int i;

	// prepare CBoneProxy array
	for (i = 0; i < NumBones; i++)
	{
		Bones[i].Bone   = &RefSkeleton[i];
		Bones[i].Parent = (i > 0) ? &Bones[RefSkeleton[i].ParentIndex] : NULL;
		SortedBones[i]  = &Bones[i];
	}

	// sort bones
	NumSortedBones = 1;
	SortedBones[0] = &Bones[0];
	SortBoneArray(&Bones[0], NumBones);

	// build remap table
	int Remap[MAX_MESHBONES];
	int RemapBack[MAX_MESHBONES];
	for (i = 0; i < NumBones; i++)
	{
		const CBoneProxy *P = SortedBones[i];
		int OldIndex = P - Bones;
		Remap[OldIndex] = i;
		RemapBack[i] = OldIndex;
//		appPrintf("%s[%d] -> [%d] %s <- %s\n", (i != OldIndex) ? "*" : "", i, OldIndex, *P->Bone->Name, P->Parent ? *P->Parent->Bone->Name : "None");
	}

	// build new RefSkeleton
	TArray<CSkelMeshBone> NewSkeleton;
	NewSkeleton.Empty(NumBones);
	for (i = 0; i < NumBones; i++)
	{
		CSkelMeshBone *Bone = new (NewSkeleton) CSkelMeshBone;
		*Bone = RefSkeleton[RemapBack[i]];
		int oldParent = Bone->ParentIndex;
		Bone->ParentIndex = (oldParent > 0) ? Remap[oldParent] : 0;
	}
	CopyArray(RefSkeleton, NewSkeleton);

	// remap bone influences
	for (int lod = 0; lod < Lods.Num(); lod++)
	{
		CSkelMeshLod &L = Lods[lod];
		CSkelMeshVertex *V = L.Verts;
		for (i = 0; i < L.NumVerts; i++, V++)
		{
			for (int j = 0; j < NUM_INFLUENCES; j++)
			{
				int Bone = V->Bone[j];
				if (Bone < 0) break;
				V->Bone[j] = Remap[Bone];
			}
		}
	}
}


int CSkeletalMesh::FindBone(const char *Name) const
{
	for (int i = 0; i < RefSkeleton.Num(); i++)
	{
		if (!stricmp(RefSkeleton[i].Name, Name))
			return i;
	}
	return -1;
}


int CSkeletalMesh::GetRootBone() const
{
	if (!Lods.Num() || !RefSkeleton.Num())
		return -1;
	// find first bone with attached vertices
	const CSkelMeshLod &L = Lods[0];
	const CSkelMeshVertex *V = L.Verts;
	int RootBone = RefSkeleton.Num() + 1;
	for (int i = 0; i < L.NumVerts; i++, V++)
	{
		for (int j = 0; j < NUM_INFLUENCES; j++)
		{
			int Bone = V->Bone[j];
			if (Bone < 0) break;
			if (Bone < RootBone)
				RootBone = Bone;
		}
	}
	if (RootBone >= RefSkeleton.Num())
	{
		// no influences found?
		return -1;
	}
#if 1
	return RootBone;
#else
	// get parent of this bone
	if (RootBone == 0)
		return 0;
	return RefSkeleton[RootBone].ParentIndex;
#endif
}

void CSkeletalMesh::FinalizeMesh()
{
	for (int lod = 0; lod < Lods.Num(); lod++)
		Lods[lod].BuildNormals();
	SortBones();

	// fix bone weights
	int NumFixedVerts = 0;
	for (int lod = 0; lod < Lods.Num(); lod++)
	{
		CSkelMeshLod &L = Lods[lod];
		CSkelMeshVertex *V = L.Verts;
		for (int vert = 0; vert < L.NumVerts; vert++, V++)
		{
			byte UnpackedWeights[NUM_INFLUENCES];
			// int32 -> byte4
			*(int*)UnpackedWeights = V->PackedWeights;

			bool ShouldFix = false;
			for (int i = 0; i < NUM_INFLUENCES; i++)
			{
				int Bone = V->Bone[i];
				if (Bone < 0) break;
				if (UnpackedWeights[i] == 0)
				{
					// remove zero weight
					ShouldFix = true;
					continue;
				}
				// remove duplicated influences, if any
				for (int k = 0; k < i; k++)
				{
					if (V->Bone[k] == Bone)
					{
						// add k's weight to i, and set k's weight to 0
						int NewWeight = UnpackedWeights[i] + UnpackedWeights[k];
						if (NewWeight > 255) NewWeight = 255;
						UnpackedWeights[i] = NewWeight & 0xFF;
						UnpackedWeights[k] = 0;
						ShouldFix = true;
					}
				}
			}

			if (ShouldFix)
			{
				for (int i = NUM_INFLUENCES - 1; i >= 0; i--) // iterate in reverse order for correct removal of '0' followed by '0'
				{
					if (UnpackedWeights[i] == 0)
					{
						if (i < NUM_INFLUENCES-1)
						{
							// not very fast, but shouldn't do that too often
							memcpy(UnpackedWeights+i, UnpackedWeights+i+1, NUM_INFLUENCES-i-1);
							memcpy(V->Bone+i, V->Bone+i+1, (NUM_INFLUENCES-i-1) * sizeof(V->Bone[0]));
						}
						// remove last weight item
						UnpackedWeights[NUM_INFLUENCES-1] = 0;
						V->Bone[NUM_INFLUENCES-1] = -1;
					}
				}
				// pack weights back to vertex
				V->PackedWeights = *(int*)UnpackedWeights;
				NumFixedVerts++;
			}
		}
	}

	if (NumFixedVerts) appPrintf("INFO: fixed %d vertices\n", NumFixedVerts);
}



/*-----------------------------------------------------------------------------
	CAnimSet
-----------------------------------------------------------------------------*/

#define MAX_LINEAR_KEYS		4

static int FindTimeKey(const TArray<float> &KeyTime, float Frame)
{
	guard(FindTimeKey);

	// find index in time key array
	int NumKeys = KeyTime.Num();
	// *** binary search ***
	int Low = 0, High = NumKeys-1;
	while (Low + MAX_LINEAR_KEYS < High)
	{
		int Mid = (Low + High) / 2;
		if (Frame < KeyTime[Mid])
			High = Mid-1;
		else
			Low = Mid;
	}
	// *** linear search ***
	int i;
	for (i = Low; i <= High; i++)
	{
		float CurrKeyTime = KeyTime[i];
		if (Frame == CurrKeyTime)
			return i;		// exact key
		if (Frame < CurrKeyTime)
			return (i > 0) ? i - 1 : 0;	// previous key
	}
	if (i > High)
		i = High;
	return i;

	unguard;
}


// In:  KeyTime, Frame, NumFrames, Loop
// Out: X - previous key index, Y - next key index, F - fraction between keys
static void GetKeyParams(const TArray<float> &KeyTime, float Frame, float NumFrames, bool Loop, int &X, int &Y, float &F)
{
	guard(GetKeyParams);
	X = FindTimeKey(KeyTime, Frame);
	Y = X + 1;
	int NumTimeKeys = KeyTime.Num();
	if (Y >= NumTimeKeys)
	{
		if (!Loop)
		{
			// clamp animation
			Y = NumTimeKeys - 1;
			assert(X == Y);
			F = 0;
		}
		else
		{
			// loop animation
			Y = 0;
			F = (Frame - KeyTime[X]) / (NumFrames - KeyTime[X]);
		}
	}
	else
	{
		F = (Frame - KeyTime[X]) / (KeyTime[Y] - KeyTime[X]);
	}
	unguard;
}


// not 'static', because used in ExportPsa()
void CAnimTrack::GetBonePosition(float Frame, float NumFrames, bool Loop, CVec3 &DstPos, CQuat &DstQuat) const
{
	guard(CAnimTrack::GetBonePosition);

	// fast case: 1 frame only
	if (KeyTime.Num() == 1 || NumFrames == 1 || Frame == 0)
	{
		if (KeyPos.Num())  DstPos  = KeyPos[0];
		if (KeyQuat.Num()) DstQuat = KeyQuat[0];
		return;
	}

	// data for lerping
	int posX, rotX;			// index of previous frame
	int posY, rotY;			// index of next frame
	float posF, rotF;		// fraction between X and Y for lerping

	int NumTimeKeys = KeyTime.Num();
	int NumPosKeys  = KeyPos.Num();
	int NumRotKeys  = KeyQuat.Num();

	if (NumTimeKeys)
	{
		// here: KeyPos and KeyQuat sizes either equals to 1 or equals to KeyTime size
		assert(NumPosKeys <= 1 || NumPosKeys == NumTimeKeys);
		assert(NumRotKeys == 1 || NumRotKeys == NumTimeKeys);

		GetKeyParams(KeyTime, Frame, NumFrames, Loop, posX, posY, posF);
		rotX = posX;
		rotY = posY;
		rotF = posF;

		if (NumPosKeys <= 1)
		{
			posX = posY = 0;
			posF = 0;
		}
		if (NumRotKeys == 1)
		{
			rotX = rotY = 0;
			rotF = 0;
		}
	}
	else
	{
		// empty KeyTime array - keys are evenly spaced on a time line
		// note: KeyPos and KeyQuat sizes can be different
		if (KeyPosTime.Num())
		{
			GetKeyParams(KeyPosTime, Frame, NumFrames, Loop, posX, posY, posF);
		}
		else if (NumPosKeys > 1)
		{
			float Position = Frame / NumFrames * NumPosKeys;
			posX = appFloor(Position);
			posF = Position - posX;
			posY = posX + 1;
			if (posY >= NumPosKeys)
			{
				if (!Loop)
				{
					posY = NumPosKeys - 1;
					posF = 0;
				}
				else
					posY = 0;
			}
		}
		else
		{
			posX = posY = 0;
			posF = 0;
		}

		if (KeyQuatTime.Num())
		{
			GetKeyParams(KeyQuatTime, Frame, NumFrames, Loop, rotX, rotY, rotF);
		}
		else if (NumRotKeys > 1)
		{
			float Position = Frame / NumFrames * NumRotKeys;
			rotX = appFloor(Position);
			rotF = Position - rotX;
			rotY = rotX + 1;
			if (rotY >= NumRotKeys)
			{
				if (!Loop)
				{
					rotY = NumRotKeys - 1;
					rotF = 0;
				}
				else
					rotY = 0;
			}
		}
		else
		{
			rotX = rotY = 0;
			rotF = 0;
		}
	}

	// get position
	if (posF > 0)
		Lerp(KeyPos[posX], KeyPos[posY], posF, DstPos);
	else if (NumPosKeys)		// do not change DstPos when no keys
		DstPos = KeyPos[posX];
	// get orientation
	if (rotF > 0)
		Slerp(KeyQuat[rotX], KeyQuat[rotY], rotF, DstQuat);
	else if (NumRotKeys)		// do not change DstQuat when no keys
		DstQuat = KeyQuat[rotX];

	unguard;
}


void CAnimTrack::CopyFrom(const CAnimTrack &Src)
{
	CopyArray(KeyQuat, Src.KeyQuat);
	CopyArray(KeyPos,  Src.KeyPos );
	CopyArray(KeyTime, Src.KeyTime);
	CopyArray(KeyQuatTime, Src.KeyQuatTime);
	CopyArray(KeyPosTime,  Src.KeyPosTime );
}
