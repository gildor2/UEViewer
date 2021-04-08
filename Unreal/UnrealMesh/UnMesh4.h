#ifndef __UNMESH4_H__
#define __UNMESH4_H__

#if UNREAL4

#include "UnMesh.h"			// common types
#include "UnMesh3.h"		// animation enums

// forwards
class UMaterialInterface;
class UAnimSequence4;

#define NUM_INFLUENCES_UE4				4
#define MAX_TOTAL_INFLUENCES_UE4		8
#define MAX_SKELETAL_UV_SETS_UE4		4
#define MAX_STATIC_UV_SETS_UE4			8
#define MAX_STATIC_LODS_UE4				8		// 4 before 4.9, 8 starting with 4.9

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
	TMap<FName, int32>		NameToIndexMap;

	friend FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& S);
};


struct FReferencePose
{
	FName					PoseName;
	TArray<FTransform>		ReferencePose;

	friend FArchive& operator<<(FArchive& Ar, FReferencePose& P);
};

struct FBoneReference
{
	DECLARE_STRUCT(FBoneReference);

	FName					BoneName;

	BEGIN_PROP_TABLE
		PROP_NAME(BoneName)
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive& Ar, FBoneReference& B)
	{
		return Ar << B.BoneName;
	}
};

struct FCurveMetaData
{
	bool					bMaterial;
	bool					bMorphtarget;
	TArray<FBoneReference>	LinkedBones;
	uint8					MaxLOD;

	friend FArchive& operator<<(FArchive& Ar, FCurveMetaData& D)
	{
		Ar << D.bMaterial << D.bMorphtarget << D.LinkedBones;
		if (FAnimPhysObjectVersion::Get(Ar) >= FAnimPhysObjectVersion::AddLODToCurveMetaData)
		{
			Ar << D.MaxLOD;
#if KH3
			if (Ar.Game == GAME_KH3) Ar.Seek(Ar.Tell() + 3); // MaxLOD is int32 here
#endif
		}
		return Ar;
	}
};

struct FSmartNameMapping
{
	//!! TODO: no data fields here
	int32 Dummy;

	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& N);
};


struct FSmartNameContainer
{
	TMap<FName, FSmartNameMapping> NameMappings;

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& C)
	{
		return Ar << C.NameMappings;
	}
};


struct FVirtualBone
{
	DECLARE_STRUCT(FVirtualBone);

	FName					SourceBoneName;
	FName					TargetBoneName;
	FName					VirtualBoneName;

	BEGIN_PROP_TABLE
		PROP_NAME(SourceBoneName)
		PROP_NAME(TargetBoneName)
		PROP_NAME(VirtualBoneName)
	END_PROP_TABLE
};

// TODO: this should be an "enum class", however this will break _ENUM() macro
enum EBoneTranslationRetargetingMode
{
	Animation,				// use translation from animation
	Skeleton,				// use translation from target skeleton
	AnimationScaled,		// translation from animation, scale it with TargetBoneTranslation.Size / SourceBoneTranslation.Size
	AnimationRelative,		// translation from animation, plus additive part (not just "translation" mode)
	OrientAndScale,
};

_ENUM(EBoneTranslationRetargetingMode)
{
	_E(Animation),
	_E(Skeleton),
	_E(AnimationScaled),
	_E(AnimationRelative),
	_E(OrientAndScale),
};

struct FBoneNode
{
	DECLARE_STRUCT(FBoneNode)

	EBoneTranslationRetargetingMode TranslationRetargetingMode;

	BEGIN_PROP_TABLE
		PROP_ENUM2(TranslationRetargetingMode, EBoneTranslationRetargetingMode)
	END_PROP_TABLE
};

struct FAnimSlotGroup
{
	DECLARE_STRUCT(FAnimSlotGroup)
	FName					GroupName;
	TArray<FName>			SlotNames;
	BEGIN_PROP_TABLE
		PROP_NAME(GroupName)
		PROP_ARRAY(SlotNames, PropType::FName)
	END_PROP_TABLE
};

class USkeleton : public UObject
{
	DECLARE_CLASS(USkeleton, UObject);
public:
	FReferenceSkeleton		ReferenceSkeleton;
	TMap<FName, FReferencePose> AnimRetargetSources;
	FGuid					Guid;
	FSmartNameContainer		SmartNames;
	TArray<FVirtualBone>	VirtualBones;
	TArray<FBoneNode>		BoneTree;
	TArray<USkeletalMeshSocket*> Sockets;
	TArray<FAnimSlotGroup>	SlotGroups;

	BEGIN_PROP_TABLE
		PROP_ARRAY(VirtualBones, "FVirtualBone")
		PROP_ARRAY(BoneTree, "FBoneNode")
		PROP_ARRAY(Sockets, PropType::UObject)
		PROP_ARRAY(SlotGroups, "FAnimSlotGroup")
		PROP_DROP(Notifies)			// not working with notifies in our program
	END_PROP_TABLE

	USkeleton()
	:	ConvertedAnim(NULL)
	{}

	virtual ~USkeleton();

	// generated stuff
	TArray<UAnimSequence4*>	OriginalAnims;
	CAnimSet*				ConvertedAnim;

	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad();

	// Convert a single UAnimSequence to internal animation format
	void ConvertAnims(UAnimSequence4* Seq);
};


// Skeletal mesh sampling info (declared just for unversioned properties)

struct FSkeletalMeshSamplingRegionBoneFilter
{
	DECLARE_STRUCT(FSkeletalMeshSamplingRegionBoneFilter);
	FName		BoneName;
	uint8		bIncludeOrExclude;
	uint8		bApplyToChildren;
	BEGIN_PROP_TABLE
		PROP_NAME(BoneName)
		PROP_BOOL(bIncludeOrExclude)
		PROP_BOOL(bApplyToChildren)
	END_PROP_TABLE
};

struct FSkeletalMeshSamplingRegionMaterialFilter
{
	DECLARE_STRUCT(FSkeletalMeshSamplingRegionMaterialFilter);
	FName		MaterialName;
	BEGIN_PROP_TABLE
		PROP_NAME(MaterialName)
	END_PROP_TABLE
};

struct FSkeletalMeshSamplingRegion
{
	DECLARE_STRUCT(FSkeletalMeshSamplingRegion);
	FName		Name;
	int32		LODIndex;
	uint8		bSupportUniformlyDistributedSampling;
	TArray<FSkeletalMeshSamplingRegionMaterialFilter> MaterialFilters;
	TArray<FSkeletalMeshSamplingRegionBoneFilter> BoneFilters;
	BEGIN_PROP_TABLE
		PROP_NAME(Name)
		PROP_INT(LODIndex)
		PROP_BOOL(bSupportUniformlyDistributedSampling)
		PROP_ARRAY(MaterialFilters, "FSkeletalMeshSamplingRegionMaterialFilter")
		PROP_ARRAY(BoneFilters, "FSkeletalMeshSamplingRegionBoneFilter")
	END_PROP_TABLE
};

struct FSkeletalMeshSamplingLODBuiltData
{
	DECLARE_STRUCT(FSkeletalMeshSamplingLODBuiltData);
	USE_NATIVE_SERIALIZER;
	DUMMY_PROP_TABLE
	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSamplingLODBuiltData& V);
};

struct FSkeletalMeshSamplingRegionBuiltData
{
	DECLARE_STRUCT(FSkeletalMeshSamplingRegionBuiltData);
	USE_NATIVE_SERIALIZER;
	DUMMY_PROP_TABLE
	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSamplingRegionBuiltData& V);
};

struct FSkeletalMeshSamplingBuiltData
{
	DECLARE_STRUCT(FSkeletalMeshSamplingBuiltData);
	TArray<FSkeletalMeshSamplingLODBuiltData> WholeMeshBuiltData;
	TArray<FSkeletalMeshSamplingRegionBuiltData> RegionBuiltData;
	BEGIN_PROP_TABLE
		PROP_ARRAY(WholeMeshBuiltData, "FSkeletalMeshSamplingLODBuiltData")
		PROP_ARRAY(RegionBuiltData, "FSkeletalMeshSamplingRegionBuiltData")
	END_PROP_TABLE
};

struct FSkeletalMeshSamplingInfo
{
	DECLARE_STRUCT(FSkeletalMeshSamplingInfo);
	TArray<FSkeletalMeshSamplingRegion> Regions;
	FSkeletalMeshSamplingBuiltData BuiltData;
	BEGIN_PROP_TABLE
		PROP_ARRAY(Regions, "FSkeletalMeshSamplingRegion")
		PROP_STRUC(BuiltData, FSkeletalMeshSamplingBuiltData)
	END_PROP_TABLE
};

// Defined dummy struct just for unversioned properties
struct FSkeletalMeshBuildSettings
{
	DECLARE_STRUCT(FSkeletalMeshBuildSettings);
	DUMMY_PROP_TABLE
};

struct FSkeletalMeshOptimizationSettings
{
	DECLARE_STRUCT(FSkeletalMeshOptimizationSettings);
	DUMMY_PROP_TABLE
};

struct FSkinWeightProfileInfo
{
	DECLARE_STRUCT(FSkinWeightProfileInfo);
	FName Name;
	BEGIN_PROP_TABLE
		PROP_NAME(Name)
	END_PROP_TABLE
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
	TArray<UMorphTarget*>	MorphTargets;
	TArray<USkeletalMeshSocket*> Sockets;
#if BORDERLANDS3
	byte					NumVertexColorChannels;
#endif
	FSkeletalMeshSamplingInfo SamplingInfo;
	TArray<FSkinWeightProfileInfo> SkinWeightProfiles;

	// properties
	bool					bHasVertexColors;

	CSkeletalMesh			*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_OBJ(Skeleton)
		PROP_BOOL(bHasVertexColors)
		PROP_ARRAY(LODInfo, "FSkeletalMeshLODInfo")
		PROP_ARRAY(MorphTargets, PropType::UObject)
		PROP_ARRAY(Sockets, PropType::UObject)
		PROP_STRUC(SamplingInfo, FSkeletalMeshSamplingInfo)
		PROP_ARRAY(SkinWeightProfiles, "FSkinWeightProfileInfo")
#if BORDERLANDS3
		PROP_BYTE(NumVertexColorChannels)
#endif
		PROP_DROP(bHasBeenSimplified)
		PROP_DROP(SamplingInfo)
		PROP_DROP(MinLod)
		PROP_DROP(PhysicsAsset)
		PROP_DROP(ShadowPhysicsAsset)
	END_PROP_TABLE

	USkeletalMesh4();
	virtual ~USkeletalMesh4();

	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad();

protected:
	void ConvertMesh();
};


/*-----------------------------------------------------------------------------
	UDestructibleMesh
-----------------------------------------------------------------------------*/

class UDestructibleMesh : public USkeletalMesh4
{
	DECLARE_CLASS(UDestructibleMesh, USkeletalMesh4);

	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize(Ar);
		// This class has lots of PhysX data
		DROP_REMAINING_DATA(Ar);
	}
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

#define TEXSTREAM_MAX_NUM_UVCHANNELS	4

struct FMeshUVChannelInfo
{
	DECLARE_STRUCT(FMeshUVChannelInfo);
	bool		bInitialized;
	bool		bOverrideDensities;
	float		LocalUVDensities[TEXSTREAM_MAX_NUM_UVCHANNELS];

	// Layout verified for UE4.26
	BEGIN_PROP_TABLE
		PROP_BOOL(bInitialized)
		PROP_BOOL(bOverrideDensities)
		PROP_FLOAT(LocalUVDensities)
	END_PROP_TABLE

	// The structure is serialized in both ways: native (SkeletalMesh) and as properties (StaticMesh)
	friend FArchive& operator<<(FArchive& Ar, FMeshUVChannelInfo& V)
	{
		Ar << V.bInitialized << V.bOverrideDensities;
		for (int i = 0; i < TEXSTREAM_MAX_NUM_UVCHANNELS; i++)
		{
			Ar << V.LocalUVDensities[i];
		}
		return Ar;
	}
};

struct FStaticMaterial
{
	DECLARE_STRUCT(FStaticMaterial);
	UMaterialInterface* MaterialInterface;
	FName				MaterialSlotName;
	FMeshUVChannelInfo	UVChannelData;

	// Layout verified for UE4.26
	BEGIN_PROP_TABLE
		PROP_OBJ(MaterialInterface)
		PROP_NAME(MaterialSlotName)
		PROP_DROP(ImportedMaterialSlotName, PropType::FName)
		PROP_STRUC(UVChannelData, FMeshUVChannelInfo)
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
	bool					bUseHighPrecisionTangentBasis;

	TArray<UStaticMeshSocket*> Sockets;
	TArray<UMaterialInterface*> Materials;		// pre-UE4.14
	TArray<FStaticMaterial> StaticMaterials;	// UE4.14+

	// FStaticMeshRenderData fields
	FBoxSphereBounds		Bounds;
	FBoxSphereBounds		ExtendedBounds;		// declared for unversioned property serializer
	float					ScreenSize[MAX_STATIC_LODS_UE4];
	bool					bLODsShareStaticLighting;
	TArray<FStaticMeshLODModel4> Lods;
	TArray<FStaticMeshSourceModel> SourceModels;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, PropType::UObject)
		PROP_ARRAY(SourceModels, "FStaticMeshSourceModel")
		PROP_ARRAY(StaticMaterials, "FStaticMaterial")
		PROP_ARRAY(Sockets, PropType::UObject)	// declared for UE4.26 unversioned props
		PROP_STRUC(ExtendedBounds, FBoxSphereBounds)
		PROP_DROP(AssetUserData)
		PROP_DROP(LightmapUVDensity)
		PROP_DROP(LightMapResolution)
		PROP_DROP(LightMapCoordinateIndex)
	END_PROP_TABLE

	UStaticMesh4();
	virtual ~UStaticMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
	void ConvertSourceModels();
};


/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

//!! References:
//!! CompressAnimationsFunctor() - Source/Editor/UnrealEd/Private/Commandlets/PackageUtilities.cpp

struct FCompressedOffsetData
{
	DECLARE_STRUCT(FCompressedOffsetData);
public:
	TArray<int32>			OffsetData;
	int32					StripSize;

	BEGIN_PROP_TABLE
		PROP_ARRAY(OffsetData, PropType::Int)
		PROP_INT(StripSize)
	END_PROP_TABLE

	int GetOffsetData(int Index, int Offset = 0)
	{
		return OffsetData[Index * StripSize + Offset];
	}

	friend FArchive& operator<<(FArchive& Ar, FCompressedOffsetData& D)
	{
		return Ar << D.OffsetData << D.StripSize;
	}

	bool IsValid() const
	{
		return StripSize > 0 && OffsetData.Num() != 0;
	}
};


struct FTrackToSkeletonMap
{
	DECLARE_STRUCT(FTrackToSkeletonMap);

	int32					BoneTreeIndex;

	BEGIN_PROP_TABLE
		PROP_INT(BoneTreeIndex)
		PROP_DROP(SkeletonIndex)	// this field was deprecated even before 4.0 release, however it appears in some old assets
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive& Ar, FTrackToSkeletonMap& M)
	{
		return Ar << M.BoneTreeIndex;
	}
};


struct FRichCurve // : public FIndexedCurve
{
	DECLARE_STRUCT(FRichCurve)

	int						Dummy;	// just to make non-empty structure

	BEGIN_PROP_TABLE
		PROP_DROP(PreInfinityExtrap)
		PROP_DROP(PostInfinityExtrap)
		PROP_DROP(DefaultValue)
		PROP_DROP(Keys)
	END_PROP_TABLE
};


struct FAnimCurveBase
{
	DECLARE_STRUCT(FAnimCurveBase);

	int						CurveTypeFlags;

	BEGIN_PROP_TABLE
		PROP_DROP(Name)			// FSmartName
		PROP_DROP(LastObservedName)
		PROP_INT(CurveTypeFlags)
	END_PROP_TABLE

	void PostSerialize(FArchive& Ar);
};



struct FFloatCurve : public FAnimCurveBase
{
	DECLARE_STRUCT2(FFloatCurve, FAnimCurveBase)

	FRichCurve				FloatCurve;

	BEGIN_PROP_TABLE
		PROP_STRUC(FloatCurve, FRichCurve)
	END_PROP_TABLE
};


struct FRawCurveTracks
{
	DECLARE_STRUCT(FRawCurveTracks);

	TArray<FFloatCurve>		FloatCurves;

	BEGIN_PROP_TABLE
		PROP_ARRAY(FloatCurves, "FFloatCurve")
	END_PROP_TABLE

	void PostSerialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FRawCurveTracks& T);
};

struct FAnimLinkableElement
{
	DECLARE_STRUCT(FAnimLinkableElement);

	int32					SlotIndex;
	int32					SegmentIndex;
	float					SegmentBeginTime;
	float					SegmentLength;

	BEGIN_PROP_TABLE
		PROP_DROP(LinkedMontage, PropType::UObject)
		PROP_INT(SlotIndex)
		PROP_INT(SegmentIndex)
		PROP_DROP(LinkMethod, PropType::Byte)
		PROP_DROP(CachedLinkMethod, PropType::Byte)
		PROP_FLOAT(SegmentBeginTime)
		PROP_FLOAT(SegmentLength)
		PROP_DROP(LinkValue, PropType::Float)
		PROP_DROP(LinkedSequence, PropType::UObject)
	END_PROP_TABLE
};

struct FAnimNotifyEvent : public FAnimLinkableElement
{
	DECLARE_STRUCT2(FAnimNotifyEvent, FAnimLinkableElement);

	float					TriggerTimeOffset;
	float					EndTriggerTimeOffset;
	float					TriggerWeightThreshold;
	FName					NotifyName;
	float					Duration;
	FAnimLinkableElement	EndLink;
	bool					bConvertedFromBranchingPoint;
	float					NotifyTriggerChance;
	int32					TrackIndex;

	BEGIN_PROP_TABLE
		PROP_DROP(DisplayTime_DEPRECATED, PropType::Float)
		PROP_FLOAT(TriggerTimeOffset)
		PROP_FLOAT(EndTriggerTimeOffset)
		PROP_FLOAT(TriggerWeightThreshold)
		PROP_NAME(NotifyName)
		PROP_DROP(Notify, PropType::UObject)
		PROP_DROP(NotifyStateClass, PropType::UObject)
		PROP_FLOAT(Duration)
		PROP_STRUC(EndLink, FAnimLinkableElement)
		PROP_BOOL(bConvertedFromBranchingPoint)
		PROP_DROP(MontageTickType, PropType::Byte)
		PROP_FLOAT(NotifyTriggerChance)
		PROP_DROP(NotifyFilterType, PropType::Byte)
		PROP_DROP(NotifyFilterLOD, PropType::Int)
		PROP_DROP(bTriggerOnDedicatedServer, PropType::Bool)
		PROP_DROP(bTriggerOnFollower, PropType::Bool)
		PROP_INT(TrackIndex)
	END_PROP_TABLE
};

class UAnimationAsset : public UObject
{
	DECLARE_CLASS(UAnimationAsset, UObject);
public:
	USkeleton*				Skeleton;
	FGuid					SkeletonGuid;

	BEGIN_PROP_TABLE
		PROP_OBJ(Skeleton)
	END_PROP_TABLE

	UAnimationAsset()
	:	Skeleton(NULL)
	{}

	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize(Ar);
		if (Ar.ArVer >= VER_UE4_SKELETON_GUID_SERIALIZATION)
			Ar << SkeletonGuid;
	}
};


class UAnimSequenceBase : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset);
public:
	TArray<FAnimNotifyEvent> Notifies;
	FRawCurveTracks			RawCurveData;
	float					SequenceLength;
	float					RateScale;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Notifies, "FAnimNotifyEvent")
		PROP_FLOAT(SequenceLength)
		PROP_FLOAT(RateScale)
		PROP_STRUC(RawCurveData, FRawCurveTracks)
	END_PROP_TABLE

	UAnimSequenceBase()
	:	RateScale(1.0f)
	{}

	virtual void Serialize(FArchive& Ar);
};

enum EAnimInterpolationType
{
	Linear,
	Step,
};

_ENUM(EAnimInterpolationType)
{
	_E(Linear),
	_E(Step),
};

enum EAdditiveAnimationType
{
	AAT_None,
	AAT_LocalSpaceBase,
	AAT_RotationOffsetMeshSpace,
};

_ENUM(EAdditiveAnimationType)
{
	_E(AAT_None),
	_E(AAT_LocalSpaceBase),
	_E(AAT_RotationOffsetMeshSpace),
};

enum EAdditiveBasePoseType
{
	ABPT_None,
	ABPT_RefPose,
	ABPT_AnimScaled,
	ABPT_AnimFrame,
};

_ENUM(EAdditiveBasePoseType)
{
	_E(ABPT_None),
	_E(ABPT_RefPose),
	_E(ABPT_AnimScaled),
	_E(ABPT_AnimFrame),
};

struct FCompressedSegment
{
	int32					StartFrame;
	int32					NumFrames;
	int32					ByteStreamOffset;
	AnimationCompressionFormat TranslationCompressionFormat;
	AnimationCompressionFormat RotationCompressionFormat;
	AnimationCompressionFormat ScaleCompressionFormat;

	friend FArchive& operator<<(FArchive& Ar, FCompressedSegment& S)
	{
		Ar << S.StartFrame << S.NumFrames << S.ByteStreamOffset;
		Ar << (byte&)S.TranslationCompressionFormat << (byte&)S.RotationCompressionFormat << (byte&)S.ScaleCompressionFormat;
		return Ar;
	}
};

struct FStringCurveKey
{
	DECLARE_STRUCT(FStringCurveKey)
	float					Time;
	FString					Value;

	friend FArchive& operator<<(FArchive& Ar, FStringCurveKey& K)
	{
		return Ar << K.Time << K.Value;
	}

	USE_NATIVE_SERIALIZER;

	BEGIN_PROP_TABLE
		PROP_FLOAT(Time)
		PROP_STRING(Value)
	END_PROP_TABLE
};

struct FStringCurve
{
	DECLARE_STRUCT(FStringCurve)
	FString					DefaultValue;
	TArray<FStringCurveKey>	Keys;
	BEGIN_PROP_TABLE
		PROP_STRING(DefaultValue)
		PROP_ARRAY(Keys, "FStringCurveKey")
	END_PROP_TABLE
};

struct FBakedStringCustomAttribute
{
	DECLARE_STRUCT(FBakedStringCustomAttribute)
	FName					AttributeName;
	FStringCurve			StringCurve;
	BEGIN_PROP_TABLE
		PROP_NAME(AttributeName)
		PROP_STRUC(StringCurve, FStringCurve)
	END_PROP_TABLE
};

struct FIntegralKey
{
	DECLARE_STRUCT(FIntegralKey)
	float					Time;
	int32					Value;

	// No native serializer

	BEGIN_PROP_TABLE
		PROP_FLOAT(Time)
		PROP_INT(Value)
	END_PROP_TABLE
};

struct FIntegralCurve
{
	DECLARE_STRUCT(FIntegralCurve)
	TArray<FIntegralKey>	Keys;
	int32					DefaultValue;
	bool					bUseDefaultValueBeforeFirstKey;
	BEGIN_PROP_TABLE
		PROP_ARRAY(Keys, "FIntegralKey")
		PROP_INT(DefaultValue)
		PROP_BOOL(bUseDefaultValueBeforeFirstKey)
	END_PROP_TABLE
};

struct FBakedIntegerCustomAttribute
{
	DECLARE_STRUCT(FBakedIntegerCustomAttribute)
	FName					AttributeName;
	FIntegralCurve			IntCurve;
	BEGIN_PROP_TABLE
		PROP_NAME(AttributeName)
		PROP_STRUC(IntCurve, FIntegralCurve)
	END_PROP_TABLE
};

struct FSimpleCurveKey
{
	DECLARE_STRUCT(FSimpleCurveKey)
	float					Time;
	float					Value;

	friend FArchive& operator<<(FArchive& Ar, FSimpleCurveKey& K)
	{
		return Ar << K.Time << K.Value;
	}

	USE_NATIVE_SERIALIZER;

	BEGIN_PROP_TABLE
		PROP_FLOAT(Time)
		PROP_FLOAT(Value)
	END_PROP_TABLE
};

struct FSimpleCurve
{
	DECLARE_STRUCT(FSimpleCurve)
	int8 /*TEnumAsByte<ERichCurveInterpMode>*/ InterpMode;
	TArray<FSimpleCurveKey>	Keys;
	BEGIN_PROP_TABLE
		PROP_BYTE(InterpMode)
		PROP_ARRAY(Keys, "FSimpleCurveKey")
	END_PROP_TABLE
};

struct FBakedFloatCustomAttribute
{
	DECLARE_STRUCT(FBakedFloatCustomAttribute)
	FName					AttributeName;
	FSimpleCurve			FloatCurve;
	BEGIN_PROP_TABLE
		PROP_NAME(AttributeName)
		PROP_STRUC(FloatCurve, FSimpleCurve)
	END_PROP_TABLE
};

struct FAnimSyncMarker
{
	DECLARE_STRUCT(FAnimSyncMarker)
	FName					MarkerName;
	float					Time;
	BEGIN_PROP_TABLE
		PROP_NAME(MarkerName)
		PROP_FLOAT(Time)
	END_PROP_TABLE
};

struct FBakedCustomAttributePerBoneData
{
	DECLARE_STRUCT(FBakedCustomAttributePerBoneData);
	int32 BoneTreeIndex;
	TArray<FBakedStringCustomAttribute> StringAttributes;
	TArray<FBakedIntegerCustomAttribute> IntAttributes;
	TArray<FBakedFloatCustomAttribute> FloatAttributes;
	BEGIN_PROP_TABLE
		PROP_INT(BoneTreeIndex)
		PROP_ARRAY(StringAttributes, "FBakedStringCustomAttribute")
		PROP_ARRAY(IntAttributes, "FBakedIntegerCustomAttribute")
		PROP_ARRAY(FloatAttributes, "FBakedFloatCustomAttribute")
	END_PROP_TABLE
};

class UAnimSequence4 : public UAnimSequenceBase
{
	DECLARE_CLASS(UAnimSequence4, UAnimSequenceBase);
public:
	int32					NumFrames;
	TArray<FRawAnimSequenceTrack> RawAnimationData;
	TArray<uint8>			CompressedByteStream;
	TArray<FCompressedSegment> CompressedSegments;
	bool					bUseRawDataOnly;

	AnimationKeyFormat		KeyEncodingFormat;
	AnimationCompressionFormat TranslationCompressionFormat;
	AnimationCompressionFormat RotationCompressionFormat;
	AnimationCompressionFormat ScaleCompressionFormat;
	TArray<int32>			CompressedTrackOffsets;
	FCompressedOffsetData	CompressedScaleOffsets;
	TArray<FTrackToSkeletonMap> TrackToSkeletonMapTable;			// used for raw data
	TArray<FTrackToSkeletonMap> CompressedTrackToSkeletonMapTable;	// used for compressed data, missing before 4.12
	FRawCurveTracks			CompressedCurveData;
	EAnimInterpolationType	Interpolation;
	EAdditiveAnimationType	AdditiveAnimType;
	EAdditiveBasePoseType	RefPoseType;
	UAnimSequence4*			RefPoseSeq;
	int32					RefFrameIndex;
	FName					RetargetSource;
	TArray<FTransform>		RetargetSourceAssetReferencePose;
	bool					bEnableRootMotion;
	TArray<FAnimSyncMarker>	AuthoredSyncMarkers;
	TArray<FBakedCustomAttributePerBoneData> BakedPerBoneCustomAttributeData;

	BEGIN_PROP_TABLE
		PROP_INT(NumFrames)
		// Before UE4.12 some fields were serialized as properties
		PROP_ENUM2(KeyEncodingFormat, AnimationKeyFormat)
		PROP_ENUM2(TranslationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(RotationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(ScaleCompressionFormat, AnimationCompressionFormat)
		PROP_ARRAY(CompressedTrackOffsets, PropType::Int)
		PROP_STRUC(CompressedScaleOffsets, FCompressedOffsetData)
		PROP_ARRAY(TrackToSkeletonMapTable, "FTrackToSkeletonMap")
		PROP_ARRAY(CompressedTrackToSkeletonMapTable, "FTrackToSkeletonMap")
		PROP_STRUC(CompressedCurveData, FRawCurveTracks)
		PROP_ENUM2(Interpolation, EAnimInterpolationType)
		PROP_ENUM2(AdditiveAnimType, EAdditiveAnimationType)
		PROP_ENUM2(RefPoseType, EAdditiveBasePoseType)
		PROP_NAME(RetargetSource)
		PROP_OBJ(RefPoseSeq)
		PROP_INT(RefFrameIndex)
		PROP_ARRAY(RetargetSourceAssetReferencePose, "FTransform")
		PROP_BOOL(bEnableRootMotion)
		PROP_ARRAY(AuthoredSyncMarkers, "FAnimSyncMarker")
		PROP_ARRAY(BakedPerBoneCustomAttributeData, "FBakedCustomAttributePerBoneData")
	END_PROP_TABLE

	UAnimSequence4()
	:	TranslationCompressionFormat(ACF_None)
	,	RotationCompressionFormat(ACF_None)
	,	ScaleCompressionFormat(ACF_None)
	,	KeyEncodingFormat(AKF_ConstantKeyLerp)
	,	Interpolation(Linear)
	,	AdditiveAnimType(AAT_None)
	{}

	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	// UE4.12-4.22
	void SerializeCompressedData(FArchive& Ar);
	// UE4.23+
	void SerializeCompressedData2(FArchive& Ar);
	// UE4.25+
	void SerializeCompressedData3(FArchive& Ar);

	int GetNumTracks() const;
	int GetTrackBoneIndex(int TrackIndex) const;
	int FindTrackForBoneIndex(int BoneIndex) const;
	void TransferPerTrackData(TArray<uint8>& Dst, const TArray<uint8>& Src);
};


class UAnimStreamable : public UAnimSequenceBase
{
	DECLARE_CLASS(UAnimStreamable, UAnimSequenceBase);
public:
	virtual void Serialize(FArchive& Ar)
	{
		appPrintf("UAnimStreamable: not implemented\n");
		DROP_REMAINING_DATA(Ar);
	}
};

#define REGISTER_MESH_CLASSES_U4 \
	REGISTER_CLASS(USkeleton) \
	REGISTER_CLASS(FBoneReference) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS(FSkeletalMeshSamplingRegionBoneFilter) \
	REGISTER_CLASS(FSkeletalMeshSamplingRegionMaterialFilter) \
	REGISTER_CLASS(FSkeletalMeshSamplingRegion) \
	REGISTER_CLASS(FSkeletalMeshSamplingLODBuiltData) \
	REGISTER_CLASS(FSkeletalMeshSamplingRegionBuiltData) \
	REGISTER_CLASS(FSkeletalMeshSamplingBuiltData) \
	REGISTER_CLASS(FSkeletalMeshSamplingInfo) \
	REGISTER_CLASS(FSkeletalMeshBuildSettings) \
	REGISTER_CLASS(FSkeletalMeshOptimizationSettings) \
	REGISTER_CLASS(FSkinWeightProfileInfo) \
	REGISTER_CLASS_ALIAS(USkeletalMesh4, USkeletalMesh) \
	REGISTER_CLASS(UMorphTarget) \
	REGISTER_CLASS(USkeletalMeshSocket) \
	REGISTER_CLASS(UDestructibleMesh) \
	REGISTER_CLASS_ALIAS(UStaticMesh4, UStaticMesh) \
	REGISTER_CLASS(FMeshBuildSettings) \
	REGISTER_CLASS(FStaticMeshSourceModel) \
	REGISTER_CLASS(FMeshUVChannelInfo) \
	REGISTER_CLASS(FStaticMaterial) \
	REGISTER_CLASS(FCompressedOffsetData) \
	REGISTER_CLASS(FTrackToSkeletonMap) \
	REGISTER_CLASS(FRichCurve) \
	REGISTER_CLASS(FFloatCurve) \
	REGISTER_CLASS(FRawCurveTracks) \
	REGISTER_CLASS(FVirtualBone) \
	REGISTER_CLASS(FBoneNode) \
	REGISTER_CLASS(FAnimSlotGroup) \
	REGISTER_CLASS(FStringCurveKey) \
	REGISTER_CLASS(FStringCurve) \
	REGISTER_CLASS(FBakedStringCustomAttribute) \
	REGISTER_CLASS(FIntegralKey) \
	REGISTER_CLASS(FIntegralCurve) \
	REGISTER_CLASS(FBakedIntegerCustomAttribute) \
	REGISTER_CLASS(FSimpleCurveKey) \
	REGISTER_CLASS(FSimpleCurve) \
	REGISTER_CLASS(FAnimSyncMarker) \
	REGISTER_CLASS(FBakedFloatCustomAttribute) \
	REGISTER_CLASS(FBakedCustomAttributePerBoneData) \
	REGISTER_CLASS(FAnimLinkableElement) \
	REGISTER_CLASS(FAnimNotifyEvent) \
	REGISTER_CLASS(UAnimationAsset) \
	REGISTER_CLASS(UAnimSequenceBase) \
	REGISTER_CLASS_ALIAS(UAnimSequence4, UAnimSequence) \
	REGISTER_CLASS(UAnimStreamable)

#define REGISTER_MESH_ENUMS_U4 \
	REGISTER_ENUM(EAnimInterpolationType) \
	REGISTER_ENUM(EBoneTranslationRetargetingMode) \
	REGISTER_ENUM(EAdditiveAnimationType) \
	REGISTER_ENUM(EAdditiveBasePoseType)


#endif // UNREAL4
#endif // __UNMESH4_H__
