#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial2.h"

//#define XPR_DEBUG			1

/*-----------------------------------------------------------------------------
	UTexture (Unreal engine 1 and 2)
-----------------------------------------------------------------------------*/

void UTexture::Serialize(FArchive &Ar)
{
	guard(UTexture::Serialize);
	Super::Serialize(Ar);
#if BIOSHOCK
	TRIBES_HDR(Ar, 0x2E);
	if (Ar.Game == GAME_Bioshock && t3_hdrSV >= 1)
		Ar << CachedBulkDataSize;
	if (Ar.Game == GAME_Bioshock && Format == 12)	// remap format; note: Bioshock used 3DC name, but real format is DXT5N
		Format = TEXF_DXT5N;
#endif // BIOSHOCK
#if SWRC
	if (Ar.Game == GAME_RepCommando)
	{
		if (Format == 14) Format = TEXF_CxV8U8;		//?? not verified
	}
#endif // SWRC
#if VANGUARD
	if (Ar.Game == GAME_Vanguard && Ar.ArVer >= 128 && Ar.ArLicenseeVer >= 25)
	{
		// has some table for fast mipmap lookups
		Ar.Seek(Ar.Tell() + 142);	// skip that table
		// serialize mips using AR_INDEX count (this game uses int for array counts in all other places)
		int Count;
		Ar << AR_INDEX(Count);
		Mips.AddDefaulted(Count);
		for (int i = 0; i < Count; i++)
			Ar << Mips[i];
		return;
	}
#endif // VANGUARD
#if AA2
	if (Ar.Game == GAME_AA2 && Ar.ArLicenseeVer >= 8)
	{
		int unk;		// always 10619
		Ar << unk;
	}
#endif // AA2
	Ar << Mips;
	if (Ar.Engine() == GAME_UE1)
	{
		// UE1
		bMasked = false;			// ignored by UE1, used surface.PolyFlags instead (but UE2 ignores PolyFlags ...)
		if (bHasComp)				// skip compressed mipmaps
		{
			TArray<FMipmap>	CompMips;
			Ar << CompMips;
		}
	}
#if XIII
	if (Ar.Game == GAME_XIII)
	{
		if (Ar.ArLicenseeVer >= 42)
		{
			// serialize palette
			if (Format == TEXF_P8 || Format == 13)	// 13 == TEXF_P4
			{
				assert(!Palette);
				Palette = new UPalette;
				Ar << Palette->Colors;
			}
		}
		if (Ar.ArLicenseeVer >= 55)
			Ar.Seek(Ar.Tell() + 3);
	}
#endif // XIII
#if EXTEEL
	if (Ar.Game == GAME_Exteel)
	{
		// note: this property is serialized as UObject's property too
		byte MaterialType;			// enum GFMaterialType
		Ar << MaterialType;
	}
#endif // EXTEEL
	unguard;
}


#if UC2

struct XprEntry
{
	char				Name[64];
	int					DataOffset;
	int					DataSize;
};

struct XprInfo
{
	const CGameFileInfo *File;
	TArray<XprEntry>	Items;
	int					DataStart;
};

static TArray<XprInfo> xprFiles;

static bool ReadXprFile(const CGameFileInfo *file)
{
	guard(ReadXprFile);

	FArchive *Ar = appCreateFileReader(file);

	int Tag, FileLen, DataStart, DataCount;
	*Ar << Tag << FileLen << DataStart << DataCount;
	//?? "XPR0" - xpr variant with a single object (texture) inside
	if (Tag != BYTES4('X','P','R','1'))
	{
#if XPR_DEBUG
		appPrintf("Unknown XPR tag in %s\n", file->RelativeName);
#endif
		delete Ar;
		return true;
	}
#if XPR_DEBUG
	appPrintf("Scanning %s ...\n", file->RelativeName);
#endif

	XprInfo *Info = new(xprFiles) XprInfo;
	Info->File      = file;
	Info->DataStart = DataStart;
	// read filelist
	int i;
	for (i = 0; i < DataCount; i++)
	{
		int NameOffset, DataOffset;
		*Ar << NameOffset << DataOffset;
		int savePos = Ar->Tell();
		Ar->Seek(NameOffset + 12);
		// read name
		char c, buf[256];
		int n = 0;
		while (true)
		{
			*Ar << c;
			if (n < ARRAY_COUNT(buf))
				buf[n++] = c;
			if (!c) break;
		}
		buf[ARRAY_COUNT(buf)-1] = 0;			// just in case
		// create item
		XprEntry *Entry = new(Info->Items) XprEntry;
		appStrncpyz(Entry->Name, buf, ARRAY_COUNT(Entry->Name));
		Entry->DataOffset = DataOffset + 12;
		assert(Entry->DataOffset < DataStart);
		// seek back
		Ar->Seek(savePos);
		// setup size of previous item
		if (i >= 1)
		{
			XprEntry *PrevEntry = &Info->Items[i - 1];
			PrevEntry->DataSize = Entry->DataOffset - PrevEntry->DataOffset;
		}
		// setup size of the last item
		if (i == DataCount - 1)
			Entry->DataSize = DataStart - Entry->DataOffset;
	}
	// scan data
	// data block is either embedded in this block or followed after DataStart position
	for (i = 0; i < DataCount; i++)
	{
		XprEntry *Entry = &Info->Items[i];
#if XPR_DEBUG
//		appPrintf("  %08X [%08X]  %s\n", Entry->DataOffset, Entry->DataSize, Entry->Name);
#endif
		Ar->Seek(Entry->DataOffset);
		int id;
		*Ar << id;
		switch (id)
		{
		case 0x80020001:
			// header is 4 dwords + immediately followed data
			Entry->DataOffset += 4 * 4;
			Entry->DataSize   -= 4 * 4;
			break;

		case 0x00040001:
			// header is 5 dwords + external data
			{
				int pos;
				*Ar << pos;
				Entry->DataOffset = DataStart + pos;
			}
			break;

		case 0x00020001:
			// header is 4 dwords + external data
			{
				int d1, d2, pos;
				*Ar << d1 << d2 << pos;
				Entry->DataOffset = DataStart + pos;
			}
			break;

		default:
			// header is 2 dwords - offset and size + external data
			{
				int pos;
				*Ar << pos;
				Entry->DataOffset = DataStart + pos;
			}
			break;
		}
	}
	// setup sizes of blocks placed after DataStart (not embedded into file list)
	for (i = 0; i < DataCount; i++)
	{
		XprEntry *Entry = &Info->Items[i];
		if (Entry->DataOffset < DataStart) continue; // embedded data
		// Entry points to a data block placed after DataStart position
		// we should find a next block
		int NextPos = FileLen;
		for (int j = i + 1; j < DataCount; j++)
		{
			XprEntry *NextEntry = &Info->Items[j];
			if (NextEntry->DataOffset < DataStart) continue; // embedded data
			NextPos = NextEntry->DataOffset;
			break;
		}
		Entry->DataSize = NextPos - Entry->DataOffset;
	}
#if XPR_DEBUG
	for (i = 0; i < DataCount; i++)
	{
		XprEntry *Entry = &Info->Items[i];
		appPrintf("  %3d %08X [%08X] .. %08X  %s\n", i, Entry->DataOffset, Entry->DataSize, Entry->DataOffset + Entry->DataSize, Entry->Name);
	}
#endif

	delete Ar;
	return true;

	unguardf("%s", file->RelativeName);
}


byte *FindXprData(const char *Name, int *DataSize)
{
	// scan xprs
	static bool ready = false;
	if (!ready)
	{
		ready = true;
		appEnumGameFiles(ReadXprFile, "xpr");
	}
	// find a file
	for (int i = 0; i < xprFiles.Num(); i++)
	{
		XprInfo *Info = &xprFiles[i];
		for (int j = 0; j < Info->Items.Num(); j++)
		{
			XprEntry *File = &Info->Items[j];
			if (strcmp(File->Name, Name) == 0)
			{
				// found
				appPrintf("Loading stream %s from %s (%d bytes)\n", Name, Info->File->RelativeName, File->DataSize);
				FArchive *Reader = appCreateFileReader(Info->File);
				Reader->Seek(File->DataOffset);
				byte *buf = (byte*)appMalloc(File->DataSize);
				Reader->Serialize(buf, File->DataSize);
				delete Reader;
				if (DataSize) *DataSize = File->DataSize;
				return buf;
			}
		}
	}
	appPrintf("WARNING: external stream %s was not found\n", Name);
	if (DataSize) *DataSize = 0;
	return NULL;
}

#endif // UC2

#if BIOSHOCK

//#define DUMP_BIO_CATALOG			1
//#define DEBUG_BIO_BULK				1

struct BioBulkCatalogItem
{
	FString				ObjectName;
	FString				PackageName;
	int					f10;				// always 0
	int					DataOffset;
	int					DataSize;
	int					DataSize2;			// the same as DataSize
	int					f20;

	friend FArchive& operator<<(FArchive &Ar, BioBulkCatalogItem &S)
	{
		Ar << S.ObjectName << S.PackageName << S.f10 << S.DataOffset << S.DataSize << S.DataSize2 << S.f20;
		assert(S.f10 == 0);
//		assert(S.DataSize == S.DataSize2);	-- the same on PC, but not the same on XBox360
#if DUMP_BIO_CATALOG
		appPrintf("  %s / %s - %08X:%08X %X %X %X\n", *S.ObjectName, *S.PackageName, S.f10, S.DataOffset, S.DataSize, S.DataSize2, S.f20);
#endif
		return Ar;
	}
};


struct BioBulkCatalogFile
{
	int64				f0;
	FString				Filename;
	TArray<BioBulkCatalogItem> Items;

	friend FArchive& operator<<(FArchive &Ar, BioBulkCatalogFile &S)
	{
		Ar << S.f0 << S.Filename;
#if DUMP_BIO_CATALOG
		appPrintf("<<< %s >>>\n", *S.Filename);
#endif
		Ar << S.Items;
#if DEBUG_BIO_BULK
		int minS2 = 99999999, maxS2 = -99999999, min20 = 99999999, max20 = -99999999;
		for (int i = 0; i < S.Items.Num(); i++)
		{
			int n1 = S.Items[i].DataSize2;
			if (n1 < minS2) minS2 = n1;
			if (n1 > maxS2) maxS2 = n1;
			int n2 = S.Items[i].f20;
			if (n2 < min20) min20 = n1;
			if (n2 > max20) max20 = n1;
		}
		appPrintf("DS2=%X..%X  f20=%X..%X", minS2, maxS2, min20, max20);
#endif // DEBUG_BIO_BULK
		return Ar;
	}
};

struct BioBulkCatalog
{
	byte				Endian;	//?? or Platform: 0=PC, 1=XBox360, 2=PS3?
	int64				f4;
	int					fC;
	TArray<BioBulkCatalogFile> Files;

	friend FArchive& operator<<(FArchive &Ar, BioBulkCatalog &S)
	{
		Ar << S.Endian;
		if (S.Endian) Ar.ReverseBytes = true;
		Ar << S.f4 << S.fC << S.Files;
		return Ar;
	}
};

static TArray<BioBulkCatalog> bioCatalog;

static bool BioReadBulkCatalogFile(const CGameFileInfo *file)
{
	guard(BioReadBulkCatalogFile);
	FArchive *Ar = appCreateFileReader(file);
	// setup for reading Bioshock data
	Ar->ArVer         = 141;
	Ar->ArLicenseeVer = 0x38;
	Ar->Game          = GAME_Bioshock;
	// serialize
	appPrintf("Reading %s\n", file->RelativeName);
	BioBulkCatalog *cat = new (bioCatalog) BioBulkCatalog;
	*Ar << *cat;
	// finalize
	delete Ar;
	return true;
	unguardf("%s", file->RelativeName);
}


static void BioReadBulkCatalog()
{
	static bool ready = false;
	if (ready) return;
	ready = true;
	appEnumGameFiles(BioReadBulkCatalogFile, "bdc");
	if (!bioCatalog.Num()) appPrintf("WARNING: no *.bdc files found\n");
}

static byte *FindBioTexture(const UTexture *Tex)
{
	int needSize = Tex->CachedBulkDataSize & 0xFFFFFFFF;
#if DEBUG_BIO_BULK
	appPrintf("Search for ... %s (size=%X)\n", Tex->Name, needSize);
#endif
	BioReadBulkCatalog();
	for (int i = 0; i < bioCatalog.Num(); i++)
	{
		BioBulkCatalog &Cat = bioCatalog[i];
		for (int j = 0; j < Cat.Files.Num(); j++)
		{
			const BioBulkCatalogFile &File = Cat.Files[j];
			for (int k = 0; k < File.Items.Num(); k++)
			{
				const BioBulkCatalogItem &Item = File.Items[k];
				if (!strcmp(Tex->Name, *Item.ObjectName))
				{
					if (abs(needSize - Item.DataSize) > 0x4000)		// differs in 16k
					{
#if DEBUG_BIO_BULK
						appPrintf("... Found %s in %s with wrong BulkDataSize %X (need %X)\n", Tex->Name, *File.Filename, Item.DataSize, needSize);
#endif
						continue;
					}
#if DEBUG_BIO_BULK
					appPrintf("... Found %s in %s at %X size %X (%dx%d fmt=%d bpp=%g strip:%d mips:%d)\n", Tex->Name, *File.Filename, Item.DataOffset, Item.DataSize,
						Tex->USize, Tex->VSize, Tex->Format, (float)Item.DataSize / (Tex->USize * Tex->VSize),
						Tex->HasBeenStripped, Tex->StrippedNumMips);
#endif
					// found
					const CGameFileInfo *bulkFile = appFindGameFile(*File.Filename);
					if (!bulkFile)
					{
						// no bulk file
						appPrintf("Decompressing %s: %s is missing\n", Tex->Name, *File.Filename);
						return NULL;
					}

					appPrintf("Reading %s mip level %d (%dx%d) from %s\n", Tex->Name, 0, Tex->USize, Tex->VSize, bulkFile->RelativeName);
					FArchive *Reader = appCreateFileReader(bulkFile);
					Reader->Seek(Item.DataOffset);
					byte *buf = (byte*)appMalloc(max(Item.DataSize, needSize));
					Reader->Serialize(buf, Item.DataSize);
					delete Reader;
					return buf;
				}
			}
		}
	}
#if DEBUG_BIO_BULK
	appPrintf("... Bulk for %s was not found\n", Tex->Name);
#endif
	return NULL;
}

#endif // BIOSHOCK

bool UTexture::GetTextureData(CTextureData &TexData) const
{
	guard(UTexture::GetTextureData);

	TexData.Platform           = PLATFORM_PC;
	TexData.OriginalFormatEnum = Format;
	TexData.OriginalFormatName = EnumToName(Format);
	TexData.Obj                = this;
	TexData.Palette            = Palette;

	const FArchive* PackageAr = GetPackageArchive();

	// process external sources for some games
#if BIOSHOCK
	if (PackageAr && PackageAr->Game == GAME_Bioshock && CachedBulkDataSize) //?? check bStripped or Baked ?
	{
		byte* CompressedData = FindBioTexture(this);	// may be NULL
		if (CompressedData)
		{
			CMipMap* DstMip = new (TexData.Mips) CMipMap;
			DstMip->CompressedData = CompressedData;
			DstMip->ShouldFreeData = true;
			DstMip->USize = USize;
			DstMip->VSize = VSize;
			DstMip->DataSize = (int)CachedBulkDataSize;
		}
		TexData.Platform = PackageAr->Platform;
	}
#endif // BIOSHOCK
#if UC2
	if (PackageAr && PackageAr->Engine() == GAME_UE2X)
	{
		// try to find texture inside XBox xpr files
		int DataSize;
		byte* CompressedData = FindXprData(Name, &DataSize);	// may be NULL
		if (CompressedData)
		{
			CMipMap* DstMip = new (TexData.Mips) CMipMap;
			DstMip->CompressedData = CompressedData;
			DstMip->ShouldFreeData = true;
			DstMip->USize = USize;
			DstMip->VSize = VSize;
			DstMip->DataSize = DataSize;
		}
	}
#endif // UC2

	if (TexData.Mips.Num() == 0)
	{
		// texture was not taken from external source
		for (int n = 0; n < Mips.Num(); n++)
		{
			// find 1st mipmap with non-null data array
			// reference: DemoPlayerSkins.utx/DemoSkeleton have null-sized 1st 2 mips
			const FMipmap &Mip = Mips[n];
			if (!Mip.DataArray.Num())
				continue;
			CMipMap* DstMip = new (TexData.Mips) CMipMap;
			DstMip->CompressedData = &Mip.DataArray[0];
			DstMip->ShouldFreeData = false;
			DstMip->USize = Mip.USize;
			DstMip->VSize = Mip.VSize;
			DstMip->DataSize = Mip.DataArray.Num();
			break;
		}
	}

	ETexturePixelFormat intFormat;

	//?? return old code back - UE1 and UE2 differs in codes 6 and 7 only
	if (Package && (PackageAr->Engine() == GAME_UE1))
	{
		// UE1 has different ETextureFormat layout
		switch (Format)
		{
		case 0:
			intFormat = TPF_P8;
			break;
//		case 1:
//			intFormat = TPF_RGB32; // in script source code: TEXF_RGB32, but TEXF_RGBA7 in .h
//			break;
//		case 2:
//			intFormat = TPF_RGB64; // in script source code: TEXF_RGB64, but TEXF_RGB16 in .h
//			break;
		case 3:
			intFormat = TPF_DXT1;
			break;
		case 4:
			intFormat = TPF_RGB8;
			break;
		case 5:
			intFormat = TPF_BGRA8;
			break;
		// newer UE1 versions has DXT3 and DXT5
		case 6:
			intFormat = TPF_DXT3;
			break;
		case 7:
			intFormat = TPF_DXT5;
			break;
		default:
			appNotify("Unknown UE1 texture format: %d", Format);
			return false;
		}
	}
	else
	{
		// UE2
		switch (Format)
		{
		case TEXF_P8:
			intFormat = TPF_P8;
			break;
		case TEXF_DXT1:
			intFormat = TPF_DXT1;
			break;
		case TEXF_RGB8:
			intFormat = TPF_RGB8;
			break;
		case TEXF_RGBA8:
			intFormat = TPF_BGRA8;
			break;
		case TEXF_DXT3:
			intFormat = TPF_DXT3;
			break;
		case TEXF_DXT5:
			intFormat = TPF_DXT5;
			break;
		case TEXF_L8:
			intFormat = TPF_G8;
			break;
		case TEXF_CxV8U8:
			intFormat = TPF_V8U8_2;
			break;
		case TEXF_DXT5N:
			intFormat = TPF_DXT5N;
			break;
		case TEXF_3DC:
			intFormat = TPF_BC5;
			break;
		default:
			appNotify("Unknown UE2 texture format: %s (%d)", TexData.OriginalFormatName, Format);
			return false;
		}
	}

	TexData.Format = intFormat;

#if BIOSHOCK && SUPPORT_XBOX360
	if (TexData.Mips.Num() && TexData.Platform == PLATFORM_XBOX360)
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
#endif // BIOSHOCK && SUPPORT_XBOX360
#if BIOSHOCK
	if (Package && PackageAr->Game == GAME_Bioshock)
	{
		// This game has DataSize stored for all mipmaps, we should compute side of 1st mipmap
		// in order to accept this value when uploading texture to video card (some vendors rejects
		// large values)
		//?? Place code to CTextureData method?
		const CPixelFormatInfo &Info = PixelFormatInfo[intFormat];
		int numBlocks = TexData.Mips[0].USize * TexData.Mips[0].VSize / (Info.BlockSizeX * Info.BlockSizeY);	// used for validation only
		int requiredDataSize = numBlocks * Info.BytesPerBlock;
		if (requiredDataSize > TexData.Mips[0].DataSize)
		{
			appNotify("Bioshock texture %s: data too small; %dx%d, requires %X bytes, got %X\n",
				Name, TexData.Mips[0].USize, TexData.Mips[0].VSize, requiredDataSize, TexData.Mips[0].DataSize);
		}
		else if (requiredDataSize < TexData.Mips[0].DataSize)
		{
//			appPrintf("Bioshock texture %s: stripping data size from %X to %X\n", Name, TexData.Mips[0].DataSize, requiredDataSize);
			TexData.Mips[0].DataSize = requiredDataSize;
		}
	}
#endif // BIOSHOCK

	return (TexData.Mips.Num() > 0);

	unguardf("%s", Name);
}
