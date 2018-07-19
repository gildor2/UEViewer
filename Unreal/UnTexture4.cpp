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

void FTexture2DMipMap::Serialize4(FArchive &Ar, FTexture2DMipMap& Mip)
{
	guard(FTexture2DMipMap::Serialize4);

	bool cooked = false;
	if (Ar.ArVer >= VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)
		Ar << cooked;

	//?? Can all 'Mip.Data.Skip(Ar)' here, but ensure any mip types will be loaded by LoadBulkTexture().
	//?? Calling Skip() will eliminate extra seeks when interleaving reading of FTexture2DMipMap and
	//?? bulk data which located in the same uasset, but at different position.
	//?? To see the problem (performance problem): load data from pak, modify FPakFile, add logging of
	//?? Seek() and decompression calls. You'll see that loading of big bulk data chinks is interleaved
	//?? with reading 4-byte ints at different locations.
	Mip.Data.Serialize(Ar);
	Ar << Mip.SizeX << Mip.SizeY;
	if (Ar.Game >= GAME_UE4(20))
	{
		int32 SizeZ;
		Ar << SizeZ;
	}
	if (Ar.ArVer >= VER_UE4_TEXTURE_DERIVED_DATA2 && !cooked)
	{
		FString DerivedDataKey;
		Ar << DerivedDataKey;
	}
#if 0
	// Oculus demos ("Henry") can have extra int here
	int tmp;
	Ar << tmp;
#endif

	unguard;
}


struct FTexturePlatformData
{
	int32		SizeX;
	int32		SizeY;
	int32		NumSlices;				// 1 for simple texture, 6 for cubemap - 6 textures are joined into one (check!!)
	FString		PixelFormat;
	TArray<FTexture2DMipMap> Mips;

	friend FArchive& operator<<(FArchive& Ar, FTexturePlatformData& D)
	{
		// see TextureDerivedData.cpp, SerializePlatformData()
		guard(FTexturePlatformData<<);
		Ar << D.SizeX << D.SizeY << D.NumSlices;

#if GEARS4
		if (Ar.Game == GAME_Gears4)
		{
			FName PixelFormatName;
			Ar << PixelFormatName;
			D.PixelFormat = *PixelFormatName;
			goto after_pixel_format;
		}
#endif // GEARS4
		Ar << D.PixelFormat;

	after_pixel_format:
		int FirstMip;
		Ar << FirstMip;					// only for cooked, but we don't read FTexturePlatformData for non-cooked textures
		DBG("   SizeX=%d SizeY=%d NumSlices=%d PixelFormat=%s FirstMip=%d\n", D.SizeX, D.SizeY, D.NumSlices, *D.PixelFormat, FirstMip);
		D.Mips.Serialize2<FTexture2DMipMap::Serialize4>(Ar);
		return Ar;
		unguard;
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
			int32 SkipOffset;
			Ar << SkipOffset;
			if (Ar.Game >= GAME_UE4(20))
			{
				int32 SkipOffsetH;
				Ar << SkipOffsetH;
				assert(SkipOffsetH == 0);
			}

			EPixelFormat PixelFormat = (EPixelFormat)NameToEnum("EPixelFormat", PixelFormatEnum);

			if (Format == PF_Unknown)
			{
				//?? check whether we can support this pixel format
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
	else if (SourceArt.BulkData != NULL)
	{
		guard(UTexture4::LoadSourceData);

		//!! WARNING: this code in theory is common for all UTexture types, but we're working with mips here,
		//!! so it is implemented only for UTexture2D.
		EPixelFormat NewPixelFormat = PF_Unknown;
		int BytesPerPixel = 0;
		const char* FormatName = EnumToName(Source.Format);
		switch (Source.Format)
		{
		case TSF_G8:
			BytesPerPixel = 1;
			NewPixelFormat = PF_G8;
			break;
		case TSF_BGRA8:
		case TSF_RGBA8:
			BytesPerPixel = 4;
			NewPixelFormat = PF_B8G8R8A8;
			break;
		}
		Format = NewPixelFormat;
		SizeX = Source.SizeX;
		SizeY = Source.SizeY;

		appPrintf("... Loading SourceArt: %s, NumMips=%d, Slices=%d, PNGCompressed=%d\n", FormatName, Source.NumMips, Source.NumSlices, Source.bPNGCompressed);

		if (Source.NumSlices == 1 && Source.bPNGCompressed == false && BytesPerPixel > 0)
		{
			// Cubemaps and PNG images are not supported
			int MipSizeX = SizeX;
			int MipSizeY = SizeY;
			int MipOffset = 0;

			Mips.AddDefaulted(Source.NumMips);
			const byte* SourceData = SourceArt.BulkData;
			int SourceDataSize = SourceArt.ElementCount;
//			appPrintf("SourceDataSize = %X\n", SourceDataSize);
			for (int MipIndex = 0; MipIndex < Source.NumMips; MipIndex++, MipSizeX >>= 1, MipSizeY >>= 1)
			{
				// Don't allow Size to go to zero.
				if (MipSizeX == 0) MipSizeX = 1;
				if (MipSizeY == 0) MipSizeY = 1;
				// Copy mip data to UTexture2D.
				FTexture2DMipMap& Mip = Mips[MipIndex];
				Mip.SizeX = MipSizeX;
				Mip.SizeY = MipSizeY;
				int MipDataSize = MipSizeX * MipSizeY * BytesPerPixel;
//				appPrintf("mip %d: %d x %d, %X bytes, offset %X\n", MipIndex, MipSizeX, MipSizeY, MipDataSize, MipOffset);
				assert(MipOffset + MipDataSize <= SourceDataSize);
				Mip.Data.BulkData = (byte*)appMalloc(MipDataSize);
				Mip.Data.ElementCount = MipDataSize;
				memcpy(Mip.Data.BulkData, SourceArt.BulkData + MipOffset, MipDataSize);
				MipOffset += MipDataSize;
			}
		}
		// compresses source art will be loaded in UTexture2D::GetTextureData()

		unguard;
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
