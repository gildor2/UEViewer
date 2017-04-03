#include "Core.h"

#if UNREAL3

#include "UnrealClasses.h"
#include "UnMesh3.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"				// for checking game type

#include "SkeletalMesh.h"
#include "TypeConvert.h"


// following defines will help finding new undocumented compression schemes
#define FIND_HOLES			1
//#define DEBUG_DECOMPRESS	1

/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

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


UAnimSet::~UAnimSet()
{
	delete ConvertedAnim;
}


#if LOST_PLANET3

struct FReducedAnimData_LP3
{
	int16					v1;
	int16					v2;

	friend FArchive& operator<<(FArchive &Ar, FReducedAnimData_LP3 &D)
	{
		return Ar << D.v1 << D.v2;
	}
};

#endif // LOST_PLANET3


void UAnimSet::Serialize(FArchive &Ar)
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
		for (int i = 0; i < HashSequences.Num(); i++) Sequences.Add(HashSequences[i].Seq);
		return;
		unguard;
	}
#endif // FRONTLINES
#if LOST_PLANET3
	if (Ar.Game == GAME_LostPlanet3 && Ar.ArLicenseeVer >= 90)
	{
		TArray<FReducedAnimData_LP3> d;
		Ar << d;
//		appPrintf("LostPlanet AnimSet %s: %d reduced tracks\n", Name, d.Num());
	}
#endif // LOST_PLANET3
	unguard;
}


/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

#if DEBUG_DECOMPRESS
#define DBG(...)			appPrintf(__VA_ARGS__)
#else
#define DBG(...)
#endif

void UAnimSequence::Serialize(FArchive &Ar)
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
	if (Ar.Game == GAME_MassEffect3) goto old_code;		// Mass Effect 3 has no RawAnimationData
#endif // MASSEFF
#if MOH2010
	if (Ar.Game == GAME_MOH2010) goto old_code;
#endif
#if TERA
	if (Ar.Game == GAME_Tera && Ar.ArLicenseeVer >= 11) goto new_code; // we have overriden ArVer, so compare by ArLicenseeVer ...
#endif
#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 181) // Transformers: Fall of Cybertron, no version in code
	{
		int UseNewFormat;
		Ar << UseNewFormat;
		if (UseNewFormat)
		{
			Ar << Trans3Data;
			return;
		}
	}
#endif // TRANSFORMERS
	if (Ar.ArVer >= 577)
	{
	new_code:
		Ar << RawAnimData;			// this field was moved to RawAnimationData, RawAnimData is deprecated
	}
#if PLA
	if (Ar.Game == GAME_PLA && Ar.ArVer >= 900)
	{
		FGuid unk;
		Ar << unk;
	}
#endif // PLA
old_code:
	Ar << CompressedByteStream;
#if ARGONAUTS
	if (Ar.Game == GAME_Argonauts && Ar.ArLicenseeVer >= 30)
	{
		Ar << CompressedTrackTimes;
		if (Ar.ReverseBytes)
		{
			// CompressedTrackTimes is originally serialized as array of words, should swap low and high words
			for (int i = 0; i < CompressedTrackTimes.Num(); i++)
			{
				unsigned v = CompressedTrackTimes[i];
				CompressedTrackTimes[i] = ((v & 0xFFFF) << 16) | ((v >> 16) & 0xFFFF);
			}
		}
	}
#endif // ARGONAUTS
#if BATMAN
	if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4 && Ar.ArLicenseeVer >= 55)
		Ar << AnimZip_Data;
#endif // BATMAN
#if LOST_PLANET3
	if (Ar.Game == GAME_LostPlanet3 && Ar.ArLicenseeVer >= 90)
	{
		TArray<FReducedAnimData_LP3> d;
		Ar << d;
	#if 0
		UAnimSet *AnimSet = static_cast<UAnimSet*>(Outer);
		assert(AnimSet && AnimSet->IsA("AnimSet"));
		//!!!!!
		printf("%s reduced: %d, %d tracks\n", *SequenceName, d.Num(), AnimSet->TrackBoneNames.Num());
		for (int i = 0; i < d.Num(); i++)
		{
			const FReducedAnimData_LP3 &v = d[i];
			printf("   %d  %d\n", v.v1, v.v2);
		}
	#endif
	}
#endif // LOST_PLANET3
	unguard;
}


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


#if ARGONAUTS

static void ReadArgonautsTimeArray(const TArray<unsigned> &SourceArray, int FirstKey, int NumKeys, TArray<float> &Times, float TimeScale)
{
	guard(ReadArgonautsTimeArray);

	Times.Empty(NumKeys);
	if (NumKeys <= 1) return;

	TimeScale /= 65535.0f;			// 0 -> 0.0f, 65535 -> track length

	for (int i = 0; i < NumKeys; i++)
	{
		int index = FirstKey + i;
		unsigned v = SourceArray[index / 2];
		if (!(index & 1))
			v &= 0xFFFF;			// low word
		else
			v >>= 16;				// high word
		Times.Add(v * TimeScale);
	}

	unguard;
}

#endif // ARGONAUTS


#if TRANSFORMERS

void UAnimSequence::DecodeTrans3Anims(CAnimSequence *Dst, UAnimSet *Owner) const
{
	guard(UAnimSequence::DecodeTrans3Anims);

	// read some counts first
	FMemReader Reader1(Trans3Data.GetData(), Trans3Data.Num());
	Reader1.SetupFrom(*Package);

	int NumberOfStaticRotations, NumberOfStaticTranslations, NumberOfAnimatedRotations, NumberOfAnimatedTranslations,
		NumberOfAnimatedUncompressedTranslations;
	Reader1 << NumberOfStaticRotations << NumberOfStaticTranslations
			<< NumberOfAnimatedRotations << NumberOfAnimatedTranslations
			<< NumberOfAnimatedUncompressedTranslations;

	// create new reader for keyframe data
	int StartOffset = Reader1.Tell();	// always equals to 20
	FMemReader Reader((uint8*)Trans3Data.GetData() + StartOffset, Trans3Data.Num() - StartOffset);

	// key index offsets
	int StartOfStaticRotations        = 0;
	int StartOfStaticTranslations     = StartOfStaticRotations + NumberOfStaticRotations;
	int StartOfAnimatedRotations      = StartOfStaticTranslations + NumberOfStaticTranslations;
	int StartOfAnimatedTranslations   = StartOfAnimatedRotations + NumberOfAnimatedRotations;
	int StartOfAnimUncompTranslations = StartOfAnimatedTranslations + NumberOfAnimatedTranslations;

	// determine quaternion size for this format
	int QuatSize;
	switch (RotationCompressionFormat)
	{
	case ACF_None:
		QuatSize = 16; break;
	case ACF_Float96NoW:
		QuatSize = 12; break;
	case ACF_Fixed48NoW:
	case ACF_IntervalFixed48NoW:
		QuatSize = 6; break;
	case ACF_IntervalFixed32NoW:
	case ACF_Fixed32NoW:
	case ACF_Float32NoW:
		QuatSize = 4; break;
	default:
		appError("Unknown RotationCompressionFormat %d (%s)", RotationCompressionFormat, EnumToName(RotationCompressionFormat));
	}

	// block sizes
	int StaticRotationSize        = 16 * NumberOfStaticRotations;					// FQuat
	int StaticTranslationsSize    = 12 * NumberOfStaticTranslations;				// FVector
	int AnimatedRotationSize      = QuatSize * NumberOfAnimatedRotations;
	int AnimatedTranslationSize   = 4  * NumberOfAnimatedTranslations;				// FPackedVector_Trans
	int AnimUncompTranslationSize = 12 * NumberOfAnimatedUncompressedTranslations;	// FVector
	int AnimatedDataSize          = AnimatedRotationSize + AnimatedTranslationSize + AnimUncompTranslationSize;
	// interval data blocks
	int RotationIntervalSize = 0;
	if (RotationCompressionFormat == ACF_IntervalFixed32NoW || RotationCompressionFormat == ACF_IntervalFixed48NoW)
		RotationIntervalSize = 40 * NumberOfAnimatedRotations;						// 3+3+4 floats
	int TranslationIntervalSize = 24 * NumberOfAnimatedTranslations;

	// compute offsets, in order of appearance in data
	int StaticRotationOffset       = 0;
	int StaticTranslationOffset    = StaticRotationOffset + StaticRotationSize;
	int RotationIntervalOffset     = StaticTranslationOffset + StaticTranslationsSize;
	int TranslationIntervalOffset  = RotationIntervalOffset + RotationIntervalSize;
	int AnimatedRotationOffset     = TranslationIntervalOffset + TranslationIntervalSize;
	int AnimatedTranslationOffset  = AnimatedRotationOffset + AnimatedRotationSize;
	int AnimatedUncompTranslationOffset = AnimatedTranslationOffset + AnimatedTranslationSize;

	// Differences in original game code: serialization function copies data to anither array with alignment of AnimatedRotationSize to 4 and
	// RotationIntervalOffset to 16, plus is makes rotation interval data in size of 48 bytes each (it takes 40 bytes in a package)

	// verification
	int TotalDataSize = AnimatedRotationOffset + AnimatedDataSize * NumFrames;
	assert(TotalDataSize == Trans3Data.Num() - StartOffset);

	DBG("          TF3: StatRot=%d StatTrans=%d AnimRot=%d AnimTrans=%d UncompTrans=%d TotalSize=%d (%d)\n",
		NumberOfStaticRotations, NumberOfStaticTranslations, NumberOfAnimatedRotations, NumberOfAnimatedTranslations,
		NumberOfAnimatedUncompressedTranslations, Trans3Data.Num() - StartOffset, TotalDataSize
	);
/*	DBG("  StatRot: %08X [%d]\n"
		"  StatTr:  %08X [%d]\n"
		"  RotInt:  %08X [%d]\n"
		"  TrInt:   %08X [%d]\n"
		"  AnRot:   %08X [%d] + N\n"
		"  AnTr:    %08X [%d]\n"
		"  AnTrU:   %08X [%d]\n",
		StaticRotationOffset, NumberOfStaticRotations,
		StaticTranslationOffset, NumberOfStaticTranslations,
		RotationIntervalOffset, NumberOfAnimatedRotations,
		TranslationIntervalOffset, NumberOfAnimatedTranslations,
		AnimatedRotationOffset, NumberOfAnimatedRotations,
		AnimatedTranslationOffset, NumberOfAnimatedTranslations,
		AnimatedUncompTranslationOffset, NumberOfAnimatedUncompressedTranslations
	); */

	static const CVec3 nullVec  = { 0, 0, 0 };
	static const CQuat nullQuat = { 0, 0, 0, 1 };

	int NumTracks = Owner->TrackBoneNames.Num();
	assert(TrackOffsets.Num() == NumTracks * 2);
	Dst->Tracks.Empty(NumTracks);
	int i;
	for (int Bone = 0; Bone < NumTracks; Bone++)
	{
		CAnimTrack *A = new (Dst->Tracks) CAnimTrack;
		int RotKeyIndex   = TrackOffsets[Bone * 2    ];
		int TransKeyIndex = TrackOffsets[Bone * 2 + 1];

		// decode translation
		DBG("          Trans: key=%d -> ", TransKeyIndex);
		if (TransKeyIndex < StartOfStaticTranslations)
		{
			// null vector
			DBG("null ");
			A->KeyPos.Add(nullVec);
		}
		else if (TransKeyIndex < StartOfAnimatedTranslations)
		{
			// static vector (single key)
			TransKeyIndex -= StartOfStaticTranslations;
			DBG("static[%d] ", TransKeyIndex);
			Reader.Seek(StaticTranslationOffset + 12 * TransKeyIndex);
			FVector pos;
			Reader << pos;
			A->KeyPos.Add(CVT(pos));
		}
		else if (TransKeyIndex < StartOfAnimUncompTranslations)
		{
			// animated compressed translation
			TransKeyIndex -= StartOfAnimatedTranslations;
			DBG("comp[%d] ", TransKeyIndex);
			A->KeyPos.Empty(NumFrames);
			Reader.Seek(TranslationIntervalOffset + 24 * TransKeyIndex);
			FVector Mins, Ranges;
			Reader << Mins << Ranges;
			for (i = 0; i < NumFrames; i++)
			{
				Reader.Seek(AnimatedTranslationOffset + 4 * TransKeyIndex + i * AnimatedDataSize);
				FPackedVector_Trans pos;
				Reader << pos;
				FVector pos2 = pos.ToVector(Mins, Ranges); // convert
				A->KeyPos.Add(CVT(pos2));
			}
		}
		else
		{
			// animated uncompressed translation
			TransKeyIndex -= StartOfAnimUncompTranslations;
			DBG("uncomp[%d] ", TransKeyIndex);
			A->KeyPos.Empty(NumFrames);
			for (i = 0; i < NumFrames; i++)
			{
				Reader.Seek(AnimatedUncompTranslationOffset + 12 * TransKeyIndex + i * AnimatedDataSize);
				FVector pos;
				Reader << pos;
				A->KeyPos.Add(CVT(pos));
			}
		}

		// decode rotation
		DBG("; Rot: key=%d -> ", RotKeyIndex);
		if (RotKeyIndex < StartOfStaticRotations)
		{
			// null quaternion
			DBG("null ");
			A->KeyQuat.Add(nullQuat);
		}
		else if (RotKeyIndex < StartOfAnimatedRotations)
		{
			// static rotation
			RotKeyIndex -= StartOfStaticRotations;
			DBG("static[%d] ", RotKeyIndex);
			Reader.Seek(StaticRotationOffset + 16 * RotKeyIndex);
			FQuat q;
			Reader << q;
			q.W *= -1;
			A->KeyQuat.Add(CVT(q));
		}
		else
		{
			// animated rotation
			RotKeyIndex -= StartOfAnimatedRotations;
			DBG("comp[%d] ", RotKeyIndex);
			A->KeyQuat.Empty(NumFrames);
			FVector Mins, Ranges;
			FQuat TransQuatBase;
			if (RotationIntervalSize)
			{
				// get interval data
				Reader.Seek(RotationIntervalOffset + 40 * RotKeyIndex);
				Reader << Mins << Ranges;
				Reader << TransQuatBase.W << TransQuatBase.X << TransQuatBase.Y << TransQuatBase.Z;
			}
			for (i = 0; i < NumFrames; i++)
			{
				Reader.Seek(AnimatedRotationOffset + QuatSize * RotKeyIndex + i * AnimatedDataSize);
				switch (RotationCompressionFormat)
				{
				TR (ACF_None, FQuat)
				TR (ACF_Float96NoW, FQuatFloat96NoW)
				TR (ACF_Fixed48NoW, FQuatFixed48NoW)
				TR (ACF_Fixed32NoW, FQuatFixed32NoW)
				TRR(ACF_IntervalFixed32NoW, FQuatIntervalFixed32NoW)
				TR (ACF_Float32NoW, FQuatFloat32NoW)
				TRR(ACF_IntervalFixed48NoW, FQuatIntervalFixed48NoW_Trans)
				default:
					appError("Unknown rotation compression method: %d (%s)", RotationCompressionFormat, EnumToName(RotationCompressionFormat));
				}
			}
			if (RotationIntervalSize)
			{
				for (i = 0; i < NumFrames; i++)
				{
					CQuat q = A->KeyQuat[i];
					q.Mul(CVT(TransQuatBase));
					q.w *= -1;
					A->KeyQuat[i] = q;
				}
			}
		}

		DBG(" - %s\n", *Owner->TrackBoneNames[Bone]);
	}

	unguard;
}

#endif // TRANSFORMERS


#if BLADENSOUL

void ReadBnS_ZOnlyRLE(FArchive& Reader, int RotKeys, CAnimTrack* A)
{
	guard(ReadBnS_ZOnlyRLE);

	int32 keyMode, numRLE_keys;
	Reader << keyMode << numRLE_keys;
	assert(keyMode >= 0 || keyMode <= 3);
	// Read RLE decoding table
	TArray<int16> RLETable;
	RLETable.AddUninitialized(numRLE_keys * 2);
	for (int i = 0; i < numRLE_keys; i++)
	{
		Reader << RLETable[i*2] << RLETable[i*2+1];
	}
	int nextRLE_index = 0;
	int numAddedKeys = 0;
	while (numAddedKeys < RotKeys)
	{
		FQuatFloat96NoW q;
		if (keyMode == 0)
		{
			Reader << q;
		}
		else
		{
			q.X = q.Y = q.Z = 0;
			float v;
			Reader << v;
			(&q.X)[keyMode - 1] = v;
		}
		FQuat q2 = q;		// conversion
		// Now decode RLE table
		if ((nextRLE_index < RLETable.Num()) && (RLETable[nextRLE_index] == numAddedKeys))
		{
			int numSameKeys = RLETable[nextRLE_index+1] - RLETable[nextRLE_index] + 1;
			for (int i = 0; i < numSameKeys; i++)
			{
				A->KeyQuat.Add(CVT(q2));
			}
			nextRLE_index += 2;
			numAddedKeys += numSameKeys;
		}
		else
		{
			A->KeyQuat.Add(CVT(q2));
			numAddedKeys++;
		}
	}

	unguard;
}

#endif // BLADENSOUL

void UAnimSet::ConvertAnims()
{
	guard(UAnimSet::ConvertAnims);

	int i, j;

	CAnimSet *AnimSet = new CAnimSet(this);
	ConvertedAnim = AnimSet;

	int ArVer  = GetArVer();
	int ArGame = GetGame();

#if MASSEFF
	UBioAnimSetData *BioData = NULL;
	if ((ArGame >= GAME_MassEffect && ArGame <= GAME_MassEffect3) && !TrackBoneNames.Num() && Sequences.Num())
	{
		// Mass Effect has separated TrackBoneNames from UAnimSet to UBioAnimSetData
		BioData = Sequences[0]->m_pBioAnimSetData;
		if (BioData)
		{
			bAnimRotationOnly = BioData->bAnimRotationOnly;
			CopyArray(TrackBoneNames, BioData->TrackBoneNames);
			CopyArray(UseTranslationBoneNames, BioData->UseTranslationBoneNames);
		}
	}
#endif // MASSEFF

	if (!TrackBoneNames.Num())
	{
		// in Mass Effect 2 it is possible that BioAnimSetData placed in other package which is missing (for umodel),
		// so m_pBioAnimSetData would be NULL and we will get the following error
		appPrintf("WARNING: AnimSet %s has %d sequences, but empty TrackBoneNames\n", Name, Sequences.Num());
		return;
	}
	CopyArray(AnimSet->TrackBoneNames, TrackBoneNames);

#if FIND_HOLES
	bool findHoles = true;
#endif
	int NumTracks = TrackBoneNames.Num();

	AnimSet->AnimRotationOnly = bAnimRotationOnly;
	if (UseTranslationBoneNames.Num())
	{
		AnimSet->UseAnimTranslation.AddZeroed(NumTracks);
		for (i = 0; i < UseTranslationBoneNames.Num(); i++)
		{
			for (j = 0; j < TrackBoneNames.Num(); j++)
				if (UseTranslationBoneNames[i] == TrackBoneNames[j])
					AnimSet->UseAnimTranslation[j] = true;
		}
	}
	if (ForceMeshTranslationBoneNames.Num())
	{
		AnimSet->ForceMeshTranslation.AddZeroed(NumTracks);
		for (i = 0; i < ForceMeshTranslationBoneNames.Num(); i++)
		{
			for (j = 0; j < TrackBoneNames.Num(); j++)
				if (ForceMeshTranslationBoneNames[i] == TrackBoneNames[j])
					AnimSet->ForceMeshTranslation[j] = true;
		}
	}

	DBG("----------- AnimSet %s: %d seq, %d bones -----------\n", Name, Sequences.Num(), TrackBoneNames.Num());

	for (i = 0; i < Sequences.Num(); i++)
	{
		const UAnimSequence *Seq = Sequences[i];
		if (!Seq)
		{
			appPrintf("WARNING: %s: no sequence %d\n", Name, i);
			continue;
		}
#if DEBUG_DECOMPRESS
		appPrintf("Sequence %d (%s, %s):%s %d bones, %d offsets (%g per bone), %d frames, %d compressed data\n"
			   "          trans %s, rot %s, key %s\n",
			i, *Seq->SequenceName, Seq->Name,
			Seq->bIsAdditive ? " [additive]" : "",
			NumTracks, Seq->CompressedTrackOffsets.Num(), Seq->CompressedTrackOffsets.Num() / (float)NumTracks,
			Seq->NumFrames,
			Seq->CompressedByteStream.Num(),
			EnumToName(Seq->TranslationCompressionFormat),
			EnumToName(Seq->RotationCompressionFormat),
			EnumToName(Seq->KeyEncodingFormat)
		);
	#if TRANSFORMERS
		if (ArGame == GAME_Transformers && Seq->Trans3Data.Num()) goto no_track_details;
	#endif
		for (int i2 = 0; i2 < Seq->CompressedTrackOffsets.Num(); /*empty*/)
		{
			if (Seq->KeyEncodingFormat != AKF_PerTrackCompression)
			{
				int TransOffset = Seq->CompressedTrackOffsets[i2  ];
				int TransKeys   = Seq->CompressedTrackOffsets[i2+1];
				int RotOffset   = Seq->CompressedTrackOffsets[i2+2];
				int RotKeys     = Seq->CompressedTrackOffsets[i2+3];
				appPrintf("    [%d] = trans %d[%d] rot %d[%d] - %s\n", i2/4,
					TransOffset, TransKeys, RotOffset, RotKeys, *TrackBoneNames[i2/4]
				);
				i2 += 4;
			}
			else
			{
				int TransOffset = Seq->CompressedTrackOffsets[i2  ];
				int RotOffset   = Seq->CompressedTrackOffsets[i2+1];
				appPrintf("    [%d] = trans %d rot %d - %s\n", i2/2,
					TransOffset, RotOffset, *TrackBoneNames[i2/2]
				);
				i2 += 2;
			}
		}
	no_track_details: ;
#endif // DEBUG_DECOMPRESS
#if TRANSFORMERS
		if (ArGame == GAME_Transformers && Seq->Trans3Data.Num())
		{
			CAnimSequence *Dst = new CAnimSequence;
			AnimSet->Sequences.Add(Dst);
			Dst->Name      = Seq->SequenceName;
			Dst->NumFrames = Seq->NumFrames;
			Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;
			Seq->DecodeTrans3Anims(Dst, this);
			continue;
		}
#endif // TRANSFORMERS
#if MASSEFF
		if (Seq->m_pBioAnimSetData != BioData)
		{
			appNotify("Mass Effect AnimSequence %s/%s has different BioAnimSetData object, removing track",
				Name, *Seq->SequenceName);
			continue;
		}
#endif // MASSEFF
#if BATMAN
		if (ArGame >= GAME_Batman2 && ArGame <= GAME_Batman4 && Seq->AnimZip_Data.Num())
		{
			CAnimSequence *Dst = new CAnimSequence;
			AnimSet->Sequences.Add(Dst);
			Dst->Name      = Seq->SequenceName;
			Dst->NumFrames = Seq->NumFrames;
			Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;
			Seq->DecodeBatman2Anims(Dst, this);
			continue;
		}
#endif // BATMAN
		// some checks
		int offsetsPerBone = 4;
		if (Seq->KeyEncodingFormat == AKF_PerTrackCompression)
			offsetsPerBone = 2;
#if TLR
		if (ArGame == GAME_TLR) offsetsPerBone = 6;
#endif
#if XMEN
		if (ArGame == GAME_XMen) offsetsPerBone = 6;		// has additional CutInfo array
#endif
		if (Seq->CompressedTrackOffsets.Num() != NumTracks * offsetsPerBone && !Seq->RawAnimData.Num())
		{
			appNotify("AnimSequence %s/%s has wrong CompressedTrackOffsets size (has %d, expected %d), removing track",
				Name, *Seq->SequenceName, Seq->CompressedTrackOffsets.Num(), NumTracks * offsetsPerBone);
			continue;
		}

		// create CAnimSequence
		CAnimSequence *Dst = new CAnimSequence;
		AnimSet->Sequences.Add(Dst);
		Dst->Name      = Seq->SequenceName;
		Dst->NumFrames = Seq->NumFrames;
		Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;

		// bone tracks ...
		Dst->Tracks.Empty(NumTracks);

		FMemReader Reader(Seq->CompressedByteStream.GetData(), Seq->CompressedByteStream.Num());
		Reader.SetupFrom(*Package);

		bool HasTimeTracks = (Seq->KeyEncodingFormat == AKF_VariableKeyLerp);

		int offsetIndex = 0;
		for (j = 0; j < NumTracks; j++, offsetIndex += offsetsPerBone)
		{
			CAnimTrack *A = new (Dst->Tracks) CAnimTrack;

			int k;

			if (!Seq->CompressedTrackOffsets.Num())	//?? or if RawAnimData.Num() != 0
			{
				// using RawAnimData array
				assert(Seq->RawAnimData.Num() == NumTracks);
				CopyArray(A->KeyPos,  CVT(Seq->RawAnimData[j].PosKeys));
				CopyArray(A->KeyQuat, CVT(Seq->RawAnimData[j].RotKeys));
				CopyArray(A->KeyTime, Seq->RawAnimData[j].KeyTimes);	// may be empty
				for (int k = 0; k < A->KeyTime.Num(); k++)
					A->KeyTime[k] *= Dst->Rate;
				continue;
			}

			FVector Mins, Ranges;	// common ...
			static const CVec3 nullVec  = { 0, 0, 0 };
			static const CQuat nullQuat = { 0, 0, 0, 1 };

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

				uint32 PackedInfo;
				AnimationCompressionFormat KeyFormat;
				int ComponentMask;
				int NumKeys;

#define DECODE_PER_TRACK_INFO(info)										\
				KeyFormat = (AnimationCompressionFormat)(info >> 28);	\
				ComponentMask = (info >> 24) & 0xF;						\
				NumKeys       = info & 0xFFFFFF;						\
				HasTimeTracks = (ComponentMask & 8) != 0;

				guard(TransKeys);
				// read translation keys
				if (TransOffset == -1)
				{
					A->KeyPos.Add(nullVec);
					DBG("    [%d] no translation data\n", j);
				}
				else
				{
					Reader.Seek(TransOffset);
					Reader << PackedInfo;
					DECODE_PER_TRACK_INFO(PackedInfo);
					A->KeyPos.Empty(NumKeys);
					DBG("    [%d] trans: fmt=%d (%s), %d keys, mask %d\n", j,
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
//						case ACF_None:
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
					DBG("    [%d] no rotation data\n", j);
				}
				else
				{
					Reader.Seek(RotOffset);
					Reader << PackedInfo;
					DECODE_PER_TRACK_INFO(PackedInfo);
#if BORDERLANDS
					if (ArGame == GAME_Borderlands || ArGame == GAME_AliensCM)	// Borderlands 2
					{
						// this game has more different key formats; each described by number. which
						// could differ from numbers in UnMesh3.h; so, transcode format
						switch (KeyFormat)
						{
						case 6:  KeyFormat = ACF_Delta40NoW; break; // not used
						case 7:  KeyFormat = ACF_Delta48NoW; break; // not used
						case 8:  KeyFormat = ACF_Identity;   break;
						case 9:  KeyFormat = ACF_PolarEncoded32; break;
						case 10: KeyFormat = ACF_PolarEncoded48; break;
						}
					}
#endif // BORDERLANDS
					A->KeyQuat.Empty(NumKeys);
					DBG("    [%d] rot  : fmt=%d (%s), %d keys, mask %d\n", j,
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
//						TR (ACF_None, FQuat)
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
#if BORDERLANDS
						TR (ACF_PolarEncoded32, FQuatPolarEncoded32)
						TR (ACF_PolarEncoded48, FQuatPolarEncoded48)
#endif // BORDERLANDS
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
#if TLR
			int ScaleOffset = 0, ScaleKeys = 0;
			if (ArGame == GAME_TLR)
			{
				ScaleOffset  = Seq->CompressedTrackOffsets[offsetIndex+4];
				ScaleKeys    = Seq->CompressedTrackOffsets[offsetIndex+5];
			}
#endif // TLR
//			appPrintf("[%d:%d:%d] :  %d[%d]  %d[%d]  %d[%d]\n", j, Seq->RotationCompressionFormat, Seq->TranslationCompressionFormat, TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys);

			A->KeyPos.Empty(TransKeys);
			A->KeyQuat.Empty(RotKeys);

			// read translation keys
			if (TransKeys)
			{
#if FIND_HOLES
				int hole = TransOffset - Reader.Tell();
				if (findHoles && hole/** && abs(hole) > 4*/)	//?? should not be holes at all
				{
					appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before TransTrack (KeyFormat=%d/%d)",
						Name, *Seq->SequenceName, j, hole, Seq->KeyEncodingFormat, Seq->TranslationCompressionFormat);
///					findHoles = false;
				}
#endif // FIND_HOLES
				Reader.Seek(TransOffset);
				AnimationCompressionFormat TranslationCompressionFormat = Seq->TranslationCompressionFormat;
#if ARGONAUTS
				if (ArGame == GAME_Argonauts) goto do_not_override_trans_format;
#endif
				if (TransKeys == 1)
					TranslationCompressionFormat = ACF_None;	// single key is stored without compression
			do_not_override_trans_format:
				// read mins/ranges
				if (TranslationCompressionFormat == ACF_IntervalFixed32NoW)
				{
					assert(ArVer >= 761);
					Reader << Mins << Ranges;
				}
#if BORDERLANDS
				FVector Base;
				if (ArGame == GAME_Borderlands && (TranslationCompressionFormat == ACF_Delta40NoW || TranslationCompressionFormat == ACF_Delta48NoW))
				{
					Reader << Mins << Ranges << Base;
				}
#endif // BORDERLANDS

#if TRANSFORMERS
				if (ArGame == GAME_Transformers && TransKeys >= 4 && GetLicenseeVer() >= 100)
				{
					FVector Scale, Offset;
					Reader << Scale.X;
					if (Scale.X != -1)
					{
						Reader << Scale.Y << Scale.Z << Offset;
//						appPrintf("  trans: %g %g %g -- %g %g %g\n", FVECTOR_ARG(Offset), FVECTOR_ARG(Scale));
						for (k = 0; k < TransKeys; k++)
						{
							FPackedVector_Trans pos;
							Reader << pos;
							FVector pos2 = pos.ToVector(Offset, Scale); // convert
							A->KeyPos.Add(CVT(pos2));
						}
						goto trans_keys_done;
					} // else - original code with 4-byte overhead
				} // else - original code for uncompressed vector
#endif // TRANSFORMERS

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
#if BORDERLANDS
					case ACF_Delta48NoW:
						{
							if (k == 0)
							{
								// "Base" works as 1st key
								A->KeyPos.Add(CVT(Base));
								continue;
							}
							FVectorDelta48NoW V;
							Reader << V;
							FVector V2;
							V2 = V.ToVector(Mins, Ranges, Base);
							Base = V2;			// for delta
							A->KeyPos.Add(CVT(V2));
						}
						break;
#endif // BORDERLANDS
#if ARGONAUTS
					case ATCF_Float16:
						{
							uint16 x, y, z;
							Reader << x << y << z;
							FVector v;
							v.X = half2float(x) / 2;	// Argonauts has "half" with biased exponent, so fix it with division by 2
							v.Y = half2float(y) / 2;
							v.Z = half2float(z) / 2;
							A->KeyPos.Add(CVT(v));
						}
						break;
#endif // ARGONAUTS
					default:
						appError("Unknown translation compression method: %d (%s)", TranslationCompressionFormat, EnumToName(TranslationCompressionFormat));
					}
				}

			trans_keys_done:
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (HasTimeTracks)
					ReadTimeArray(Reader, TransKeys, A->KeyPosTime, Seq->NumFrames);
			}
			else
			{
//				A->KeyPos.Add(nullVec);
//				appNotify("No translation keys!");
			}

#if DEBUG_DECOMPRESS
			int TransEnd = Reader.Tell();
#endif
#if FIND_HOLES
			int hole = RotOffset - Reader.Tell();
			if (findHoles && hole/** && abs(hole) > 4*/)	//?? should not be holes at all
			{
				appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before RotTrack (KeyFormat=%d/%d)",
					Name, *Seq->SequenceName, j, hole, Seq->KeyEncodingFormat, Seq->RotationCompressionFormat);
///				findHoles = false;
			}
#endif // FIND_HOLES
			// read rotation keys
			Reader.Seek(RotOffset);
			AnimationCompressionFormat RotationCompressionFormat = Seq->RotationCompressionFormat;
			if (RotKeys <= 0)
				goto rot_keys_done;
			if (RotKeys == 1)
			{
				RotationCompressionFormat = ACF_Float96NoW;	// single key is stored without compression
			}
			else if (RotationCompressionFormat == ACF_IntervalFixed32NoW || ArVer < 761)
			{
#if SHADOWS_DAMNED
				if (ArGame == GAME_ShadowsDamned) goto skip_ranges;
#endif
				// starting with version 761 Mins/Ranges are read only when needed - i.e. for ACF_IntervalFixed32NoW
				Reader << Mins << Ranges;
			skip_ranges: ;
			}
#if BORDERLANDS
			FQuat Base;
			if (ArGame == GAME_Borderlands && (RotationCompressionFormat == ACF_Delta40NoW || RotationCompressionFormat == ACF_Delta48NoW))
			{
				Reader << Base;			// in addition to Mins and Ranges
			}
#endif // BORDERLANDS
#if TRANSFORMERS
			FQuat TransQuatBase;
			if (ArGame == GAME_Transformers && RotKeys >= 2)
				Reader << TransQuatBase;
#endif // TRANSFORMERS
#if BLADENSOUL
			if (ArGame == GAME_BladeNSoul && RotationCompressionFormat == ACF_ZOnlyRLE)
			{
				ReadBnS_ZOnlyRLE(Reader, RotKeys, A);
				goto rot_keys_done;
			}
#endif // BLADENSOUL

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
#if BATMAN
				TR (ACF_Fixed48Max, FQuatFixed48Max)
#endif
#if MASSEFF
				TR (ACF_BioFixed48, FQuatBioFixed48)	// Mass Effect 2 animation compression
#endif
#if BORDERLANDS
				case ACF_Delta48NoW:
					{
						if (k == 0)
						{
							// "Base" works as 1st key
							A->KeyQuat.Add(CVT(Base));
							continue;
						}
						FQuatDelta48NoW q;
						Reader << q;
						FQuat q2;
						q2 = q.ToQuat(Mins, Ranges, Base);
						Base = q2;			// for delta
						A->KeyQuat.Add(CVT(q2));
					}
					break;
				TR (ACF_PolarEncoded32, FQuatPolarEncoded32)
				TR (ACF_PolarEncoded48, FQuatPolarEncoded48)
#endif // BORDERLANDS
#if TRANSFORMERS || ARGONAUTS
				case ACF_IntervalFixed48NoW:
	#if TRANSFORMERS
					if (ArGame == GAME_Transformers)
					{
						FQuatIntervalFixed48NoW_Trans q;
						FQuat q2;
						Reader << q;
						q2 = q.ToQuat(Mins, Ranges);
						A->KeyQuat.Add(CVT(q2));
					}
	#endif
	#if ARGONAUTS
					if (ArGame == GAME_Argonauts)
					{
						FQuatIntervalFixed48NoW_Argo q;
						FQuat q2;
						Reader << q;
						q2 = q.ToQuat(Mins, Ranges);
						A->KeyQuat.Add(CVT(q2));
					}
	#endif // ARGONAUTS
					break;
#endif // TRANSFORMERS || ARGONAUTS
#if ARGONAUTS
				TR (ACF_Fixed64NoW, FQuatFixed64NoW_Argo)
				TR (ACF_Float48NoW, FQuatFloat48NoW_Argo)
#endif // ARGONAUTS
				default:
					appError("Unknown rotation compression method: %d (%s)", RotationCompressionFormat, EnumToName(RotationCompressionFormat));
				}
			}

#if TRANSFORMERS
			if (ArGame == GAME_Transformers && RotKeys >= 2 &&
				(RotationCompressionFormat == ACF_IntervalFixed32NoW || RotationCompressionFormat == ACF_IntervalFixed48NoW))
			{
				for (int i = 0; i < RotKeys; i++)
				{
					CQuat q = A->KeyQuat[i];
					q.Mul(CVT(TransQuatBase));
					A->KeyQuat[i] = q;
				}
			}
#endif // TRANSFORMERS

		rot_keys_done:
			// align to 4 bytes
			Reader.Seek(Align(Reader.Tell(), 4));
			if (HasTimeTracks)
				ReadTimeArray(Reader, RotKeys, A->KeyQuatTime, Seq->NumFrames);

#if TLR
			if (ScaleKeys)
			{
				// no ScaleKeys support, simply drop data
				Reader.Seek(ScaleOffset + ScaleKeys * 12);
				Reader.Seek(Align(Reader.Tell(), 4));
			}
#endif // TLR

#if ARGONAUTS
			if (ArGame == GAME_Argonauts && Seq->CompressedTrackTimeOffsets.Num())
			{
				// convert time tracks
				ReadArgonautsTimeArray(Seq->CompressedTrackTimes, Seq->CompressedTrackTimeOffsets[j*2  ], TransKeys, A->KeyPosTime,  Seq->NumFrames);
				ReadArgonautsTimeArray(Seq->CompressedTrackTimes, Seq->CompressedTrackTimeOffsets[j*2+1], RotKeys,   A->KeyQuatTime, Seq->NumFrames);
			}
#endif // ARGONAUTS

#if DEBUG_DECOMPRESS
//			appPrintf("[%s : %s] Frames=%d KeyPos.Num=%d KeyQuat.Num=%d KeyFmt=%s\n", *Seq->SequenceName, *TrackBoneNames[j],
//				Seq->NumFrames, A->KeyPos.Num(), A->KeyQuat.Num(), *Seq->KeyEncodingFormat);
			appPrintf("  ->[%d]: t %d .. %d + r %d .. %d (%d/%d keys)\n", j,
				TransOffset, TransEnd, RotOffset, Reader.Tell(), TransKeys, RotKeys);
#endif // DEBUG_DECOMPRESS
		}
	}

	unguard;
}


#if MASSEFF

void UBioAnimSetData::PostLoad()
{
	TArray<UAnimSequence*> LinkedSequences;

	for (int i = 0; i < Package->Summary.ExportCount; i++)
	{
		FObjectExport &Exp = Package->ExportTable[i];
		UObject *Obj = Exp.Object;
		if (!Obj) continue;

		if (Obj->IsA("AnimSet"))
		{
			UAnimSet *Set = static_cast<UAnimSet*>(Obj);
			if (Set->m_pBioAnimSetData == this)
				return;					// this UBioAnimSetData already has
		}
		else if (Obj->IsA("AnimSequence"))
		{
			UAnimSequence *Seq = static_cast<UAnimSequence*>(Obj);
			if (Seq->m_pBioAnimSetData == this)
				LinkedSequences.Add(Seq);
		}
	}

	if (!LinkedSequences.Num()) return;	// there is no UAnimSequence for this UBioAnimSetData

	// generate UAnimSet
	char AnimSetName[256];
	strcpy(AnimSetName, Name);
	int len = strlen(AnimSetName);
	if (len > 15 && !strcmp(AnimSetName + len - 15, "_BioAnimSetData"))	// truncate "_BioAnimSetData" suffix
		AnimSetName[len - 15] = 0;
	appPrintf("Generating AnimSet %s (%d sequences)\n", AnimSetName, LinkedSequences.Num());
	UAnimSet *AnimSet = static_cast<UAnimSet*>(CreateClass("AnimSet"));
	AnimSet->Name              = appStrdupPool(AnimSetName);
	AnimSet->Package           = Package;
	AnimSet->m_pBioAnimSetData = this;
	CopyArray(AnimSet->Sequences, LinkedSequences);

	AnimSet->PostLoad();
}

#endif // MASSEFF


#endif // UNREAL3
