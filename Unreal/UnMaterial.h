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
	UUnrealMaterial *Diffuse;
	UUnrealMaterial *Normal;
	UUnrealMaterial *Specular;
	UUnrealMaterial *SpecPower;
	UUnrealMaterial *Opacity;
	UUnrealMaterial *Emissive;
	UUnrealMaterial *Cube;
	UUnrealMaterial	*Mask;					// multiple mask textures baked into a single one
	// channels
	ETextureCannel	EmissiveChannel;
	ETextureCannel	SpecularMaskChannel;
	ETextureCannel	SpecularPowerChannel;
	ETextureCannel	CubemapMaskChannel;
	// colors
	FLinearColor	EmissiveColor;
	// mobile
	bool			bUseMobileSpecular;
	float			MobileSpecularPower;
	int				MobileSpecularMask;		// EMobileSpecularMask
	// tweaks
	bool			SpecularFromAlpha;
	bool			OpacityFromAlpha;
};


class UUnrealMaterial : public UObject		// no such class in Unreal Engine, needed as common base for UE1/UE2/UE3
{
	DECLARE_CLASS(UUnrealMaterial, UObject);
public:
	virtual byte *Decompress(int &USize, int &VSize) const
	{
		return NULL;
	}
#if RENDERING
	UUnrealMaterial()
	:	DrawTimestamp(0)
	{}

	void SetMaterial(unsigned PolyFlags = 0);	// main function to use outside

	virtual void Bind()							// non-empty for textures only
	{}
	virtual void SetupGL(unsigned PolyFlags);	// PolyFlags used for UE1 only
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
	CShader			GLShader;
	int				DrawTimestamp;				// timestamp for object validation
		//?? may be use GLShader.DrawTimestamp only ?
#endif // RENDERING
};

//!! UE2 mesh uses UMaterial, which will not be declared
//!! use this macro for cast to UUnrealMaterial (for easy location of all conversion places)
#define MATERIAL_CAST(x)	( (UUnrealMaterial*)(x) )
//!! this macro can be used to "downgrade" UE3 material to UE2 UMaterial
#define MATERIAL_CAST2(x)	( (UMaterial*)      (x) )
//!! this program requires refactoring to remove all MATERIAL_CAST macros


#endif // __UNMATERIAL_H__
