#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial3.h"
#include "UnPackage.h"


/*-----------------------------------------------------------------------------
	UTexture/UTexture2D (Unreal engine 3)
-----------------------------------------------------------------------------*/

#if UNREAL3

void UTexture3::Serialize(FArchive &Ar)
{
#if UNREAL4
	if (Ar.Game >= GAME_UE4)
	{
		Serialize4(Ar);
		return;
	}
#endif

	guard(UTexture3::Serialize);

	Super::Serialize(Ar);
#if TRANSFORMERS
	// Transformers: SourceArt is moved to separate class; but The Bourne Conspiracy has it (real ArLicenseeVer is unknown)
	if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 100) return;
#endif
#if BORDERLANDS
	if (Ar.Game == GAME_Borderlands && Ar.ArVer >= 832) return;	// Borderlands 2; version unknown
#endif
#if APB
	if (Ar.Game == GAME_APB)
	{
		// APB has source art stored in separate bulk
		// here are 2 headers: 1 for SourceArt and 1 for TIndirectArray<BulkData> MipSourceArt
		// we can skip these blocks by skipping headers
		// each header is a magic 0x5D0E7707 + position in package
		Ar.Seek(Ar.Tell() + 8 * 2);
		return;
	}
#endif // APB
#if MASSEFF
	if (Ar.Game == GAME_MassEffect3) return;
#endif
#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3) return;
#endif
	SourceArt.Serialize(Ar);
#if GUNLEGEND
	if (Ar.Game == GAME_GunLegend && Ar.ArVer >= 811)
	{
		// TIndirectArray<FByteBulkData> NonTopSourceArt
		int Count;
		Ar << Count;
		for (int i = 0; i < Count; i++)
		{
			FByteBulkData tmpBulk;
			tmpBulk.Skip(Ar);
		}
	}
#endif // GUNLEGEND
	unguard;
}


void UTexture2D::Serialize(FArchive &Ar)
{
	guard(UTexture2D::Serialize);

#if UNREAL4
	if (Ar.Game >= GAME_UE4)
	{
		Serialize4(Ar);
		return;
	}
#endif

	Super::Serialize(Ar);
#if TERA
	if (Ar.Game == GAME_Tera && Ar.ArLicenseeVer >= 3)
	{
		FString SourceFilePath; // field from UTexture
		Ar << SourceFilePath;
	}
#endif // TERA
	if (Ar.ArVer < 297)
	{
		int Format2;
		Ar << SizeX << SizeY << Format2;
		Format = (EPixelFormat)Format2;		// int -> byte (enum)
	}
#if BORDERLANDS
	if (Ar.Game == GAME_Borderlands && Ar.ArLicenseeVer >= 46) Ar.Seek(Ar.Tell() + 16);	// Borderlands 1,2; some hash; version unknown!!
#endif
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArLicenseeVer >= 31)	//?? may be MidwayVer ?
	{
		FByteBulkData CustomMipSourceArt;
		CustomMipSourceArt.Skip(Ar);
		if (Ar.ArVer >= 573)				// Injustice, version unknown
		{
			int unk1, unk2;
			TArray<int> unk3;
			Ar << unk1 << unk2 << unk3;
		}
		Ar << Mips;
		goto skip_rest_quiet;				// Injustice has some extra mipmap arrays
	}
#endif // MKVSDC
	Ar << Mips;
#if BORDERLANDS
	if (Ar.Game == GAME_Borderlands && Ar.ArLicenseeVer >= 46) Ar.Seek(Ar.Tell() + 16);	// Borderlands 1,2; some hash; version unknown!!
#endif
#if MASSEFF
	if (Ar.Game >= GAME_MassEffect && Ar.Game <= GAME_MassEffect3)
	{
		int unkFC;
		if (Ar.ArLicenseeVer >= 65) Ar << unkFC;
		if (Ar.ArLicenseeVer >= 99) goto tfc_guid;
	}
#endif // MASSEFF
#if HUXLEY
	if (Ar.Game == GAME_Huxley) goto skip_rest_quiet;
#endif
#if DCU_ONLINE
	if (Ar.Game == GAME_DCUniverse && (Ar.ArLicenseeVer & 0xFF00) >= 0x1700) return;
#endif
#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3) return;
#endif
	if (Ar.ArVer >= 567)
	{
	tfc_guid:
		Ar << TextureFileCacheGuid;
	}
#if XCOM_BUREAU
	if (Ar.Game == GAME_XcomB) return;
#endif
	// Extra check for some incorrectly upgrated UE3 versions, in particular for
	// Dungeons & Dragons: Daggerdale
	if (Ar.Tell() + 32 >= Ar.GetStopper())
	{
		// heuristic: not enough space for extra mips; example: Batman2 - it has version 805 but stores some
		// integer instead of TArray<FTexture2DMipMap>
		goto skip_rest;
	}

	if (Ar.ArVer >= 674)
		Ar << CachedPVRTCMips;

	if (Ar.ArVer >= 857)
	{
		int CachedFlashMipsMaxResolution;
		FByteBulkData CachedFlashMips;
		Ar << CachedFlashMipsMaxResolution;
		Ar << CachedATITCMips;
		CachedFlashMips.Serialize(Ar);
	}

#if PLA
	if (Ar.Game == GAME_PLA) goto skip_rest_quiet;
#endif
	if (Ar.ArVer >= 864) Ar << CachedETCMips;

	// some hack to support more games ...
skip_rest:
	if (Ar.Tell() < Ar.GetStopper())
	{
		appPrintf("UTexture2D %s: dropping %d bytes\n", Name, Ar.GetStopper() - Ar.Tell());
	skip_rest_quiet:
		DROP_REMAINING_DATA(Ar);
	}

	unguard;
}


#if DCU_ONLINE

// string hashing
static unsigned GCRCTable[256];

static void BuildCRCTable()
{
	assert(!GCRCTable[0]);
    for (int i = 0; i < 256; i++)
	{
		unsigned hash = i << 24;
		for (int j = 0; j < 8; j++)
		{
			if (hash & 0x80000000)
				hash = (hash << 1) ^ 0x04C11DB7;
			else
				hash <<= 1;
		}
		GCRCTable[i] = hash;
	}
}

static unsigned appStrihash(const char *str)
{
	if (!GCRCTable[0]) BuildCRCTable();

	unsigned Hash = 0;
	while (*str)
	{
		byte B = toupper(*str++);
		Hash   = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
		B      = 0;		// simulate unicode
		Hash   = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
	}
	return Hash;
}

// UTextureFileCacheRemap class

struct FTfcRemapEntry_DCU
{
	unsigned		Hash;
	int				Offset;
	FString			CollisionString;

	friend FArchive& operator<<(FArchive &Ar, FTfcRemapEntry_DCU &R)
	{
		guard(FTfcRemapEntry_DCU<<);
		Ar << R.Hash << R.Offset << R.CollisionString;
		if (R.CollisionString.Num() > 1) appNotify("HASH COLLISION: %s\n", *R.CollisionString);	//!!
		return Ar;
		unguard;
	}
};

class UTextureFileCacheRemap : public UObject
{
	DECLARE_CLASS(UTextureFileCacheRemap, UObject);
public:
	TArray<FTfcRemapEntry_DCU> TextureHashToRemapOffset;	// really - TMultiMap<unsigned, TFC_Remap_Info>

	void Serialize(FArchive &Ar)
	{
		guard(UTextureFileCacheRemap::Serialize);

		Super::Serialize(Ar);
		Ar << TextureHashToRemapOffset;

		unguard;
	}
};


// Main worker function
static int GetRealTextureOffset_DCU_2(unsigned Hash, const char *TFCName)
{
	guard(GetRealTextureOffset_DCU_2);

	static bool classRegistered = false;
	if (!classRegistered)
	{
		BEGIN_CLASS_TABLE
			REGISTER_CLASS(UTextureFileCacheRemap)
		END_CLASS_TABLE
		classRegistered = true;
	}

	// code to load object TextureFileCacheRemap from the package <TFC_NAME>_Remap

	// load a package
	char PkgName[256];
	appSprintf(ARRAY_ARG(PkgName), "%s_Remap", TFCName);
	UnPackage *Package = UnPackage::LoadPackage(PkgName);
	if (!Package)
	{
		appPrintf("Package %s was not found\n", PkgName);
		return -1;
	}
	// find object ...
	int mapExportIdx = Package->FindExport("TextureFileCacheRemap", "TextureFileCacheRemap");
	if (mapExportIdx == INDEX_NONE)
	{
		appPrintf("ERROR: unable to find export \"TextureFileCacheRemap\" in package %s\n", PkgName);
		return -1;
	}
	// ... and load it
	const UTextureFileCacheRemap *Remap = static_cast<UTextureFileCacheRemap*>(Package->CreateExport(mapExportIdx));
	assert(Remap);
	int Offset = -1;
	for (int i = 0; i < Remap->TextureHashToRemapOffset.Num(); i++)
	{
		const FTfcRemapEntry_DCU &E = Remap->TextureHashToRemapOffset[i];
		if (E.Hash == Hash)
		{
			Offset = E.Offset;
			break;
		}
	}
	return Offset;

	unguardf("TFC=%s Hash=%08X", TFCName, Hash);
}


static int GetRealTextureOffset_DCU(const UTexture2D *Obj)
{
	guard(GetRealTextureOffset_DCU);

	char ObjName[256];
	Obj->GetFullName(ARRAY_ARG(ObjName), true, true, true);
	unsigned Hash = appStrihash(ObjName);
	const char *TFCName = Obj->TextureFileCacheName;
	return GetRealTextureOffset_DCU_2(Hash, TFCName);

	unguardf("%s.%s", Obj->Package->Name, Obj->Name);
}


UUIStreamingTextures::~UUIStreamingTextures()
{
	// free generated data
	int i;
	for (i = 0; i < Textures.Num(); i++)
		delete Textures[i];
}


void UUIStreamingTextures::PostLoad()
{
	guard(UUIStreamingTextures::PostLoad);

	assert(Package->Game == GAME_DCUniverse);
	// code is similar to Rune's USkelModel::Serialize()
	appPrintf("Creating %d UI textures ...\n", TextureHashToInfo.Num());
	for (int i = 0; i < TextureHashToInfo.Num(); i++)
	{
		// create new UTexture2D
		const UIStreamingTexture_Info_DCU &S = TextureHashToInfo[i];
		UTexture2D *Tex = static_cast<UTexture2D*>(CreateClass("Texture2D"));
		char nameBuf[256];
		appSprintf(ARRAY_ARG(nameBuf), "UITexture_%08X", S.Hash);
		const char *name = appStrdupPool(nameBuf);
		Textures.AddItem(Tex);
		// setup UOnject
		Tex->Name         = name;
		Tex->Package      = Package;
		Tex->PackageIndex = INDEX_NONE;		// not really exported
		Tex->Outer        = NULL;
		Tex->TextureFileCacheName = S.TextureFileCacheName;
		Tex->Format       = (EPixelFormat)S.Format;
		Tex->SizeX        = S.nWidth;
		Tex->SizeY        = S.nHeight;
//		Tex->SRGB         = S.bSRGB;
		// create mipmap
		FTexture2DMipMap *Mip = new (Tex->Mips) FTexture2DMipMap;
		Mip->SizeX        = S.nWidth;
		Mip->SizeY        = S.nHeight;
		Mip->Data.BulkDataFlags        = S.BulkDataFlags;
		Mip->Data.ElementCount         = S.ElementCount;
		Mip->Data.BulkDataSizeOnDisk   = S.BulkDataSizeOnDisk;
		Mip->Data.BulkDataOffsetInFile = S.BulkDataOffsetInFile;
		// find TFC remap
//		unsigned Hash = appStrihash("UIICONS101_I1.dds");	//??
//		appPrintf("Hash: %08X\n", Hash);
		if (Mip->Data.BulkDataOffsetInFile < 0)
		{
			int Offset = GetRealTextureOffset_DCU_2(S.Hash, S.TextureFileCacheName);
			if (Offset < 0)
				Mip->Data.BulkDataFlags = BULKDATA_Unused;
			else
				Mip->Data.BulkDataOffsetInFile = Offset - Mip->Data.BulkDataOffsetInFile - 1;
			appPrintf("OFFS: %X\n", Mip->Data.BulkDataOffsetInFile);
		}
#if 1
		appPrintf("%d: %s  {%08X} %dx%d %s [%I64X + %08X]\n", i, *S.TextureFileCacheName, S.Hash,
			S.nWidth, S.nHeight, EnumToName("EPixelFormat", Tex->Format),
			S.BulkDataOffsetInFile, S.BulkDataSizeOnDisk
		);
#endif
	}
	appPrintf("... done\n");

	unguard;
}

#endif // DCU_ONLINE

#if TRIBES4

//#define DUMP_RTC_CATALOG	1

struct Tribes4MipEntry
{
	byte				f1;					// always == 1
	int					FileOffset;
	int					PackedSize;
	int					UnpackedSize;

	friend FArchive& operator<<(FArchive &Ar, Tribes4MipEntry &M)
	{
		guard(Tribes4MipEntry<<);
		return Ar << M.f1 << M.FileOffset << M.PackedSize << M.UnpackedSize;
		unguard;
	}
};

struct Tribes4TextureEntry
{
	FString				Name;
	EPixelFormat		Format;				// 2, 3, 5, 7, 25
	byte				f2;					// always = 2
	short				USize, VSize;
	TArray<Tribes4MipEntry>	Mips;

	friend FArchive& operator<<(FArchive &Ar, Tribes4TextureEntry &E)
	{
		guard(Tribes4TextureEntry<<);
		// here is non-nullterminated string, so can't use FString serializer directly
		assert(Ar.IsLoading);
		int NameLen;
		Ar << NameLen;
		E.Name.Empty(NameLen + 1);
		Ar.Serialize((void*)*E.Name, NameLen);
		return Ar << (byte&)E.Format << E.f2 << E.USize << E.VSize << E.Mips;
		unguard;
	}
};

static TArray<Tribes4TextureEntry> tribes4Catalog;
static FArchive *tribes4DataAr = NULL;

static void Tribes4ReadRtcData()
{
	guard(Tribes4ReadRtcData);

	static bool ready = false;
	if (ready) return;
	ready = true;

	const CGameFileInfo *hdrFile = appFindGameFile("texture.cache.hdr.rtc");
	if (!hdrFile)
	{
		appPrintf("WARNING: unable to find %s\n", "texture.cache.hdr.rtc");
		return;
	}
	const CGameFileInfo *dataFile = appFindGameFile("texture.cache.data.rtc");
	if (!dataFile)
	{
		appPrintf("WARNING: unable to find %s\n", "texture.cache.data.rtc");
		return;
	}
	tribes4DataAr = appCreateFileReader(dataFile);

	FArchive *Ar = appCreateFileReader(hdrFile);
	Ar->Game  = GAME_Tribes4;
	Ar->ArVer = 805;			// just in case
	*Ar << tribes4Catalog;
	assert(Ar->IsEof());

	delete Ar;

#if DUMP_RTC_CATALOG
	for (int i = 0; i < tribes4Catalog.Num(); i++)
	{
		const Tribes4TextureEntry &Tex = tribes4Catalog[i];
		appPrintf("%d: %s - %s %d %d %d\n", i, *Tex.Name, EnumToName("EPixelFormat", Tex.Format), Tex.f2, Tex.USize, Tex.VSize);
		if (Tex.Format != 2 && Tex.Format != 3 && Tex.Format != 5 && Tex.Format != 7 && Tex.Format != 25) appError("f1=%d", Tex.Format);
		if (Tex.f2 != 2) appError("f2=%d", Tex.f2);
		for (int j = 0; j < Tex.Mips.Num(); j++)
		{
			const Tribes4MipEntry &Mip = Tex.Mips[j];
			assert(Mip.f1 == 1);
			assert(Mip.PackedSize && Mip.UnpackedSize);
			appPrintf("  %d: %d %d %d\n", j, Mip.FileOffset, Mip.PackedSize, Mip.UnpackedSize);
		}
	}
#endif

	unguard;
}

static byte *FindTribes4Texture(const UTexture2D *Tex, CTextureData *TexData)
{
	guard(FindTribes4Texture);

	if (!tribes4Catalog.Num())
		return NULL;
	assert(tribes4DataAr);

	char ObjName[256];
	Tex->GetFullName(ARRAY_ARG(ObjName), true, true, true);
//	appPrintf("FIND: %s\n", ObjName);
	for (int i = 0; i < tribes4Catalog.Num(); i++)
	{
		const Tribes4TextureEntry &E = tribes4Catalog[i];
		if (!stricmp(E.Name, ObjName))
		{
			// found it
			assert(Tex->Format == E.Format);
//			assert(Tex->SizeX == E.USize && Tex->SizeY == E.VSize); -- not true because of cooking
			const Tribes4MipEntry &Mip = E.Mips[0];
			byte *CompressedData   = (byte*)appMalloc(Mip.PackedSize);
			byte *UncompressedData = (byte*)appMalloc(Mip.UnpackedSize);
			tribes4DataAr->Seek(Mip.FileOffset);
			tribes4DataAr->Serialize(CompressedData, Mip.PackedSize);
			appDecompress(CompressedData, Mip.PackedSize, UncompressedData, Mip.UnpackedSize, COMPRESS_ZLIB);
			appFree(CompressedData);
			TexData->USize    = E.USize;
			TexData->VSize    = E.VSize;
			TexData->DataSize = Mip.UnpackedSize;
			return UncompressedData;
		}
	}
	return NULL;

	unguard;
}

#endif // TRIBES4


#if MARVEL_HEROES

struct MHManifestMip
{
	int					Index;			// index of mipmap (first mip levels could be skipped with cooking)
	int					Offset;
	int					Size;

	friend FArchive& operator<<(FArchive &Ar, MHManifestMip &M)
	{
		return Ar << M.Index << M.Offset << M.Size;
	}
};

struct TFCManifest_MH
{
	FString				TFCName;
	FGuid				Guid;
	TArray<MHManifestMip> Mips;

	friend FArchive& operator<<(FArchive &Ar, TFCManifest_MH &M)
	{
		return Ar << M.TFCName << M.Guid << M.Mips;
	}
};

static TArray<TFCManifest_MH> mhTFCmanifest;

static void ReadMarvelHeroesTFCManifest()
{
	guard(ReadMarvelHeroesTFCManifest);

	static bool ready = false;
	if (ready) return;
	ready = true;

	const CGameFileInfo *fileInfo = appFindGameFile("TextureFileCacheManifest.bin");
	if (!fileInfo)
	{
		appPrintf("WARNING: unable to find %s\n", "TextureFileCacheManifest.bin");
		return;
	}
	FArchive *Ar = appCreateFileReader(fileInfo);
	Ar->Game  = GAME_MarvelHeroes;
	Ar->ArVer = 859;			// just in case
	Ar->ArLicenseeVer = 3;
	*Ar << mhTFCmanifest;
	assert(Ar->IsEof());

	delete Ar;

	unguard;
}


static int GetRealTextureOffset_MH(const UTexture2D *Obj, int MipIndex)
{
	guard(GetRealTextureOffset_MH);

	ReadMarvelHeroesTFCManifest();

	appPrintf("LOOK %08X-%08X-%08X-%08X\n", Obj->TextureFileCacheGuid.A, Obj->TextureFileCacheGuid.B, Obj->TextureFileCacheGuid.C, Obj->TextureFileCacheGuid.D);
	for (int i = 0; i < mhTFCmanifest.Num(); i++)
	{
		const TFCManifest_MH &M = mhTFCmanifest[i];
		if (M.Guid == Obj->TextureFileCacheGuid)
		{
			const MHManifestMip &Mip = M.Mips[0];
			assert(Mip.Index == MipIndex);
			appPrintf("%s - %08X-%08X-%08X-%08X = %X %X\n", *M.TFCName, M.Guid.A, M.Guid.B, M.Guid.C, M.Guid.D, Mip.Offset, Mip.Size);
			return Mip.Offset;
		}
	}

	return -1;	// not found

	unguard;
}

#endif // MARVEL_HEROES


bool UTexture2D::LoadBulkTexture(const TArray<FTexture2DMipMap> &MipsArray, int MipIndex, const char* tfcSuffix) const
{
	const CGameFileInfo *bulkFile = NULL;

	guard(UTexture2D::LoadBulkTexture);

	const FTexture2DMipMap &Mip = MipsArray[MipIndex];

	// Here: data is either in TFC file or in other package
	char bulkFileName[256];
	if (stricmp(TextureFileCacheName, "None") != 0)
	{
		// TFC file is assigned
		strcpy(bulkFileName, TextureFileCacheName);
		static const char* tfcExtensions[] = { "tfc", "xxx" };
		for (int i = 0; i < ARRAY_COUNT(tfcExtensions); i++)
		{
			const char* bulkFileExt = tfcExtensions[i];
			bulkFile = appFindGameFile(bulkFileName, bulkFileExt);
			if (bulkFile) break;
#if SUPPORT_ANDROID
			if (!bulkFile)
			{
				if (!tfcSuffix) tfcSuffix = "DXT";
				appSprintf(ARRAY_ARG(bulkFileName), "%s_%s", *TextureFileCacheName, tfcSuffix);
				bulkFile = appFindGameFile(bulkFileName, bulkFileExt);
				if (bulkFile) break;
			}
#endif // SUPPORT_ANDROID
		}
		if (!bulkFile)
		{
			appPrintf("Decompressing %s: TFC file \"%s\" is missing\n", Name, *TextureFileCacheName);
			return false;
		}
	}
	else
	{
		// data is inside another package
		//!! copy-paste from UnPackage::CreateExport(), should separate function
		// find outermost package
		if (this->PackageIndex)
		{
			int PackageIndex = this->PackageIndex;		// export entry for this object (UTexture2D)
			while (true)
			{
				const FObjectExport &Exp2 = Package->GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;			// get parent (UPackage)
				PackageIndex = Exp2.PackageIndex - 1;	// subtract 1 from package index
			}
			const FObjectExport &Exp2 = Package->GetExport(PackageIndex);
			if (Exp2.ExportFlags & EF_ForcedExport)
			{
				strcpy(bulkFileName,  Exp2.ObjectName);
//				appPrintf("BULK: %s (%X)\n", *Exp2.ObjectName, Exp2.ExportFlags);
			}
		}
		if (!bulkFileName[0]) return false;				// just in case
		bulkFile = appFindGameFile(bulkFileName);
		if (!bulkFile)
		{
			appPrintf("Decompressing %s: package %s is missing\n", Name, bulkFileName);
			return false;
		}
	}

	assert(bulkFile);									// missing file is processed above
	appPrintf("Reading %s mip level %d (%dx%d) from %s\n", Name, MipIndex, Mip.SizeX, Mip.SizeY, bulkFile->RelativeName);
	FArchive *Ar = appCreateFileReader(bulkFile);
	Ar->SetupFrom(*Package);
	FByteBulkData *Bulk = const_cast<FByteBulkData*>(&Mip.Data);	//!! const_cast
	if (Bulk->BulkDataOffsetInFile < 0)
	{
#if DCU_ONLINE
		if (Package->Game == GAME_DCUniverse)
		{
			int Offset = GetRealTextureOffset_DCU(this);
			if (Offset < 0) return false;
			Bulk->BulkDataOffsetInFile = Offset - Bulk->BulkDataOffsetInFile - 1;
//			appPrintf("OFFS: %X\n", Bulk->BulkDataOffsetInFile);
		}
#endif // DCU_ONLINE
#if MARVEL_HEROES
		if (Package->Game == GAME_MarvelHeroes)
		{
			int Offset = GetRealTextureOffset_MH(this, MipIndex);
			if (Offset < 0) return false;
			Bulk->BulkDataOffsetInFile = Offset;
		}
#endif // MARVEL_HEROES
		if (Bulk->BulkDataOffsetInFile < 0)
		{
			appPrintf("ERROR: BulkOffset = %d\n", (int)Bulk->BulkDataOffsetInFile);
			return false;
		}
	}
//	appPrintf("Bulk %X %I64X [%d] f=%X\n", Bulk, Bulk->BulkDataOffsetInFile, Bulk->ElementCount, Bulk->BulkDataFlags);
	Bulk->SerializeData(*Ar);
	delete Ar;
	return true;

	unguardf("File=%s", bulkFile ? bulkFile->RelativeName : "none");
}


bool UTexture2D::GetTextureData(CTextureData &TexData) const
{
	guard(UTexture2D::GetTextureData);

	TexData.OriginalFormatEnum = Format;
	TexData.OriginalFormatName = EnumToName("EPixelFormat", Format);
	TexData.Obj                = this;

#if TRIBES4
	if (Package && Package->Game == GAME_Tribes4)
	{
		Tribes4ReadRtcData();

		TexData.CompressedData = FindTribes4Texture(this, &TexData);	// may be NULL
		if (TexData.CompressedData)
		{
			TexData.ShouldFreeData = true;
			TexData.Platform       = Package->Platform;
		}
	}
#endif // TRIBES4

	ETexturePixelFormat intFormat;
	if (Format == PF_A8R8G8B8 || Format == PF_B8G8R8A8)	// PF_A8R8G8B8 was renamed to PF_B8G8R8A8
		intFormat = TPF_BGRA8;
	else if (Format == PF_DXT1)
		intFormat = TPF_DXT1;
	else if (Format == PF_DXT3)
		intFormat = TPF_DXT3;
	else if (Format == PF_DXT5)
		intFormat = TPF_DXT5;
	else if (Format == PF_G8)
		intFormat = TPF_G8;
	else if (Format == PF_V8U8)
		intFormat = TPF_V8U8;
	else if (Format == PF_BC5)
		intFormat = TPF_BC5;
	else if (Format == PF_BC7)
		intFormat = TPF_BC7;
	else if (Format == PF_A1)
		intFormat = TPF_A1;
#if MASSEFF
//	else if (Format == PF_NormapMap_LQ) -- seems not used
//		intFormat = TPF_BC5;
	else if (Format == PF_NormalMap_HQ)
		intFormat = TPF_BC5;
#endif // MASSEFF
	else
	{
		appNotify("Unknown texture format: %s (%d)", TexData.OriginalFormatName, Format);
		return false;
	}

	const TArray<FTexture2DMipMap> *MipsArray = &Mips;
	const char* tfcSuffix = NULL;

#if SUPPORT_ANDROID
	if (!Mips.Num())
	{
		if (CachedETCMips.Num())
		{
			MipsArray = &CachedETCMips;
			tfcSuffix = "ETC";
			if (Format == PF_DXT1)
				intFormat = TPF_ETC1;
			else if (Format == PF_DXT5)
			{
				appPrintf("ETC2 texture format is not supported\n"); // TPF_ETC2_EAC
				return false;
			}
		}
		else if (CachedPVRTCMips.Num())
		{
			MipsArray = &CachedPVRTCMips;
			tfcSuffix = "PVRTC";
			if (Format == PF_DXT1) // the same code as for iOS
				intFormat = bForcePVRTC4 ? TPF_PVRTC4 : TPF_PVRTC2;
			else if (Format == PF_DXT5)
				intFormat = TPF_PVRTC4;
		}
		else if (CachedATITCMips.Num())
		{
			appPrintf("Unsupported ATI texture compression\n");
			return false;
		}
	}
#endif // SUPPORT_ANDROID

	if (!TexData.CompressedData)
	{
		bool bulkChecked = false;
		for (int n = 0; n < MipsArray->Num(); n++)
		{
			// find 1st mipmap with non-null data array
			// reference: DemoPlayerSkins.utx/DemoSkeleton have null-sized 1st 2 mips
			const FTexture2DMipMap &Mip = (*MipsArray)[n];
			const FByteBulkData &Bulk = Mip.Data;
			if (!Mip.Data.BulkData)
			{
				//?? Separate this function ?
				// check for external bulk
				//!! * -notfc cmdline switch
				//!! * material viewer: support switching mip levels (for xbox decompression testing)
				if (Bulk.BulkDataFlags & BULKDATA_Unused) continue;		// mip level is stripped
				if (!(Bulk.BulkDataFlags & BULKDATA_StoreInSeparateFile)) continue;
				// some optimization in a case of missing bulk file
				if (bulkChecked) continue;				// already checked for previous mip levels
				bulkChecked = true;
				if (!LoadBulkTexture(*MipsArray, n, tfcSuffix)) continue;	// note: this could be called for any mip level, not for the 1st only
			}
			// this mipmap has data
			TexData.CompressedData = Bulk.BulkData;
			TexData.USize          = Mip.SizeX;
			TexData.VSize          = Mip.SizeY;
			TexData.DataSize       = Bulk.ElementCount * Bulk.GetElementSize();
			TexData.Platform       = Package->Platform;
			break;
		}
	}

#if SUPPORT_IPHONE
	if (Package->Platform == PLATFORM_IOS)
	{
		if (Format == PF_DXT1)
			intFormat = bForcePVRTC4 ? TPF_PVRTC4 : TPF_PVRTC2;
		else if (Format == PF_DXT5)
			intFormat = TPF_PVRTC4;
	}
#endif // SUPPORT_IPHONE

	TexData.Format = intFormat;

#if SUPPORT_XBOX360
	if (TexData.Platform == PLATFORM_XBOX360)
		TexData.DecodeXBox360();
#endif

	return (TexData.CompressedData != NULL);

	unguardf("%s", Name);
}


#endif // UNREAL3
