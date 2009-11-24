#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"


#define USE_NVIMAGE			1
//#define PROFILE_DDS			1


#if !USE_NVIMAGE
#	include "libs/ddslib.h"				// texture decompression
#else
#	include <nvcore/StdStream.h>
#	include <nvimage/Image.h>
#	include <nvimage/DirectDrawSurface.h>
#	undef __FUNC__						// conflicted with our guard macros
#endif // USE_NVIMAGE


// replaces random 'alpha=0' color with black
static void PostProcessAlpha(byte *pic, int width, int height)
{
	for (int pixelCount = width * height; pixelCount > 0; pixelCount--, pic += 4)
	{
		if (pic[3] != 0)	// not completely transparent
			continue;
		pic[0] = pic[1] = pic[2] = 0;
	}
}


byte *DecompressTexture(const byte *Data, int width, int height, ETextureFormat SrcFormat,
	const char *Name, UPalette *Palette)
{
	guard(DecompressTexture);
	int size = width * height * 4;
	byte *dst = new byte [size];

#if 0
	{
		// visualize UV map
		memset(dst, 0xFF, size);
		byte *d = dst;
		for (int i = 0; i < width * height; i++, d += 4)
		{
			int x = i % width;
			int y = i / width;
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
	switch (SrcFormat)
	{
	case TEXF_P8:
		{
			if (!Palette)
			{
				appNotify("DecompressTexture: TEXF_P8 with NULL palette");
				memset(dst, 0xFF, size);
				return dst;
			}
			byte *d = dst;
			for (int i = 0; i < width * height; i++)
			{
				const FColor &c = Palette->Colors[Data[i]];
				*d++ = c.R;
				*d++ = c.G;
				*d++ = c.B;
				*d++ = c.A;
			}
		}
		return dst;
	case TEXF_RGB8:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < width * height; i++)
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
	case TEXF_RGBA8:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < width * height; i++)
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
	case TEXF_L8:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < width * height; i++)
			{
				byte b = *s++;
				*d++ = b;
				*d++ = b;
				*d++ = b;
				*d++ = 255;
			}
		}
		return dst;
	case TEXF_CxV8U8:
		{
			//!! not checked (Republic Commando possibly has textures of such type)
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < width * height; i++)
			{
				byte u = *s++;
				byte v = *s++;
				d[1] = u;
				d[2] = v;
#if 0
				float uf = (u - 128.0f) / 128.0f;
				float vf = (v - 128.0f) / 128.0f;
				d[0] = (1 - uf * uf - vf * vf) * 128 + 128;
#else
				d[0] = 0;
#endif
				d[3] = 255;
				d += 4;
			}
		}
		return dst;
	}

	unsigned fourCC;

	switch (SrcFormat)
	{
	case TEXF_DXT1:
		fourCC = BYTES4('D','X','T','1');
		break;
	case TEXF_DXT3:
		fourCC = BYTES4('D','X','T','3');
		break;
	case TEXF_DXT5:
	case TEXF_DXT5N:
		fourCC = BYTES4('D','X','T','5');
		break;
	case TEXF_3DC:
		fourCC = BYTES4('A','T','I','2');
		break;
	default:
		appNotify("%s: unknown texture format %d \n", Name, SrcFormat);
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
	dds.width  = width;
	dds.height = height;
	dds.data   = const_cast<byte*>(Data);
	if (DDSDecompress(&dds, dst) != 0)
		appError("Error in DDSDecompress");

#else // USE_NVIMAGE

	nv::DDSHeader header;
	header.setFourCC(fourCC & 0xFF, (fourCC >> 8) & 0xFF, (fourCC >> 16) & 0xFF, (fourCC >> 24) & 0xFF);
	header.setWidth(width);
	header.setHeight(height);
	header.setNormalFlag(SrcFormat == TEXF_DXT5N || SrcFormat == TEXF_3DC);	// flag to restore normalmap from 2 colors
	nv::MemoryInputStream input(Data, width * height * 4);	//!! size is incorrect, it is greater than should be

	nv::DirectDrawSurface dds(header, &input);
	nv::Image image;
	dds.mipmap(&image, 0, 0);

	byte *s = (byte*)image.pixels();
	byte *d = dst;
	bool hasAlpha = image.format() == nv::Image::Format_ARGB;

	for (int i = 0; i < width * height; i++, s += 4, d += 4)
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

	if (SrcFormat == TEXF_DXT1)
		PostProcessAlpha(dst, width, height);

	return dst;
	unguardf(("fmt=%d", SrcFormat));
}
