#include "Core.h"

#if UNREAL3 && BATMAN

#include "UnrealClasses.h"
#include "UnMesh3.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"

#include "SkeletalMesh.h"
#include "TypeConvert.h"


//#define DEBUG_DECOMPRESS		1

/*!!---------------------------------------------------------------------------
BUGS:
- Batman 4:
  t -path=data/3/Batman4 -notex Film_B1_Cine_Ch23_Gordon_Flashback.upk Gordon_BM3_Skin_Head -anim=Baked_CH02_Gordon_Flashback_BM_FACE_0
    face mesh, with bad looking animation - verified everything, works correctly,
    but the mesh still looking badly
  t -path=data/3/Batman4 -notex Ace_A1 ACE_A1_PlatformDestruction -anim=Baked_ACE_A1_PlatformDestruction_Anims_0
    this mesh has AnimRotationOnly=true, but should be false; there's some object
    quickly flying around while animating - perhaps design bug?
- CHECK: Catwoman animation where she is hanging on her arms (arms are "up"):
  shoulders are ugly - probably this could be fixed with animation of Twist
  bones
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	Batman 2+ specific mesh code
-----------------------------------------------------------------------------*/

/*
TODO:
- check EnableTwistBoneFixers and EnableClavicleFixer vars in USkeletalMesh
  (set to true by defaultproperties, may be false somewhere)
  - verify - may be there is no meshes with disabled fixers at all
* move these bones into correct hierarchy (will require to recompute bindpose)
- compute tracks for Twist bones in UAnimSet (no "enable fixer" vars in UAnimSet/
  UAnimSequence, so compute them for missing tracks?)
  - fithout computing: these bones will not be animated and their positions will
    force mesh to be deformed incorrectly (a bit)
*/

struct Bat2Fixer
{
	const char *Bone;
	const char *NewParent;
};

static const Bat2Fixer FixNames[] =
{
	{ "Bip01_RThighTwist", "Bip01_R_Thigh"    },
	{ "Bip01_RCalfTwist",  "Bip01_R_Calf"     },
	{ "Bip01_LThighTwist", "Bip01_L_Thigh"    },
	{ "Bip01_LCalfTwist",  "Bip01_L_Calf"     },
	{ "Bip01_RUpArmTwist", "Bip01_R_UpperArm" },
	{ "Bip01_R_Foretwist", "Bip01_R_Forearm"  },
	{ "Bip01_LUpArmTwist", "Bip01_L_UpperArm" },
	{ "Bip01_L_Foretwist", "Bip01_L_Forearm"  },
};


void USkeletalMesh3::FixBatman2Skeleton()
{
	/*
		Batman 2 has 2 Twist bones for thigh, calf, forearm and upperarm. These bones
		are animated proceduraly in game (for less animation memory footprint?) so
		these bones has no tracks in UAnimSet. Unfortunately, these bones are attached
		to incorrect parent bone (legs has separate hierarchy at all!) so we must
		reattach thesee bones to a better place.
	*/

	//?? use EnableTwistBoneFixers and EnableClavicleFixer vars
	CSkeletalMesh *Mesh = ConvertedMesh;
	CCoords Coords[MAX_MESHBONES];
	int i;
	int NumBones = Mesh->RefSkeleton.Num();

	// compute coordinates for all bones
	//!! code similar to CSkelMeshInstance::SetMesh()
	for (i = 0; i < NumBones; i++)
	{
		CSkelMeshBone &B = Mesh->RefSkeleton[i];
		assert(B.ParentIndex <= i);
		CVec3 BP;
		CQuat BO;
		BP = B.Position;
		BO = B.Orientation;
		if (!i) BO.Conjugate();
		CCoords &BC = Coords[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		if (i)	// do not rotate root bone
			Coords[B.ParentIndex].UnTransformCoords(BC, BC);
	}

	int NumFixedBones = 0;

	// verify all bones
	for (i = 0; i < NumBones; i++)
	{
		CSkelMeshBone &B = Mesh->RefSkeleton[i];
		int j;

		// find Bat2Fixer entry
		const Bat2Fixer *Fix = NULL;
		for (j = 0; j < ARRAY_COUNT(FixNames); j++)
		{
			if (!stricmp(B.Name, FixNames[j].Bone))
			{
				Fix = &FixNames[j];
				break;
			}
		}
		if (!Fix) continue;

		// should move bone in hierarchy
		// find new parent bone
		int NewParent = Mesh->FindBone(Fix->NewParent);
		if (NewParent < 0) continue;	// not found

		// change parent
		B.ParentIndex = NewParent;
		// recompute bone position
		CCoords BC;
		Coords[NewParent].TransformCoords(Coords[i], BC);
		B.Position    = BC.origin;
		B.Orientation.FromAxis(BC.axis);

		NumFixedBones++;
	}

	Mesh->SortBones();

	if (NumFixedBones > 0) appPrintf("Fixed Batman skeleton: %d/%d bones\n", NumFixedBones, NumBones);
}


/*-----------------------------------------------------------------------------
	Batman 2 specific animation code
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------

Batman2 research:
=================

AnimZipData:
+00 offset of single rotation track (< 0 -> no track)
+04 offset of single translation track (< 0 -> no track)
+08 rotation track count
+0C rotation tracks offset -> TrackData[count]
+10 translation track count
+14 translation tracks offset -> TrackData[count]

TrackData:
+0 (b) compression method - EAnimZipRotationCodec or EAnimZipTranslationScaleCodec
+1 (b) number of bones stored in this TrackData
+2 (w) number of keys
+4 (d) offset of bone indices
+8 (d) offset of compressed data

EAnimZipTranslationScaleCodec:
0.	AZTSC_Float_128						-- FVector+float
1.	AZTSC_NoScale_Float_96				-- FVector
2.	AZTSC_NoScale_Interval_Fixed_48		-- packed vector + FVector[2] as interval
3.	AZTSC_NoScale_Interval_Fixed_24		-- packed vector + FVector[2] as interval

EAnimZipRotationCodec:
0.	AZRC_QuatMax_48
1.	AZRC_QuatMax_40
2.	AZRC_QuatRelative_32				-- has interval - byte[10]
3.	AZRC_QuatRelative_24				-- ...
4.	AZRC_QuatRelative_16				-- ...
5.	AZRC_FixedAxis_16					-- has interval - byte[3]
6.	AZRC_FixedAxis_8					-- ...

All keys are equally spaced on time track.
Each TrackData has a set of bones stored in its data. Each bone represented as byte
index stored at <offset of bone indices> position.

Interval data stored immediately after bone indices, i.e. at position
<offset of bone indices> + <number of bones>. Each bone has own interval set.

Batman4 has more than 256 animated bones, so layout of data structures is
a bit different.

-----------------------------------------------------------------------------*/


struct FAnimZipHeader
{
	// "single track" - camera? not used here
	int						SingleRotationOffset;		// < 0 = no track
	int						SingleTranslationOffset;	// < 0 = no track
	int						RotationCount;
	int						RotationOffset;
	int						TranslationCount;
	int						TranslationOffset;

	friend FArchive& operator<<(FArchive &Ar, FAnimZipHeader &H)
	{
		return Ar << H.SingleRotationOffset << H.SingleTranslationOffset
				  << H.RotationCount << H.RotationOffset
				  << H.TranslationCount << H.TranslationOffset;
	}
};

enum EAnimZipTranslationScaleCodec
{
	AZTSC_Float_128,
	AZTSC_NoScale_Float_96,
	AZTSC_NoScale_Interval_Fixed_48,
	AZTSC_NoScale_Interval_Fixed_24,
	AZTSC_MAX,
};

#if ANIM_DEBUG_INFO
static const char* TranslationCodecName[] =
{
	"Float_128",
	"NoScale_Float_96",
	"NoScale_Interval_Fixed_48",
	"NoScale_Interval_Fixed_24",
};
#endif

enum EAnimZipRotationCodec
{
	AZRC_QuatMax_48,
	AZRC_QuatMax_40,
	AZRC_QuatRelative_32,
	AZRC_QuatRelative_24,
	AZRC_QuatRelative_16,
	AZRC_FixedAxis_16,
	AZRC_FixedAxis_8,
	AZRC_MAX,
};

#if ANIM_DEBUG_INFO
static const char* RotationCodecName[] =
{
	"QuatMax_48",
	"QuatMax_40",
	"QuatRelative_32",
	"QuatRelative_24",
	"QuatRelative_16",
	"FixedAxis_16",
	"FixedAxis_8",
};
#endif

struct FAnimZipTrack
{
	uint16					NumBones;
	uint16					NumKeys;
	int						BoneInfoOffset;
	int						CompressedDataOffset;
	byte					Codec;

	friend FArchive& operator<<(FArchive &Ar, FAnimZipTrack &T)
	{
		if (Ar.Game < GAME_Batman4)
		{
			T.NumBones = 0;
			Ar << T.Codec;
			Ar << (byte&)T.NumBones;		// 8-bit bone index
			Ar << T.NumKeys;
			Ar << T.BoneInfoOffset;
			Ar << T.CompressedDataOffset;
		}
		else
		{
			assert(Ar.Game == GAME_Batman4);
			Ar << T.NumBones;				// 16-bit bone index
			Ar << T.NumKeys;
			Ar << T.BoneInfoOffset;
			Ar << T.CompressedDataOffset;
			int Codec;
			Ar << Codec;
			T.Codec = Codec & 0xFF;
		}
		return Ar;
	}
};


static int FindTrack(int Bone, FArchive &Ar, FAnimZipTrack &T, int Pos, int Count, UAnimSet *Owner, const char *What)
{
	guard(FindTrack);

	int TrackIndex = -1;
	int i;
	for (i = 0; (i < Count) && (TrackIndex < 0); i++)
	{
		// read header
		Ar.Seek(Pos);
		Ar << T;
		Pos = Ar.Tell();
		// find bone
		Ar.Seek(T.BoneInfoOffset);
		if (Ar.Game < GAME_Batman4)
		{
			// find bone in byte array
			for (int b = 0; b < T.NumBones; b++)
			{
				byte b0;
				Ar << b0;
				if (b0 == Bone)
				{
					TrackIndex = b;
					break;
				}
			}
		}
		else
		{
			// find bone in uint16 array
			for (int b = 0; b < T.NumBones; b++)
			{
				uint16 b0;
				Ar << b0;
				if (b0 == Bone)
				{
					TrackIndex = b;
					break;
				}
			}
		}
	}

#if DEBUG_DECOMPRESS
	if (TrackIndex < 0)
	{
//		appPrintf("bone %d (%s): %s not found\n", Bone, *Owner->TrackBoneNames[Bone], What);
	}
	else
	{
		appPrintf("bone %d (%s): %s at %d[%d.%d], codec = %d, keys = %d\n", Bone, *Owner->TrackBoneNames[Bone], What, T.CompressedDataOffset, i, TrackIndex, T.Codec, T.NumKeys);
	}
#endif

	return TrackIndex;

	unguard;
}


// see FQuatFixed48Max for more information
static CQuat FinishQuatMax(int Shift, int H, int M, int L, int S)
{
	float scale0 = 1.0f / ((1 << Shift) - 1);
	static const float shift = 0.70710678118f;		// sqrt(0.5)
	static const float scale = 1.41421356237f;		// sqrt(0.5)*2
	scale0 *= scale;
	float l = L * scale0 - shift;
	float m = M * scale0 - shift;
	float h = H * scale0 - shift;
	float a = sqrt(1.0f - (l*l + m*m + h*h));

	CQuat r;
	switch (S)			// choose where to place "a"
	{
	case 0:
		r.Set(a, m, h, l);
		break;
	case 1:
		r.Set(m, a, h, l);
		break;
	case 2:
		r.Set(m, h, a, l);
		break;
	default:
		r.Set(m, h, l, a);
		break;
	}
	return r;
}


static CQuat FinishQuatRelative(int Shift, unsigned X, unsigned Y, unsigned Z, char *Interval)
{
	CQuat Base, Delta;

	Base.x = Interval[0] / 127.0f;
	Base.y = Interval[1] / 127.0f;
	Base.z = Interval[2] / 127.0f;
	Base.w = Interval[3] / 127.0f;
	Base.Normalize();

	float Scale = 1.0f / ((1 << Shift) - 1);
	Delta.x = X * Scale * ((byte)Interval[7]) / 127.5f + Interval[4] / 127.0f;
	Delta.y = Y * Scale * ((byte)Interval[8]) / 127.5f + Interval[5] / 127.0f;
	Delta.z = Z * Scale * ((byte)Interval[9]) / 127.5f + Interval[6] / 127.0f;
	float wSq = 1.0f - (Delta.x*Delta.x + Delta.y*Delta.y + Delta.z*Delta.z);
	Delta.w = (wSq > 0) ? sqrt(wSq) : 0;

	Base.Mul(Delta);
	return Base;
}


static CQuat FinishQuatFixedAxis(int Shift, unsigned Value, byte *Interval)
{
	CQuat r;
	r.Set(0, 0, 0, 0);
	switch (Interval[0])
	{
	case 0:
		r.x = 1;
		break;
	case 1:
		r.y = 1;
		break;
	default:
//	case 2:
		r.z = 1;
		break;
	}
	static const float Scale = M_PI * 2 / 255;
	float Angle = (Interval[1] * Scale + Value * Interval[2] * Scale / ((1 << Shift) - 1)) * 0.5f;
	float AngleSin = sin(Angle);

	r.x *= AngleSin;
	r.y *= AngleSin;
	r.z *= AngleSin;
	r.w = cos(Angle);
	return r;
}


static int FindAndDecodeRotation(int Bone, FArchive &Ar, CAnimTrack &Track, int Pos, int Count, UAnimSet *Owner)
{
	guard(FindAndDecodeRotation);

	FAnimZipTrack T;
	int TrackIndex = FindTrack(Bone, Ar, T, Pos, Count, Owner, "rotation");
	if (TrackIndex < 0) return -1;

	int IntervalDataOffset;
	if (Ar.Game < GAME_Batman4)
	{
		IntervalDataOffset = T.BoneInfoOffset + T.NumBones;		// bone index is byte
	}
	else
	{
		IntervalDataOffset = T.BoneInfoOffset + T.NumBones * 2;	// bone index is uint16
	}
	Track.KeyQuat.Empty(T.NumKeys);

	switch (T.Codec)
	{
	case AZRC_QuatMax_48:
		{
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 6);
				byte b[6];
				Ar.Serialize(b, 6);
				CQuat q = FinishQuatMax(
					15,
					((b[2] << 8) | b[3]) & 0x7FFF,			// 2nd 2 bytes, big-endian
					((b[0] << 8) | b[1]) & 0x7FFF,			// 1st 2 bytes, big-endian
					((b[4] << 8) | b[5]) & 0x7FFF,			// last 2 bytes
					((b[2] >> 6) & 2) | (b[4] >> 7)
				);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	case AZRC_QuatMax_40:
		{
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 5);
				byte b[5];
				Ar.Serialize(b, 5);							// AAB.BCC.DDE.E
				CQuat q = FinishQuatMax(
					12,
					 ((b[1] << 8) | b[2]) & 0xFFF,			// BCC
					(((b[0] << 8) | b[1]) >> 4) & 0xFFF,	// AAB
					(((b[3] << 8) | b[4]) >> 4) & 0xFFF,	// DDE
					b[4] & 3								// E
				);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	case AZRC_QuatRelative_32:
		{
			Ar.Seek(IntervalDataOffset + 10 * TrackIndex);
			char Interval[10];		// signed
			Ar.Serialize(Interval, 10);
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 4);
				byte b[4];
				Ar.Serialize(b, 4);
				unsigned val32 = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
				CQuat q = FinishQuatRelative(10, (val32 >> 20) & 0x3FF, (val32 >> 10) & 0x3FF, val32 & 0x3FF, Interval);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	case AZRC_QuatRelative_24:
		{
			Ar.Seek(IntervalDataOffset + 10 * TrackIndex);
			char Interval[10];		// signed
			Ar.Serialize(Interval, 10);
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 3);
				byte b[3];
				Ar.Serialize(b, 3);
				CQuat q = FinishQuatRelative(8, b[0], b[1], b[2], Interval);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	case AZRC_QuatRelative_16:
		{
			Ar.Seek(IntervalDataOffset + 10 * TrackIndex);
			char Interval[10];		// signed
			Ar.Serialize(Interval, 10);
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 2);
				byte b[2];
				Ar.Serialize(b, 2);
				unsigned val32 = (b[0] << 8) | b[1];
				CQuat q = FinishQuatRelative(5, (val32 >> 10) & 0x1F, (val32 >> 5) & 0x1F, val32 & 0x1F, Interval);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	case AZRC_FixedAxis_16:
		{
			Ar.Seek(IntervalDataOffset + 3 * TrackIndex);
			byte Interval[3];
			Ar.Serialize(Interval, 3);
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 2);
				uint16 w;
				Ar << w;
				CQuat q = FinishQuatFixedAxis(16, w, Interval);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	case AZRC_FixedAxis_8:
		{
			Ar.Seek(IntervalDataOffset + 3 * TrackIndex);
			byte Interval[3];
			Ar.Serialize(Interval, 3);
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 1);
				byte b;
				Ar << b;
				CQuat q = FinishQuatFixedAxis(8, b, Interval);
				Track.KeyQuat.Add(q);
			}
		}
		break;

	default:
		appError("Unknown rotation codec %d", T.Codec);
	}

	return T.Codec;

	unguard;
}


static int FindAndDecodeTranslation(int Bone, FArchive &Ar, CAnimTrack &Track, int Pos, int Count, UAnimSet *Owner)
{
	guard(FindAndDecodeTranslation);

	FAnimZipTrack T;
	int TrackIndex = FindTrack(Bone, Ar, T, Pos, Count, Owner, "translation");
	if (TrackIndex < 0) return -1;

	int IntervalDataOffset;
	if (Ar.Game < GAME_Batman4)
	{
		IntervalDataOffset = T.BoneInfoOffset + T.NumBones;		// bone index is byte
	}
	else
	{
		IntervalDataOffset = T.BoneInfoOffset + T.NumBones * 2;	// bone index is uint16
	}
	Track.KeyPos.Empty(T.NumKeys);

	switch (T.Codec)
	{
	case AZTSC_Float_128:
		{
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 16);
				FVector v;
				float	f;
				Ar << v << f;
				Track.KeyPos.Add(CVT(v));
			}
		}
		break;

	case AZTSC_NoScale_Float_96:
		{
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 12);
				FVector v;
				Ar << v;
				Track.KeyPos.Add(CVT(v));
			}
		}
		break;

	case AZTSC_NoScale_Interval_Fixed_48:
		{
			// read interval
			Ar.Seek(IntervalDataOffset + TrackIndex * 12 * 2);
			FVector Mins, Ranges;
			Ar << Mins << Ranges;
			// read track
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 6);
				int16 vi[3];		// signed
				Ar << vi[0] << vi[1] << vi[2];
				FVector v;
				v.X = vi[0] / 32767.0f * Ranges.X + Mins.X;
				v.Y = vi[1] / 32767.0f * Ranges.Y + Mins.Y;
				v.Z = vi[2] / 32767.0f * Ranges.Z + Mins.Z;
				Track.KeyPos.Add(CVT(v));
			}
		}
		break;

	case AZTSC_NoScale_Interval_Fixed_24:
		{
			// read interval
			Ar.Seek(IntervalDataOffset + TrackIndex * 12 * 2);
			FVector Mins, Ranges;
			Ar << Mins << Ranges;
			// read track
			for (int i = 0; i < T.NumKeys; i++)
			{
				Ar.Seek(T.CompressedDataOffset + (TrackIndex + T.NumBones * i) * 3);
				char vi[3];			// signed
				Ar << vi[0] << vi[1] << vi[2];
				FVector v;
				v.X = vi[0] / 127.0f * Ranges.X + Mins.X;
				v.Y = vi[1] / 127.0f * Ranges.Y + Mins.Y;
				v.Z = vi[2] / 127.0f * Ranges.Z + Mins.Z;
				Track.KeyPos.Add(CVT(v));
			}
		}
		break;

	default:
		appError("Unknown translation codec %d", T.Codec);
	}

	return T.Codec;

	unguard;
}


void UAnimSequence::DecodeBatman2Anims(CAnimSequence *Dst, UAnimSet *Owner) const
{
	guard(UAnimSequence::DecodeBatman2Anims);

#if DEBUG_DECOMPRESS
	appPrintf("\nProcessing track %s\n", *SequenceName);
#endif

	FMemReader Reader(AnimZip_Data.GetData(), AnimZip_Data.Num());
	Reader.SetupFrom(*Package);

	FAnimZipHeader Hdr;
	Reader << Hdr;

	int NumTracks = Owner->TrackBoneNames.Num();
	Dst->Tracks.Empty(NumTracks);

#if ANIM_DEBUG_INFO
	int RotationCodecUsed[AZRC_MAX];
	int TranslationCodecUsed[AZTSC_MAX];
	memset(&RotationCodecUsed, 0, sizeof(RotationCodecUsed));
	memset(&TranslationCodecUsed, 0, sizeof(TranslationCodecUsed));
#endif

	int Bone;
	for (Bone = 0; Bone < NumTracks; Bone++)
	{
		// decompress track
		CAnimTrack *Track = new (Dst->Tracks) CAnimTrack;
		int RotationCodec = FindAndDecodeRotation(Bone, Reader, *Track, Hdr.RotationOffset, Hdr.RotationCount, Owner);
		int TranslationCodec = FindAndDecodeTranslation(Bone, Reader, *Track, Hdr.TranslationOffset, Hdr.TranslationCount, Owner);
		// fix quaternions
		if (Bone > 0)
		{
			for (int i = 0; i < Track->KeyQuat.Num(); i++)
				Track->KeyQuat[i].Conjugate();
		}
#if ANIM_DEBUG_INFO
		if (RotationCodec >= 0) RotationCodecUsed[RotationCodec]++;
		if (TranslationCodec >= 0) TranslationCodecUsed[TranslationCodec]++;
#endif
	}

#if ANIM_DEBUG_INFO
	char Buffer[1024];
	Buffer[0] = 0;
	appStrcatn(ARRAY_ARG(Buffer), S_RED "\n  Rotation:\n" S_WHITE);
	for (int i = 0; i < ARRAY_COUNT(RotationCodecUsed); i++)
	{
		if (RotationCodecUsed[i] > 0) appStrcatn(ARRAY_ARG(Buffer), va("    %s [%d]\n", RotationCodecName[i], RotationCodecUsed[i]));
	}
	appStrcatn(ARRAY_ARG(Buffer), S_RED "  Translation:\n" S_WHITE);
	for (int i = 0; i < ARRAY_COUNT(TranslationCodecUsed); i++)
	{
		if (TranslationCodecUsed[i] > 0) appStrcatn(ARRAY_ARG(Buffer), va("    %s [%d]\n", TranslationCodecName[i], TranslationCodecUsed[i]));
	}
	Dst->DebugInfo = Buffer;
#endif // ANIM_DEBUG_INFO

#if 0
	// fix some missing tracks
	for (Bone = 0; Bone < NumTracks; Bone++)
	{
		if (Dst->Tracks[Bone].HasKeys()) continue;

		// find bone name which track will be used instead
		const char *BoneName = Owner->TrackBoneNames[Bone];
		const char *Replacement = NULL;
		for (int i = 0; i < ARRAY_COUNT(FixNames); i++)
		{
			if (!stricmp(BoneName, FixNames[i].Src))
			{
				Replacement = FixNames[i].Dst;
				break;
			}
		}
		if (!Replacement) continue;
		// find bone index
		int ReplaceBone = -1;
		for (int i = 0; i < NumTracks; i++)
			if (!stricmp(Replacement, Owner->TrackBoneNames[i]))
			{
				ReplaceBone = i;
				break;
			}
		if (ReplaceBone < 0) continue;
		// copy track
#if DEBUG_DECOMPRESS
		appPrintf("... copying track: %s (%d) -> %s (%d)\n", Replacement, ReplaceBone, BoneName, Bone);
#endif
		Dst->Tracks[Bone].CopyFrom(Dst->Tracks[ReplaceBone]);
	}
#endif

	unguard;
}


#endif // UNREAL3 && BATMAN
