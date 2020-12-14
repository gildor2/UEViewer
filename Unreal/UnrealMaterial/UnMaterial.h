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
#define PARAM(name)		if (name != NULL) OutTextures.AddUnique(name);
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
	bool					PBRMaterial;
};

#undef ITERATE_ALL_PARAMS


// Should change PixelFormatInfo[] in UnTexture.cpp when modify this enum!
enum ETexturePixelFormat
{
	TPF_UNKNOWN,
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
	TPF_BC4,			// alias names: 3Dc+, ATI1, BC4
	TPF_BC5,			// alias names: 3Dc, ATI2, BC5
	TPF_BC6H,
	TPF_BC7,
	TPF_A1,				// 8 monochrome pixels per byte
	TPF_RGBA4,
	TPF_FLOAT_RGBA,
#if SUPPORT_IPHONE
	TPF_PVRTC2,
	TPF_PVRTC4,
#endif
#if SUPPORT_ANDROID
	TPF_ETC1,
	TPF_ETC2_RGB,
	TPF_ETC2_RGBA,
	TPF_ASTC_4x4,
	TPF_ASTC_6x6,
	TPF_ASTC_8x8,
	TPF_ASTC_10x10,
	TPF_ASTC_12x12,
#endif // SUPPORT_ANDROID
	TPF_PNG_BGRA,		// UE3+ SourceArt format
	TPF_PNG_RGBA,
	TPF_MAX
};


struct CPixelFormatInfo
{
	unsigned	FourCC;				// 0 when not DDS-compatible
	byte		BlockSizeX;
	byte		BlockSizeY;
	byte		BytesPerBlock;
	int16		X360AlignX;			// 0 when unknown or not supported on XBox360
	int16		X360AlignY;
	byte		Float;				// 0 for RGBA8, 1 for RGBA32
	const char*	Name;

	bool IsDXT() const
	{
		return FourCC != 0;
	}
};


extern const CPixelFormatInfo PixelFormatInfo[];	// index in array is TPF_... constant

struct CMipMap
{
	const byte*				CompressedData;			// not TArray because we could just point to another data block without memory reallocation
	int						DataSize;				// this information is used for exporting compressed texture
	int						USize;
	int						VSize;
	bool					ShouldFreeData;			// free CompressedData when set to true

	CMipMap()
	{
		memset(this, 0, sizeof(CMipMap));
	}
	~CMipMap()
	{
		ReleaseData();
	}
	void SetOwnedDataBuffer(const byte* buf, int size)
	{
		// Release old data if any
		ReleaseData();
		// Assign new buffer, mark it as we own it (will release by outself)
		CompressedData = buf;
		DataSize = size;
		ShouldFreeData = true;
	}
#if UNREAL3
	void SetBulkData(const FByteBulkData& Bulk)
	{
		// Release old data if any
		ReleaseData();
		CompressedData = Bulk.BulkData;
		DataSize = Bulk.ElementCount * Bulk.GetElementSize();
		if (!GExportInProgress)
		{
			// Bulk owns data buffer
			ShouldFreeData = false;
		}
		else
		{
			// We can "grab" texture data from this bulk with possibly damaging the UObject data,
			// however during export this is safe because this texture UObject won't be used anywhere else
			const_cast<FByteBulkData&>(Bulk).BulkData = NULL;
			ShouldFreeData = true;
		}
	}
#endif // UNREAL3
	void ReleaseData()
	{
		if (ShouldFreeData)
			appFree(const_cast<byte*>(CompressedData));
		CompressedData = NULL;
		DataSize = 0;
		ShouldFreeData = false;
	}
};

struct CTextureData
{
	TArray<CMipMap>			Mips;					// mipmaps; could have 0, 1 or N entries
	ETexturePixelFormat		Format;
	int						Platform;
	const char*				OriginalFormatName;		// string value from typeinfo
	int						OriginalFormatEnum;		// ETextureFormat or EPixelFormat
	bool					isNormalmap;
	const UPalette*			Palette;				// for TPF_P8

protected:
	const char*				ObjectName;
	const char*				ObjectClass;
	int						ObjectGame;

public:
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
		for (int i = 0; i < Mips.Num(); i++)
			Mips[i].ReleaseData();
	}

	unsigned GetFourCC() const;

	void SetObject(const UObject* Obj)
	{
		ObjectClass = Obj->GetClassName();
		ObjectName = Obj->Name;
		ObjectGame = Obj->Package ? Obj->GetPackageArchive()->Game : GAME_UNKNOWN;
	}

	const char* GetObjectName() const
	{
		return ObjectName;
	}

	// Determine if CTextureData could be used without alive UObject (i.e. it's safe to use it after object released)
	bool OwnsAllData() const
	{
		if (Palette) return false;
		for (const CMipMap& Mip : Mips)
			if (!Mip.ShouldFreeData)
				return false;
		return true;
	}

	byte* Decompress(int MipLevel = 0, int Slice = -1);		// may return NULL in a case of error

#if SUPPORT_XBOX360
	bool DecodeXBox360(int MipLevel);
#endif
#if SUPPORT_PS4
	bool DecodePS4(int MipLevel);
#endif
#if SUPPORT_SWITCH
	bool DecodeNSW(int MipLevel);
#endif
};

// There's no such class in Unreal Engine, we use it as common base for UE1/UE2/UE3
class UUnrealMaterial : public UObject
{
	DECLARE_CLASS(UUnrealMaterial, UObject);
public:
	// Texture methods.
	// Convert engine's texture format to internal format value
	virtual ETexturePixelFormat GetTexturePixelFormat() const
	{
		return TPF_UNKNOWN;
	}
	// Fill empty CTextureData structure with actual data for upload.
	virtual bool GetTextureData(CTextureData &TexData) const
	{
		return false;
	}
	// Release data cached with GetTextureData().
	virtual void ReleaseTextureData() const
	{}

	void GetMetadata(FArchive& Ar) const
	{
		// Use object's name as default metadata, override when needed
		char ObjName[256];
		GetFullName(ARRAY_ARG(ObjName));
		int Len = strlen(ObjName);
		Ar << Len;
		Ar.Serialize(ObjName, Len);
	}

#if RENDERING
	UUnrealMaterial()
	:	DrawTimestamp(0)
	,	LockCount(0)
	,	NormalUnpackExpr(NULL)
	{}

	void SetMaterial();								// main function to use from outside

	//!! WARNING: UTextureCube will not work correctly - referenced textures are not encountered
	virtual void AppendReferencedTextures(TArray<UUnrealMaterial*>& OutTextures, bool onlyRendered = true) const;

	// Texture interface
	// TODO: make separate class for that, use with multiple inheritance
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

// Another one custom material class used for correctly handling PolyFlags in UE1 while rendering
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


// Parameters picked up from UMaterial graph

struct CParameterValueBase
{
	FName Name;
	FName Group;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CParameterValueBase)
	BEGIN_PROP_TABLE
		PROP_NAME(Name)
		PROP_NAME(Group)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};

struct CTextureParameterValue : public CParameterValueBase
{
	UUnrealMaterial* Texture;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT2(CTextureParameterValue, CParameterValueBase)
	BEGIN_PROP_TABLE
		PROP_OBJ(Texture)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};

struct CScalarParameterValue : public CParameterValueBase
{
	float Value;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT2(CScalarParameterValue, CParameterValueBase)
	BEGIN_PROP_TABLE
		PROP_FLOAT(Value)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};

struct CVectorParameterValue : public CParameterValueBase
{
	FLinearColor Value;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT2(CVectorParameterValue, CParameterValueBase)
	BEGIN_PROP_TABLE
		PROP_STRUC(Value, FLinearColor)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};


#define REGISTER_MATERIAL_VCLASSES \
	REGISTER_CLASS(CTextureParameterValue) \
	REGISTER_CLASS(CScalarParameterValue) \
	REGISTER_CLASS(CVectorParameterValue)


//!! UE2 mesh uses UMaterial, which sometimes will be declared as 'forward declaration'.
//!! Use this macro for cast to UUnrealMaterial (for easy location of all conversion places)
#define MATERIAL_CAST(x)	( (UUnrealMaterial*)(x) )


#endif // __UNMATERIAL_H__
