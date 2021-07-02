//DHK_BEGIN

#ifndef __UNLANDSCAPE_H__
#define __UNLANDSCAPE_H__
#include "UnrealMaterial/UnMaterial3.h"

#if UNREAL4

class USceneComponent : public UObject
{
	DECLARE_CLASS(USceneComponent, UObject);

public:
	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;

	BEGIN_PROP_TABLE
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		END_PROP_TABLE

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(USceneComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UPhysicalMaterial : public UObject
{
	DECLARE_CLASS(UPhysicalMaterial, UObject);

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(UPhysicalMaterial::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class ULandscapeGizmoActiveActor : public UObject
{
	DECLARE_CLASS(ULandscapeGizmoActiveActor, UObject);

public:
	USceneComponent* RootComponent;

	BEGIN_PROP_TABLE
		PROP_OBJ(RootComponent)
		END_PROP_TABLE

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(ULandscapeGizmoActiveActor::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class ULandscapeLayerInfoObject : public UObject
{
	DECLARE_CLASS(ULandscapeLayerInfoObject, UObject);

public:
	FName LayerName;
	FLinearColor LayerUsageDebugColor;

	BEGIN_PROP_TABLE
		PROP_NAME(LayerName)
		PROP_STRUC(LayerUsageDebugColor, FLinearColor)
		END_PROP_TABLE

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(ULandscapeLayerInfoObject::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		this->GetTypeinfo()->DumpProps(this);
		unguard;
	}
};

struct FWeightmapLayerAllocationInfo
{
	DECLARE_STRUCT(FWeightmapLayerAllocationInfo)

	ULandscapeLayerInfoObject* LayerInfo;
	byte WeightmapTextureIndex;
	byte WeightmapTextureChannel;

	BEGIN_PROP_TABLE
		PROP_OBJ(LayerInfo)
		PROP_BYTE(WeightmapTextureIndex)
		PROP_BYTE(WeightmapTextureChannel)
		END_PROP_TABLE

public:
	friend FArchive& operator<<(FArchive& Ar, FWeightmapLayerAllocationInfo& Info)
	{
		return Ar << Info.LayerInfo << Info.WeightmapTextureIndex << Info.WeightmapTextureChannel;
	}
};

class ULandscapeHeightfieldCollisionComponent : public UObject
{
	DECLARE_CLASS(ULandscapeHeightfieldCollisionComponent, UObject);

public:
	TArray<ULandscapeLayerInfoObject*> ComponentLayerInfos;
	int32 SectionBaseX;
	int32 SectionBaseY;
	FVector RelativeLocation;
	int32 CollisionSizeQuads;
	float CollisionScale;
	FGuid HeightfieldGuid;
	FBox CachedLocalBox;
	FGuid RenderComponent;//ULandscapeComponent Lazy
	TArray<UPhysicalMaterial*> CookedPhysicalMaterials;
	UObject* AttachParent;

	BEGIN_PROP_TABLE
		PROP_ARRAY(ComponentLayerInfos, PropType::UObject)
		PROP_INT(SectionBaseX)
		PROP_INT(SectionBaseY)
		PROP_VECTOR(RelativeLocation)
		PROP_INT(CollisionSizeQuads)
		PROP_FLOAT(CollisionScale)
		PROP_STRUC(HeightfieldGuid, FGuid)
		//PROP_STRUC(CachedLocalBox, FBox)
		PROP_STRUC(RenderComponent, FGuid)//TODO Lazy
		PROP_ARRAY(CookedPhysicalMaterials, PropType::UObject)
		PROP_OBJ(AttachParent)
		END_PROP_TABLE

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(ULandscapeHeightfieldCollisionComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class ULandscapeComponent : public UObject
{
	DECLARE_CLASS(ULandscapeComponent, UObject);

public:
	int32 ComponentSizeQuads;
	int32 SubsectionSizeQuads;
	int32 NumSubsections;
	int32 SectionBaseX;
	int32 SectionBaseY;
	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;
	TArray<UMaterialInstanceConstant*> MaterialInstances;
	TArray<byte> LODIndexToMaterialIndex;
	TArray<byte> MaterialIndexToDisabledTessellationMaterial;
	FVector4 WeightmapScaleBias;
	float WeightmapSubsectionOffset;
	FVector4 HeightmapScaleBias;
	FBox CachedLocalBox;
	FGuid CollisionComponent;//ULandscapeHeightfieldCollisionComponent Lazy
	UTexture2D* HeightmapTexture;
	TArray<FWeightmapLayerAllocationInfo> WeightmapLayerAllocations;
	TArray<UTexture2D*> WeightmapTextures;
	FGuid MapBuildDataId;
	FGuid StateId;
	int32 VisibilityId;
	int32 CollisionMipLevel;
	UObject* AttachParent;

	BEGIN_PROP_TABLE
		PROP_INT(ComponentSizeQuads)
		PROP_INT(SubsectionSizeQuads)
		PROP_INT(NumSubsections)
		PROP_INT(SectionBaseX)
		PROP_INT(SectionBaseY)
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		PROP_ARRAY(MaterialInstances, PropType::UObject)
		PROP_ARRAY(LODIndexToMaterialIndex, PropType::Byte)
		PROP_ARRAY(MaterialIndexToDisabledTessellationMaterial, PropType::Byte)
		PROP_STRUC(WeightmapScaleBias, FVector4)
		PROP_FLOAT(WeightmapSubsectionOffset)
		PROP_STRUC(HeightmapScaleBias, FVector4)
		//PROP_STRUC(CachedLocalBox, FBox)
		PROP_STRUC(CollisionComponent, FGuid)
		PROP_OBJ(HeightmapTexture)
		PROP_ARRAY(WeightmapLayerAllocations, "FWeightmapLayerAllocationInfo")
		PROP_ARRAY(WeightmapTextures, PropType::UObject)
		PROP_STRUC(MapBuildDataId, FGuid)
		PROP_STRUC(StateId, FGuid)
		PROP_INT(VisibilityId)
		PROP_INT(CollisionMipLevel)
		PROP_OBJ(AttachParent)
		END_PROP_TABLE

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(ULandscapeComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class ULandscape : public UObject
{
	DECLARE_CLASS(ULandscape, UObject);

public:
	FGuid LandscapeGuid;
	float LOD0ScreenSize;
	UMaterialInterface* LandscapeMaterial;
	TArray<ULandscapeComponent*> LandscapeComponents;
	TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;
	bool bHasLandscapeGrass;
	int32 ComponentSizeQuads;
	int32 SubsectionSizeQuads;
	int32 NumSubsections;
	USceneComponent* RootComponent;

	BEGIN_PROP_TABLE
		PROP_STRUC(LandscapeGuid, FGuid)
		PROP_FLOAT(LOD0ScreenSize)
		PROP_OBJ(LandscapeMaterial)
		PROP_ARRAY(LandscapeComponents, PropType::UObject)
		PROP_ARRAY(CollisionComponents, PropType::UObject)
		PROP_BOOL(bHasLandscapeGrass)
		PROP_INT(ComponentSizeQuads)
		PROP_INT(SubsectionSizeQuads)
		PROP_INT(NumSubsections)
		PROP_OBJ(RootComponent)
		END_PROP_TABLE

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(ULandscape::Serialize);
		Super::Serialize(Ar);
		unguard;
	}
};

struct FFloatInterval
{
	DECLARE_STRUCT(FFloatInterval)

	float Min;
	float Max;

	BEGIN_PROP_TABLE
		PROP_FLOAT(Min)
		PROP_FLOAT(Max)
		END_PROP_TABLE

public:
	friend FArchive& operator<<(FArchive& Ar, FFloatInterval& Interval)
	{
		return Ar << Interval.Min << Interval.Max;
	}
};

struct FStreamingTextureBuildInfo
{
	DECLARE_STRUCT(FStreamingTextureBuildInfo)

	int32 PackedRelativeBox;
	int32 TextureLevelIndex;
	float TexelFactor;

	BEGIN_PROP_TABLE
		PROP_INT(PackedRelativeBox)
		PROP_INT(TextureLevelIndex)
		PROP_FLOAT(TexelFactor)
		END_PROP_TABLE

public:
	friend FArchive& operator<<(FArchive& Ar, FStreamingTextureBuildInfo& Info)
	{
		return Ar << Info.PackedRelativeBox << Info.TextureLevelIndex << Info.TexelFactor;
	}
};

#define REGISTER_LANDSCAPE_CLASSES \
 REGISTER_CLASS(USceneComponent) \
 REGISTER_CLASS(UPhysicalMaterial) \
 REGISTER_CLASS_ALIAS(UMaterialInstanceConstant, ULandscapeMaterialInstanceConstant) \
 REGISTER_CLASS(ULandscapeGizmoActiveActor) \
 REGISTER_CLASS(ULandscapeLayerInfoObject) \
 REGISTER_CLASS(FWeightmapLayerAllocationInfo) \
 REGISTER_CLASS(ULandscapeHeightfieldCollisionComponent) \
 REGISTER_CLASS(ULandscapeComponent) \
 REGISTER_CLASS(ULandscape) 
#endif // UNREAL4

#endif // __UNLANDSCAPE_H__

//DHK_END