#ifndef __SKELETAL_MESH_H__
#define __SKELETAL_MESH_H__

#include "MeshCommon.h"

/*-----------------------------------------------------------------------------
	Local SkeletalMesh class, encapsulated all UE versions
-----------------------------------------------------------------------------*/

/*!!

TODO:
- support vertex colors (for StaticMesh too!)
- dump properties of the SkeletalMesh:
  - number of vertex influences (1-4)
  - number of really used bones -- to exclude hair, fingers etc -- display as "Bones: N/M"
- remove RootBone.Orientation.W *= -1 for UE3, place this code for UE2 (find all places where bone orientation is accessed!)
  - fix (invert logic) mesh rendering and export code too
- UMaterialWithPolyFlags: should make correct display of the wrapped material with <M> mesh mode - when has
  wrapper, will display IT instead of original material (see CMeshViewer::PrintMaterialInfo() - everything is here)
  - will also need to change:
    - CVertMeshInstance, but NOTE: UTextureWithFlags will be stored in CSkeletalMesh (not instance!) and
      in CVertMeshInstance (not mesh!) - probably should generate materials in UVertMesh too
    - Dump() functions for meshes

*/


#define MAX_MESHBONES				2048
#define NUM_INFLUENCES				4
//#define ANIM_DEBUG_INFO				1

struct CSkelMeshVertex : public CMeshVertex
{
	uint32					PackedWeights;			// Works with 4 weights only!
	int16					Bone[NUM_INFLUENCES];	// Bone < 0 - end of list

	void UnpackWeights(CVec4& OutWeights) const
	{
#if USE_SSE
		OutWeights.mm = UnpackPackedBytes(PackedWeights);
#else
		float Scale = 1.0f / 255;
		OutWeights.v[0] =  (PackedWeights        & 0xFF) * Scale;
		OutWeights.v[1] = ((PackedWeights >> 8 ) & 0xFF) * Scale;
		OutWeights.v[2] = ((PackedWeights >> 16) & 0xFF) * Scale;
		OutWeights.v[3] = ((PackedWeights >> 24) & 0xFF) * Scale;
#endif
	}
};


struct CSkelMeshBone
{
	FName					Name;
	int						ParentIndex;
	CVec3					Position;
	CQuat					Orientation;
};


struct CSkelMeshLod : public CBaseMeshLod
{
	CSkelMeshVertex			*Verts;

	CSkelMeshLod()
	:	Verts(NULL)
	{}
	~CSkelMeshLod()
	{
		if (Verts) appFree(Verts);
	}

	void BuildNormals()
	{
		if (HasNormals) return;
		BuildNormalsCommon(Verts, sizeof(CSkelMeshVertex), NumVerts, Indices);
		HasNormals = true;
	}

	void BuildTangents()
	{
		if (HasTangents) return;
		BuildTangentsCommon(Verts, sizeof(CSkelMeshVertex), Indices);
		HasTangents = true;
	}

	void AllocateVerts(int Count)
	{
		guard(CSkelMeshLod::AllocateVerts);
		assert(Verts == NULL);
		Verts    = (CSkelMeshVertex*)appMalloc(sizeof(CSkelMeshVertex) * Count, 16);		// alignment for SSE
		NumVerts = Count;
		AllocateUVBuffers();
		unguard;
	}

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CSkelMeshLod)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Sections, CMeshSection)
		PROP_INT(NumVerts)
		VPROP_ARRAY_COUNT(Indices.Indices16, IndexCount)
		PROP_INT(NumTexCoords)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};


struct CSkelMeshSocket
{
	FName					Name;
	FName					Bone;
	CCoords					Transform;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CSkelMeshSocket)
	BEGIN_PROP_TABLE
		PROP_NAME(Name)
		PROP_NAME(Bone)
	END_PROP_TABLE
#endif
};


class CSkeletalMesh
{
public:
	UObject					*OriginalMesh;			//?? make common for all mesh classes
	FBox					BoundingBox;			//?? common
	FSphere					BoundingSphere;			//?? common
	CVec3					MeshOrigin;
	CVec3					MeshScale;
	FRotator				RotOrigin;
	TArray<CSkelMeshBone>	RefSkeleton;
	TArray<CSkelMeshLod>	Lods;
	TArray<CSkelMeshSocket>	Sockets;

	CSkeletalMesh(UObject *Original)
	:	OriginalMesh(Original)
	{}

	void FinalizeMesh();

#if RENDERING
	void LockMaterials()
	{
		for (int i = 0; i < Lods.Num(); i++)
			Lods[i].LockMaterials();
	}

	void UnlockMaterials()
	{
		for (int i = 0; i < Lods.Num(); i++)
			Lods[i].UnlockMaterials();
	}
#endif

	void SortBones();
	int FindBone(const char *Name) const;
	int GetRootBone() const;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CSkeletalMesh)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Lods, CSkelMeshLod)
		VPROP_ARRAY_COUNT(Lods, LodCount)
		PROP_VECTOR(MeshOrigin)						// CVec3 as FVector
		PROP_VECTOR(MeshScale)
		PROP_ROTATOR(RotOrigin)
		VPROP_ARRAY_COUNT(RefSkeleton, BoneCount)
		PROP_ARRAY(Sockets, CSkelMeshSocket)
		VPROP_ARRAY_COUNT(Sockets, SocketCount)
	END_PROP_TABLE
private:
	CSkeletalMesh()									// for InternalConstructor()
	{}
#endif // DECLARE_VIEWER_PROPS
};


/*-----------------------------------------------------------------------------
	CAnimSet class, common for UMeshAnimation (UE2) and UAnimSet (UE3)
-----------------------------------------------------------------------------*/

/*
	Animation data exclusion in favour of bind pose:
	- Tribes
	  - uses AnalogTrack.Flags to completely exclude animation of particular bone in specific animation
	- UC2
	  - has empty KeyPos and/or KeyQuat arrays
	- UE3
	  - control from UAnimSet for all sequences by bAnimRotationOnly and UseTranslationBoneNames/ForceMeshTranslationBoneNames
	  - done for whole animation set at once, not per-track
	  - rotation is always taken from the animation
	  - root bone is always uses translation from the animation
	  - other bones uses translation from BindPose when
	    (bAnimRotationOnly && UseTranslationBoneNames[Bone]) || ForceMeshTranslationBoneNames[Bone]
	    * ForceMeshTranslationBoneNames tracks are removed from UAnimSequence (has single invalid translation key)
	  - UAnimSequence is always has at least one key for excluded bone (there is no empty arrays)
*/

/*!!
	Warning: very unoptimal data format. Each CAnimSequence has NumBones CAnimTrack's, each CAnimTrack has up to 5 arrays.
	Ideally should put all the data into CAnimSequence as a single data block (like was done for UAnimSequence).
*/


struct CAnimTrack
{
	TArray<CQuat>			KeyQuat;
	TArray<CVec3>			KeyPos;
	// 3 time arrays; should be used either KeyTime or KeyQuatTime + KeyPosTime
	// When the corresponding array is empty, it will assume that Array[i] == i
	TArray<float>			KeyTime;
	TArray<float>			KeyQuatTime;
	TArray<float>			KeyPosTime;

	// DstPos and DstQuat will not be changed when KeyPos and KeyQuat are empty
	void GetBonePosition(float Frame, float NumFrames, bool Loop, CVec3 &DstPos, CQuat &DstQuat) const;
	inline bool HasKeys() const
	{
		return (KeyQuat.Num() + KeyPos.Num()) > 0;
	}
	void CopyFrom(const CAnimTrack &Src);
};


class CAnimSequence
{
public:
	FName					Name;					// sequence's name
	int						NumFrames;
	float					Rate;
	TArray<CAnimTrack>		Tracks;					// for each CAnimSet.TrackBoneNames
#if ANIM_DEBUG_INFO
	FString					DebugInfo;
#endif
};


// taken from UE3/SkeletalMeshComponent
enum EAnimRotationOnly
{
	/** Use settings defined in each AnimSet (default) */
	EARO_AnimSet,
	/** Force AnimRotationOnly enabled on all AnimSets, but for this SkeletalMesh only */
	EARO_ForceEnabled,
	/** Force AnimRotationOnly disabled on all AnimSets, but for this SkeletalMesh only */
	EARO_ForceDisabled
};


class CAnimSet
{
public:
	UObject					*OriginalAnim;			//?? make common for all mesh classes
	TArray<FName>			TrackBoneNames;
	TArray<CAnimSequence*>	Sequences;

	bool					AnimRotationOnly;
	TArray<bool>			UseAnimTranslation;		// per bone; used with AnimRotationOnly mode
	TArray<bool>			ForceMeshTranslation;	// pre bone; used regardless of AnimRotationOnly

	CAnimSet(UObject *Original)
	:	OriginalAnim(Original)
	{}

	~CAnimSet()
	{
		for (int i = 0; i < Sequences.Num(); i++)
			delete Sequences[i];
	}

	bool ShouldAnimateTranslation(int BoneIndex, EAnimRotationOnly RotationMode = EARO_AnimSet) const
	{
		if (BoneIndex == 0)							// root bone is always fully animated
			return true;
		if (ForceMeshTranslation.Num() && ForceMeshTranslation[BoneIndex])
			return false;
		bool AnimRotationOnly2 = AnimRotationOnly;
		if (RotationMode == EARO_ForceEnabled)
			AnimRotationOnly2 = true;
		else if (RotationMode == EARO_ForceDisabled)
			AnimRotationOnly2 = false;
		if (!AnimRotationOnly2)
			return true;
		if (UseAnimTranslation.Num() && UseAnimTranslation[BoneIndex])
			return true;
		return false;
	}
};


#define REGISTER_SKELMESH_VCLASSES \
	REGISTER_CLASS(CMeshSection) \
	REGISTER_CLASS(CSkelMeshLod) \
	REGISTER_CLASS(CSkelMeshSocket)


#endif // __SKELETAL_MESH_H__
