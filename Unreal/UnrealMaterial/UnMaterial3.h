#ifndef __UNMATERIAL3_H__
#define __UNMATERIAL3_H__


/*
UE3 MATERIALS TREE:
~~~~~~~~~~~~~~~~~~~
	Surface (empty)
		Texture
			Texture2D
				ShadowMapTexture2D
				TerrainWeightMapTexture
				TextureFlipBook
			Texture2DComposite
			Texture3D
			TextureCube
			TextureMovie
			TextureRenderTarget
				TextureRenderTarget2D
				TextureRenderTargetCube
		MaterialInterface (empty)
			Material
				MaterialExpression
					MaterialExpressionTextureSample
					...
			MaterialInstance
				MaterialInstanceConstant
				MaterialInstanceTimeVarying
*/


#if UNREAL3

/*-----------------------------------------------------------------------------
	Unreal Engine 3 materials
-----------------------------------------------------------------------------*/

#if UNREAL4

enum ETextureSourceFormat
{
	TSF_Invalid,
	TSF_G8,
	TSF_BGRA8,
	TSF_BGRE8,
	TSF_RGBA16,
	TSF_RGBA16F,

	// Deprecated formats:
	TSF_RGBA8,
	TSF_RGBE8,
};

_ENUM(ETextureSourceFormat)
{
	_E(TSF_Invalid),
	_E(TSF_G8),
	_E(TSF_BGRA8),
	_E(TSF_BGRE8),
	_E(TSF_RGBA16),
	_E(TSF_RGBA16F),
	_E(TSF_RGBA8),
	_E(TSF_RGBE8),
};

enum ETextureCompressionSettings
{
	TC_Default,
	TC_Normalmap,
	TC_Masks,					// UE4
	TC_Grayscale,
	TC_Displacementmap,
	TC_VectorDisplacementmap,
	TC_HDR,						// UE4
	TC_HighDynamicRange = TC_HDR,
	TC_EditorIcon,				// UE4
	TC_Alpha,					// UE4
	TC_DistanceFieldFont,		// UE4
	TC_HDR_Compressed,			// UE4
	TC_BC7,						// UE4

	// The following enum values are for UE3
	TC_NormalmapAlpha,
	TC_OneBitAlpha,
	TC_NormalmapUncompressed,
	TC_NormalmapBC5,
	TC_OneBitMonochrome,
	TC_SimpleLightmapModification,

#if MASSEFF
	TC_NormalmapHQ,
	TC_NormalmapLQ,
	TC_NormalmapBC7,			// ME LE
#endif
#if DAYSGONE
	TC_BendDefault,
	TC_BendNormalmap,
#endif
};

_ENUM(ETextureCompressionSettings)
{
	_E(TC_Default),
	_E(TC_Normalmap),
	_E(TC_Masks),
	_E(TC_Grayscale),
	_E(TC_Displacementmap),
	_E(TC_VectorDisplacementmap),
	_E(TC_HDR),
	_E(TC_HighDynamicRange),
	_E(TC_EditorIcon),
	_E(TC_Alpha),
	_E(TC_DistanceFieldFont),
	_E(TC_HDR_Compressed),
	_E(TC_BC7),
	_E(TC_NormalmapAlpha),
	_E(TC_OneBitAlpha),
	_E(TC_NormalmapUncompressed),
	_E(TC_NormalmapBC5),
	_E(TC_OneBitMonochrome),
	_E(TC_SimpleLightmapModification),

#if MASSEFF
	_E(TC_NormalmapHQ),
	_E(TC_NormalmapLQ),
	_E(TC_NormalmapBC7),
#endif
#if DAYSGONE
	_E(TC_BendDefault),
	_E(TC_BendNormalmap),
#endif
};

struct FTextureSource
{
	DECLARE_STRUCT(FTextureSource)

	int				SizeX;
	int				SizeY;
	int				NumSlices;
	int				NumMips;
	bool			bPNGCompressed;
	ETextureSourceFormat Format;

	BEGIN_PROP_TABLE
		PROP_INT(SizeX)
		PROP_INT(SizeY)
		PROP_INT(NumSlices)
		PROP_INT(NumMips)
		PROP_BOOL(bPNGCompressed)
		PROP_ENUM2(Format, ETextureSourceFormat)
		PROP_DROP(Id)		// Guid
	END_PROP_TABLE
};

#endif // UNREAL4

class UTexture3 : public UUnrealMaterial	// in UE3 it is derived from USurface->UObject; real name is UTexture
{
	DECLARE_CLASS(UTexture3, UUnrealMaterial)
public:
	float			UnpackMin[4];
	float			UnpackMax[4];
	FByteBulkData	SourceArt;
#if UNREAL4
	FTextureSource	Source;
#endif
	ETextureCompressionSettings CompressionSettings;

	UTexture3()
	{
		UnpackMax[0] = UnpackMax[1] = UnpackMax[2] = UnpackMax[3] = 1.0f;
		CompressionSettings = TC_Default;
	}

	BEGIN_PROP_TABLE
		PROP_FLOAT(UnpackMin)
		PROP_FLOAT(UnpackMax)
		PROP_ENUM2(CompressionSettings, ETextureCompressionSettings)
#if UNREAL4
		PROP_STRUC(Source, FTextureSource)
		PROP_DROP(AssetImportData)
#endif
		// no properties required (all are for importing and cooking)
		PROP_DROP(SRGB)
		PROP_DROP(RGBE)
		PROP_DROP(CompressionNoAlpha)
		PROP_DROP(CompressionNone)
		PROP_DROP(CompressionNoMipmaps)
		PROP_DROP(CompressionFullDynamicRange)
		PROP_DROP(DeferCompression)
		PROP_DROP(NeverStream)
		PROP_DROP(bDitherMipMapAlpha)
		PROP_DROP(bPreserveBorderR)
		PROP_DROP(bPreserveBorderG)
		PROP_DROP(bPreserveBorderB)
		PROP_DROP(bPreserveBorderA)
		PROP_DROP(Filter)
		PROP_DROP(LODGroup)
		PROP_DROP(LODBias)
		PROP_DROP(SourceFilePath)
		PROP_DROP(SourceFileTimestamp)
		PROP_DROP(LightingGuid)
		PROP_DROP(AdjustRGBCurve)			// UDK
		PROP_DROP(AdjustSaturation)			// UDK
		PROP_DROP(AdjustBrightnessCurve)	// UDK
		PROP_DROP(MipGenSettings)
#if HUXLEY
		PROP_DROP(SourceArtWidth)
		PROP_DROP(SourceArtHeight)
#endif // HUXLEY
#if A51
		PROP_DROP(LODBiasWindows)
#endif // A51
#if BATMAN
		PROP_DROP(ForceOldCompression)
#endif // BATMAN
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar);
#if UNREAL4
	void Serialize4(FArchive& Ar);
#endif

#if RENDERING
	virtual bool IsTexture() const
	{
		return true;
	}
#endif // RENDERING
};

// Note: real enumeration values are not important for UE3/UE4 because these values are serialized
// as FName. By the way, it's better to keep UE3 values in place, because some games could have
// custom property serializers (for example, Batman2), which serializes enumaration values as integer.
enum EPixelFormat
{
	PF_Unknown,
	PF_A32B32G32R32F,
	PF_A8R8G8B8,			// exists in UE4, but marked as not used
	PF_G8,
	PF_G16,
	PF_DXT1,
	PF_DXT3,
	PF_DXT5,
	PF_UYVY,
	PF_FloatRGB,			// A RGB FP format with platform-specific implementation, for use with render targets
	PF_FloatRGBA,			// A RGBA FP format with platform-specific implementation, for use with render targets
	PF_DepthStencil,		// A depth+stencil format with platform-specific implementation, for use with render targets
	PF_ShadowDepth,			// A depth format with platform-specific implementation, for use with render targets
	PF_FilteredShadowDepth, // not in UE4
	PF_R32F,				// UE3
	PF_G16R16,
	PF_G16R16F,
	PF_G16R16F_FILTER,
	PF_G32R32F,
	PF_A2B10G10R10,
	PF_A16B16G16R16,
	PF_D24,
	PF_R16F,
	PF_R16F_FILTER,
	PF_BC5,
	PF_V8U8,
	PF_A1,
	PF_FloatR11G11B10,
	PF_A4R4G4B4,			// not in UE4
#if UNREAL4
	PF_B8G8R8A8,			// new name for PF_A8R8G8B8
	PF_R32FLOAT,			// == PF_R32F in UE4
	PF_A8,
	PF_R32_UINT,
	PF_R32_SINT,
	PF_PVRTC2,
	PF_PVRTC4,
	PF_R16_UINT,
	PF_R16_SINT,
	PF_R16G16B16A16_UINT,
	PF_R16G16B16A16_SINT,
	PF_R5G6B5_UNORM,
	PF_R8G8B8A8,
//	PF_A8R8G8B8,
	PF_BC4,
	PF_R8G8,
	PF_ATC_RGB,
	PF_ATC_RGBA_E,
	PF_ATC_RGBA_I,
	PF_X24_G8,
	PF_ETC1,
	PF_ETC2_RGB,
	PF_ETC2_RGBA,
	PF_R32G32B32A32_UINT,
	PF_R16G16_UINT,
	PF_ASTC_4x4,
	PF_ASTC_6x6,
	PF_ASTC_8x8,
	PF_ASTC_10x10,
	PF_ASTC_12x12,
	PF_BC6H,
	PF_BC7,
#endif // UNREAL4
#if GEARSU
	PF_DXN,
#endif
#if MASSEFF
	PF_NormalMap_LQ,
	PF_NormalMap_HQ,
#endif
};

_ENUM(EPixelFormat)
{
	_E(PF_Unknown),
	_E(PF_A32B32G32R32F),
	_E(PF_A8R8G8B8),
	_E(PF_G8),
	_E(PF_G16),
	_E(PF_DXT1),
	_E(PF_DXT3),
	_E(PF_DXT5),
	_E(PF_UYVY),
	_E(PF_FloatRGB),
	_E(PF_FloatRGBA),
	_E(PF_DepthStencil),
	_E(PF_ShadowDepth),
	_E(PF_FilteredShadowDepth),
	_E(PF_R32F),
	_E(PF_G16R16),
	_E(PF_G16R16F),
	_E(PF_G16R16F_FILTER),
	_E(PF_G32R32F),
	_E(PF_A2B10G10R10),
	_E(PF_A16B16G16R16),
	_E(PF_D24),
	_E(PF_R16F),
	_E(PF_R16F_FILTER),
	_E(PF_BC5),
	_E(PF_V8U8),
	_E(PF_A1),
	_E(PF_FloatR11G11B10),
	_E(PF_A4R4G4B4),
#if UNREAL4
	_E(PF_B8G8R8A8),
	_E(PF_R32FLOAT),
	_E(PF_A8),
	_E(PF_R32_UINT),
	_E(PF_R32_SINT),
	_E(PF_PVRTC2),
	_E(PF_PVRTC4),
	_E(PF_R16_UINT),
	_E(PF_R16_SINT),
	_E(PF_R16G16B16A16_UINT),
	_E(PF_R16G16B16A16_SINT),
	_E(PF_R5G6B5_UNORM),
	_E(PF_R8G8B8A8),
//	_E(PF_A8R8G8B8),
	_E(PF_BC4),
	_E(PF_R8G8),
	_E(PF_ATC_RGB),
	_E(PF_ATC_RGBA_E),
	_E(PF_ATC_RGBA_I),
	_E(PF_X24_G8),
	_E(PF_ETC1),
	_E(PF_ETC2_RGB),
	_E(PF_ETC2_RGBA),
	_E(PF_R32G32B32A32_UINT),
	_E(PF_R16G16_UINT),
	_E(PF_ASTC_4x4),
	_E(PF_ASTC_6x6),
	_E(PF_ASTC_8x8),
	_E(PF_ASTC_10x10),
	_E(PF_ASTC_12x12),
	_E(PF_BC6H),
	_E(PF_BC7),
#endif // UNREAL4
#if GEARSU
	_E(PF_DXN),
#endif
#if MASSEFF
	_E(PF_NormalMap_LQ),
	_E(PF_NormalMap_HQ),
#endif
};

enum ETextureFilter
{
	TF_Nearest,
	TF_Linear
};

enum ETextureAddress
{
	TA_Wrap,
	TA_Clamp,
	TA_Mirror
};

_ENUM(ETextureAddress)
{
	_E(TA_Wrap),
	_E(TA_Clamp),
	_E(TA_Mirror)
};

struct FTexture2DMipMap
{
	FByteBulkData	Data;	// FTextureMipBulkData
	int32			SizeX;
	int32			SizeY;

#if UNREAL3
	static void Serialize3(FArchive& Ar, FTexture2DMipMap& Mip);
#endif
#if UNREAL4
	static void Serialize4(FArchive& Ar, FTexture2DMipMap& Mip);
#endif
};

class UTexture2D : public UTexture3
{
	DECLARE_CLASS(UTexture2D, UTexture3)
public:
	TArray<FTexture2DMipMap> Mips;
	TArray<FTexture2DMipMap> CachedPVRTCMips;
	TArray<FTexture2DMipMap> CachedATITCMips;
	TArray<FTexture2DMipMap> CachedETCMips;
	int32			SizeX;
	int32			SizeY;
	EPixelFormat	Format;
	ETextureAddress	AddressX;
	ETextureAddress	AddressY;
	FName			TextureFileCacheName;
	FGuid			TextureFileCacheGuid;
	int32			MipTailBaseIdx;
	bool			bForcePVRTC4;		// iPhone
#if UNREAL4
	FIntPoint		ImportedSize;
	int				NumSlices;			//todo: important only while UTextureCube4 is derived from UTexture2D in out implementation
#endif

#if RENDERING
	// rendering implementation fields
	GLuint			TexNum;
#endif

#if RENDERING
	UTexture2D()
	:	TexNum(0)
	,	Format(PF_Unknown)
	,	AddressX(TA_Wrap)
	,	AddressY(TA_Wrap)
	,	bForcePVRTC4(false)
	,	NumSlices(1)
	{}
#endif

	BEGIN_PROP_TABLE
		PROP_INT(SizeX)
		PROP_INT(SizeY)
		PROP_ENUM2(Format, EPixelFormat)
		PROP_ENUM2(AddressX, ETextureAddress)
		PROP_ENUM2(AddressY, ETextureAddress)
		PROP_NAME(TextureFileCacheName)
		PROP_INT(MipTailBaseIdx)
#if UNREAL4
		PROP_STRUC(ImportedSize, FIntPoint)
#endif
		// drop unneeded props
		PROP_DROP(bGlobalForceMipLevelsToBeResident)
		PROP_DROP(OriginalSizeX)
		PROP_DROP(OriginalSizeY)
		PROP_BOOL(bForcePVRTC4)
		PROP_DROP(FirstResourceMemMip)
		PROP_DROP(SystemMemoryData)
#if FRONTLINES
		PROP_DROP(NumMips)
		PROP_DROP(SourceDataSizeX)
		PROP_DROP(SourceDataSizeY)
#endif // FRONTLINES
#if MASSEFF
		PROP_DROP(TFCFileGuid)
#endif
#if BATMAN
		// Batman 3
		PROP_DROP(MipTailInfo)
#endif
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar);
#if UNREAL4
	void Serialize4(FArchive& Ar);
#endif

	virtual void GetMetadata(FArchive& Ar) const;

	const TArray<FTexture2DMipMap>* GetMipmapArray() const;

	bool LoadBulkTexture(const TArray<FTexture2DMipMap> &MipsArray, int MipIndex, const char* tfcSuffix, bool verbose) const;
	virtual ETexturePixelFormat GetTexturePixelFormat() const;
	virtual bool GetTextureData(CTextureData &TexData) const;
	virtual void ReleaseTextureData() const;
#if RENDERING
	virtual void SetupGL();
	virtual bool Upload();
	virtual bool Bind();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void Release();
#endif
};


class ULightMapTexture2D : public UTexture2D
{
	DECLARE_CLASS(ULightMapTexture2D, UTexture2D)
public:
	virtual void Serialize(FArchive &Ar)
	{
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
	}
};


class UTextureCube3 : public UTexture3
{
	DECLARE_CLASS(UTextureCube3, UTexture3)
public:
	UTexture2D*		FacePosX;
	UTexture2D*		FaceNegX;
	UTexture2D*		FacePosY;
	UTexture2D*		FaceNegY;
	UTexture2D*		FacePosZ;
	UTexture2D*		FaceNegZ;

#if RENDERING
	// rendering implementation fields
	GLuint			TexNum;
#endif

	BEGIN_PROP_TABLE
		PROP_OBJ(FacePosX)
		PROP_OBJ(FaceNegX)
		PROP_OBJ(FacePosY)
		PROP_OBJ(FaceNegY)
		PROP_OBJ(FacePosZ)
		PROP_OBJ(FaceNegZ)
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		Super::Serialize(Ar);
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 108) goto skip_rest_quiet; // has TArray<FTexture2DMipMap>
#endif

		// some hack to support more games ...
		if (Ar.Tell() < Ar.GetStopper())
		{
			appPrintf("UTextureCube3 %s: dropping %d bytes\n", Name, Ar.GetStopper() - Ar.Tell());
		skip_rest_quiet:
			DROP_REMAINING_DATA(Ar);
		}
	}

#if RENDERING
	virtual bool Upload();
	virtual bool Bind();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void Release();
	virtual bool IsTextureCube() const
	{
		return true;
	}
#endif // RENDERING
};


#if UNREAL4

// In UE4, UTextureCube is derived from UTexture, however it has almost identical serializer to UTexture2D.
// In order to reuse the code, we'll derive it from UTexture2D.
class UTextureCube4 : public UTexture2D
{
	DECLARE_CLASS(UTextureCube4, UTexture2D)
public:
#if RENDERING
	virtual bool Upload();
	virtual bool Bind();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void Release();
	virtual bool IsTextureCube() const
	{
		return true;
	}
#endif // RENDERING
};

#endif // UNREAL4


enum EBlendMode
{
	BLEND_Opaque,
	BLEND_Masked,
	BLEND_Translucent,
	BLEND_Additive,
	BLEND_Modulate,
	BLEND_AlphaComposite,	// UE4 = 5, UE3 = 7
	BLEND_DitheredTranslucent, // UE4 = 6,UE3 = 8
	BLEND_ModulateAndAdd,	// UE3 = 5
	BLEND_SoftMasked,		// UE3 = 6
};

_ENUM(EBlendMode)
{
	_E(BLEND_Opaque),
	_E(BLEND_Masked),
	_E(BLEND_Translucent),
	_E(BLEND_Additive),
	_E(BLEND_Modulate),
	_E(BLEND_AlphaComposite),
	_E(BLEND_DitheredTranslucent),
	_E(BLEND_ModulateAndAdd),
	_E(BLEND_SoftMasked),
};

enum EMaterialLightingModel	// unused now
{
	MLM_Phong,
	MLM_NonDirectional,
	MLM_Unlit,
	MLM_SHPRT,
	MLM_Custom
};

enum EMobileSpecularMask
{
	MSM_Constant,
	MSM_Luminance,
	MSM_DiffuseRed,
	MSM_DiffuseGreen,
	MSM_DiffuseBlue,
	MSM_DiffuseAlpha,
	MSM_MaskTextureRGB,
	MSM_MaskTextureRed,
	MSM_MaskTextureGreen,
	MSM_MaskTextureBlue,
	MSM_MaskTextureAlpha
};

_ENUM(EMobileSpecularMask)
{
	_E(MSM_Constant),
	_E(MSM_Luminance),
	_E(MSM_DiffuseRed),
	_E(MSM_DiffuseGreen),
	_E(MSM_DiffuseBlue),
	_E(MSM_DiffuseAlpha),
	_E(MSM_MaskTextureRGB),
	_E(MSM_MaskTextureRed),
	_E(MSM_MaskTextureGreen),
	_E(MSM_MaskTextureBlue),
	_E(MSM_MaskTextureAlpha)
};


#if UNREAL4

struct FLightmassMaterialInterfaceSettings
{
	DECLARE_STRUCT(FLightmassMaterialInterfaceSettings);
	DUMMY_PROP_TABLE
};

struct FMaterialTextureInfo
{
	DECLARE_STRUCT(FMaterialTextureInfo);
	float			SamplingScale;
	int32			UVChannelIndex;
	FName			TextureName;
	BEGIN_PROP_TABLE
		PROP_FLOAT(SamplingScale)
		PROP_INT(UVChannelIndex)
		PROP_NAME(TextureName)
	END_PROP_TABLE
};

#endif // UNREAL4

class UMaterialInterface : public UUnrealMaterial
{
	DECLARE_CLASS(UMaterialInterface, UUnrealMaterial)
public:
	// Mobile
	UTexture3		*FlattenedTexture;
	UTexture3		*MobileBaseTexture;
	UTexture3		*MobileNormalTexture;
	bool			bUseMobileSpecular;
	float			MobileSpecularPower;
	EMobileSpecularMask MobileSpecularMask;
	UTexture3		*MobileMaskTexture;
#if UNREAL4
	TArray<FMaterialTextureInfo> TextureStreamingData;
#endif // UNREAL4

	UMaterialInterface()
	:	MobileSpecularPower(16)
//	,	MobileSpecularMask(MSM_Constant)
	{}

	BEGIN_PROP_TABLE
		PROP_DROP(PreviewMesh)
		PROP_DROP(LightingGuid)
		// Mobile
		PROP_OBJ(FlattenedTexture)
		PROP_OBJ(MobileBaseTexture)
		PROP_OBJ(MobileNormalTexture)
		PROP_DROP(bMobileAllowFog)
		PROP_BOOL(bUseMobileSpecular)
		PROP_DROP(MobileSpecularColor)
		PROP_DROP(bUseMobilePixelSpecular)
		PROP_FLOAT(MobileSpecularPower)
		PROP_ENUM2(MobileSpecularMask, EMobileSpecularMask)
		PROP_OBJ(MobileMaskTexture)
#if MASSEFF
		PROP_DROP(m_Guid)
#endif
#if UNREAL4
		PROP_ARRAY(TextureStreamingData, "FMaterialTextureInfo")
		PROP_DROP(LightmassSettings, "FLightmassMaterialInterfaceSettings")
		PROP_DROP(SubsurfaceProfile)
#endif
	END_PROP_TABLE

#if RENDERING
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};

#if UNREAL4

enum class EMaterialParameterType
{
	Scalar,
	Vector,
	Texture,
	Font,
	RuntimeVirtualTexture,
	Count
};

struct FMaterialParameterInfo
{
	DECLARE_STRUCT(FMaterialParameterInfo)

	FName			Name;

	BEGIN_PROP_TABLE
		PROP_NAME(Name)
		// Layer information
		PROP_DROP(Association, PropType::Byte)
		PROP_DROP(Index, PropType::Int)
	END_PROP_TABLE
};

struct FMaterialCachedParameterEntry
{
	DECLARE_STRUCT(FMaterialCachedParameterEntry)

	TArray<FMaterialParameterInfo> ParameterInfos;

	BEGIN_PROP_TABLE
		PROP_ARRAY(ParameterInfos, "FMaterialParameterInfo")
		PROP_DROP(NameHashes)
		PROP_DROP(ExpressionGuids)
		PROP_DROP(Overrides)
	END_PROP_TABLE
};

struct FMaterialCachedParameters
{
	DECLARE_STRUCT(FMaterialCachedParameters)

	FMaterialCachedParameterEntry Entries[(int)EMaterialParameterType::Count];
	TArray<float> ScalarValues;
	TArray<FLinearColor> VectorValues;
	TArray<UTexture3*> TextureValues;

	BEGIN_PROP_TABLE
		PROP_STRUC(Entries, FMaterialCachedParameterEntry)
		PROP_ALIAS(Entries, RuntimeEntries) // renamed in 4.26
		PROP_ARRAY(ScalarValues, PropType::Float)
		PROP_ARRAY(VectorValues, "FLinearColor")
		PROP_ARRAY(TextureValues, PropType::UObject)
		PROP_DROP(FontValues)
		PROP_DROP(FontPageValues)
		PROP_DROP(RuntimeVirtualTextureValues)
	END_PROP_TABLE
};

struct FMaterialCachedExpressionData
{
	DECLARE_STRUCT(FMaterialCachedExpressionData)

	FMaterialCachedParameters Parameters;
	TArray<UObject*> ReferencedTextures;

	BEGIN_PROP_TABLE
		PROP_STRUC(Parameters, FMaterialCachedParameters)
		PROP_ARRAY(ReferencedTextures, PropType::UObject)
		PROP_DROP(FunctionInfos)
		PROP_DROP(ParameterCollectionInfos)
		PROP_DROP(DefaultLayers)
		PROP_DROP(DefaultLayerBlends)
		PROP_DROP(GrassTypes)
		PROP_DROP(DynamicParameterNames)
		PROP_DROP(QualityLevelsUsed)
		PROP_DROP(bHasRuntimeVirtualTextureOutput)
		PROP_DROP(bHasSceneColor)
	END_PROP_TABLE
};

#endif // UNREAL4

class UMaterial3 : public UMaterialInterface
{
	DECLARE_CLASS(UMaterial3, UMaterialInterface)
public:
	bool			TwoSided;
	bool			bDisableDepthTest;
	bool			bIsMasked;
	EBlendMode		BlendMode;
	float			OpacityMaskClipValue;
	TArray<UTexture3*> ReferencedTextures;
	TArray<UObject*> Expressions;
#if UNREAL4
	FMaterialCachedExpressionData CachedExpressionData;
#endif

	// Generated data
	TArray<CTextureParameterValue> CollectedTextureParameters;
	TArray<CScalarParameterValue> CollectedScalarParameters;
	TArray<CVectorParameterValue> CollectedVectorParameters;

	UMaterial3()
	:	OpacityMaskClipValue(0.333f)		//?? check
	,	BlendMode(BLEND_Opaque)
	{}

	BEGIN_PROP_TABLE
		PROP_BOOL(TwoSided)
		PROP_BOOL(bDisableDepthTest)
		PROP_BOOL(bIsMasked)
		PROP_ARRAY(ReferencedTextures, PropType::UObject)
		PROP_ARRAY(Expressions, PropType::UObject)
		PROP_STRUC(CachedExpressionData, FMaterialCachedExpressionData)
		PROP_ENUM2(BlendMode, EBlendMode)
		PROP_FLOAT(OpacityMaskClipValue)
#if DECLARE_VIEWER_PROPS
		PROP_ARRAY(CollectedTextureParameters, "CTextureParameterValue")
		PROP_ARRAY(CollectedScalarParameters, "CScalarParameterValue")
		PROP_ARRAY(CollectedVectorParameters, "CVectorParameterValue")
#endif // DECLARE_VIEWER_PROPS
		//!! should be used (main material inputs in UE3 material editor)
		PROP_DROP(DiffuseColor)
		PROP_DROP(DiffusePower)				// GoW2
		PROP_DROP(EmissiveColor)
		PROP_DROP(SpecularColor)
		PROP_DROP(SpecularPower)
		PROP_DROP(Opacity)
		PROP_DROP(OpacityMask)
		PROP_DROP(Distortion)
		PROP_DROP(TwoSidedLightingMask)		// TransmissionMask ?
		PROP_DROP(TwoSidedLightingColor)	// TransmissionColor ?
		PROP_DROP(Normal)
		PROP_DROP(CustomLighting)
		// drop other props
		PROP_DROP(PhysMaterial)
		PROP_DROP(PhysicalMaterial)
		PROP_DROP(LightingModel)			//!! use it (EMaterialLightingModel)
		// usage
		PROP_DROP(bUsedAsLightFunction)
		PROP_DROP(bUsedWithFogVolumes)
		PROP_DROP(bUsedAsSpecialEngineMaterial)
		PROP_DROP(bUsedWithSkeletalMesh)
		PROP_DROP(bUsedWithParticleSystem)
		PROP_DROP(bUsedWithParticleSprites)
		PROP_DROP(bUsedWithBeamTrails)
		PROP_DROP(bUsedWithParticleSubUV)
		PROP_DROP(bUsedWithFoliage)
		PROP_DROP(bUsedWithSpeedTree)
		PROP_DROP(bUsedWithStaticLighting)
		PROP_DROP(bUsedWithLensFlare)
		PROP_DROP(bUsedWithGammaCorrection)
		PROP_DROP(bUsedWithInstancedMeshParticles)
		PROP_DROP(bUsedWithDecals)			// GoW2
		PROP_DROP(bUsedWithFracturedMeshes)	// GoW2
		PROP_DROP(bUsedWithFluidSurfaces)	// GoW2
		PROP_DROP(bUsedWithSplineMeshes)
		PROP_DROP(bUsedWithInstancedStaticMeshes)
		// physics
		PROP_DROP(PhysMaterialMask)
		PROP_DROP(PhysMaterialMaskUVChannel)
		PROP_DROP(BlackPhysicalMaterial)
		PROP_DROP(WhitePhysicalMaterial)
		// other
		PROP_DROP(Wireframe)
		PROP_DROP(bIsFallbackMaterial)
		PROP_DROP(FallbackMaterial)			//!! use it
		PROP_DROP(EditorX)
		PROP_DROP(EditorY)
		PROP_DROP(EditorPitch)
		PROP_DROP(EditorYaw)
		PROP_DROP(EditorComments)
		PROP_DROP(EditorCompounds)
		PROP_DROP(bUsesDistortion)
		PROP_DROP(bUsesSceneColor)
		PROP_DROP(bUsedWithMorphTargets)
		PROP_DROP(bAllowFog)
		PROP_DROP(ReferencedTextureGuids)
#if MEDGE
		PROP_DROP(BakerBleedBounceAmount)
		PROP_DROP(BakerAlpha)
#endif // MEDGE
#if TLR
		PROP_DROP(VFXShaderType)
#endif
#if MASSEFF
		PROP_DROP(bUsedWithLightEnvironments)
		PROP_DROP(AllowsEffectsMaterials)
#endif // MASSEFF
#if UNREAL4
		PROP_DROP(bUsedWithMeshParticles)
		PROP_DROP(bCanMaskedBeAssumedOpaque)
		PROP_DROP(ShadingModel)
		PROP_DROP(ShadingModels)
		PROP_DROP(bEnableResponsiveAA)
		PROP_DROP(StateId)
		PROP_DROP(CachedQualityLevelsUsed)
		PROP_DROP(bUseMaterialAttributes)
#endif
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar);
#if UNREAL4
	void Serialize4(FArchive& Ar);
#endif

	virtual void PostLoad();
	void ScanMaterialExpressions();
#if UNREAL4
	void ScanUE4Textures();
#endif

#if RENDERING
	virtual void SetupGL();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered) const;
	virtual bool IsTranslucent() const;
#endif
};

#if UNREAL4

struct FMaterialInstanceBasePropertyOverrides
{
	DECLARE_STRUCT(FMaterialInstanceBasePropertyOverrides);

	bool			bOverride_BlendMode;
	EBlendMode		BlendMode;

	bool			bOverride_TwoSided;
	bool			TwoSided;

	bool			bOverride_OpacityMaskClipValue;
	float			OpacityMaskClipValue;

	BEGIN_PROP_TABLE
		PROP_BOOL(bOverride_BlendMode)
		PROP_ENUM2(BlendMode, EBlendMode)
		PROP_BOOL(bOverride_TwoSided)
		PROP_BOOL(TwoSided)
		PROP_BOOL(bOverride_OpacityMaskClipValue)
		PROP_FLOAT(OpacityMaskClipValue)
		PROP_DROP(bOverride_ShadingModel)
		PROP_DROP(ShadingModel)
	END_PROP_TABLE

	FMaterialInstanceBasePropertyOverrides()
	:	bOverride_BlendMode(false)
	,	BlendMode(BLEND_Opaque)
	,	bOverride_TwoSided(false)
	,	TwoSided(0)
	{}
};

struct FStaticParameterBase
{
	DECLARE_STRUCT(FStaticParameterBase);
	FMaterialParameterInfo ParameterInfo;
	bool bOverride;
	BEGIN_PROP_TABLE
		PROP_STRUC(ParameterInfo, FMaterialParameterInfo)
		PROP_BOOL(bOverride)
		PROP_DROP(ExpressionGUID, PropType::FVector4) // FGuid
	END_PROP_TABLE
};

struct FStaticSwitchParameter : public FStaticParameterBase
{
	DECLARE_STRUCT2(FStaticSwitchParameter, FStaticParameterBase);
	bool Value;
	BEGIN_PROP_TABLE
		PROP_BOOL(Value)
	END_PROP_TABLE
};

struct FStaticComponentMaskParameter : public FStaticParameterBase
{
	DECLARE_STRUCT2(FStaticComponentMaskParameter, FStaticParameterBase);
	bool R;
	bool G;
	bool B;
	bool A;
	BEGIN_PROP_TABLE
		PROP_BOOL(R)
		PROP_BOOL(G)
		PROP_BOOL(B)
		PROP_BOOL(A)
	END_PROP_TABLE
};

struct FStaticTerrainLayerWeightParameter : public FStaticParameterBase
{
	DECLARE_STRUCT2(FStaticTerrainLayerWeightParameter, FStaticParameterBase);
	int32 WeightmapIndex;
	bool bWeightBasedBlend;
	BEGIN_PROP_TABLE
		PROP_INT(WeightmapIndex)
		PROP_BOOL(bWeightBasedBlend)
	END_PROP_TABLE
};

struct FStaticMaterialLayersParameter : public FStaticParameterBase
{
	DECLARE_STRUCT2(FStaticMaterialLayersParameter, FStaticParameterBase);
	// FMaterialLayersFunctions Value;
	DUMMY_PROP_TABLE
};

struct FStaticParameterSet
{
	DECLARE_STRUCT(FStaticParameterSet);

	TArray<FStaticSwitchParameter> StaticSwitchParameters;
	TArray<FStaticComponentMaskParameter> StaticComponentMaskParameters;
	TArray<FStaticTerrainLayerWeightParameter> TerrainLayerWeightParameters;
	TArray<FStaticMaterialLayersParameter> MaterialLayersParameters;

	BEGIN_PROP_TABLE
		PROP_ARRAY(StaticSwitchParameters, "FStaticSwitchParameter")
		PROP_ARRAY(StaticComponentMaskParameters, "FStaticComponentMaskParameter")
		PROP_ARRAY(TerrainLayerWeightParameters, "FStaticTerrainLayerWeightParameter")
		PROP_ARRAY(MaterialLayersParameters, "FStaticMaterialLayersParameter")
	END_PROP_TABLE
};

#endif // UNREAL4

struct FScalarParameterValue
{
	DECLARE_STRUCT(FScalarParameterValue)

	FName			ParameterName;
	float			ParameterValue;
#if UNREAL4
	FMaterialParameterInfo ParameterInfo;
#endif

	const char* GetName() const
	{
#if UNREAL4
		if (ParameterInfo.Name != "None")
			return *ParameterInfo.Name;
#endif
		return ParameterName;
	}

	BEGIN_PROP_TABLE
#if UNREAL4
		PROP_STRUC(ParameterInfo, FMaterialParameterInfo) // property 0 in UE4.26
#endif
		PROP_FLOAT(ParameterValue)						// property 1
		PROP_DROP(ExpressionGUID, PropType::FVector4)	// property 2
		PROP_NAME(ParameterName)
#if TRANSFORMERS
		PROP_DROP(ParameterCategory)
#endif
	END_PROP_TABLE
};

struct FTextureParameterValue
{
	DECLARE_STRUCT(FTextureParameterValue)

	FName			ParameterName;
	UTexture3		*ParameterValue;
#if UNREAL4
	FMaterialParameterInfo ParameterInfo;
#endif

	const char* GetName() const
	{
#if UNREAL4
		if (ParameterInfo.Name != "None")
			return *ParameterInfo.Name;
#endif
		return ParameterName;
	}

	BEGIN_PROP_TABLE
#if UNREAL4
		PROP_STRUC(ParameterInfo, FMaterialParameterInfo) // property 0
#endif
		PROP_OBJ(ParameterValue)						// property 1
		PROP_DROP(ExpressionGUID, PropType::FVector4)	// property 2
		PROP_NAME(ParameterName)
#if TRANSFORMERS
		PROP_DROP(ParameterCategory)
#endif
	END_PROP_TABLE
};

struct FVectorParameterValue
{
	DECLARE_STRUCT(FVectorParameterValue)

	FName			ParameterName;
	FLinearColor	ParameterValue;
//	FGuid			ExpressionGUID;
#if UNREAL4
	FMaterialParameterInfo ParameterInfo;
#endif

	const char* GetName() const
	{
#if UNREAL4
		if (ParameterInfo.Name != "None")
			return *ParameterInfo.Name;
#endif
		return ParameterName;
	}

	BEGIN_PROP_TABLE
#if UNREAL4
		PROP_STRUC(ParameterInfo, FMaterialParameterInfo) // property 0
#endif
		PROP_STRUC(ParameterValue, FLinearColor)		// property 1
		PROP_DROP(ExpressionGUID, PropType::FVector4)	// property 2
		PROP_NAME(ParameterName)
#if TRANSFORMERS
		PROP_DROP(ParameterCategory)
#endif
	END_PROP_TABLE
};

#if UNREAL4

struct FRuntimeVirtualTextureParameterValue
{
	DECLARE_STRUCT(FRuntimeVirtualTextureParameterValue)

//	FGuid			ExpressionGUID;
	FMaterialParameterInfo ParameterInfo;

	BEGIN_PROP_TABLE
		PROP_STRUC(ParameterInfo, FMaterialParameterInfo) // property 0
		PROP_DROP(ParameterValue, PropType::UObject)	// property 1
		PROP_DROP(ExpressionGUID, PropType::FVector4)	// property 2
	END_PROP_TABLE
};

struct FFontParameterValue
{
	DECLARE_STRUCT(FFontParameterValue)

//	FGuid			ExpressionGUID;
	FMaterialParameterInfo ParameterInfo;

	BEGIN_PROP_TABLE
		PROP_STRUC(ParameterInfo, FMaterialParameterInfo) // property 0
		PROP_DROP(FontValue, PropType::UObject)			// property 1
		PROP_DROP(FontPage, PropType::Int)				// property 2
		PROP_DROP(ExpressionGUID, PropType::FVector4)	// property 3
	END_PROP_TABLE
};

#endif // UNREAL4

class UMaterialInstance : public UMaterialInterface
{
	DECLARE_CLASS(UMaterialInstance, UMaterialInterface)
public:
	UUnrealMaterial	*Parent;				// UMaterialInterface*
	TArray<FScalarParameterValue> ScalarParameterValues;
	TArray<FTextureParameterValue> TextureParameterValues;
	TArray<FVectorParameterValue> VectorParameterValues;
#if UNREAL4
	FMaterialInstanceBasePropertyOverrides BasePropertyOverrides;
	FStaticParameterSet StaticParameters;
	TArray<FRuntimeVirtualTextureParameterValue> RuntimeVirtualTextureParameterValues;
	TArray<FFontParameterValue> FontParameterValues;
#endif

	BEGIN_PROP_TABLE
		PROP_OBJ(Parent)
		PROP_ARRAY(ScalarParameterValues,  "FScalarParameterValue" )
		PROP_ARRAY(TextureParameterValues, "FTextureParameterValue")
		PROP_ARRAY(VectorParameterValues,  "FVectorParameterValue" )
		PROP_DROP(FontParameterValues)
#if UNREAL4
		PROP_STRUC(BasePropertyOverrides, FMaterialInstanceBasePropertyOverrides)
		PROP_STRUC(StaticParameters, FStaticParameterSet)
		PROP_ARRAY(RuntimeVirtualTextureParameterValues, "FRuntimeVirtualTextureParameterValue")
		PROP_ARRAY(FontParameterValues, "FFontParameterValue")
#endif
		PROP_DROP(PhysMaterial)
		PROP_DROP(bHasStaticPermutationResource)
		PROP_DROP(ReferencedTextures)		// this is a textures from Parent plus own overridden textures
		PROP_DROP(ReferencedTextureGuids)
		PROP_DROP(ParentLightingGuid)
		// physics
		PROP_DROP(PhysMaterialMask)
		PROP_DROP(PhysMaterialMaskUVChannel)
		PROP_DROP(BlackPhysicalMaterial)
		PROP_DROP(WhitePhysicalMaterial)
#if UNREAL4
		PROP_DROP(bOverrideSubsurfaceProfile)
#endif
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);			//?? drop native data
	}
};

class UMaterialInstanceConstant : public UMaterialInstance
{
	DECLARE_CLASS(UMaterialInstanceConstant, UMaterialInstance)
public:

#if RENDERING
	virtual void SetupGL();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered) const;
	virtual bool IsTranslucent() const
	{
		return (Parent && Parent != this) ? Parent->IsTranslucent() : false;
	}
#endif
};


#if DCU_ONLINE

struct UIStreamingTexture_Info_DCU
{
	unsigned		Hash;				// real texture has no hash, it is a Map<int,UIStreamingTexture_Info>
	int				nWidth;
	int				nHeight;
	int				BulkDataFlags;
	int				ElementCount;
	int				BulkDataOffsetInFile;
	int				BulkDataSizeOnDisk;
	int				Format;
	byte			bSRGB;
	FName			TextureFileCacheName;

	friend FArchive& operator<<(FArchive &Ar, UIStreamingTexture_Info_DCU &S)
	{
		Ar << S.Hash;					// serialize it in the structure
		return Ar << S.nWidth << S.nHeight << S.BulkDataFlags << S.ElementCount << S.BulkDataOffsetInFile
				  << S.BulkDataSizeOnDisk << S.Format << S.bSRGB << S.TextureFileCacheName;
	}
};

//?? non-functional class, remove it later
class UUIStreamingTextures : public UObject
{
	DECLARE_CLASS(UUIStreamingTextures, UObject);
public:
	TArray<UIStreamingTexture_Info_DCU> TextureHashToInfo;

	// generated data
	TArray<UTexture2D*>		Textures;
	virtual ~UUIStreamingTextures();

	virtual void Serialize(FArchive &Ar)
	{
		guard(UUIStreamingTextures::Serialize);
		Super::Serialize(Ar);
		Ar << TextureHashToInfo;
		unguard;
	}

	virtual void PostLoad();
};

#endif // DCU_ONLINE


#define REGISTER_MATERIAL_CLASSES_U3	\
	REGISTER_CLASS_ALIAS(UMaterial3, UMaterial) \
	REGISTER_CLASS(UTexture2D)			\
	REGISTER_CLASS(ULightMapTexture2D)	\
	REGISTER_CLASS_ALIAS(UTextureCube3, UTextureCube) \
	REGISTER_CLASS(FScalarParameterValue)  \
	REGISTER_CLASS(FTextureParameterValue) \
	REGISTER_CLASS(FVectorParameterValue)  \
	REGISTER_CLASS(UMaterialInstanceConstant)

#define REGISTER_MATERIAL_CLASSES_DCUO	\
	REGISTER_CLASS(UUIStreamingTextures)

#define REGISTER_MATERIAL_ENUMS_U3		\
	REGISTER_ENUM(EPixelFormat)			\
	REGISTER_ENUM(ETextureAddress)		\
	REGISTER_ENUM(ETextureCompressionSettings) \
	REGISTER_ENUM(EBlendMode)			\
	REGISTER_ENUM(EMobileSpecularMask)	\

#define REGISTER_MATERIAL_CLASSES_U4	\
	REGISTER_CLASS_ALIAS(UTextureCube4, UTextureCube) \
	REGISTER_CLASS_ALIAS(UMaterialInstanceConstant, UMaterialInstanceDynamic) \
	REGISTER_CLASS(FMaterialParameterInfo) \
	REGISTER_CLASS(FMaterialCachedParameterEntry) \
	REGISTER_CLASS(FMaterialCachedParameters) \
	REGISTER_CLASS(FMaterialCachedExpressionData) \
	REGISTER_CLASS(FTextureSource)		\
	REGISTER_CLASS(FMaterialInstanceBasePropertyOverrides) \
	REGISTER_CLASS(FStaticParameterBase) \
	REGISTER_CLASS(FStaticSwitchParameter) \
	REGISTER_CLASS(FStaticComponentMaskParameter) \
	REGISTER_CLASS(FStaticTerrainLayerWeightParameter) \
	REGISTER_CLASS(FStaticMaterialLayersParameter) \
	REGISTER_CLASS(FStaticParameterSet) \
	REGISTER_CLASS(FLightmassMaterialInterfaceSettings) \
	REGISTER_CLASS(FMaterialTextureInfo) \
	REGISTER_CLASS(FFontParameterValue) \
	REGISTER_CLASS(FRuntimeVirtualTextureParameterValue)

#define REGISTER_MATERIAL_ENUMS_U4		\
	REGISTER_ENUM(ETextureSourceFormat)

#endif // UNREAL3

#endif // __UNMATERIAL3_H__
