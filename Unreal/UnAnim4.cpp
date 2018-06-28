#include "Core.h"

#if UNREAL4

#include "UnrealClasses.h"
#include "UnMesh4.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"

#include "SkeletalMesh.h"
#include "TypeConvert.h"

//#define DEBUG_DECOMPRESS	1

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
	}
	else
	{
		appPrintf("USkeleton has old AnimRetargetSources format, skipping\n");
		DROP_REMAINING_DATA(Ar);
		return;
	}

	if (Ar.ArVer >= VER_UE4_SKELETON_GUID_SERIALIZATION)
		Ar << Guid;

	guard(SmartNames);
	if (Ar.ArVer >= VER_UE4_SKELETON_ADD_SMARTNAMES)
		Ar << SmartNames;
	unguard;

	unguard;
}


void USkeleton::PostLoad()
{
	guard(USkeleton::PostLoad);

	// Create empty AnimSet if not yet created
	ConvertAnims(NULL);

/*
	// Gather all loaded UAnimSequence objects referencing this skeleton
	const TArray<UnPackage*>& PackageMap = UnPackage::GetPackageMap();

	for (int i = 0; i < PackageMap.Num(); i++)
	{
		UnPackage* p = PackageMap[i];
		for (int j = 0; j < p->Summary.ExportCount; j++)
		{
			FObjectExport& Exp = p->ExportTable[j];
			if (Exp.Object && Exp.Object->IsA("AnimSequence4"))
			{
				UAnimSequence4* Seq = (UAnimSequence4*)Exp.Object;
				if (Seq->Skeleton == this)
				{
					Anims.Add(Seq);
				}
			}
		}
	}

	ConvertAnims();
*/
	unguard;
}


#if DEBUG_DECOMPRESS
#define DBG(...)			appPrintf(__VA_ARGS__)
#else
#define DBG(...)
#endif

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
#define TPR(Enum, VecType)						\
				case Enum:						\
					{							\
						VecType v;				\
						Reader << v;			\
						FVector v2 = v.ToVector(Mins, Ranges); \
						A->KeyPos.Add(CVT(v2));	\
					}							\
					break;
// rotation
#define TR(Enum, QuatType)						\
				case Enum:						\
					{							\
						QuatType q;				\
						Reader << q;			\
						A->KeyQuat.Add(CVT(q));	\
					}							\
					break;
// rotation ranged
#define TRR(Enum, QuatType)						\
				case Enum:						\
					{							\
						QuatType q;				\
						Reader << q;			\
						FQuat q2 = q.ToQuat(Mins, Ranges); \
						A->KeyQuat.Add(CVT(q2));\
					}							\
					break;

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

static void FixRotationKeys(CAnimSequence* Anim)
{
	for (int TrackIndex = 0; TrackIndex < Anim->Tracks.Num(); TrackIndex++)
	{
		if (TrackIndex == 0) continue;	// don't fix root track
		CAnimTrack* Track = Anim->Tracks[TrackIndex];
		for (int KeyIndex = 0; KeyIndex < Track->KeyQuat.Num(); KeyIndex++)
		{
			Track->KeyQuat[KeyIndex].Conjugate();
		}
	}
}

void USkeleton::ConvertAnims(UAnimSequence4* Seq)
{
	guard(USkeleton::ConvertAnims);

	CAnimSet* AnimSet = ConvertedAnim;

	if (!AnimSet)
	{
		AnimSet = new CAnimSet(this);
		ConvertedAnim = AnimSet;

		// Copy bone names
		AnimSet->TrackBoneNames.Empty(ReferenceSkeleton.RefBoneInfo.Num());
		for (int i = 0; i < ReferenceSkeleton.RefBoneInfo.Num(); i++)
		{
			AnimSet->TrackBoneNames.Add(ReferenceSkeleton.RefBoneInfo[i].Name);
		}

		//TODO: verify if UE4 has AnimRotationOnly stuff
		AnimSet->AnimRotationOnly = false;
	}

	if (!Seq) return; // allow calling ConvertAnims(NULL) to create empty AnimSet

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
		#if 0 //?? disabled at 11.04.2017: it takes strip 1 if strip size > 1 - not sure why
			int ScaleStripSize = Seq->CompressedScaleOffsets.StripSize;
			ScaleOffset = Seq->CompressedScaleOffsets.OffsetData[localTrackIndex * ScaleStripSize];
			if (ScaleStripSize > 1)
				ScaleKeys = Seq->CompressedScaleOffsets.OffsetData[localTrackIndex * ScaleStripSize + 1];
		#else
			ScaleKeys = Seq->CompressedScaleOffsets.GetOffsetData(localTrackIndex);
		#endif
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

	// some checks
	int offsetsPerBone = 4;
	if (Seq->KeyEncodingFormat == AKF_PerTrackCompression)
		offsetsPerBone = 2;

	if (Seq->CompressedTrackOffsets.Num() != NumTracks * offsetsPerBone && !Seq->RawAnimationData.Num())
	{
		appNotify("AnimSequence %s has wrong CompressedTrackOffsets size (has %d, expected %d), removing track",
			Seq->Name, Seq->CompressedTrackOffsets.Num(), NumTracks * offsetsPerBone);
		return;
	}

	// create CAnimSequence
	CAnimSequence *Dst = new CAnimSequence;
	AnimSet->Sequences.Add(Dst);
	Dst->Name      = Seq->Name;
	Dst->NumFrames = Seq->NumFrames;
	Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;

	// bone tracks ...
	Dst->Tracks.Empty(NumTracks);

	FMemReader Reader(Seq->CompressedByteStream.GetData(), Seq->CompressedByteStream.Num());
	Reader.SetupFrom(*Package);

	bool HasTimeTracks = (Seq->KeyEncodingFormat == AKF_VariableKeyLerp);

	for (int BoneIndex = 0; BoneIndex < ReferenceSkeleton.RefBoneInfo.Num(); BoneIndex++)
	{
		CAnimTrack *A = new CAnimTrack;
		Dst->Tracks.Add(A);

		int TrackIndex = Seq->FindTrackForBoneIndex(BoneIndex);

		if (TrackIndex < 0)
		{
			// this track has no animation, use static pose from ReferenceSkeleton
			const FTransform& RefPose = ReferenceSkeleton.RefBonePose[BoneIndex];
			A->KeyPos.Add(CVT(RefPose.Translation));
			A->KeyQuat.Add(CVT(RefPose.Rotation));
			//!! RefPose.Scale3D
			continue;
		}

		int k;

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

		FVector Mins, Ranges;	// common ...
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

			uint32 PackedInfo;
			AnimationCompressionFormat KeyFormat;
			int ComponentMask;
			int NumKeys;

#define DECODE_PER_TRACK_INFO(info)									\
			KeyFormat = (AnimationCompressionFormat)(info >> 28);	\
			ComponentMask = (info >> 24) & 0xF;						\
			NumKeys       = info & 0xFFFFFF;						\
			HasTimeTracks = (ComponentMask & 8) != 0;

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
				Reader << PackedInfo;
				DECODE_PER_TRACK_INFO(PackedInfo);
				A->KeyPos.Empty(NumKeys);
				DBG("    [%d] trans: fmt=%d (%s), %d keys, mask %d\n", TrackIndex,
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
				for (k = 0; k < NumKeys; k++)
				{
					switch (KeyFormat)
					{
//					case ACF_None:
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
							A->KeyPos.Add(CVT(v));
						}
						break;
					TPR(ACF_IntervalFixed32NoW, FVectorIntervalFixed32)
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
							A->KeyPos.Add(v);
						}
						break;
					case ACF_Identity:
						A->KeyPos.Add(nullVec);
						break;
					default:
						appError("Unknown translation compression method: %d (%s)", KeyFormat, EnumToName(KeyFormat));
					}
				}
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (HasTimeTracks)
					ReadTimeArray(Reader, NumKeys, A->KeyPosTime, Seq->NumFrames);
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
				Reader << PackedInfo;
				DECODE_PER_TRACK_INFO(PackedInfo);
				A->KeyQuat.Empty(NumKeys);
				DBG("    [%d] rot  : fmt=%d (%s), %d keys, mask %d\n", TrackIndex,
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
				for (k = 0; k < NumKeys; k++)
				{
					switch (KeyFormat)
					{
//					TR (ACF_None, FQuat)
					case ACF_Float96NoW:
						{
							FQuatFloat96NoW q;
							Reader << q;
							FQuat q2 = q;				// convert
							A->KeyQuat.Add(CVT(q2));
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
							A->KeyQuat.Add(CVT(q2));
						}
						break;
					TR (ACF_Fixed32NoW, FQuatFixed32NoW)
					TRR(ACF_IntervalFixed32NoW, FQuatIntervalFixed32NoW)
					TR (ACF_Float32NoW, FQuatFloat32NoW)
					case ACF_Identity:
						A->KeyQuat.Add(nullQuat);
						break;
					default:
						appError("Unknown rotation compression method: %d (%s)", KeyFormat, EnumToName(KeyFormat));
					}
				}
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (HasTimeTracks)
					ReadTimeArray(Reader, NumKeys, A->KeyQuatTime, Seq->NumFrames);
			}
			unguard;

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

			for (k = 0; k < TransKeys; k++)
			{
				switch (TranslationCompressionFormat)
				{
				TP (ACF_None,               FVector)
				TP (ACF_Float96NoW,         FVector)
				TPR(ACF_IntervalFixed32NoW, FVectorIntervalFixed32)
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

		for (k = 0; k < RotKeys; k++)
		{
			switch (RotationCompressionFormat)
			{
			TR (ACF_None, FQuat)
			TR (ACF_Float96NoW, FQuatFloat96NoW)
			TR (ACF_Fixed48NoW, FQuatFixed48NoW)
			TR (ACF_Fixed32NoW, FQuatFixed32NoW)
			TRR(ACF_IntervalFixed32NoW, FQuatIntervalFixed32NoW)
			TR (ACF_Float32NoW, FQuatFloat32NoW)
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

	unguardf("Skel=%s Anim=%s", Name, Seq->Name);
}


/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

// PostSerialize() adds some serialization logic to data which were serialized as properties

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
		// part of data were serialized as properties
		Ar << CompressedByteStream;
	}
	else
	{
		bool bSerializeCompressedData;
		Ar << bSerializeCompressedData;

		if (bSerializeCompressedData)
		{
			// these fields were serialized as properties in pre-UE4.12 engine version
			Ar << (byte&)KeyEncodingFormat;
			Ar << (byte&)TranslationCompressionFormat;
			Ar << (byte&)RotationCompressionFormat;
			Ar << (byte&)ScaleCompressionFormat;

			Ar << CompressedTrackOffsets;
			Ar << CompressedScaleOffsets;

			Ar << CompressedTrackToSkeletonMapTable;
			Ar << CompressedCurveData;

			if (Ar.Game >= GAME_UE4(17))
			{
				// UE4.17+
				int32 CompressedRawDataSize;
				Ar << CompressedRawDataSize;
			}

			// compressed data
			Ar << CompressedByteStream;
			Ar << bUseRawDataOnly;

			if (KeyEncodingFormat == AKF_PerTrackCompression && CompressedScaleOffsets.OffsetData.Num())
			{
				TArray<uint8> SwappedData;
				TransferPerTrackData(SwappedData, CompressedByteStream);
				Exchange(SwappedData, CompressedByteStream);
			}
		}
	}

	unguard;
}

// UE4 has some mess in AEFPerTrackCompressionCodec::ByteSwapOut() (and ByteSwapIn): it sends
// data in order: translation data, rotation data, scale data. However, scale data stored in
// CompressedByteStream before translation and rotation. In other words, data reordered, but
// offsets pointed to original data. Here we're reordering data back, duplicating functionality
// of AEFPerTrackCompressionCodec::ByteSwapOut().
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
	if (!Skeleton) return;		// missing package etc
	Skeleton->ConvertAnims(this);

	// Release original animaiton data to save memory
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
