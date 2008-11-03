#ifndef __UNMATERIAL_H__
#define __UNMATERIAL_H__

// Ploygon flags (used in UE1 only ?)
#define PF_Masked			0x00000002
#define PF_Translucent		0x00000004
#define PF_Modulated		0x00000040
#define PF_TwoSided			0x00000100


/*
MATERIALS TREE:
~~~~~~~~~~~~~~~
- Material
    Combiner
-   Modifier
      ColorModifier
-     FinalBlend
      MaterialSequence
      MaterialSwitch
      OpacityModifier
      TexModifier
        TexCoordSource
        TexEnvMap
        TexMatrix
        TexOscillator
          TexOscillatorTriggered
        TexPanner
          TexPannerTriggered
        TexRotator
        TexScaler
        VariableTexPanner
-   RenderedMaterial
-     BitmapMaterial
        ScriptedTexture
        ShadowBitmapMaterial
-       Texture
          Cubemap
      ConstantMaterial
        ConstantColor
        FadeColor
      ParticleMaterial
      ProjectorMaterial
      Shader
      TerrainMaterial
      VertexColor
*/

#if RENDERING
#	define BIND		virtual void Bind(unsigned PolyFlags)
#else
#	define BIND
#endif


class UMaterial : public UObject
{
	DECLARE_CLASS(UMaterial, UObject);
public:
	UMaterial		*FallbackMaterial;
	UMaterial		*DefaultMaterial;
	byte			SurfaceType;			// enum ESurfaceType: wood, metal etc
	int				MaterialType;			// same as SurfaceType ?
#if SPLINTER_CELL
	// Splinter Cell extra fields
	bool			bUseTextureAsHeat;
	UObject			*HeatMaterial;
#endif

	UMaterial()
	:	FallbackMaterial(NULL)
//	,	DefaultMaterial(DefaultTexture)
	{}
	BEGIN_PROP_TABLE
		PROP_OBJ(FallbackMaterial)
		PROP_OBJ(DefaultMaterial)
		PROP_BYTE(SurfaceType)
#if SPLINTER_CELL
		PROP_BOOL(bUseTextureAsHeat)
		PROP_OBJ(HeatMaterial)
#endif
	END_PROP_TABLE

#if LINEAGE2
	virtual void Serialize(FArchive &Ar)
	{
		guard(UMaterial::Serialize);
		Super::Serialize(Ar);
		if (Ar.IsLineage2)
		{
			//?? separate to cpp
			//?? look at latest script code for data layout
			int unk1;
			word unk2, unk3;
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x10)
				Ar << unk1;
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x1E)
			{
				int i;
				// some function
				char c0, TextureTranform, MAX_SAMPLER_NUM, MAX_TEXMAT_NUM, MAX_PASS_NUM, TwoPassRenderState, AlphaRef;
				if (Ar.ArLicenseeVer >= 0x21)
					Ar << c0;
				Ar << TextureTranform << MAX_SAMPLER_NUM << MAX_TEXMAT_NUM << MAX_PASS_NUM << TwoPassRenderState << AlphaRef;
				int SrcBlend, DestBlend, OverriddenFogColor;
				Ar << SrcBlend << DestBlend << OverriddenFogColor;
				// serialize matTexMatrix[16] (strange code)
				for (i = 0; i < 8; i++)
				{
					char b1, b2;
					Ar << b1 << b2;
					for (int j = 0; j < 126; j++)
					{
						// really, 1Kb of floats and ints ...
						char b3;
						Ar << b3;
					}
				}
				// another nested function - serialize FC_* variables
				char c[8];					// union with "int FC_Color1, FC_Color2" (strange code)
				Ar << c[2] << c[1] << c[0] << c[3] << c[6] << c[5] << c[4] << c[7];
				int FC_FadePeriod, FC_FadePhase, FC_ColorFadeType;	// really, floats?
				Ar << FC_FadePeriod << FC_FadePhase << FC_ColorFadeType;
				// end of nested function
				for (i = 0; i < 16; i++)
				{
					FString strTex;			// strTex[16]
					Ar << strTex;
				}
				// end of function
				FString ShaderCode;
				Ar << ShaderCode;
				if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x1F)
					Ar << unk2 << unk3;
			}
		}
		unguard;
	}
#endif

#if RENDERING
	virtual void Bind(unsigned PolyFlags)
	{}
#endif
};


class URenderedMaterial : public UMaterial
{
	DECLARE_CLASS(URenderedMaterial, UMaterial);
	// no properties here
};


enum ETextureFormat
{
	TEXF_P8,			// used 8-bit palette
	TEXF_RGBA7,
	TEXF_RGB16,			// 16-bit texture
	TEXF_DXT1,
	TEXF_RGB8,
	TEXF_RGBA8,			// 32-bit texture
	TEXF_NODATA,
	TEXF_DXT3,
	TEXF_DXT5,
	TEXF_L8,			// 8-bit grayscale
	TEXF_G16,			// 16-bit grayscale (terrain heightmaps)
	TEXF_RRRGGGBBB
};

enum ETexClampMode
{
	TC_Wrap,
	TC_Clamp
};

class UBitmapMaterial : public URenderedMaterial // abstract
{
	DECLARE_CLASS(UBitmapMaterial, URenderedMaterial);
public:
	ETextureFormat	Format;
	ETexClampMode	UClampMode, VClampMode;
	byte			UBits, VBits;	// texture size log2 (number of bits in size value)
	int				USize, VSize;	// texture size
	int				UClamp, VClamp;	// same as UClampMode/VClampMode ?

	BEGIN_PROP_TABLE
		PROP_BYTE(Format)
		PROP_BYTE(UClampMode)
		PROP_BYTE(VClampMode)
		PROP_BYTE(UBits)
		PROP_BYTE(VBits)
		PROP_INT(USize)
		PROP_INT(VSize)
		PROP_INT(UClamp)
		PROP_INT(VClamp)
	END_PROP_TABLE
};


class UPalette : public UObject
{
	DECLARE_CLASS(UPalette, UObject);
public:
	TArray<FColor>	Colors;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UPalette::Serialize);
		Super::Serialize(Ar);
		Ar << Colors;
		assert(Colors.Num() == 256);	// NUM_PAL_COLORS in UT
		// UE1 uses Palette[0] as color {0,0,0,0} when texture uses PF_Masked flag
		// (see UOpenGLRenderDevice::SetTexture())
		if (Ar.ArVer < 100)
			Colors[0].A = 0;
		unguard;
	}
};


enum ELODSet
{
	LODSET_None,
	LODSET_World,
	LODSET_PlayerSkin,
	LODSET_WeaponSkin,
	LODSET_Terrain,
	LODSET_Interface,
	LODSET_RenderMap,
	LODSET_Lightmap,
};


struct FMipmap
{
	TLazyArray<byte>	DataArray;
	int					USize, VSize;
	byte				UBits, VBits;

	friend FArchive& operator<<(FArchive &Ar, FMipmap &M)
	{
#if TRIBES3
		TRIBES_HDR(Ar, 0x1A);
		if (t3_hdrSV >= 1)
		{
			TLazyArray<byte> SkipArray;
			Ar << SkipArray;
		}
#endif
		return Ar << M.DataArray << M.USize << M.VSize << M.UBits << M.VBits;
	}
};


class UTexture : public UBitmapMaterial
{
	DECLARE_CLASS(UTexture, UBitmapMaterial);
public:
	// palette
	UPalette		*Palette;
	// detail texture
	UMaterial		*Detail;
	float			DetailScale;
	// internal info
	FColor			MipZero;
	FColor			MaxColor;
	int				InternalTime[2];
	// texture flags
	bool			bMasked;
	bool			bAlphaTexture;
	bool			bTwoSided;
	bool			bHighColorQuality;
	bool			bHighTextureQuality;
	bool			bRealtime;
	bool			bParametric;
	// level of detail
	ELODSet			LODSet;
	int				NormalLOD;
	int				MinLOD;
	// animation
	UTexture		*AnimNext;
	byte			PrimeCount;
	float			MinFrameRate, MaxFrameRate;
	// mipmaps
	TArray<FMipmap>	Mips;				// native
	ETextureFormat	CompFormat;			// UT1, SplinterCell, Tribes3
	// UE1 fields
	byte			bHasComp;

#if RENDERING
	// rendering implementation fields
	GLint			TexNum;
#endif

	UTexture()
	:	LODSet(LODSET_World)
	,	MipZero(64, 128, 64, 0)
	,	MaxColor(255, 255, 255, 255)
	,	DetailScale(8.0f)
#if RENDERING
	,	TexNum(-1)
#endif
	{}
#if RENDERING
	byte *Decompress(int &USize, int &VSize) const;
#endif
	virtual void Serialize(FArchive &Ar)
	{
		guard(UTexture::Serialize);
		Super::Serialize(Ar);
		Ar << Mips;
		if (Ar.ArVer < 100)
		{
			// UE1
			bMasked = false;			// ignored by UE1, used surface.PolyFlags instead (but UE2 ignores PolyFlags ...)
			if (bHasComp)				// skip compressed mipmaps
			{
				TArray<FMipmap>	CompMips;
				Ar << CompMips;
			}
		}
		unguard;
	}
	BEGIN_PROP_TABLE
		PROP_OBJ(Palette)
		PROP_OBJ(Detail)
		PROP_FLOAT(DetailScale)
		PROP_COLOR(MipZero)
		PROP_COLOR(MaxColor)
		PROP_INT(InternalTime)
		PROP_BOOL(bMasked)
		PROP_BOOL(bAlphaTexture)
		PROP_BOOL(bTwoSided)
		PROP_BOOL(bHighColorQuality)
		PROP_BOOL(bHighTextureQuality)
		PROP_BOOL(bRealtime)
		PROP_BOOL(bParametric)
		PROP_BYTE(LODSet)
		PROP_INT(NormalLOD)
		PROP_INT(MinLOD)
		PROP_OBJ(AnimNext)
		PROP_BYTE(PrimeCount)
		PROP_FLOAT(MinFrameRate)
		PROP_FLOAT(MaxFrameRate)
		PROP_BYTE(CompFormat)
		PROP_BOOL(bHasComp)
	END_PROP_TABLE

	BIND;
};


class UModifier : public UMaterial
{
	DECLARE_CLASS(UModifier, UMaterial);
public:
	UMaterial		*Material;

	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
	END_PROP_TABLE
};


enum EFrameBufferBlending
{
	FB_Overwrite,
	FB_Modulate,
	FB_AlphaBlend,
	FB_AlphaModulate_MightNotFogCorrectly,
	FB_Translucent,
	FB_Darken,
	FB_Brighten,
	FB_Invisible,
};

class UFinalBlend : public UModifier
{
	DECLARE_CLASS(UFinalBlend, UModifier);
public:
	EFrameBufferBlending FrameBufferBlending;
	bool			ZWrite;
	bool			ZTest;
	bool			AlphaTest;
	bool			TwoSided;
	byte			AlphaRef;
#if LINEAGE2
	bool			TreatAsTwoSided;			// strange ...
#endif

	UFinalBlend()
	:	FrameBufferBlending(FB_Overwrite)
	,	ZWrite(true)
	,	ZTest(true)
	,	TwoSided(false)
	{}
	BEGIN_PROP_TABLE
		PROP_BYTE(FrameBufferBlending)
		PROP_BOOL(ZWrite)
		PROP_BOOL(ZTest)
		PROP_BOOL(AlphaTest)
		PROP_BOOL(TwoSided)
		PROP_BYTE(AlphaRef)
		PROP_BOOL(TreatAsTwoSided)
	END_PROP_TABLE

	BIND;
};


#define REGISTER_MATERIAL_CLASSES		\
	REGISTER_CLASS(UMaterial)			\
	REGISTER_CLASS(UBitmapMaterial)		\
	REGISTER_CLASS(UPalette)			\
	REGISTER_CLASS(UTexture)			\
	REGISTER_CLASS(UFinalBlend)


#endif // __UNMATERIAL_H__
