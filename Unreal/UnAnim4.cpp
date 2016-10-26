#include "Core.h"

#if UNREAL4

#include "UnrealClasses.h"
#include "UnMesh4.h"
#include "UnMeshTypes.h"

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

#endif // UNREAL4
