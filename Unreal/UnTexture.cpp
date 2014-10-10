#include "UnTextureNVTT.h"

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
	byte		X360AlignX;			// 0 when unknown or not supported on XBox360
	byte		X360AlignY;
};


static const CPixelFormatInfo PixelFormatInfo[] =
{
	// FourCC					BlockSizeX	BlockSizeY	BytesPerBlock	X360AlignX	X360AlignY
	{ 0,						1,			1,			1,				0,			0		},	// TPF_P8
	{ 0,						1,			1,			1,				64,			64		},	// TPF_G8
//	{																						},	// TPF_G16
	{ 0,						1,			1,			3,				0,			0,		},	// TPF_RGB8
	{ 0,						1,			1,			4,				32,			32		},	// TPF_RGBA8
	{ BYTES4('D','X','T','1'),	4,			4,			8,				128,		128		},	// TPF_DXT1
	{ BYTES4('D','X','T','3'),	4,			4,			16,				128,		128		},	// TPF_DXT3
	{ BYTES4('D','X','T','5'),	4,			4,			16,				128,		128		},	// TPF_DXT5
	{ BYTES4('D','X','T','5'),	4,			4,			16,				128,		128		},	// TPF_DXT5N
	{ 0,						1,			1,			2,				64,			32		},	// TPF_V8U8
	{ 0,						1,			1,			2,				64,			32		},	// TPF_V8U8_2
	{ BYTES4('A','T','I','2'),	4,			4,			16,				0,			0		},	// TPF_BC5
	{ 0,						4,			4,			16,				0,			0		},	// TPF_BC7
	{ 0,						8,			1,			1,				0,			0		},	// TPF_A1
#if IPHONE
	{ 0,						8,			4,			8,				0,			0		},	// TPF_PVRTC2
	{ 0,						4,			4,			8,				0,			0		},	// TPF_PVRTC4
#endif
#if ANDROID
	{ 0,						4,			4,			8,				0,			0		},	// TPF_ETC1
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
		(Format == TPF_DXT1 || Format == TPF_DXT3 || Format == TPF_DXT5 || Format == TPF_DXT5N || Format == TPF_BC5) );
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
	case TPF_V8U8:
	case TPF_V8U8_2:
		{
			const byte *s = Data;
			byte *d = dst;
			byte offset = (Format == TPF_V8U8) ? 128 : 0;
			for (int i = 0; i < USize * VSize; i++)
			{
				byte u = *s++ + offset;		// byte + byte -> byte, overflow is normal here
				byte v = *s++ + offset;
				d[0] = u;
				d[1] = v;
				float uf = (u - offset) / 255.0f * 2 - 1;
				float vf = (v - offset) / 255.0f * 2 - 1;
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
	case TPF_A1:
		appNotify("TPF_A1 unsupported");	//!! easy to do, but need samples - I've got some PF_A1 textures with no mipmaps inside
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

#if ANDROID
	case TPF_ETC1:
	#if PROFILE_DDS
		appResetProfiler();
	#endif
		PVRTDecompressETC(Data, USize, VSize, dst, 0);
	#if PROFILE_DDS
		appPrintProfiler();
	#endif
		return dst;
#endif // ANDROID
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

	nv::DDSHeader header;
	nv::Image image;
	header.setFourCC(fourCC & 0xFF, (fourCC >> 8) & 0xFF, (fourCC >> 16) & 0xFF, (fourCC >> 24) & 0xFF);
	header.setWidth(USize);
	header.setHeight(VSize);
	header.setNormalFlag(Format == TPF_DXT5N || Format == TPF_BC5);	// flag to restore normalmap from 2 colors
	DecodeDDS(Data, USize, VSize, header, image);

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

#if PROFILE_DDS
	appPrintProfiler();
#endif

	if (Format == TPF_DXT1)
		PostProcessAlpha(dst, USize, VSize);	//??

	return dst;
	unguardf("fmt=%s(%d)", OriginalFormatName, OriginalFormatEnum);
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
	if (!Info.X360AlignX)
	{
		strcpy(ErrorMessage, "unsupported texture format");

	error:
		ReleaseCompressedData();
		CompressedData = NULL;
		appNotify("ERROR: DecodeXBox360 %s'%s' (%s=%d): %s", Obj->GetClassName(), Obj->Name,
			OriginalFormatName, OriginalFormatEnum, ErrorMessage);
		return;
	}

	int bytesPerBlock = Info.BytesPerBlock;
	int USize1 = Align(USize, Info.X360AlignX);
	int VSize1 = Align(VSize, Info.X360AlignY);
	// NOTE: 16x16 textures will not work! (needs different untiling or no untiling at all?)

	float bpp = (float)DataSize / (USize1 * VSize1) * Info.BlockSizeX * Info.BlockSizeY;	// used for validation only

//	appPrintf("%s'%s': %d x %d (%d x %d aligned), %s, %d bpp (format), %g bpp (real), %d bytes\n", Obj->GetClassName(), Obj->Name,
//		USize, VSize, USize1, VSize1, OriginalFormatName, Info.BytesPerBlock, bpp, DataSize);

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
	byte *buf = (byte*)appMalloc(DataSize);
	UntileXbox360TexturePacked(CompressedData, buf, USize1, USize, VSize1, Info.BlockSizeX, Info.BlockSizeY, Info.BytesPerBlock);

	// swap bytes
	if (bytesPerBlock > 1)
		appReverseBytes(buf, DataSize / 2, 2);

	// release old CompressedData
	ReleaseCompressedData();
	CompressedData = buf;
	ShouldFreeData = (buf != NULL);			// data were allocated here ...
	DataSize       = (USize / Info.BlockSizeX) * (VSize / Info.BlockSizeY) * Info.BytesPerBlock; // essential for exporting

	return;		// no error

	unguard;
}

#endif // XBOX360


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
		Mips.Add(Count);
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
	TexData.OriginalFormatName = EnumToName("ETextureFormat", Format);
	TexData.Obj                = this;
	TexData.Palette            = Palette;

	// process external sources for some games
#if BIOSHOCK
	if (Package && Package->Game == GAME_Bioshock && CachedBulkDataSize) //?? check bStripped or Baked ?
	{
		TexData.CompressedData = FindBioTexture(this);	// may be NULL
		TexData.ShouldFreeData = (TexData.CompressedData != NULL);
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
		TexData.ShouldFreeData = (TexData.CompressedData != NULL);
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

	//?? return old code back - UE1 and UE2 differs in codes 6 and 7 only
	if (Package && (Package->Engine() == GAME_UE1))
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
			intFormat = TPF_RGBA8;
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
			intFormat = TPF_RGBA8;
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

#if BIOSHOCK && XBOX360
	if (TexData.CompressedData && TexData.Platform == PLATFORM_XBOX360)
		TexData.DecodeXBox360();
#endif
#if BIOSHOCK
	if (Package && Package->Game == GAME_Bioshock)
	{
		// This game has DataSize stored for all mipmaps, we should compute side of 1st mipmap
		// in order to accept this value when uploading texture to video card (some vendors rejects
		// large values)
		//?? Place code to CTextureData method?
		const CPixelFormatInfo &Info = PixelFormatInfo[intFormat];
		int bytesPerBlock = Info.BytesPerBlock;
		int numBlocks = TexData.USize * TexData.VSize / (Info.BlockSizeX * Info.BlockSizeY);	// used for validation only
		int requiredDataSize = numBlocks * Info.BytesPerBlock;
		if (requiredDataSize > TexData.DataSize)
		{
			appNotify("Bioshock texture %s: data too small; %dx%d, requires %X bytes, got %X\n",
				Name, TexData.USize, TexData.VSize, requiredDataSize, TexData.DataSize);
		}
		else if (requiredDataSize < TexData.DataSize)
		{
//			appPrintf("Bioshock texture %s: stripping data size from %X to %X\n", Name, TexData.DataSize, requiredDataSize);
			TexData.DataSize = requiredDataSize;
		}
	}
#endif // BIOSHOCK

	return (TexData.CompressedData != NULL);

	unguardf("%s", Name);
}


#if UNREAL3

/*-----------------------------------------------------------------------------
	UTexture/UTexture2D (Unreal engine 3)
-----------------------------------------------------------------------------*/

void UTexture3::Serialize(FArchive &Ar)
{
	guard(UTexture3::Serialize);

#if UNREAL4
	if (Ar.Game >= GAME_UE4)
	{
		Serialize4(Ar);
		return;
	}
#endif

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
	if (Ar.Tell() == Ar.GetStopper()) return;

	if (Ar.ArVer >= 674)
	{
		TArray<FTexture2DMipMap> CachedPVRTCMips;
		Ar << CachedPVRTCMips;
		if (CachedPVRTCMips.Num()) appPrintf("*** %s has %d PVRTC mips (%d normal mips)\n", Name, CachedPVRTCMips.Num(), Mips.Num()); //!!
	}

	if (Ar.ArVer >= 857)
	{
		int CachedFlashMipsMaxResolution;
		TArray<FTexture2DMipMap> CachedATITCMips;
		FByteBulkData CachedFlashMips;
		Ar << CachedFlashMipsMaxResolution;
		Ar << CachedATITCMips;
		CachedFlashMips.Serialize(Ar);
		if (CachedATITCMips.Num()) appPrintf("*** %s has %d ATITC mips (%d normal mips)\n", Name, CachedATITCMips.Num(), Mips.Num()); //!!
	}

#if PLA
	if (Ar.Game == GAME_PLA) goto skip_rest_quiet;
#endif
	if (Ar.ArVer >= 864) Ar << CachedETCMips;

	// some hack to support more games ...
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


bool UTexture2D::LoadBulkTexture(const TArray<FTexture2DMipMap> &MipsArray, int MipIndex, bool UseETC_TFC) const
{
	const CGameFileInfo *bulkFile = NULL;

	guard(UTexture2D::LoadBulkTexture);

	const FTexture2DMipMap &Mip = MipsArray[MipIndex];

	// Here: data is either in TFC file or in other package
	char bulkFileName[256];
	const char *bulkFileExt = NULL;
	if (stricmp(TextureFileCacheName, "None") != 0)
	{
		// TFC file is assigned
		//!! cache checking of tfc file(s) + cache handles (FFileReader)
		//!! note #1: there can be few cache files!
		//!! note #2: XMen (PC) has renamed tfc file after cooking (TextureFileCacheName value is wrong)
		strcpy(bulkFileName, TextureFileCacheName);
		bulkFileExt  = "tfc";
#if ANDROID
		if (UseETC_TFC) appSprintf(ARRAY_ARG(bulkFileName), "%s_ETC", *TextureFileCacheName);
#endif
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
				strcpy(bulkFileName,  Exp2.ObjectName);
				bulkFileExt  = NULL;					// find package file
//				appPrintf("BULK: %s (%X)\n", *Exp2.ObjectName, Exp2.ExportFlags);
			}
		}
		if (!bulkFileName[0]) return false;				// just in case
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
	Bulk->SerializeChunk(*Ar);
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

	const TArray<FTexture2DMipMap> *MipsArray = &Mips;
	bool AndroidCompression = false;

#if ANDROID
	if (!Mips.Num() && CachedETCMips.Num())
	{
		MipsArray = &CachedETCMips;
		AndroidCompression = true;
	}
#endif

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
				if (!LoadBulkTexture(*MipsArray, n, AndroidCompression)) continue;	// note: this could be called for any mip level, not for the 1st only
			}
			// this mipmap has data
			TexData.CompressedData = Bulk.BulkData;
			TexData.USize          = Mip.SizeX;
			TexData.VSize          = Mip.SizeY;
			TexData.DataSize       = Bulk.ElementCount * Bulk.GetElementSize();
			TexData.Platform       = Package->Platform;
#if 0
			FILE *f = fopen(va("%s-%s.dmp", Package->Name, Name), "wb");
			if (f)
			{
				fwrite(Bulk.BulkData, Bulk.ElementCount * Bulk.GetElementSize(), 1, f);
				fclose(f);
			}
#endif
			break;
		}
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
		intFormat = TPF_V8U8;
	else if (Format == PF_BC5)
		intFormat = TPF_BC5;
	else if (Format == PF_BC7)
		intFormat = TPF_BC7;
	else if (Format == PF_A1)
		intFormat = TPF_A1;
#if MASSEFF
//??else if (Format == PF_NormapMap_LQ) -- seems not used
//??	intFormat = TPF_BC5;
	else if (Format == PF_NormalMap_HQ)
		intFormat = TPF_BC5;
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
		else if (Format == PF_DXT5)
			intFormat = TPF_PVRTC4;
	}
#endif // IPHONE

#if ANDROID
	if (AndroidCompression)
	{
		if (Format == PF_DXT1)
			intFormat = TPF_ETC1;
//??		else if (Format == PF_DXT5)
//??			intFormat = TPF_ETC2_EAC; ???
	}
#endif // ANDROID

	TexData.Format = intFormat;

#if XBOX360
	if (TexData.Platform == PLATFORM_XBOX360)
		TexData.DecodeXBox360();
#endif

	return (TexData.CompressedData != NULL);

	unguardf("%s", Name);
}


#endif // UNREAL3


#if UNREAL4

/*-----------------------------------------------------------------------------
	UTexture/UTexture2D (Unreal engine 4)
-----------------------------------------------------------------------------*/

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

	if (bCooked)
	{
		//!! C:\Projects\Epic\UnrealEngine4\Engine\Source\Runtime\Engine\Private\TextureDerivedData.cpp
		//!! - UTexture::SerializeCookedPlatformData
		//!! - FTexturePlatformData::SerializeCooked
		//!! CubeMap has "Slices" - images are placed together into single bitmap?
		appError("SerializeCookedData");
	}

	unguard;
}

#endif // UNREAL4
