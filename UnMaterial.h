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
#if SPLINTER_CELL
	byte			CompFormat;
#endif
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
#if SPLINTER_CELL
		PROP_BYTE(CompFormat)
#endif
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
