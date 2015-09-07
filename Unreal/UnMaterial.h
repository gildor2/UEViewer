#ifndef __UNMATERIAL_H__
#define __UNMATERIAL_H__


/*-----------------------------------------------------------------------------
	Internal base material class
-----------------------------------------------------------------------------*/

enum ETextureCannel
{
	TC_NONE = 0,
	TC_R,
	TC_G,
	TC_B,
	TC_A,
	TC_MA					// 1-Alpha
};


class UUnrealMaterial;
class UPalette;


struct CMaterialParams
{
	CMaterialParams()
	{
		memset(this, 0, sizeof(*this));
		EmissiveColor.Set(0.5f, 0.5f, 1.0f, 1);		// light-blue color
	}

#define ITERATE_ALL_PARAMS	\
	PARAM(Diffuse)			\
	PARAM(Normal)			\
	PARAM(Specular)			\
	PARAM(SpecPower)		\
	PARAM(Opacity)			\
	PARAM(Emissive)			\
	PARAM(Cube)				\
	PARAM(Mask)

	bool IsNull() const
	{
#define PARAM(name)		ret |= (size_t)name;
		size_t ret = 0;
		ITERATE_ALL_PARAMS;
		return ret == 0;
#undef PARAM
	}

	void AppendAllTextures(TArray<UUnrealMaterial*>& OutTextures)
	{
#define PARAM(name)		if (name != NULL) OutTextures.Add(name);
		ITERATE_ALL_PARAMS;
#undef PARAM
	}

	// textures
	UUnrealMaterial			*Diffuse;
	UUnrealMaterial			*Normal;
	UUnrealMaterial			*Specular;
	UUnrealMaterial			*SpecPower;
	UUnrealMaterial			*Opacity;
	UUnrealMaterial			*Emissive;
	UUnrealMaterial			*Cube;
	UUnrealMaterial			*Mask;					// multiple mask textures baked into a single one
	// channels (used with Mask texture)
	ETextureCannel			EmissiveChannel;
	ETextureCannel			SpecularMaskChannel;
	ETextureCannel			SpecularPowerChannel;
	ETextureCannel			CubemapMaskChannel;
	// colors
	FLinearColor			EmissiveColor;
	// mobile
	bool					bUseMobileSpecular;
	float					MobileSpecularPower;
	int						MobileSpecularMask;		// EMobileSpecularMask
	// tweaks
	bool					SpecularFromAlpha;
	bool					OpacityFromAlpha;
};

#undef ITERATE_ALL_PARAMS


// Should change PixelFormatInfo[] in UnTexture.cpp when modify this enum!
enum ETexturePixelFormat
{
	TPF_P8,				// 8-bit paletted
	TPF_G8,				// 8-bit grayscale
//	TPF_G16,			// 16-bit grayscale (terrain heightmaps)
	TPF_RGB8,
	TPF_RGBA8,			// 32-bit texture
	TPF_BGRA8,			// 32-bit texture
	TPF_DXT1,
	TPF_DXT3,
	TPF_DXT5,
	TPF_DXT5N,
	TPF_V8U8,
	TPF_V8U8_2,			// different decoding, has color offset compared to TPF_V8U8
	TPF_BC5,			// alias names: 3Dc, ATI2, BC5
	TPF_BC7,
	TPF_A1,				// 8 monochrome pixels per byte
	TPF_RGBA4,
#if SUPPORT_IPHONE
	TPF_PVRTC2,
	TPF_PVRTC4,
#endif
#if SUPPORT_ANDROID
	TPF_ETC1,
	TPF_ETC2,
#endif
	TPF_MAX
};


struct CPixelFormatInfo
{
	unsigned	FourCC;				// 0 when not DDS-compatible
	byte		BlockSizeX;
	byte		BlockSizeY;
	byte		BytesPerBlock;
	byte		X360AlignX;			// 0 when unknown or not supported on XBox360
	byte		X360AlignY;
	const char*	Name;
};


extern const CPixelFormatInfo PixelFormatInfo[];	// index in array is TPF_... constant

struct CTextureData
{
	const byte				*CompressedData;
	bool					ShouldFreeData;			// free CompressedData when set to true
	ETexturePixelFormat		Format;
	int						DataSize;				// used for XBox360 decompressor and DDS export
	int						USize;
	int						VSize;
	int						Platform;
	const char				*OriginalFormatName;	// string value from typeinfo
	int						OriginalFormatEnum;		// ETextureFormat or EPixelFormat
	const UObject			*Obj;					// for error reporting
	const UPalette			*Palette;				// for TPF_P8

	CTextureData()
	{
		memset(this, 0, sizeof(CTextureData));
	}
	~CTextureData()
	{
		ReleaseCompressedData();
	}
	void ReleaseCompressedData()
	{
		if (ShouldFreeData && CompressedData)
		{
			appFree(const_cast<byte*>(CompressedData));
			ShouldFreeData = false;
			CompressedData = NULL;
		}
	}

	unsigned GetFourCC() const;
	bool IsDXT() const;

	byte *Decompress();								// may return NULL in a case of error

#if SUPPORT_XBOX360
	void DecodeXBox360();
#endif
};


class UUnrealMaterial : public UObject				// no such class in Unreal Engine, needed as common base for UE1/UE2/UE3
{
	DECLARE_CLASS(UUnrealMaterial, UObject);
public:
	// texture methods
	virtual bool GetTextureData(CTextureData &TexData) const
	{
		return false;
	}

	virtual void ReleaseTextureData() const
	{}

#if RENDERING
	UUnrealMaterial()
	:	DrawTimestamp(0)
	,	LockCount(0)
	,	NormalUnpackExpr(NULL)
	{}

	void SetMaterial();								// main function to use from outside

	//!! WARNING: UTextureCube will not work correctly - referenced textures are not encountered
	void AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures);

	// Texture interface
	virtual bool Upload()
	{
		return false;
	}
	virtual bool Bind()
	{
		return false;
	}
	virtual bool IsTexture() const
	{
		return false;
	}
	virtual bool IsTextureCube() const
	{
		return false;
	}
	void Lock();
	void Unlock();

	virtual void SetupGL();							// used by SetMaterial()
	virtual void Release();							//!! make it protected
	virtual void GetParams(CMaterialParams &Params) const
	{}
	virtual bool IsTranslucent() const
	{
		return false;
	}

	const char*				NormalUnpackExpr;

protected:
	// rendering implementation fields
	CShader					GLShader;
	int						DrawTimestamp;			// timestamp for object validation
	int						LockCount;
		//?? may be use GLShader.DrawTimestamp only ?
#endif // RENDERING
};


class UMaterialWithPolyFlags : public UUnrealMaterial
{
	DECLARE_CLASS(UMaterialWithPolyFlags, UUnrealMaterial);
public:
	// PolyFlags support functions

#if RENDERING
	virtual void SetupGL();

	// Proxy functions

	virtual void Release()
	{
		Super::Release();
		if (Material) Material->Release();
	}

	virtual void GetParams(CMaterialParams &Params) const
	{
		if (Material) Material->GetParams(Params);
	}

	virtual bool IsTranslucent() const
	{
		return Material ? Material->IsTranslucent() : false;
	}

	static UUnrealMaterial *Create(UUnrealMaterial *OriginalMaterial, unsigned PolyFlags);

protected:
	UUnrealMaterial			*Material;
	unsigned				PolyFlags;
#endif // RENDERING
};




//!! UE2 mesh uses UMaterial, which sometimes will be declared as 'forward declaration'.
//!! Use this macro for cast to UUnrealMaterial (for easy location of all conversion places)
#define MATERIAL_CAST(x)	( (UUnrealMaterial*)(x) )


#endif // __UNMATERIAL_H__
