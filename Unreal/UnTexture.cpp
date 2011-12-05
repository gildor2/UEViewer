#define USE_NVIMAGE			1			//!! always used in ExportTexture()

#if !USE_NVIMAGE
#	include "libs/ddslib.h"				// texture decompression
#else
#	include <nvcore/StdStream.h>
#	include <nvimage/Image.h>
#	include <nvimage/DirectDrawSurface.h>
#	undef __FUNC__						// conflicted with our guard macros
#endif // USE_NVIMAGE


#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnPackage.h"

#include "UnMaterial2.h"
#include "UnMaterial3.h"


#if IPHONE
#	include <PVRTDecompress.h>
#endif

//#define PROFILE_DDS			1
//#define XPR_DEBUG			1

/*-----------------------------------------------------------------------------
	Texture decompression
-----------------------------------------------------------------------------*/

// replaces random 'alpha=0' color with black
static void PostProcessAlpha(byte *pic, int width, int height)
{
	guard(PostProcessAlpha);

	for (int pixelCount = width * height; pixelCount > 0; pixelCount--, pic += 4)
	{
		if (pic[3] != 0)	// not completely transparent
			continue;
		pic[0] = pic[1] = pic[2] = 0;
	}

	unguard;
}


struct CPixelFormatInfo
{
	unsigned	FourCC;				// 0 when not DDS-compatible
	byte		BlockSizeX;
	byte		BlockSizeY;
	byte		BytesPerBlock;
	byte		X360Align;			// 0 when unknown or not supported on XBox360
};

static const CPixelFormatInfo PixelFormatInfo[] =
{
	// FourCC					BlockSizeX	BlockSizeY	BytesPerBlock	X360Align
	{ 0,						1,			1,			1,				0			},	// TPF_P8
	{ 0,						1,			1,			1,				64			},	// TPF_G8
//	{																				},	// TPF_G16
	{ 0,						1,			1,			3,				0			},	// TPF_RGB8
	{ 0,						1,			1,			4,				32			},	// TPF_RGBA8
	{ BYTES4('D','X','T','1'),	4,			4,			8,				128			},	// TPF_DXT1
	{ BYTES4('D','X','T','3'),	4,			4,			16,				128			},	// TPF_DXT3
	{ BYTES4('D','X','T','5'),	4,			4,			16,				128			},	// TPF_DXT5
	{ BYTES4('D','X','T','5'),	4,			4,			16,				128			},	// TPF_DXT5N
	{ 0,						1,			1,			2,				0			},	// TPF_CxV8U8
	{ BYTES4('A','T','I','2'),	4,			4,			16,				0			},	// TPF_3DC
#if IPHONE
	{ 0,						8,			4,			8,				0			},	// TPF_PVRTC2
	{ 0,						4,			4,			8,				0			},	// TPF_PVRTC4
#endif
};


unsigned CTextureData::GetFourCC() const
{
	return PixelFormatInfo[Format].FourCC;
}


bool CTextureData::IsDXT() const
{
	return (
		CompressedData != NULL &&
		(Format == TPF_DXT1 || Format == TPF_DXT3 || Format == TPF_DXT5 || Format == TPF_DXT5N || Format == TPF_3DC) );
}


byte *CTextureData::Decompress()
{
	guard(CTextureData::Decompress);

	int size = USize * VSize * 4;
	byte *dst = new byte [size];

#if 0
	{
		// visualize UV map
		memset(dst, 0xFF, size);
		byte *d = dst;
		for (int i = 0; i < USize * VSize; i++, d += 4)
		{
			int x = i % USize;
			int y = i / USize;
			d[0] = d[1] = d[2] = 0;
			int x0 = x % 16;
			int y0 = y % 16;
			if (x0 == 0)			d[0] = 255;	// red - binormal axis
			else if (y0 == 0)		d[2] = 255;	// blue - tangent axis
			else if (x0 + y0 < 7)	d[1] = 128;	// gark green
		}
		return dst;
	}
#endif

	// process non-dxt formats here
	const byte *Data = CompressedData;
	switch (Format)
	{
	case TPF_P8:
		{
			if (!Palette)
			{
				appNotify("DecompressTexture: TPF_P8 with NULL palette");
				memset(dst, 0xFF, size);
				return dst;
			}
			byte *d = dst;
			for (int i = 0; i < USize * VSize; i++)
			{
				const FColor &c = Palette->Colors[Data[i]];
				*d++ = c.R;
				*d++ = c.G;
				*d++ = c.B;
				*d++ = c.A;
			}
		}
		return dst;
	case TPF_RGB8:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < USize * VSize; i++)
			{
				// BGRA -> RGBA
				*d++ = s[2];
				*d++ = s[1];
				*d++ = s[0];
				*d++ = 255;
				s += 3;
			}
		}
		return dst;
	case TPF_RGBA8:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < USize * VSize; i++)
			{
				// BGRA -> RGBA
				*d++ = s[2];
				*d++ = s[1];
				*d++ = s[0];
				*d++ = s[3];
				s += 4;
			}
		}
		return dst;
	case TPF_G8:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < USize * VSize; i++)
			{
				byte b = *s++;
				*d++ = b;
				*d++ = b;
				*d++ = b;
				*d++ = 255;
			}
		}
		return dst;
	case TPF_CxV8U8:		//!! bad for Tribes
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < USize * VSize; i++)
			{
				byte u = *s++;
				byte v = *s++;
				d[0] = u - 128;
				d[1] = v - 128;
				float uf = u / 255.0f * 2 - 1;
				float vf = v / 255.0f * 2 - 1;
				float t  = 1.0f - uf * uf - vf * vf;
				if (t >= 0)
					d[2] = 255 - 255 * appFloor(sqrt(t));
				else
					d[2] = 255;
				d[3] = 255;
				d += 4;
			}
		}
		return dst;
#if IPHONE
	case TPF_PVRTC2:
	case TPF_PVRTC4:
	#if PROFILE_DDS
		appResetProfiler();
	#endif
		PVRTDecompressPVRTC(Data, Format == TPF_PVRTC2, USize, VSize, dst);
	#if PROFILE_DDS
		appPrintProfiler();
	#endif
		return dst;
#endif // IPHONE
	}

	staticAssert(ARRAY_COUNT(PixelFormatInfo) == TPF_MAX, Wrong_PixelFormatInfo_array);
	unsigned fourCC = PixelFormatInfo[Format].FourCC;
	if (!fourCC)
	{
		appNotify("%s: unknown texture format %d \n", Obj->Name, Format);
		memset(dst, 0xFF, size);
		return dst;
	}

#if PROFILE_DDS
	appResetProfiler();
#endif

#if !USE_NVIMAGE

	// setup for DDSLib
	ddsBuffer_t dds;
	memcpy(dds.magic, "DDS ", 4);
	dds.pixelFormat.fourCC = fourCC;
	dds.size   = 124;
	dds.width  = USize;
	dds.height = VSize;
	dds.data   = const_cast<byte*>(Data);
	guard(DDSDecompress);
	if (DDSDecompress(&dds, dst) != 0)
		appError("Error in DDSDecompress");
	unguard;

#else // USE_NVIMAGE

	nv::DDSHeader header;
	header.setFourCC(fourCC & 0xFF, (fourCC >> 8) & 0xFF, (fourCC >> 16) & 0xFF, (fourCC >> 24) & 0xFF);
	header.setWidth(USize);
	header.setHeight(VSize);
	header.setNormalFlag(Format == TPF_DXT5N || Format == TPF_3DC);	// flag to restore normalmap from 2 colors
	nv::MemoryInputStream input(Data, USize * VSize * 4);	//!! size is incorrect, it is greater than it should be

	nv::DirectDrawSurface dds(header, &input);
	nv::Image image;
	guard(nv::DecompressDDS);
	dds.mipmap(&image, 0, 0);
	unguard;

	byte *s = (byte*)image.pixels();
	byte *d = dst;

	for (int i = 0; i < USize * VSize; i++, s += 4, d += 4)
	{
		// BGRA -> RGBA
		d[0] = s[2];
		d[1] = s[1];
		d[2] = s[0];
		d[3] = s[3];
	}

#endif // USE_NVIMAGE

#if PROFILE_DDS
	appPrintProfiler();
#endif

	if (Format == TPF_DXT1)
		PostProcessAlpha(dst, USize, VSize);	//??

	return dst;
	unguardf(("fmt=%s(%d)", OriginalFormatName, OriginalFormatEnum));
}


/*-----------------------------------------------------------------------------
	XBox360 texture decompression
-----------------------------------------------------------------------------*/

#if XBOX360

inline int appLog2(int n)
{
	int r;
	for (r = -1; n; n >>= 1, r++)
	{ /*empty*/ }
	return r;
}

// Input:
//		x/y		coordinate of block
//		width	width of image in blocks
//		logBpb	log2(bytesPerBlock)
// Reference:
//		XGAddress2DTiledOffset() from XDK
static unsigned GetTiledOffset(int x, int y, int width, int logBpb)
{
	assert(width <= 8192);
	assert(x < width);

	int alignedWidth = Align(width, 32);
	// top bits of coordinates
	int macro  = ((x >> 5) + (y >> 5) * (alignedWidth >> 5)) << (logBpb + 7);
	// lower bits of coordinates (result is 6-bit value)
	int micro  = ((x & 7) + ((y & 0xE) << 2)) << logBpb;
	// mix micro/macro + add few remaining x/y bits
	int offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4);
	// mix bits again
	return (((offset & ~0x1FF) << 3) +					// upper bits (offset bits [*-9])
			((y & 16) << 7) +							// next 1 bit
			((offset & 0x1C0) << 2) +					// next 3 bits (offset bits [8-6])
			(((((y & 8) >> 2) + (x >> 3)) & 3) << 6) +	// next 2 bits
			(offset & 0x3F)								// lower 6 bits (offset bits [5-0])
			) >> logBpb;
}

// Untile decompressed texture
// This function also removes U alignment when originalWidth < tiledWidth
static void UntileXbox360Texture(const unsigned *src, unsigned *dst, int tiledWidth, int originalWidth, int height, int blockSizeX, int blockSizeY, int bytesPerBlock)
{
	guard(UntileXbox360Texture);

	int blockWidth          = tiledWidth / blockSizeX;			// width of image in blocks
	int originalBlockWidth  = originalWidth / blockSizeX;		// width of image in blocks
	int blockHeight         = height / blockSizeY;				// height of image in blocks
	int logBpp              = appLog2(bytesPerBlock);

	int numImageBlocks = blockWidth * blockHeight;				// used for verification

	// iterate image blocks
	for (int y = 0; y < blockHeight; y++)
	{
		for (int x = 0; x < originalBlockWidth; x++)			// process only a part of image when originalWidth < tiledWidth
		{
			unsigned swzAddr = GetTiledOffset(x, y, blockWidth, logBpp);	// do once for whole block
			assert(swzAddr < numImageBlocks);
			int sy = swzAddr / blockWidth;
			int sx = swzAddr % blockWidth;
			// copy block per-pixel from [sx,sy] to [x,y]
			int y2 = y * blockSizeY;
			int y3 = sy * blockSizeY;
			for (int y1 = 0; y1 < blockSizeY; y1++, y2++, y3++)
			{
				// copy line of blockSizeX pixels
				int x2 = x * blockSizeX;
				int x3 = sx * blockSizeX;
				unsigned       *pDst = dst + y2 * originalWidth + x2;
				const unsigned *pSrc = src + y3 * tiledWidth + x3;
				for (int x1 = 0; x1 < blockSizeX; x1++)
					*pDst++ = *pSrc++;
			}
		}
	}
	unguard;
}

// Untile compressed texture
// This function also removes U alignment when originalWidth < tiledWidth
static void UntileXbox360TexturePacked(const byte *src, byte *dst, int tiledWidth, int originalWidth, int height, int blockSizeX, int blockSizeY, int bytesPerBlock)
{
	guard(UntileXbox360TexturePacked);

	int blockWidth          = tiledWidth / blockSizeX;			// width of image in blocks
	int originalBlockWidth  = originalWidth / blockSizeX;		// width of image in blocks
	int blockHeight         = height / blockSizeY;				// height of image in blocks
	int logBpp              = appLog2(bytesPerBlock);

	int numImageBlocks = blockWidth * blockHeight;				// used for verification

	// iterate image blocks
	for (int y = 0; y < blockHeight; y++)
	{
		for (int x = 0; x < originalBlockWidth; x++)
		{
			unsigned swzAddr = GetTiledOffset(x, y, blockWidth, logBpp);	// do once for whole block
			assert(swzAddr < numImageBlocks);
			int sy = swzAddr / blockWidth;
			int sx = swzAddr % blockWidth;

			byte       *pDst = dst + (y  * originalBlockWidth + x ) * bytesPerBlock;
			const byte *pSrc = src + (sy * blockWidth         + sx) * bytesPerBlock;
			memcpy(pDst, pSrc, bytesPerBlock);
		}
	}
	unguard;
}


void CTextureData::DecodeXBox360()
{
	guard(CTextureData::DecodeXBox360);

	if (!CompressedData)
		return;

	const CPixelFormatInfo &Info = PixelFormatInfo[Format];

	char ErrorMessage[256];
	if (!Info.X360Align)
	{
		strcpy(ErrorMessage, "unknown texture format");

	error:
		if (ShouldFreeData) delete CompressedData;
		CompressedData = NULL;
		appNotify("ERROR: DecodeXBox360 %s'%s' (%s=%d): %s", Obj->GetClassName(), Obj->Name,
			OriginalFormatName, OriginalFormatEnum, ErrorMessage);
	}

	int bytesPerBlock = Info.BytesPerBlock;
	int USize1 = Align(USize, Info.X360Align);
	int VSize1 = Align(VSize, Info.X360Align);

	float bpp = (float)DataSize / (USize1 * VSize1) * Info.BlockSizeX * Info.BlockSizeY;	// used for validation only

#if BIOSHOCK
	// some verification
	float rate = bpp / bytesPerBlock;
	if (Obj->Package->Game == GAME_Bioshock && (rate >= 1 && rate < 1.5f))	// allow placing of mipmaps into this buffer
		bpp = bytesPerBlock;
#endif // BIOSHOCK

	if (fabs(bpp - bytesPerBlock) > 0.001f)
	{
		appSprintf(ARRAY_ARG(ErrorMessage), "bytesPerBlock: got %g, need %d", bpp, bytesPerBlock);
		goto error;
	}

	// untile and unalign
	byte *buf = new byte[DataSize];
	UntileXbox360TexturePacked(CompressedData, buf, USize1, USize, VSize1, Info.BlockSizeX, Info.BlockSizeY, Info.BytesPerBlock);

	// swap bytes
	if (bytesPerBlock > 1)
		appReverseBytes(buf, DataSize / 2, 2);

	// release old CompressedData
	if (ShouldFreeData) delete CompressedData;
	CompressedData = buf;
	ShouldFreeData = true;			// data were allocated here ...
	DataSize       = (USize / Info.BlockSizeX) * (VSize / Info.BlockSizeY) * Info.BytesPerBlock; // essential for exporting

	return;		// no error

	unguard;
}

#endif // XBOX360


/*-----------------------------------------------------------------------------
	UTexture::Decompress (UE2)
-----------------------------------------------------------------------------*/

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

	unguardf(("%s", file->RelativeName));
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
				byte *buf = new byte[File->DataSize];
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
	unguardf(("%s", file->RelativeName));
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
			if (!strcmp(Tex->Name, Item.ObjectName))
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
				const CGameFileInfo *bulkFile = appFindGameFile(File.Filename);
				if (!bulkFile)
				{
					// no bulk file
					appPrintf("Decompressing %s: %s is missing\n", Tex->Name, *File.Filename);
					return NULL;
				}

				appPrintf("Reading %s mip level %d (%dx%d) from %s\n", Tex->Name, 0, Tex->USize, Tex->VSize, bulkFile->RelativeName);
				FArchive *Reader = appCreateFileReader(bulkFile);
				Reader->Seek(Item.DataOffset);
				byte *buf = new byte[max(Item.DataSize, needSize)];
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
	TexData.OriginalFormatName = EnumToName("ETextureFormat", Format);
	TexData.Obj                = this;
	TexData.Palette            = Palette;

	// process external sources for some games
#if BIOSHOCK
	if (Package && Package->Game == GAME_Bioshock && CachedBulkDataSize) //?? check bStripped or Baked ?
	{
		BioReadBulkCatalog();

		TexData.CompressedData = FindBioTexture(this);	// may be NULL
		TexData.ShouldFreeData = true;
		TexData.USize          = USize;
		TexData.VSize          = VSize;
		TexData.DataSize       = CachedBulkDataSize;
		TexData.Platform       = Package->Platform;
	}
#endif // BIOSHOCK
#if UC2
	if (Package && Package->Engine() == GAME_UE2X)
	{
		// try to find texture inside XBox xpr files
		TexData.CompressedData = FindXprData(Name, &TexData.DataSize);
		TexData.ShouldFreeData = true;
		TexData.USize          = USize;
		TexData.VSize          = VSize;
	}
#endif // UC2

	if (!TexData.CompressedData)
	{
		// texture was not taken from external source
		for (int n = 0; n < Mips.Num(); n++)
		{
			// find 1st mipmap with non-null data array
			// reference: DemoPlayerSkins.utx/DemoSkeleton have null-sized 1st 2 mips
			const FMipmap &Mip = Mips[n];
			if (!Mip.DataArray.Num())
				continue;
			TexData.CompressedData = &Mip.DataArray[0];
			TexData.USize          = Mip.USize;
			TexData.VSize          = Mip.VSize;
			TexData.DataSize       = Mip.DataArray.Num();
			break;
		}
	}

	ETexturePixelFormat intFormat;
	if (Format == TEXF_P8)
		intFormat = TPF_P8;
	else if (Format == TEXF_DXT1)
		intFormat = TPF_DXT1;
	else if (Format == TEXF_RGB8)
		intFormat = TPF_RGB8;
	else if (Format == TEXF_RGBA8)
		intFormat = TPF_RGBA8;
	else if (Format == TEXF_DXT3)
		intFormat = TPF_DXT3;
	else if (Format == TEXF_DXT5)
		intFormat = TPF_DXT5;
	else if (Format == TEXF_L8)
		intFormat = TPF_G8;
	else if (Format == TEXF_CxV8U8)
		intFormat = TPF_CxV8U8;
	else if (Format == TEXF_DXT5N)
		intFormat = TPF_DXT5N;
	else if (Format == TEXF_3DC)
		intFormat = TPF_3DC;
	else
	{
		appNotify("Unknown texture format: %s (%d)", TexData.OriginalFormatName, Format);
		return false;
	}

	TexData.Format = intFormat;

#if BIOSHOCK && XBOX360
	if (TexData.CompressedData && TexData.Platform == PLATFORM_XBOX360)
		TexData.DecodeXBox360();
#endif

	return (TexData.CompressedData != NULL);

	unguard;
}


#if UNREAL3

/*-----------------------------------------------------------------------------
	UTexture2D::Decompress (UE3)
-----------------------------------------------------------------------------*/

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

	unguardf(("TFC=%s Hash=%08X", TFCName, Hash));
}


static int GetRealTextureOffset_DCU(const UTexture2D *Obj)
{
	guard(GetRealTextureOffset_DCU);

	char ObjName[256];
	Obj->GetFullName(ARRAY_ARG(ObjName), true, true, true);
	unsigned Hash = appStrihash(ObjName);
	const char *TFCName = Obj->TextureFileCacheName;
	return GetRealTextureOffset_DCU_2(Hash, TFCName);

	unguardf(("%s.%s", Obj->Package->Name, Obj->Name));
}


UUIStreamingTextures::~UUIStreamingTextures()
{
	// free generated data
	int i;
	for (i = 0; i < Textures.Num(); i++)
		delete Textures[i];
	for (i = 0; i < Names.Num(); i++)
		free(Names[i]);				// allocated with strdup()
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
		char *name = strdup(nameBuf);
		Textures.AddItem(Tex);
		Names.AddItem(name);
		// setup UOnject
		Tex->Name         = name;
		Tex->Package      = Package;
		Tex->PackageIndex = INDEX_NONE;		// not really exported
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
				Mip->Data.BulkDataFlags = BULKDATA_NoData;
			else
				Mip->Data.BulkDataOffsetInFile = Offset - Mip->Data.BulkDataOffsetInFile - 1;
			appPrintf("OFFS: %X\n", Mip->Data.BulkDataOffsetInFile);
		}
#if 1
		appPrintf("%d: %s  {%08X} %dx%d %s [%08X + %08X]\n", i, *S.TextureFileCacheName, S.Hash,
			S.nWidth, S.nHeight, EnumToName("EPixelFormat", Tex->Format),
			S.BulkDataOffsetInFile, S.BulkDataSizeOnDisk
		);
#endif
	}
	appPrintf("... done\n");

	unguard;
}

#endif // DCU_ONLINE

bool UTexture2D::LoadBulkTexture(int MipIndex) const
{
	const CGameFileInfo *bulkFile = NULL;

	guard(UTexture2D::LoadBulkTexture);

	const FTexture2DMipMap &Mip = Mips[MipIndex];

	// Here: data is either in TFC file or in other package
	const char *bulkFileName = NULL, *bulkFileExt = NULL;
	if (stricmp(TextureFileCacheName, "None") != 0)
	{
		// TFC file is assigned
		//!! cache checking of tfc file(s) + cache handles (FFileReader)
		//!! note #1: there can be few cache files!
		//!! note #2: XMen (PC) has renamed tfc file after cooking (TextureFileCacheName value is wrong)
		bulkFileName = TextureFileCacheName;
		bulkFileExt  = "tfc";
		bulkFile = appFindGameFile(bulkFileName, bulkFileExt);
		if (!bulkFile)
			bulkFile = appFindGameFile(bulkFileName, "xxx");	// sometimes tfc has xxx extension
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
				bulkFileName = Exp2.ObjectName;
				bulkFileExt  = NULL;					// find package file
//				appPrintf("BULK: %s (%X)\n", *Exp2.ObjectName, Exp2.ExportFlags);
			}
		}
		if (!bulkFileName) return false;					// just in case
		bulkFile = appFindGameFile(bulkFileName, bulkFileExt);
	}

	if (!bulkFile)
	{
		appPrintf("Decompressing %s: %s.%s is missing\n", Name, bulkFileName, bulkFileExt ? bulkFileExt : "*");
		return false;
	}

	appPrintf("Reading %s mip level %d (%dx%d) from %s\n", Name, MipIndex, Mip.SizeX, Mip.SizeY, bulkFile->RelativeName);
	FArchive *Ar = appCreateFileReader(bulkFile);
	Ar->SetupFrom(*Package);
	FByteBulkData *Bulk = const_cast<FByteBulkData*>(&Mip.Data);	//!! const_cast
#if DCU_ONLINE
	if (Package->Game == GAME_DCUniverse && Bulk->BulkDataOffsetInFile < 0)
	{
		int Offset = GetRealTextureOffset_DCU(this);
		if (Offset < 0) return false;
		Bulk->BulkDataOffsetInFile = Offset - Bulk->BulkDataOffsetInFile - 1;
//		appPrintf("OFFS: %X\n", Bulk->BulkDataOffsetInFile);
	}
#endif // DCU_ONLINE
//	appPrintf("%X %X [%d] f=%X\n", Bulk, Bulk->BulkDataOffsetInFile, Bulk->ElementCount, Bulk->BulkDataFlags);
	Bulk->SerializeChunk(*Ar);
	delete Ar;
	return true;

	unguardf(("File=%s", bulkFile ? bulkFile->RelativeName : "none"));
}


bool UTexture2D::GetTextureData(CTextureData &TexData) const
{
	guard(UTexture2D::GetTextureData);

	TexData.OriginalFormatEnum = Format;
	TexData.OriginalFormatName = EnumToName("ETextureFormat", Format);
	TexData.Obj                = this;

	bool bulkChecked = false;
	for (int n = 0; n < Mips.Num(); n++)
	{
		// find 1st mipmap with non-null data array
		// reference: DemoPlayerSkins.utx/DemoSkeleton have null-sized 1st 2 mips
		const FTexture2DMipMap &Mip = Mips[n];
		const FByteBulkData &Bulk = Mip.Data;
		if (!Mip.Data.BulkData)
		{
			//?? Separate this function ?
			// check for external bulk
			//!! * -notfc cmdline switch
			//!! * material viewer: support switching mip levels (for xbox decompression testing)
			if (Bulk.BulkDataFlags & BULKDATA_NoData) continue;		// mip level is stripped
			if (!(Bulk.BulkDataFlags & BULKDATA_StoreInSeparateFile)) continue;
			// some optimization in a case of missing bulk file
			if (bulkChecked) continue;				// already checked for previous mip levels
			bulkChecked = true;
			if (!LoadBulkTexture(n)) continue;		// note: this could be called for any mip level, not for the 1st only
		}
		// this mipmap has data
		TexData.CompressedData = Bulk.BulkData;
		TexData.USize          = Mip.SizeX;
		TexData.VSize          = Mip.SizeY;
		TexData.DataSize       = Bulk.ElementCount * Bulk.GetElementSize();
		TexData.Platform       = Package->Platform;
		break;
	}

	ETexturePixelFormat intFormat;
	if (Format == PF_A8R8G8B8)
		intFormat = TPF_RGBA8;
	else if (Format == PF_DXT1)
		intFormat = TPF_DXT1;
	else if (Format == PF_DXT3)
		intFormat = TPF_DXT3;
	else if (Format == PF_DXT5)
		intFormat = TPF_DXT5;
	else if (Format == PF_G8)
		intFormat = TPF_G8;
	else if (Format == PF_V8U8)
		intFormat = TPF_CxV8U8;
	else if (Format == PF_BC5)
		intFormat = TPF_3DC;
#if MASSEFF
//??else if (Format == PF_NormapMap_LQ) -- seems not used
//??	intFormat = TPF_3DC;
	else if (Format == PF_NormalMap_HQ)
		intFormat = TPF_3DC;
#endif // MASSEFF
	else
	{
		appNotify("Unknown texture format: %s (%d)", TexData.OriginalFormatName, Format);
		return false;
	}

#if IPHONE
	if (Package->Platform == PLATFORM_IOS)
	{
		if (Format == PF_DXT1)
			intFormat = bForcePVRTC4 ? TPF_PVRTC4 : TPF_PVRTC2;
		if (Format == PF_DXT5)
			intFormat = TPF_PVRTC4;
	}
#endif // IPHONE

	TexData.Format = intFormat;

#if XBOX360
	if (TexData.Platform == PLATFORM_XBOX360)
		TexData.DecodeXBox360();
#endif

	return (TexData.CompressedData != NULL);

	unguard;
}


#endif // UNREAL3
