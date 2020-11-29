#include <png.h>

#include "Core.h"
#include "UnCore.h"

struct PngReadCtx
{
	const byte* CompressedData;
	int CompressedSize;
	int ReadOffset;
};

struct PngWriteCtx
{
	TArray<byte>& CompressedData;

	PngWriteCtx(TArray<byte>& pDst)
	: CompressedData(pDst)
	{}
};

static void user_read_compressed(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PngReadCtx* ctx = (PngReadCtx*)png_get_io_ptr(png_ptr);
	if (ctx->ReadOffset + length > ctx->CompressedSize)
	{
		appError("Error in PNG data");
	}
	memcpy(data, ctx->CompressedData + ctx->ReadOffset, length);
	ctx->ReadOffset += length;
}

static void user_write_compressed(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PngWriteCtx* ctx = (PngWriteCtx*) png_get_io_ptr(png_ptr);
	int Offset = ctx->CompressedData.AddUninitialized(length);
	memcpy(ctx->CompressedData.GetData() + Offset, data, length);
}

static void user_flush_data(png_structp png_ptr)
{
}

static void user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
	appError("Error in PNG data: %s", error_msg);
}

static void user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
	appNotify("Warning in PNG: %s", warning_msg);
}

static void* user_malloc(png_structp /*png_ptr*/, png_size_t size)
{
	return appMallocNoInit(size);
}

static void user_free(png_structp /*png_ptr*/, png_voidp struct_ptr)
{
	appFree(struct_ptr);
}

bool UncompressPNG(const unsigned char* CompressedData, int CompressedSize, int Width, int Height, unsigned char* pic, bool bgra)
{
	guard(UncompressPNG);

	// Verify signature

	if (CompressedSize < 8)
	{
		return false;
	}
	if (png_sig_cmp((unsigned char*)CompressedData, 0, CompressedSize) != 0)
	{
		return false;
	}

	PngReadCtx ctx;
	ctx.CompressedData = CompressedData;
	ctx.CompressedSize = CompressedSize;
	ctx.ReadOffset = 0;

	// Read header

	png_structp png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, &ctx, user_error_fn, user_warning_fn, NULL, user_malloc, user_free);
	assert(png_ptr);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr);

	png_set_read_fn(png_ptr, &ctx, user_read_compressed);

	png_read_info(png_ptr, info_ptr);

	if (Width != png_get_image_width(png_ptr, info_ptr) || Height != png_get_image_height(png_ptr, info_ptr))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	int ColorType = png_get_color_type(png_ptr, info_ptr);
	int BitDepth = png_get_bit_depth(png_ptr, info_ptr);

	//?? Recreate png structures, otherwise png_read_png will throw an error
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, &ctx, user_error_fn, user_warning_fn, NULL, user_malloc, user_free);
	assert(png_ptr);
	info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr);
	png_set_read_fn(png_ptr, &ctx, user_read_compressed);

	// Rewind to PNF start because png_read_png will read image header again
	ctx.ReadOffset = 0;

//	Channels = info_ptr->channels;
//	Format = (ColorType & PNG_COLOR_MASK_COLOR) ? ERGBFormat::RGBA : ERGBFormat::Gray;

	// Decode image

	if (ColorType == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb(png_ptr);
	}

	if ((ColorType & PNG_COLOR_MASK_COLOR) == 0 && BitDepth < 8)
	{
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}

	// Insert alpha channel with full opacity for RGB images without alpha
	if ((ColorType & PNG_COLOR_MASK_ALPHA) == 0)
	{
		// png images don't set PNG_COLOR_MASK_ALPHA if they have alpha from a tRNS chunk, but png_set_add_alpha seems to be safe regardless
		if ((ColorType & PNG_COLOR_MASK_COLOR) == 0)
		{
			png_set_tRNS_to_alpha(png_ptr);
		}
		else if (ColorType == PNG_COLOR_TYPE_PALETTE)
		{
			png_set_tRNS_to_alpha(png_ptr);
		}
//		if (BitDepth == 8)
		{
			png_set_add_alpha(png_ptr, 0xff , PNG_FILLER_AFTER);
		}
//		else if (InBitDepth == 16)
//		{
//			png_set_add_alpha(png_ptr, 0xffff , PNG_FILLER_AFTER);
//		}
	}

	const int PixelChannels = 4; // (InFormat == ERGBFormat::Gray) ? 1 : 4;
	const int BytesPerPixel = 4; // (InBitDepth * PixelChannels) / 8;
	const int BytesPerRow = BytesPerPixel * Width;

	png_bytep* row_pointers = (png_bytep*) png_malloc(png_ptr, Height * sizeof(png_bytep));
	for (int i = 0; i < Height; i++)
	{
		row_pointers[i] = pic + i * BytesPerRow;
	}
	png_set_rows(png_ptr, info_ptr, row_pointers);

	// Use PNG_TRANSFORM_STRIP_16 to convert 16-bit textures to 8-bit
	int Transform = PNG_TRANSFORM_STRIP_16;
	if (bgra)
	{
		Transform |= PNG_TRANSFORM_BGR;
	}
//	if (BitDepth == 16)
//	{
//		Transform |= PNG_TRANSFORM_SWAP_ENDIAN;
//	}
	// Convert grayscale png to RGB if requested
	if ((ColorType & PNG_COLOR_MASK_COLOR) == 0 /* &&
		(InFormat == ERGBFormat::RGBA || InFormat == ERGBFormat::BGRA) */)
	{
		Transform |= PNG_TRANSFORM_GRAY_TO_RGB;
	}

	png_read_png(png_ptr, info_ptr, Transform, NULL);

	// Cleanup
	png_free(png_ptr, row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return true;

	unguard;
}

void CompressPNG(const unsigned char* pic, int Width, int Height, TArray<byte>& CompressedData)
{
	guard(CompressPNG);

	int PixelChannels = /*(RawFormat == ERGBFormat::Gray) ? 1 :*/ 3;

	// Verify alpha channels of texture, see the possibility to drop one. First pass: check if alpha is fully opaque
	const unsigned char* p = pic + 3;
	for (int i = Width * Height; i > 0; i--, p += 4)
	{
		if (*p != 255)
		{
			PixelChannels = 4;
			break;
		}
	}
	// Check again to see if image is fully transparent - will also remove the alpha channel
	if (PixelChannels == 4)
	{
		p = pic + 3;
		bool bAllZero = true;
		for (int i = Width * Height; i > 0; i--, p += 4)
		{
			if (*p != 0)
			{
				bAllZero = false;
				break;
			}
		}
		if (bAllZero) PixelChannels = 3;
	}

	unsigned char* newPic = NULL;
	if (PixelChannels == 3)
	{
		// Squeeze pixel information if newly allocated memory block
		newPic = new unsigned char[Width * Height * 3];
		const unsigned char* s = pic;
		unsigned char* d = newPic;
		for (int i = Width * Height; i > 0; i--)
		{
			*d++ = *s++;
			*d++ = *s++;
			*d++ = *s++;
			s++;
		}
		pic = newPic;
	}

	PngWriteCtx ctx(CompressedData);
	ctx.CompressedData.Empty(Width * Height * PixelChannels); // preallocate

	png_structp png_ptr	= png_create_write_struct(PNG_LIBPNG_VER_STRING, &ctx, user_error_fn, user_warning_fn);
	assert(png_ptr);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr);

	png_bytep* row_pointers = (png_bytep*) png_malloc( png_ptr, Height*sizeof(png_bytep) );

	const int RawBitDepth = 8;
	png_set_compression_level(png_ptr, 1); // zlib compression level: 0 (uncompressed), 1 (fast) - 9 (slow)
	png_set_IHDR(png_ptr, info_ptr, Width, Height, RawBitDepth, (PixelChannels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_write_fn(png_ptr, &ctx, user_write_compressed, user_flush_data);

	const int BytesPerPixel = (RawBitDepth * PixelChannels) / 8;
	const int BytesPerRow = BytesPerPixel * Width;

	for (int i = 0; i < Height; i++)
	{
		row_pointers[i]= (png_bytep)&pic[i * BytesPerRow];
	}
	png_set_rows(png_ptr, info_ptr, row_pointers);

	int Transform = PNG_TRANSFORM_IDENTITY;
	png_write_png(png_ptr, info_ptr, Transform, NULL);

	png_free(png_ptr, row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if (newPic)
	{
		delete[] newPic;
	}

	unguard;
}
