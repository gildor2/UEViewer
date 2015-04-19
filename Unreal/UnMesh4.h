#ifndef __UNMESH4_H__
#define __UNMESH4_H__

#if UNREAL4

#include "UnMesh.h"			// common types

// forwards
class UMaterialInterface;

#define NUM_INFLUENCES_UE4				4
#define MAX_TOTAL_INFLUENCES_UE4		8
#define MAX_STATIC_UV_SETS_UE4			8
#define MAX_STATIC_LODS_UE4				4

//!! These classes should be explicitly defined
typedef UObject UStaticMeshSocket;


/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

struct FSkeletalMaterial;
struct FStaticLODModel4;
struct FSkeletalMeshLODInfo;

struct FMeshBoneInfo
{
	FName					Name;
	int						ParentIndex;

	friend FArchive& operator<<(FArchive& Ar, FMeshBoneInfo& B)
	{
		Ar << B.Name << B.ParentIndex;
		if (Ar.ArVer < VER_UE4_REFERENCE_SKELETON_REFACTOR)
		{
			FColor Color;
			Ar << Color;
		}
		return Ar;
	}
};

struct FReferenceSkeleton
{
	TArray<FMeshBoneInfo>	RefBoneInfo;
	TArray<FTransform>		RefBonePose;
	TMap<FName, int>		NameToIndexMap;

	friend FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& S)
	{
		guard(FReferenceSkeleton<<);

		Ar << S.RefBoneInfo;
		Ar << S.RefBonePose;

		if (Ar.ArVer >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
			Ar << S.NameToIndexMap;
		if (Ar.ArVer < VER_UE4_FIXUP_ROOTBONE_PARENT && S.RefBoneInfo.Num() && S.RefBoneInfo[0].ParentIndex != INDEX_NONE)
			S.RefBoneInfo[0].ParentIndex = INDEX_NONE;

		return Ar;

		unguard;
	}
};


struct FReferencePose
{
	FName					PoseName;
	TArray<FTransform>		ReferencePose;

	friend FArchive& operator<<(FArchive& Ar, FReferencePose& P)
	{
		return Ar << P.PoseName << P.ReferencePose;
	}
};


struct FSmartNameMapping
{
	short					NextUid;
	TMap<short, FName>		UidMap;

	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& N)
	{
		return Ar << N.NextUid << N.UidMap;
	}
};


struct FSmartNameContainer
{
	TMap<FName, FSmartNameMapping> NameMappings;

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& C)
	{
		return Ar << C.NameMappings;
	}
};


class USkeleton : public UObject
{
	DECLARE_CLASS(USkeleton, UObject);
public:
	FReferenceSkeleton		ReferenceSkeleton;
	TMap<FName, FReferencePose> AnimRetargetSources;
	FGuid					Guid;
	FSmartNameContainer		SmartNames;
	//!! TODO: sockets

	virtual void Serialize(FArchive &Ar);
};


class USkeletalMesh4 : public UObject
{
	DECLARE_CLASS(USkeletalMesh4, UObject);
public:
	FBoxSphereBounds		Bounds;
	TArray<FSkeletalMaterial> Materials;
	FReferenceSkeleton		RefSkeleton;
	USkeleton				*Skeleton;
	TArray<FStaticLODModel4> LODModels;
	TArray<FSkeletalMeshLODInfo> LODInfo;

	// properties
	bool					bHasVertexColors;

	CSkeletalMesh			*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_OBJ(Skeleton)
		PROP_BOOL(bHasVertexColors)
		PROP_ARRAY(LODInfo, FSkeletalMeshLODInfo)
	END_PROP_TABLE

	USkeletalMesh4();
	virtual ~USkeletalMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
};


/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FStaticMeshLODModel4;

class UStaticMesh4 : public UObject
{
	DECLARE_CLASS(UStaticMesh4, UObject);
public:
	CStaticMesh				*ConvertedMesh;

	UObject					*BodySetup;			// UBodySetup
	UObject*				NavCollision;		// UNavCollision
	FGuid					LightingGuid;

	TArray<UStaticMeshSocket*> Sockets;

	TArray<UMaterialInterface*> Materials;

	// FStaticMeshRenderData fields
	FBoxSphereBounds		Bounds;
	float					ScreenSize[MAX_STATIC_LODS_UE4];
	float					StreamingTextureFactors[MAX_STATIC_UV_SETS_UE4];
	float					MaxStreamingTextureFactor;
	bool					bReducedBySimplygon;
	bool					bLODsShareStaticLighting;
	TArray<FStaticMeshLODModel4> Lods;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, UObject*)
	END_PROP_TABLE

	UStaticMesh4();
	virtual ~UStaticMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
};


#define REGISTER_MESH_CLASSES_U4 \
	REGISTER_CLASS(USkeleton) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS_ALIAS(USkeletalMesh4, USkeletalMesh) \
	REGISTER_CLASS_ALIAS(USkeletalMesh4, UDestructibleMesh) \
	REGISTER_CLASS_ALIAS(UStaticMesh4, UStaticMesh)


#endif // UNREAL4
#endif // __UNMESH4_H__
