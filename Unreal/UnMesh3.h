#ifndef __UNMESH3_H__
#define __UNMESH3_H__

#if UNREAL3

/*-----------------------------------------------------------------------------

UE3 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		USkeletalMesh
		UStaticMesh

-----------------------------------------------------------------------------*/

struct FBoxSphereBounds
{
	DECLARE_STRUCT(FBoxSphereBounds);
	FVector					Origin;
	FVector					BoxExtent;
	float					SphereRadius;
	BEGIN_PROP_TABLE
		PROP_VECTOR(Origin)
		PROP_VECTOR(BoxExtent)
		PROP_FLOAT(SphereRadius)
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive &Ar, FBoxSphereBounds &B)
	{
		return Ar << B.Origin << B.BoxExtent << B.SphereRadius;
	}
};


/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

struct FRawAnimSequenceTrack
{
	DECLARE_STRUCT(FRawAnimSequenceTrack);
	TArray<FVector>			PosKeys;
	TArray<FQuat>			RotKeys;
	TArray<float>			KeyTimes;

	BEGIN_PROP_TABLE
		PROP_ARRAY(PosKeys,  FVector)
		PROP_ARRAY(RotKeys,  FQuat)
		PROP_ARRAY(KeyTimes, float)
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive &Ar, FRawAnimSequenceTrack &T)
	{
		if (Ar.ArVer >= 577)
		{
			// newer UE3 version has replaced serializer for this structure
			Ar << RAW_ARRAY(T.PosKeys) << RAW_ARRAY(T.RotKeys);
			// newer version will not serialize times
			if (Ar.ArVer < 604) Ar << RAW_ARRAY(T.KeyTimes);
			return Ar;
		}
		return Ar << T.PosKeys << T.RotKeys << T.KeyTimes;
	}
};

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
	ACF_Delta40NoW,
	ACF_Delta48NoW,
#endif
#if MASSEFF
	ACF_BioFixed48,											// Mass Effect 2
#endif
#if TRANSFORMERS
	ACF_IntervalFixed48NoW,
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
#endif
#if MASSEFF
	_E(ACF_BioFixed48),
#endif
#if TRANSFORMERS
	_E(ACF_IntervalFixed48NoW),
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
};

#endif // MASSEFF


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
	TArray<int>				CompressedTrackOffsets;
	TArray<byte>			CompressedByteStream;
#if TUROK
	TArray<FBulkKeyframeDataEntry> KeyFrameData;
#endif
#if MASSEFF
	UBioAnimSetData			*m_pBioAnimSetData;
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
#if TUROK
		PROP_ARRAY(KeyFrameData, FBulkKeyframeDataEntry)
#endif
#if MASSEFF
		PROP_OBJ(m_pBioAnimSetData)
#endif
		// unsupported
		PROP_DROP(Notifies)
		PROP_DROP(CompressionScheme)
		PROP_DROP(bDoNotOverrideCompression)
		PROP_DROP(CompressCommandletVersion)
		//!! additive animations
		PROP_DROP(bIsAdditive)
		PROP_DROP(AdditiveRefName)
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
#endif // BATMAN
#if TRANSFORMERS
		PROP_DROP(TranslationScale)		//?? use it?
		PROP_DROP(bWasUsed)
		PROP_DROP(LoopOffsetTime)
#endif // TRANSFORMERS
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		guard(UAnimSequence::Serialize);
		assert(Ar.ArVer >= 372);		// older version is not yet ready
		Super::Serialize(Ar);
#if TUROK
		if (Ar.Game == GAME_Turok) return;
#endif
#if MASSEFF
		if (Ar.Game == GAME_MassEffect2 && Ar.ArLicenseeVer >= 110)
		{
			guard(SerializeMassEffect2);
			FByteBulkData RawAnimationBulkData;
			RawAnimationBulkData.Serialize(Ar);
			unguard;
		}
#endif // MASSEFF
#if MOH2010
		if (Ar.Game == GAME_MOH2010) goto old_code;
#endif
#if TERA
		if (Ar.Game == GAME_Tera && Ar.ArLicenseeVer >= 11) goto new_code; // we have overriden ArVer, so compare by ArLicenseeVer ...
#endif
		if (Ar.ArVer >= 577)
		{
		new_code:
			Ar << RawAnimData;			// this field was moved to RawAnimationData, RawAnimData is deprecated
		}
	old_code:
		Ar << CompressedByteStream;
		unguard;
	}
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
	{
		PreviewSkelMeshName.Str = "None";
	}
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

	virtual void Serialize(FArchive &Ar)
	{
		guard(UAnimSet::Serialize);
		UObject::Serialize(Ar);
#if TUROK
		if (Ar.Game == GAME_Turok)
		{
			// native part of structure
			//?? can simple skip to the end of file - these data are not used
			for (int i = 0; i < BulkDataBlocks.Num(); i++)
				Ar << BulkDataBlocks[i].mBulkData;
			return;
		}
#endif // TUROK
#if FRONTLINES
		if (Ar.Game == GAME_Frontlines && Ar.ArLicenseeVer >= 40)
		{
			guard(SerializeFrontlinesAnimSet);
			TArray<FFrontlinesHashSeq> HashSequences;
			Ar << HashSequences;
			// fill Sequences from HashSequences
			assert(Sequences.Num() == 0);
			Sequences.Empty(HashSequences.Num());
			for (int i = 0; i < HashSequences.Num(); i++) Sequences.AddItem(HashSequences[i].Seq);
			return;
			unguard;
		}
#endif // FRONTLINES
		unguard;
	}

	virtual void PostLoad()
	{
		ConvertAnims();		// should be called after loading of all used objects
	}
};



/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FStaticMeshLODModel;
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
	int						InternalVersion;	// GOW1_PC: 15, UT3: 16, UDK: 18-...
	UObject					*BodySetup;			// URB_BodySetup
	TArray<FStaticMeshLODModel> Lods;
	// kDOP tree
	TArray<FkDOPNode3>		kDOPNodes;
	TArray<FkDOPTriangle3>	kDOPTriangles;
//	TArray<FStaticMeshUnk4>	f48;

	bool					UseSimpleLineCollision;
	bool					UseSimpleBoxCollision;
	bool					UseSimpleRigidBodyCollision;
	bool					UseFullPrecisionUVs;
#if TRANSFORMERS
	FBoxSphereBounds		Bounds;				// Transformers has described StaticMesh.uc and serialized Bounds as property
#endif

	CStaticMesh				*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_OBJ(BodySetup)						// this property is serialized twice - in UObject and in UStaticMesh
		PROP_BOOL(UseSimpleLineCollision)
		PROP_BOOL(UseSimpleBoxCollision)
		PROP_BOOL(UseSimpleRigidBodyCollision)
		PROP_BOOL(UseFullPrecisionUVs)
#if TRANSFORMERS
		PROP_STRUC(Bounds, FBoxSphereBounds)
#endif
		PROP_DROP(LODDistanceRatio)
		PROP_DROP(LightMapResolution)
		PROP_DROP(LightMapCoordinateIndex)
		PROP_DROP(ContentTags)
		PROP_DROP(SourceFilePath)
		PROP_DROP(SourceFileTimestamp)
#if DECLARE_VIEWER_PROPS
		PROP_INT(InternalVersion)
#endif // DECLARE_VIEWER_PROPS
	END_PROP_TABLE

	UStaticMesh3();
	virtual ~UStaticMesh3();

	virtual void Serialize(FArchive &Ar);
};


#define REGISTER_MESH_CLASSES_TUROK \
	REGISTER_CLASS(FBulkKeyframeDataEntry) \
	REGISTER_CLASS(FBulkDataBlock)

#define REGISTER_MESH_CLASSES_MASSEFF \
	REGISTER_CLASS(UBioAnimSetData)

// UTdAnimSet - Mirror's Edge, derived from UAnimSet
#define REGISTER_MESH_CLASSES_U3	\
	REGISTER_CLASS(FRawAnimSequenceTrack) \
	REGISTER_CLASS(FBoxSphereBounds) \
	REGISTER_CLASS(UAnimSequence)	\
	REGISTER_CLASS(UAnimSet)		\
	REGISTER_CLASS_ALIAS(UAnimSet, UTdAnimSet) \
	REGISTER_CLASS_ALIAS(UStaticMesh3, UStaticMesh)

#define REGISTER_MESH_ENUMS_U3		\
	REGISTER_ENUM(AnimationCompressionFormat) \
	REGISTER_ENUM(AnimationKeyFormat)


#endif // UNREAL3

#endif // __UNMESH3_H__
