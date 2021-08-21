#include "Core.h"

#if UNREAL4

#include "UnCore.h"
#include "UnObject.h"
#include "UnMesh4.h"
#include "UnMeshTypes.h"
#include "UnrealPackage/UnPackage.h"

#include "Mesh/SkeletalMesh.h"
#include "TypeConvert.h"

//#define DEBUG_DECOMPRESS	1
//#define DEBUG_SKELMESH	1
//#define DEBUG_ANIM		1
//#define DEBUG_RETARGET	1
//#define DEBUG_RETARGET_BONE	"r_brow_outer"

// References in UE4 code: Engine/Public/AnimationCompression.h
// - FAnimationCompression_PerTrackUtils
// - DecompressRotation()
// - DecompressTranslation()
// - DecompressScale()

/*-----------------------------------------------------------------------------
	USkeleton
-----------------------------------------------------------------------------*/

USkeleton::~USkeleton()
{
	if (ConvertedAnim) delete ConvertedAnim;
}

#if BAKE_BONE_SCALES

// Get the effective scale of the particular bone, with taking into account scales of the bone's parents
static CVec3 GetBoneScale(const FReferenceSkeleton& Skel, const TArray<FTransform>& BoneTransforms, int BoneIndex)
{
	guard(GetBoneScale);

	CVec3 Scale;
	Scale.Set(1, 1, 1);
	// Get the parent bone, ignore scale of the current one
	BoneIndex = Skel.RefBoneInfo[BoneIndex].ParentIndex;

	while (BoneIndex >= 0)
	{
		FVector BoneScale = BoneTransforms[BoneIndex].Scale3D;
		// Accumulate the scale
		Scale.Scale(CVT(BoneScale));
		// Get the bone's parent
		BoneIndex = Skel.RefBoneInfo[BoneIndex].ParentIndex;
	}

	return Scale;

	unguard;
}

// Adjust array of bone transforms. Note that 'Transforms' array will be modified here, however only
// 'Translation' part will be changed. Its 'Scale3D' field is used to compute the scale.
static bool AdjustBoneScales(const FReferenceSkeleton& SkeletonHierarchy, TArray<FTransform>& Transforms)
{
	guard(AdjustBoneScales);

	if (SkeletonHierarchy.RefBoneInfo.Num() != Transforms.Num())
	{
		return false;
	}

	for (int BoneIndex = 0; BoneIndex < Transforms.Num(); BoneIndex++)
	{
		CVec3 Scale = GetBoneScale(SkeletonHierarchy, Transforms, BoneIndex);
		FTransform& Transform = Transforms[BoneIndex];
		CVT(Transform.Translation).Scale(Scale);
	}

	return true;

	unguard;
}

#endif // BAKE_BONE_SCALES

FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& S)
{
	guard(FReferenceSkeleton<<);

	Ar << S.RefBoneInfo;
	Ar << S.RefBonePose;

	if (Ar.ArVer >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
		Ar << S.NameToIndexMap;

#if DAYSGONE
	if (Ar.Game == GAME_DaysGone)
	{
		// Additional per-bone attribute
		TArray<FVector> unk;
		Ar << unk;
	}
#endif // DAYSGONE

	int NumBones = S.RefBoneInfo.Num();
	if (Ar.ArVer < VER_UE4_FIXUP_ROOTBONE_PARENT && NumBones > 0 && S.RefBoneInfo[0].ParentIndex != INDEX_NONE)
		S.RefBoneInfo[0].ParentIndex = INDEX_NONE;

#if DEBUG_SKELMESH
	assert(S.RefBonePose.Num() == NumBones);
	assert(S.NameToIndexMap.Num() == NumBones);

	for (int i = 0; i < NumBones; i++)
	{
		appPrintf("bone[%d] \"%s\" parent=%d\n",
			i, *S.RefBoneInfo[i].Name, S.RefBoneInfo[i].ParentIndex);
	}
#endif // DEBUG_SKELMESH

#ifdef DEBUG_RETARGET_BONE
	for (int i = 0; i < NumBones; i++)
	{
		if (S.RefBoneInfo[i].Name == DEBUG_RETARGET_BONE)
		{
	#if BAKE_BONE_SCALES
			CVec3 BoneScale = GetBoneScale(S, S.RefBonePose, i);
	#else
			FVector BoneScale = S.RefBonePose[i].Scale3D;
	#endif
			CVec3 Translation = CVT(S.RefBonePose[i].Translation);
			appPrintf("    [%d] %s: (%g %g %g), len %g , scale (%g %g %g)\n", i, *S.RefBoneInfo[i].Name,
				VECTOR_ARG(Translation), Translation.GetLength(), VECTOR_ARG(BoneScale));
		}
	}
#endif // DEBUG_RETARGET_BONE

#if BAKE_BONE_SCALES
	// Adjust skeleton's scale, if any. Use scale of the root bone.
	AdjustBoneScales(S, S.RefBonePose);
#endif

	return Ar;

	unguard;
}

FArchive& operator<<(FArchive& Ar, FReferencePose& P)
{
	guard(FReferencePose<<);

	Ar << P.PoseName << P.ReferencePose;

	// Serialize editor asset data
	if (Ar.ContainsEditorData())
	{
		if (FAnimPhysObjectVersion::Get(Ar) < FAnimPhysObjectVersion::ChangeRetargetSourceReferenceToSoftObjectPtr)
		{
			UObject* EditorMesh;
			Ar << EditorMesh;
		}
		else
		{
			// TSoftObjectPtr<USkeletalMesh>
			// Implementation is quite deep in source code. See ULinkerLoad::operator<<(FSoftObjectPtr&).
			// -> FSoftObjectPath::SerializePath()
			assert(Ar.ArVer >= VER_UE4_ADDED_SOFT_OBJECT_PATH); // different serialization
			FName AssetPathName;
			FString SubPathString;
			Ar << AssetPathName << SubPathString;
		}
	}

	return Ar;

	unguard;
}

struct FSmartName
{
	FName DisplayName;

	friend FArchive& operator<<(FArchive& Ar, FSmartName& N)
	{
		Ar << N.DisplayName;
		if (FAnimPhysObjectVersion::Get(Ar) < FAnimPhysObjectVersion::RemoveUIDFromSmartNameSerialize)
		{
			uint16 UID;
			Ar << UID;
		}
		if (FAnimPhysObjectVersion::Get(Ar) < FAnimPhysObjectVersion::SmartNameRefactorForDeterministicCooking)
		{
			FGuid Guid;
			Ar << Guid;
		}
		return Ar;
	}
};

FArchive& operator<<(FArchive& Ar, FSmartNameMapping& N)
{
	guard(FSmartNameMapping<<);

	FFrameworkObjectVersion::Type FrwVer = FFrameworkObjectVersion::Get(Ar);

	if (FrwVer < FFrameworkObjectVersion::SmartNameRefactor)
	{
		// pre-UE4.13 code
		int16					NextUid;
		TMap<int16, FName>		UidMap;
		return Ar << NextUid << UidMap;
	}

	// UE4.13+
	if (FAnimPhysObjectVersion::Get(Ar) < FAnimPhysObjectVersion::SmartNameRefactorForDeterministicCooking)
	{
		// UE4.13-17
		TMap<FName, FGuid> GuidMap;
		Ar << GuidMap;
	}

	if (FrwVer >= FFrameworkObjectVersion::MoveCurveTypesToSkeleton)
	{
		// UE4.14+
		TMap<FName, FCurveMetaData> CurveMetaDataMap;
		Ar << CurveMetaDataMap;
	}
	return Ar;

	unguard;
}

FArchive& operator<<(FArchive& Ar, FMeshBoneInfo& B)
{
	Ar << B.Name << B.ParentIndex;
	if (Ar.ArVer < VER_UE4_REFERENCE_SKELETON_REFACTOR)
	{
		FColor Color;
		Ar << Color;
	}
	// Support for editor packages
	if ((Ar.ArVer >= VER_UE4_STORE_BONE_EXPORT_NAMES) && Ar.ContainsEditorData())
	{
		FString ExportName;
		Ar << ExportName;
	}
	return Ar;
}

void USkeleton::Serialize(FArchive &Ar)
{
	guard(USkeleton::Serialize);

	Super::Serialize(Ar);

	if (Ar.ArVer >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
		Ar << ReferenceSkeleton;

	if (Ar.ArVer >= VER_UE4_FIX_ANIMATIONBASEPOSE_SERIALIZATION)
	{
		Ar << AnimRetargetSources;
#if DEBUG_RETARGET
		appPrintf("Retarget sources: %d\n", AnimRetargetSources.Num());
		for (int i = 0; i < AnimRetargetSources.Num(); i++)
		{
			appPrintf("  %d: %s%s\n", i, *AnimRetargetSources[i].Key,
				ReferenceSkeleton.RefBoneInfo.Num() == AnimRetargetSources[i].Value.ReferencePose.Num() ?
					"" : " // mismatched bone count");
	#ifdef DEBUG_RETARGET_BONE
			const FReferencePose& Pose = AnimRetargetSources[i].Value;
			for (int j = 0; j < Pose.ReferencePose.Num(); j++)
			{
				if (ReferenceSkeleton.RefBoneInfo.IsValidIndex(j) &&
					ReferenceSkeleton.RefBoneInfo[j].Name == DEBUG_RETARGET_BONE)
				{
		#if BAKE_BONE_SCALES
					CVec3 BoneScale = GetBoneScale(ReferenceSkeleton, Pose.ReferencePose, j);
		#else
					FVector BoneScale = Pose.ReferencePose[j].Scale3D;
		#endif
					CVec3 Translation = CVT(Pose.ReferencePose[j].Translation);
					appPrintf("    [%d] %s: (%g %g %g), len %g, scale (%g %g %g)\n", j, *ReferenceSkeleton.RefBoneInfo[j].Name,
						VECTOR_ARG(Translation), Translation.GetLength(), VECTOR_ARG(BoneScale));
				}
			}
	#endif // DEBUG_RETARGET_BONE
		}
#endif

#if BAKE_BONE_SCALES
		// Adjust scales of retarget pose bones
		for (auto& It : AnimRetargetSources)
		{
			FReferencePose& Pose = It.Value;
			guard(AdjustScalesForRetargetSource);
			if (!AdjustBoneScales(ReferenceSkeleton, Pose.ReferencePose))
			{
				appPrintf("WARNING: AnimRetargetSources[%s] has wrong bone count %d (should be %d)",
					*It.Key, Pose.ReferencePose.Num(), ReferenceSkeleton.RefBoneInfo.Num());
			}
			unguardf("%s", *It.Key);
		}
#endif // BAKE_BONE_SCALES
	}
	else
	{
		// Pre-UE4.0 code
		appPrintf("USkeleton has old AnimRetargetSources format, skipping\n");
		DROP_REMAINING_DATA(Ar);
		return;
	}

#if DEBUG_SKELMESH
	appPrintf("Skeleton BoneTree:\n");
	for (int i = 0; i < BoneTree.Num(); i++)
	{
		const FBoneNode& Bone = BoneTree[i];
		appPrintf("[%d] \"%s\" = %s\n", i, *ReferenceSkeleton.RefBoneInfo[i].Name, EnumToName(Bone.TranslationRetargetingMode));
	}
#endif // DEBUG_SKELMESH

	if (Ar.ArVer >= VER_UE4_SKELETON_GUID_SERIALIZATION)
		Ar << Guid;

	guard(SmartNames);
	if (Ar.ArVer >= VER_UE4_SKELETON_ADD_SMARTNAMES)
		Ar << SmartNames;
	unguard;

#if KH3
	if (Ar.Game == GAME_KH3)
	{
		DROP_REMAINING_DATA(Ar);
		return;
	}
#endif // KH3

	if (FAnimObjectVersion::Get(Ar) >= FAnimObjectVersion::StoreMarkerNamesOnSkeleton)
	{
		FStripDataFlags StripFlags(Ar);
		if (!StripFlags.IsEditorDataStripped())
		{
			TArray<FName> ExistingMarkerNames;
			Ar << ExistingMarkerNames;
		}
	}

	unguard;
}


void USkeleton::PostLoad()
{
	// Create an empty AnimSet if not yet created
	ConvertAnims(NULL);
}


#if DEBUG_DECOMPRESS
#define DBG(...)			appPrintf(__VA_ARGS__)
#else
#define DBG(...)
#endif

static void ReadTimeArray(FArchive &Ar, int NumKeys, TArray<float> &Times, int NumFrames)
{
	guard(ReadTimeArray);

	Times.Empty(NumKeys);
	if (NumKeys <= 1) return;

//	appPrintf("  pos=%4X keys (max=%X)[ ", Ar.Tell(), NumFrames);
	if (NumFrames < 256)
	{
		for (int k = 0; k < NumKeys; k++)
		{
			uint8 v;
			Ar << v;
			Times.Add(v);
//			if (k < 4 || k > NumKeys - 5) appPrintf(" %02X ", v);
//			else if (k == 4) appPrintf("...");
		}
	}
	else
	{
		for (int k = 0; k < NumKeys; k++)
		{
			uint16 v;
			Ar << v;
			Times.Add(v);
//			if (k < 4 || k > NumKeys - 5) appPrintf(" %04X ", v);
//			else if (k == 4) appPrintf("...");
		}
	}
//	appPrintf(" ]\n");

	// align to 4 bytes
	Ar.Seek(Align(Ar.Tell(), 4));

	unguard;
}

#define DECODE_PER_TRACK_INFO(info)									\
			KeyFormat = (AnimationCompressionFormat)(info >> 28);	\
			ComponentMask = (info >> 24) & 0xF;						\
			NumKeys       = info & 0xFFFFFF;						\
			HasTimeTracks = (ComponentMask & 8) != 0;

// Compressed data decode helpers

// position
#define TP(Enum, VecType)						\
				case Enum:						\
					{							\
						VecType v;				\
						Reader << v;			\
						A->KeyPos.Add(CVT(v));	\
					}							\
					break;
// position ranged
#define TPR(Enum, VecType, Array)				\
				case Enum:						\
					{							\
						VecType v;				\
						Reader << v;			\
						FVector v2 = v.ToVector(Mins, Ranges); \
						Array.Add(CVT(v2));		\
					}							\
					break;
// rotation
#define TR(Enum, QuatType, Array)				\
				case Enum:						\
					{							\
						QuatType q;				\
						Reader << q;			\
						Array.Add(CVT(q));		\
					}							\
					break;
// rotation ranged
#define TRR(Enum, QuatType, Array)				\
				case Enum:						\
					{							\
						QuatType q;				\
						Reader << q;			\
						FQuat q2 = q.ToQuat(Mins, Ranges); \
						Array.Add(CVT(q2));		\
					}							\
					break;

static void ReadPerTrackQuatData(FArchive& Reader, int TrackIndex, const char* TrackKind,
	TArray<CQuat>& DstKeys, TArray<float>& DstTimeKeys, int NumFrames)
{
	guard(ReadPerTrackQuatData);

	uint32 PackedInfo;
	AnimationCompressionFormat KeyFormat;
	int ComponentMask;
	int NumKeys;
	bool HasTimeTracks;

	FVector Mins, Ranges;
	static const CQuat nullQuat = { 0, 0, 0, 1 };

	Reader << PackedInfo;
	DECODE_PER_TRACK_INFO(PackedInfo);
	DstKeys.Empty(NumKeys);
	DBG("    [%d] %s: fmt=%d (%s), %d keys, mask %d\n", TrackIndex, TrackKind,
		KeyFormat, EnumToName(KeyFormat), NumKeys, ComponentMask
	);
	if (KeyFormat == ACF_IntervalFixed32NoW)
	{
		// read mins/maxs
		Mins.Set(0, 0, 0);
		Ranges.Set(0, 0, 0);
		if (ComponentMask & 1) Reader << Mins.X << Ranges.X;
		if (ComponentMask & 2) Reader << Mins.Y << Ranges.Y;
		if (ComponentMask & 4) Reader << Mins.Z << Ranges.Z;
	}
	for (int k = 0; k < NumKeys; k++)
	{
		switch (KeyFormat)
		{
		case ACF_None:
		case ACF_Float96NoW:
			{
				FQuatFloat96NoW q;
				Reader << q;
				FQuat q2 = q;				// convert
				DstKeys.Add(CVT(q2));
			}
			break;
		case ACF_Fixed48NoW:
			{
				FQuatFixed48NoW q;
				q.X = q.Y = q.Z = 32767;	// corresponds to 0
				if (ComponentMask & 1) Reader << q.X;
				if (ComponentMask & 2) Reader << q.Y;
				if (ComponentMask & 4) Reader << q.Z;
				FQuat q2 = q;				// convert
				DstKeys.Add(CVT(q2));
			}
			break;
		TR (ACF_Fixed32NoW, FQuatFixed32NoW, DstKeys)
		TRR(ACF_IntervalFixed32NoW, FQuatIntervalFixed32NoW, DstKeys)
		TR (ACF_Float32NoW, FQuatFloat32NoW, DstKeys)
		case ACF_Identity:
			DstKeys.Add(nullQuat);
			break;
		default:
			appError("Unknown %s compression method: %d (%s)", TrackKind, KeyFormat, EnumToName(KeyFormat));
		}
	}
	// align to 4 bytes
	Reader.Seek(Align(Reader.Tell(), 4));
	if (HasTimeTracks)
		ReadTimeArray(Reader, NumKeys, DstTimeKeys, NumFrames);

	unguard;
}

static void ReadPerTrackVectorData(FArchive& Reader, int TrackIndex, const char* TrackKind,
	TArray<CVec3>& DstKeys, TArray<float>& DstTimeKeys, int NumFrames)
{
	guard(ReadPerTrackVectorData);

	uint32 PackedInfo;
	AnimationCompressionFormat KeyFormat;
	int ComponentMask;
	int NumKeys;
	bool HasTimeTracks;

	FVector Mins, Ranges;
	static const CVec3 nullVec = { 0, 0, 0 };

	Reader << PackedInfo;
	DECODE_PER_TRACK_INFO(PackedInfo);
	DstKeys.Empty(NumKeys);
	DBG("    [%d] %s: fmt=%d (%s), %d keys, mask %d\n", TrackIndex, TrackKind,
		KeyFormat, EnumToName(KeyFormat), NumKeys, ComponentMask
	);
	if (KeyFormat == ACF_IntervalFixed32NoW)
	{
		// read mins/maxs
		Mins.Set(0, 0, 0);
		Ranges.Set(0, 0, 0);
		if (ComponentMask & 1) Reader << Mins.X << Ranges.X;
		if (ComponentMask & 2) Reader << Mins.Y << Ranges.Y;
		if (ComponentMask & 4) Reader << Mins.Z << Ranges.Z;
	}
	for (int k = 0; k < NumKeys; k++)
	{
		switch (KeyFormat)
		{
		case ACF_None:
		case ACF_Float96NoW:
			{
				FVector v;
				if (ComponentMask & 7)
				{
					v.Set(0, 0, 0);
					if (ComponentMask & 1) Reader << v.X;
					if (ComponentMask & 2) Reader << v.Y;
					if (ComponentMask & 4) Reader << v.Z;
				}
				else
				{
					// ACF_Float96NoW has a special case for ((ComponentMask & 7) == 0)
					Reader << v;
				}
				DstKeys.Add(CVT(v));
			}
			break;
		TPR(ACF_IntervalFixed32NoW, FVectorIntervalFixed32, DstKeys)
		case ACF_Fixed48NoW:
			{
				uint16 X, Y, Z;
				CVec3 v;
				v.Set(0, 0, 0);
				if (ComponentMask & 1)
				{
					Reader << X; v[0] = DecodeFixed48_PerTrackComponent<7>(X);
				}
				if (ComponentMask & 2)
				{
					Reader << Y; v[1] = DecodeFixed48_PerTrackComponent<7>(Y);
				}
				if (ComponentMask & 4)
				{
					Reader << Z; v[2] = DecodeFixed48_PerTrackComponent<7>(Z);
				}
				DstKeys.Add(v);
			}
			break;
		case ACF_Identity:
			DstKeys.Add(nullVec);
			break;
		default:
			appError("Unknown %s compression method: %d (%s)", TrackKind, KeyFormat, EnumToName(KeyFormat));
		}
	}
	// align to 4 bytes
	Reader.Seek(Align(Reader.Tell(), 4));
	if (HasTimeTracks)
		ReadTimeArray(Reader, NumKeys, DstTimeKeys, NumFrames);

	unguard;
}


static void FixRotationKeys(CAnimSequence* Anim)
{
	for (int TrackIndex = 0; TrackIndex < Anim->Tracks.Num(); TrackIndex++)
	{
		if (TrackIndex == 0) continue;	// don't fix root track
		CAnimTrack* Track = Anim->Tracks[TrackIndex];
		for (CQuat& Key : Track->KeyQuat)
		{
			Key.Conjugate();
		}
	}
}

#if BAKE_BONE_SCALES

// Use skeleton's bone settings to adjust animation sequences
static void AdjustSequenceBySkeleton(USkeleton* Skeleton, const TArray<FTransform>& Transforms, CAnimSequence* Anim)
{
	guard(AdjustSequenceBySkeleton);

	if (Skeleton->ReferenceSkeleton.RefBoneInfo.Num() == 0) return;
	if (Skeleton->ReferenceSkeleton.RefBoneInfo.Num() != Transforms.Num())
	{
		// Bad retarget skeleton, the situation is already signalled in USkeleton::Serialize.
#if DEBUG_RETARGET
		appPrintf("Adjust by scale: skip due to bad bone count\n");
#endif
		return;
	}

	for (int TrackIndex = 0; TrackIndex < Anim->Tracks.Num(); TrackIndex++)
	{
		CAnimTrack* Track = Anim->Tracks[TrackIndex];
		CVec3 BoneScale = GetBoneScale(Skeleton->ReferenceSkeleton, Transforms, TrackIndex);
		if ((fabs(BoneScale.X - 1.0f) > 0.001f) ||
			(fabs(BoneScale.Y - 1.0f) > 0.001f) ||
			(fabs(BoneScale.Z - 1.0f) > 0.001f))
		{
#if DEBUG_RETARGET
			appPrintf("Adjust [%d] %s by scale %g %g %g\n", TrackIndex,
				*Skeleton->ReferenceSkeleton.RefBoneInfo[TrackIndex].Name, VECTOR_ARG(BoneScale));
#endif
			for (int KeyIndex = 0; KeyIndex < Track->KeyPos.Num(); KeyIndex++)
			{
				// Scale translation by accumulated bone scale value
				Track->KeyPos[KeyIndex].Scale(BoneScale);
			}
		}
	}

	unguard;
}

#endif // BAKE_BONE_SCALES

void USkeleton::ConvertAnims(UAnimSequence4* Seq)
{
	guard(USkeleton::ConvertAnims);

	CAnimSet* AnimSet = ConvertedAnim;

	if (!AnimSet)
	{
		AnimSet = new CAnimSet(this);
		ConvertedAnim = AnimSet;

		// Copy bone names
		int NumBones = ReferenceSkeleton.RefBoneInfo.Num();
		assert(BoneTree.Num() == NumBones);

		AnimSet->TrackBoneNames.Empty(NumBones);
		AnimSet->BonePositions.Empty(NumBones);
		AnimSet->BoneModes.AddZeroed(NumBones);

#if DEBUG_ANIM
		char SkelFullName[256];
		GetFullName(ARRAY_ARG(SkelFullName));
		appPrintf("------------\nSkeleton: %s\n", SkelFullName);
#endif
		for (int i = 0; i < NumBones; i++)
		{
			// Store bone name
			AnimSet->TrackBoneNames.Add(ReferenceSkeleton.RefBoneInfo[i].Name);
			// Store skeleton's bone transform
			CSkeletonBonePosition BonePosition;
			const FTransform& Transform = ReferenceSkeleton.RefBonePose[i];
			BonePosition.Position = CVT(Transform.Translation);
			BonePosition.Orientation = CVT(Transform.Rotation);
#if DEBUG_RETARGET
			if ((fabs(Transform.Scale3D.X - 1.0f) > 0.001f) ||
				(fabs(Transform.Scale3D.Y - 1.0f) > 0.001f) ||
				(fabs(Transform.Scale3D.Z - 1.0f) > 0.001f))
				appPrintf("RefPose: bone %d (%s) has scale %g %g %g\n", i, *ReferenceSkeleton.RefBoneInfo[i].Name, VECTOR_ARG(Transform.Scale3D));
#endif // DEBUG_RETARGET
			AnimSet->BonePositions.Add(BonePosition);
			// Process bone retargeting mode
			EBoneRetargetingMode BoneMode =EBoneRetargetingMode::Animation;
			switch (BoneTree[i].TranslationRetargetingMode)
			{
			case EBoneTranslationRetargetingMode::Skeleton:
				BoneMode = EBoneRetargetingMode::Mesh;
				break;
			case EBoneTranslationRetargetingMode::Animation:
				BoneMode = EBoneRetargetingMode::Animation;
				break;
			case EBoneTranslationRetargetingMode::AnimationScaled:
				BoneMode = EBoneRetargetingMode::AnimationScaled;
				break;
			case EBoneTranslationRetargetingMode::AnimationRelative:
				BoneMode = EBoneRetargetingMode::AnimationRelative;
				break;
			case EBoneTranslationRetargetingMode::OrientAndScale:
				BoneMode = EBoneRetargetingMode::OrientAndScale;
				break;
			default:
				//todo: other modes?
				BoneMode = EBoneRetargetingMode::OrientAndScale;
			}
			AnimSet->BoneModes[i] = BoneMode;
#if DEBUG_ANIM
			appPrintf("  %d: %s: (%g %g %g) mode=%d\n", i, *ReferenceSkeleton.RefBoneInfo[i].Name,
				VECTOR_ARG(ReferenceSkeleton.RefBonePose[i].Translation), BoneTree[i].TranslationRetargetingMode);
#endif
		}

#if DEBUG_ANIM
		appPrintf("  .. CAnimSet for %s has been created\n", Name);
#endif
	}

	// Check for NULL 'Seq' only after CAnimSet is created: we're doing ConvertAnims(NULL) to create an empty AnimSet
	if (!Seq)
	{
		return;
	}
#if DEBUG_ANIM
	appPrintf("Processing Skeleton %s / AnimSequence %s\n", Name, Seq->Name);
#endif

//	DBG("----------- Skeleton %s: %d seq, %d bones -----------\n", Name, Anims.Num(), ReferenceSkeleton.RefBoneInfo.Num());

	int NumTracks = Seq->GetNumTracks();

#if DEBUG_DECOMPRESS
	appPrintf("Sequence %s: %d bones, %d offsets (%g per bone), %d frames, %d compressed data\n"
		   "          trans %s, rot %s, scale %s, key %s\n",
		Seq->Name, NumTracks, Seq->CompressedTrackOffsets.Num(), Seq->CompressedTrackOffsets.Num() / (float)NumTracks,
		Seq->NumFrames, Seq->CompressedByteStream.Num(),
		EnumToName(Seq->TranslationCompressionFormat),
		EnumToName(Seq->RotationCompressionFormat),
		EnumToName(Seq->ScaleCompressionFormat),
		EnumToName(Seq->KeyEncodingFormat)
	);
	for (int i2 = 0, localTrackIndex = 0; i2 < Seq->CompressedTrackOffsets.Num(); localTrackIndex++)
	{
		// scale information
		int ScaleKeys = 0, ScaleOffset = 0;
		if (Seq->CompressedScaleOffsets.IsValid())
		{
			ScaleOffset = Seq->CompressedScaleOffsets.GetOffsetData(localTrackIndex);
		}
		// bone name
		int BoneTrackIndex = Seq->GetTrackBoneIndex(localTrackIndex);
		const char* BoneName = "(None)";
		if (BoneTrackIndex >= ReferenceSkeleton.RefBoneInfo.Num())
			BoneName = "(bad)";
		else if (BoneTrackIndex >= 0)
			BoneName = *ReferenceSkeleton.RefBoneInfo[BoneTrackIndex].Name;
		// offsets
		if (Seq->KeyEncodingFormat != AKF_PerTrackCompression)
		{
			int TransOffset = Seq->CompressedTrackOffsets[i2  ];
			int TransKeys   = Seq->CompressedTrackOffsets[i2+1];
			int RotOffset   = Seq->CompressedTrackOffsets[i2+2];
			int RotKeys     = Seq->CompressedTrackOffsets[i2+3];
			appPrintf("    [%d] = trans %d[%d] rot %d[%d] scale %d[%d] - %s\n", localTrackIndex,
				TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys, BoneName);
			i2 += 4;
		}
		else
		{
			int TransOffset = Seq->CompressedTrackOffsets[i2  ];
			int RotOffset   = Seq->CompressedTrackOffsets[i2+1];
			appPrintf("    [%d] = trans %d rot %d scale %d - %s\n", localTrackIndex, TransOffset, RotOffset, ScaleOffset, BoneName);
			i2 += 2;
		}
	}
#endif // DEBUG_DECOMPRESS

	int offsetsPerBone = 4;
	if (Seq->KeyEncodingFormat == AKF_PerTrackCompression)
		offsetsPerBone = 2;

	// Check for valid data to avoid crash if it's something wrong there
	if (Seq->CompressedTrackOffsets.Num() != NumTracks * offsetsPerBone && !Seq->RawAnimationData.Num())
	{
		appNotify("AnimSequence %s has wrong CompressedTrackOffsets size (has %d, expected %d), removing track",
			Seq->Name, Seq->CompressedTrackOffsets.Num(), NumTracks * offsetsPerBone);
		return;
	}

	// Store UAnimSequence in 'OriginalAnims' array, we just need it from time to time
	OriginalAnims.Add(Seq);

	// Create CAnimSequence
	CAnimSequence *Dst = new CAnimSequence(Seq);
	AnimSet->Sequences.Add(Dst);
	Dst->Name      = Seq->Name;
	Dst->NumFrames = Seq->NumFrames;
	Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;
	Dst->bAdditive = Seq->AdditiveAnimType != AAT_None;

	// Store information for animation retargeting.
	// Reference: UAnimSequence::GetRetargetTransforms()
	const TArray<FTransform>* RetargetTransforms = NULL;
	if (Seq->RetargetSource == "None" && Seq->RetargetSourceAssetReferencePose.Num())
	{
		// We'll use RetargetSourceAssetReferencePose as a retarget base
		RetargetTransforms = &Seq->RetargetSourceAssetReferencePose;
#if DEBUG_RETARGET
		appPrintf("  .. %s: Use RetargetSourceAssetReferencePose\n", Seq->Name);
#endif
	}
	else
	{
		// Use USkeleton pose for retarget base.
		// Reference: USkeleton::GetRefLocalPoses()
#if DEBUG_RETARGET
		appPrintf("  .. %s: Use RetargetSource '%s'\n", Name, *Seq->RetargetSource);
#endif
		if (Seq->RetargetSource != "None")
		{
			const FReferencePose* RefPose = AnimRetargetSources.Find(Seq->RetargetSource);
			// The result might be NULL if there's no RetargetSource for this animation
			if (RefPose)
			{
				RetargetTransforms = &RefPose->ReferencePose;
#if DEBUG_RETARGET
				appPrintf("  .. Found RefPose for '%s'\n", *Seq->RetargetSource);
#endif
			}
		}
		if (!RetargetTransforms)
		{
			// Animation will use ReferenceSkeleton for retargeting, we've already copied the
			// information into CAnimSet::BonePositions array/
		}
	}

	if (RetargetTransforms)
	{
		//todo: Solve this: RetargetTransforms size may not match ReferenceSkeleton and sequence's track count.
		//todo: UE4 does some remapping "track to skeleton bone index map". Without assertion things works, seems
		//todo: because RetargetTransforms array is smaller (or of the same size).
		//assert(RetargetTransforms->Num() == ReferenceSkeleton.RefBoneInfo.Num());
		Dst->RetargetBasePose.Empty(RetargetTransforms->Num());
		for (const FTransform& BoneTransform : *RetargetTransforms)
		{
			CSkeletonBonePosition BonePosition;
			BonePosition.Position = CVT(BoneTransform.Translation);
			BonePosition.Orientation = CVT(BoneTransform.Rotation);
			Dst->RetargetBasePose.Add(BonePosition);
#if DEBUG_RETARGET
			if (BoneTransform.Scale3D.X != 1.0f || BoneTransform.Scale3D.Y != 1.0f || BoneTransform.Scale3D.Z != 1.0f)
				appPrintf("Retarget: bone %d (%s) has scale %g %g %g\n", &BoneTransform - RetargetTransforms->GetData(),
					*AnimSet->TrackBoneNames[&BoneTransform - RetargetTransforms->GetData()], VECTOR_ARG(BoneTransform.Scale3D));
#endif // DEBUG_RETARGET
		}
	}

	// bone tracks ...
	Dst->Tracks.Empty(NumTracks);

	// There could be an animation consisting of only trans with offsets == -1, what means
	// use of RefPose. In this case there's no point adding the animation to AnimSet. We'll
	// create FMemReader even for empty CompressedByteStream, otherwise it would be hard to
	// create a valid CAnimSequence which won't crash animation export.
	FMemReader Reader(
		Seq->CompressedByteStream.Num() ? Seq->CompressedByteStream.GetData() : (const uint8*)"",
		Seq->CompressedByteStream.Num());
	Reader.SetupFrom(*Package);

	bool HasTimeTracks = (Seq->KeyEncodingFormat == AKF_VariableKeyLerp);

	for (int BoneIndex = 0; BoneIndex < ReferenceSkeleton.RefBoneInfo.Num(); BoneIndex++)
	{
		CAnimTrack *A = new CAnimTrack;
		Dst->Tracks.Add(A);

		int TrackIndex = Seq->FindTrackForBoneIndex(BoneIndex);

		if (TrackIndex < 0)
		{
			// This bone is not animated with this UAnimSequence (but it may be animated with other
			// ones which shares the same USkeleton). Just use an empty track, it should be properly
			// handled by our animation system.
			continue;
		}

		if (!Seq->CompressedTrackOffsets.Num())	//?? or if RawAnimData.Num() != 0
		{
			// using RawAnimData array
			assert(Seq->RawAnimationData.Num() == NumTracks);
			CopyArray(A->KeyPos,  CVT(Seq->RawAnimationData[TrackIndex].PosKeys));
			CopyArray(A->KeyQuat, CVT(Seq->RawAnimationData[TrackIndex].RotKeys));
			CopyArray(A->KeyTime, Seq->RawAnimationData[TrackIndex].KeyTimes);	// may be empty
			for (int k = 0; k < A->KeyTime.Num(); k++)
				A->KeyTime[k] *= Dst->Rate;
			continue;
		}

		static const CVec3 nullVec  = { 0, 0, 0 };
		static const CQuat nullQuat = { 0, 0, 0, 1 };

		int offsetIndex = TrackIndex * offsetsPerBone;

		//----------------------------------------------
		// decode AKF_PerTrackCompression data
		//----------------------------------------------
		if (Seq->KeyEncodingFormat == AKF_PerTrackCompression)
		{
			// this format uses different key storage
			guard(PerTrackCompression);
			assert(Seq->TranslationCompressionFormat == ACF_Identity);
			assert(Seq->RotationCompressionFormat == ACF_Identity);

			int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
			int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+1];
			int ScaleOffset = Seq->CompressedScaleOffsets.IsValid() ? Seq->CompressedScaleOffsets.OffsetData[TrackIndex] : -1;
#if 0
			const int BytesToDump = 64;
			if (TransOffset >= 0) { Reader.Seek(TransOffset); DUMP_ARC_BYTES(Reader, BytesToDump, "Trans"); }
			if (RotOffset >= 0) { Reader.Seek(RotOffset); DUMP_ARC_BYTES(Reader, BytesToDump,  "Rot"); }
			if (ScaleOffset >= 0) { Reader.Seek(ScaleOffset); DUMP_ARC_BYTES(Reader, BytesToDump, "Scale"); }
#endif

			guard(TransKeys);
			// read translation keys
			if (TransOffset == -1)
			{
				A->KeyPos.Add(nullVec);
				DBG("    [%d] no translation data\n", TrackIndex);
			}
			else
			{
				Reader.Seek(TransOffset);
				ReadPerTrackVectorData(Reader, TrackIndex, "translation", A->KeyPos, A->KeyPosTime, Seq->NumFrames);
			}
			unguard;

			guard(RotKeys);
			// read rotation keys
			if (RotOffset == -1)
			{
				A->KeyQuat.Add(nullQuat);
				DBG("    [%d] no rotation data\n", TrackIndex);
			}
			else
			{
				Reader.Seek(RotOffset);
				ReadPerTrackQuatData(Reader, TrackIndex, "rotation", A->KeyQuat, A->KeyQuatTime, Seq->NumFrames);
			}
			unguard;

#if SUPPORT_SCALE_KEYS
			guard(ScaleKeys);
			// read scale keys
			if (ScaleOffset == -1)
			{
				DBG("    [%d] no scale data\n", TrackIndex);
			}
			else
			{
				Reader.Seek(ScaleOffset);
				ReadPerTrackVectorData(Reader, TrackIndex, "scale", A->KeyScale, A->KeyScaleTime, Seq->NumFrames);
			}
			unguard;
#endif // SUPPORT_SCALE_KEYS

			unguard;

			continue;
			// end of AKF_PerTrackCompression block ...
		}

		//----------------------------------------------
		// end of AKF_PerTrackCompression decoder
		//----------------------------------------------

		// read animations
		int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
		int TransKeys   = Seq->CompressedTrackOffsets[offsetIndex+1];
		int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+2];
		int RotKeys     = Seq->CompressedTrackOffsets[offsetIndex+3];
//		appPrintf("[%d:%d:%d] :  %d[%d]  %d[%d]  %d[%d]\n", j, Seq->RotationCompressionFormat, Seq->TranslationCompressionFormat, TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys);

		A->KeyPos.Empty(TransKeys);
		A->KeyQuat.Empty(RotKeys);

		FVector Mins, Ranges;

		// read translation keys
		if (TransKeys)
		{
			Reader.Seek(TransOffset);
			AnimationCompressionFormat TranslationCompressionFormat = Seq->TranslationCompressionFormat;
			if (TransKeys == 1)
				TranslationCompressionFormat = ACF_None;	// single key is stored without compression
			// read mins/ranges
			if (TranslationCompressionFormat == ACF_IntervalFixed32NoW)
			{
				Reader << Mins << Ranges;
			}

			for (int k = 0; k < TransKeys; k++)
			{
				switch (TranslationCompressionFormat)
				{
				TP (ACF_None,               FVector)
				TP (ACF_Float96NoW,         FVector)
				TPR(ACF_IntervalFixed32NoW, FVectorIntervalFixed32, A->KeyPos)
				TP (ACF_Fixed48NoW,         FVectorFixed48)
				case ACF_Identity:
					A->KeyPos.Add(nullVec);
					break;
				default:
					appError("Unknown translation compression method: %d (%s)", TranslationCompressionFormat, EnumToName(TranslationCompressionFormat));
				}
			}

			// align to 4 bytes
			Reader.Seek(Align(Reader.Tell(), 4));
			if (HasTimeTracks)
				ReadTimeArray(Reader, TransKeys, A->KeyPosTime, Seq->NumFrames);
		}
		else
		{
//			A->KeyPos.Add(nullVec);
//			appNotify("No translation keys!");
		}

#if DEBUG_DECOMPRESS
		int TransEnd = Reader.Tell();
#endif
		// read rotation keys
		Reader.Seek(RotOffset);
		AnimationCompressionFormat RotationCompressionFormat = Seq->RotationCompressionFormat;

		if (RotKeys == 1)
		{
			RotationCompressionFormat = ACF_Float96NoW;	// single key is stored without compression
		}
		else if (RotKeys > 1 && RotationCompressionFormat == ACF_IntervalFixed32NoW)
		{
			// Mins/Ranges are read only when needed - i.e. for ACF_IntervalFixed32NoW
			Reader << Mins << Ranges;
		}

		for (int k = 0; k < RotKeys; k++)
		{
			switch (RotationCompressionFormat)
			{
			TR (ACF_None, FQuat, A->KeyQuat)
			TR (ACF_Float96NoW, FQuatFloat96NoW, A->KeyQuat)
			TR (ACF_Fixed48NoW, FQuatFixed48NoW, A->KeyQuat)
			TR (ACF_Fixed32NoW, FQuatFixed32NoW, A->KeyQuat)
			TRR(ACF_IntervalFixed32NoW, FQuatIntervalFixed32NoW, A->KeyQuat)
			TR (ACF_Float32NoW, FQuatFloat32NoW, A->KeyQuat)
			case ACF_Identity:
				A->KeyQuat.Add(nullQuat);
				break;
			default:
				appError("Unknown rotation compression method: %d (%s)", RotationCompressionFormat, EnumToName(RotationCompressionFormat));
			}
		}

		if (HasTimeTracks)
		{
			// align to 4 bytes
			Reader.Seek(Align(Reader.Tell(), 4));
			ReadTimeArray(Reader, RotKeys, A->KeyQuatTime, Seq->NumFrames);
		}

#if DEBUG_DECOMPRESS
//		appPrintf("[%s : %s] Frames=%d KeyPos.Num=%d KeyQuat.Num=%d KeyFmt=%s\n", *Seq->SequenceName, *TrackBoneNames[j],
//			Seq->NumFrames, A->KeyPos.Num(), A->KeyQuat.Num(), *Seq->KeyEncodingFormat);
		appPrintf("  -> [%d]: t %d .. %d + r %d .. %d (%d/%d keys)\n", TrackIndex,
			TransOffset, TransEnd, RotOffset, Reader.Tell(), TransKeys, RotKeys);
#endif // DEBUG_DECOMPRESS
	}

	// Now should invert all imported rotations
	FixRotationKeys(Dst);

#if BAKE_BONE_SCALES
	// And apply scales to positions, when skeleton has any
	AdjustSequenceBySkeleton(this, RetargetTransforms ? *RetargetTransforms : ReferenceSkeleton.RefBonePose, Dst);
#endif

	unguardf("Skel=%s Anim=%s", Name, Seq->Name);
}


/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

// PostSerialize() adds some serialization logic to data which were serialized as properties. These functions
// were called in UE4.12 and earlier as "Serialize" and in 4.13 renamed to "PostSerialize".

void FAnimCurveBase::PostSerialize(FArchive& Ar)
{
	if (FFrameworkObjectVersion::Get(Ar) < FFrameworkObjectVersion::SmartNameRefactor)
	{
		if (Ar.ArVer >= VER_UE4_SKELETON_ADD_SMARTNAMES)
		{
			uint16 CurveUid;			// SmartName::UID_Type == uint16 (SmartName is a namespace)
			Ar << CurveUid;
		}
	}
}

void FRawCurveTracks::PostSerialize(FArchive& Ar)
{
	for (int i = 0; i < FloatCurves.Num(); i++)
	{
		FloatCurves[i].PostSerialize(Ar);
	}
	//!! non-cooked assets also may have TransformCurves
}

 FArchive& operator<<(FArchive& Ar, FRawCurveTracks& T)
{
	guard(FRawCurveTracks<<);
	// This structure is always serialized as property list
	FRawCurveTracks::StaticGetTypeinfo()->SerializeUnrealProps(Ar, &T);
	if (Ar.Game < GAME_UE4(13))
	{
		T.PostSerialize(Ar);
	}
	return Ar;
	unguard;
}

void UAnimSequenceBase::Serialize(FArchive& Ar)
{
	guard(UAnimSequenceBase::Serialize);

	Super::Serialize(Ar);
	RawCurveData.PostSerialize(Ar);		// useful only between VER_UE4_SKELETON_ADD_SMARTNAMES and FFrameworkObjectVersion::SmartNameRefactor

	unguard;
}

void UAnimSequence4::Serialize(FArchive& Ar)
{
	guard(UAnimSequence4::Serialize);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);
	if (!StripFlags.IsEditorDataStripped())
	{
		Ar << RawAnimationData;
		if (Ar.ArVer >= VER_UE4_ANIMATION_ADD_TRACKCURVES)
		{
			TArray<FRawAnimSequenceTrack> SourceRawAnimationData;
			Ar << SourceRawAnimationData;
		}
	}

	if (FFrameworkObjectVersion::Get(Ar) < FFrameworkObjectVersion::MoveCompressedAnimDataToTheDDC)
	{
		// Part of data were serialized as properties
		Ar << CompressedByteStream;
#if SEAOFTHIEVES
		if ((Ar.Game == GAME_SeaOfThieves) && (CompressedByteStream.Num() == 1) && (Ar.GetStopper() - Ar.Tell() > 0))
		{
			// Sea of Thieves has extra int32 == 1 before the CompressedByteStream
			Ar.Seek(Ar.Tell() - 1);
			Ar << CompressedByteStream;
		}
#endif // SEAOFTHIEVES

		// Fix layout of "byte swapped" data (workaround for UE4 bug)
		if (KeyEncodingFormat == AKF_PerTrackCompression && CompressedScaleOffsets.OffsetData.Num())
		{
			TArray<uint8> SwappedData;
			TransferPerTrackData(SwappedData, CompressedByteStream);
			Exchange(SwappedData, CompressedByteStream);
		}
	}
	else
	{
		// UE4.12+
		bool bSerializeCompressedData;
		Ar << bSerializeCompressedData;

		if (bSerializeCompressedData)
		{
			if (Ar.Game < GAME_UE4(23))
				SerializeCompressedData(Ar);
			else if (Ar.Game < GAME_UE4(25))
				SerializeCompressedData2(Ar);
			else
				SerializeCompressedData3(Ar);

			Ar << bUseRawDataOnly;
		}
	}

	unguard;
}

void UAnimSequence4::SerializeCompressedData(FArchive& Ar)
{
	guard(UAnimSequence4::SerializeCompressedData);

	// These fields were serialized as properties in pre-UE4.12 engine version
	Ar << (byte&)KeyEncodingFormat;
	Ar << (byte&)TranslationCompressionFormat;
	Ar << (byte&)RotationCompressionFormat;
	Ar << (byte&)ScaleCompressionFormat;
#if DEBUG_ANIM
	appPrintf("Key: %d (%s) Trans: %d (%s) Rot: %d (%s) Scale: %d (%s)\n",
		KeyEncodingFormat, EnumToName(KeyEncodingFormat),
		TranslationCompressionFormat, EnumToName(TranslationCompressionFormat),
		RotationCompressionFormat, EnumToName(RotationCompressionFormat),
		ScaleCompressionFormat, EnumToName(ScaleCompressionFormat));
#endif

	Ar << CompressedTrackOffsets;
	Ar << CompressedScaleOffsets;
#if DEBUG_ANIM
	appPrintf("TrackOffsets: %d ScaleOffsets: %d\n", CompressedTrackOffsets.Num(), CompressedScaleOffsets.OffsetData.Num());
#endif

	if (Ar.Game >= GAME_UE4(21))
	{
		// UE4.21+ - added compressed segments; disappeared in 4.23
		Ar << CompressedSegments;
		if (CompressedSegments.Num())
		{
			appNotify("animation has CompressedSegments!");
		}
	}

	Ar << CompressedTrackToSkeletonMapTable;

	if (Ar.Game < GAME_UE4(22))
	{
		Ar << CompressedCurveData;
	}
	else
	{
		TArray<FSmartName> CompressedCurveNames;
		Ar << CompressedCurveNames;
	}

#if LIS2
	if (Ar.Game == GAME_LIS2)
	{
		// Here we'll have either CompressedByteStream, or some 8 byte value. Let's check what's next.
		// With newer Life Is Strange 2, it seems developers moved CompressedTrackOffsets into CompressedByteStream,
		// with doing some special encoding at the beginning of data stream. This is the version when byte stream
		// is prepended with 8-byte value. Also, 1st 4 bytes in byte stream repeats array size.
		int32 Count;
		Ar << Count;
		if (Count != Ar.GetStopper() - Ar.Tell() - 4)
			Ar.Seek(Ar.Tell() + 4);
		else
			Ar.Seek(Ar.Tell() - 4);
		goto no_raw_data_size; // this is basically UE4.17, but with older animation format
	}
#endif // LIS2
	if (Ar.Game >= GAME_UE4(17))
	{
		// UE4.17+
		int32 CompressedRawDataSize;
		Ar << CompressedRawDataSize;
	}
no_raw_data_size:
	if (Ar.Game >= GAME_UE4(22))
	{
		int32 CompressedNumFrames;
		Ar << CompressedNumFrames;
	}

	// compressed data
	int32 NumBytes;
	Ar << NumBytes;

	if (NumBytes)
	{
		CompressedByteStream.AddUninitialized(NumBytes);
		Ar.Serialize(CompressedByteStream.GetData(), NumBytes);
	}

	if (Ar.Game >= GAME_UE4(22))
	{
		FString CurveCodecPath;
		TArray<byte> CompressedCurveByteStream;
		Ar << CurveCodecPath << CompressedCurveByteStream;
	}

	// Fix layout of "byte swapped" data (workaround for UE4 bug)
	if (KeyEncodingFormat == AKF_PerTrackCompression && CompressedScaleOffsets.OffsetData.Num() && Ar.Game < GAME_UE4(23))
	{
		TArray<uint8> SwappedData;
		TransferPerTrackData(SwappedData, CompressedByteStream);
		Exchange(SwappedData, CompressedByteStream);
	}

	unguard;
}

// UE4.23-4.24 has changed compressed data layout for streaming, so it's worth making a separate
// serializer function for it.
void UAnimSequence4::SerializeCompressedData2(FArchive& Ar)
{
	guard(UAnimSequence4::SerializeCompressedData2);

	int32 CompressedRawDataSize;
	Ar << CompressedRawDataSize;

	Ar << CompressedTrackToSkeletonMapTable;

	TArray<FSmartName> CompressedCurveNames;
	Ar << CompressedCurveNames;

#if DEBUG_ANIM
	appPrintf("TrackToSkeleton: %d, CurveNames: %d\n", CompressedTrackToSkeletonMapTable.Num(), CompressedCurveNames.Num());
#endif

	// Since 4.23, this is FUECompressedAnimData::SerializeCompressedData

	Ar << (byte&)KeyEncodingFormat;
	Ar << (byte&)TranslationCompressionFormat;
	Ar << (byte&)RotationCompressionFormat;
	Ar << (byte&)ScaleCompressionFormat;
#if DEBUG_ANIM
	appPrintf("Key: %d (%s) Trans: %d (%s) Rot: %d (%s) Scale: %d (%s)\n",
		KeyEncodingFormat, EnumToName(KeyEncodingFormat),
		TranslationCompressionFormat, EnumToName(TranslationCompressionFormat),
		RotationCompressionFormat, EnumToName(RotationCompressionFormat),
		ScaleCompressionFormat, EnumToName(ScaleCompressionFormat));
#endif

	int32 CompressedNumFrames;
	Ar << CompressedNumFrames;

	// SerializeView() just serializes array size
	int32 CompressedTrackOffsets_Num, CompressedScaleOffsets_Num, CompressedByteStream_Num;
	Ar << CompressedTrackOffsets_Num;
	Ar << CompressedScaleOffsets_Num;
	Ar << CompressedScaleOffsets.StripSize;
	Ar << CompressedByteStream_Num;
	// ... end of FUECompressedAnimData::SerializeCompressedData

#if DEBUG_ANIM
	appPrintf("NumFrames: %d, TrackOffsets: %d, ScaleOffsets: %d, ByteStream: %d\n", CompressedNumFrames, CompressedTrackOffsets_Num, CompressedScaleOffsets_Num, CompressedByteStream_Num);
#endif

	int32 NumBytes;
	Ar << NumBytes;

	bool bUseBulkDataForLoad = false;
	Ar << bUseBulkDataForLoad;

	// In UE4.23 CompressedByteStream field exists in FUECompressedAnimData (as TArrayView) and in
	// FCompressedAnimSequence (as byte array). Serialization is done in FCompressedAnimSequence,
	// either as TArray or as bulk, and then array is separated onto multiple "views" for
	// FUECompressedAnimData. We'll use a different name for "joined" serialized array here to
	// avoid confuse.
	TArray<byte> SerializedByteStream;

	if (bUseBulkDataForLoad)
	{
		appError("Anim: bUseBulkDataForLoad not implemented");
		//todo: read from bulk to SerializedByteStream
	}
	else
	{
		if (NumBytes)
		{
			SerializedByteStream.AddUninitialized(NumBytes);
			Ar.Serialize(SerializedByteStream.GetData(), NumBytes);
		}
	}

	// Setup all array views from single array. In UE4 this is done in FUECompressedAnimData::InitViewsFromBuffer.
	// We'll simply copy array data away from SerializedByteStream, and then SerializedByteStream
	// will be released from memory as it is a local variable here.
	// Note: copying is not byte-order wise, so if there will be any problems in the future,
	// should use byte swap functions.
	const byte* AnimData = SerializedByteStream.GetData();
	const byte* AnimDataEnd = AnimData + SerializedByteStream.Num();

#define MAP_VIEW(Array, Size)			\
	Array.AddUninitialized(Size);		\
	memcpy((void*)Array.GetData(), AnimData, Size * Array.GetTypeSize()); \
	AnimData += Size * Array.GetTypeSize();

	MAP_VIEW(CompressedTrackOffsets, CompressedTrackOffsets_Num);
	MAP_VIEW(CompressedScaleOffsets.OffsetData, CompressedScaleOffsets_Num);
	MAP_VIEW(CompressedByteStream, CompressedByteStream_Num);
	assert(AnimData == AnimDataEnd);

	FString CurveCodecPath;
	TArray<byte> CompressedCurveByteStream;
	Ar << CurveCodecPath << CompressedCurveByteStream;

#if DEBUG_ANIM
	appPrintf("CurveCodec: %s, CurveByteStream: %d\n", *CurveCodecPath, CompressedCurveByteStream.Num());
#endif

	unguard;
}

// UE4.25 has changed data layout, and therefore serialization order has been changed too.
// In UE4.25 serialization is done in FCompressedAnimSequence::SerializeCompressedData().
void UAnimSequence4::SerializeCompressedData3(FArchive& Ar)
{
	guard(UAnimSequence4::SerializeCompressedData3);

	int32 CompressedRawDataSize;
	Ar << CompressedRawDataSize;

	Ar << CompressedTrackToSkeletonMapTable;

	TArray<FSmartName> CompressedCurveNames;
	Ar << CompressedCurveNames;

	int32 NumBytes;
	Ar << NumBytes;

	bool bUseBulkDataForLoad;
	Ar << bUseBulkDataForLoad;

	// In UE4.23 CompressedByteStream field exists in FUECompressedAnimData (as TArrayView) and in
	// FCompressedAnimSequence (as byte array). Serialization is done in FCompressedAnimSequence,
	// either as TArray or as bulk, and then array is separated onto multiple "views" for
	// FUECompressedAnimData. We'll use a different name for "joined" serialized array here to
	// avoid confuse.
	TArray<byte> SerializedByteStream;

	if (bUseBulkDataForLoad)
	{
		appError("Anim: bUseBulkDataForLoad not implemented");
		//todo: read from bulk to SerializedByteStream
	}
	else
	{
		if (NumBytes)
		{
			SerializedByteStream.AddUninitialized(NumBytes);
			Ar.Serialize(SerializedByteStream.GetData(), NumBytes);
		}
	}

	FString BoneCodecDDCHandle, CurveCodecPath;
	Ar << BoneCodecDDCHandle << CurveCodecPath;
#if DEBUG_ANIM
	appPrintf("BoneCodec (%s) CurveCodec (%s)\n", *BoneCodecDDCHandle, *CurveCodecPath);
#endif

	TArray<byte> CompressedCurveByteStream;
	Ar << CompressedCurveByteStream;

	if (!BoneCodecDDCHandle.IsEmpty())
	{
		// The following part is ICompressedAnimData::SerializeCompressedData
		int32 CompressedNumFrames;
		Ar << CompressedNumFrames;
		// todo: editor-only data here

		// FUECompressedAnimData::SerializeCompressedData
		Ar << (byte&)KeyEncodingFormat;
		Ar << (byte&)TranslationCompressionFormat;
		Ar << (byte&)RotationCompressionFormat;
		Ar << (byte&)ScaleCompressionFormat;

		// SerializeView() just serializes array size
		int32 CompressedTrackOffsets_Num, CompressedScaleOffsets_Num, CompressedByteStream_Num;
		Ar << CompressedByteStream_Num;
		Ar << CompressedTrackOffsets_Num;
		Ar << CompressedScaleOffsets_Num;
		Ar << CompressedScaleOffsets.StripSize;

		// Setup all array views from single array. In UE4 this is done in FUECompressedAnimData::InitViewsFromBuffer.
		// We'll simply copy array data away from SerializedByteStream, and then SerializedByteStream
		// will be released from memory as it is a local variable here.
		// Note: copying is not byte-order wise, so if there will be any problems in the future,
		// should use byte swap functions.
		const byte* AnimData = SerializedByteStream.GetData();
		const byte* AnimDataEnd = AnimData + SerializedByteStream.Num();

		MAP_VIEW(CompressedTrackOffsets, CompressedTrackOffsets_Num);
		MAP_VIEW(CompressedScaleOffsets.OffsetData, CompressedScaleOffsets_Num);
		MAP_VIEW(CompressedByteStream, CompressedByteStream_Num);
		assert(AnimData == AnimDataEnd);
	}

#undef MAP_VIEW

	unguard;
}

// UE4 has some mess in AEFPerTrackCompressionCodec::ByteSwapOut() (and ByteSwapIn): it sends
// data in order: translation data, rotation data, scale data. However, scale data stored in
// CompressedByteStream before translation and rotation. In other words, data reordered, but
// offsets pointed to original data. Here we're reordering data back, duplicating functionality
// of AEFPerTrackCompressionCodec::ByteSwapOut().
// The bug was reported to Epic at UDN: https://udn.unrealengine.com/questions/488635/view.html
// and it seems it was already fixed for UE4.23.
void UAnimSequence4::TransferPerTrackData(TArray<uint8>& Dst, const TArray<uint8>& Src)
{
	guard(UAnimSequence4::TransferPerTrackData);

	Dst.AddZeroed(Src.Num());

	int NumTracks = CompressedTrackOffsets.Num() / 2;

	const uint8* SrcData = Src.GetData();

	for (int TrackIndex = 0; TrackIndex < NumTracks; TrackIndex++)
	{
		for (int Kind = 0; Kind < 3; Kind++)
		{
			// Get track offset
			int Offset = 0;
			switch (Kind)
			{
			case 0: // translation data
				Offset = CompressedTrackOffsets[TrackIndex * 2];
				break;
			case 1: // rotation data
				Offset = CompressedTrackOffsets[TrackIndex * 2 + 1];
				break;
			default: // case 2 - scale data
				Offset = CompressedScaleOffsets.GetOffsetData(TrackIndex);
			}
			if (Offset == INDEX_NONE)
			{
				continue;
			}

			uint8* DstData = Dst.GetData() + Offset;

			// Copy data

	#define COPY(size) \
			{ \
				int ct = size; /* avoid macro expansion troubles */ \
				memcpy(DstData, SrcData, size); \
				SrcData += ct; \
				DstData += ct; \
			}

			// Decode animation header
			uint32 PackedInfo;
			PackedInfo = *(uint32*)SrcData;
			COPY(sizeof(uint32));

			AnimationCompressionFormat KeyFormat;
			int ComponentMask;
			int NumKeys;
			bool HasTimeTracks;
			DECODE_PER_TRACK_INFO(PackedInfo);

			static const int NumComponentsPerMask[8] = { 3, 1, 1, 2, 1, 2, 2, 3 }; // number of identity bits in value, 0 == all bits
			int NumComponents = NumComponentsPerMask[ComponentMask & 7];

			// mins/randes
			if (KeyFormat == ACF_IntervalFixed32NoW)
			{
				COPY(sizeof(float) * NumComponents * 2);
			}

			// keys
			switch (KeyFormat)
			{
			case ACF_Float96NoW:
				COPY(sizeof(float) * NumComponents * NumKeys);
				break;
			case ACF_Fixed48NoW:
				COPY(sizeof(uint16) * NumComponents * NumKeys);
				break;
			case ACF_IntervalFixed32NoW:
			case ACF_Fixed32NoW:
			case ACF_Float32NoW:
				COPY(sizeof(uint32) * NumKeys); // always stored full data, ComponentMask used only for mins/ranges
				break;
			case ACF_Identity:
				// nothing
				break;
			}

			// time data
			if (HasTimeTracks)
			{
				// padding
				int CurrentOffset = DstData - Dst.GetData();
				int AlignedOffset = Align(CurrentOffset, 4);
				if (AlignedOffset != CurrentOffset)
				{
					COPY(AlignedOffset - CurrentOffset);
				}
				// copy time
				COPY((NumFrames < 256 ? sizeof(uint8) : sizeof(uint16)) * NumKeys);
			}

			// align to 4 bytes
			int CurrentOffset = DstData - Dst.GetData();
			int AlignedOffset = Align(CurrentOffset, 4);
			assert(AlignedOffset <= Dst.Num());
			if (AlignedOffset != CurrentOffset)
			{
				COPY(AlignedOffset - CurrentOffset);
			}
		}
	}

	unguard;
}

void UAnimSequence4::PostLoad()
{
	guard(UAnimSequence4::PostLoad);
	if (!Skeleton)
	{
		// missing package etc
		appPrintf("WARNING: unable to load animation %s, missing Skeleton\n", Name);
		return;
	}

#if BAKE_BONE_SCALES
	// Scale RetargetSourceAssetReferencePose
	if (RetargetSourceAssetReferencePose.Num())
	{
		AdjustBoneScales(Skeleton->ReferenceSkeleton, RetargetSourceAssetReferencePose);
	}
#endif // BAKE_BONE_SCALES

	Skeleton->ConvertAnims(this);

	// Release original animation data to save memory
	RawAnimationData.Empty();
	CompressedByteStream.Empty();
	CompressedTrackOffsets.Empty();

	unguard;
}

// WARNING: the following functions uses some logic to use either CompressedTrackToSkeletonMapTable or TrackToSkeletonMapTable.
// This logic should be the same everywhere. Note: CompressedTrackToSkeletonMapTable appeared in UE4.12, so it will always be
// empty when loading animations from older engines.

int UAnimSequence4::GetNumTracks() const
{
	return CompressedTrackToSkeletonMapTable.Num() ? CompressedTrackToSkeletonMapTable.Num() : TrackToSkeletonMapTable.Num();
}

int UAnimSequence4::GetTrackBoneIndex(int TrackIndex) const
{
	if (CompressedTrackToSkeletonMapTable.Num())
		return CompressedTrackToSkeletonMapTable[TrackIndex].BoneTreeIndex;
	else
		return TrackToSkeletonMapTable[TrackIndex].BoneTreeIndex;
}

int UAnimSequence4::FindTrackForBoneIndex(int BoneIndex) const
{
	const TArray<FTrackToSkeletonMap>& TrackMap = CompressedTrackToSkeletonMapTable.Num() ? CompressedTrackToSkeletonMapTable : TrackToSkeletonMapTable;
	for (int TrackIndex = 0; TrackIndex < TrackMap.Num(); TrackIndex++)
	{
		if (TrackMap[TrackIndex].BoneTreeIndex == BoneIndex)
			return TrackIndex;
	}
	return INDEX_NONE;
}

#endif // UNREAL4
