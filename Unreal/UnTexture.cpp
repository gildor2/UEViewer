#include "UnTextureNVTT.h"

#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial2.h"		// for UPalette

#if SUPPORT_IPHONE
#	include <PVRTDecompress.h>
#endif

#if SUPPORT_ANDROID
#	include "libs/astc/astc_codec_internals.h"
#endif

#include <detex.h>

#if 0
#	define PROFILE_DDS(cmd)		cmd
#else
#	define PROFILE_DDS(cmd)
#endif

//#define DEBUG_XBOX360_TEX		1

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


const CPixelFormatInfo PixelFormatInfo[] =
{
	// FourCC				BlockSizeX	BlockSizeY BytesPerBlock X360AlignX	X360AlignY	Name
	{ 0,						1,			1,			1,			0,			0,			"P8"		},	// TPF_P8
	{ 0,						1,			1,			1,			64,			64,			"G8"		},	// TPF_G8
//	{																									},	// TPF_G16
	{ 0,						1,			1,			3,			0,			0,			"RGB8"		},	// TPF_RGB8
	{ 0,						1,			1,			4,			32,			32,			"RGBA8"		},	// TPF_RGBA8
	{ 0,						1,			1,			4,			32,			32,			"BGRA8"		},	// TPF_BGRA8
	{ BYTES4('D','X','T','1'),	4,			4,			8,			128,		128,		"DXT1"		},	// TPF_DXT1
	{ BYTES4('D','X','T','3'),	4,			4,			16,			128,		128,		"DXT3"		},	// TPF_DXT3
	{ BYTES4('D','X','T','5'),	4,			4,			16,			128,		128,		"DXT5"		},	// TPF_DXT5
	{ BYTES4('D','X','T','5'),	4,			4,			16,			128,		128,		"DXT5N"		},	// TPF_DXT5N
	{ 0,						1,			1,			2,			64,			32,			"V8U8"		},	// TPF_V8U8
	{ 0,						1,			1,			2,			64,			32,			"V8U8"		},	// TPF_V8U8_2
	{ BYTES4('A','T','I','2'),	4,			4,			16,			0,			0,			"BC5"		},	// TPF_BC5
	{ 0,						4,			4,			16,			0,			0,			"BC7"		},	// TPF_BC7
	{ 0,						8,			1,			1,			0,			0,			"A1"		},	// TPF_A1
	{ 0,						1,			1,			2,			0,			0,			"RGBA4"		},	// TPF_RGBA4
#if SUPPORT_IPHONE
	{ 0,						8,			4,			8,			0,			0,			"PVRTC2"	},	// TPF_PVRTC2
	{ 0,						4,			4,			8,			0,			0,			"PVRTC4"	},	// TPF_PVRTC4
#endif
#if SUPPORT_ANDROID
	{ 0,						4,			4,			8,			0,			0,			"ETC1"		},	// TPF_ETC1
	{ 0,						4,			4,			8,			0,			0,			"ETC2_RGB"	},	// TPF_ETC2_RGB
	{ 0,						4,			4,			16,			0,			0,			"ETC2_RGBA"	},	// TPF_ETC2_RGBA
	{ 0,						4,			4,			16,			0,			0,			"ATC_4x4"	},	// TPF_ASTC_4x4
	{ 0,						6,			6,			16,			0,			0,			"ATC_6x6"	},	// TPF_ASTC_6x6
	{ 0,						8,			8,			16,			0,			0,			"ATC_8x8"	},	// TPF_ASTC_8x8
	{ 0,						10,			10,			16,			0,			0,			"ATC_10x10"	},	// TPF_ASTC_10x10
	{ 0,						12,			12,			16,			0,			0,			"ATC_12x12"	},	// TPF_ASTC_12x12
#endif
};


unsigned CTextureData::GetFourCC() const
{
	return PixelFormatInfo[Format].FourCC;
}


bool CTextureData::IsDXT() const
{
	return (Mips.Num() > 0) && (PixelFormatInfo[Format].FourCC != 0);
}


byte *CTextureData::Decompress(int MipLevel)
{
	guard(CTextureData::Decompress);

	if (!Mips.IsValidIndex(MipLevel))
		return NULL;

	const CMipMap& Mip = Mips[MipLevel];

	// Get mip map data
	int USize = Mip.USize;
	int VSize = Mip.VSize;
	const byte *Data = Mip.CompressedData;

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
			memcpy(dst, Data, USize * VSize * 4);
		}
		return dst;
	case TPF_BGRA8:
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
	case TPF_RGBA4:
		{
			const byte *s = Data;
			byte *d = dst;
			for (int i = 0; i < USize * VSize; i++)
			{
				byte b1 = s[0];
				byte b2 = s[1];
				// BGRA -> RGBA
				*d++ = b2 & 0xF0;
				*d++ = (b2 & 0xF) << 4;
				*d++ = b1 & 0xF0;
				*d++ = (b1 & 0xF) << 4;
				s += 2;
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
					d[2] = 255 - 255 * appFloor(sqrt(t));	//!! TODO: check for correct function here - should be (t+1.0)*127.5, at least for 'offset==0'
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

#if SUPPORT_IPHONE
	case TPF_PVRTC2:
	case TPF_PVRTC4:
		PROFILE_DDS(appResetProfiler());
		PVRTDecompressPVRTC(Data, Format == TPF_PVRTC2, USize, VSize, dst);
		PROFILE_DDS(appPrintProfiler());
		return dst;
#endif // SUPPORT_IPHONE

#if SUPPORT_ANDROID
	case TPF_ETC1:
#if 1
		PROFILE_DDS(appResetProfiler());
		PVRTDecompressETC(Data, USize, VSize, dst, 0);
		PROFILE_DDS(appPrintProfiler());
#else
		{
			// NOTE: this code works well too
			detexTexture tex;
			tex.format = DETEX_TEXTURE_FORMAT_ETC1;
			tex.data = const_cast<byte*>(Data);	// will be used as 'const' anyway
			tex.width = USize;
			tex.height = VSize;
			tex.width_in_blocks = USize / 4;
			tex.height_in_blocks = VSize / 4;
			PROFILE_DDS(appResetProfiler());
			detexDecompressTextureLinear(&tex, dst, DETEX_PIXEL_FORMAT_RGBA8);
			PROFILE_DDS(appPrintProfiler());
		}
#endif
		return dst;
	case TPF_ETC2_RGB:
		{
			detexTexture tex;
			tex.format = DETEX_TEXTURE_FORMAT_ETC2;
			tex.data = const_cast<byte*>(Data);	// will be used as 'const' anyway
			tex.width = USize;
			tex.height = VSize;
			tex.width_in_blocks = USize / 4;
			tex.height_in_blocks = VSize / 4;
			PROFILE_DDS(appResetProfiler());
			detexDecompressTextureLinear(&tex, dst, DETEX_PIXEL_FORMAT_RGBA8);
			PROFILE_DDS(appPrintProfiler());
		}
		return dst;
	case TPF_ETC2_RGBA:
		{
			detexTexture tex;
			tex.format = DETEX_TEXTURE_FORMAT_ETC2_EAC;
			tex.data = const_cast<byte*>(Data);	// will be used as 'const' anyway
			tex.width = USize;
			tex.height = VSize;
			tex.width_in_blocks = USize / 4;
			tex.height_in_blocks = VSize / 4;
			PROFILE_DDS(appResetProfiler());
			detexDecompressTextureLinear(&tex, dst, DETEX_PIXEL_FORMAT_RGBA8);
			PROFILE_DDS(appPrintProfiler());
		}
		return dst;
	case TPF_ASTC_4x4:
	case TPF_ASTC_6x6:
	case TPF_ASTC_8x8:
	case TPF_ASTC_10x10:
	case TPF_ASTC_12x12:
		{
			static bool initialized = false;
			if (!initialized)
			{
				build_quantization_mode_table();
				initialized = true;
			}
			int blockDim = PixelFormatInfo[Format].BlockSizeX;
			assert(PixelFormatInfo[Format].BlockSizeY == blockDim);
			int xBlocks = (USize + blockDim - 1) / blockDim;
			int yBlocks = (VSize + blockDim - 1) / blockDim;
			const int xdim = blockDim, ydim = blockDim, zdim = 1, z = 0;
			const astc_decode_mode decode_mode = DECODE_LDR;
			static const swizzlepattern swz_decode = { 0, 1, 2, 3 };
			imageblock pb;

			astc_codec_image* img = allocate_image(8 /*bitness*/, USize, VSize, 1 /*zsize*/, 0);
			initialize_image(img);

			for (int y = 0; y < yBlocks; y++)
			{
				for (int x = 0; x < xBlocks; x++)
				{
					int offset = ((y * xBlocks) + x) * 16;
					const byte* bp = Data + offset;
					physical_compressed_block pcb = *(physical_compressed_block *) bp;
					symbolic_compressed_block scb;
					physical_to_symbolic(xdim, ydim, zdim, pcb, &scb);
					decompress_symbolic_block(decode_mode, xdim, ydim, zdim, x * xdim, y * ydim, z * zdim, &scb, &pb);
					write_imageblock(img, &pb, xdim, ydim, zdim, x * xdim, y * ydim, z * zdim, swz_decode);
				}
			}

			memcpy(dst, img->imagedata8[0][0], size);

			if (isNormalmap)
			{
				// UE4 drops blue channel for normal maps before encoding, restore it
				byte *d = dst;
				for (int i = 0; i < USize * VSize; i++)
				{
					byte u = d[0];
					byte v = d[1];
					assert(d[2] == 0);
					float uf = u / 255.0f * 2 - 1;
					float vf = v / 255.0f * 2 - 1;
					float t  = 1.0f - uf * uf - vf * vf;
					if (t >= 0)
						d[2] = appFloor((t + 1.0f) * 127.5f);
					else
						d[2] = 255;
					d += 4;
				}
			}

			destroy_image(img);
		}
		return dst;
#endif // SUPPORT_ANDROID

	case TPF_BC7:
		{
			detexTexture tex;
			tex.format = DETEX_TEXTURE_FORMAT_BPTC;
			tex.data = const_cast<byte*>(Data);	// will be used as 'const' anyway
			tex.width = USize;
			tex.height = VSize;
			tex.width_in_blocks = USize / 4;
			tex.height_in_blocks = VSize / 4;
			PROFILE_DDS(appResetProfiler());
			detexDecompressTextureLinear(&tex, dst, DETEX_PIXEL_FORMAT_RGBA8);
			PROFILE_DDS(appPrintProfiler());
		}
		return dst;
	}

	staticAssert(ARRAY_COUNT(PixelFormatInfo) == TPF_MAX, Wrong_PixelFormatInfo_array);
	unsigned fourCC = PixelFormatInfo[Format].FourCC;
	if (!fourCC)
	{
		appNotify("Unable to unpack texture %s: unsupported texture format %s\n", Obj->Name, PixelFormatInfo[Format].Name);
		memset(dst, 0xFF, size);
		return dst;
	}

	PROFILE_DDS(appResetProfiler());

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

	PROFILE_DDS(appPrintProfiler());

	if (Format == TPF_DXT1)
		PostProcessAlpha(dst, USize, VSize);	//??

	return dst;
	unguardf("fmt=%s(%d)", OriginalFormatName, OriginalFormatEnum);
}


/*-----------------------------------------------------------------------------
	XBox360 texture decompression
-----------------------------------------------------------------------------*/

#if SUPPORT_XBOX360

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

// Untile decompressed texture.
// This function also removes U alignment when originalWidth < tiledWidth
// Note: this function is not used, and now it is outdated. See UntileCompressedXbox360Texture
// for more details.
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

// Untile compressed texture - it will remains compressed, but in PC format instead of XBox360.
// This function also removes U alignment when originalWidth < tiledWidth
//!! Note: this function doesn't work well with non-square textures - UModel will not crash, but textures
//!! will not appear correctly. Example (from Gears of War 3):
//!!   umodel GearGame.xxx -game=gowj T_Ramp_Right_To_Left
static void UntileCompressedXbox360Texture(const byte *src, byte *dst, int tiledWidth, int originalWidth, int tiledHeight, int originalHeight, int blockSizeX, int blockSizeY, int bytesPerBlock)
{
	guard(UntileCompressedXbox360Texture);

	int tiledBlockWidth     = tiledWidth / blockSizeX;			// width of image in blocks
	int originalBlockWidth  = originalWidth / blockSizeX;		// width of image in blocks
	int tiledBlockHeight    = tiledHeight / blockSizeY;			// height of image in blocks
	int originalBlockHeight = originalHeight / blockSizeY;		// height of image in blocks
	int logBpp              = appLog2(bytesPerBlock);

	// XBox360 has packed multiple lower mip levels into a single tile - should use special code
	// to unpack it.
	// Packing looks like this:
	// ....CCCCBBBBBBBBAAAAAAAAAAAAAAAA
	// ....CCCCBBBBBBBBAAAAAAAAAAAAAAAA
	// E.......BBBBBBBBAAAAAAAAAAAAAAAA
	// ........BBBBBBBBAAAAAAAAAAAAAAAA
	// DD..............AAAAAAAAAAAAAAAA
	// ................AAAAAAAAAAAAAAAA
	// ................AAAAAAAAAAAAAAAA
	// ................AAAAAAAAAAAAAAAA
	// (Where mips are A,B,C,D,E - E is 1x1, D is 2x2 etc)
	// Force sxOffset=0 and enable DEBUG_MIPS in UnRender.cpp to visualize this layout.
	// So we should offset X coordinate when unpacking to the width of mip level.
	// Note: this doesn't work with non-square textures.
	int sxOffset = 0;
	if ((tiledBlockWidth >= originalBlockWidth * 2) && (originalWidth == 16))
	{
		sxOffset = originalBlockWidth;
#if DEBUG_XBOX360_TEX
		appPrintf("sxOffset=%d\n", sxOffset);
#endif
	}

	int numImageBlocks = tiledBlockWidth * tiledBlockHeight;	// used for verification

	// iterate image blocks
	for (int dy = 0; dy < originalBlockHeight; dy++)
	{
		for (int dx = 0; dx < originalBlockWidth; dx++)
		{
			unsigned swzAddr = GetTiledOffset(dx + sxOffset, dy, tiledBlockWidth, logBpp);	// do once for whole block
			assert(swzAddr < numImageBlocks);
			int sy = swzAddr / tiledBlockWidth;
			int sx = swzAddr % tiledBlockWidth;

			byte       *pDst = dst + (dy * originalBlockWidth + dx) * bytesPerBlock;
			const byte *pSrc = src + (sy * tiledBlockWidth    + sx) * bytesPerBlock;
			memcpy(pDst, pSrc, bytesPerBlock);
		}
	}
	unguard;
}


bool CTextureData::DecodeXBox360(int MipLevel)
{
	guard(CTextureData::DecodeXBox360);

#if DEBUG_XBOX360_TEX
	if (MipLevel == 0)
	{
		appPrintf("Texture %s in format %s has %d mips:\n", Obj->Name, OriginalFormatName, Mips.Num());
		for (int i = 0; i < Mips.Num(); i++)
		{
			const CMipMap& Mip = Mips[i];
			appPrintf("%d: %d x %d, 0x%X bytes\n", i, Mip.USize, Mip.VSize, Mip.DataSize);
		}
	}
#endif // DEBUG_XBOX360_TEX

	if (!Mips.IsValidIndex(MipLevel))
		return false;
	CMipMap& Mip = Mips[MipLevel];

	const CPixelFormatInfo &Info = PixelFormatInfo[Format];

	char ErrorMessage[256];
	if (!Info.X360AlignX)
	{
		strcpy(ErrorMessage, "unsupported texture format");

	error:
		Mip.ReleaseData();
		appNotify("ERROR: DecodeXBox360 %s'%s' mip %d (%s=%d): %s", Obj->GetClassName(), Obj->Name, MipLevel,
			OriginalFormatName, OriginalFormatEnum, ErrorMessage);
		return false;
	}

	int USize1 = Align(Mip.USize, Info.X360AlignX);
	int VSize1 = Align(Mip.VSize, Info.X360AlignY);
	int UBlockSize = USize1 / Info.BlockSizeX;
	int VBlockSize = VSize1 / Info.BlockSizeY;
	int TotalBlocks = Mip.DataSize / Info.BytesPerBlock;

	float bpp = (float)Mip.DataSize / (USize1 * VSize1) * Info.BlockSizeX * Info.BlockSizeY;	// used for validation only
#if DEBUG_XBOX360_TEX
	appPrintf("DecodeXBox360: %s'%s': %d x %d (%d x %d aligned), %s, %d bpp (format), %g bpp (real), %d bytes\n", Obj->GetClassName(), Obj->Name,
		Mip.USize, Mip.VSize, USize1, VSize1, OriginalFormatName, Info.BytesPerBlock, bpp, Mip.DataSize);
#endif

	if (UBlockSize * VBlockSize > TotalBlocks)
	{
//		VSize1 = TotalBlocks / UBlockSize * Info.BlockSizeY;
#if DEBUG_XBOX360_TEX
		appPrintf("... can't fit aligned texture to a tile, dropping mip\n");
#endif
		return false;
	}

#if BIOSHOCK
	// some verification
	float rate = bpp / Info.BytesPerBlock;
	if (Obj->GetGame() == GAME_Bioshock && (rate >= 1 && rate < 1.5f))	// allow placing of mipmaps into this buffer
		bpp = Info.BytesPerBlock;
#endif // BIOSHOCK

	if (fabs(bpp - Info.BytesPerBlock) > 0.001f)
	{
		// Some XBox360 games (Lollipop Chainsaw, ...) has lower mip level (16x16) with half or 1/4 of data size - allow that.
		// TODO: review this code, perhaps useless due to recent changes in lower mipmap levels support. At least, check
		// 'bpp * 2' and 'bpp * 4' cases.
		if ( (Mip.VSize >= 32) || ( (bpp * 2 != Info.BytesPerBlock) && (bpp * 4 != Info.BytesPerBlock) ) )
		{
			appSprintf(ARRAY_ARG(ErrorMessage), "bytesPerBlock: got %g, need %d", bpp, Info.BytesPerBlock);
			goto error;
		}
	}

	// untile and unalign
	byte *buf = (byte*)appMalloc(Mip.DataSize);   	// older code: 'Mip.DataSize * 16'; perhaps should use Mip.USize * Mip.VSize * BytesPerPixel
	UntileCompressedXbox360Texture(Mip.CompressedData, buf, USize1, Mip.USize, VSize1, Mip.VSize, Info.BlockSizeX, Info.BlockSizeY, Info.BytesPerBlock);

	// swap bytes
	if (Format == TPF_RGBA8 || Format == TPF_BGRA8)
	{
		// Swap dwords for 32-bit formats
		appReverseBytes(buf, Mip.DataSize / 4, 4);
	}
	else if (Info.BytesPerBlock > 1)
	{
		// Swap words for everything else
		appReverseBytes(buf, Mip.DataSize / 2, 2);
	}

	// release old CompressedData
	Mip.ReleaseData();
	Mip.CompressedData = buf;
	Mip.ShouldFreeData = true;			// data were allocated here ...
	Mip.DataSize = (Mip.USize / Info.BlockSizeX) * (Mip.VSize / Info.BlockSizeY) * Info.BytesPerBlock; // essential for exporting

	return true;	// no error

	unguard;
}

#endif // SUPPORT_XBOX360
