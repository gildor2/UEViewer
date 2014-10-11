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
	bool IsNull() const
	{
#define C(x) ((size_t)x)
		return (C(Diffuse) | C(Normal) | C(Specular) | C(SpecPower) | C(Opacity) | C(Emissive) | C(Cube)) == 0;
#undef C
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


// Should change PixelFormatInfo[] in UnTexture.cpp when modify this enum!
enum ETexturePixelFormat
{
	TPF_P8,				// 8-bit paletted
	TPF_G8,				// 8-bit grayscale
//	TPF_G16,			// 16-bit grayscale (terrain heightmaps)
	TPF_RGB8,
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
#if IPHONE
	TPF_PVRTC2,
	TPF_PVRTC4,
#endif
#if ANDROID
	TPF_ETC1,
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

#if XBOX360
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

#if RENDERING
	UUnrealMaterial()
	:	DrawTimestamp(0)
	{}

	void SetMaterial();								// main function to use from outside

	virtual void Bind()								// implemented for textures only
	{}
	virtual void SetupGL();
	virtual void Release();
	virtual void GetParams(CMaterialParams &Params) const
	{}
	virtual bool IsTranslucent() const
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

protected:
	// rendering implementation fields
	CShader					GLShader;
	int						DrawTimestamp;			// timestamp for object validation
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
