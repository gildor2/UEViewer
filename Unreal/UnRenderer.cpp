#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"

#include "UnrealMaterial/UnMaterial2.h"
#include "UnrealMaterial/UnMaterial3.h"

#if RENDERING

#include "GlWindow.h"

#include "Shaders.h"

#define MAX_IMG_SIZE			4096
#define BAD_TEXTURE				((GLuint) -2)	// the texture object has permanent error, don't try to upload it again

static UTexture *DefaultUTexture = NULL;
static GLuint GetDefaultTexNum();


//#define SHOW_SHADER_PARAMS	1
//#define PROFILE				1				// profile everything
//#define DEBUG_UPLOAD			1				// debug texture uploads
//#define DEBUG_MIPS			1				// use to debug decompression of lower mip levels, especially for XBox360


#if DEBUG_UPLOAD
// profiling
#define PROFILE_UPLOAD(...)		__VA_ARGS__
#define PROFILE_SHADER(...)		__VA_ARGS__
#else
// no profiling
#define PROFILE_UPLOAD(...)
#define PROFILE_SHADER(...)
#endif

#if MAX_DEBUG
#define SHOW_SHADER_PARAMS 1
bool GShowShaderParams = false;
#endif

/*-----------------------------------------------------------------------------
	Mipmapping and resampling
-----------------------------------------------------------------------------*/

//?? move to CoreGL

static void ResampleTexture(unsigned* in, int inwidth, int inheight, unsigned* out, int outwidth, int outheight)
{
	int		i;
	unsigned p1[MAX_IMG_SIZE], p2[MAX_IMG_SIZE];

	unsigned fracstep = (inwidth << 16) / outwidth;
	unsigned frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	float f, f1, f2;
	f = (float)inheight / outheight;
	for (i = 0, f1 = 0.25f * f, f2 = 0.75f * f; i < outheight; i++, out += outwidth, f1 += f, f2 += f)
	{
		unsigned *inrow  = in + inwidth * appFloor(f1);
		unsigned *inrow2 = in + inwidth * appFloor(f2);
		for (int j = 0; j < outwidth; j++)
		{
			int		n, r, g, b, a;
			byte	*pix;

			n = r = g = b = a = 0;
#define PROCESS_PIXEL(row,col)	\
	pix = (byte *)row + col[j];		\
	if (pix[3])	\
	{			\
		n++;	\
		r += *pix++; g += *pix++; b += *pix++; a += *pix;	\
	}
			PROCESS_PIXEL(inrow,  p1);
			PROCESS_PIXEL(inrow,  p2);
			PROCESS_PIXEL(inrow2, p1);
			PROCESS_PIXEL(inrow2, p2);
#undef PROCESS_PIXEL

			switch (n)		// NOTE: generic version ("x /= n") is 50% slower
			{
			// case 1 - divide by 1 - do nothing
			case 2: r >>= 1; g >>= 1; b >>= 1; a >>= 1; break;
			case 3: r /= 3;  g /= 3;  b /= 3;  a /= 3;  break;
			case 4: r >>= 2; g >>= 2; b >>= 2; a >>= 2; break;
			case 0: r = g = b = 0; break;
			}

			((byte *)(out+j))[0] = r;
			((byte *)(out+j))[1] = g;
			((byte *)(out+j))[2] = b;
			((byte *)(out+j))[3] = a;
		}
	}
}


static void MipMap(byte* in, int width, int height)
{
	width *= 4;		// sizeof(rgba)
	height >>= 1;
	byte *out = in;
	for (int i = 0; i < height; i++, in += width)
	{
		for (int j = 0; j < width; j += 8, out += 4, in += 8)
		{
			int		r, g, b, a, am, n;

			r = g = b = a = am = n = 0;
//!! should perform removing of alpha-channel when IMAGE_NOALPHA specified
//!! should perform removing (making black) color channel when alpha==0 (NOT ALWAYS?)
//!!  - should analyze shader, and it will not use blending with alpha (or no blending at all)
//!!    then remove alpha channel (require to process shader's *map commands after all other commands, this
//!!    can be done with delaying [map|animmap|clampmap|animclampmap] lines and executing after all)
#if 0
#define PROCESS_PIXEL(idx)	\
	if (in[idx+3])	\
	{	\
		n++;	\
		r += in[idx]; g += in[idx+1]; b += in[idx+2]; a += in[idx+3];	\
		am = max(am, in[idx+3]);	\
	}
#else
#define PROCESS_PIXEL(idx)	\
	{	\
		n++;	\
		r += in[idx]; g += in[idx+1]; b += in[idx+2]; a += in[idx+3];	\
		am = max(am, in[idx+3]);	\
	}
#endif
			PROCESS_PIXEL(0);
			PROCESS_PIXEL(4);
			PROCESS_PIXEL(width);
			PROCESS_PIXEL(width+4);
#undef PROCESS_PIXEL
			//!! NOTE: currently, always n==4 here
			switch (n)
			{
			// case 1 - divide by 1 - do nothing
			case 2:
				r >>= 1; g >>= 1; b >>= 1; a >>= 1;
				break;
			case 3:
				r /= 3; g /= 3; b /= 3; a /= 3;
				break;
			case 4:
				r >>= 2; g >>= 2; b >>= 2; a >>= 2;
				break;
			case 0:
				r = g = b = 0;
				break;
			}
			out[0] = r; out[1] = g; out[2] = b;
			// generate alpha-channel for mipmaps (don't let it be transparent)
			// dest alpha = (MAX(a[0]..a[3]) + AVG(a[0]..a[3])) / 2
			// if alpha = 255 or 0 (for all 4 points) -- it will holds its value
			out[3] = (am + a) / 2;
		}
	}
}


/*-----------------------------------------------------------------------------
	Uploading textures
-----------------------------------------------------------------------------*/

//?? move to CoreGL

static void GetImageDimensions(int width, int height, int* scaledWidth, int* scaledHeight)
{
	int sw, sh;
	for (sw = 1; sw < width;  sw <<= 1) ;
	for (sh = 1; sh < height; sh <<= 1) ;

	// scale down only when new image dimension is larger than 64 and
	// larger than 4/3 of original image dimension
	if (sw > 64 && sw > (width * 4 / 3))  sw >>= 1;
	if (sh > 64 && sh > (height * 4 / 3)) sh >>= 1;

	while (sw > MAX_IMG_SIZE) sw >>= 1;
	while (sh > MAX_IMG_SIZE) sh >>= 1;

	if (sw < 1) sw = 1;
	if (sh < 1) sh = 1;

	*scaledWidth  = sw;
	*scaledHeight = sh;
}

#if DEBUG_UPLOAD
#define DBG(msg, ...)		appPrintf("TexUp: " msg "\n", __VA_ARGS__);
#else
#define DBG(...)
#endif

// Decompress and upload a texture. Returns false when decompression failed.
static bool UploadTex(UUnrealMaterial* Tex, GLenum target, CTextureData& TexData, bool doMipmap, int slice = -1)
{
	guard(UploadTex);

	byte *pic = TexData.Decompress(0, slice);
	if (!pic)
	{
		// some internal decompression error, message should be already printed to log
		return false;
	}

	const CMipMap& Mip0 = TexData.Mips[0];

	/*----- Calculate internal dimensions of the new texture --------*/
	int scaledWidth = Mip0.USize;
	int scaledHeight = Mip0.VSize;
	bool isPowerOfTwo = (scaledWidth & (scaledWidth-1) == 0) && (scaledHeight & (scaledHeight-1) == 0);

	if ((!GL_SUPPORT(QGL_2_0) && !isPowerOfTwo) || (scaledWidth > MAX_IMG_SIZE) || (scaledHeight > MAX_IMG_SIZE))
	{
		GetImageDimensions(Mip0.USize, Mip0.VSize, &scaledWidth, &scaledHeight);
	}

	if (!isPowerOfTwo && TexData.Mips.Num() < 2)
	{
		// MipMap() function won't work correctly with np2 textures
		doMipmap = false;
	}

	bool floatTexture = PixelFormatInfo[TexData.Format].Float != 0;
	if (floatTexture)
	{
		// ResampleTexture will not work with floating point data
		scaledWidth = Mip0.USize;
		scaledHeight = Mip0.VSize;
	}

	// Resample texture if desired
	// Copy or resample texture to new buffer (we will generate mipmaps there later)
	if (Mip0.USize != scaledWidth || Mip0.VSize != scaledHeight)
	{
		byte *scaledPic = new byte [scaledWidth * scaledHeight * 4];
		DBG("resample %dx%d to %dx%d", Mip0.USize, Mip0.VSize, scaledWidth, scaledHeight);
		ResampleTexture((unsigned*)pic, Mip0.USize, Mip0.VSize, (unsigned*)scaledPic, scaledWidth, scaledHeight);
		// replace 'pic' with resampled texture data
		delete[] pic;
		pic = scaledPic;
	}

	/*------------- Determine texture format to upload --------------*/
	GLenum format;
	int alpha = 1; //?? image->alphaType;
	format = (alpha ? 4 : 3);
	if (floatTexture)
	{
		doMipmap = false;
		format = GL_RGBA32F_ARB;
	}

	/*------------------ Upload the image ---------------------------*/
	// First mipmap
	DBG("up_uncomp %X (%s, %d mips): %d %d (%d)", target, Tex->Name, TexData.Mips.Num(), Mip0.USize, Mip0.VSize, TexData.Mips.Num());
	glTexImage2D(target, 0, format, scaledWidth, scaledHeight, 0, GL_RGBA, floatTexture ? GL_FLOAT : GL_UNSIGNED_BYTE, pic);

	// Upload or build other mipmaps
	if (doMipmap && TexData.Mips.Num() > 1 && GL_SUPPORT(QGL_1_2)) // GL 1.2 is required for GL_TEXTURE_MAX_LEVEL
	{
		guard(UploadMips);
		DBG("upload mips %s", Tex->Name);
		// use provided mipmaps; assume all have power-of-2 dimensions
		glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, TexData.Mips.Num() - 1);
		for (int mipLevel = 1; mipLevel < TexData.Mips.Num(); mipLevel++)
		{
			const CMipMap& Mip = TexData.Mips[mipLevel];
			byte* pic = TexData.Decompress(mipLevel, slice);

#if DEBUG_MIPS
			// colorize mip levels
			static const FVector cc[] = { {1,1,0}, {0,1,1}, {1,0,1}, {1,0,0}, {0,1,0}, {0,0,1} };
			byte* d = pic;
			FVector v = cc[min(mipLevel-1, 5)];
			for (int i = 0; i < Mip.USize * Mip.VSize; i++, d += 4)
			{
				float r = d[0], g = d[1], b = d[2];
				float c = (r + g + b) / 3.0f;
				d[0] = byte(c * v.X);
				d[1] = byte(c * v.Y);
				d[2] = byte(c * v.Z);
			}
#endif // DEBUG_MIPS

			assert(pic);
			DBG("   mip %d x %d (%X)", Mip.USize, Mip.VSize, Mip.DataSize);
			glTexImage2D(target, mipLevel, format, Mip.USize, Mip.VSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
			delete pic;
		}
		unguard;
	}
	else if (doMipmap)
	{
		guard(BuildMips);
		// build mipmaps
		DBG("build mips %s", Tex->Name);
		int mipLevel = 0;
		while (scaledWidth > 1 || scaledHeight > 1)
		{
			MipMap(pic, scaledWidth, scaledHeight);
			mipLevel++;
			scaledWidth  >>= 1;
			scaledHeight >>= 1;
			if (scaledWidth < 1)  scaledWidth  = 1;
			if (scaledHeight < 1) scaledHeight = 1;
			glTexImage2D(target, mipLevel, format, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
		}
		unguard;
	}
	else
	{
		glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
	}

#if DEBUG_MIPS
	// blur textures to display lower mips
	glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 3.0f);
#endif

	delete[] pic;
	return true;

	unguard;
}


// Try to upload a compressed texture. Returns false when not uploaded (due to hardware restrictions etc).
static bool UploadCompressedTex(UUnrealMaterial* Tex, GLenum target, GLenum target2, CTextureData& TexData, bool doMipmap, int slice = -1)
{
	guard(UploadCompressedTex);

#if DEBUG_MIPS
	return false; // always decompress
#endif

	Tex->NormalUnpackExpr = NULL;

	// verify GL capabilities
	if (!GL_SUPPORT(QGL_1_4))
		return false;		// cannot automatically generate mipmaps
	// at this point we have Open GL 1.4+

	const CMipMap& Mip0 = TexData.Mips[0];

	//?? support some other formats too
	// TPF_V8U8 = GL_RG8_SNORM (GL3.1)
	// TPF_G8   = GL_LUMINANCE
	// Notes:
	// - most formats are uploaded with glTexImage2D(), not with glCompressedTexImage2D()
	// - formats has different extension requirements (not QGL_EXT_TEXTURE_COMPRESSION_S3TC)

	GLenum format0 = 4, format1 = GL_RGBA, format2 = 0;
	if (TexData.Format == TPF_BGRA8)
	{
		// GL 1.2 - avoid byte swapping, upload texture "as is"
		//?? support other uncompressed formats, like A1, G8 etc
		format1 = GL_BGRA;
		format2 = GL_UNSIGNED_BYTE;
	}
	else if (TexData.Format == TPF_RGBA4)
	{
		format2 = GL_UNSIGNED_SHORT_4_4_4_4;
	}
	else if (TexData.Format == TPF_FLOAT_RGBA && GL_SUPPORT(QGL_ARB_HALF_FLOAT_PIXEL))
	{
		format0 = GL_RGBA16F_ARB;
		format2 = GL_HALF_FLOAT_ARB;
	}

	// Allow slices. We're supporting only 6 slices.
	int dataSize = Mip0.DataSize;
	const byte* dataPtr = Mip0.CompressedData;
	if (slice >= 0)
	{
		dataSize /= 6;
		dataPtr += dataSize * slice;
	}

	if (format1 && format2)
	{
		glTexImage2D(target, 0, format0, Mip0.USize, Mip0.VSize, 0, format1, format2, dataPtr);
		//!! support uploading mipmaps here
		if (doMipmap)
			glGenerateMipmapEXT(target);
		return true;
	}

	GLenum format;
	switch (TexData.Format)
	{
	case TPF_DXT1:
		if (!GL_SUPPORT(QGL_EXT_TEXTURE_COMPRESSION_S3TC)) return false;
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case TPF_DXT3:
		if (!GL_SUPPORT(QGL_EXT_TEXTURE_COMPRESSION_S3TC)) return false;
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case TPF_DXT5:
		if (!GL_SUPPORT(QGL_EXT_TEXTURE_COMPRESSION_S3TC)) return false;
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
//	case TPF_V8U8:
//		if (!GL_SUPPORT(QGL_3_1)) return false;
//		format = GL_RG8_SNORM;
//		break;
	case TPF_BC4:
		if (!GL_SUPPORT(QGL_ARB_TEXTURE_COMPRESSION_RGTC)) return false;
		format = GL_COMPRESSED_RED_RGTC1;
		break;
	case TPF_BC5:
		if (!GL_SUPPORT(QGL_ARB_TEXTURE_COMPRESSION_RGTC)) return false;
		format = GL_COMPRESSED_RG_RGTC2;
		Tex->NormalUnpackExpr = "normal.z = sqrt(max(1.0 - normal.x * normal.x - normal.y * normal.y, 0.0));";
		break;
	case TPF_BC6H:
		if (!GL_SUPPORT(QGL_ARB_TEXTURE_COMPRESSION_BPTC)) return false;
		format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
		break;
	case TPF_BC7:
		if (!GL_SUPPORT(QGL_ARB_TEXTURE_COMPRESSION_BPTC)) return false;
		format = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
		break;
	default:
		return false;
	}

	if (doMipmap && (TexData.Mips.Num() == 1))
	{
		// don't generate mipmaps for small compressed images
		const CMipMap& Mip = TexData.Mips[0];
		if (Mip.USize < 32 || Mip.VSize < 32)
			doMipmap = false;
	}

	if (!doMipmap)
	{
		// no mipmaps required
		DBG("up (%s, %d no-mips): %d %d (%d) (%s) (0x%X)", Tex->Name, TexData.Mips.Num(), Mip0.USize, Mip0.VSize, TexData.Mips.Num(), TexData.OriginalFormatName, Mip0.DataSize);
		glCompressedTexImage2D(target, 0, format, Mip0.USize, Mip0.VSize, 0, dataSize, dataPtr);
		glTexParameteri(target2, GL_TEXTURE_MAX_LEVEL, 0);	// GL 1.2
	}
	else if (TexData.Mips.Num() > 1 && GL_SUPPORT(QGL_1_2)) // GL 1.2 is required for GL_TEXTURE_MAX_LEVEL
	{
		guard(UploadMips);
		// has mipmaps
		DBG("up (%s, %d mips): %d %d (%d) (%s) (0x%X)", Tex->Name, TexData.Mips.Num(), Mip0.USize, Mip0.VSize, TexData.Mips.Num(), TexData.OriginalFormatName, Mip0.DataSize);
		glTexParameteri(target2, GL_TEXTURE_MAX_LEVEL, TexData.Mips.Num() - 1);
		for (int mipLevel = 0; mipLevel < TexData.Mips.Num(); mipLevel++)
		{
			const CMipMap& Mip = TexData.Mips[mipLevel];
			// Slices
			dataSize = Mip.DataSize;
			dataPtr = Mip.CompressedData;
			if (slice >= 0)
			{
				dataSize /= 6;
				dataPtr += dataSize * slice;
			}
			// Upload
			glCompressedTexImage2D(target, mipLevel, format, Mip.USize, Mip.VSize, 0, dataSize, dataPtr);
			GLenum error = glGetError();
			DBG("   mip %d x %d (%X)", Mip.USize, Mip.VSize, Mip.DataSize);
			if (error != 0)
			{
				appPrintf("Failed to upload mip %d of texture %s in format 0x%04X: error 0x%X\n", mipLevel, Tex->Name, format, error);
				if (mipLevel > 0)
				{
					// Recover from error: when failed to upload lower mip levels, make it still working with previous mips
					glTexParameteri(target2, GL_TEXTURE_MAX_LEVEL, mipLevel - 1);	// GL 1.2
				}
				DBG("%d x %d (%X)", Mip.USize, Mip.VSize, Mip.DataSize);
				break;
			}
		}
		unguard;
	}
	else if (GL_SUPPORT(QGL_EXT_FRAMEBUFFER_OBJECT))
	{
		// code below generates mipmaps using GL 3.0 or GL_EXT_framebuffer_object
		DBG("up+build_mips (%s): %d %d (%d) (%s) (%d)", Tex->Name, Mip0.USize, Mip0.VSize, TexData.Mips.Num(), TexData.OriginalFormatName, Mip0.DataSize);
		glCompressedTexImage2D(target, 0, format, Mip0.USize, Mip0.VSize, 0, dataSize, dataPtr);
		if (target2 != GL_TEXTURE_CUBE_MAP_ARB)
		{
			glGenerateMipmapEXT(target2);
		}
	}
	else
	{
		// GL 1.4 - set GL_GENERATE_MIPMAP before uploading
		DBG("up+build_mips (%s): old code", Tex->Name);
		glTexParameteri(target2, GL_GENERATE_MIPMAP, GL_TRUE);
		glCompressedTexImage2D(target, 0, format, Mip0.USize, Mip0.VSize, 0, dataSize, dataPtr);
	}

	GLenum error = glGetError();
	if (error)
	{
		appPrintf("Failed to upload texture %s in format 0x%04X, error 0x%04X\n", Tex->Name, format, error);
		return false;
	}

	return true;

	unguard;
}


static int Upload2D(UUnrealMaterial *Tex, bool doMipmap, bool clampS, bool clampT)
{
	guard(Upload2D);

	CTextureData TexData;
	PROFILE_UPLOAD(appResetProfiler());
	if (!Tex->GetTextureData(TexData))
	{
		appPrintf("WARNING: %s %s has no valid mipmaps\n", Tex->GetClassName(), Tex->Name);
		return BAD_TEXTURE;
	}

	bool floatTexture = PixelFormatInfo[TexData.Format].Float != 0;
	if (floatTexture)
	{
		doMipmap = false;
	}

	GLuint TexNum;
	glGenTextures(1, &TexNum);
	glBindTexture(GL_TEXTURE_2D, TexNum);

	if (!UploadCompressedTex(Tex, GL_TEXTURE_2D, GL_TEXTURE_2D, TexData, doMipmap))
	{
		// upload uncompressed
		if (!UploadTex(Tex, GL_TEXTURE_2D, TexData, doMipmap))
		{
			glDeleteTextures(1, &TexNum);
			return BAD_TEXTURE;
		}
	}

	bool isDefault = (Tex->Package == NULL) && (Tex->Name == "Default");

	// setup min/max filter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, doMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);	// trilinear filter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isDefault ? GL_NEAREST : GL_LINEAR);

	// setup wrap flags
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampS ? GL_CLAMP : GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampT ? GL_CLAMP : GL_REPEAT);

	PROFILE_UPLOAD(appPrintf("Uploaded %s (%dx%d)\n", Tex->Name, TexData.Mips[0].USize, TexData.Mips[0].VSize); appPrintProfiler("..."));
	return TexNum;

	unguardf("%s'%s'", Tex->GetClassName(), Tex->Name);
}


static bool UploadCubeSide(UUnrealMaterial *Tex, int side, bool useSlices = false)
{
	guard(UploadCubeSide);

	CTextureData TexData;
	if (!Tex->GetTextureData(TexData))
	{
		appPrintf("WARNING: %s %s has no valid mipmaps\n", Tex->GetClassName(), Tex->Name);
		return false;
	}

#if 0
	byte *pic2 = pic;
	for (int i = 0; i < USize * VSize; i++)
	{
		*pic2++ = (side & 1) * 255;
		*pic2++ = (side & 2) * 255;
		*pic2++ = (side & 4) * 255;
		*pic2++ = 255;
	}
#endif

	int slice = useSlices ? side : -1;

	// Automatic mipmap generation doesn't work with cubemaps, so allow mipmaps only for
	// explicitly provided data.
	// https://www.opengl.org/sdk/docs/man/html/glGenerateMipmap.xhtml
	bool doMipmap = TexData.Mips.Num() > 1;

	GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side;
	if (!UploadCompressedTex(Tex, target, GL_TEXTURE_CUBE_MAP_ARB, TexData, doMipmap, slice))
	{
		// upload uncompressed
		if (!UploadTex(Tex, target, TexData, doMipmap, slice))
		{
			// some internal decompression error, message should be already displayed
			return false;
		}
	}

	if (side == 5)
	{
		// the last one

		// setup min/max filter
		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, doMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);	// trilinear filter
		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// setup wrap flags
		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenerateMipmapEXT(GL_TEXTURE_CUBE_MAP_ARB);
		GL_ResetError();	// previous function could fail for some reasons
	}

	return true;

	unguard;
}

#undef DBG

/*-----------------------------------------------------------------------------
	Unreal materials support
-----------------------------------------------------------------------------*/

const CShader &GL_UseGenericShader(GenericShaderType type)
{
	guard(GL_UseGenericShader);

	assert(type >= 0 && type < GS_Count);

	static CShader shaders[GS_Count];
	static const char* defines[GS_Count] =
	{
		NULL,							// GS_Textured
		"#define TEXTURING 0",			// GS_White
		"#define NORMALMAP 1"			// GS_NormalMap
	};

	CShader &Sh = shaders[type];
	if (!Sh.IsValid()) Sh.Make(Generic_ush, defines[type]);
	Sh.Use();
	Sh.SetUniform("useLighting", glIsEnabled(GL_LIGHTING));
	Sh.SetUniform("eyeLocation", viewOrigin);
	return Sh;

	unguardf("type=%d", type);
}


#if SHOW_SHADER_PARAMS
#define DBG(...)	if (GShowShaderParams) { DrawTextLeft(__VA_ARGS__); }
#else
#define DBG(...)
#endif

void GL_NormalmapShader(CShader &shader, CMaterialParams &Params)
{
	guard(GL_NormalmapShader);

	const char *subst[10];
	char  defines[512];
	defines[0] = 0;

	enum
	{
		I_Diffuse = 0,
		I_Normal,
		I_Specular,
		I_SpecularPower,
		I_Opacity,
		I_Emissive,
		I_Cube,
		I_Mask,
	};

	static const char *maskChannel[] =
	{
		"0.0",									// TC_NONE - bad, should not be used
		"texture2D(maskTex, TexCoord).r",		// TC_R
		"texture2D(maskTex, TexCoord).g",		// TC_G
		"texture2D(maskTex, TexCoord).b",		// TC_B
		"texture2D(maskTex, TexCoord).a",		// TC_A
		"(1.0-texture2D(maskTex, TexCoord).a)",	// TC_MA
	};

#define ADD_DEFINE(name)	appStrcatn(ARRAY_ARG(defines), "#define " name "\n")

	// diffuse
	glActiveTexture(GL_TEXTURE0);	// used for BindDefaultMaterial() too
	bool bHasDiffuse = false;
	if (Params.Diffuse && Params.Diffuse->Bind())
	{
		bHasDiffuse = true;
		DBG("Diffuse  : %s", Params.Diffuse->Name);
		ADD_DEFINE("DIFFUSE 1");
	}
	else if (!Params.Cube)
	{
		if (!DefaultUTexture) GetDefaultTexNum();
		DefaultUTexture->Bind();
		ADD_DEFINE("DIFFUSE 1");
	}

	// normal
	const char* normalUnpackExpr = "";
	if (Params.Normal)	//!! reimplement ! plus, check for correct normalmap texture (VTC texture compression etc ...)
	{
		DBG("Normal   : %s", Params.Normal->Name);
		glActiveTexture(GL_TEXTURE0 + I_Normal);
		if (!Params.Normal->Bind())
			Params.Normal = NULL;
		else
			normalUnpackExpr = Params.Normal->NormalUnpackExpr ? Params.Normal->NormalUnpackExpr : "";
	}

	// specular
	const char *specularExpr = "vec3(0.0)"; //?? vec3(specular)
	if (Params.Specular)
	{
		DBG("Specular : %s", Params.Specular->Name);
		glActiveTexture(GL_TEXTURE0 + I_Specular);
		if (Params.Specular->Bind())
			specularExpr = va("texture2D(specTex, TexCoord).%s * vec3(specular) * 1.5", !Params.SpecularFromAlpha ? "rgb" : "a");
	}
	else if (Params.SpecPower)
	{
		//todo: refactor at shader level, make a special function for PBR material, so no multiple texture2D will be used
		// No specular color, but has specular power - use diffuse or white color (e.g. happens with UE4)
		glActiveTexture(GL_TEXTURE0 + I_SpecularPower);
		if (bHasDiffuse && Params.PBRMaterial && Params.SpecPower->Bind())
		{
			// Ideally should analyze metalness: when metallic=1, reflect environment (white) color. When
			// metallic=0, reflect material's diffuse color.
			specularExpr = "(GetMaterialDiffuseColor(TexCoord).rgb * texture2D(spPowTex, TexCoord).r * 0.2) * gl_FrontMaterial.shininess";
		}
		else
		{
			specularExpr = "vec3(1.0)";
		}
	}
	// specular power
	const char *specPowerExpr = "gl_FrontMaterial.shininess";
	if (Params.SpecPower)
	{
		DBG("SpecPower: %s", Params.SpecPower->Name);
		glActiveTexture(GL_TEXTURE0 + I_SpecularPower);
		if (Params.SpecPower->Bind())
		{
			if (!Params.PBRMaterial)
				specPowerExpr = "texture2D(spPowTex, TexCoord).g * 100.0 + 5.0";
			else
				specPowerExpr = "(2.0 / pow(texture2D(spPowTex, TexCoord).r, 4.0) - 2.0) * gl_FrontMaterial.shininess / 10.0";
		}
	}

	// opacity mask
	const char *opacityExpr = "1.0";
	if (Params.Opacity)
	{
		DBG("Opacity  : %s", Params.Opacity->Name);
		glActiveTexture(GL_TEXTURE0 + I_Opacity);
		if (Params.Opacity->Bind())
			opacityExpr = va("texture2D(opacTex, TexCoord).%s", !Params.OpacityFromAlpha ? "r" : "a");
	}
	else if (Params.Diffuse)
	{
		DBG("Opacity from diffuse");
		opacityExpr = va("texture2D(diffTex, TexCoord).a");
	}

	// emission
	const char *emissExpr = "vec3(0.0)";
	if (Params.Emissive)
	{
		DBG("Emissive : %s", Params.Emissive->Name);
		glActiveTexture(GL_TEXTURE0 + I_Emissive);
		if (Params.Emissive->Bind())
		{
			ADD_DEFINE("EMISSIVE 1");
			emissExpr = va("vec3(%g,%g,%g) * texture2D(emisTex, TexCoord).g * 2.0",
				Params.EmissiveColor.R, Params.EmissiveColor.G, Params.EmissiveColor.B
			);
		}
	}

	// cubemap
	const char *cubeExpr = "vec3(0.0)";
	const char *cubeMaskExpr = "1.0";
	if (Params.Cube)
	{
		DBG("Cubemap  : %s", Params.Cube->Name);
		glActiveTexture(GL_TEXTURE0 + I_Cube);
		if (Params.Cube->Bind())
		{
			ADD_DEFINE("CUBE 1");
			if (Params.Emissive)
			{
				// use emissive as cubemap mask
				cubeMaskExpr = "texture2D(emisTex, TexCoord).g";
				emissExpr = "vec3(0.0)";
			}
			else
			{
				cubeExpr = "textureCube(cubeTex, TexCoord).rgb";
			}
		}
	}

	// mask
	if (Params.Mask)
	{
		DBG("Mask     : %s", Params.Mask->Name);
		glActiveTexture(GL_TEXTURE0 + I_Mask);
		if (Params.Mask->Bind())
		{
			// channels
			if (Params.EmissiveChannel)
			{
				emissExpr = va("vec3(%g,%g,%g) * %s * 2.0",
					Params.EmissiveColor.R, Params.EmissiveColor.G, Params.EmissiveColor.B,
					maskChannel[Params.EmissiveChannel]
				);
				ADD_DEFINE("EMISSIVE 1");
			}
			if (Params.SpecularMaskChannel)
				specularExpr  = va("vec3(%s)", maskChannel[Params.SpecularMaskChannel]);
			if (Params.SpecularPowerChannel)
				specPowerExpr = va("%s * 100.0 + 5.0", maskChannel[Params.SpecularPowerChannel]);
			if (Params.CubemapMaskChannel)
				cubeMaskExpr = maskChannel[Params.CubemapMaskChannel];
		}
	}

	//!! NOTE: Specular and SpecPower are scaled by const to improve visual; should be scaled by parameters from material
	subst[0] = Params.Normal ? "texture2D(normTex, TexCoord).rgb * 2.0 - 1.0" : "vec3(0.0, 0.0, 1.0)";
	subst[1] = specularExpr;
	subst[2] = specPowerExpr;
	subst[3] = opacityExpr;
	subst[4] = emissExpr;
	subst[5] = cubeExpr;
	subst[6] = cubeMaskExpr;
	subst[7] = normalUnpackExpr;
	// finalize paramerers and make shader
	subst[8] = NULL;

	if (Params.bUseMobileSpecular)
	{
		static const char* mobileSpecExpr[] = {				// EMobileSpecularMask values
			"vec3(1.0)",									// MSM_Constant
			"texture2D(diffTex, TexCoord).rgb * 2.0",		// MSM_Luminance
			"texture2D(diffTex, TexCoord).r * vec3(2.0)",	// MSM_DiffuseRed
			"texture2D(diffTex, TexCoord).g * vec3(2.0)",	// MSM_DiffuseGreen
			"texture2D(diffTex, TexCoord).b * vec3(2.0)",	// MSM_DiffuseBlue
			"texture2D(diffTex, TexCoord).a * vec3(2.0)",	// MSM_DiffuseAlpha
			"texture2D(opacTex, TexCoord).rgb * 2.0",		// MSM_MaskTextureRGB
			"texture2D(opacTex, TexCoord).r * vec3(2.0)",	// MSM_MaskTextureRed
			"texture2D(opacTex, TexCoord).g * vec3(2.0)",	// MSM_MaskTextureGreen
			"texture2D(opacTex, TexCoord).b * vec3(2.0)",	// MSM_MaskTextureBlue
			"texture2D(opacTex, TexCoord).a * vec3(2.0)",	// MSM_MaskTextureAlpha
		};
		// TODO: check presence of textures used in a shader (diffTex and opacTex)
		subst[1] = mobileSpecExpr[Params.MobileSpecularMask];
		subst[2] = va("%f * 1.0", Params.MobileSpecularPower);	//??
	}

	glActiveTexture(GL_TEXTURE0);

	//?? should check IsValid before preparing params above (they're needed only once)
	//?? (but this will not allow SHOW_SHADER_PARAMS to work)
	if (!shader.IsValid())
	{
		PROFILE_SHADER(appResetProfiler());
		shader.Make(Normal_ush, defines, subst);
		PROFILE_SHADER(appPrintf("Compiled shader\n"); appPrintProfiler());
	}
	shader.Use();

	shader.SetUniform("diffTex",  I_Diffuse);
	shader.SetUniform("normTex",  I_Normal);
	shader.SetUniform("specTex",  I_Specular);
	shader.SetUniform("spPowTex", I_SpecularPower);
	shader.SetUniform("opacTex",  I_Opacity);
	shader.SetUniform("emisTex",  I_Emissive);
	shader.SetUniform("cubeTex",  I_Cube);
	shader.SetUniform("maskTex",  I_Mask);

	unguard;
}


void UUnrealMaterial::Lock()
{
	LockCount++;

//	appPrintf("Lock(%d) %s\n", LockCount, Name);

	if (!IsTexture() && LockCount == 1)
	{
		// material
		TArray<UUnrealMaterial*> Textures;
		AppendReferencedTextures(Textures);
		for (int i = 0; i < Textures.Num(); i++)
			Textures[i]->Lock();
	}
}

void UUnrealMaterial::Unlock()
{
	assert(LockCount > 0);
//	appPrintf("Unlock(%d) %s\n", LockCount-1, Name);
	if (--LockCount > 0) return;


	if (IsTexture())
	{
		Release();
	}
	else
	{
		// material
		TArray<UUnrealMaterial*> Textures;
		AppendReferencedTextures(Textures);
		for (int i = 0; i < Textures.Num(); i++)
			Textures[i]->Unlock();
	}
}

void UUnrealMaterial::Release()
{
	guard(UUnrealMaterial::Release);
#if USE_GLSL
	GLShader.Release();
#endif
	DrawTimestamp = 0;
	unguard;
}


void UUnrealMaterial::SetMaterial()
{
	guard(UUnrealMaterial::SetMaterial);

	SetupGL();

	CMaterialParams Params;
	GetParams(Params);

	if (Params.IsNull())
	{
		BindDefaultMaterial();
		return;
	}

#if USE_GLSL
	if (GUseGLSL)
	{
		if (Params.Diffuse == this)
		{
			// simple texture
			if (Params.Diffuse->Bind())
				GL_UseGenericShader(GS_Textured);
			else
				BindDefaultMaterial();
		}
		else
		{
			DBG(S_BLUE"---- %s %s ----", GetClassName(), Name);
			GL_NormalmapShader(GLShader, Params);
		}
	}
	else
#endif // USE_GLSL
	{
		if (!Params.Diffuse || !Params.Diffuse->Bind())
			BindDefaultMaterial();
	}

	unguardf("%s", Name);
}


void UUnrealMaterial::AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered) const
{
	guard(UUnrealMaterial::AppendReferencedTextures);

	CMaterialParams Params;
	GetParams(Params);
	Params.AppendAllTextures(OutTextures);

	unguard;
}


void UUnrealMaterial::SetupGL()
{
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
}


void UMaterialWithPolyFlags::SetupGL()
{
	Super::SetupGL();
	if (Material) Material->SetupGL();

	// twosided material
	if (PolyFlags & PF_TwoSided)
		glDisable(GL_CULL_FACE);

	// handle blending
	if (PolyFlags & PF_Translucent)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	}
	else if (PolyFlags & PF_Modulated)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	}
	else if (PolyFlags & (PF_Masked/*|PF_TwoSided*/))
	{
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glAlphaFunc(GL_GREATER, 0.1f);
	}
}


static TArray<UMaterialWithPolyFlags*> WrappedMaterials;

UUnrealMaterial *UMaterialWithPolyFlags::Create(UUnrealMaterial *OriginalMaterial, unsigned PolyFlags)
{
	if (PolyFlags == 0) return OriginalMaterial;	// no wrapping required
	// find for the same material with the same flags wrapped before
	for (int i = 0; i < WrappedMaterials.Num(); i++)
	{
		UMaterialWithPolyFlags *WM = WrappedMaterials[i];
		if (WM->Material == OriginalMaterial && WM->PolyFlags == PolyFlags)
			return WM;
	}
	// material is not wrapped yet, so wrap it
	UMaterialWithPolyFlags *WM = new UMaterialWithPolyFlags;
	WM->Name      = (OriginalMaterial) ? OriginalMaterial->Name : "None";
	WM->Material  = OriginalMaterial;
	WM->PolyFlags = PolyFlags;
	WrappedMaterials.Add(WM);
//	appNotify("WRAP: %s %X", OriginalMaterial ? OriginalMaterial->Name : "NULL", PolyFlags);
	return WM;
}


static GLuint GetDefaultTexNum()
{
	if (!DefaultUTexture)
	{
		// allocate material
		DefaultUTexture = new UTexture;
		DefaultUTexture->bTwoSided = true;
		DefaultUTexture->Mips.AddDefaulted();
		DefaultUTexture->Format = TEXF_RGBA8;
		DefaultUTexture->Package = NULL;
		DefaultUTexture->Name = "Default";

#if 0
		// create 1st mipmap
#define TEX_SIZE	64
		FMipmap &Mip = DefaultUTexture->Mips[0];
		Mip.USize = Mip.VSize = TEX_SIZE;
		Mip.DataArray.AddUninitialized(TEX_SIZE*TEX_SIZE*4);
		byte *pic = &Mip.DataArray[0];
		for (int x = 0; x < TEX_SIZE; x++)
		{
			for (int y = 0; y < TEX_SIZE; y++)
			{
				static const byte colors[4][4] =
				{
//					{192,64,64,255}, {32,32,192,255}, {32,239,32,255}, {32,192,192,255}				// BGRA; most colorful
#define N 96		// saturation level; 0 = greyscale, 255-64-16 = max color (64 is max greyscale, 16 for nested checkerboards)
					{64+N,64,64,255}, {48,48,48+N,255}, {48,48+N,48,255}, {64,64+N/2,64+N/2,255}	// BGRA; colorized
#undef N
//					{64,64,64,255}, {48,48,48,255}, {48,48,48,255}, {64,64,64,255}					// BGRA; greyscale
				};
				byte *p = pic + y * TEX_SIZE * 4 + x * 4;
				int i1 = x < TEX_SIZE / 2;
				int i2 = y < TEX_SIZE / 2;
				const byte *c = colors[i1 * 2 + i2];	// top checkerboard level
				int corr = ((x ^ y) & 4) ? 4 : -4;		// nested checkerboard
//				corr += ((x ^ y) & 8) ? 12 : -12;		// one more nested checkerboard level
				p[0] = c[0] + corr;
				p[1] = c[1] + corr;
				p[2] = c[2] + corr;
				p[3] = c[3];
			}
		}
#undef TEX_SIZE
#else
		// create 1st mipmap
#define TEX_SIZE	512
		FMipmap &Mip = DefaultUTexture->Mips[0];
		Mip.USize = Mip.VSize = TEX_SIZE;
		Mip.DataArray.AddUninitialized(TEX_SIZE*TEX_SIZE*4);
		byte *pic = &Mip.DataArray[0];
		for (int x = 0; x < TEX_SIZE; x++)
		{
			for (int y = 0; y < TEX_SIZE; y++)
			{
				static const byte colors[12][3] =
				{
					// colors: bright, mid, dark
					// red:
					{ 254,100,66 }, { 203,80,53 }, { 128,50,33 },
					// green:
					{ 139,159,66 }, { 125,143,59 }, { 84,96,40 },
					// blue:
					{ 184,203,207 }, { 147,162,166 }, { 98,108,110 },
					// white:
					{ 249,244,229 }, { 224,219,206 }, { 132,129,121 }
				};
				byte *p = pic + y * TEX_SIZE * 4 + x * 4;
				const int partSize = TEX_SIZE/4;
				int x1 = x / partSize;
				int y1 = y / partSize;
				int x2 = x % partSize;
				int y2 = y % partSize;
				int x3 = x2 / (partSize/2);
				int y3 = y2 / (partSize/2);

				// Dark grid
				int colorIndex;
				if (x2 == 0 || x2 == partSize-1 || y2 == 0 || y2 == partSize-1)
				{
					colorIndex = -1; // black
				}
				else if (x % (partSize/4) == 0 || y % (partSize/4) == 0)
				{
					colorIndex = 2; // middle grid, dark color
				}
				else
				{
					colorIndex = (x3 + y3) & 1;
				}

				byte r, b, g;
				if (colorIndex < 0)
				{
					// Black border
					r = g = b = 0;
				}
				else
				{
					colorIndex = ((x1 + y1) & 3) * 3 + colorIndex;
					r = colors[colorIndex][0];
					g = colors[colorIndex][1];
					b = colors[colorIndex][2];
				}

				p[0] = b;
				p[1] = g;
				p[2] = r;
				p[3] = 255;
			}
		}
#undef TEX_SIZE
#endif
	}
	DefaultUTexture->Upload();
	return DefaultUTexture->TexNum;
}

void BindDefaultMaterial(bool White)
{
	guard(BindDefaultMaterial);

	glDepthMask(GL_TRUE);		//?? place into other places too

	if (GL_SUPPORT(QGL_1_3))
	{
		// disable all texture units except 0
		int numTexUnits = 1;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &numTexUnits);
		for (int i = 1; i < numTexUnits; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		}
		glActiveTexture(GL_TEXTURE0);
	}

#if USE_GLSL
	if (GUseGLSL) GL_UseGenericShader(White ? GS_White : GS_Textured);
#endif

	if (White)
	{
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		return;
	}

	// bind texture
	if (!DefaultUTexture) GetDefaultTexNum(); // create default texture
	DefaultUTexture->Bind();

	unguard;
}


void UTexture::SetupGL()
{
	guard(UTexture::SetupGL);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	glEnable(GL_DEPTH_TEST);
	// bTwoSided
	if (bTwoSided || IsTextureCube())
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	// alpha-test
	if (bMasked || (bAlphaTexture && Format == TEXF_DXT1))	// masked or 1-bit alpha
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.8f);
	}
	else if (bAlphaTexture)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.1f);
	}
	// blending
	// partially taken from UT/OpenGLDrv
	if (bAlphaTexture || bMasked)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}

	unguard;
}

bool UTexture::Upload()
{
	if (TexNum == BAD_TEXTURE) return false;
	if (!GL_TouchObject(DrawTimestamp))
		TexNum = Upload2D(this, true, UClampMode == TC_Clamp, VClampMode == TC_Clamp);
	return (TexNum != BAD_TEXTURE);
}

bool UTexture::Bind()
{
	guard(UTexture::Bind);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	if (!Upload())
	{
		glDisable(GL_TEXTURE_2D);
		return false;
	}

	// bind texture
	glBindTexture(GL_TEXTURE_2D, TexNum);
	return true;

	unguard;
}


void UTexture::GetParams(CMaterialParams &Params) const
{
	Params.Diffuse = (UUnrealMaterial*)this;
}


bool UTexture::IsTranslucent() const
{
	return bAlphaTexture || bMasked;
}


void UTexture::Release()
{
	guard(UTexture::Release);
	if (GL_IsValidObject(TexNum, DrawTimestamp))
		glDeleteTextures(1, &TexNum);
	Super::Release();
	unguard;
}


bool UCubemap::Upload()
{
	for (int side = 0; side < 6; side++)
	{
		if (Faces[side] == NULL)
			return false; // one of faces is missing
	}

	if (TexNum == BAD_TEXTURE) return false;
	if (GL_TouchObject(DrawTimestamp)) return true;

	// upload all cube sides
	glGenTextures(1, &TexNum);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, TexNum);
	for (int side = 0; side < 6; side++)
	{
		UTexture *Tex = Faces[side];
		bool success = UploadCubeSide(Tex, side);
		// release bulk data immediately
		//!! better - use Lock (when uploaded)/Unlock (when destroyed/released) for referenced textures
		Tex->ReleaseTextureData();

		if (!success)
		{
			glDeleteTextures(1, &TexNum);
			TexNum = BAD_TEXTURE;
			break;
		}
	}
	return (TexNum != BAD_TEXTURE);
}

bool UCubemap::Bind()
{
	guard(UCubemap::Bind);

	bool isBad = false;
	for (int side = 0; side < 6; side++)
	{
		if (Faces[side] == NULL)
		{
			isBad = true;
			break;
		}
	}

	if (!GUseGLSL || isBad)
	{
		BindDefaultMaterial();
		return false;
	}

	glEnable(GL_TEXTURE_CUBE_MAP_ARB);

	if (!Upload())
	{
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		return false;
	}

	// bind texture
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, TexNum);
	return true;

	unguard;
}


void UCubemap::GetParams(CMaterialParams &Params) const
{
	Params.Cube = (UUnrealMaterial*)this;
}


void UCubemap::Release()
{
	guard(UCubemap::Release);
	if (GL_IsValidObject(TexNum, DrawTimestamp))
		glDeleteTextures(1, &TexNum);
	Super::Release();
	unguard;
}


void UModifier::SetupGL()
{
	guard(UModifier::SetupGL);
	if (Material) Material->SetupGL();
	unguard;
}

void UModifier::GetParams(CMaterialParams &Params) const
{
	guard(UModifier::GetParams);
	if (Material)
		Material->GetParams(Params);
	unguard;
}


void UFinalBlend::SetupGL()
{
	guard(UFinalBlend::SetupGL);

	glEnable(GL_DEPTH_TEST);
	// override material settings
	// TwoSided
	if (TwoSided)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	// AlphaTest
	if (AlphaTest)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, AlphaRef / 255.0f);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}
	// FrameBufferBlending
	if (FrameBufferBlending == FB_Overwrite)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);
	switch (FrameBufferBlending)
	{
//	case FB_Overwrite:
//		glBlendFunc(GL_ONE, GL_ZERO);				// src
//		break;
	case FB_Modulate:
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);	// src*dst*2
		break;
	case FB_AlphaBlend:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case FB_AlphaModulate_MightNotFogCorrectly:
		//!!
		break;
	case FB_Translucent:
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		break;
	case FB_Darken:
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR); // dst - src
		break;
	case FB_Brighten:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);			// src*srcA + dst
		break;
	case FB_Invisible:
		glBlendFunc(GL_ZERO, GL_ONE);				// dst
		break;
	}
	//!! ZWrite, ZTest

	unguard;
}


bool UFinalBlend::IsTranslucent() const
{
	return (FrameBufferBlending != FB_Overwrite) || AlphaTest;
}


void UShader::SetupGL()
{
	guard(UShader::SetupGL);

	glEnable(GL_DEPTH_TEST);
	// TwoSided
	if (TwoSided)
		glDisable(GL_CULL_FACE);
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	//?? glEnable(GL_ALPHA_TEST) only when Opacity is set ?
	glDisable(GL_ALPHA_TEST);

	// blending
	if (OutputBlending == OB_Normal && !Opacity)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);
	switch (OutputBlending)
	{
	case OB_Normal:
		if (Opacity) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case OB_Masked:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glAlphaFunc(GL_GREATER, 0.0f);
		glEnable(GL_ALPHA_TEST);
		break;
	case OB_Modulate:
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);	// src*dst*2
		break;
	case OB_Translucent:
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		break;
	case OB_Invisible:
		glBlendFunc(GL_ZERO, GL_ONE);				// dst
		break;
	case OB_Brighten:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);			// src*srcA + dst
		break;
	case OB_Darken:
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR); // dst - src
		break;
	}

	unguard;
}


void UShader::GetParams(CMaterialParams &Params) const
{
	guard(UShader::GetParams);

	if (Diffuse)
	{
		Diffuse->GetParams(Params);
	}
#if BIOSHOCK
	if (NormalMap)
	{
		if ((UShader*)this == NormalMap) return;	// recurse; Bioshock has such data ...
		CMaterialParams Params2;
		NormalMap->GetParams(Params2);
		Params.Normal = Params2.Diffuse;
	}
#endif
	if (SpecularityMask)
	{
		CMaterialParams Params2;
		SpecularityMask->GetParams(Params2);
		Params.Specular          = Params2.Diffuse;
		Params.SpecularFromAlpha = true;
	}
	if (Opacity)
	{
		CMaterialParams Params2;
		Opacity->GetParams(Params2);
		Params.Opacity          = Params2.Diffuse;
		Params.OpacityFromAlpha = true;
	}

	unguardf("%s", Name);
}


bool UShader::IsTranslucent() const
{
	return (OutputBlending != OB_Normal);
}



#if BIOSHOCK

//?? NOTE: Bioshock EFrameBufferBlending is used for UShader and UFinalBlend, plus it has different values
// based on UShader and UFinalBlend Bind()
void UFacingShader::SetupGL()
{
	guard(UFacingShader::SetupGL);

	glEnable(GL_DEPTH_TEST);
	// TwoSided
	if (TwoSided)
		glDisable(GL_CULL_FACE);
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	// part of UFinalBlend::SetupGL()
	switch (OutputBlending)
	{
//	case FB_Overwrite:
//		glBlendFunc(GL_ONE, GL_ZERO);				// src
//		break;
	case FB_Modulate:
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);	// src*dst*2
		break;
	case FB_AlphaBlend:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case FB_AlphaModulate_MightNotFogCorrectly:
		//!!
		break;
	case FB_Translucent:
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		break;
	case FB_Darken:
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR); // dst - src
		break;
	case FB_Brighten:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);			// src*srcA + dst
		break;
	case FB_Invisible:
		glBlendFunc(GL_ZERO, GL_ONE);				// dst
		break;
	}

	unguard;
}


void UFacingShader::GetParams(CMaterialParams &Params) const
{
	guard(UFacingShader::GetParams);

	if (FacingDiffuse)
	{
		CMaterialParams Params2;
		FacingDiffuse->GetParams(Params2);
		Params.Diffuse = Params2.Diffuse;
	}
	if (NormalMap)
	{
		CMaterialParams Params2;
		NormalMap->GetParams(Params2);
		Params.Normal = Params2.Diffuse;
	}
	if (FacingSpecularColorMap)
	{
		CMaterialParams Params2;
		FacingSpecularColorMap->GetParams(Params2);
		Params.Specular = Params2.Diffuse;
	}
	if (FacingEmissive)
	{
		CMaterialParams Params2;
		FacingEmissive->GetParams(Params2);
		Params.Emissive = Params2.Diffuse;
	}

	unguard;
}


bool UFacingShader::IsTranslucent() const
{
	return (OutputBlending != FB_Overwrite);
}

#endif // BIOSHOCK


#if SPLINTER_CELL

void UUnreal3Material::SetupGL()
{
	guard(UUnreal3Material::SetupGL);

	glEnable(GL_DEPTH_TEST);
	// TwoSided
	if (bDoubleSided)
		glDisable(GL_CULL_FACE);
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	// part of UFinalBlend::SetupGL()
	if (BlendingMode == U3BM_OPAQUE)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);
	switch (BlendingMode)
	{
//	case U3BM_OPAQUE:
//		glBlendFunc(GL_ONE, GL_ZERO);				// src
//		break;
	case U3BM_TRANSLUCENT:
	case U3BM_TRANSLUCENT_NO_DISTORTION:
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		break;
	case U3BM_ADDITIVE:
		glBlendFunc(GL_ONE, GL_ONE);				// src + dst
		break;
	case U3BM_MASKED:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	// src*srcA + dst*(1-srcA)
		break;
	}

	if (BlendingMode == U3BM_MASKED)
	{
		glAlphaFunc(GL_GREATER, 0.0f);
		glEnable(GL_ALPHA_TEST);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}

	unguard;
}


void UUnreal3Material::GetParams(CMaterialParams &Params) const
{
	guard(UUnreal3Material::GetParams);

	for (int i = 0; i < ARRAY_COUNT(Textures); i++)
	{
		UTexture *Tex = Textures[i];
		if (!Tex) continue;
		const char *Name = Tex->Name;
		int len = strlen(Name);
		if (!stricmp(Name + len - 2, "_d"))
			Params.Diffuse = Tex;
		else if (!stricmp(Name + len - 2, "_n"))
			Params.Normal = Tex;
		else if (!stricmp(Name + len - 2, "_m"))
			Params.Mask = Tex;
//		else
//			appPrintf("Tex: %s\n", Name);
	}
	Params.SpecularMaskChannel = TC_G;

	unguard;
}


bool UUnreal3Material::IsTranslucent() const
{
	return (BlendingMode != U3BM_OPAQUE);
}


void USCX_basic_material::SetupGL()
{
	guard(USCX_basic_material::SetupGL);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	unguard;
}


void USCX_basic_material::GetParams(CMaterialParams &Params) const
{
	guard(USCX_basic_material::GetParams);

	Params.Diffuse = Base;
	Params.Normal  = Normal;
	Params.Mask    = SpecularMask;		// Params.Specular, but single channel
	Params.Cube    = Environment;

	switch (SpecularSource)
	{
	case SpecSrc_SRed:
		Params.SpecularMaskChannel = TC_R;
		break;
	case SpecSrc_SGreen:
		Params.SpecularMaskChannel = TC_G;
		break;
	case SpecSrc_SBlue:
		Params.SpecularMaskChannel = TC_B;
		break;
	case SpecSrc_NAlpha:
		Params.SpecularMaskChannel = TC_MA;	//?? TC_A ?
		break;
	}

	unguard;
}


bool USCX_basic_material::IsTranslucent() const
{
	return false;
}


#endif // SPLINTER_CELL


void UCombiner::GetParams(CMaterialParams &Params) const
{
	guard(UCombiner::GetParams);

	CMaterialParams Params2;

	switch (CombineOperation)
	{
	case CO_Use_Color_From_Material1:
		if (Material1) Material1->GetParams(Params2);
		break;
	case CO_Use_Color_From_Material2:
		if (Material2) Material2->GetParams(Params2);
		break;
	case CO_Multiply:
	case CO_Add:
	case CO_Subtract:
	case CO_AlphaBlend_With_Mask:
	case CO_Add_With_Mask_Modulation:
		if (Material1 && Material2)
		{
			if (Material2->IsA("TexEnvMap"))
			{
				// special case: Material1 is a UTexEnvMap
				Material1->GetParams(Params2);
				Params.Specular = Params2.Diffuse;
				Params.SpecularFromAlpha = true;
			}
			else if (Material1->IsA("TexEnvMap"))
			{
				// special case: Material1 is a UTexEnvMap
				Material2->GetParams(Params2);
				Params.Specular = Params2.Diffuse;
				Params.SpecularFromAlpha = true;
			}
			else
			{
				// no specular; heuristic: usually Material2 contains more significant texture
				Material2->GetParams(Params2);
				if (!Params2.Diffuse) Material1->GetParams(Params2);	// fallback
			}
		}
		else if (Material1)
			Material1->GetParams(Params2);
		else if (Material2)
			Material2->GetParams(Params2);
		break;
	case CO_Use_Color_From_Mask:
		if (Mask) Mask->GetParams(Params2);
		break;
	}
	Params.Diffuse = Params2.Diffuse;

	// cannot implement masking right now: UE3 uses color, but UE2 - alpha
/*	switch (AlphaOperation)
	{
	case AO_Use_Mask:
	case AO_Multiply:
	case AO_Add:
	case AO_Use_Alpha_From_Material1:
	case AO_Use_Alpha_From_Material2:
	} */

	unguard;
}


#if UNREAL3


void UMaterialInterface::GetParams(CMaterialParams &Params) const
{
#if SUPPORT_IPHONE
	//?? these parameters are common for UMaterial3 and UMaterialInstanceConstant (UMaterialInterface)
	if (FlattenedTexture)		Params.Diffuse = FlattenedTexture;
	if (MobileBaseTexture)		Params.Diffuse = MobileBaseTexture;
	if (MobileNormalTexture)	Params.Normal  = MobileNormalTexture;
	if (MobileMaskTexture)		Params.Opacity = MobileMaskTexture;
	Params.bUseMobileSpecular  = bUseMobileSpecular;
	Params.MobileSpecularPower = MobileSpecularPower;
	Params.MobileSpecularMask  = MobileSpecularMask;
#endif // SUPPORT_IPHONE
}

static void SetupUE3BlendMode(EBlendMode BlendMode)
{
	glDepthMask(BlendMode == BLEND_Translucent ? GL_FALSE : GL_TRUE); // may be, BLEND_Masked too

	if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
		glDisable(GL_BLEND);
	else
	{
		glEnable(GL_BLEND);
		switch (BlendMode)
		{
//		case BLEND_Masked:
//			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	//?? should use opacity channel; has lighting
//			break;
		case BLEND_Translucent:
//			glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);	//?? should use opacity channel; no lighting
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	//?? should use opacity channel; no lighting
			break;
		case BLEND_Additive:
			glBlendFunc(GL_ONE, GL_ONE);
			break;
		case BLEND_Modulate:
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
			break;
		default:
			glDisable(GL_BLEND);
			DrawTextLeft("Unknown BlendMode %d", BlendMode);
		}
	}
}


//!! NOTE: when using this function sharing of shader between MaterialInstanceConstant's is impossible
//!! (shader may differs because of different texture sets - some available, some - not)

void UMaterial3::SetupGL()
{
	guard(UMaterial3::SetupGL);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);

	// TwoSided
	if (TwoSided)
		glDisable(GL_CULL_FACE);
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	// depth test
	if (bDisableDepthTest)
		glDisable(GL_DEPTH_TEST);
	else
		glEnable(GL_DEPTH_TEST);
	// alpha mask
	if (bIsMasked || BlendMode == BLEND_Masked)
	{
		glEnable(GL_BLEND);
//		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glAlphaFunc(GL_GREATER, OpacityMaskClipValue);
		glEnable(GL_ALPHA_TEST);
	}

	SetupUE3BlendMode(BlendMode);

	unguard;
}


bool UMaterial3::IsTranslucent() const
{
	return BlendMode != BLEND_Opaque;
}


void UMaterial3::GetParams(CMaterialParams &Params) const
{
	guard(UMaterial3::GetParams);

	Super::GetParams(Params);

	int DiffWeight = 0, NormWeight = 0, SpecWeight = 0, SpecPowWeight = 0, OpWeight = 0, EmWeight = 0, CubeWeight = 0;
#define DIFFUSE(check,weight)			\
	if (weight > DiffWeight && check)	\
	{									\
	/*	DrawTextLeft("D: %d > %d = %s", weight, DiffWeight, Tex->Name); */ \
		Params.Diffuse = Tex;			\
		DiffWeight = weight;			\
	}
#define NORMAL(check,weight)			\
	if (weight > NormWeight && check)	\
	{									\
	/*	DrawTextLeft("N: %d > %d = %s", weight, NormWeight, Tex->Name); */ \
		Params.Normal = Tex;			\
		NormWeight = weight;			\
	}
#define SPECULAR(check,weight)			\
	if (weight > SpecWeight && check)	\
	{									\
	/*	DrawTextLeft("S: %d > %d = %s", weight, SpecWeight, Tex->Name); */ \
		Params.Specular = Tex;			\
		SpecWeight = weight;			\
	}
#define SPECPOW(check,weight)			\
	if (weight > SpecPowWeight && check)\
	{									\
	/*	DrawTextLeft("SP: %d > %d = %s", weight, SpecPowWeight, Tex->Name); */ \
		Params.SpecPower = Tex;			\
		SpecPowWeight = weight;			\
	}
#define OPACITY(check,weight)			\
	if (weight > OpWeight && check)		\
	{									\
	/*	DrawTextLeft("O: %d > %d = %s", weight, OpWeight, Tex->Name); */ \
		Params.Opacity = Tex;			\
		OpWeight = weight;				\
	}
#define EMISSIVE(check,weight)			\
	if (weight > EmWeight && check)		\
	{									\
	/*	DrawTextLeft("E: %d > %d = %s", weight, EmWeight, Tex->Name); */ \
		Params.Emissive = Tex;			\
		EmWeight = weight;				\
	}
#define CUBEMAP(check,weight)			\
	if (weight > CubeWeight && check)	\
	{									\
	/*	DrawTextLeft("CUB: %d > %d = %s", weight, CubeWeight, Tex->Name); */ \
		Params.Cube = Tex;				\
		CubeWeight = weight;			\
	}
#define BAKEDMASK(check,weight)			\
	if (weight > MaskWeight && check)	\
	{									\
	/*	DrawTextLeft("MASK: %d > %d = %s", weight, MaskWeight, Tex->Name); */ \
		Params.Mask = Tex;				\
		MaskWeight = weight;			\
	}
#define EMISSIVE_COLOR(check,weight)	\
	if (weight > EmcWeight && check)	\
	{									\
	/*	DrawTextLeft("EC: %d > %d = %g %g %g", weight, EmcWeight, FCOLOR_ARG(Color)); */ \
		Params.EmissiveColor = Color;	\
		EmcWeight = weight;				\
	}

	int ArGame = GetGame();

	for (int i = 0; i < ReferencedTextures.Num(); i++)
	{
		UTexture3 *Tex = ReferencedTextures[i];
		if (!Tex) continue;
		// Prepare the texture name in lower case, so parameter checks will be faster
		// (no "stricmp" or "appStristr" calls). Warning: all strcmp operations should use
		// a lowercase string for comparison.
		char Name[256];
		appStrncpylwr(Name, Tex->Name, ARRAY_COUNT(Name));
		int len = strlen(Name);
		//!! - separate code (common for UMaterial3 + UMaterialInstanceConstant)
		//!! - may implement with tables + macros
		//!! - catch normalmap, specular and emissive textures
		if (strstr(Name, "noise")) continue;
		if (strstr(Name, "detail")) continue;

		DIFFUSE(strstr(Name, "diff"), 100);
		NORMAL (strstr(Name, "norm"), 100);
		DIFFUSE(!strcmp(Name + len - 4, "_tex"), 80);
		DIFFUSE(strstr(Name, "_tex"), 60);
		DIFFUSE(!strcmp(Name + len - 2, "_d"), 20);
		OPACITY(strstr(Name, "_om"), 20);
//		CUBEMAP(strstr(Name, "cubemap"), 100); -- bad
#if 0
		if (!strcmp(Name + len - 3, "_di"))		// The Last Remnant ...
			Diffuse = Tex;
		if (!strnicmp(Name + len - 4, "_di", 3))	// The Last Remnant ...
			Diffuse = Tex;
		if (strstr(Name, "_diffuse"))
			Diffuse = Tex;
#endif
		DIFFUSE (strstr(Name, "_di"), 20);
//		DIFFUSE (strstr(Name, "_ma"), 8 );		// The Last Remnant; low priority
		DIFFUSE (strstr(Name, "_d" ), 11);
		DIFFUSE (strstr(Name, "albedo"), 19);
		DIFFUSE (!strcmp(Name + len - 2, "_c"), 10);
		DIFFUSE (!strcmp(Name + len - 3, "_cm"), 12);
		NORMAL  (!strcmp(Name + len - 2, "_n"), 20);
		NORMAL  (!strcmp(Name + len - 3, "_nm"), 20);
		NORMAL  (strstr(Name, "_n"), 9);
#if BULLETSTORM
		if (ArGame == GAME_Bulletstorm)
		{
			DIFFUSE (strstr(Name, "_c"), 12);
			NORMAL(strstr(Name, "_ts"), 5);
			SPECULAR(strstr(Name, "_s"), 5);
		}
#endif // BULLETSTORM
		SPECULAR(!strcmp(Name + len - 2, "_s"), 20);
		SPECULAR(strstr(Name, "_s_"), 15);
		SPECPOW (!strcmp(Name + len - 3, "_sp"), 20);
		SPECPOW (!strcmp(Name + len - 3, "_sm"), 20);
		SPECPOW (strstr(Name, "_sp"), 9);
		EMISSIVE(!strcmp(Name + len - 2, "_e"), 20);
		EMISSIVE(!strcmp(Name + len - 3, "_em"), 21);
		OPACITY (!strcmp(Name + len - 2, "_a"), 20);
		if (bIsMasked)
		{
			OPACITY (!strcmp(Name + len - 5, "_mask"), 2);
		}
		// Magna Catra 2
		DIFFUSE (!strncmp(Name, "df_", 3), 20);
		SPECULAR(!strncmp(Name, "sp_", 3), 20);
//		OPACITY (!strncmp(Name, "op_", 3), 20);
		NORMAL  (!strncmp(Name, "no_", 3), 20);

		NORMAL  (strstr(Name, "norm"), 80);
		EMISSIVE(strstr(Name, "emis"), 80);
		SPECULAR(strstr(Name, "specular"), 80);
		OPACITY (strstr(Name, "opac"),  80);
		OPACITY (strstr(Name, "alpha"), 100);

		DIFFUSE(i == 0, 1);							// 1st texture as lowest weight
//		CUBEMAP(Tex->IsTextureCube(), 1);			// any cubemap
	}
	// do not allow normal map became a diffuse
	if ( (Params.Diffuse == Params.Normal && DiffWeight < NormWeight) ||
		 (Params.Diffuse && Params.Diffuse->IsTextureCube()) )
		Params.Diffuse = NULL;

#if UNREAL4
	if (ArGame >= GAME_UE4_BASE)
	{
		Params.PBRMaterial = true;
		if (Params.Specular)
		{
			// PRB material, no specular color
			Params.SpecPower = Params.Specular;
			Params.Specular = NULL;
		}
	}
#endif // UNREA:4

	unguard;
}

void UMaterial3::AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered) const
{
	guard(UMaterial3::AppendReferencedTextures);
	if (onlyRendered)
	{
		// default implementation does that
		Super::AppendReferencedTextures(OutTextures, onlyRendered);
	}
	else
	{
		for (int i = 0; i < ReferencedTextures.Num(); i++)
		{
			if (ReferencedTextures[i])
				OutTextures.AddUnique(ReferencedTextures[i]);
		}
	}
	unguard;
}

void UTexture2D::SetupGL()
{
	guard(UTexture2D::SetupGL);

	glEnable(GL_TEXTURE_2D);

	if (!IsTextureCube())
	{
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		glEnable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
		glDisable(GL_CULL_FACE);
	}

	glEnable(GL_DEPTH_TEST);

	unguard;
}

bool UTexture2D::Upload()
{
	if (TexNum == BAD_TEXTURE) return false;
	if (!GL_TouchObject(DrawTimestamp))
        TexNum = Upload2D(this, Mips.Num() == 0 || Mips.Num() > 1, AddressX == TA_Clamp, AddressY == TA_Clamp);
	return (TexNum != BAD_TEXTURE);
}

bool UTexture2D::Bind()
{
	guard(UTexture2D::Bind);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	if (!Upload())
	{
		glDisable(GL_TEXTURE_2D);
		return false;
	}

	// bind texture
	glBindTexture(GL_TEXTURE_2D, TexNum);
	return true;

	unguard;
}


void UTexture2D::GetParams(CMaterialParams &Params) const
{
	Params.Diffuse = (UUnrealMaterial*)this;
}


void UTexture2D::Release()
{
	guard(UTexture2D::Release);
	if (GL_IsValidObject(TexNum, DrawTimestamp))
		glDeleteTextures(1, &TexNum);
	ReleaseTextureData();
	Super::Release();
	unguard;
}

bool UTextureCube3::Upload()
{
	if (!FacePosX || !FacePosY || !FacePosZ || !FaceNegX || !FaceNegY || !FaceNegZ)
		return false; // one of faces is missing

	if (TexNum == BAD_TEXTURE) return false;
	if (GL_TouchObject(DrawTimestamp)) return true;

	// upload all cube sides
	glGenTextures(1, &TexNum);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, TexNum);
	for (int side = 0; side < 6; side++)
	{
		//?? can validate USize/VSize to be identical for all sides, plus - the same for "doMipmap" value
		UTexture2D *Tex = NULL;
		switch (side)
		{
		case 0:
			Tex = FacePosX;
			break;
		case 1:
			Tex = FaceNegX;
			break;
		case 2:
			Tex = FacePosY;
			break;
		case 3:
			Tex = FaceNegY;
			break;
		case 4:
			Tex = FacePosZ;
			break;
		case 5:
			Tex = FaceNegZ;
			break;
		}

		bool success = UploadCubeSide(Tex, side);
		// release bulk data immediately
		//!! better - use Lock (when uploaded)/Unlock (when destroyed/released) for referenced textures
		Tex->ReleaseTextureData();

		if (!success)
		{
			glDeleteTextures(1, &TexNum);
			TexNum = BAD_TEXTURE;
			break;
		}
	}
	return (TexNum != BAD_TEXTURE);
}

bool UTextureCube3::Bind()
{
	guard(UTextureCube3::Bind);

	if (!GUseGLSL || !FacePosX || !FacePosY || !FacePosZ || !FaceNegX || !FaceNegY || !FaceNegZ)
	{
		BindDefaultMaterial();
		return false;
	}

	glEnable(GL_TEXTURE_CUBE_MAP_ARB);

	if (!Upload())
	{
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		return false;
	}

	// bind texture
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, TexNum);
	return true;

	unguard;
}


void UTextureCube3::GetParams(CMaterialParams &Params) const
{
	Params.Cube = (UUnrealMaterial*)this;
}


void UTextureCube3::Release()
{
	guard(UTextureCube3::Release);
	if (GL_IsValidObject(TexNum, DrawTimestamp))
		glDeleteTextures(1, &TexNum);
	Super::Release();
	unguard;
}


#if UNREAL4


bool UTextureCube4::Upload()
{
	if (NumSlices != 6)
		return Super::Upload(); // use as Texture2D

	if (TexNum == BAD_TEXTURE) return false;
	if (GL_TouchObject(DrawTimestamp)) return true;

	// upload all cube sides
	glGenTextures(1, &TexNum);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, TexNum);
	for (int side = 0; side < 6; side++)
	{
		bool success = UploadCubeSide(this, side, true);
		// release bulk data immediately

		if (!success)
		{
			glDeleteTextures(1, &TexNum);
			TexNum = BAD_TEXTURE;
			break;
		}
	}

	ReleaseTextureData();

	return (TexNum != BAD_TEXTURE);
}

bool UTextureCube4::Bind()
{
	guard(UTextureCube4::Bind);

	if (NumSlices != 6) // probably source texture
		return Super::Bind();

	glEnable(GL_TEXTURE_CUBE_MAP_ARB);

	if (!Upload())
	{
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		return false;
	}

	// bind texture
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, TexNum);
	return true;

	unguard;
}


void UTextureCube4::GetParams(CMaterialParams &Params) const
{
	if (NumSlices != 6) return; // can't show this as cubemap
	Params.Cube = (UUnrealMaterial*)this;
}


void UTextureCube4::Release()
{
	guard(UTextureCube4::Release);
	if (GL_IsValidObject(TexNum, DrawTimestamp))
		glDeleteTextures(1, &TexNum);
	Super::Release();
	unguard;
}

#endif // UNREAL4

void UMaterialInstanceConstant::SetupGL()
{
	// redirect to Parent until UMaterial3
	if (Parent && Parent != this) Parent->SetupGL();

#if UNREAL4
	if (BasePropertyOverrides.bOverride_TwoSided)
	{
		if (BasePropertyOverrides.TwoSided)
			glDisable(GL_CULL_FACE);
		else
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
	}
	if (BasePropertyOverrides.bOverride_BlendMode)
	{
		SetupUE3BlendMode(BasePropertyOverrides.BlendMode);
	}
#endif // UNREAL4
}


void UMaterialInstanceConstant::GetParams(CMaterialParams &Params) const
{
	guard(UMaterialInstanceConstant::GetParams);

	// get params from linked UMaterial3
	if (Parent && Parent != this) Parent->GetParams(Params);

	Super::GetParams(Params);
	CMaterialParams ParemtParams = Params;

	// get local parameters
	int DiffWeight = 0, NormWeight = 0, SpecWeight = 0, SpecPowWeight = 0, OpWeight = 0, EmWeight = 0, EmcWeight = 0, CubeWeight = 0, MaskWeight = 0;

	if (TextureParameterValues.Num())
		Params.Opacity = NULL;			// it's better to disable opacity mask from parent material

	int ArGame = GetGame();

	int i;
	for (i = 0; i < TextureParameterValues.Num(); i++)
	{
		const FTextureParameterValue &P = TextureParameterValues[i];
		// Prepare the texture name in lower case, so parameter checks will be faster
		char Name[256];
		appStrncpylwr(Name, P.GetName(), ARRAY_COUNT(Name));
		UTexture3  *Tex  = P.ParameterValue;
		if (!Tex) continue;

		if (strstr(Name, "detail")) continue;	// details normal etc
		if (strstr(Name, "gradient")) continue; // emissive gradient etc

		DIFFUSE (strstr(Name, "dif"), 100);
		DIFFUSE (strstr(Name, "albedo"), 100);
		DIFFUSE (strstr(Name, "color"), 80);
		NORMAL  (strstr(Name, "norm") && !strstr(Name, "fx"), 100);
		SPECPOW (strstr(Name, "specpow"), 100);
		SPECULAR(strstr(Name, "spec"), 100);
		EMISSIVE(strstr(Name, "emiss"), 100);
		CUBEMAP (strstr(Name, "cube"), 100);
		CUBEMAP (strstr(Name, "refl"), 90);
		OPACITY (strstr(Name, "opac"), 90);
		OPACITY (strstr(Name, "trans") && !strstr(Name, "transm"), 80);
		OPACITY (strstr(Name, "opacity"), 100);
		OPACITY (strstr(Name, "alpha"), 100);
//??		OPACITY (strstr(Name, "mask"), 100);
//??		Params.OpacityFromAlpha = true;
#if TRON
		if (ArGame == GAME_Tron)
		{
			SPECPOW(strstr(Name, "sppw"), 100);
			EMISSIVE(strstr(Name, "emss"), 100);
			BAKEDMASK(strstr(Name, "mask"), 100);
		}
#endif // TRON
#if BATMAN
		if (ArGame == GAME_Batman2)
		{
			BAKEDMASK(!strcmp(Name, "material_attributes"), 100);
			EMISSIVE (strstr(Name, "reflection_mask"), 100);
		}
#endif // BATMAN
#if BLADENSOUL
		if (ArGame == GAME_BladeNSoul)
		{
			BAKEDMASK(!strcmp(Name, "body_mask_rgb"), 100);
		}
#endif // BLADENSOUL
#if DISHONORED
		if (ArGame == GAME_Dishonored)
		{
			CUBEMAP (strstr(Name, "cubemap_tex"), 100);
			EMISSIVE(strstr(Name, "cubemap_mask"), 100);
		}
#endif // DISHONORED
#if UNREAL4
		if (ArGame >= GAME_UE4_BASE)
		{
			// PRB material, no specular color
			Params.PBRMaterial = true;
			if (Params.Specular)
			{
				Params.SpecPower = Params.Specular;
				Params.Specular = NULL;
			}
			if (ParemtParams.Emissive == Params.Emissive &&
				ParemtParams.Diffuse != Params.Diffuse)
			{
				// Reset emissive if it came from parent, and diffuse has been changed locally
				Params.Emissive = NULL;
			}
		}
#endif // UNREA:4
	}
	for (i = 0; i < VectorParameterValues.Num(); i++)
	{
		const FVectorParameterValue &P = VectorParameterValues[i];
		const char *Name = P.GetName();
		const FLinearColor &Color = P.ParameterValue;
		EMISSIVE_COLOR(strstr(Name, "emissive"), 100);
#if TRON
		if (ArGame == GAME_Tron)
		{
			EMISSIVE_COLOR(strstr(Name, "pipingcolour"), 90);
		}
#endif
	}

#if TRON
	if (ArGame == GAME_Tron)
	{
		if (Params.Mask && Params.SpecPower && Params.Emissive)
			Params.Mask = NULL;		// some different meaning for this texture
		if (Params.Mask)
		{
			Params.EmissiveChannel      = TC_MA;
			Params.SpecularMaskChannel  = TC_G;
			Params.SpecularPowerChannel = TC_B;
			Params.CubemapMaskChannel   = TC_R;
		}
	}
#endif // TRON

#if BATMAN
	if (ArGame == GAME_Batman2)
	{
		if (Params.Mask)
		{
			Params.SpecularMaskChannel  = TC_R;
			Params.SpecularPowerChannel = TC_G;
			// TC_B = skin mask
		}
	}
#endif // BATMAN

#if BLADENSOUL
	if (ArGame == GAME_BladeNSoul)
	{
		if (Params.Mask)
		{
			Params.CubemapMaskChannel   = TC_B;
			Params.SpecularPowerChannel = TC_G;
			// TC_R = skin
		}
	}
#endif // BLADENSOUL

	// try to get diffuse texture when nothing found
	if (!Params.Diffuse && TextureParameterValues.Num() == 1)
		Params.Diffuse = TextureParameterValues[0].ParameterValue;

	unguard;
}

void UMaterialInstanceConstant::AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered) const
{
	guard(UMaterialInstanceConstant::AppendReferencedTextures);
	if (onlyRendered)
	{
		// default implementation does that
		Super::AppendReferencedTextures(OutTextures, onlyRendered);
	}
	else
	{
		for (int i = 0; i < TextureParameterValues.Num(); i++)
		{
			if (TextureParameterValues[i].ParameterValue)
				OutTextures.AddUnique(TextureParameterValues[i].ParameterValue);
		}
		if (Parent && Parent != this) Parent->AppendReferencedTextures(OutTextures, onlyRendered);
	}
	unguard;
}

#endif // UNREAL3


#endif // RENDERING
