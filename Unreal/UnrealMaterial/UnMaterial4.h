#ifndef __UNMATERIAL4_H__
#define __UNMATERIAL4_H__

class UMaterialExpression : public UObject
{
	DECLARE_CLASS(UMaterialExpression, UObject)
public:
	BEGIN_PROP_TABLE
		PROP_DROP(Material)
		PROP_DROP(Function)
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
	int DefaultValue;

	BEGIN_PROP_TABLE
		PROP_INT(DefaultValue)
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

#define REGISTER_EXPRESSION_CLASSES_U4 \
	REGISTER_CLASS(UMaterialExpressionTextureSample) \
	REGISTER_CLASS(UMaterialExpressionTextureSampleParameter2D) \
	REGISTER_CLASS(UMaterialExpressionScalarParameter) \
	REGISTER_CLASS(UMaterialExpressionStaticBoolParameter) \
	REGISTER_CLASS_ALIAS(UMaterialExpressionStaticBoolParameter, UMaterialExpressionStaticSwitchParameter) \
	REGISTER_CLASS(UMaterialExpressionVectorParameter)

#endif // __UNMATERIAL4_H__
