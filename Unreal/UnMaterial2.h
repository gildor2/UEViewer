#ifndef __UNMATERIAL2_H__
#define __UNMATERIAL2_H__

// Ploygon flags (used in UE1 only ?)
#define PF_Masked			0x00000002
#define PF_Translucent		0x00000004
#define PF_Modulated		0x00000040
#define PF_TwoSided			0x00000100


/*
UE2 MATERIALS TREE:
~~~~~~~~~~~~~~~~~~~
-	Material
-		Combiner
-		Modifier
			ColorModifier
-			FinalBlend
			MaterialSequence
			MaterialSwitch
			OpacityModifier
-			TexModifier
				TexCoordSource
-				TexEnvMap
				TexMatrix
-				TexOscillator
					TexOscillatorTriggered
-				TexPanner
					TexPannerTriggered
-				TexRotator
-				TexScaler
				VariableTexPanner
-		RenderedMaterial
-			BitmapMaterial
				ScriptedTexture
				ShadowBitmapMaterial
-				Texture
					Cubemap
-			ConstantMaterial
-				ConstantColor
				FadeColor
			ParticleMaterial
			ProjectorMaterial
-			Shader
			TerrainMaterial
			VertexColor
#if UC2
			PixelShaderMaterial
				PSEmissiveShader
				PSSkinShader
					PSEmissiveSkinShader
#endif
*/

/*-----------------------------------------------------------------------------
	Unreal Engine 1/2 materials
-----------------------------------------------------------------------------*/

#if LINEAGE2

struct FLineageMaterialStageProperty
{
	FString			unk1;
	TArray<FString>	unk2;

	friend FArchive& operator<<(FArchive &Ar, FLineageMaterialStageProperty &P)
	{
		return Ar << P.unk1 << P.unk2;
	}
};

struct FLineageShaderProperty
{
	// possibly, MaterialInfo, TextureTranform, TwoPassRenderState, AlphaRef
	byte			b1, b2;
	byte			b3[5], b4[5], b5[5], b6[5], b7[5], b8[5];
	// possibly, SrcBlend, DestBlend, OverriddenFogColor
	int				i1[5], i2[5], i3[5];
	// nested structure
	// possibly, int FC_Color1, FC_Color2 (strange byte order)
	byte			be[8];
	// possibly, float FC_FadePeriod, FC_FadePhase, FC_ColorFadeType
	int				ie[3];
	// stages
	TArray<FLineageMaterialStageProperty> Stages;

	friend FArchive& operator<<(FArchive &Ar, FLineageShaderProperty &P)
	{
		int i;
		Ar << P.b1 << P.b2;

		if (Ar.ArVer < 129)
		{
			Ar << P.b3[0] << P.b4[0] << P.i1[0] << P.i2[0] << P.i3[0];
		}
		else if (Ar.ArVer == 129)
		{
			for (i = 0; i < 5; i++)
			{
				Ar << P.b3[i] << P.b4[i];
				Ar << P.i1[i] << P.i2[i] << P.i3[i];
			}
		}
		else // if (Ar.ArVer >= 130)
		{
			for (i = 0; i < 5; i++)
			{
				Ar << P.b3[i] << P.b4[i] << P.b5[i] << P.b6[i] << P.b7[i] << P.b8[i];
				Ar << P.i2[i] << P.i3[i];
			}
		}

		for (i = 0; i < 8; i++) Ar << P.be[i];
		for (i = 0; i < 3; i++) Ar << P.ie[i];
		Ar << P.Stages;
		return Ar;
	}
};

#endif // LINEAGE2

#if BIOSHOCK

enum EMaskChannel
{
	MC_A,
	MC_R,
	MC_G,
	MC_B
};

class UMaterial;

struct FMaskMaterial
{
	DECLARE_STRUCT(FMaskMaterial);
	UMaterial		*Material;
	EMaskChannel	Channel;

	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
		PROP_ENUM(Channel)
	END_PROP_TABLE
};

#endif // BIOSHOCK


class UMaterial : public UUnrealMaterial	// real base is UObject
{
	DECLARE_CLASS(UMaterial, UUnrealMaterial);
public:
	UMaterial		*FallbackMaterial;
	UMaterial		*DefaultMaterial;
	byte			SurfaceType;			// enum ESurfaceType: wood, metal etc
//	int				MaterialType;			// int for UE2, but byte for EXTEEL
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
		PROP_ENUM(SurfaceType)
//		PROP_INT(MaterialType)
		PROP_DROP(MaterialType)
#if SPLINTER_CELL
		PROP_BOOL(bUseTextureAsHeat)
		PROP_OBJ(HeatMaterial)
#endif // SPLINTER_CELL
#if BIOSHOCK
		PROP_DROP(MaterialVisualType)
		PROP_DROP(Subtitle)
		PROP_DROP(AcceptProjectors)
#endif // BIOSHOCK
	END_PROP_TABLE

#if LINEAGE2
	virtual void Serialize(FArchive &Ar)
	{
		guard(UMaterial::Serialize);
		Super::Serialize(Ar);
		if (Ar.Game == GAME_Lineage2)
		{
			guard(SerializeLineage2Material);
			//?? separate to cpp
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 16 && Ar.ArLicenseeVer < 37)
			{
				int unk1;
				Ar << unk1;					// simply drop obsolete variable (int Reserved ?)
			}
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 30 && Ar.ArLicenseeVer < 37)
			{
				int i;
				// some function
				byte MaterialInfo, TextureTranform, MAX_SAMPLER_NUM, MAX_TEXMAT_NUM, MAX_PASS_NUM, TwoPassRenderState, AlphaRef;
				if (Ar.ArLicenseeVer >= 33 && Ar.ArLicenseeVer < 36)
					Ar << MaterialInfo;
				Ar << TextureTranform << MAX_SAMPLER_NUM << MAX_TEXMAT_NUM << MAX_PASS_NUM << TwoPassRenderState << AlphaRef;
				int SrcBlend, DestBlend, OverriddenFogColor;
				Ar << SrcBlend << DestBlend << OverriddenFogColor;
				// serialize matTexMatrix[16] (strange code)
				for (i = 0; i < 8; i++)
				{
					char b1, b2;
					Ar << b1;
					if (Ar.ArLicenseeVer < 36) Ar << b2;
					for (int j = 0; j < 126; j++)
					{
						// really, 1Kb of floats and ints ...
						char b3;
						Ar << b3;
					}
				}
				// another nested function - serialize FC_* variables
				char c[8];					// union with "int FC_Color1, FC_Color2" (strange byte order)
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
			}
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 37)
			{
				// ShaderProperty + ShaderCode
				FLineageShaderProperty ShaderProp;
				FString ShaderCode;
				Ar << ShaderProp << ShaderCode;
			}
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 31)
			{
				uint16 ver1, ver2;			// 'int MaterialCodeVersion' serialized as 2 words
				Ar << ver1 << ver2;
			}
			unguard;
		}
		unguard;
	}
#endif // LINEAGE2
};


class URenderedMaterial : public UMaterial
{
	DECLARE_CLASS(URenderedMaterial, UMaterial);
	// no properties here
};


class UConstantMaterial : public URenderedMaterial
{
	DECLARE_CLASS(UConstantMaterial, URenderedMaterial);
};


class UConstantColor : public UConstantMaterial
{
	DECLARE_CLASS(UConstantColor, UConstantMaterial);
public:
	FColor			Color;

	BEGIN_PROP_TABLE
		PROP_COLOR(Color)
	END_PROP_TABLE
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
	TEXF_RRRGGGBBB,
	// Tribes texture formats
	TEXF_CxV8U8,
	TEXF_DXT5N,			// Note: in Bioshock this value has name 3DC, but really DXT5N is used
	TEXF_3DC,			// BC5 compression
};

_ENUM(ETextureFormat)
{
	_E(TEXF_P8),
	_E(TEXF_RGBA7),
	_E(TEXF_RGB16),
	_E(TEXF_DXT1),
	_E(TEXF_RGB8),
	_E(TEXF_RGBA8),
	_E(TEXF_NODATA),
	_E(TEXF_DXT3),
	_E(TEXF_DXT5),
	_E(TEXF_L8),
	_E(TEXF_G16),
	_E(TEXF_RRRGGGBBB),
	// Tribes texture formats
	_E(TEXF_CxV8U8),
	_E(TEXF_DXT5N),
	_E(TEXF_3DC),
};

enum ETexClampMode
{
	TC_Wrap,
	TC_Clamp
};

_ENUM(ETexClampMode)
{
	_E(TC_Wrap),
	_E(TC_Clamp)
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
		PROP_ENUM2(Format, ETextureFormat)
		PROP_ENUM2(UClampMode, ETexClampMode)
		PROP_ENUM2(VClampMode, ETexClampMode)
		PROP_BYTE(UBits)
		PROP_BYTE(VBits)
		PROP_INT(USize)
		PROP_INT(VSize)
		PROP_INT(UClamp)
		PROP_INT(VClamp)
#if BIOSHOCK
		PROP_DROP(Type)
#endif
	END_PROP_TABLE

#if RENDERING
	virtual bool IsTexture() const
	{
		return true;
	}
#endif // RENDERING
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
		// Note: UT2 does not use paletted textures, but HP3 does ...
		Colors[0].A = 0;
#if UNDYING
		if (Ar.Game == GAME_Undying)
		{
			int unk;
			Ar << unk;
		}
#endif // UNDYING
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
		if (t3_hdrSV == 1)
		{
			TLazyArray<byte> SkipArray;
			Ar << SkipArray;
		}
#endif // TRIBES3
		Ar << M.DataArray << M.USize << M.VSize << M.UBits << M.VBits;
		return Ar;
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
#if BIOSHOCK
	int64			CachedBulkDataSize;
	bool			HasBeenStripped;
	byte			StrippedNumMips;
	bool			bBaked;
#endif

#if RENDERING
	// rendering implementation fields
	GLuint			TexNum;
#endif

	UTexture()
	:	LODSet(LODSET_World)
	,	MipZero(64, 128, 64, 0)
	,	MaxColor(255, 255, 255, 255)
	,	DetailScale(8.0f)
#if RENDERING
	,	TexNum(0)
#endif
	{}
	virtual bool GetTextureData(CTextureData &TexData) const;
	virtual void Serialize(FArchive &Ar);

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
		PROP_ENUM(LODSet)
		PROP_INT(NormalLOD)
		PROP_INT(MinLOD)
		PROP_OBJ(AnimNext)
		PROP_BYTE(PrimeCount)
		PROP_FLOAT(MinFrameRate)
		PROP_FLOAT(MaxFrameRate)
		PROP_ENUM(CompFormat)
		PROP_BOOL(bHasComp)
#if XIII
		PROP_DROP(HitSound)
#endif
#if SPLINTER_CELL
		PROP_DROP(bFullLoaded)
		PROP_DROP(iCachedLODLevel)
		PROP_DROP(TexVerNumber)
#endif
#if BIOSHOCK
		PROP_BOOL(HasBeenStripped)
		PROP_BYTE(StrippedNumMips)
		PROP_BOOL(bBaked)
		PROP_DROP(ResourceCategory)
		PROP_DROP(ConsoleDropMips)
		PROP_DROP(bStreamable)
		PROP_DROP(CachedBulkDataSize)	//?? serialized twice?
		PROP_DROP(BestTextureInstanceWeight)
		PROP_DROP(SourcePath)
		PROP_DROP(Keywords)
		PROP_DROP(LastModifiedTime_LoInt)
		PROP_DROP(LastModifiedTime_HiInt)
		PROP_DROP(LastModifiedByUser)
#endif // BIOSHOCK
	END_PROP_TABLE

#if RENDERING
	virtual bool Upload();
	virtual bool Bind();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void SetupGL();
	virtual void Release();
	virtual bool IsTranslucent() const;
#endif // RENDERING
};


enum EOutputBlending
{
	OB_Normal,
	OB_Masked,
	OB_Modulate,
	OB_Translucent,
	OB_Invisible,
	OB_Brighten,
	OB_Darken
};

_ENUM(EOutputBlending)
{
	_E(OB_Normal),
	_E(OB_Masked),
	_E(OB_Modulate),
	_E(OB_Translucent),
	_E(OB_Invisible),
	_E(OB_Brighten),
	_E(OB_Darken)
};


class UShader : public URenderedMaterial
{
	DECLARE_CLASS(UShader, URenderedMaterial);
public:
	UMaterial		*Diffuse;
#if BIOSHOCK
	UMaterial		*NormalMap;
#endif
	UMaterial		*Opacity;
	UMaterial		*Specular;
	UMaterial		*SpecularityMask;
	UMaterial		*SelfIllumination;
	UMaterial		*SelfIlluminationMask;
	UMaterial		*Detail;
	float			DetailScale;
	EOutputBlending	OutputBlending;
	bool			TwoSided;
	bool			Wireframe;
	bool			ModulateStaticLighting2X;
	bool			PerformLightingOnSpecularPass;
	bool			ModulateSpecular2X;
#if LINEAGE2
	bool			TreatAsTwoSided;			// strange ...
	bool			ZWrite;
	bool			AlphaTest;
	byte			AlphaRef;
#endif // LINEAGE2
#if BIOSHOCK
	FMaskMaterial	Opacity_Bio;
	FMaskMaterial	HeightMap;
	FMaskMaterial	SpecularMask;
	FMaskMaterial	GlossinessMask;
	FMaskMaterial	ReflectionMask;
	FMaskMaterial	EmissiveMask;
	FMaskMaterial	SubsurfaceMask;
	FMaskMaterial	ClipMask;
#endif // BIOSHOCK

	UShader()
	:	ModulateStaticLighting2X(true)
	,	ModulateSpecular2X(false)
	,	DetailScale(8.0f)
	{}

	BEGIN_PROP_TABLE
		PROP_OBJ(Diffuse)
#if BIOSHOCK
		PROP_OBJ(NormalMap)
#endif
		PROP_OBJ(Opacity)
		PROP_OBJ(Specular)
		PROP_OBJ(SpecularityMask)
		PROP_OBJ(SelfIllumination)
		PROP_OBJ(SelfIlluminationMask)
		PROP_OBJ(Detail)
		PROP_FLOAT(DetailScale)
		PROP_ENUM2(OutputBlending, EOutputBlending)
		PROP_BOOL(TwoSided)
		PROP_BOOL(Wireframe)
		PROP_BOOL(ModulateStaticLighting2X)
		PROP_BOOL(PerformLightingOnSpecularPass)
		PROP_BOOL(ModulateSpecular2X)
#if LINEAGE2
		PROP_BOOL(TreatAsTwoSided)
		PROP_BOOL(ZWrite)
		PROP_BOOL(AlphaTest)
		PROP_BYTE(AlphaRef)
#endif // LINEAGE2
#if BIOSHOCK
		PROP_STRUC(Opacity_Bio,    FMaskMaterial)
		PROP_STRUC(HeightMap,      FMaskMaterial)
		PROP_STRUC(SpecularMask,   FMaskMaterial)
		PROP_STRUC(GlossinessMask, FMaskMaterial)
		PROP_STRUC(ReflectionMask, FMaskMaterial)
		PROP_STRUC(EmissiveMask,   FMaskMaterial)
		PROP_STRUC(SubsurfaceMask, FMaskMaterial)
		PROP_STRUC(ClipMask,       FMaskMaterial)

		PROP_DROP(Emissive)

		PROP_DROP(DiffuseColor)
		PROP_DROP(EmissiveBrightness)
		PROP_DROP(EmissiveColor)
		PROP_DROP(Glossiness)
		PROP_DROP(ReflectionBrightness)
		PROP_DROP(SpecularColor)
		PROP_DROP(SpecularBrightness)
		PROP_DROP(SpecularCubeMapBrightness)
		PROP_DROP(SpecularColorMap)
		PROP_DROP(UseSpecularCubemaps)
		PROP_DROP(HeightMap)
		PROP_DROP(HeightMapStrength)
		PROP_DROP(Masked)			//??
		PROP_DROP(MaterialVisualType)
		PROP_DROP(DiffuseTextureAnimator)
		PROP_DROP(DiffuseColorAnimator)
		PROP_DROP(OpacityTextureAnimator)
		PROP_DROP(SelfIllumTextureAnimator)
		PROP_DROP(SelfIllumColorAnimator)
		PROP_DROP(Subsurface)
		PROP_DROP(SubsurfaceColor2x)
		PROP_DROP(DistortionStrength)
		PROP_DROP(ForceTransparentSorting)
		PROP_DROP(MaxAlphaClipValue)
		PROP_DROP(MinAlphaClipValue)
#endif // BIOSHOCK
	END_PROP_TABLE

#if BIOSHOCK
	virtual void Serialize(FArchive &Ar)
	{
		guard(UShader::Serialize);
		Super::Serialize(Ar);
		TRIBES_HDR(Ar, 0x29);
		unguard;
	}
#endif

#if RENDERING
	virtual void SetupGL();
	virtual bool IsTranslucent() const;
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};


class UModifier : public UMaterial
{
	DECLARE_CLASS(UModifier, UMaterial);
public:
	UMaterial		*Material;

	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
	END_PROP_TABLE

#if RENDERING
	virtual void SetupGL();
	virtual bool IsTranslucent() const
	{
		return Material ? Material->IsTranslucent() : false;
	}
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};


//?? NOTE: Bioshock EFrameBufferBlending is used for UShader and UFinalBlend, plus it has different values
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
#if LINEAGE2	//?? not needed
	FB_Add,
	FB_InWaterBlend,
	FB_Capture,
#endif
};

_ENUM(EFrameBufferBlending)
{
	_E(FB_Overwrite),
	_E(FB_Modulate),
	_E(FB_AlphaBlend),
	_E(FB_AlphaModulate_MightNotFogCorrectly),
	_E(FB_Translucent),
	_E(FB_Darken),
	_E(FB_Brighten),
	_E(FB_Invisible),
#if LINEAGE2	//?? not needed
	_E(FB_Add),
	_E(FB_InWaterBlend),
	_E(FB_Capture),
#endif
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
		PROP_ENUM2(FrameBufferBlending, EFrameBufferBlending)
		PROP_BOOL(ZWrite)
		PROP_BOOL(ZTest)
		PROP_BOOL(AlphaTest)
		PROP_BOOL(TwoSided)
		PROP_BYTE(AlphaRef)
#if LINEAGE2
		PROP_BOOL(TreatAsTwoSided)
#endif
	END_PROP_TABLE

#if RENDERING
	virtual void SetupGL();
	virtual bool IsTranslucent() const;
#endif
};


enum EColorOperation
{
	CO_Use_Color_From_Material1,
	CO_Use_Color_From_Material2,
	CO_Multiply,
	CO_Add,
	CO_Subtract,
	CO_AlphaBlend_With_Mask,
	CO_Add_With_Mask_Modulation,
	CO_Use_Color_From_Mask,
};

_ENUM(EColorOperation)
{
	_E(CO_Use_Color_From_Material1),
	_E(CO_Use_Color_From_Material2),
	_E(CO_Multiply),
	_E(CO_Add),
	_E(CO_Subtract),
	_E(CO_AlphaBlend_With_Mask),
	_E(CO_Add_With_Mask_Modulation),
	_E(CO_Use_Color_From_Mask)
};

enum EAlphaOperation
{
	AO_Use_Mask,
	AO_Multiply,
	AO_Add,
	AO_Use_Alpha_From_Material1,
	AO_Use_Alpha_From_Material2,
};

_ENUM(EAlphaOperation)
{
	_E(AO_Use_Mask),
	_E(AO_Multiply),
	_E(AO_Add),
	_E(AO_Use_Alpha_From_Material1),
	_E(AO_Use_Alpha_From_Material2),
};

class UCombiner : public UMaterial
{
	DECLARE_CLASS(UCombiner, UMaterial);
public:
	EColorOperation	CombineOperation;
	EAlphaOperation	AlphaOperation;
	UMaterial		*Material1;
	UMaterial		*Material2;
	UMaterial		*Mask;
	bool			InvertMask;
	bool			Modulate2X;
	bool			Modulate4X;

	UCombiner()
	:	AlphaOperation(AO_Use_Mask)
	{}

	BEGIN_PROP_TABLE
		PROP_ENUM2(CombineOperation, EColorOperation)
		PROP_ENUM2(AlphaOperation, EAlphaOperation)
		PROP_OBJ(Material1)
		PROP_OBJ(Material2)
		PROP_OBJ(Mask)
		PROP_BOOL(InvertMask)
		PROP_BOOL(Modulate2X)
		PROP_BOOL(Modulate4X)
	END_PROP_TABLE

#if RENDERING
	virtual void GetParams(CMaterialParams &Params) const;
	virtual bool IsTranslucent() const
	{
		return false;
	}
#endif
};


enum ETexCoordSrc
{
	TCS_Stream0,
	TCS_Stream1,
	TCS_Stream2,
	TCS_Stream3,
	TCS_Stream4,
	TCS_Stream5,
	TCS_Stream6,
	TCS_Stream7,
	TCS_WorldCoords,
	TCS_CameraCoords,
	TCS_WorldEnvMapCoords,
	TCS_CameraEnvMapCoords,
	TCS_ProjectorCoords,
	TCS_NoChange,				// don't specify a source, just modify it
};

_ENUM(ETexCoordSrc)
{
	_E(TCS_Stream0),
	_E(TCS_Stream1),
	_E(TCS_Stream2),
	_E(TCS_Stream3),
	_E(TCS_Stream4),
	_E(TCS_Stream5),
	_E(TCS_Stream6),
	_E(TCS_Stream7),
	_E(TCS_WorldCoords),
	_E(TCS_CameraCoords),
	_E(TCS_WorldEnvMapCoords),
	_E(TCS_CameraEnvMapCoords),
	_E(TCS_ProjectorCoords),
	_E(TCS_NoChange),
};

enum ETexCoordCount
{
	TCN_2DCoords,
	TCN_3DCoords,
	TCN_4DCoords
};

_ENUM(ETexCoordCount)
{
	_E(TCN_2DCoords),
	_E(TCN_3DCoords),
	_E(TCN_4DCoords)
};

class UTexModifier : public UModifier
{
	DECLARE_CLASS(UTexModifier, UModifier);
public:
	ETexCoordSrc	TexCoordSource;
	ETexCoordCount	TexCoordCount;
	bool			TexCoordProjected;

	UTexModifier()
	:	TexCoordSource(TCS_NoChange)
	,	TexCoordCount(TCN_2DCoords)
	{}

	BEGIN_PROP_TABLE
		PROP_ENUM2(TexCoordSource, ETexCoordSrc)
		PROP_ENUM2(TexCoordCount, ETexCoordCount)
		PROP_BOOL(TexCoordProjected)
	END_PROP_TABLE
};


enum ETexEnvMapType
{
	EM_WorldSpace,
	EM_CameraSpace,
};

_ENUM(ETexEnvMapType)
{
	_E(EM_WorldSpace),
	_E(EM_CameraSpace),
};

class UTexEnvMap : public UTexModifier
{
	DECLARE_CLASS(UTexEnvMap, UTexModifier)
public:
	ETexEnvMapType	EnvMapType;

	UTexEnvMap()
	:	EnvMapType(EM_CameraSpace)
	{
		TexCoordCount = TCN_3DCoords;
	}

	BEGIN_PROP_TABLE
		PROP_ENUM2(EnvMapType, ETexEnvMapType)
	END_PROP_TABLE
};


enum ETexOscillationType
{
	OT_Pan,
	OT_Stretch,
	OT_StretchRepeat,
	OT_Jitter,
};

_ENUM(ETexOscillationType)
{
	_E(OT_Pan),
	_E(OT_Stretch),
	_E(OT_StretchRepeat),
	_E(OT_Jitter),
};

class UTexOscillator : public UTexModifier
{
	DECLARE_CLASS(UTexOscillator, UTexModifier);
public:
	float			UOscillationRate;
	float			VOscillationRate;
	float			UOscillationPhase;
	float			VOscillationPhase;
	float			UOscillationAmplitude;
	float			VOscillationAmplitude;
	ETexOscillationType UOscillationType;
	ETexOscillationType VOscillationType;
	float			UOffset;
	float			VOffset;
	// transient variables
//	FMatrix			M;
//	float			CurrentUJitter;
//	float			CurrentVJitter;

	BEGIN_PROP_TABLE
		PROP_FLOAT(UOscillationRate)
		PROP_FLOAT(VOscillationRate)
		PROP_FLOAT(UOscillationPhase)
		PROP_FLOAT(VOscillationPhase)
		PROP_FLOAT(UOscillationAmplitude)
		PROP_FLOAT(VOscillationAmplitude)
		PROP_ENUM2(UOscillationType, ETexOscillationType)
		PROP_ENUM2(VOscillationType, ETexOscillationType)
		PROP_FLOAT(UOffset)
		PROP_FLOAT(VOffset)
		PROP_DROP(M)
		PROP_DROP(CurrentUJitter)
		PROP_DROP(CurrentVJitter)
	END_PROP_TABLE

	UTexOscillator()
	:	UOscillationRate(1)
	,	VOscillationRate(1)
	,	UOscillationAmplitude(0.1f)
	,	VOscillationAmplitude(0.1f)
	,	UOscillationType(OT_Pan)
	,	VOscillationType(OT_Pan)
	{}
};


class UTexPanner : public UTexModifier
{
	DECLARE_CLASS(UTexPanner, UTexModifier);
public:
	FRotator		PanDirection;
	float			PanRate;
//	FMatrix			M;

	BEGIN_PROP_TABLE
		PROP_ROTATOR(PanDirection)
		PROP_FLOAT(PanRate)
		PROP_DROP(M)
	END_PROP_TABLE

	UTexPanner()
	:	PanRate(0.1f)
	{}
};


enum ETexRotationType
{
	TR_FixedRotation,
	TR_ConstantlyRotating,
	TR_OscillatingRotation,
};

_ENUM(ETexRotationType)
{
	_E(TR_FixedRotation),
	_E(TR_ConstantlyRotating),
	_E(TR_OscillatingRotation),
};

class UTexRotator : public UTexModifier
{
	DECLARE_CLASS(UTexRotator, UTexModifier);
public:
	ETexRotationType TexRotationType;
	FRotator		Rotation;
	float			UOffset;
	float			VOffset;
	FRotator		OscillationRate;
	FRotator		OscillationAmplitude;
	FRotator		OscillationPhase;
//	FMatrix			M;

	BEGIN_PROP_TABLE
		PROP_ENUM2(TexRotationType, ETexRotationType)
		PROP_ROTATOR(Rotation)
		PROP_FLOAT(UOffset)
		PROP_FLOAT(VOffset)
		PROP_ROTATOR(OscillationRate)
		PROP_ROTATOR(OscillationAmplitude)
		PROP_ROTATOR(OscillationPhase)
		PROP_DROP(M)
	END_PROP_TABLE

	UTexRotator()
	:	TexRotationType(TR_FixedRotation)
	{}
};


class UTexScaler : public UTexModifier
{
	DECLARE_CLASS(UTexScaler, UTexModifier)
public:
	float			UScale;
	float			VScale;
	float			UOffset;
	float			VOffset;
//	FMatrix			M;

	BEGIN_PROP_TABLE
		PROP_FLOAT(UScale)
		PROP_FLOAT(VScale)
		PROP_FLOAT(UOffset)
		PROP_FLOAT(VOffset)
		PROP_DROP(M)
	END_PROP_TABLE

	UTexScaler()
	:	UScale(1.0f)
	,	VScale(1.0f)
	{}
};


#if BIOSHOCK

class UFacingShader : public URenderedMaterial
{
	DECLARE_CLASS(UFacingShader, URenderedMaterial);
public:
	UMaterial		*EdgeDiffuse;
	UMaterial		*FacingDiffuse;
	FMaskMaterial	EdgeOpacity;
	FMaskMaterial	FacingOpacity;
	float			EdgeOpacityScale;
	float			FacingOpacityScale;
	UMaterial		*NormalMap;
	FColor			EdgeDiffuseColor;
	FColor			FacingDiffuseColor;
	FColor			EdgeSpecularColor;
	FColor			FacingSpecularColor;
	FMaskMaterial	FacingSpecularMask;
	UMaterial		*FacingSpecularColorMap;
	FMaskMaterial	FacingGlossinessMask;
	FMaskMaterial	EdgeSpecularMask;
	UMaterial		*EdgeSpecularColorMap;
	FMaskMaterial	EdgeGlossinessMask;
	UMaterial		*EdgeEmissive;
	UMaterial		*FacingEmissive;
	FMaskMaterial	EdgeEmissiveMask;
	FMaskMaterial	FacingEmissiveMask;
	float			EdgeEmissiveBrightness;
	float			FacingEmissiveBrightness;
	FColor			EdgeEmissiveColor;
	FColor			FacingEmissiveColor;
	EFrameBufferBlending OutputBlending;
	bool			Masked;
	bool			TwoSided;
	FMaskMaterial	SubsurfaceMask;
	FMaskMaterial	NoiseMask;

	UFacingShader()
	//?? unknown default properties
	{}

	BEGIN_PROP_TABLE
		PROP_OBJ(EdgeDiffuse)
		PROP_OBJ(FacingDiffuse)
		PROP_STRUC(EdgeOpacity, FMaskMaterial)
		PROP_STRUC(FacingOpacity, FMaskMaterial)
		PROP_FLOAT(EdgeOpacityScale)
		PROP_FLOAT(FacingOpacityScale)
		PROP_OBJ(NormalMap)
		PROP_COLOR(EdgeDiffuseColor)
		PROP_COLOR(FacingDiffuseColor)
		PROP_COLOR(EdgeSpecularColor)
		PROP_COLOR(FacingSpecularColor)
		PROP_STRUC(FacingSpecularMask, FMaskMaterial)
		PROP_OBJ(FacingSpecularColorMap)
		PROP_STRUC(FacingGlossinessMask, FMaskMaterial)
		PROP_STRUC(EdgeSpecularMask, FMaskMaterial)
		PROP_OBJ(EdgeSpecularColorMap)
		PROP_STRUC(EdgeGlossinessMask, FMaskMaterial)
		PROP_OBJ(EdgeEmissive)
		PROP_OBJ(FacingEmissive)
		PROP_STRUC(EdgeEmissiveMask, FMaskMaterial)
		PROP_STRUC(FacingEmissiveMask, FMaskMaterial)
		PROP_FLOAT(EdgeEmissiveBrightness)
		PROP_FLOAT(FacingEmissiveBrightness)
		PROP_COLOR(EdgeEmissiveColor)
		PROP_COLOR(FacingEmissiveColor)
		PROP_ENUM2(OutputBlending, EFrameBufferBlending)
		PROP_BOOL(Masked)
		PROP_BOOL(TwoSided)
		PROP_STRUC(SubsurfaceMask, FMaskMaterial)
		PROP_STRUC(NoiseMask, FMaskMaterial)
		PROP_DROP(EdgeDiffuseTextureAnimator)
		PROP_DROP(FacingDiffuseTextureAnimator)
		PROP_DROP(EdgeDiffuseColorAnimator)
		PROP_DROP(FacingDiffuseColorAnimator)
		PROP_DROP(EdgeOpacityTextureAnimator)
		PROP_DROP(FacingOpacityTextureAnimator)
		PROP_DROP(BlendTextureAnimator)
		PROP_DROP(NormalTextureAnimator)
		PROP_DROP(EdgeSelfIllumColorAnimator)
		PROP_DROP(FacingSelfIllumColorAnimator)
		PROP_DROP(FacingSpecularAnimator)
		PROP_DROP(EdgeSpecularAnimator)
		PROP_DROP(EdgeGlossiness)
		PROP_DROP(FacingGlossiness)
		PROP_DROP(EdgeSpecularBrightness)
		PROP_DROP(FacingSpecularBrightness)
		PROP_DROP(Subsurface)
		PROP_DROP(SubsurfaceColor2x)
		PROP_DROP(ForceTransparentSorting)
		PROP_DROP(Hardness)
	END_PROP_TABLE

#if RENDERING
	virtual void SetupGL();
	virtual bool IsTranslucent() const;
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};

#endif // BIOSHOCK


#if SPLINTER_CELL

enum U3BlendingMode
{
	U3BM_OPAQUE,
	U3BM_TRANSLUCENT,
	U3BM_TRANSLUCENT_NO_DISTORTION,
	U3BM_ADDITIVE,
	U3BM_MASKED
};

class UUnreal3Material : public URenderedMaterial
{
	DECLARE_CLASS(UUnreal3Material, URenderedMaterial);
public:
	union
	{
		// used in in property serialization
		struct
		{
			UTexture	*Texture0, *Texture1, *Texture2, *Texture3, *Texture4, *Texture5, *Texture6, *Texture7;
			UTexture	*Texture8, *Texture9, *Texture10, *Texture11, *Texture12, *Texture13, *Texture14, *Texture15;
		};
		// used in our framework
		UTexture	*Textures[16];
	};
	U3BlendingMode	BlendingMode;
	bool			bDoubleSided;
	bool			bHasEmissive;
	bool			bForceNoZWrite;

	BEGIN_PROP_TABLE
		PROP_ENUM2(BlendingMode, U3BlendingMode)
		PROP_BOOL(bDoubleSided)
		PROP_BOOL(bHasEmissive)
		PROP_BOOL(bForceNoZWrite)
		PROP_OBJ(Texture0) PROP_OBJ(Texture1) PROP_OBJ(Texture2) PROP_OBJ(Texture3)
		PROP_OBJ(Texture4) PROP_OBJ(Texture5) PROP_OBJ(Texture6) PROP_OBJ(Texture7)
		PROP_OBJ(Texture8) PROP_OBJ(Texture9) PROP_OBJ(Texture10) PROP_OBJ(Texture11)
		PROP_OBJ(Texture12) PROP_OBJ(Texture13) PROP_OBJ(Texture14) PROP_OBJ(Texture15)
	END_PROP_TABLE

#if RENDERING
	virtual void SetupGL();
	virtual bool IsTranslucent() const;
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};


enum SpecSrc
{
	SpecSrc_SRed,
	SpecSrc_SGreen,
	SpecSrc_SBlue,
	SpecSrc_NAlpha,
};

class USCX_basic_material : public URenderedMaterial
{
	DECLARE_CLASS(USCX_basic_material, URenderedMaterial);
public:
	UTexture		*Base;
	UTexture		*Normal;
	UTexture		*SpecularMask;
	UTexture		*Environment;		//!! UCubemap
	UTexture		*HeightMap;
	UTexture		*HDRMask;
	SpecSrc			SpecularSource;

	BEGIN_PROP_TABLE
		PROP_OBJ(Base)
		PROP_OBJ(Normal)
		PROP_OBJ(SpecularMask)
		PROP_OBJ(Environment)
		PROP_OBJ(HeightMap)
		PROP_OBJ(HDRMask)
		PROP_ENUM2(SpecularSource, SpecSrc)
	END_PROP_TABLE

#if RENDERING
	virtual void SetupGL();
	virtual bool IsTranslucent() const;
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};

#endif // SPLINTER_CELL


#define REGISTER_MATERIAL_CLASSES		\
	REGISTER_CLASS(UConstantColor)		\
	REGISTER_CLASS(UBitmapMaterial)		\
	REGISTER_CLASS(UPalette)			\
	REGISTER_CLASS(UShader)				\
	REGISTER_CLASS(UCombiner)			\
	REGISTER_CLASS(UTexture)			\
	REGISTER_CLASS(UFinalBlend)			\
	REGISTER_CLASS(UTexEnvMap)			\
	REGISTER_CLASS(UTexOscillator)		\
	REGISTER_CLASS(UTexPanner)			\
	REGISTER_CLASS(UTexRotator)			\
	REGISTER_CLASS(UTexScaler)

#define REGISTER_MATERIAL_CLASSES_BIO	\
	REGISTER_CLASS(FMaskMaterial)		\
	REGISTER_CLASS(UFacingShader)

#define REGISTER_MATERIAL_CLASSES_SCELL	\
	REGISTER_CLASS(UUnreal3Material)	\
	REGISTER_CLASS(USCX_basic_material)

#define REGISTER_MATERIAL_ENUMS			\
	REGISTER_ENUM(ETextureFormat)		\
	REGISTER_ENUM(ETexClampMode)		\
	REGISTER_ENUM(EOutputBlending)		\
	REGISTER_ENUM(EFrameBufferBlending)	\
	REGISTER_ENUM(EColorOperation)		\
	REGISTER_ENUM(EAlphaOperation)		\
	REGISTER_ENUM(ETexCoordSrc)			\
	REGISTER_ENUM(ETexCoordCount)		\
	REGISTER_ENUM(ETexEnvMapType)		\
	REGISTER_ENUM(ETexOscillationType)	\
	REGISTER_ENUM(ETexRotationType)


#endif // __UNMATERIAL2_H__
