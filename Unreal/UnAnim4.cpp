#include "Core.h"

#if UNREAL4

#include "UnrealClasses.h"
#include "UnMesh4.h"
#include "UnMeshTypes.h"

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
