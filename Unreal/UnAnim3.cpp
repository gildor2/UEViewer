#include "Core.h"

#if UNREAL3

#include "UnrealClasses.h"
#include "UnMesh3.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"				// for checking game type

#include "SkeletalMesh.h"
#include "TypeConvert.h"


/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

UAnimSet::~UAnimSet()
{
	delete ConvertedAnim;
}


// following defines will help finding new undocumented compression schemes
#define FIND_HOLES			1
//#define DEBUG_DECOMPRESS	1

static void ReadTimeArray(FArchive &Ar, int NumKeys, TArray<float> &Times, int NumFrames)
{
	guard(ReadTimeArray);
	if (NumKeys <= 1) return;

//	appPrintf("  pos=%4X keys (max=%X)[ ", Ar.Tell(), NumFrames);
	if (NumFrames < 256)
	{
		for (int k = 0; k < NumKeys; k++)
		{
			byte v;
			Ar << v;
			Times.AddItem(v);
//			if (k < 4 || k > NumKeys - 5) appPrintf(" %02X ", v);
//			else if (k == 4) appPrintf("...");
		}
	}
	else
	{
		for (int k = 0; k < NumKeys; k++)
		{
			word v;
			Ar << v;
			Times.AddItem(v);
//			if (k < 4 || k > NumKeys - 5) appPrintf(" %04X ", v);
//			else if (k == 4) appPrintf("...");
		}
	}
//	appPrintf(" ]\n");

	// align to 4 bytes
	Ar.Seek(Align(Ar.Tell(), 4));

	unguard;
}


#if TRANSFORMERS

static FQuat TransModifyQuat(const FQuat &q, const FQuat &m)
{
	FQuat r;
	float x  = q.X, y  = q.Y, z  = q.Z, w  = q.W;
	float sx = m.X, sy = m.Y, sz = m.Z, sw = m.W;

	float VAR_A = (sy-sz)*(z-y);
	float VAR_B = (sz+sy)*(w-x);
	float VAR_C = (sw-sx)*(z+y);
	float VAR_D = (sw+sz)*(w-y) + (sw-sz)*(w+y) + (sx+sy)*(x+z);
	float xmm0  = ( VAR_D + (sx-sy)*(z-x) ) / 2;

	r.X =  (sx+sw)*(x+w) + xmm0 - VAR_D;
	r.Y = -(sw+sz)*(w-y) + xmm0 + VAR_B;
	r.Z = -(sw-sz)*(w+y) + xmm0 + VAR_C;
	r.W = -(sx+sy)*(x+z) + xmm0 + VAR_A;

	return r;
}

#endif // TRANSFORMERS


void UAnimSet::ConvertAnims()
{
	guard(UAnimSet::ConvertAnims);

	int i, j;

	CAnimSet *AnimSet = new CAnimSet(this);
	ConvertedAnim = AnimSet;

#if MASSEFF
	UBioAnimSetData *BioData = NULL;
	if ((Package->Game == GAME_MassEffect || Package->Game == GAME_MassEffect2) && !TrackBoneNames.Num() && Sequences.Num())
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

	CopyArray(AnimSet->TrackBoneNames, TrackBoneNames);

#if FIND_HOLES
	bool findHoles = true;
#endif
	int NumTracks = TrackBoneNames.Num();

	AnimSet->AnimRotationOnly = bAnimRotationOnly;
	if (UseTranslationBoneNames.Num())
	{
		AnimSet->UseAnimTranslation.Add(NumTracks);
		for (i = 0; i < UseTranslationBoneNames.Num(); i++)
		{
			for (j = 0; j < TrackBoneNames.Num(); j++)
				if (UseTranslationBoneNames[i] == TrackBoneNames[j])
					AnimSet->UseAnimTranslation[j] = true;
		}
	}
	if (ForceMeshTranslationBoneNames.Num())
	{
		AnimSet->ForceMeshTranslation.Add(NumTracks);
		for (i = 0; i < ForceMeshTranslationBoneNames.Num(); i++)
		{
			for (j = 0; j < TrackBoneNames.Num(); j++)
				if (ForceMeshTranslationBoneNames[i] == TrackBoneNames[j])
					AnimSet->ForceMeshTranslation[j] = true;
		}
	}

	for (i = 0; i < Sequences.Num(); i++)
	{
		const UAnimSequence *Seq = Sequences[i];
		if (!Seq)
		{
			appPrintf("WARNING: %s: no sequence %d\n", Name, i);
			continue;
		}
#if DEBUG_DECOMPRESS
		appPrintf("Sequence: %d bones, %d offsets (%g per bone), %d frames, %d compressed data\n"
			   "          trans %s, rot %s, key %s\n",
			NumTracks, Seq->CompressedTrackOffsets.Num(), Seq->CompressedTrackOffsets.Num() / (float)NumTracks,
			Seq->NumFrames,
			Seq->CompressedByteStream.Num(),
			EnumToName("AnimationCompressionFormat", Seq->TranslationCompressionFormat),
			EnumToName("AnimationCompressionFormat", Seq->RotationCompressionFormat),
			EnumToName("AnimationKeyFormat",         Seq->KeyEncodingFormat)
		);
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
#endif // DEBUG_DECOMPRESS
#if MASSEFF
		if (Seq->m_pBioAnimSetData != BioData)
		{
			appNotify("Mass Effect AnimSequence %s/%s has different BioAnimSetData object, removing track",
				Name, *Seq->SequenceName);
			continue;
		}
#endif // MASSEFF
		// some checks
		int offsetsPerBone = 4;
		if (Seq->KeyEncodingFormat == AKF_PerTrackCompression)
			offsetsPerBone = 2;
#if TLR
		if (Package->Game == GAME_TLR) offsetsPerBone = 6;
#endif
#if XMEN
		if (Package->Game == GAME_XMen) offsetsPerBone = 6;		// has additional CutInfo array
#endif
		if (Seq->CompressedTrackOffsets.Num() != NumTracks * offsetsPerBone && !Seq->RawAnimData.Num())
		{
			appNotify("AnimSequence %s/%s has wrong CompressedTrackOffsets size (has %d, expected %d), removing track",
				Name, *Seq->SequenceName, Seq->CompressedTrackOffsets.Num(), NumTracks * offsetsPerBone);
			continue;
		}

		// create CAnimSequence
		CAnimSequence *Dst = new (AnimSet->Sequences) CAnimSequence;
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

// position
#define TP(Enum, VecType)						\
				case Enum:						\
					{							\
						VecType v;				\
						Reader << v;			\
						A->KeyPos.AddItem(CVT(v)); \
					}							\
					break;
// position ranged
#define TPR(Enum, VecType)						\
				case Enum:						\
					{							\
						VecType v;				\
						Reader << v;			\
						FVector v2 = v.ToVector(Mins, Ranges); \
						A->KeyPos.AddItem(CVT(v2)); \
					}							\
					break;
// rotation
#define TR(Enum, QuatType)						\
				case Enum:						\
					{							\
						QuatType q;				\
						Reader << q;			\
						A->KeyQuat.AddItem(CVT(q)); \
					}							\
					break;
// rotation ranged
#define TRR(Enum, QuatType)						\
				case Enum:						\
					{							\
						QuatType q;				\
						Reader << q;			\
						FQuat q2 = q.ToQuat(Mins, Ranges); \
						A->KeyQuat.AddItem(CVT(q2));	\
					}							\
					break;

			// decode AKF_PerTrackCompression data
			if (Seq->KeyEncodingFormat == AKF_PerTrackCompression)
			{
				// this format uses different key storage
				guard(PerTrackCompression);
				assert(Seq->TranslationCompressionFormat == ACF_Identity);
				assert(Seq->RotationCompressionFormat == ACF_Identity);

				int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
				int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+1];

				unsigned PackedInfo;
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
					A->KeyPos.AddItem(nullVec);
#if DEBUG_DECOMPRESS
					appPrintf("    [%d] no translation data\n", j);
#endif
				}
				else
				{
					Reader.Seek(TransOffset);
					Reader << PackedInfo;
					DECODE_PER_TRACK_INFO(PackedInfo);
					A->KeyPos.Empty(NumKeys);
					if (HasTimeTracks) A->KeyPosTime.Empty(NumKeys);
#if DEBUG_DECOMPRESS
					appPrintf("    [%d] trans: fmt=%d (%s), %d keys, mask %d\n", j,
						KeyFormat, EnumToName("AnimationCompressionFormat", KeyFormat), NumKeys, ComponentMask
					);
#endif
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
								A->KeyPos.AddItem(CVT(v));
							}
							break;
						TPR(ACF_IntervalFixed32NoW, FVectorIntervalFixed32)
						case ACF_Fixed48NoW:
							{
								FVectorFixed48 v;
								v.X = v.Y = v.Z = 32767;	// corresponds to 0
								if (ComponentMask & 1) Reader << v.X;
								if (ComponentMask & 2) Reader << v.Y;
								if (ComponentMask & 4) Reader << v.Z;
								FVector v2 = v;				// convert
								float scale = 1.0f / 128;	// here vector is 128 times smaller
								v2.X *= scale;
								v2.Y *= scale;
								v2.Z *= scale;
								A->KeyPos.AddItem(CVT(v2));
							}
							break;
						case ACF_Identity:
							A->KeyPos.AddItem(nullVec);
							break;
						default:
							appError("Unknown translation compression method: %d", KeyFormat);
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
					A->KeyQuat.AddItem(nullQuat);
#if DEBUG_DECOMPRESS
					appPrintf("    [%d] no rotation data\n", j);
#endif
				}
				else
				{
					Reader.Seek(RotOffset);
					Reader << PackedInfo;
					DECODE_PER_TRACK_INFO(PackedInfo);
					A->KeyQuat.Empty(NumKeys);
					if (HasTimeTracks) A->KeyQuatTime.Empty(NumKeys);
#if DEBUG_DECOMPRESS
					appPrintf("    [%d] rot  : fmt=%d (%s), %d keys, mask %d\n", j,
						KeyFormat, EnumToName("AnimationCompressionFormat", KeyFormat), NumKeys, ComponentMask
					);
#endif
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
								A->KeyQuat.AddItem(CVT(q2));
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
								A->KeyQuat.AddItem(CVT(q2));
							}
							break;
						TR (ACF_Fixed32NoW, FQuatFixed32NoW)
						TRR(ACF_IntervalFixed32NoW, FQuatIntervalFixed32NoW)
						TR (ACF_Float32NoW, FQuatFloat32NoW)
						case ACF_Identity:
							A->KeyQuat.AddItem(nullQuat);
							break;
						default:
							appError("Unknown rotation compression method: %d", Seq->RotationCompressionFormat);
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

			// non-AKF_PerTrackCompression block

			// read animations
			int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
			int TransKeys   = Seq->CompressedTrackOffsets[offsetIndex+1];
			int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+2];
			int RotKeys     = Seq->CompressedTrackOffsets[offsetIndex+3];
#if TLR
			int ScaleOffset = 0, ScaleKeys = 0;
			if (Package->Game == GAME_TLR)
			{
				ScaleOffset  = Seq->CompressedTrackOffsets[offsetIndex+4];
				ScaleKeys    = Seq->CompressedTrackOffsets[offsetIndex+5];
			}
#endif // TLR
//			appPrintf("[%d:%d:%d] :  %d[%d]  %d[%d]  %d[%d]\n", j, Seq->RotationCompressionFormat, Seq->TranslationCompressionFormat, TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys);

			A->KeyPos.Empty(TransKeys);
			A->KeyQuat.Empty(RotKeys);
			if (HasTimeTracks)
			{
				A->KeyPosTime.Empty(TransKeys);
				A->KeyQuatTime.Empty(RotKeys);
			}

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
				if (TransKeys == 1)
					TranslationCompressionFormat = ACF_None;	// single key is stored without compression
				// read mins/ranges
				if (TranslationCompressionFormat == ACF_IntervalFixed32NoW)
				{
					assert(Package->ArVer >= 761);
					Reader << Mins << Ranges;
				}
#if BORDERLANDS
				FVector Base;
				if (Package->Game == GAME_Borderlands && (TranslationCompressionFormat == ACF_Delta40NoW || TranslationCompressionFormat == ACF_Delta48NoW))
				{
					Reader << Mins << Ranges << Base;
				}
#endif // BORDERLANDS

#if TRANSFORMERS
				if (Package->Game == GAME_Transformers && TransKeys >= 4)
				{
					assert(Package->ArLicenseeVer >= 100);
					FVector Scale, Offset;
					Reader << Scale.X;
					if (Scale.X != -1)
					{
						Reader << Scale.Y << Scale.Z << Offset;
//						appPrintf("  trans: %g %g %g -- %g %g %g\n", FVECTOR_ARG(Offset), FVECTOR_ARG(Scale));
						for (k = 0; k < TransKeys; k++)
						{
							FPackedVectorTrans pos;
							Reader << pos;
							FVector pos2 = pos.ToVector(Offset, Scale); // convert
							A->KeyPos.AddItem(CVT(pos2));
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
						A->KeyPos.AddItem(nullVec);
						break;
#if BORDERLANDS
					case ACF_Delta48NoW:
						{
							if (k == 0)
							{
								// "Base" works as 1st key
								A->KeyPos.AddItem(CVT(Base));
								continue;
							}
							FVectorDelta48NoW V;
							Reader << V;
							FVector V2;
							V2 = V.ToVector(Mins, Ranges, Base);
							Base = V2;			// for delta
							A->KeyPos.AddItem(CVT(V2));
						}
						break;
#endif // BORDERLANDS
					default:
						appError("Unknown translation compression method: %d", Seq->TranslationCompressionFormat);
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
//				A->KeyPos.AddItem(nullVec);
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
			else if (RotationCompressionFormat == ACF_IntervalFixed32NoW || Package->ArVer < 761)
			{
#if SHADOWS_DAMNED
				if (Package->Game == GAME_ShadowsDamned) goto skip_ranges;
#endif
				// starting with version 761 Mins/Ranges are read only when needed - i.e. for ACF_IntervalFixed32NoW
				Reader << Mins << Ranges;
			skip_ranges: ;
			}
#if BORDERLANDS
			FQuat Base;
			if (Package->Game == GAME_Borderlands && (RotationCompressionFormat == ACF_Delta40NoW || RotationCompressionFormat == ACF_Delta48NoW))
			{
				Reader << Base;			// in addition to Mins and Ranges
			}
#endif // BORDERLANDS
#if TRANSFORMERS
			FQuat TransQuatMod;
			if (Package->Game == GAME_Transformers && RotKeys >= 2)
				Reader << TransQuatMod;
#endif // TRANSFORMERS

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
					A->KeyQuat.AddItem(nullQuat);
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
							A->KeyQuat.AddItem(CVT(Base));
							continue;
						}
						FQuatDelta48NoW q;
						Reader << q;
						FQuat q2;
						q2 = q.ToQuat(Mins, Ranges, Base);
						Base = q2;			// for delta
						A->KeyQuat.AddItem(CVT(q2));
					}
					break;
#endif // BORDERLANDS
#if TRANSFORMERS
				case ACF_IntervalFixed48NoW:
					{
						FQuatIntervalFixed48NoW q;
						FQuat q2;
						Reader << q;
						q2 = q.ToQuat(Mins, Ranges);
						q2 = TransModifyQuat(q2, TransQuatMod);
						A->KeyQuat.AddItem(CVT(q2));
					}
					break;
#endif // TRANSFORMERS
				default:
					appError("Unknown rotation compression method: %d", Seq->RotationCompressionFormat);
				}
			}

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


#endif // UNREAL3
