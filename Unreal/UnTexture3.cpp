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
	if (Ar.Game >= GAME_UE4_BASE)
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
	if (Ar.Game >= GAME_UE4_BASE)
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
		if (Ar.ArVer >= 677)
		{
			// MK X has enum properties serialized as bytes, without text - so PixelFormat values should be remapped
			// Insertions are:
			//   PF_FloatRGBA_Full=14
			//   PF_BC6=20
			//   PF_BC7=21
			// Other enum values are in the same order.
			if (Format == PF_R16F)
			{
				Format = PF_BC7;
			}
		}
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
#if XCOM
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
		if (!R.CollisionString.IsEmpty()) appNotify("HASH COLLISION: %s\n", *R.CollisionString);	//!!
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
		Textures.Add(Tex);
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
		appPrintf("%d: %s  {%08X} %dx%d %s [%llX + %08X]\n", i, *S.TextureFileCacheName, S.Hash,
			S.nWidth, S.nHeight, EnumToName(Tex->Format),
			S.BulkDataOffsetInFile, S.BulkDataSizeOnDisk
		);
#endif
	}
	appPrintf("... done\n");

	unguard;
}

#endif // DCU_ONLINE

//!! Rename "Tribes4" to "Redux" - supported in 2 UE3 games: Tribes Ascend and Blacklight Retribution
#if TRIBES4

//#define DUMP_RTC_CATALOG	1

struct ReduxMipEntry
{
	byte				f1;					// always == 1
	unsigned			FileOffset;
	int					PackedSize;
	int					UnpackedSize;

	friend FArchive& operator<<(FArchive &Ar, ReduxMipEntry &M)
	{
		guard(ReduxMipEntry<<);
		return Ar << M.f1 << M.FileOffset << M.PackedSize << M.UnpackedSize;
		unguard;
	}
};

struct ReduxTextureEntry
{
	FString				Name;
	EPixelFormat		Format;				// 2, 3, 5, 7, 25
	byte				f2;					// always = 2
	int16				USize, VSize;
	TArray<ReduxMipEntry>	Mips;

	friend FArchive& operator<<(FArchive &Ar, ReduxTextureEntry &E)
	{
		guard(ReduxTextureEntry<<);
		// here is non-nullterminated string, so can't use FString serializer directly
		assert(Ar.IsLoading);
		int NameLen;
		Ar << NameLen;
		E.Name.GetDataArray().AddZeroed(NameLen + 1);
		Ar.Serialize((void*)*E.Name, NameLen);
		return Ar << (byte&)E.Format << E.f2 << E.USize << E.VSize << E.Mips;
		unguard;
	}
};

static TArray<ReduxTextureEntry> reduxCatalog;
static FArchive *reduxDataAr = NULL;

static void ReduxReadRtcData()
{
	guard(ReduxReadRtcData);

	static bool ready = false;
	if (ready) return;
	ready = true;

	const CGameFileInfo *hdrFile = appFindGameFile("texture.cache.hdr.rtc");
	if (!hdrFile)
	{
		appPrintf("WARNING: unable to find %s\n", "texture.cache.hdr.rtc");
		return;
	}
	bool NewReduxSystem = false; // old one for Tribes4, new one - Blacklight: Retribution
	const CGameFileInfo *dataFile = appFindGameFile("texture.cache.data.rtc");
	if (!dataFile)
	{
		dataFile = appFindGameFile("texture.cache.0.data.rtc");
		if (!dataFile)
		{
			appPrintf("WARNING: unable to find %s\n", "texture.cache[.0].data.rtc");
			return;
		}
		NewReduxSystem = true;
	}
	reduxDataAr = appCreateFileReader(dataFile);

	FArchive *Ar = appCreateFileReader(hdrFile);
	Ar->Game  = GAME_Tribes4;
	Ar->ArVer = 805;			// just in case
	if (NewReduxSystem)
		Ar->Seek(8);			// skip 8 bytes of header: for Blacklight there are 2 ints: 0, 1
	*Ar << reduxCatalog;
	assert(Ar->IsEof());

	delete Ar;

#if DUMP_RTC_CATALOG
	for (int i = 0; i < reduxCatalog.Num(); i++)
	{
		const ReduxTextureEntry &Tex = reduxCatalog[i];
		appPrintf("%d: %s - %s %d %d %d\n", i, *Tex.Name, EnumToName(Tex.Format), Tex.f2, Tex.USize, Tex.VSize);
		if (Tex.Format != 2 && Tex.Format != 3 && Tex.Format != 5 && Tex.Format != 7 && Tex.Format != 25) appError("f1=%d", Tex.Format);
		if (Tex.f2 != 2) appError("f2=%d", Tex.f2);
		for (int j = 0; j < Tex.Mips.Num(); j++)
		{
			const ReduxMipEntry &Mip = Tex.Mips[j];
			assert(Mip.f1 == 1);
			assert(Mip.PackedSize && Mip.UnpackedSize);
			appPrintf("  %d: %d %d %d\n", j, Mip.FileOffset, Mip.PackedSize, Mip.UnpackedSize);
		}
	}
#endif

	unguard;
}

static byte FindReduxTexture(const UTexture2D *Tex, CTextureData *TexData)
{
	guard(FindReduxTexture);

	if (!reduxCatalog.Num())
		return false;
	assert(reduxDataAr);

	char ObjName[256];
	Tex->GetFullName(ARRAY_ARG(ObjName), true, true, true);
//	appPrintf("FIND: %s\n", ObjName);
	for (int i = 0; i < reduxCatalog.Num(); i++)
	{
		const ReduxTextureEntry &E = reduxCatalog[i];
		if (!stricmp(*E.Name, ObjName))
		{
			// found it
			assert(Tex->Format == E.Format);
//			assert(Tex->SizeX == E.USize && Tex->SizeY == E.VSize); -- not true because of cooking
			const ReduxMipEntry &Mip = E.Mips[0];
			byte *CompressedData   = (byte*)appMalloc(Mip.PackedSize);
			byte *UncompressedData = (byte*)appMalloc(Mip.UnpackedSize);
			reduxDataAr->Seek64(Mip.FileOffset);
			reduxDataAr->Serialize(CompressedData, Mip.PackedSize);
			appDecompress(CompressedData, Mip.PackedSize, UncompressedData, Mip.UnpackedSize, COMPRESS_ZLIB);
			appFree(CompressedData);
			CMipMap* DstMip = new (TexData->Mips) CMipMap;
			DstMip->CompressedData = UncompressedData;
			DstMip->ShouldFreeData = true;
			DstMip->USize = E.USize;
			DstMip->VSize    = E.VSize;
			DstMip->DataSize = Mip.UnpackedSize;
			return true;
		}
	}
	return false;

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


bool UTexture2D::LoadBulkTexture(const TArray<FTexture2DMipMap> &MipsArray, int MipIndex, const char* tfcSuffix, bool verbose) const
{
	const CGameFileInfo *bulkFile = NULL;

	guard(UTexture2D::LoadBulkTexture);

	const FTexture2DMipMap &Mip = MipsArray[MipIndex];

	// Here: data is either in TFC file or in other package
	char bulkFileName[256];
	bulkFileName[0] = 0;
	if (stricmp(TextureFileCacheName, "None") != 0)
	{
		// TFC file is assigned
		static const char* tfcExtensions[] = { "tfc", "xxx" };
		for (int i = 0; i < ARRAY_COUNT(tfcExtensions); i++)
		{
			strcpy(bulkFileName, TextureFileCacheName);
			if (char* s = strchr(bulkFileName, '.'))
			{
				// MK X has string with file extension - cut it
				if (!stricmp(s, ".tfc") || !stricmp(s, ".xxx"))
					*s = 0;
			}
			const char* bulkFileExt = tfcExtensions[i];
			bulkFile = appFindGameFile(bulkFileName, bulkFileExt);
			if (bulkFile) break;
#if SUPPORT_ANDROID
			if (!bulkFile)
			{
				if (!tfcSuffix) tfcSuffix = "DXT";
				appSprintf(ARRAY_ARG(bulkFileName), "%s_%s", bulkFileName, tfcSuffix);
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
#if UNREAL4
		if (GetGame() >= GAME_UE4_BASE)
		{
			//!! check for presence of BULKDATA_PayloadAtEndOfFile flag
			strcpy(bulkFileName, Package->Filename);
			if (Mip.Data.BulkDataFlags & BULKDATA_PayloadInSeperateFile)
			{
				// UE4.12+, store bulk payload in .ubulk file
				char* s = strrchr(bulkFileName, '.');
				if (s && !stricmp(s, ".uasset"))
					strcpy(s, ".ubulk");
			}
		}
		else
#endif // UNREAL4
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
	if (verbose)
		appPrintf("Reading %s mip level %d (%dx%d) from %s\n", Name, MipIndex, Mip.SizeX, Mip.SizeY, bulkFile->RelativeName);

	FArchive *Ar = appCreateFileReader(bulkFile);
	Ar->SetupFrom(*Package);
	FByteBulkData *Bulk = const_cast<FByteBulkData*>(&Mip.Data);
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
//	appPrintf("Bulk %X %llX [%d] f=%X\n", Bulk, Bulk->BulkDataOffsetInFile, Bulk->ElementCount, Bulk->BulkDataFlags);
	Bulk->SerializeData(*Ar);
	delete Ar;
	return true;

	unguardf("File=%s", bulkFile ? bulkFile->RelativeName : "none");
}


void UTexture2D::ReleaseTextureData() const
{
	guard(UTexture2D::ReleaseTextureData);

	// Release bulk data loaded with GetTextureData() call. Next time GetTextureData() will be
	// called, bulk data will be loaded again.

	const TArray<FTexture2DMipMap> *MipsArray = GetMipmapArray();

	for (int n = 0; n < MipsArray->Num(); n++)
	{
		const FTexture2DMipMap &Mip = (*MipsArray)[n];
		const FByteBulkData &Bulk = Mip.Data;
		if (Bulk.BulkData && (Bulk.BulkDataFlags & BULKDATA_StoreInSeparateFile))
			const_cast<FByteBulkData*>(&Bulk)->ReleaseData();
	}

	unguardf("%s", Name);
}


const TArray<FTexture2DMipMap>* UTexture2D::GetMipmapArray() const
{
	guard(UTexture2D::GetMipmapArray);

	const TArray<FTexture2DMipMap> *MipsArray = &Mips;
#if SUPPORT_ANDROID
	if (!MipsArray->Num())
	{
		if (CachedETCMips.Num())
			MipsArray = &CachedETCMips;
		else if (CachedPVRTCMips.Num())
			MipsArray = &CachedPVRTCMips;
		else if (CachedATITCMips.Num())
			MipsArray = &CachedATITCMips;
	}
#endif // SUPPORT_ANDROID

	return MipsArray;

	unguard;
}


bool UTexture2D::GetTextureData(CTextureData &TexData) const
{
	guard(UTexture2D::GetTextureData);

	TexData.OriginalFormatEnum = Format;
	TexData.OriginalFormatName = EnumToName(Format);
	TexData.Obj                = this;

#if TRIBES4
	if (Package && Package->Game == GAME_Tribes4)
	{
		ReduxReadRtcData();

		if (FindReduxTexture(this, &TexData))
		{
			TexData.Platform = Package->Platform;
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
#if UNREAL4
#if SUPPORT_IPHONE
	else if (Format == PF_PVRTC2)
		intFormat = TPF_PVRTC2;
	else if (Format == PF_PVRTC4)
		intFormat = TPF_PVRTC4;
#endif
#if SUPPORT_ANDROID
	else if (Format == PF_ETC1)
		intFormat = TPF_ETC1;
	else if (Format == PF_ETC2_RGB)		// GL_COMPRESSED_RGB8_ETC2
		intFormat = TPF_ETC2_RGB;
	else if (Format == PF_ETC2_RGBA)	// GL_COMPRESSED_RGBA8_ETC2_EAC
		intFormat = TPF_ETC2_RGBA;
	else if (Format == PF_ASTC_4x4)
		intFormat = TPF_ASTC_4x4;
	else if (Format == PF_ASTC_6x6)
		intFormat = TPF_ASTC_6x6;
	else if (Format == PF_ASTC_8x8)
		intFormat = TPF_ASTC_8x8;
	else if (Format == PF_ASTC_10x10)
		intFormat = TPF_ASTC_10x10;
	else if (Format == PF_ASTC_12x12)
		intFormat = TPF_ASTC_12x12;
#endif // SUPPORT_ANDROID
#endif // UNREAL4
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
				intFormat = TPF_RGBA8;
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

	if (TexData.Mips.Num() == 0 && MipsArray->Num())
	{
		bool bulkFailed = false;
		bool dataLoaded = false;
		int OrigUSize = (*MipsArray)[0].SizeX;
		int OrigVSize = (*MipsArray)[0].SizeY;
		for (int mipLevel = 0; mipLevel < MipsArray->Num(); mipLevel++)
		{
			// find 1st mipmap with non-null data array
			// reference: DemoPlayerSkins.utx/DemoSkeleton have null-sized 1st 2 mips
			const FTexture2DMipMap &Mip = (*MipsArray)[mipLevel];
			const FByteBulkData &Bulk = Mip.Data;
			if (!Mip.Data.BulkData)
			{
				// check for external bulk
				//?? Separate this function ?
				//!! * -notfc cmdline switch
				//!! * material viewer: support switching mip levels (for xbox decompression testing)
				if (Bulk.BulkDataFlags & BULKDATA_Unused) continue;		// mip level is stripped
				if (!(Bulk.BulkDataFlags & BULKDATA_StoreInSeparateFile)) continue; // equals to BULKDATA_PayloadAtEndOfFile for UE4
				// some optimization in a case of missing bulk file
				if (bulkFailed) continue;				// already checked for previous mip levels - no TFC file exists
				if (!LoadBulkTexture(*MipsArray, mipLevel, tfcSuffix, !dataLoaded))
				{
					bulkFailed = true;
					continue;	// note: this could be called for any mip level, not for the 1st only
				}
				else
				{
					dataLoaded = true;
				}
			}
			// this mipmap has data
			CMipMap* DstMip = new (TexData.Mips) CMipMap;
			DstMip->CompressedData = Bulk.BulkData;
			DstMip->DataSize = Bulk.ElementCount * Bulk.GetElementSize();
			DstMip->ShouldFreeData = false;
			// Note: UE3 can store incorrect SizeX/SizeY for lowest mips - these values could have 4x4 for all smaller mips
			// (perhaps minimal size of DXT block). So compute mip size by ourselves.
			DstMip->USize = max(1, OrigUSize >> mipLevel);
			DstMip->VSize = max(1, OrigVSize >> mipLevel);
//			printf("+%d: %d x %d (%X)\n", mipLevel, DstMip->USize, DstMip->VSize, DstMip->DataSize);
			TexData.Platform = Package->Platform;
		}
	}

	// Older Android and iOS UE3 versions didn't have dedicated CachedETCMips etc, all data were stored in Mips, but with different format
#if SUPPORT_IPHONE
	if (Package->Platform == PLATFORM_IOS && Mips.Num())
	{
		if (Format == PF_DXT1)
			intFormat = bForcePVRTC4 ? TPF_PVRTC4 : TPF_PVRTC2;
		else if (Format == PF_DXT5)
			intFormat = TPF_PVRTC4;
	}
#endif // SUPPORT_IPHONE

#if SUPPORT_ANDROID
	if (Package->Platform == PLATFORM_ANDROID && Mips.Num())
	{
		if (Format == PF_DXT1)
			intFormat = TPF_ETC1;
		else if (Format == PF_DXT5)
			intFormat = TPF_RGBA4;	// probably check BytesPerPixel - should be 2 for this format
	}
#endif // SUPPORT_ANDROID

	TexData.Format = intFormat;
	TexData.isNormalmap = (CompressionSettings == TC_Normalmap);

#if SUPPORT_XBOX360
	if (TexData.Platform == PLATFORM_XBOX360)
	{
		for (int MipLevel = 0; MipLevel < TexData.Mips.Num(); MipLevel++)
		{
			if (!TexData.DecodeXBox360(MipLevel))
			{
				// failed to decode this mip
				TexData.Mips.RemoveAt(MipLevel, TexData.Mips.Num() - MipLevel);
				break;
			}
		}
	}
#endif // SUPPORT_XBOX360

	return (TexData.Mips.Num() > 0);

	unguardf("%s", Name);
}


#endif // UNREAL3
