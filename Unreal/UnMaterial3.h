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
	FTextureSource	Source;
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
#endif // UNREAL4
#if THIEF4
	PF_BC7,
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
#endif // UNREAL4
#if THIEF4 // || UNREAL4
	_E(PF_BC7),
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
	int				SizeX;
	int				SizeY;

	friend FArchive& operator<<(FArchive &Ar, FTexture2DMipMap &Mip)
	{
		guard(FTexture2DMipMap<<);
#if UNREAL4
		if (Ar.Game >= GAME_UE4_BASE)
		{
			// this code is generally the same as for UE3, but it's quite simple, so keep
			// it in separate code block
			bool cooked = false;
			if (Ar.ArVer >= VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)
				Ar << cooked;
			//?? Can all 'Mip.Data.Skip(Ar)' here, but ensure any mip types will be loaded by LoadBulkTexture().
			//?? Calling Skip() will eliminate extra seeks when interleaving reading of FTexture2DMipMap and
			//?? bulk data which located in the same uasset, but at different position.
			//?? To see the problem (performance problem): load data from pak, modify FPakFile, add logging of
			//?? Seek() and decompression calls. You'll see that loading of big bulk data chinks is interleaved
			//?? with reading 4-byte ints at different locations.
			Mip.Data.Serialize(Ar);
			Ar << Mip.SizeX << Mip.SizeY;
			if (Ar.ArVer >= VER_UE4_TEXTURE_DERIVED_DATA2 && !cooked)
			{
				FString DerivedDataKey;
				Ar << DerivedDataKey;
			}
	#if 0
			// Oculus demos ("Henry") can have extra int here
			int tmp;
			Ar << tmp;
	#endif
			return Ar;
		}
#endif // UNREAL4
		Mip.Data.Serialize(Ar);
#if DARKVOID
		if (Ar.Game == GAME_DarkVoid)
		{
			FByteBulkData DataX360Gamma;
			DataX360Gamma.Serialize(Ar);
		}
#endif // DARKVOID
		return Ar << Mip.SizeX << Mip.SizeY;
		unguard;
	}
};

class UTexture2D : public UTexture3
{
	DECLARE_CLASS(UTexture2D, UTexture3)
public:
	TArray<FTexture2DMipMap> Mips;
	TArray<FTexture2DMipMap> CachedPVRTCMips;
	TArray<FTexture2DMipMap> CachedATITCMips;
	TArray<FTexture2DMipMap> CachedETCMips;
	int				SizeX;
	int				SizeY;
	EPixelFormat	Format;
	ETextureAddress	AddressX;
	ETextureAddress	AddressY;
	FName			TextureFileCacheName;
	FGuid			TextureFileCacheGuid;
	int				MipTailBaseIdx;
	bool			bForcePVRTC4;		// iPhone
#if UNREAL4
	FIntPoint		ImportedSize;
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

	const TArray<FTexture2DMipMap>* GetMipmapArray() const;

	bool LoadBulkTexture(const TArray<FTexture2DMipMap> &MipsArray, int MipIndex, const char* tfcSuffix, bool verbose) const;
	virtual bool GetTextureData(CTextureData &TexData) const;
	virtual void ReleaseTextureData() const;
#if RENDERING
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


class UTextureCube : public UTexture3
{
	DECLARE_CLASS(UTextureCube, UTexture3)
public:
	UTexture2D		*FacePosX;
	UTexture2D		*FaceNegX;
	UTexture2D		*FacePosY;
	UTexture2D		*FaceNegY;
	UTexture2D		*FacePosZ;
	UTexture2D		*FaceNegZ;

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
			appPrintf("UTextureCube %s: dropping %d bytes\n", Name, Ar.GetStopper() - Ar.Tell());
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


enum EBlendMode
{
	BLEND_Opaque,
	BLEND_Masked,
	BLEND_Translucent,
	BLEND_Additive,
	BLEND_Modulate,
	BLEND_ModulateAndAdd,
	BLEND_SoftMasked,
	BLEND_AlphaComposite,
	BLEND_DitheredTranslucent
};

_ENUM(EBlendMode)
{
	_E(BLEND_Opaque),
	_E(BLEND_Masked),
	_E(BLEND_Translucent),
	_E(BLEND_Additive),
	_E(BLEND_Modulate),
	_E(BLEND_ModulateAndAdd),
	_E(BLEND_SoftMasked),
	_E(BLEND_AlphaComposite),
	_E(BLEND_DitheredTranslucent)
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
	END_PROP_TABLE

#if RENDERING
	virtual void GetParams(CMaterialParams &Params) const;
#endif
};


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

	UMaterial3()
	:	OpacityMaskClipValue(0.333f)		//?? check
	,	BlendMode(BLEND_Opaque)
	{}

	BEGIN_PROP_TABLE
		PROP_BOOL(TwoSided)
		PROP_BOOL(bDisableDepthTest)
		PROP_BOOL(bIsMasked)
		PROP_ARRAY(ReferencedTextures, UObject*)
		PROP_ENUM2(BlendMode, EBlendMode)
		PROP_FLOAT(OpacityMaskClipValue)
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
		PROP_DROP(Expressions)				//!! use it
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
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		Super::Serialize(Ar);
#if UNREAL4
		if (Ar.Game >= GAME_UE4_BASE)
		{
			// UE4 has complex FMaterialResource format, so avoid reading anything here, but
			// scan package's imports for UTexture objects instead
			ScanForTextures();
			DROP_REMAINING_DATA(Ar);
			return;
		}
#endif // UNREAL4
#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3)
		{
			// Bioshock Infinite has different internal material format
			FGuid unk44;
			TArray<UTexture2D*> Textures[5];
			Ar << unk44;
			for (int i = 0; i < 5; i++)
			{
				Ar << Textures[i];
				if (!ReferencedTextures.Num() && Textures[i].Num())
					CopyArray(ReferencedTextures, Textures[i]);
	#if 0
				appPrintf("Arr[%d]\n", i);
				for (int j = 0; j < Textures[i].Num(); j++)
				{
					UObject *obj = Textures[i][j];
					appPrintf("... %d: %s %s\n", j, obj ? obj->GetClassName() : "??", obj ? obj->Name : "??");
				}
	#endif
			}
			return;
		}
#endif // BIOSHOCK3
#if MKVSDC
		if (Ar.Game == GAME_MK)
		{
			// MK X has version 677, but different format of UMaterial
			DROP_REMAINING_DATA(Ar);
			return;
		}
#endif // MKVSDC
#if METRO_CONF
		if (Ar.Game == GAME_MetroConflict && Ar.ArLicenseeVer >= 21) goto mask;
#endif
		if (Ar.ArVer >= 858)
		{
		mask:
			int unkMask;		// default 1
			Ar << unkMask;
		}
		if (Ar.ArVer >= 656)
		{
			guard(SerializeFMaterialResource);
			// Starting with version 656 UE3 has deprecated ReferencedTextures array.
			// This array is serialized inside FMaterialResource which is not needed
			// for us in other case.
			// FMaterialResource serialization is below
			TArray<FString>			f10;
			TMap<UObject*, int>		f1C;
			int						f58;
			FGuid					f60;
			int						f80;
			Ar << f10 << f1C << f58 << f60 << f80;
			if (Ar.ArVer >= 656) Ar << ReferencedTextures;	// that is ...
			// other fields are not interesting ...
			unguard;
		}
		DROP_REMAINING_DATA(Ar);			//?? drop native data
	}

#if UNREAL4
	void ScanForTextures();
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

	BEGIN_PROP_TABLE
		PROP_BOOL(bOverride_BlendMode)
		PROP_ENUM2(BlendMode, EBlendMode)
		PROP_BOOL(bOverride_TwoSided)
		PROP_BOOL(TwoSided)
	END_PROP_TABLE

	FMaterialInstanceBasePropertyOverrides()
	:	bOverride_BlendMode(false)
	,	BlendMode(BLEND_Opaque)
	,	bOverride_TwoSided(false)
	,	TwoSided(0)
	{}
};

#endif // UNREAL4

class UMaterialInstance : public UMaterialInterface
{
	DECLARE_CLASS(UMaterialInstance, UMaterialInterface)
public:
	UUnrealMaterial	*Parent;				// UMaterialInterface*
#if UNREAL4
	FMaterialInstanceBasePropertyOverrides BasePropertyOverrides;
#endif

	BEGIN_PROP_TABLE
		PROP_OBJ(Parent)
#if UNREAL4
		PROP_STRUC(BasePropertyOverrides, FMaterialInstanceBasePropertyOverrides)
#endif
		PROP_DROP(PhysMaterial)
		PROP_DROP(bHasStaticPermutationResource)
		PROP_DROP(ReferencedTextures)		// this is a textures from Parent plus own overrided textures
		PROP_DROP(ReferencedTextureGuids)
		PROP_DROP(ParentLightingGuid)
		// physics
		PROP_DROP(PhysMaterialMask)
		PROP_DROP(PhysMaterialMaskUVChannel)
		PROP_DROP(BlackPhysicalMaterial)
		PROP_DROP(WhitePhysicalMaterial)
	END_PROP_TABLE

	virtual void Serialize(FArchive &Ar)
	{
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);			//?? drop native data
	}
};


struct FScalarParameterValue
{
	DECLARE_STRUCT(FScalarParameterValue)

	FName			ParameterName;
	float			ParameterValue;
//	FGuid			ExpressionGUID;

	BEGIN_PROP_TABLE
		PROP_NAME(ParameterName)
		PROP_FLOAT(ParameterValue)
		PROP_DROP(ExpressionGUID)	//!! test nested structure serialization later
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
//	FGuid			ExpressionGUID;

	BEGIN_PROP_TABLE
		PROP_NAME(ParameterName)
		PROP_OBJ(ParameterValue)
		PROP_DROP(ExpressionGUID)	//!! test nested structure serialization later
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

	BEGIN_PROP_TABLE
		PROP_NAME(ParameterName)
		PROP_STRUC(ParameterValue, FLinearColor)
		PROP_DROP(ExpressionGUID)	//!! test nested structure serialization later
#if TRANSFORMERS
		PROP_DROP(ParameterCategory)
#endif
	END_PROP_TABLE
};

class UMaterialInstanceConstant : public UMaterialInstance
{
	DECLARE_CLASS(UMaterialInstanceConstant, UMaterialInstance)
public:
	TArray<FScalarParameterValue>	ScalarParameterValues;
	TArray<FTextureParameterValue>	TextureParameterValues;
	TArray<FVectorParameterValue>	VectorParameterValues;

	BEGIN_PROP_TABLE
		PROP_ARRAY(ScalarParameterValues,  FScalarParameterValue )
		PROP_ARRAY(TextureParameterValues, FTextureParameterValue)
		PROP_ARRAY(VectorParameterValues,  FVectorParameterValue )
		PROP_DROP(FontParameterValues)
	END_PROP_TABLE

#if RENDERING
	virtual void SetupGL();
	virtual void GetParams(CMaterialParams &Params) const;
	virtual void AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered) const;
	virtual bool IsTranslucent() const
	{
		return Parent ? Parent->IsTranslucent() : false;
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
	REGISTER_CLASS(UTextureCube)		\
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
	REGISTER_CLASS(FTextureSource)		\
	REGISTER_CLASS(FMaterialInstanceBasePropertyOverrides)

#define REGISTER_MATERIAL_ENUMS_U4		\
	REGISTER_ENUM(ETextureSourceFormat)

#endif // UNREAL3

#endif // __UNMATERIAL3_H__
