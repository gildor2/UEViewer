#ifndef __UNMESH4_H__
#define __UNMESH4_H__

#if UNREAL4

#include "UnMesh.h"			// common types

// forwards
class UMaterialInterface;

#define NUM_INFLUENCES_UE4				4
#define MAX_TOTAL_INFLUENCES_UE4		8
#define MAX_SKELETAL_UV_SETS_UE4		4
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

	friend FArchive& operator<<(FArchive& Ar, FMeshBoneInfo& B);
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
		//!! TODO: non-cooked package also has P.ReferenceMesh (no way to detect if this is saved or not!)
	}
};


struct FSmartNameMapping
{
	int16					NextUid;
	TMap<int16, FName>		UidMap;

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

struct FMeshBuildSettings
{
	DECLARE_STRUCT(FMeshBuildSettings);

	bool					bRecomputeNormals;
	bool					bRecomputeTangents;

	BEGIN_PROP_TABLE
		PROP_BOOL(bRecomputeNormals)
		PROP_BOOL(bRecomputeTangents)
		PROP_DROP(bUseMikkTSpace)
		PROP_DROP(bRemoveDegenerates)
		PROP_DROP(bBuildAdjacencyBuffer)
		PROP_DROP(bBuildReversedIndexBuffer)
		PROP_DROP(bUseFullPrecisionUVs)
		PROP_DROP(bGenerateLightmapUVs)
		PROP_DROP(MinLightmapResolution)
		PROP_DROP(SrcLightmapIndex)
		PROP_DROP(DstLightmapIndex)
		PROP_DROP(BuildScale3D)						//?? FVector, use?
		PROP_DROP(DistanceFieldResolutionScale)
		PROP_DROP(bGenerateDistanceFieldAsIfTwoSided)
		PROP_DROP(DistanceFieldReplacementMesh)
	END_PROP_TABLE
};

struct FStaticMeshSourceModel
{
	DECLARE_STRUCT(FStaticMeshSourceModel);
	FByteBulkData			BulkData;
	FMeshBuildSettings		BuildSettings;

	BEGIN_PROP_TABLE
		PROP_STRUC(BuildSettings, FMeshBuildSettings)
		PROP_DROP(ReductionSettings)
		PROP_DROP(ScreenSize)
	END_PROP_TABLE
};

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
	TArray<FStaticMeshSourceModel> SourceModels;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, UObject*)
		PROP_ARRAY(SourceModels, FStaticMeshSourceModel)
	END_PROP_TABLE

	UStaticMesh4();
	virtual ~UStaticMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
	void ConvertSourceModels();
};


#define REGISTER_MESH_CLASSES_U4 \
	REGISTER_CLASS(USkeleton) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS_ALIAS(USkeletalMesh4, USkeletalMesh) \
	REGISTER_CLASS_ALIAS(USkeletalMesh4, UDestructibleMesh) \
	REGISTER_CLASS_ALIAS(UStaticMesh4, UStaticMesh) \
	REGISTER_CLASS(FMeshBuildSettings) \
	REGISTER_CLASS(FStaticMeshSourceModel)


#endif // UNREAL4
#endif // __UNMESH4_H__
