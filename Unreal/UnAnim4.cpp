#include "Core.h"

#if UNREAL4

#include "UnrealClasses.h"
#include "UnMesh4.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"

#include "SkeletalMesh.h"
#include "TypeConvert.h"

#define DEBUG_DECOMPRESS	1

/*-----------------------------------------------------------------------------
	USkeleton
-----------------------------------------------------------------------------*/

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

	if (Ar.ArVer >= VER_UE4_SKELETON_ADD_SMARTNAMES)
		Ar << SmartNames;

	unguard;
}


void USkeleton::PostLoad()
{
	guard(USkeleton::PostLoad);

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

	unguard;
}


#if DEBUG_DECOMPRESS
#define DBG(...)			appPrintf(__VA_ARGS__)
#else
#define DBG(...)
#endif

void USkeleton::ConvertAnims()
{
	guard(USkeleton::ConvertAnims);

	int i, j;

	CAnimSet *AnimSet = new CAnimSet(this);
	ConvertedAnim = AnimSet;

	// Copy bone names
	AnimSet->TrackBoneNames.Empty(ReferenceSkeleton.RefBoneInfo.Num());
	for (i = 0; i < ReferenceSkeleton.RefBoneInfo.Num(); i++)
	{
		AnimSet->TrackBoneNames.Add(ReferenceSkeleton.RefBoneInfo[i].Name);
	}

	//TODO: verify if UE4 has AnimRotationOnly stuff
	AnimSet->AnimRotationOnly = false;

	DBG("----------- Skeleton %s: %d seq, %d bones -----------\n", Name, Anims.Num(), ReferenceSkeleton.RefBoneInfo.Num());

	for (i = 0; i < Anims.Num(); i++)
	{
		const UAnimSequence4 *Seq = Anims[i];
		int NumTracks = Seq->GetNumTracks();

#if DEBUG_DECOMPRESS
		appPrintf("Sequence %d (%s):%s %d bones, %d offsets (%g per bone), %d frames, %d compressed data\n"
			   "          trans %s, rot %s, scale %s, key %s\n",
			i, Seq->Name,
			"", //?? Seq->bIsAdditive ? " [additive]" : "",
			NumTracks, Seq->CompressedTrackOffsets.Num(), Seq->CompressedTrackOffsets.Num() / (float)NumTracks,
			Seq->NumFrames,
			Seq->CompressedByteStream.Num(),
			EnumToName("AnimationCompressionFormat", Seq->TranslationCompressionFormat),
			EnumToName("AnimationCompressionFormat", Seq->RotationCompressionFormat),
			EnumToName("AnimationCompressionFormat", Seq->ScaleCompressionFormat),
			EnumToName("AnimationKeyFormat",         Seq->KeyEncodingFormat)
		);
		for (int i2 = 0; i2 < Seq->CompressedTrackOffsets.Num(); /*empty*/)
		{
			if (Seq->KeyEncodingFormat != AKF_PerTrackCompression)
			{
				FName BoneName = ReferenceSkeleton.RefBoneInfo[Seq->GetTrackBoneIndex(i2/4)].Name;
				int TransOffset = Seq->CompressedTrackOffsets[i2  ];
				int TransKeys   = Seq->CompressedTrackOffsets[i2+1];
				int RotOffset   = Seq->CompressedTrackOffsets[i2+2];
				int RotKeys     = Seq->CompressedTrackOffsets[i2+3];
				appPrintf("    [%d] = trans %d[%d] rot %d[%d] - %s\n", i2/4, TransOffset, TransKeys, RotOffset, RotKeys, *BoneName);
				i2 += 4;
			}
			else
			{
				FName BoneName = ReferenceSkeleton.RefBoneInfo[Seq->GetTrackBoneIndex(i2/2)].Name;
				int TransOffset = Seq->CompressedTrackOffsets[i2  ];
				int RotOffset   = Seq->CompressedTrackOffsets[i2+1];
				appPrintf("    [%d] = trans %d rot %d - %s\n", i2/2, TransOffset, RotOffset, *BoneName);
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
			appNotify("AnimSequence %s/%s has wrong CompressedTrackOffsets size (has %d, expected %d), removing track",
				Name, Seq->Name, Seq->CompressedTrackOffsets.Num(), NumTracks * offsetsPerBone);
			continue;
		}

		// create CAnimSequence
		CAnimSequence *Dst = new (AnimSet->Sequences) CAnimSequence;
		Dst->Name      = Seq->Name;
		Dst->NumFrames = Seq->NumFrames;
		Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;





	}

	unguard;
}


/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

void UAnimSequence4::Serialize(FArchive& Ar)
{
	guard(UAnimSequence4::Serialize);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);
	if (!StripFlags.IsEditorDataStripped())
	{
		Ar << RawAnimationData;
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

			// compressed data
			Ar << CompressedByteStream;
			Ar << bUseRawDataOnly;
		}
	}

	unguard;
}


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


#endif // UNREAL4
