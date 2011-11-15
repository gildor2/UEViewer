#ifndef __SKELETALMESH_H__
#define __SKELETALMESH_H__

#include "MeshCommon.h"

/*-----------------------------------------------------------------------------
	Local SkeletalMesh class, encapsulated all UE versions
-----------------------------------------------------------------------------*/

#define HAS_POLY_FLAGS			1			// mandatory for UE1, rarely (or never?) used in UE2

/*

May be make dummy material class to exclude PolyFlags from meshes at all (keep them in UVertMesh only)?
	class UTextureWithFlags : public UUnrealMaterial
	{
		UTexture	*Tex;
		unsigned	PolyFlags;
	}
- create this only when PolyFlags != 0
- make a function: CreateTextureWrapper(UTexture* Tex, unsigned PolyFlags) - will find or create wrapper
- can remove PolyFlags from most of rendering functions! (used for UTexture and UFinalBlend)
  ! make correct display of the wrapper material with <M> mesh mode

*/


/*!!

SKELETALMESH CONVERSION STAGES:

* separate UE2 and UE3 USkeletalMesh
  * move all UE3 declarations to UnMesh3.h
  * cleanup UE2 USkeletalMesh (can add "viewer" properties)
  * move most properties from UE3 cpp serializer to the class (like were done for UStaticMesh3)
* convert UE3 SkeletalMesh to CSkeletalMesh

- use CSkeletalMesh in CSkelMeshInstance
  * separate CSkelMeshViewer from CLodMeshViewer, parent will be CMeshViewer
  * separate CSkelMeshInstance from CLodMeshInstance
  * combine CLodMeshViewer and CVertMeshViewer
  - use CSkeletalMesh in CSkelMeshViewer
    * needs to duplicate some methods in both MeshInstance and MeshViewer: LOD->(Skel+Vert)
    - enable CSkeletalMesh in main.cpp, remove old viewer code
  - cleanup MeshInstance/MeshViewer interfaces, cleanup "virtual" modifiers

- verify code for reading verts from chunks: UE3/UDK CH_AnimHuman has non-cooked meshes with these vertices
- verify mesh sockets

! make common code for BuildNormals, make common CMeshBaseVertex (== CStaticMeshVertex ?)
    * require CStaticMeshVertex to use CVecT instead of CVec3, rename to CMeshVertex[Base?] and move to MeshCommon.h
    * changes from CVec3 to CVecT will require aligned allocator

- use normals from UE3 USkeletalMesh
  - verify HasNormals/HasTangents to work

- update exporter to use CSkeletalMesh, no new functionality yet
  ! currently main.cpp has functions to call exporter, but they are disabled to allow compilation
- convert UE2 mesh to CSkeletalMesh

- load all UV sets for UE3 skeletal mesh
- toggle UV sets in viewer (verify with Mirror's Edge?)
  - use textured mesh for both static and skeletal meshes when displaying UVSet > 0; untextured mesh is useless
- export all UV sets for pskx
  - can make common function for StaticMesh and SkeletalMesh - pass ( Verts[0].UV[0] + sizeof(Vertex) )

- support 32-bit index buffer in loader/viewer
- support 32-bit indices in pskx (different section name for triangles?)

- dump properties of the SkeletalMesh:
  - require FVector/FRotator dump (or CVec3 ?)
    - can make something like "DECLARE_SEPARATE_TYPEINFO(Class)" ?
    - can separate typeinfo into different header and include it from UnCore.h to support typeinfo for
      UnCore math types
      - in this case can clean UnObject.h include from some files
  - gather statistics for LODs:
    - number of vertex influences (1-4)
    - number of used bones -- to exclude hair, fingers etc

- move ULodMesh and UVertMesh to UnMesh2.h
? separate UnMesh.cpp to Common+UE2 or UE1+UE2 (check what's better)
- cleanup UnMesh.h and TypeConvert.h includes
- cleanup TODOs
- cleanup UnMesh3.cpp - has some old commented code around serializers
- cleanup SkelMeshInstance code for conversion UE2->old_CSkeletalMesh

! verify Gears3 meshes - had some problems before (overlapping vertex indices)
  - find old umodel build which had problems, find problematic meshes, or place appNotify() or assert()
    * "bad" umodel is r135, mesh is in data\3X\GOW3_b2\GearGame.xxx
    for chunks when "Chunk[Index>0].FirstVertex > 0"
  - verify these meshes with new umodel

- place animation information somewhere in the bottom of the screen (needs DrawTextLeftBottom or use DrawTextPos() with use of GetWindowSize() ?)

*/


#define NUM_INFLUENCES				4

struct CSkelMeshSection
{
	UUnrealMaterial			*Material;
	int						FirstIndex;
	int						NumFaces;
#if HAS_POLY_FLAGS
	unsigned				PolyFlags;				// PF_...
#endif

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CSkelMeshSection)
	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
		PROP_INT(FirstIndex)
		PROP_INT(NumFaces)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};


struct CSkelMeshVertex : public CMeshVertex
{
	//?? use short for Bone[] and byte for Weight[]
	int						Bone[NUM_INFLUENCES];	// Bone < 0 - end of list
	float					Weight[NUM_INFLUENCES];
};


struct CSkelMeshBone
{
	FName					Name;
	int						ParentIndex;
	CVec3					Position;
	CQuat					Orientation;
};


struct CSkelMeshLod
{
	// generic properties
	int						NumTexCoords;
	bool					HasNormals;
	bool					HasTangents;
	// geometry
	TArray<CSkelMeshSection> Sections;
	CSkelMeshVertex			*Verts;
	int						NumVerts;
	CIndexBuffer			Indices;

	~CSkelMeshLod()
	{
		if (Verts) appFree(Verts);
	}

	void BuildTangents();

	void AllocateVerts(int Count)
	{
		guard(CSkelMeshLod::AllocateVerts);
		assert(Verts == NULL);
		Verts    = (CSkelMeshVertex*)appMalloc(sizeof(CSkelMeshVertex) * Count, 16);		// alignment for SSE
		NumVerts = Count;
		unguard;
	}

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CSkelMeshLod)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Sections, CStaticMeshSection)
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

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CSkeletalMesh)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Lods, CSkelMeshLod)
		VPROP_ARRAY_COUNT(Lods, LodCount)
		PROP_VECTOR(MeshOrigin)						// CVec3 as FVector
		PROP_VECTOR(MeshScale)
		PROP_ROTATOR(RotOrigin)
		VPROP_ARRAY_COUNT(RefSkeleton, BoneCount)
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
};


struct CAnimSequence
{
	FName					Name;					// sequence's name
	int						NumFrames;
	float					Rate;
	TArray<CAnimTrack>		Tracks;					// for each CAnimSet.TrackBoneNames
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
	TArray<CAnimSequence>	Sequences;

	bool					AnimRotationOnly;
	TArray<bool>			UseAnimTranslation;		// per bone; used with AnimRotationOnly mode
	TArray<bool>			ForceMeshTranslation;	// pre bone; used regardless of AnimRotationOnly

	CAnimSet(UObject *Original)
	:	OriginalAnim(Original)
	{}

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
	REGISTER_CLASS(CSkelMeshSection) \
	REGISTER_CLASS(CSkelMeshLod)


#endif // __SKELETALMESH_H__
