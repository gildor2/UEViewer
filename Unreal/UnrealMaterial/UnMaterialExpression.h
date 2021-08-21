#ifndef __UNMATERIAL_EXPRESSION_H__
#define __UNMATERIAL_EXPRESSION_H__

class UMaterialExpression : public UObject
{
	DECLARE_CLASS(UMaterialExpression, UObject)
	TYPE_FLAGS(TYPE_SilentLoad);
public:
	BEGIN_PROP_TABLE
		PROP_DROP(Material)
		PROP_DROP(Function)
		PROP_DROP(EditorX)
		PROP_DROP(EditorY)
		PROP_DROP(Desc)
	END_PROP_TABLE
};

class UMaterialExpressionTextureBase : public UMaterialExpression
{
	DECLARE_CLASS(UMaterialExpressionTextureBase, UMaterialExpression)
public:
	UTexture3* Texture;

	BEGIN_PROP_TABLE
		PROP_OBJ(Texture)
		PROP_DROP(SamplerType)
	END_PROP_TABLE
};

class UMaterialExpressionTextureSample : public UMaterialExpressionTextureBase
{
	DECLARE_CLASS(UMaterialExpressionTextureSample, UMaterialExpressionTextureBase)
public:
	BEGIN_PROP_TABLE
		PROP_DROP(Coordinates)
		PROP_DROP(SamplerSource)
	END_PROP_TABLE
};

class UMaterialExpressionTextureSampleParameter : public UMaterialExpressionTextureSample
{
	DECLARE_CLASS(UMaterialExpressionTextureSampleParameter, UMaterialExpressionTextureSample)
public:
	FName ParameterName;
	FName Group;

	BEGIN_PROP_TABLE
		PROP_NAME(ParameterName)
		PROP_NAME(Group)
		PROP_DROP(ExpressionGUID)
	END_PROP_TABLE
};

class UMaterialExpressionTextureSampleParameter2D : public UMaterialExpressionTextureSampleParameter
{
	DECLARE_CLASS(UMaterialExpressionTextureSampleParameter2D, UMaterialExpressionTextureSampleParameter)
};

class UMaterialExpressionParameter : public UMaterialExpression
{
	DECLARE_CLASS(UMaterialExpressionParameter, UMaterialExpression)
public:
	FName ParameterName;
	FName Group;

	BEGIN_PROP_TABLE
		PROP_NAME(ParameterName)
		PROP_NAME(Group)			// editoronly
		PROP_DROP(ExpressionGUID)
	END_PROP_TABLE
};

class UMaterialExpressionScalarParameter : public UMaterialExpressionParameter
{
	DECLARE_CLASS(UMaterialExpressionScalarParameter, UMaterialExpressionParameter)
public:
	float DefaultValue;

	BEGIN_PROP_TABLE
		PROP_FLOAT(DefaultValue)
	END_PROP_TABLE
};

class UMaterialExpressionStaticBoolParameter : public UMaterialExpressionParameter
{
	DECLARE_CLASS(UMaterialExpressionStaticBoolParameter, UMaterialExpressionParameter)
public:
	bool DefaultValue;

	BEGIN_PROP_TABLE
		PROP_BOOL(DefaultValue)
	END_PROP_TABLE
};

class UMaterialExpressionStaticSwitchParameter : public UMaterialExpressionStaticBoolParameter
{
	DECLARE_CLASS(UMaterialExpressionStaticSwitchParameter, UMaterialExpressionStaticBoolParameter)
public:
	bool DefaultValue;

	BEGIN_PROP_TABLE
		PROP_BOOL(DefaultValue)
		PROP_DROP(A)
		PROP_DROP(B)
	END_PROP_TABLE
};

class UMaterialExpressionVectorParameter : public UMaterialExpressionParameter
{
	DECLARE_CLASS(UMaterialExpressionVectorParameter, UMaterialExpressionParameter)
public:
	FLinearColor DefaultValue;

	BEGIN_PROP_TABLE
		PROP_STRUC(DefaultValue, FLinearColor)
	END_PROP_TABLE
};

#define REGISTER_EXPRESSION_CLASSES \
	REGISTER_CLASS(UMaterialExpressionTextureSample) \
	REGISTER_CLASS(UMaterialExpressionTextureSampleParameter2D) \
	REGISTER_CLASS(UMaterialExpressionScalarParameter) \
	REGISTER_CLASS(UMaterialExpressionStaticBoolParameter) \
	REGISTER_CLASS(UMaterialExpressionStaticSwitchParameter) \
	REGISTER_CLASS(UMaterialExpressionVectorParameter)

#endif // __UNMATERIAL_EXPRESSION_H__
