#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial3.h"

/*-----------------------------------------------------------------------------
	UTexture/UTexture2D (Unreal engine 4)
-----------------------------------------------------------------------------*/

#if UNREAL4

struct FTexturePlatformData
{
	int			SizeX;
	int			SizeY;
	int			NumSlices;				// 1 for simple texture, 6 for cubemap - 6 textures are joined into one (check!!)
	FString		PixelFormat;
	TArray<FTexture2DMipMap> Mips;

	friend FArchive& operator<<(FArchive& Ar, FTexturePlatformData& D)
	{
		Ar << D.SizeX << D.SizeY << D.NumSlices << D.PixelFormat;
		int FirstMip;
		Ar << FirstMip;					// only for cooked, but we don't read FTexturePlatformData for non-cooked textures
		Ar << D.Mips;
		return Ar;
	}
};

void UTexture3::Serialize4(FArchive& Ar)
{
	guard(UTexture3::Serialize4);
	Super::Serialize(Ar);				// UObject

	FStripDataFlags StripFlags(Ar);

	assert(Ar.ArVer >= VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)

	if (!StripFlags.IsEditorDataStripped())
	{
		SourceArt.Serialize(Ar);
	}
	unguard;
}


void UTexture2D::Serialize4(FArchive& Ar)
{
	guard(UTexture2D::Serialize4);

	Super::Serialize4(Ar);

	FStripDataFlags StripFlags(Ar);		// note: these flags are used for pre-VER_UE4_TEXTURE_SOURCE_ART_REFACTOR versions

	int bCooked = 0;
	if (Ar.ArVer >= VER_UE4_ADD_COOKED_TO_TEXTURE2D) Ar << bCooked;

	if (Ar.ArVer < VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)
		appError("VER_UE4_TEXTURE_SOURCE_ART_REFACTOR");

	// Formats are added in UE4 in Source/Developer/<Platform>TargetPlatform/Private/<Platform>TargetPlatform.h,
	// in TTargetPlatformBase::GetTextureFormats(). Formats are choosen depending on platform settings (for example,
	// for Android) or depending on UTexture::CompressionSettings. Windows, Linux and Mac platform uses the same
	// texture format (see FTargetPlatformBase::GetDefaultTextureFormatName()). Other platforms uses different formats,
	// but all of them (except Android) uses single texture format per object.

	if (bCooked)
	{
		FName PixelFormatEnum;
		Ar << PixelFormatEnum;
		while (stricmp(PixelFormatEnum, "None") != 0)
		{
			int SkipOffset;
			Ar << SkipOffset;
			EPixelFormat PixelFormat = (EPixelFormat)NameToEnum("EPixelFormat", PixelFormatEnum);
			//?? check whether we can support this pixel format
			//!! add support for PVRTC textures - for UE3 these formats are mapped to DXT, but
			//!! in UE4 there's separate enums - should support them in UTexture2D::GetTextureData
			if (Format == PF_Unknown)
			{
				appPrintf("Loading data for format %s ...\n", PixelFormatEnum.Str);
				// the texture was not loaded yet
				FTexturePlatformData Data;
				Ar << Data;
				assert(Ar.Tell() == SkipOffset);
				// copy data to UTexture2D
				Exchange(Mips, Data.Mips);		// swap arrays to avoid copying
				SizeX = Data.SizeX;
				SizeY = Data.SizeY;
				Format = PixelFormat;
				//!! NumSlices
			}
			else
			{
				appPrintf("Skipping data for format %s\n", PixelFormatEnum.Str);
				Ar.Seek(SkipOffset);
			}
			// read next format name
			Ar << PixelFormatEnum;
		}
	}

	unguard;
}

#endif // UNREAL4
