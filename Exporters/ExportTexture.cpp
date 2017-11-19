#include "UnTextureNVTT.h"

#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial3.h"

#include "Exporters.h"


#define TGA_SAVE_BOTTOMLEFT	1


#define TGA_ORIGIN_MASK		0x30
#define TGA_BOTLEFT			0x00
#define TGA_BOTRIGHT		0x10					// unused
#define TGA_TOPLEFT			0x20
#define TGA_TOPRIGHT		0x30					// unused

#if _MSC_VER
#pragma pack(push,1)
#endif

struct GCC_PACK tgaHdr_t
{
	byte 	id_length, colormap_type, image_type;
	uint16	colormap_index, colormap_length;
	byte	colormap_size;
	uint16	x_origin, y_origin;				// unused
	uint16	width, height;
	byte	pixel_size, attributes;
};

#if _MSC_VER
#pragma pack(pop)
#endif


bool GNoTgaCompress = false;
bool GExportDDS = false;

//?? place this function outside (cannot place to Core - using FArchive)

void WriteTGA(FArchive &Ar, int width, int height, byte *pic)
{
	guard(WriteTGA);

	int		i;

	byte *src;
	int size = width * height;
	// convert RGB to BGR (inplace!)
	for (i = 0, src = pic; i < size; i++, src += 4)
		Exchange(src[0], src[2]);

	// check for 24 bit image possibility
	int colorBytes = 3;
	for (i = 0, src = pic + 3; i < size; i++, src += 4)
		if (src[0] != 255)									// src initialized with offset 3
		{
			colorBytes = 4;
			break;
		}

	byte *packed = (byte*)appMalloc(width * height * colorBytes);
	byte *threshold = packed + width * height * colorBytes - 16; // threshold for "dst"

	src = pic;
	byte *dst = packed;
	int column = 0;
	byte *flag = NULL;
	bool rle = false;

	bool useCompression = true;
	if (GNoTgaCompress)
		threshold = dst;									// will break compression loop immediately

	for (i = 0; i < size; i++)
	{
		if (dst >= threshold)								// when compressed is too large, same uncompressed
		{
			useCompression = false;
			break;
		}

		byte b = *src++;
		byte g = *src++;
		byte r = *src++;
		byte a = *src++;

		if (column < width - 1 &&							// not on screen edge; NOTE: when i == size-1, col==width-1
			b == src[0] && g == src[1] && r == src[2] && a == src[3] &&	// next pixel will be the same
			!(rle && flag && *flag == 254))					// flag overflow
		{
			if (!rle || !flag)
			{
				// starting new RLE sequence
				flag = dst++;
				*flag = 128 - 1;							// will be incremented below
				*dst++ = b; *dst++ = g; *dst++ = r;			// store RGB
				if (colorBytes == 4) *dst++ = a;			// store alpha
			}
			(*flag)++;										// enqueue one more texel
			rle = true;
		}
		else
		{
			if (rle)
			{
				// previous block was RLE, and next (now: current) byte was
				// the same - enqueue it to previous block and close block
				(*flag)++;
				flag = NULL;
			}
			else
			{
				if (column == 0)							// check for screen edge
					flag = NULL;
				if (!flag)
				{
					// start new copy sequence
					flag = dst++;
					*flag = 255;
				}
				*dst++ = b; *dst++ = g; *dst++ = r;			// store RGB
				if (colorBytes == 4) *dst++ = a;			// store alpha
				(*flag)++;
				if (*flag == 127) flag = NULL;				// check for overflow
			}
			rle = false;
		}

		if (++column == width) column = 0;
	}

	// write header
	tgaHdr_t header;
	memset(&header, 0, sizeof(header));
	header.width  = width;
	header.height = height;
#if 0
	// debug: write black/white image
	header.pixel_size = 8;
	header.image_type = 3;
	fwrite(&header, 1, sizeof(header), f);
	for (i = 0; i < width * height; i++, pic += 4)
	{
		int c = (pic[0]+pic[1]+pic[2]) / 3;
		fwrite(&c, 1, 1, f);
	}
#else
	header.pixel_size = colorBytes * 8;
#if TGA_SAVE_BOTTOMLEFT
	header.attributes = TGA_BOTLEFT;
#else
	header.attributes = TGA_TOPLEFT;
#endif
	if (useCompression)
	{
		header.image_type = 10;		// RLE
		// write data
		Ar.Serialize(&header, sizeof(header));
		Ar.Serialize(packed, dst - packed);
	}
	else
	{
		header.image_type = 2;		// uncompressed
		// convert to 24 bits image, when needed
		if (colorBytes == 3)
			for (i = 0, src = dst = pic; i < size; i++)
			{
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				src++;
			}
		// write data
		Ar.Serialize(&header, sizeof(header));
		Ar.Serialize(pic, size * colorBytes);
	}
#endif

	appFree(packed);

	unguard;
}


static void WriteDDS(const CTextureData &TexData, const char *Filename)
{
	guard(WriteDDS);

	if (!TexData.Mips.Num()) return;
	const CMipMap& Mip = TexData.Mips[0];

	unsigned fourCC = TexData.GetFourCC();

	// code from CTextureData::Decompress()
	if (!fourCC)
		appError("unknown texture format %d \n", TexData.Format);	// should not happen - IsDXT() should not pass execution here

	nv::DDSHeader header;
	header.setFourCC(fourCC & 0xFF, (fourCC >> 8) & 0xFF, (fourCC >> 16) & 0xFF, (fourCC >> 24) & 0xFF);
//	header.setPixelFormat(32, 0xFF, 0xFF << 8, 0xFF << 16, 0xFF << 24);	// bit count and per-channel masks
	//!! Note: should use setFourCC for compressed formats, and setPixelFormat for uncompressed - these functions are
	//!! incompatible. When fourcc is used, color masks are zero, and vice versa.
	header.setWidth(Mip.USize);
	header.setHeight(Mip.VSize);
//	header.setNormalFlag(TexData.Format == TPF_DXT5N || TexData.Format == TPF_3DC); -- required for decompression only
	header.setLinearSize(Mip.DataSize);

	appMakeDirectoryForFile(Filename);

	byte headerBuffer[128];							// DDS header is 128 bytes long
	memset(headerBuffer, 0, 128);
	WriteDDSHeader(headerBuffer, header);
	FArchive *Ar = new FFileWriter(Filename);
	Ar->Serialize(headerBuffer, 128);
	Ar->Serialize(const_cast<byte*>(Mip.CompressedData), Mip.DataSize);
	delete Ar;

	unguard;
}


void ExportTexture(const UUnrealMaterial *Tex)
{
	guard(ExportTexture);

	if (GDontOverwriteFiles)
	{
		if (CheckExportFilePresence(Tex, "%s.tga", Tex->Name)) return;
		if (CheckExportFilePresence(Tex, "%s.dds", Tex->Name)) return;
		if (CheckExportFilePresence(Tex, "%s.png", Tex->Name)) return;
	}

	// for UTexture3 (UE3+), can check SourceArt for PNG data and save it if available
	if (Tex->IsA("Texture3"))
	{
		// Note: there are several SourceArt texture formats available. We're detecting PNG
		// by checking for magic value, however it is possible to analyze bPNGCompressed and
		// ETextureSourceFormat enum to determine exact format.
		const UTexture3* Tex3 = static_cast<const UTexture3*>(Tex);
		const FByteBulkData& Bulk = Tex3->SourceArt;
		if (Bulk.BulkData && memcmp(Bulk.BulkData, "\x89PNG", 4) == 0)
		{
			FArchive *Ar = CreateExportArchive(Tex, "%s.png", Tex->Name);
			if (Ar)
			{
				Ar->Serialize(Bulk.BulkData, Bulk.ElementCount * Bulk.GetElementSize());
				delete Ar;
			}
			return;
		}
	}

	byte *pic = NULL;
	int width, height;

	CTextureData TexData;
	if (Tex->GetTextureData(TexData))
	{
		if (GExportDDS && TexData.IsDXT())
		{
			WriteDDS(TexData, GetExportFileName(Tex, "%s.dds", Tex->Name));
			return;
		}

		width = TexData.Mips[0].USize;
		height = TexData.Mips[0].VSize;
		pic = TexData.Decompress();
	}

	if (!pic)
	{
		appPrintf("WARNING: texture %s has no valid mipmaps\n", Tex->Name);
		// produce 1x1-pixel tga
		// should erase file?
		width = height = 1;
		pic = new byte[4];
	}

#if TGA_SAVE_BOTTOMLEFT
	// flip image vertically (UnrealEd for UE2 have a bug with importing TGA_TOPLEFT images,
	// it simply ignores orientation flags)
	for (int i = 0; i < height / 2; i++)
	{
		byte *p1 = pic + width * 4 * i;
		byte *p2 = pic + width * 4 * (height - i - 1);
		for (int j = 0; j < width * 4; j++)
			Exchange(p1[j], p2[j]);
	}
#endif

	FArchive *Ar = CreateExportArchive(Tex, "%s.tga", Tex->Name);
	if (Ar)
	{
		WriteTGA(*Ar, width, height, pic);
		delete Ar;
	}

	delete pic;

	Tex->ReleaseTextureData();

	unguard;
}
