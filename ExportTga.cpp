#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"


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
	unsigned short colormap_index, colormap_length;
	byte	colormap_size;
	unsigned short x_origin, y_origin;				// unused
	unsigned short width, height;
	byte	pixel_size, attributes;
};

#if _MSC_VER
#pragma pack(pop)
#endif


void ExportTga(UTexture *Tex, FArchive &Ar)
{
	int		i;

	int width, height;
	byte *pic = Tex->Decompress(width, height);
	if (!pic)
	{
		appNotify("WARNING: texture %s has no valid mipmaps", *Tex->Name);
		return;		//?? should erase file ?
	}

	byte *src;
	int size = width * height;
	// convert RGB to BGR (inplace!)
	for (i = 0, src = pic; i < size; i++, src += 4)
		Exchange(src[0], src[2]);

	byte *packed = (byte*)appMalloc(width * height * 4);
	byte *threshold = packed + width * height * 4 - 16;		// threshold for "dst"

	src = pic;
	byte *dst = packed;
	int column = 0;
	byte *flag = NULL;
	bool rle = false;

	bool useCompression = true;
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
			b == src[0] && g == src[1] && r == src[2] && a == src[3] &&	// next byte will be the same
			!(rle && flag && *flag == 254))					// flag overflow
		{
			if (!rle || !flag)
			{
				// starting new RLE sequence
				flag = dst++;
				*flag = 128 - 1;							// will be incremented below
				*dst++ = b; *dst++ = g; *dst++ = r; *dst++ = a;
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
				if (!flag)
				{
					// start new copy sequence
					flag = dst++;
					*flag = 0 - 1;							// 255, to be exact
				}
				*dst++ = b; *dst++ = g; *dst++ = r; *dst++ = a;	// copy texel
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
	header.width      = width;
	header.height     = height;
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
	header.pixel_size = 32;			//!! support 24 bit too
	header.attributes = TGA_TOPLEFT;
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
		// write data
		Ar.Serialize(&header, sizeof(header));
		Ar.Serialize(pic, size * 4);
	}
#endif

	appFree(packed);
}
