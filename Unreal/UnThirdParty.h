#ifndef __UNTHIRDPARTY_H__
#define __UNTHIRDPARTY_H__
#include "UnrealMesh/UnMesh4.h"


#if UNREAL3

/*-----------------------------------------------------------------------------
	SwfMovie (ScaleForm)
-----------------------------------------------------------------------------*/

class UGFxRawData : public UObject
{
	DECLARE_CLASS(UGFxRawData, UObject);
public:
	TArray<byte>		RawData;

	BEGIN_PROP_TABLE
		PROP_ARRAY(RawData, PropType::Byte)
		END_PROP_TABLE
};


class USwfMovie : public UGFxRawData
{
	DECLARE_CLASS(USwfMovie, UGFxRawData);
};


/*-----------------------------------------------------------------------------
	FaceFX
-----------------------------------------------------------------------------*/

class UFaceFXAnimSet : public UObject
{
	DECLARE_CLASS(UFaceFXAnimSet, UObject);
public:
	TArray<byte>		RawFaceFXAnimSetBytes;

	virtual void Serialize(FArchive& Ar)
	{
		guard(UFaceFXAnimSet::Serialize);
		Super::Serialize(Ar);
		Ar << RawFaceFXAnimSetBytes;
		// the next is RawFaceFXMiniSessionBytes
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};


class UFaceFXAsset : public UObject
{
	DECLARE_CLASS(UFaceFXAsset, UObject);
public:
	TArray<byte>		RawFaceFXActorBytes;

	virtual void Serialize(FArchive& Ar)
	{
		guard(UFaceFXAsset::Serialize);
		Super::Serialize(Ar);
		Ar << RawFaceFXActorBytes;
		// the next is RawFaceFXSessionBytes
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};


// UGFxMovieInfo is DC Universe Online USwfMovie analogue

#define REGISTER_3RDP_CLASSES3		\
	REGISTER_CLASS(USwfMovie)		\
	REGISTER_CLASS_ALIAS(USwfMovie, UGFxMovieInfo) \
	REGISTER_CLASS(UFaceFXAnimSet)	\
	REGISTER_CLASS(UFaceFXAsset)


#endif // UNREAL3

#if UNREAL4

//enums
enum ELightUnits //main
{
	Unitless,
	Candelas,
	Lumens,
};

_ENUM(ELightUnits) //name
{
	_E(Unitless),
		_E(Candelas),
		_E(Lumens),
};
//end

//test
class UBrushComponent : public UObject
{
	DECLARE_CLASS(UBrushComponent, UObject);
public:

	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;

	BEGIN_PROP_TABLE
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UBrushComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UAkAudioBank : public UObject
{
	DECLARE_CLASS(UAkAudioBank, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UAkAudioBank::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UAkAudioEvent : public UObject
{
	DECLARE_CLASS(UAkAudioEvent, UObject);
public:

	UAkAudioBank* RequiredBank;

	BEGIN_PROP_TABLE
		PROP_OBJ(RequiredBank)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UAkAudioEvent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UAkComponent : public UObject
{
	DECLARE_CLASS(UAkComponent, UObject);
public:

	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;
	UAkAudioEvent* AkAudioEvent;

	BEGIN_PROP_TABLE
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		PROP_OBJ(AkAudioEvent)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UAkComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UAkAmbientSound : public UObject
{
	DECLARE_CLASS(UAkAmbientSound, UObject);
public:

	UAkComponent* AkComponent;

	BEGIN_PROP_TABLE
		PROP_OBJ(AkComponent)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UAkAmbientSound::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UAudioVolume : public UObject
{
	DECLARE_CLASS(UAudioVolume, UObject);
public:

	UBrushComponent* BrushComponent;

	BEGIN_PROP_TABLE
		PROP_OBJ(BrushComponent)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UAudioVolume::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class USoundCue : public UObject
{
	DECLARE_CLASS(USoundCue, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(USoundCue::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UAudioComponent : public UObject
{
	DECLARE_CLASS(UAudioComponent, UObject);
public:

	USoundCue* Sound;
	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;

	BEGIN_PROP_TABLE
		PROP_OBJ(Sound)
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UAudioComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};


class UBodySetup : public UObject
{
	DECLARE_CLASS(UBodySetup, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UBodySetup::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UModel : public UObject
{
	DECLARE_CLASS(UModel, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UModel::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

//volumes
class ULightmassImportanceVolume : public UObject
{
	DECLARE_CLASS(ULightmassImportanceVolume, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(ULightmassImportanceVolume::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UPostProcessVolume : public UObject
{
	DECLARE_CLASS(UPostProcessVolume, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UPostProcessVolume::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UPrecomputedVisibilityVolume : public UObject
{
	DECLARE_CLASS(UPrecomputedVisibilityVolume, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UPrecomputedVisibilityVolume::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UEmitter : public UObject
{
	DECLARE_CLASS(UEmitter, UObject);
public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(UEmitter::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UParticleSystem : public UObject
{
	DECLARE_CLASS(UParticleSystem, UObject);
public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(UParticleSystem::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UParticleSystemComponent : public UObject
{
	DECLARE_CLASS(UParticleSystemComponent, UObject);
public:

	UParticleSystem* Template;
	FVector OldPosition;
	FVector RelativeLocation;

	BEGIN_PROP_TABLE
		PROP_OBJ(Template)
		PROP_VECTOR(OldPosition)
		PROP_VECTOR(RelativeLocation)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UParticleSystemComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UNiagaraComponent : public UObject
{
	DECLARE_CLASS(UNiagaraComponent, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UNiagaraComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UDecalComponent : public UObject
{
	DECLARE_CLASS(UDecalComponent, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UDecalComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UNiagaraActor : public UObject
{
	DECLARE_CLASS(UNiagaraActor, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UNiagaraActor::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

/*class USceneComponent : public UObject
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

		virtual void Serialize(FArchive& Ar)
	{
		guard(USceneComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};*/

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

//TODO
/*
	FBodyInstance
	RuntimeFloatCurve
*/

struct FFloatInterval
{
	DECLARE_STRUCT(FFloatInterval)

	float Min;
	float Max;

	BEGIN_PROP_TABLE
		PROP_FLOAT(Min)
		PROP_FLOAT(Max)
		END_PROP_TABLE

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

		friend FArchive& operator<<(FArchive& Ar, FStreamingTextureBuildInfo& Info)
	{
		return Ar << Info.PackedRelativeBox << Info.TextureLevelIndex << Info.TexelFactor;
	}
};

class USkeletalMeshComponent : public UObject
{
	DECLARE_CLASS(USkeletalMeshComponent, UObject);
public:

	USkeletalMesh4* SkeletalMesh;
	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;

	BEGIN_PROP_TABLE
		PROP_OBJ(SkeletalMesh)
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(USkeletalMeshComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

/*class USkeletalMeshActor : public UObject
{
	DECLARE_CLASS(USkeletalMeshActor, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(USkeletalMeshActor::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};*/

class UStaticMeshComponent : public UObject
{
	DECLARE_CLASS(UStaticMeshComponent, UObject);
public:

	UStaticMesh4* StaticMesh;
	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;

	BEGIN_PROP_TABLE
		PROP_OBJ(StaticMesh)
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UStaticMeshComponent::Serialize);
		//Ar.Seek(Ar.Tell() + 0x04);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);//0x04 bytes 0x00
		unguard;
	}
};

class UStaticMeshActor : public UObject
{
	DECLARE_CLASS(UStaticMeshActor, UObject);
public:

	UStaticMeshComponent* StaticMeshComponent;

	BEGIN_PROP_TABLE
		PROP_OBJ(StaticMeshComponent)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UStaticMeshActor::Serialize);
		Super::Serialize(Ar);
		unguard;
	}
};

class UWorldSettings : public UObject
{
	DECLARE_CLASS(UWorldSettings, UObject);
public:

	virtual void Serialize(FArchive& Ar)
	{
		guard(UWorldSettings::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class ULevel : public UObject
{
	DECLARE_CLASS(ULevel, UObject);
public:

	UWorldSettings* WorldSettings;

	BEGIN_PROP_TABLE
		PROP_OBJ(WorldSettings)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(ULevel::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class USphereReflectionCaptureComponent : public UObject
{
	DECLARE_CLASS(USphereReflectionCaptureComponent, UObject);
public:

	float InfluenceRadius;
	float Brightness;
	FVector RelativeLocation;

	BEGIN_PROP_TABLE
		PROP_FLOAT(InfluenceRadius)
		PROP_FLOAT(Brightness)
		PROP_VECTOR(RelativeLocation)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(USphereReflectionCaptureComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UPointLightComponent : public UObject
{
	DECLARE_CLASS(UPointLightComponent, UObject);
public:

	FVector RelativeLocation;
	FRotator RelativeRotation;
	FRotator RelativeScale3D;
	byte Mobility;
	ELightUnits IntensityUnits;
	FColor LightColor;
	float AttenuationRadius;
	bool CastShadows;
	float IndirectLightingIntensity;
	float VolumetricScatteringIntensity;

	BEGIN_PROP_TABLE
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		PROP_BYTE(Mobility)
		PROP_ENUM2(IntensityUnits, ELightUnits)
		PROP_COLOR(LightColor)
		PROP_FLOAT(AttenuationRadius)
		PROP_BOOL(CastShadows)
		PROP_FLOAT(IndirectLightingIntensity)
		PROP_FLOAT(VolumetricScatteringIntensity)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UPointLightComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class UPointLight : public UObject
{
	DECLARE_CLASS(UPointLight, UObject);
public:

	UPointLightComponent* PointLightComponent;

	BEGIN_PROP_TABLE
		PROP_OBJ(PointLightComponent)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(UPointLight::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class USpotLightComponent : public UObject
{
	DECLARE_CLASS(USpotLightComponent, UObject);
public:

	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;
	byte Mobility;
	float Intensity;
	FColor LightColor;
	float AttenuationRadius;
	float OuterConeAngle;
	float InnerConeAngle;
	float SourceRadius;
	float SourceLength;
	float Temperature;
	bool bUseTemperature;
	bool bAffectsWorld;
	bool CastShadows;
	ELightUnits IntensityUnits;
	bool bUseInverseSquaredFalloff;
	float LightFalloffExponent;
	bool bCastVolumetricShadow;
	float IndirectLightingIntensity;
	float VolumetricScatteringIntensity;
	float ShadowBias;

	BEGIN_PROP_TABLE
		PROP_VECTOR(RelativeLocation)
		PROP_ROTATOR(RelativeRotation)
		PROP_VECTOR(RelativeScale3D)
		PROP_BYTE(Mobility)
		PROP_FLOAT(Intensity)
		PROP_COLOR(LightColor)
		PROP_FLOAT(AttenuationRadius)
		PROP_FLOAT(OuterConeAngle)
		PROP_FLOAT(InnerConeAngle)
		PROP_FLOAT(SourceRadius)
		PROP_FLOAT(SourceLength)
		PROP_FLOAT(Temperature)
		PROP_BOOL(bUseTemperature)
		PROP_BOOL(bAffectsWorld)
		PROP_BOOL(CastShadows)
		PROP_ENUM2(IntensityUnits, ELightUnits)
		PROP_BOOL(bUseInverseSquaredFalloff)
		PROP_FLOAT(LightFalloffExponent)
		PROP_BOOL(bCastVolumetricShadow)
		PROP_FLOAT(IndirectLightingIntensity)
		PROP_FLOAT(VolumetricScatteringIntensity)
		PROP_FLOAT(ShadowBias)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(USpotLightComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class USpotLight : public UObject
{
	DECLARE_CLASS(USpotLight, UObject);
public:

	USpotLightComponent* SpotLightComponent;

	BEGIN_PROP_TABLE
		PROP_OBJ(SpotLightComponent)
		END_PROP_TABLE

		virtual void Serialize(FArchive& Ar)
	{
		guard(USpotLight::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

//deleted
/*
	SceneComponent
	SkeletalMeshActor
*/

#define REGISTER_3RDP_CLASSES4 \
 REGISTER_CLASS(UBrushComponent) \
 REGISTER_CLASS(UAkAudioBank) \
 REGISTER_CLASS(UAkAudioEvent) \
 REGISTER_CLASS(UAkComponent) \
 REGISTER_CLASS(UAkAmbientSound) \
 REGISTER_CLASS(UAudioVolume) \
 REGISTER_CLASS(USoundCue) \
 REGISTER_CLASS(UAudioComponent) \
 REGISTER_CLASS(UBodySetup) \
 REGISTER_CLASS(UModel) \
 REGISTER_CLASS(ULightmassImportanceVolume) \
 REGISTER_CLASS(UPostProcessVolume) \
 REGISTER_CLASS(UPrecomputedVisibilityVolume) \
 REGISTER_CLASS(UEmitter) \
 REGISTER_CLASS(UParticleSystem) \
 REGISTER_CLASS(UParticleSystemComponent) \
 REGISTER_CLASS(UNiagaraComponent) \
 REGISTER_CLASS(UNiagaraActor) \
 REGISTER_CLASS(UDecalComponent) \
 REGISTER_CLASS(UPhysicalMaterial) \
 REGISTER_CLASS(FFloatInterval) \
 REGISTER_CLASS(USkeletalMeshComponent) \
 REGISTER_CLASS(UStaticMeshComponent) \
 REGISTER_CLASS(UStaticMeshActor) \
 REGISTER_CLASS(UWorldSettings) \
 REGISTER_CLASS(ULevel) \
 REGISTER_CLASS(USphereReflectionCaptureComponent) \
 REGISTER_CLASS(UPointLightComponent) \
 REGISTER_CLASS(UPointLight) \
 REGISTER_CLASS(USpotLightComponent) \
 REGISTER_CLASS(USpotLight) \
 REGISTER_CLASS(FStreamingTextureBuildInfo)

#define REGISTER_LIGHT_ENUMS_U4 \
	REGISTER_ENUM(ELightUnits)

#endif // UNREAL4


#endif // __UNTHIRDPARTY_H__
