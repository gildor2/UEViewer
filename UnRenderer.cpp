#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"

#include "GlWindow.h"
#include "libs/ddslib.h"


#define MAX_IMG_SIZE		4096
#define DEFAULT_TEX_NUM		2			// note: glIsTexture(0) will always return GL_FALSE
#define RESERVED_TEXTURES	32


/*-----------------------------------------------------------------------------
	Mipmapping and resampling
-----------------------------------------------------------------------------*/

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


static void Upload(int handle, const void *pic, int width, int height, bool doMipmap, bool clampS, bool clampT)
{
	guard(Upload);

	/*----- Calculate internal dimensions of the new texture --------*/
	int scaledWidth, scaledHeight;
	GetImageDimensions(width, height, &scaledWidth, &scaledHeight);

	/*---------------- Resample/lightscale texture ------------------*/
	unsigned *scaledPic = new unsigned [scaledWidth * scaledHeight];
	if (width != scaledWidth || height != scaledHeight)
		ResampleTexture((unsigned*)pic, width, height, scaledPic, scaledWidth, scaledHeight);
	else
		memcpy(scaledPic, pic, scaledWidth * scaledHeight * sizeof(unsigned));

	/*------------- Determine texture format to upload --------------*/
	GLenum format;
	int alpha = 1; //?? image->alphaType;
	format = (alpha ? 4 : 3);

	/*------------------ Upload the image ---------------------------*/
	glBindTexture(GL_TEXTURE_2D, handle);
	glTexImage2D(GL_TEXTURE_2D, 0, format, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledPic);
	if (!doMipmap)
	{
		// setup min/max filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		int miplevel = 0;
		while (scaledWidth > 1 || scaledHeight > 1)
		{
			MipMap((byte *) scaledPic, scaledWidth, scaledHeight);
			miplevel++;
			scaledWidth  >>= 1;
			scaledHeight >>= 1;
			if (scaledWidth < 1)  scaledWidth  = 1;
			if (scaledHeight < 1) scaledHeight = 1;
			glTexImage2D(GL_TEXTURE_2D, miplevel, format, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledPic);
		}
		// setup min/max filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);	// trilinear filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// setup wrap flags
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampS ? GL_CLAMP : GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampT ? GL_CLAMP : GL_REPEAT);

	delete scaledPic;

	unguard;
}


/*-----------------------------------------------------------------------------
	Unreal materials support
-----------------------------------------------------------------------------*/

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


static byte *DecompressTexture(const byte *Data, int width, int height, ETextureFormat SrcFormat,
	const char *Name, UPalette *Palette)
{
	guard(DecompressTexture);
	int size = width * height * 4;
	byte *dst = new byte [size];

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
	case TEXF_RGBA8:
		memcpy(dst, Data, size);
		return dst;
	}

	// setup for DDSLib
	ddsBuffer_t dds;
	memcpy(dds.magic, "DDS ", 4);
	dds.size   = 124;
	dds.width  = width;
	dds.height = height;
	dds.data   = const_cast<byte*>(Data);
	switch (SrcFormat)
	{
	case TEXF_DXT1:
		dds.pixelFormat.fourCC = BYTES4('D','X','T','1');
		break;
	case TEXF_DXT3:
		dds.pixelFormat.fourCC = BYTES4('D','X','T','3');
		break;
	case TEXF_DXT5:
		dds.pixelFormat.fourCC = BYTES4('D','X','T','5');
		break;
	default:
		appNotify("%s: unknown texture format %d \n", Name, SrcFormat);
		memset(dst, 0xFF, size);
		return dst;
	}
	if (DDSDecompress(&dds, dst) != 0)
		appError("Error in DDSDecompress");
	if (SrcFormat == TEXF_DXT1)
		PostProcessAlpha(dst, width, height);

	return dst;
	unguardf(("fmt=%d", SrcFormat));
}


UMaterial *BindDefaultMaterial()
{
	static UTexture *Mat = NULL;
	if (!Mat)
	{
		// allocate material
		Mat = new UTexture;
		Mat->bTwoSided = true;
		Mat->Mips.Add();
		Mat->Format = TEXF_RGBA8;
		// create 1st mipmap
#define TEX_SIZE	64
		FMipmap &Mip = Mat->Mips[0];
		Mip.USize = Mip.VSize = TEX_SIZE;
		Mip.DataArray.Add(TEX_SIZE*TEX_SIZE*4);
		byte *pic = &Mip.DataArray[0];
		for (int x = 0; x < TEX_SIZE; x++)
		{
			for (int y = 0; y < TEX_SIZE; y++)
			{
				static const byte colors[4][4] = {
					{255,128,0}, {0,192,192}, {255,64,64}, {64,255,64}
				};
				byte *p = pic + y * TEX_SIZE * 4 + x * 4;
				int i1 = x < TEX_SIZE / 2;
				int i2 = y < TEX_SIZE / 2;
				const byte *c = colors[i1 * 2 + i2];
				memcpy(p, c, 4);
			}
		}
#undef TEX_SIZE
	}
	Mat->Bind(0);
	return Mat;
}


byte *UTexture::Decompress(int &USize, int &VSize) const
{
	guard(UTexture::Decompress);
	//?? combine with DecompressTexture() ?
	for (int n = 0; n < Mips.Num(); n++)
	{
		// find 1st mipmap with non-null data array
		// reference: DemoPlayerSkins.utx/DemoSkeleton have null-sized 1st 2 mips
		const FMipmap &Mip = Mips[n];
		if (!Mip.DataArray.Num())
			continue;
		USize = Mip.USize;
		VSize = Mip.VSize;
		return DecompressTexture(&Mip.DataArray[0], USize, VSize, Format, Name, Palette);
	}
	// no valid mipmaps
	return NULL;
	unguard;
}


//!! note: unloading textures is not supported now
void UTexture::Bind(unsigned PolyFlags)
{
	guard(UTexture::Bind);
	glEnable(GL_TEXTURE_2D);
	// bTwoSided
	if (bTwoSided || (PolyFlags & PF_TwoSided))
		glDisable(GL_CULL_FACE);
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
		glAlphaFunc(GL_GREATER, 0.0f);
	}
	// blending
	// partially taken from UT/OpenGLDrv
	if (PolyFlags & PF_Translucent)				// UE1
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	}
	else if (PolyFlags & PF_Modulated)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	}
	else if (bAlphaTexture || bMasked || (PolyFlags & PF_Masked))
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}
	// uploading ...
	bool upload = false;
	if (TexNum < 0)
	{
		static GLint lastTexNum = RESERVED_TEXTURES;
		TexNum = ++lastTexNum;
		upload = true;
	}
	else if (!glIsTexture(TexNum))
	{
		// surface lost (window resized etc), should re-upload texture
		upload = true;
	}
	if (upload)
	{
		// upload texture
		int USize, VSize;
		byte *pic = Decompress(USize, VSize);
		if (pic)
		{
			Upload(TexNum, pic, USize, VSize, Mips.Num() > 1,
				UClampMode == TC_Clamp, VClampMode == TC_Clamp);
			delete pic;
		}
		else
		{
			appNotify("WARNING: texture %s has no valid mipmaps", Name);
			TexNum = DEFAULT_TEX_NUM;		// "default texture"
		}
	}
	// bind texture
	glBindTexture(GL_TEXTURE_2D, TexNum);
	unguardf(("%s", Name));
}


void UFinalBlend::Bind(unsigned PolyFlags)
{
	if (!Material)
	{
		BindDefaultMaterial();
		return;
	}
	Material->Bind(PolyFlags);
	// override material settings
	// TwoSided
	if (TwoSided || (PolyFlags & PF_TwoSided))
		glDisable(GL_CULL_FACE);
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	// AlphaTest
	if (AlphaTest || (PolyFlags & PF_Masked))
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, AlphaRef / 255.0f);
	}
	else
		glDisable(GL_ALPHA_TEST);
	// FrameBufferBlending
	if (FrameBufferBlending == FB_Overwrite)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);
	switch (FrameBufferBlending)
	{
	case FB_Overwrite:
		glBlendFunc(GL_ONE, GL_ZERO);				// src
		break;
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
}
