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
*/

#if RENDERING
#	define BIND		virtual void Bind(unsigned PolyFlags)
#else
#	define BIND
#endif


#if LINEAGE2

struct FLineageMaterialStageProperty
{
	FString			unk1;
	TArray<FString>	unk2;

	friend FArchive& operator<<(FArchive &Ar, FLineageMaterialStageProperty &P)
	{
		Ar << P.unk1 << P.unk2;
	}
};

struct FLineageShaderProperty
{
	// possibly, MaterialInfo, TextureTranform, TwoPassRenderState, AlphaRef
	byte			b1[4];
	// possibly, SrcBlend, DestBlend, OverriddenFogColor
	int				i1[3];
	// nested structure
	// possibly, int FC_Color1, FC_Color2 (strange byte order)
	byte			b2[8];
	// possibly, float FC_FadePeriod, FC_FadePhase, FC_ColorFadeType
	int				i2[3];
	// stages
	TArray<FLineageMaterialStageProperty> Stages;

	friend FArchive& operator<<(FArchive &Ar, FLineageShaderProperty &P)
	{
		int i;
		for (i = 0; i < 4; i++) Ar << P.b1[i];
		for (i = 0; i < 3; i++) Ar << P.i1[i];
		for (i = 0; i < 8; i++) Ar << P.b2[i];
		for (i = 0; i < 3; i++) Ar << P.i2[i];
		Ar << P.Stages;
		return Ar;
	}
};

#endif // LINEAGE2


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
		PROP_ENUM(SurfaceType)
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
			guard(SerializeLineage2Material);
			//?? separate to cpp
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x10 && Ar.ArLicenseeVer < 0x25)
			{
				int unk1;
				Ar << unk1;					// simply drop obsolete variable (int Reserved ?)
			}
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x1E && Ar.ArLicenseeVer < 0x25)
			{
				int i;
				// some function
				byte MaterialInfo, TextureTranform, MAX_SAMPLER_NUM, MAX_TEXMAT_NUM, MAX_PASS_NUM, TwoPassRenderState, AlphaRef;
				if (Ar.ArLicenseeVer >= 0x21 && Ar.ArLicenseeVer < 0x24)
					Ar << MaterialInfo;
				Ar << TextureTranform << MAX_SAMPLER_NUM << MAX_TEXMAT_NUM << MAX_PASS_NUM << TwoPassRenderState << AlphaRef;
				int SrcBlend, DestBlend, OverriddenFogColor;
				Ar << SrcBlend << DestBlend << OverriddenFogColor;
				// serialize matTexMatrix[16] (strange code)
				for (i = 0; i < 8; i++)
				{
					char b1, b2;
					Ar << b1;
					if (Ar.ArLicenseeVer < 0x24) Ar << b2;
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
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x25)
			{
				// ShaderProperty + ShaderCode
				FLineageShaderProperty ShaderProp;
				FString ShaderCode;
				Ar << ShaderProp << ShaderCode;
			}
			if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x1F)
			{
				word ver1, ver2;			// 'int MaterialCodeVersion' serialized as 2 words
				Ar << ver1 << ver2;
			}
			unguard;
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
		PROP_ENUM(Format)
		PROP_ENUM(UClampMode)
		PROP_ENUM(VClampMode)
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
		// Note: UT2 does not use paletted textures, but HP3 does ...
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
	byte *Decompress(int &USize, int &VSize) const;
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
#if EXTEEL
		if (Ar.IsExteel)
		{
			byte MaterialType;			// enum GFMaterialType
			Ar << MaterialType;
		}
#endif
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
		PROP_ENUM(LODSet)
		PROP_INT(NormalLOD)
		PROP_INT(MinLOD)
		PROP_OBJ(AnimNext)
		PROP_BYTE(PrimeCount)
		PROP_FLOAT(MinFrameRate)
		PROP_FLOAT(MaxFrameRate)
		PROP_ENUM(CompFormat)
		PROP_BOOL(bHasComp)
	END_PROP_TABLE

	BIND;
};


enum EOutputBlending
{
	OB_Normal,
	OB_Masked,
	OB_Modulate,
	OB_Translucent,
	OB_Invisible,
	OB_Brighten,
	OB_Darken,
};


class UShader : public URenderedMaterial
{
	DECLARE_CLASS(UShader, URenderedMaterial);
public:
	UMaterial		*Diffuse;
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
#endif

	UShader()
	:	ModulateStaticLighting2X(true)
	,	ModulateSpecular2X(false)
	,	DetailScale(8.0f)
	{}

	BEGIN_PROP_TABLE
		PROP_OBJ(Diffuse)
		PROP_OBJ(Opacity)
		PROP_OBJ(Specular)
		PROP_OBJ(SpecularityMask)
		PROP_OBJ(SelfIllumination)
		PROP_OBJ(SelfIlluminationMask)
		PROP_OBJ(Detail)
		PROP_FLOAT(DetailScale)
		PROP_ENUM(OutputBlending)
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
#endif
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
#if LINEAGE2			// possibly, new UE2 blends
	FB_Add,
	FB_InWaterBlend,
	FB_Capture,
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
		PROP_ENUM(FrameBufferBlending)
		PROP_BOOL(ZWrite)
		PROP_BOOL(ZTest)
		PROP_BOOL(AlphaTest)
		PROP_BOOL(TwoSided)
		PROP_BYTE(AlphaRef)
#if LINEAGE2
		PROP_BOOL(TreatAsTwoSided)
#endif
	END_PROP_TABLE

	BIND;
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

enum EAlphaOperation
{
	AO_Use_Mask,
	AO_Multiply,
	AO_Add,
	AO_Use_Alpha_From_Material1,
	AO_Use_Alpha_From_Material2,
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
		PROP_ENUM(CombineOperation)
		PROP_ENUM(AlphaOperation)
		PROP_OBJ(Material1)
		PROP_OBJ(Material2)
		PROP_OBJ(Mask)
		PROP_BOOL(InvertMask)
		PROP_BOOL(Modulate2X)
		PROP_BOOL(Modulate4X)
	END_PROP_TABLE

	BIND;
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

enum ETexCoordCount
{
	TCN_2DCoords,
	TCN_3DCoords,
	TCN_4DCoords
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
		PROP_ENUM(TexCoordSource)
		PROP_ENUM(TexCoordCount)
		PROP_ENUM(TexCoordProjected)
	END_PROP_TABLE

	BIND;
};


enum ETexEnvMapType
{
	EM_WorldSpace,
	EM_CameraSpace,
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
		PROP_ENUM(EnvMapType)
	END_PROP_TABLE
};


enum ETexOscillationType
{
	OT_Pan,
	OT_Stretch,
	OT_StretchRepeat,
	OT_Jitter,
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
		PROP_ENUM(UOscillationType)
		PROP_ENUM(VOscillationType)
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
		PROP_ENUM(TexRotationType)
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


#endif // __UNMATERIAL_H__
