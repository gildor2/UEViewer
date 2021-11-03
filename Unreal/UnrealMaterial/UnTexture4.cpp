#include "Core.h"
#include "UnCore.h"
#include "UE4Version.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial3.h"
#include "UnMaterialExpression.h"
#include "UnrealPackage/UnPackage.h"


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

#if BORDERLANDS3
	if (Ar.Game == GAME_Borderlands3)
	{
		uint16 SizeX, SizeY, SizeZ;
		Ar << SizeX << SizeY << SizeZ;
		Mip.SizeX = SizeX;
		Mip.SizeY = SizeY;
		goto after_mip_size;
	}
#endif // BORDERLANDS3

	Ar << Mip.SizeX << Mip.SizeY;
	if (Ar.Game >= GAME_UE4(20))
	{
		int32 SizeZ;
		Ar << SizeZ;
	}

after_mip_size:
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
	int32		NumSlices;				// 1 for simple texture, 6 for cubemap - 6 textures are joined into one
	FString		PixelFormat;
	TArray<FTexture2DMipMap> Mips;

	friend FArchive& operator<<(FArchive& Ar, FTexturePlatformData& D)
	{
		// see TextureDerivedData.cpp, SerializePlatformData()
		guard(FTexturePlatformData<<);
		Ar << D.SizeX << D.SizeY << D.NumSlices;
		D.NumSlices &= 0x3fffffff; // 2 higher bits are BitMask_CubeMap and BitMask_HasOptData since UE4.24

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
		int32 FirstMip;
		Ar << FirstMip;					// only for cooked, but we don't read FTexturePlatformData for non-cooked textures
		DBG("   SizeX=%d SizeY=%d NumSlices=%d PixelFormat=%s FirstMip=%d\n", D.SizeX, D.SizeY, D.NumSlices, *D.PixelFormat, FirstMip);
		D.Mips.Serialize2<FTexture2DMipMap::Serialize4>(Ar);

		if (Ar.Game >= GAME_UE4(23))
		{
			bool bIsVirtual;
			Ar << bIsVirtual;
			if (bIsVirtual)
			{
				// Requires extra serializing
				appError("VirtualTextures are not supported");
			}
		}

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

#if JEDI
	if (Ar.Game == GAME_Jedi && !stricmp(Name, "UnchangedPRT"))
	{
		// This texture is only the object which has oodle decompression error, skip it
		DROP_REMAINING_DATA(Ar);
		return;
	}
#endif // JEDI

	Super::Serialize4(Ar);

	FStripDataFlags StripFlags(Ar);		// note: these flags are used for pre-VER_UE4_TEXTURE_SOURCE_ART_REFACTOR versions

	bool bCooked = false;
	if (Ar.ArVer >= VER_UE4_ADD_COOKED_TO_TEXTURE2D) Ar << bCooked;

	if (Ar.ArVer < VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)
	{
		appNotify("Untested code: UTexture2D::LegacySerialize");
		// This code lives in UTexture2D::LegacySerialize(). It relies on some deprecated properties, and modern
		// code UE4 can't read cooked packages prepared with pre-VER_UE4_TEXTURE_SOURCE_ART_REFACTOR version of
		// the engine. So, it's not possible to know what should happen there unless we'll get some working game
		// which uses old UE4 version.bDisableDerivedDataCache_DEPRECATED in UE4 serialized as property, when set
		// to true - has serialization of TArray<FTexture2DMipMap>. We suppose here that it's 'false'.
		FGuid TextureFileCacheGuid_DEPRECATED;
		Ar << TextureFileCacheGuid_DEPRECATED;
	}

	// Formats are added in UE4 in Source/Developer/<Platform>TargetPlatform/Private/<Platform>TargetPlatform.h,
	// in TTargetPlatformBase::GetTextureFormats(). Formats are chosen depending on platform settings (for example,
	// for Android) or depending on UTexture::CompressionSettings. Windows, Linux and Mac platform uses the same
	// texture format (see FTargetPlatformBase::GetDefaultTextureFormatName()). Other platforms uses different formats,
	// but all of them (except Android) uses single texture format per object.

	if (bCooked)
	{
		FName PixelFormatEnum;
		Ar << PixelFormatEnum;
		while (stricmp(PixelFormatEnum, "None") != 0)
		{
			DBG("  PixelFormat: %s\n", *PixelFormatEnum);
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
//				appPrintf("Loading data for format %s ...\n", PixelFormatEnum.Str);
				// the texture was not loaded yet
				FTexturePlatformData Data;
				Ar << Data;
			#if SEAOFTHIEVES
				if (Ar.Game == GAME_SeaOfThieves) Ar.Seek(Ar.Tell() + 4);
			#endif
				assert(Ar.Tell() == SkipOffset);
				// copy data to UTexture2D
				Exchange(Mips, Data.Mips);		// swap arrays to avoid copying
				SizeX = Data.SizeX;
				SizeY = Data.SizeY;
				Format = PixelFormat;
				NumSlices = Data.NumSlices;
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
				Mip.Data.BulkData = (byte*)appMallocNoInit(MipDataSize);
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

void UMaterial3::Serialize4(FArchive& Ar)
{
	guard(UMaterial3::Serialize4);

	if ((Ar.Game >= GAME_UE4(26)) && (Package->Summary.PackageFlags & PKG_UnversionedProperties))
	{
		// TODO: implement unversioned properties for UMaterial
		DROP_REMAINING_DATA(Ar);
		return;
	}

	Super::Serialize(Ar);
	DROP_REMAINING_DATA(Ar);

	unguard;
}

void UMaterial3::PostLoad()
{
	guard(UMaterial3::PostLoad);

	ScanMaterialExpressions();
#if UNREAL4
	// UE4 has complex FMaterialResource format, so avoid reading anything here, but
	// scan package's imports for UTexture objects instead. Here we're attempting to
	// collect data from material expressions, so do the work in PostLoad to ensure
	// all expressions are serialized.
	if (GetGame() >= GAME_UE4_BASE)
		ScanUE4Textures();
#endif

	unguard;
}

void UMaterial3::ScanMaterialExpressions()
{
	guard(UMaterial3::ScanMaterialExpressions)

	// Note: code is common for UE3 and UE4
	//todo: may be move code to UE3 cpp file?
	if (Expressions.Num())
	{
		guard(Expressions);

		for (const UObject* Obj : Expressions)
		{
			if (!Obj) continue;

			if (Obj->IsA("MaterialExpressionTextureSampleParameter"))
			{
				const UMaterialExpressionTextureSampleParameter2D* Expr = static_cast<const UMaterialExpressionTextureSampleParameter2D*>(Obj);
				CTextureParameterValue Param;
				Param.Name = Expr->ParameterName;
				Param.Group = Expr->Group;
				Param.Texture = Expr->Texture;
				CollectedTextureParameters.Add(Param);
			}
			else if (Obj->IsA("MaterialExpressionTextureSample"))
			{
				// This is not a parameter,
				const UMaterialExpressionTextureSample* Expr = static_cast<const UMaterialExpressionTextureSample*>(Obj);
				if (Expr->Texture) ReferencedTextures.AddUnique(Expr->Texture);
			}
			else if (Obj->IsA("MaterialExpressionScalarParameter"))
			{
				const UMaterialExpressionScalarParameter* Expr = static_cast<const UMaterialExpressionScalarParameter*>(Obj);
				CScalarParameterValue Param;
				Param.Name = Expr->ParameterName;
				Param.Group = Expr->Group;
				Param.Value = Expr->DefaultValue;
				CollectedScalarParameters.Add(Param);
			}
			else if (Obj->IsA("MaterialExpressionVectorParameter"))
			{
				const UMaterialExpressionVectorParameter* Expr = static_cast<const UMaterialExpressionVectorParameter*>(Obj);
				CVectorParameterValue Param;
				Param.Name = Expr->ParameterName;
				Param.Group = Expr->Group;
				Param.Value = Expr->DefaultValue;
				CollectedVectorParameters.Add(Param);
			}
		}

		unguard;
	}

	// UE4.25+
	guard(CachedExpressionData);

	const FMaterialCachedParameters& Params = CachedExpressionData.Parameters;
	for (int i = 0; i < Params.TextureValues.Num(); i++)
	{
		CTextureParameterValue Param;
		Param.Name = Params.Entries[(int)EMaterialParameterType::Texture].ParameterInfos[i].Name;
		Param.Texture = Params.TextureValues[i];
		CollectedTextureParameters.Add(Param);
	}
	for (int i = 0; i < Params.ScalarValues.Num(); i++)
	{
		CScalarParameterValue Param;
		Param.Name = Params.Entries[(int)EMaterialParameterType::Scalar].ParameterInfos[i].Name;
		Param.Value = Params.ScalarValues[i];
		CollectedScalarParameters.Add(Param);
	}
	for (int i = 0; i < Params.VectorValues.Num(); i++)
	{
		CVectorParameterValue Param;
		Param.Name = Params.Entries[(int)EMaterialParameterType::Vector].ParameterInfos[i].Name;
		Param.Value = Params.VectorValues[i];
		CollectedVectorParameters.Add(Param);
	}

	unguard;

	unguard;
}

void UMaterial3::ScanUE4Textures()
{
	guard(UMaterial3::ScanUE4Textures);

//	printf("--> %d imports\n", Package->Summary.ImportCount);
	if (ReferencedTextures.Num() == 0)
	{
		// Scan only if parsing of expressions failed
		//!! NOTE: this code will not work when textures are located in the same package - they don't present in import table
		//!! but could be found in export table. That's true for Simplygon-generated materials.
		for (int i = 0; i < Package->Summary.ImportCount; i++)
		{
			const FObjectImport &Imp = Package->GetImport(i);
//			printf("--> import %d (%s)\n", i, *Imp.ClassName);
			// Only load textures
			if (strnicmp(Imp.ClassName, "Texture", 7) == 0)
			{
				UObject* obj = Package->CreateImport(i);
//				if (obj) printf("--> %s (%s)\n", obj->Name, obj->GetClassName());
				if (obj && obj->IsA("Texture3"))
					ReferencedTextures.AddUnique(static_cast<UTexture3*>(obj));
			}
		}
	}

	unguard;
}

#endif // UNREAL4
