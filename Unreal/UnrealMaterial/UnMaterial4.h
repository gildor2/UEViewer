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

#define REGISTER_EXPRESSION_CLASSES_U4 \
	REGISTER_CLASS(UMaterialExpressionTextureSample) \
	REGISTER_CLASS(UMaterialExpressionTextureSampleParameter2D)

#endif // __UNMATERIAL4_H__
