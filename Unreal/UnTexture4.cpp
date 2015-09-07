#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial3.h"
#include "UnPackage.h"


//#define DEBUG_TEX		1

#if DEBUG_TEX
#define DBG(...)			appPrintf(__VA_ARGS__);
#else
#define DBG(...)
#endif



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
		DBG("   SizeX=%d SizeY=%d NumSlices=%d PixelFormat=%s FirstMip=%d\n", D.SizeX, D.SizeY, D.NumSlices, *D.PixelFormat, FirstMip);
		Ar << D.Mips;
		return Ar;
	}
};

void UTexture3::Serialize4(FArchive& Ar)
{
	guard(UTexture3::Serialize4);
	Super::Serialize(Ar);				// UObject

	FStripDataFlags StripFlags(Ar);

	// For version prior to VER_UE4_TEXTURE_SOURCE_ART_REFACTOR, UTexture::Serialize will do exactly the same
	// serialization action, but it will perform some extra processing of format and compression settings.
	// Cooked packages are not affected by this code.

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

	bool bCooked = false;
	if (Ar.ArVer >= VER_UE4_ADD_COOKED_TO_TEXTURE2D) Ar << bCooked;

	if (Ar.ArVer < VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)
	{
		appNotify("Untested code: UTexture2D::LegacySerialize");
		// This code lives in UTexture2D::LegacySerialize(). It relies on some depracated properties, and modern
		// code UE4 can't read cooked packages prepared with pre-VER_UE4_TEXTURE_SOURCE_ART_REFACTOR version of
		// the engine. So, it's not possible to know what should happen there unless we'll get some working game
		// which uses old UE4 version.bDisableDerivedDataCache_DEPRECATED in UE4 serialized as property, when set
		// to true - has serialization of TArray<FTexture2DMipMap>. We suppose here that it's 'false'.
		FGuid TextureFileCacheGuid_DEPRECATED;
		Ar << TextureFileCacheGuid_DEPRECATED;
	}

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

void UMaterial3::ScanForTextures()
{
	guard(UMaterial3::ScanForTextures);

//	printf("--> %d imports\n", Package->Summary.ImportCount);
	//!! NOTE: this code will not work when textures are located in the same package - they don't present in import table
	//!! but could be found in export table. That's true for Simplygon-generated materials.
	for (int i = 0; i < Package->Summary.ImportCount; i++)
	{
		const FObjectImport &Imp = Package->GetImport(i);
//		printf("--> import %d (%s)\n", i, *Imp.ClassName);
		if (stricmp(Imp.ClassName, "Class") == 0 || stricmp(Imp.ClassName, "Package") == 0)
			continue;		// avoid spamming to log
		UObject* obj = Package->CreateImport(i);
//		if (obj) printf("--> %s (%s)\n", obj->Name, obj->GetClassName());
		if (obj && obj->IsA("Texture3"))
			ReferencedTextures.Add(static_cast<UTexture3*>(obj));
	}

	unguard;
}

#endif // UNREAL4
