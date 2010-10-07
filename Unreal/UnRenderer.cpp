#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"


#if RENDERING

#include "GlWindow.h"

#include "Shaders.h"

#define MAX_IMG_SIZE		4096


static GLuint DefaultTexNum = 0;		//?? bad code ... should move default texture to CoreGL ?

//#define SHOW_SHADER_PARAMS	1

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
	return Sh;

	unguardf(("type=%d", type));
}


#if SHOW_SHADER_PARAMS
#define DBG(x)		DrawTextLeft x
#else
#define DBG(x)
#endif

const CShader &GL_NormalmapShader(CShader &shader, CMaterialParams &Params)
{
	guard(GL_NormalmapShader);

	const char *subst[10];

	enum
	{
		I_Diffuse = 0,
		I_Normal,
		I_Specular,
		I_SpecularPower,
		I_Opacity,
		I_Emissive
	};

	DBG(("--------"));

	// diffuse
	glActiveTexture(GL_TEXTURE0);	// used for BindDefaultMaterial() too
	if (Params.Diffuse)
	{
		DBG(("Diffuse  : %s", Params.Diffuse->Name));
		Params.Diffuse->Bind();
	}
	else
		BindDefaultMaterial();
	// normal
	if (Params.Normal)	//!! reimplement ! plus, check for correct normalmap texture (VTC texture compression etc ...)
	{
		DBG(("Normal   : %s", Params.Normal->Name));
		glActiveTexture(GL_TEXTURE0 + I_Normal);
		Params.Normal->Bind();
	}
	// specular
	if (Params.Specular)
	{
		DBG(("Specular : %s", Params.Specular->Name));
		glActiveTexture(GL_TEXTURE0 + I_Specular);
		Params.Specular->Bind();
	}
	// specular power
	if (Params.SpecPower)
	{
		DBG(("SpecPower: %s", Params.SpecPower->Name));
		glActiveTexture(GL_TEXTURE0 + I_SpecularPower);
		Params.SpecPower->Bind();
	}
	// opacity mask
	if (Params.Opacity)
	{
		DBG(("Opacity  : %s", Params.Opacity->Name));
		glActiveTexture(GL_TEXTURE0 + I_Opacity);
		Params.Opacity->Bind();
	}
	// emission
	if (Params.Emissive)
	{
		DBG(("Emissive : %s", Params.Emissive->Name));
		glActiveTexture(GL_TEXTURE0 + I_Emissive);
		Params.Emissive->Bind();
	}

	const char *specularExpr = "vec3(0.0)"; //?? vec3(specular)
	if (Params.Specular)
	{
		specularExpr = va("texture2D(specTex, TexCoord).%s * vec3(specular) * 1.5", !Params.SpecularFromAlpha ? "rgb" : "a");
	}
	const char *opacityExpr = "1.0";
	if (Params.Opacity)
	{
		opacityExpr = va("texture2D(opacTex, TexCoord).%s", !Params.OpacityFromAlpha ? "g" : "a");
	}

	//!! NOTE: Specular and SpecPower are scaled by const to improve visual; should be scaled by parameters from material
	subst[0] = Params.Normal    ? "texture2D(normTex, TexCoord).rgb * 2.0 - 1.0"  : "vec3(0.0, 0.0, 1.0)";
	subst[1] = specularExpr;
	subst[2] = Params.SpecPower ? "texture2D(spPowTex, TexCoord).g * 100.0 + 5.0" : "gl_FrontMaterial.shininess";
	subst[3] = opacityExpr;	//?? Params.Opacity   ? "texture2D(opacTex, TexCoord).g"                : "1.0";
	subst[4] = Params.Emissive  ? "vec3(0.5,0.5,1.0) * texture2D(emisTex, TexCoord).g * 2.0" : "vec3(0.0)"; //?? not scaled, because sometimes looks ugly ...
	// finalize paramerers and make shader
	subst[5] = NULL;

	glActiveTexture(GL_TEXTURE0);


	if (!shader.IsValid()) shader.Make(Normal_ush, NULL, subst);
	shader.Use();

	shader.SetUniform("diffTex",  I_Diffuse);
	shader.SetUniform("normTex",  I_Normal);
	shader.SetUniform("specTex",  I_Specular);
	shader.SetUniform("spPowTex", I_SpecularPower);
	shader.SetUniform("opacTex",  I_Opacity);
	shader.SetUniform("emisTex",  I_Emissive);
	return shader;

	unguard;
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


void UUnrealMaterial::SetMaterial(unsigned PolyFlags)
{
	guard(UUnrealMaterial::SetMaterial);

	SetupGL(PolyFlags);

	CMaterialParams Params;
	GetParams(Params);

	if (!Params.Diffuse)	//?? !Params.IsNull()
	{
		//?? may be, use diffuse color + other params
		BindDefaultMaterial();
		return;
	}

#if USE_GLSL
	if (GL_SUPPORT(QGL_2_0))
	{
		if (Params.Diffuse == this)
		{
			// simple texture
			Params.Diffuse->Bind();
			GL_UseGenericShader(GS_Textured);
		}
		else
		{
			GL_NormalmapShader(GLShader, Params);
		}
	}
	else
#endif // USE_GLSL
	{
		if (Params.Diffuse)	//?? already checked above
			Params.Diffuse->Bind();
		else
			BindDefaultMaterial();
	}

	unguardf(("%s", Name));
}


void UUnrealMaterial::SetupGL(unsigned PolyFlags)
{
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
}


void BindDefaultMaterial(bool White)
{
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
		}
		glActiveTexture(GL_TEXTURE0);
	}

#if USE_GLSL
	if (GL_SUPPORT(QGL_2_0)) GL_UseGenericShader(White ? GS_White : GS_Textured);
#endif

	if (White)
	{
		glDisable(GL_TEXTURE_2D);
		return;
	}

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
					{192,64,64,255}, {32,32,255,255}, {32,255,32,255}, {32,192,192,255}	// BGRA
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
	Mat->Bind();
	DefaultTexNum = Mat->TexNum;
}


void UTexture::SetupGL(unsigned PolyFlags)
{
	guard(UTexture::SetupGL);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
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
	else if (bAlphaTexture || (PolyFlags & (PF_Masked|PF_TwoSided)))
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.1f);
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
	else if (bAlphaTexture || bMasked || (PolyFlags & (PF_Masked/*|PF_TwoSided*/)))
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


void UTexture::Bind()
{
	guard(UTexture::Bind);

	glEnable(GL_TEXTURE_2D);

	bool upload = !GL_TouchObject(DrawTimestamp);

	if (upload)
	{
		// upload texture
		glGenTextures(1, &TexNum);
		int USize, VSize;
		byte *pic = Decompress(USize, VSize);
		if (pic)
		{
			Upload(TexNum, pic, USize, VSize, Mips.Num() > 1, UClampMode == TC_Clamp, VClampMode == TC_Clamp);
			delete pic;
		}
		else
		{
			appNotify("WARNING: texture %s has no valid mipmaps", Name);
			//?? note: not working (no access to generated default texture!!)
			//?? also should glDeleteTextures(1, &TexNum) - generated but not used?
			if (!DefaultTexNum) BindDefaultMaterial();	//?? will produce bad result, but only for one frame
			TexNum = DefaultTexNum;						// "default texture"
		}
	}
	// bind texture
	glBindTexture(GL_TEXTURE_2D, TexNum);

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
	if (GL_IsValidObject(TexNum, DrawTimestamp) && TexNum != DefaultTexNum)
		glDeleteTextures(1, &TexNum);
	Super::Release();
	unguard;
}


void UModifier::SetupGL(unsigned PolyFlags)
{
	guard(UModifier::SetupGL);
	if (Material) Material->SetupGL(PolyFlags);
	unguard;
}

void UModifier::GetParams(CMaterialParams &Params) const
{
	guard(UModifier::GetParams);
	if (Material)
		Material->GetParams(Params);
	unguard;
}


void UFinalBlend::SetupGL(unsigned PolyFlags)
{
	guard(UFinalBlend::SetupGL);

	glEnable(GL_DEPTH_TEST);
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


void UShader::SetupGL(unsigned PolyFlags)
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

	unguardf(("%s", Name));
}


bool UShader::IsTranslucent() const
{
	return (OutputBlending != OB_Normal);
}



#if BIOSHOCK

//?? NOTE: Bioshock EFrameBufferBlending is used for UShader and UFinalBlend, plus it has different values
// based on UShader and UFinalBlend Bind()
void UFacingShader::SetupGL(unsigned PolyFlags)
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
	return OutputBlending != FB_Overwrite;
}

#endif // BIOSHOCK


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

//!! NOTE: when using this function sharing of shader between MaterialInstanceConstant's is impossible
//!! (shader may differs because of different texture sets - some available, some - not)

void UMaterial3::SetupGL(unsigned PolyFlags)
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
	if (bIsMasked)
	{
		glEnable(GL_BLEND);
//		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glAlphaFunc(GL_GREATER, OpacityMaskClipValue);
		glEnable(GL_ALPHA_TEST);
	}

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

	unguard;
}


bool UMaterial3::IsTranslucent() const
{
	return BlendMode != BLEND_Opaque;
}


void UMaterial3::GetParams(CMaterialParams &Params) const
{
	guard(UMaterial3::GetParams);

	int DiffWeight = 0, NormWeight = 0, SpecWeight = 0, SpecPowWeight = 0, OpWeight = 0, EmWeight = 0;
#define DIFFUSE(check,weight)			\
	if (check && weight > DiffWeight)	\
	{									\
	/*	DrawTextLeft("D: %d > %d = %s", weight, DiffWeight, Tex->Name); */ \
		Params.Diffuse = Tex;			\
		DiffWeight = weight;			\
	}
#define NORMAL(check,weight)			\
	if (check && weight > NormWeight)	\
	{									\
	/*	DrawTextLeft("N: %d > %d = %s", weight, NormWeight, Tex->Name); */ \
		Params.Normal = Tex;			\
		NormWeight = weight;			\
	}
#define SPECULAR(check,weight)			\
	if (check && weight > SpecWeight)	\
	{									\
	/*	DrawTextLeft("S: %d > %d = %s", weight, SpecWeight, Tex->Name); */ \
		Params.Specular = Tex;			\
		SpecWeight = weight;			\
	}
#define SPECPOW(check,weight)			\
	if (check && weight > SpecPowWeight)\
	{									\
	/*	DrawTextLeft("SP: %d > %d = %s", weight, SpecPowWeight, Tex->Name); */ \
		Params.SpecPower = Tex;			\
		SpecPowWeight = weight;			\
	}
#define OPACITY(check,weight)			\
	if (check && weight > OpWeight)		\
	{									\
	/*	DrawTextLeft("O: %d > %d = %s", weight, OpWeight, Tex->Name); */ \
		Params.Opacity = Tex;			\
		OpWeight = weight;				\
	}
#define EMISSIVE(check,weight)			\
	if (check && weight > EmWeight)		\
	{									\
	/*	DrawTextLeft("E: %d > %d = %s", weight, EmWeight, Tex->Name); */ \
		Params.Emissive = Tex;			\
		EmWeight = weight;				\
	}
	for (int i = 0; i < ReferencedTextures.Num(); i++)
	{
		UTexture3 *Tex = ReferencedTextures[i];
		if (!Tex) continue;
		const char *Name = Tex->Name;
		int len = strlen(Name);
		//!! - separate code (common for UMaterial3 + UMaterialInstanceConstant)
		//!! - may implement with tables + macros
		//!! - catch normalmap, specular and emissive textures
		if (appStristr(Name, "noise")) continue;
		DIFFUSE(appStristr(Name, "diff"), 100);
		NORMAL (appStristr(Name, "norm"), 100);
		DIFFUSE(!stricmp(Name + len - 4, "_Tex"), 80);
		DIFFUSE (appStristr(Name, "_Tex"), 60)
		DIFFUSE(!stricmp(Name + len - 2, "_D"), 20);
		OPACITY(appStristr(Name, "_OM"), 20);
#if 0
		if (!stricmp(Name + len - 3, "_DI"))		// The Last Remnant ...
			Diffuse = Tex;
		if (!strnicmp(Name + len - 4, "_DI", 3))	// The Last Remnant ...
			Diffuse = Tex;
		if (appStristr(Name, "_Diffuse"))
			Diffuse = Tex;
#endif
		DIFFUSE (appStristr(Name, "_DI"), 20)
		DIFFUSE (appStristr(Name, "_MA"), 10)		// The Last Remnant; low priority
		DIFFUSE (appStristr(Name, "_D" ), 11)
		DIFFUSE (!stricmp(Name + len - 2, "_C"), 10);
		NORMAL  (!stricmp(Name + len - 2, "_N"), 20);
		NORMAL  (!stricmp(Name + len - 3, "_NM"), 20);
		SPECULAR(!stricmp(Name + len - 2, "_S"), 20);
		SPECPOW (!stricmp(Name + len - 3, "_SP"), 20);
		EMISSIVE(!stricmp(Name + len - 2, "_E"), 20);
		OPACITY (!stricmp(Name + len - 2, "_A"), 20);
		OPACITY (!stricmp(Name + len - 5, "_Mask"), 10);
		// Magna Catra 2
		DIFFUSE (!strnicmp(Name, "df_", 3), 20);
		SPECULAR(!strnicmp(Name, "sp_", 3), 20);
//		OPACITY (!strnicmp(Name, "op_", 3), 20);
		NORMAL  (!strnicmp(Name, "no_", 3), 20);

		NORMAL  (appStristr(Name, "Norm"), 80);
		EMISSIVE(appStristr(Name, "Emis"), 80);
		SPECULAR(appStristr(Name, "Specular"), 80);
		OPACITY (appStristr(Name, "Opac"),  80);

		DIFFUSE(i == 0, 1)							// 1st texture as lowest weight
	}

	unguard;
}


void UTexture2D::Bind()
{
	guard(UTexture2D::Bind);

	glEnable(GL_TEXTURE_2D);

	bool upload = !GL_TouchObject(DrawTimestamp);

	if (upload)
	{
		// upload texture
		glGenTextures(1, &TexNum);
		int USize, VSize;
		byte *pic = Decompress(USize, VSize);
		if (pic)
		{
			Upload(TexNum, pic, USize, VSize, Mips.Num() > 1, AddressX == TA_Clamp, AddressY == TA_Clamp);
			delete pic;
		}
		else
		{
			appNotify("WARNING: texture %s has no valid mipmaps", Name);
			if (!DefaultTexNum) BindDefaultMaterial();	//?? will produce bad result, but only for one frame
			TexNum = DefaultTexNum;						// "default texture"; not working (see UTexture::Bind())
		}
	}
	// bind texture
	glBindTexture(GL_TEXTURE_2D, TexNum);

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
	Super::Release();
	unguard;
}


void UMaterialInstanceConstant::SetupGL(unsigned PolyFlags)
{
	// redirect to Parent until UMaterial3
	if (Parent) Parent->SetupGL(PolyFlags);
}


void UMaterialInstanceConstant::GetParams(CMaterialParams &Params) const
{
	guard(UMaterialInstanceConstant::GetParams);

	// get params from linked UMaterial3
	if (Parent) Parent->GetParams(Params);

	// get local parameters
	bool normalSet = false;
	for (int i = 0; i < TextureParameterValues.Num(); i++)
	{
		const FTextureParameterValue &P = TextureParameterValues[i];
//		if (!P.ParameterValue) continue;
		const char *p = P.ParameterName;
		if (appStristr(p, "diff") || appStristr(p, "color"))
			Params.Diffuse = P.ParameterValue;
		else if (appStristr(p, "norm") && !appStristr(p, "fx") && !normalSet)	// no NormalFX
		{
			Params.Normal = P.ParameterValue;
			normalSet = true;
		}
		else if (appStristr(p, "specpow"))
			Params.SpecPower = P.ParameterValue;
		else if (appStristr(p, "spec"))
			Params.Specular = P.ParameterValue;
		else if (appStristr(p, "emiss"))
			Params.Emissive = P.ParameterValue;
	}

	// try to get diffuse texture when nothing found
	if (!Params.Diffuse && TextureParameterValues.Num() == 1)
		Params.Diffuse = TextureParameterValues[0].ParameterValue;

	unguard;
}


#endif // UNREAL3


#endif // RENDERING
