#ifndef __UNMESH3_H__
#define __UNMESH3_H__

#if UNREAL3

/*-----------------------------------------------------------------------------

UE3 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		USkeletalMesh
		UStaticMesh
		UAnimSequence
		UAnimSet

-----------------------------------------------------------------------------*/

#include "UnMesh.h"			// common types

// forwards
class UMaterialInterface;


/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FStaticLODModel3;


struct FSkeletalMeshLODInfo
{
	DECLARE_STRUCT(FSkeletalMeshLODInfo);
	float					DisplayFactor;
	float					LODHysteresis;
	TArray<int>				LODMaterialMap;
	TArray<bool>			bEnableShadowCasting;

	BEGIN_PROP_TABLE
		PROP_FLOAT(DisplayFactor)
		PROP_FLOAT(LODHysteresis)
		PROP_ARRAY(LODMaterialMap, int)
		PROP_ARRAY(bEnableShadowCasting, bool)
		PROP_DROP(TriangleSorting)
		PROP_DROP(TriangleSortSettings)
		PROP_DROP(bHasBeenSimplified)
#if FRONTLINES
		PROP_DROP(bExcludeFromConsoles)
		PROP_DROP(bCanRemoveForLowDetail)
#endif
#if MCARTA
		PROP_DROP(LODMaterialDrawOrder)
#endif
#if MKVSDC
		PROP_DROP(BonesEnabled)
		PROP_DROP(UsedForParticleSpawning)
#endif
	END_PROP_TABLE
};


class USkeletalMeshSocket : public UObject
{
	DECLARE_CLASS(USkeletalMeshSocket, UObject);
public:
	FName					SocketName;
	FName					BoneName;
	FVector					RelativeLocation;
	FRotator				RelativeRotation;
	FVector					RelativeScale;

	USkeletalMeshSocket()
	{
		RelativeLocation.Set(0, 0, 0);
		RelativeRotation.Set(0, 0, 0);
		RelativeScale.Set(1, 1, 1);
	}
	BEGIN_PROP_TABLE
		PROP_NAME(SocketName)
		PROP_NAME(BoneName)
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale)
#if MKVSDC
		PROP_DROP(m_EditConst)
		PROP_DROP(m_SocketName)
		PROP_DROP(m_RelativeLocation)
		PROP_DROP(m_RelativeRotation)
#endif // MKVSDC
	END_PROP_TABLE
};

#if MKVSDC
// MK X USkeleton
class USkeleton_MK : public UObject
{
	DECLARE_CLASS(USkeleton_MK, UObject);
public:
	TArray<VJointPos>		BonePos;
	TArray<FName>			BoneName;
	TArray<int16>			BoneParent;

	virtual void Serialize(FArchive &Ar);
};
#endif // MKVSDC


class USkeletalMesh3 : public UObject
{
	DECLARE_CLASS(USkeletalMesh3, UObject);
public:
	FBoxSphereBounds		Bounds;
	TArray<FSkeletalMeshLODInfo> LODInfo;
	TArray<USkeletalMeshSocket*> Sockets;
	FVector					MeshOrigin;
	FRotator				RotOrigin;
	TArray<FMeshBone>		RefSkeleton;
	int						SkeletalDepth;
	bool					bHasVertexColors;
	TArray<UMaterialInterface*> Materials;
	TArray<FStaticLODModel3> LODModels;
#if BATMAN
	// Batman 2
	bool					EnableTwistBoneFixers;
	bool					EnableClavicleFixer;
#endif // BATMAN
#if MKVSDC
	USkeleton_MK*			Skeleton;
#endif

	CSkeletalMesh			*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_ARRAY(LODInfo, FSkeletalMeshLODInfo)
		PROP_ARRAY(Sockets, UObject*)
		PROP_BOOL(bHasVertexColors)
		PROP_DROP(SkelMeshGUID)
		PROP_DROP(SkelMirrorTable)
		PROP_DROP(FaceFXAsset)
		PROP_DROP(bDisableSkeletalAnimationLOD)
		PROP_DROP(bForceCPUSkinning)
		PROP_DROP(bUsePackedPosition)
		PROP_DROP(BoundsPreviewAsset)
		PROP_DROP(PerPolyCollisionBones)
		PROP_DROP(AddToParentPerPolyCollisionBone)
		PROP_DROP(bUseSimpleLineCollision)
		PROP_DROP(bUseSimpleBoxCollision)
		PROP_DROP(LODBiasPS3)
		PROP_DROP(LODBiasXbox360)
		PROP_DROP(ClothToGraphicsVertMap)
		PROP_DROP(ClothWeldingMap)
		PROP_DROP(ClothWeldingDomain)
		PROP_DROP(ClothWeldedIndices)
		PROP_DROP(NumFreeClothVerts)
		PROP_DROP(ClothIndexBuffer)
		PROP_DROP(ClothBones)
		PROP_DROP(bEnableClothPressure)
		PROP_DROP(bEnableClothDamping)
		PROP_DROP(ClothStretchStiffness)
		PROP_DROP(ClothDensity)
		PROP_DROP(ClothFriction)
		PROP_DROP(ClothTearFactor)
		PROP_DROP(SourceFilePath)
		PROP_DROP(SourceFileTimestamp)
#if MEDGE
		PROP_DROP(NumUVSets)
#endif
#if BATMAN
		PROP_DROP(SkeletonName)
		PROP_DROP(Stretches)
		// Batman 2
		PROP_BOOL(EnableTwistBoneFixers)
		PROP_BOOL(EnableClavicleFixer)
		PROP_DROP(ClothingTeleportRefBones)
#endif // BATMAN
#if MKVSDC
		PROP_OBJ(Skeleton)
#endif
#if DECLARE_VIEWER_PROPS
		PROP_ARRAY(Materials, UObject*)
#endif // DECLARE_VIEWER_PROPS
	END_PROP_TABLE

	USkeletalMesh3();
	virtual ~USkeletalMesh3();

	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad();			// used to reconstruct sockets
#if BATMAN
	void FixBatman2Skeleton();
#endif

protected:
	void ConvertMesh();
};


/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

struct FRawAnimSequenceTrack
{
	DECLARE_STRUCT(FRawAnimSequenceTrack);
	TArray<FVector>			PosKeys;
	TArray<FQuat>			RotKeys;
	TArray<float>			KeyTimes;		// UE3: obsolete
#if UNREAL4
	TArray<FVector>			ScaleKeys;
#endif

	BEGIN_PROP_TABLE
		PROP_ARRAY(PosKeys,  FVector)
		PROP_ARRAY(RotKeys,  FQuat)
		PROP_ARRAY(KeyTimes, float)
#if UNREAL4
		PROP_ARRAY(ScaleKeys, FVector)
#endif
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive &Ar, FRawAnimSequenceTrack &T)
	{
		guard(FRawAnimSequenceTrack<<);
#if UNREAL4
		if (Ar.Game >= GAME_UE4_BASE)
		{
			T.PosKeys.BulkSerialize(Ar);
			T.RotKeys.BulkSerialize(Ar);
			if (Ar.ArVer >= VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION)
			{
				T.ScaleKeys.BulkSerialize(Ar);
			}
			return Ar;
		}
#endif // UNREAL4
		if (Ar.ArVer >= 577)
		{
			// newer UE3 version has replaced serializer for this structure
			T.PosKeys.BulkSerialize(Ar);
			T.RotKeys.BulkSerialize(Ar);
			// newer version will not serialize times
			if (Ar.ArVer < 604) T.KeyTimes.BulkSerialize(Ar);
			return Ar;
		}
		return Ar << T.PosKeys << T.RotKeys << T.KeyTimes;
		unguard;
	}
};

// Note: this enum should be binary compatible with UE4 AnimationCompressionFormat, because
// it is serialized by value in UAnimSequence4.
enum AnimationCompressionFormat
{
	ACF_None,
	ACF_Float96NoW,
	ACF_Fixed48NoW,
	ACF_IntervalFixed32NoW,
	ACF_Fixed32NoW,
	ACF_Float32NoW,
	ACF_Identity,
#if BATMAN
	ACF_Fixed48Max,
#endif
#if BORDERLANDS
	ACF_Delta40NoW,											// not implemented in game?
	ACF_Delta48NoW,
	ACF_PolarEncoded32,										// Borderlands 2
	ACF_PolarEncoded48,										// Borderlands 2
#endif
#if MASSEFF
	ACF_BioFixed48,											// Mass Effect 2
#endif
#if ARGONAUTS
	ACF_Float48NoW,
	ACF_Fixed64NoW,
	ATCF_Float16,											// half[3]; used for translation compression (originally came from different enum)
#endif
#if TRANSFORMERS || ARGONAUTS
	ACF_IntervalFixed48NoW,									// 2 games has such quaternion type, and it is different!
#endif
#if DISHONORED
	ACF_EdgeAnim,
#endif
#if BLADENSOUL
	ACF_ZOnlyRLE,
#endif
};

_ENUM(AnimationCompressionFormat)
{
	_E(ACF_None),
	_E(ACF_Float96NoW),
	_E(ACF_Fixed48NoW),
	_E(ACF_IntervalFixed32NoW),
	_E(ACF_Fixed32NoW),
	_E(ACF_Float32NoW),
	_E(ACF_Identity),
#if BATMAN
	_E(ACF_Fixed48Max),
#endif
#if BORDERLANDS
	_E(ACF_Delta40NoW),
	_E(ACF_Delta48NoW),
	_E(ACF_PolarEncoded32),
	_E(ACF_PolarEncoded48),
#endif
#if MASSEFF
	_E(ACF_BioFixed48),
#endif
#if ARGONAUTS
	_E(ACF_Float48NoW),
	_E(ACF_Fixed64NoW),
	_E(ATCF_Float16),
#endif
#if TRANSFORMERS || ARGONAUTS
	_E(ACF_IntervalFixed48NoW),
#endif
#if DISHONORED
	_E(ACF_EdgeAnim),
#endif
#if BLADENSOUL
	_E(ACF_ZOnlyRLE),
#endif
};

enum AnimationKeyFormat
{
	AKF_ConstantKeyLerp,									// animation keys are placed on evenly-spaced intervals
	AKF_VariableKeyLerp,									// animation keys have explicit key times
	AKF_PerTrackCompression,
};

_ENUM(AnimationKeyFormat)
{
	_E(AKF_ConstantKeyLerp),
	_E(AKF_VariableKeyLerp),
	_E(AKF_PerTrackCompression),
};


#if TUROK

struct FBulkKeyframeDataEntry
{
	DECLARE_STRUCT(FBulkKeyframeDataEntry);
	int						mStartKeyFrame;
	int						mEndKeyFrame;
	int						mBulkDataBlock;
	int						mUncompressedDataOffset;

	BEGIN_PROP_TABLE
		PROP_INT(mStartKeyFrame)
		PROP_INT(mEndKeyFrame)
		PROP_INT(mBulkDataBlock)
		PROP_INT(mUncompressedDataOffset)
	END_PROP_TABLE
};

struct FBulkDataBlock
{
	DECLARE_STRUCT(FBulkDataBlock);
	int						mUncompressedDataSize;
	TArray<byte>			mBulkData;		// uses native serializer

	BEGIN_PROP_TABLE
		PROP_INT(mUncompressedDataSize)
	END_PROP_TABLE
};

#endif // TUROK


#if MASSEFF
// Bioware has separated some common UAnimSequence settings

class UBioAnimSetData : public UObject
{
	DECLARE_CLASS(UBioAnimSetData, UObject);
public:
	bool					bAnimRotationOnly;
	TArray<FName>			TrackBoneNames;
	TArray<FName>			UseTranslationBoneNames;

	BEGIN_PROP_TABLE
		PROP_BOOL(bAnimRotationOnly)
		PROP_ARRAY(TrackBoneNames, FName)
		PROP_ARRAY(UseTranslationBoneNames, FName)
	END_PROP_TABLE

	virtual void PostLoad();
};

#endif // MASSEFF


class UAnimSet;

class UAnimSequence : public UObject
{
	DECLARE_CLASS(UAnimSequence, UObject);
public:
	FName					SequenceName;
//	TArray<FAnimNotifyEvent> Notifies;	// analogue of FMeshAnimNotify
	float					SequenceLength;
	int						NumFrames;
	float					RateScale;
	bool					bNoLoopingInterpolation;
	TArray<FRawAnimSequenceTrack> RawAnimData;
	AnimationCompressionFormat TranslationCompressionFormat;
	AnimationCompressionFormat RotationCompressionFormat;
	AnimationKeyFormat		KeyEncodingFormat;				// GoW2+
	TArray<int32>			CompressedTrackOffsets;
	TArray<uint8>			CompressedByteStream;
	bool					bIsAdditive;
	FName					AdditiveRefName;
#if TUROK
	TArray<FBulkKeyframeDataEntry> KeyFrameData;
#endif
#if MASSEFF
	UBioAnimSetData			*m_pBioAnimSetData;
#endif
#if ARGONAUTS
	TArray<unsigned>		CompressedTrackTimes;			// used as TArray<uint16>
	TArray<int>				CompressedTrackTimeOffsets;
#endif
#if BATMAN
	TArray<uint8>			AnimZip_Data;
#endif
#if TRANSFORMERS
	TArray<int>				Tracks;							// Transformers: Fall of Cybertron
	TArray<int>				TrackOffsets;
	TArray<uint8>			Trans3Data;
#endif

	UAnimSequence()
	:	RateScale(1.0f)
	,	TranslationCompressionFormat(ACF_None)
	,	RotationCompressionFormat(ACF_None)
	,	KeyEncodingFormat(AKF_ConstantKeyLerp)
	{}

	BEGIN_PROP_TABLE
		PROP_NAME(SequenceName)
		PROP_FLOAT(SequenceLength)
		PROP_INT(NumFrames)
		PROP_FLOAT(RateScale)
		PROP_BOOL(bNoLoopingInterpolation)
		PROP_ARRAY(RawAnimData, FRawAnimSequenceTrack)
		PROP_ENUM2(TranslationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(RotationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(KeyEncodingFormat, AnimationKeyFormat)
		PROP_ARRAY(CompressedTrackOffsets, int)
		PROP_BOOL(bIsAdditive)
		PROP_NAME(AdditiveRefName)
#if TUROK
		PROP_ARRAY(KeyFrameData, FBulkKeyframeDataEntry)
#endif
#if MASSEFF
		PROP_OBJ(m_pBioAnimSetData)
#endif
#if ARGONAUTS
		PROP_ARRAY(CompressedTrackTimeOffsets, int)
		PROP_DROP(CompressedTrackSizes)
#endif
#if TRANSFORMERS
		PROP_ARRAY(Tracks, int)				//?? not used?
		PROP_ARRAY(TrackOffsets, int)
#endif
		// unsupported
		PROP_DROP(Notifies)
		PROP_DROP(CompressionScheme)
		PROP_DROP(bDoNotOverrideCompression)
		PROP_DROP(CompressCommandletVersion)
		//!! curves
		PROP_DROP(CurveData)
#if TLR
		PROP_DROP(ActionID)
		PROP_DROP(m_ExtraData)
#endif // TLR
#if BATMAN
		PROP_DROP(FramesPerSecond)
		PROP_DROP(MeetingPointBeginRotationEnabled)
		PROP_DROP(MeetingPointEndRotationEnabled)
		PROP_DROP(MeetingPointBeginTranslationEnabled)
		PROP_DROP(MeetingPointEndTranslationEnabled)
		PROP_DROP(MeetingPointBeginRotation)
		PROP_DROP(MeetingPointEndRotation)
		PROP_DROP(MeetingPointBeginTranslation)
		PROP_DROP(MeetingPointEndTranslation)
		PROP_DROP(ReferencePoint)
		PROP_DROP(ReferencePointYaw)
		PROP_DROP(ReferenceOptions)
		PROP_DROP(CollisionOptions)
		PROP_DROP(BlendInPoint)
		PROP_DROP(BlendInDuration)
		PROP_DROP(BlendOutPoint)
		PROP_DROP(BlendOutDuration)
		PROP_DROP(ClippedStartPoint)
		PROP_DROP(ClippedEndPoint)
		PROP_DROP(CanCancelBeforeHerePoint)
		PROP_DROP(CanCancelAfterHerePoint)
		PROP_DROP(CanCorrectAfterHerePoint)
		PROP_DROP(ForwardYawOutPoint)
		PROP_DROP(ForwardYawStartOffset)
		PROP_DROP(ForwardYawEndOffset)
		PROP_DROP(FloorHeightInPoint)
		PROP_DROP(FloorHeightOutPoint)
		PROP_DROP(FloorHeightStartOffset)
		PROP_DROP(FloorHeightEndOffset)
		PROP_DROP(CollisionOptionsOutPoint)
		PROP_DROP(LinearCentre)
		PROP_DROP(LinearSpan)
		PROP_DROP(AllowCheekyBlendIn)
		PROP_DROP(AllowCheekyBlendOut)
		PROP_DROP(DisableProportionalMotionDuringBlendOut)
		PROP_DROP(bUseSimpleRootMotionXY)
		PROP_DROP(bUseSimpleFloorHeight)
		PROP_DROP(bUseSimpleForwardYaw)
		PROP_DROP(ClipRootMotionOutPoint)
		PROP_DROP(ProportionalMotionDistanceCap)
		// Batman 2
		PROP_DROP(WeaponSwitchPointEnabled)
		// Batman 4
		PROP_DROP(FootSyncOut)
		PROP_DROP(FootSyncOutSpeed)
		PROP_DROP(FootSyncOutDirection)
		PROP_DROP(MotionOptions)
		PROP_DROP(CollisionOptions2)
		PROP_DROP(AnimZip_LinearMotion)
#endif // BATMAN
#if TRANSFORMERS
		PROP_DROP(TranslationScale)		//?? use it?
		PROP_DROP(bWasUsed)
		PROP_DROP(LoopOffsetTime)
#endif // TRANSFORMERS
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar);

#if BATMAN
	void DecodeBatman2Anims(CAnimSequence *Dst, UAnimSet *Owner) const;
#endif
#if TRANSFORMERS
	void DecodeTrans3Anims(CAnimSequence *Dst, UAnimSet *Owner) const;
#endif
};

#if FRONTLINES
// native map<name, object>
struct FFrontlinesHashSeq
{
	FName          Name;
	UAnimSequence *Seq;

	friend FArchive& operator<<(FArchive &Ar, FFrontlinesHashSeq &S)
	{
		return Ar << S.Name << S.Seq;
	}
};
#endif // FRONTLINES

class UAnimSet : public UObject
{
	DECLARE_CLASS(UAnimSet, UObject);
public:
	bool					bAnimRotationOnly;
	TArray<FName>			TrackBoneNames;
	TArray<UAnimSequence*>	Sequences;
	TArray<FName>			UseTranslationBoneNames;
	TArray<FName>			ForceMeshTranslationBoneNames;
	FName					PreviewSkelMeshName;
#if TUROK
	TArray<FBulkDataBlock>	BulkDataBlocks;
	int						KeyFrameSize;
	int						RotationChannels;
	int						TranslationChannels;
#endif // TUROK
#if MASSEFF
	UBioAnimSetData			*m_pBioAnimSetData;
#endif

	CAnimSet				*ConvertedAnim;

	UAnimSet()
	:	bAnimRotationOnly(true)
	{}
	virtual ~UAnimSet();

	BEGIN_PROP_TABLE
		PROP_BOOL(bAnimRotationOnly)
		PROP_ARRAY(TrackBoneNames, FName)
		PROP_ARRAY(Sequences, UObject*)
		PROP_ARRAY(UseTranslationBoneNames, FName)
		PROP_ARRAY(ForceMeshTranslationBoneNames, FName)
		PROP_NAME(PreviewSkelMeshName)
#if TUROK
		PROP_ARRAY(BulkDataBlocks, FBulkDataBlock)
		PROP_INT(KeyFrameSize)
		PROP_INT(RotationChannels)
		PROP_INT(TranslationChannels)
#endif // TUROK
#if MASSEFF
		PROP_OBJ(m_pBioAnimSetData)
#endif
#if BATMAN
		PROP_DROP(SkeletonName)
#endif
	END_PROP_TABLE

	void ConvertAnims();
	virtual void Serialize(FArchive &Ar);

	virtual void PostLoad()
	{
		ConvertAnims();		// should be called after loading of all used objects
	}
};



/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FStaticMeshLODModel3;
struct FkDOPNode3;
struct FkDOPTriangle3;

/*struct FStaticMeshUnk4
{
	TArray<UObject*>	f0;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUnk4 &V)
	{
		return Ar << V.f0;
	}
};*/


class UStaticMesh3 : public UObject
{
	DECLARE_CLASS(UStaticMesh3, UObject);
public:
	// NOTE: UStaticMesh is not mirrored by script with exception of Transformers game, so most
	// field names are unknown
	FBoxSphereBounds		Bounds;
	int						InternalVersion;	// GOW1_PC: 15, UT3: 16, UDK: 18-...
	UObject					*BodySetup;			// URB_BodySetup
	TArray<FStaticMeshLODModel3> Lods;
	// kDOP tree
	TArray<FkDOPNode3>		kDOPNodes;
	TArray<FkDOPTriangle3>	kDOPTriangles;
//	TArray<FStaticMeshUnk4>	f48;

	bool					UseSimpleLineCollision;
	bool					UseSimpleBoxCollision;
	bool					UseSimpleRigidBodyCollision;
	bool					UseFullPrecisionUVs;
#if BATMAN
	// Batman 2
	bool					CanCompressPositions;
	bool					CanStripNormalsAndTangents;
#endif // BATMAN

	CStaticMesh				*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_OBJ(BodySetup)						// this property is serialized twice - as a property and in UStaticMesh::Serialize
		PROP_BOOL(UseSimpleLineCollision)
		PROP_BOOL(UseSimpleBoxCollision)
		PROP_BOOL(UseSimpleRigidBodyCollision)
		PROP_BOOL(UseFullPrecisionUVs)
#if TRANSFORMERS
		PROP_STRUC(Bounds, FBoxSphereBounds)	// Transformers has described StaticMesh.uc and serialized Bounds as property
#endif
#if BATMAN
		PROP_BOOL(CanCompressPositions)
		PROP_BOOL(CanStripNormalsAndTangents)
#endif // BATMAN
		PROP_DROP(LODDistanceRatio)
		PROP_DROP(LightMapResolution)
		PROP_DROP(LightMapCoordinateIndex)
		PROP_DROP(ContentTags)
		PROP_DROP(SourceFilePath)
		PROP_DROP(SourceFileTimestamp)
		PROP_DROP(bCanBecomeDynamic)
#if BIOSHOCK3
		PROP_DROP(UseCompressedPositions)
#endif
#if DECLARE_VIEWER_PROPS
		PROP_INT(InternalVersion)
#endif // DECLARE_VIEWER_PROPS
	END_PROP_TABLE

	UStaticMesh3();
	virtual ~UStaticMesh3();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
};


#define REGISTER_MESH_CLASSES_TUROK \
	REGISTER_CLASS(FBulkKeyframeDataEntry) \
	REGISTER_CLASS(FBulkDataBlock)

#define REGISTER_MESH_CLASSES_MASSEFF \
	REGISTER_CLASS(UBioAnimSetData)

#define REGISTER_MESH_CLASSES_TRANS \
	REGISTER_CLASS_ALIAS(USkeletalMesh3, USkeletalMeshSkeleton)

#define REGISTER_MESH_CLASSES_MK \
	REGISTER_CLASS_ALIAS(USkeleton_MK, USkeleton)

// UGolemSkeletalMesh - APB: Reloaded, derived from USkeletalMesh
// UTdAnimSet - Mirror's Edge, derived from UAnimSet
#define REGISTER_MESH_CLASSES_U3	\
	REGISTER_CLASS(USkeletalMeshSocket) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS_ALIAS(USkeletalMesh3, UGolemSkeletalMesh) \
	REGISTER_CLASS(FRawAnimSequenceTrack) \
	REGISTER_CLASS(UAnimSequence)	\
	REGISTER_CLASS(UAnimSet)		\
	REGISTER_CLASS_ALIAS(UAnimSet, UTdAnimSet) \
	REGISTER_CLASS_ALIAS(USkeletalMesh3, USkeletalMesh) \
	REGISTER_CLASS_ALIAS(UStaticMesh3, UStaticMesh) \
	REGISTER_CLASS_ALIAS(UStaticMesh3, UFracturedStaticMesh)

#define REGISTER_MESH_ENUMS_U3		\
	REGISTER_ENUM(AnimationCompressionFormat) \
	REGISTER_ENUM(AnimationKeyFormat)


#endif // UNREAL3

#endif // __UNMESH3_H__
