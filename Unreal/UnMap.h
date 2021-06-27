//DHK

#ifndef __UNMAP_H__
#define __UNMAP_H__
#include "UnrealMesh/UnMesh4.h"
#include "UnCore.h"

#if UNREAL3

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

enum ESpawnActorCollisionHandlingMethod
{
	Undefined,
	AlwaysSpawn,
	AdjustIfPossibleButAlwaysSpawn,
	AdjustIfPossibleButDontSpawnIfColliding,
	DontSpawnIfColliding
};

_ENUM(ESpawnActorCollisionHandlingMethod)
{
	_E(Undefined),
		_E(AlwaysSpawn),
		_E(AdjustIfPossibleButAlwaysSpawn),
		_E(AdjustIfPossibleButDontSpawnIfColliding),
		_E(DontSpawnIfColliding),
};
//end

//TODO scene=brush component

class UBrush : public UObject
{
	DECLARE_CLASS(UBrush, UObject);

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(UBrush::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

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

public:
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

public:
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

public:
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

public:
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

public:
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

public:
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

struct FPostProcessSettings
{
	DECLARE_STRUCT(FPostProcessSettings)

	UTexture* LensFlareBokehShape;
	UTextureCube4* AmbientCubemap;

	FVector4 ColorSaturation;
	FVector4 ColorContrast;
	FVector4 ColorGamma;
	FVector4 ColorGain;
	FVector4 ColorOffset;
	FVector4 ColorSaturationShadows;
	FVector4 ColorContrastShadows;
	FVector4 ColorGammaShadows;
	FVector4 ColorGainShadows;
	FVector4 ColorOffsetShadows;
	FVector4 ColorSaturationMidtones;
	FVector4 ColorContrastMidtones;
	FVector4 ColorGammaMidtones;
	FVector4 ColorGainMidtones;
	FVector4 ColorOffsetMidtones;
	FVector4 ColorSaturationHighlights;
	FVector4 ColorContrastHighlights;
	FVector4 ColorGammaHighlights;
	FVector4 ColorGainHighlights;
	FVector4 ColorOffsetHighlights;

	bool bOverride_WhiteTemp;
	bool bOverride_WhiteTint;
	bool bOverride_ColorSaturation;
	bool bOverride_ColorContrast;
	bool bOverride_ColorGamma;
	bool bOverride_ColorGain;
	bool bOverride_ColorOffset;
	bool bOverride_ColorSaturationShadows;
	bool bOverride_ColorContrastShadows;
	bool bOverride_ColorGammaShadows;
	bool bOverride_ColorGainShadows;
	bool bOverride_ColorOffsetShadows;
	bool bOverride_ColorSaturationMidtones;
	bool bOverride_ColorContrastMidtones;
	bool bOverride_ColorGammaMidtones;
	bool bOverride_ColorGainMidtones;
	bool bOverride_ColorOffsetMidtones;
	bool bOverride_ColorSaturationHighlights;
	bool bOverride_ColorContrastHighlights;
	bool bOverride_ColorGammaHighlights;
	bool bOverride_ColorGainHighlights;
	bool bOverride_ColorOffsetHighlights;
	bool bOverride_ColorCorrectionShadowsMax;
	bool bOverride_ColorCorrectionHighlightsMin;
	bool bOverride_BlueCorrection;
	bool bOverride_ExpandGamut;
	bool bOverride_ToneCurveAmount;
	bool bOverride_FilmWhitePoint;
	bool bOverride_FilmSaturation;
	bool bOverride_FilmChannelMixerRed;
	bool bOverride_FilmChannelMixerGreen;
	bool bOverride_FilmChannelMixerBlue;
	bool bOverride_FilmContrast;
	bool bOverride_FilmDynamicRange;
	bool bOverride_FilmHealAmount;
	bool bOverride_FilmToeAmount;
	bool bOverride_FilmShadowTint;
	bool bOverride_FilmShadowTintBlend;
	bool bOverride_FilmShadowTintAmount;
	bool bOverride_FilmSlope;
	bool bOverride_FilmToe;
	bool bOverride_FilmShoulder;
	bool bOverride_FilmBlackClip;
	bool bOverride_FilmWhiteClip;
	bool bOverride_SceneColorTint;
	bool bOverride_SceneFringeIntensity;
	bool bOverride_ChromaticAberrationStartOffset;
	bool bOverride_AmbientCubemapTint;
	bool bOverride_AmbientCubemapIntensity;
	bool bOverride_BloomMethod;
	bool bOverride_BloomIntensity;
	bool bOverride_BloomThreshold;
	bool bOverride_Bloom1Tint;
	bool bOverride_Bloom1Size;
	bool bOverride_Bloom2Size;
	bool bOverride_Bloom2Tint;
	bool bOverride_Bloom3Tint;
	bool bOverride_Bloom3Size;
	bool bOverride_Bloom4Tint;
	bool bOverride_Bloom4Size;
	bool bOverride_Bloom5Tint;
	bool bOverride_Bloom5Size;
	bool bOverride_Bloom6Tint;
	bool bOverride_Bloom6Size;
	bool bOverride_BloomSizeScale;
	bool bOverride_BloomConvolutionTexture;
	bool bOverride_BloomConvolutionSize;
	bool bOverride_BloomConvolutionCenterUV;
	bool bOverride_BloomConvolutionPreFilter_DEPRECATED;
	bool bOverride_BloomConvolutionPreFilterMin;
	bool bOverride_BloomConvolutionPreFilterMax;
	bool bOverride_BloomConvolutionPreFilterMult;
	bool bOverride_BloomConvolutionBufferScale;
	bool bOverride_BloomDirtMaskIntensity;
	bool bOverride_BloomDirtMaskTint;
	bool bOverride_BloomDirtMask;
	bool bOverride_CameraShutterSpeed;
	bool bOverride_CameraISO;
	bool bOverride_AutoExposureMethod;
	bool bOverride_AutoExposureLowPercent;
	bool bOverride_AutoExposureHighPercent;
	bool bOverride_AutoExposureMinBrightness;
	bool bOverride_AutoExposureMaxBrightness;
	bool bOverride_AutoExposureCalibrationConstant_DEPRECATED;
	bool bOverride_AutoExposureSpeedUp;
	bool bOverride_AutoExposureSpeedDown;
	bool bOverride_AutoExposureBias;
	bool bOverride_AutoExposureBiasCurve;
	bool bOverride_AutoExposureMeterMask;
	bool bOverride_AutoExposureApplyPhysicalCameraExposure;
	bool bOverride_HistogramLogMin;
	bool bOverride_HistogramLogMax;
	bool bOverride_LensFlareIntensity;
	bool bOverride_LensFlareTint;
	bool bOverride_LensFlareTints;
	bool bOverride_LensFlareBokehSize;
	bool bOverride_LensFlareBokehShape;
	bool bOverride_LensFlareThreshold;
	bool bOverride_VignetteIntensity;
	bool bOverride_GrainIntensity;
	bool bOverride_GrainJitter;
	bool bOverride_AmbientOcclusionIntensity;
	bool bOverride_AmbientOcclusionStaticFraction;
	bool bOverride_AmbientOcclusionRadius;
	bool bOverride_AmbientOcclusionFadeDistance;
	bool bOverride_AmbientOcclusionFadeRadius;
	bool bOverride_AmbientOcclusionDistance_DEPRECATED;
	bool bOverride_AmbientOcclusionRadiusInWS;
	bool bOverride_AmbientOcclusionPower;
	bool bOverride_AmbientOcclusionBias;
	bool bOverride_AmbientOcclusionQuality;
	bool bOverride_AmbientOcclusionMipBlend;
	bool bOverride_AmbientOcclusionMipScale;
	bool bOverride_AmbientOcclusionMipThreshold;
	bool bOverride_AmbientOcclusionTemporalBlendWeight;
	bool bOverride_AnamorphicAspectRatio;
	bool bOverride_IndirectLightingIntensity;
	bool bOverride_ScreenSpaceReflectionIntensity;
	bool bOverride_ScreenSpaceReflectionQuality;
	bool bOverride_ScreenSpaceReflectionMaxRoughness;
	bool bOverride_DepthOfFieldFocalDistance;
	bool bOverride_DepthOfFieldDepthBlurRadius;
	bool bOverride_DepthOfFieldDepthBlurAmount;
	bool bOverride_DepthOfFieldMethod;
	bool bOverride_MobileHQGaussian;
	bool bOverride_DepthOfFieldFstop;
	bool bOverride_AutoExposureCalibrationConstant;
	bool bOverride_DepthOfFieldFocalRegion;
	bool bOverride_DepthOfFieldNearTransitionRegion;
	bool bOverride_DepthOfFieldFarTransitionRegion;
	bool bOverride_DepthOfFieldScale;
	bool bOverride_DepthOfFieldMaxBokehSize;
	bool bOverride_DepthOfFieldNearBlurSize;
	bool bOverride_DepthOfFieldFarBlurSize;
	bool bOverride_MotionBlurAmount;
	bool bOverride_MotionBlurMax;
	bool bOverride_MotionBlurPerObjectSize;

	float HistogramLogMin;
	float HistogramLogMax;
	float AutoExposureCalibrationConstant;
	float LensFlareThreshold;
	float GrainJitter;
	float DepthOfFieldScale;
	float DepthOfFieldMaxBokehSize;
	float DepthOfFieldFstop;
	float FilmToe;
	float BloomSizeScale;
	float AutoExposureBias;
	float AmbientOcclusionIntensity;
	float AmbientOcclusionRadius;
	float IndirectLightingIntensity;
	float ScreenSpaceReflectionIntensity;
	float ScreenSpaceReflectionQuality;
	float BloomIntensity;
	float AutoExposureMinBrightness;
	float AutoExposureMaxBrightness;
	float AutoExposureSpeedUp;
	float AutoExposureSpeedDown;
	float LensFlareIntensity;
	float LensFlareBokehSize;
	float SceneFringeIntensity;
	float ChromaticAberrationStartOffset;
	float VignetteIntensity;
	float FilmSlope;
	float AmbientCubemapIntensity;
	float DepthOfFieldFocalDistance;
	float DepthOfFieldDepthBlurRadius;
	float DepthOfFieldDepthBlurAmount;

	BEGIN_PROP_TABLE
		PROP_OBJ(LensFlareBokehShape)
		PROP_OBJ(AmbientCubemap)
		PROP_STRUC(ColorSaturation, FVector4)
		PROP_STRUC(ColorContrast, FVector4)
		PROP_STRUC(ColorGamma, FVector4)
		PROP_STRUC(ColorGain, FVector4)
		PROP_STRUC(ColorOffset, FVector4)
		PROP_STRUC(ColorSaturationShadows, FVector4)
		PROP_STRUC(ColorContrastShadows, FVector4)
		PROP_STRUC(ColorGammaShadows, FVector4)
		PROP_STRUC(ColorGainShadows, FVector4)
		PROP_STRUC(ColorOffsetShadows, FVector4)
		PROP_STRUC(ColorSaturationMidtones, FVector4)
		PROP_STRUC(ColorContrastMidtones, FVector4)
		PROP_STRUC(ColorGammaMidtones, FVector4)
		PROP_STRUC(ColorGainMidtones, FVector4)
		PROP_STRUC(ColorOffsetMidtones, FVector4)
		PROP_STRUC(ColorSaturationHighlights, FVector4)
		PROP_STRUC(ColorContrastHighlights, FVector4)
		PROP_STRUC(ColorGammaHighlights, FVector4)
		PROP_STRUC(ColorGainHighlights, FVector4)
		PROP_STRUC(ColorOffsetHighlights, FVector4)
		PROP_BOOL(bOverride_DepthOfFieldFstop)
		PROP_BOOL(bOverride_DepthOfFieldFocalDistance)
		PROP_BOOL(bOverride_DepthOfFieldDepthBlurRadius)
		PROP_BOOL(bOverride_DepthOfFieldDepthBlurAmount)
		PROP_BOOL(bOverride_DepthOfFieldMethod)
		PROP_BOOL(bOverride_MobileHQGaussian)
		PROP_BOOL(bOverride_IndirectLightingIntensity)
		PROP_BOOL(bOverride_ScreenSpaceReflectionIntensity)
		PROP_BOOL(bOverride_ScreenSpaceReflectionQuality)
		PROP_BOOL(bOverride_ScreenSpaceReflectionMaxRoughness)
		PROP_BOOL(bOverride_WhiteTemp)
		PROP_BOOL(bOverride_WhiteTint)
		PROP_BOOL(bOverride_ColorSaturation)
		PROP_BOOL(bOverride_ColorContrast)
		PROP_BOOL(bOverride_ColorGamma)
		PROP_BOOL(bOverride_ColorGain)
		PROP_BOOL(bOverride_ColorOffset)
		PROP_BOOL(bOverride_ColorSaturationShadows)
		PROP_BOOL(bOverride_ColorContrastShadows)
		PROP_BOOL(bOverride_ColorGammaShadows)
		PROP_BOOL(bOverride_ColorGainShadows)
		PROP_BOOL(bOverride_ColorOffsetShadows)
		PROP_BOOL(bOverride_ColorSaturationMidtones)
		PROP_BOOL(bOverride_ColorContrastMidtones)
		PROP_BOOL(bOverride_ColorGammaMidtones)
		PROP_BOOL(bOverride_ColorGainMidtones)
		PROP_BOOL(bOverride_ColorOffsetMidtones)
		PROP_BOOL(bOverride_ColorSaturationHighlights)
		PROP_BOOL(bOverride_ColorContrastHighlights)
		PROP_BOOL(bOverride_ColorGammaHighlights)
		PROP_BOOL(bOverride_ColorGainHighlights)
		PROP_BOOL(bOverride_ColorOffsetHighlights)
		PROP_BOOL(bOverride_ColorCorrectionShadowsMax)
		PROP_BOOL(bOverride_ColorCorrectionHighlightsMin)
		PROP_BOOL(bOverride_BlueCorrection)
		PROP_BOOL(bOverride_ExpandGamut)
		PROP_BOOL(bOverride_ToneCurveAmount)
		PROP_BOOL(bOverride_FilmWhitePoint)
		PROP_BOOL(bOverride_FilmSaturation)
		PROP_BOOL(bOverride_FilmChannelMixerRed)
		PROP_BOOL(bOverride_FilmChannelMixerGreen)
		PROP_BOOL(bOverride_FilmChannelMixerBlue)
		PROP_BOOL(bOverride_FilmContrast)
		PROP_BOOL(bOverride_FilmDynamicRange)
		PROP_BOOL(bOverride_FilmHealAmount)
		PROP_BOOL(bOverride_FilmToeAmount)
		PROP_BOOL(bOverride_FilmShadowTint)
		PROP_BOOL(bOverride_FilmShadowTintBlend)
		PROP_BOOL(bOverride_FilmShadowTintAmount)
		PROP_BOOL(bOverride_FilmSlope)
		PROP_BOOL(bOverride_FilmToe)
		PROP_BOOL(bOverride_FilmShoulder)
		PROP_BOOL(bOverride_FilmBlackClip)
		PROP_BOOL(bOverride_FilmWhiteClip)
		PROP_BOOL(bOverride_SceneColorTint)
		PROP_BOOL(bOverride_SceneFringeIntensity)
		PROP_BOOL(bOverride_ChromaticAberrationStartOffset)
		PROP_BOOL(bOverride_AmbientCubemapTint)
		PROP_BOOL(bOverride_AmbientCubemapIntensity)
		PROP_BOOL(bOverride_BloomMethod)
		PROP_BOOL(bOverride_BloomIntensity)
		PROP_BOOL(bOverride_BloomThreshold)
		PROP_BOOL(bOverride_Bloom1Tint)
		PROP_BOOL(bOverride_Bloom1Size)
		PROP_BOOL(bOverride_Bloom2Size)
		PROP_BOOL(bOverride_Bloom2Tint)
		PROP_BOOL(bOverride_Bloom3Tint)
		PROP_BOOL(bOverride_Bloom3Size)
		PROP_BOOL(bOverride_Bloom4Tint)
		PROP_BOOL(bOverride_Bloom4Size)
		PROP_BOOL(bOverride_Bloom5Tint)
		PROP_BOOL(bOverride_Bloom5Size)
		PROP_BOOL(bOverride_Bloom6Tint)
		PROP_BOOL(bOverride_Bloom6Size)
		PROP_BOOL(bOverride_BloomSizeScale)
		PROP_BOOL(bOverride_BloomConvolutionTexture)
		PROP_BOOL(bOverride_BloomConvolutionSize)
		PROP_BOOL(bOverride_BloomConvolutionCenterUV)
		PROP_BOOL(bOverride_BloomConvolutionPreFilter_DEPRECATED)
		PROP_BOOL(bOverride_BloomConvolutionPreFilterMin)
		PROP_BOOL(bOverride_BloomConvolutionPreFilterMax)
		PROP_BOOL(bOverride_BloomConvolutionPreFilterMult)
		PROP_BOOL(bOverride_BloomConvolutionBufferScale)
		PROP_BOOL(bOverride_BloomDirtMaskIntensity)
		PROP_BOOL(bOverride_BloomDirtMaskTint)
		PROP_BOOL(bOverride_BloomDirtMask)
		PROP_BOOL(bOverride_CameraShutterSpeed)
		PROP_BOOL(bOverride_CameraISO)
		PROP_BOOL(bOverride_AutoExposureMethod)
		PROP_BOOL(bOverride_AutoExposureLowPercent)
		PROP_BOOL(bOverride_AutoExposureHighPercent)
		PROP_BOOL(bOverride_AutoExposureMinBrightness)
		PROP_BOOL(bOverride_AutoExposureMaxBrightness)
		PROP_BOOL(bOverride_AutoExposureCalibrationConstant_DEPRECATED)
		PROP_BOOL(bOverride_AutoExposureSpeedUp)
		PROP_BOOL(bOverride_AutoExposureSpeedDown)
		PROP_BOOL(bOverride_AutoExposureBias)
		PROP_BOOL(bOverride_AutoExposureBiasCurve)
		PROP_BOOL(bOverride_AutoExposureMeterMask)
		PROP_BOOL(bOverride_AutoExposureApplyPhysicalCameraExposure)
		PROP_BOOL(bOverride_HistogramLogMin)
		PROP_BOOL(bOverride_HistogramLogMax)
		PROP_BOOL(bOverride_LensFlareIntensity)
		PROP_BOOL(bOverride_LensFlareTint)
		PROP_BOOL(bOverride_LensFlareTints)
		PROP_BOOL(bOverride_LensFlareBokehSize)
		PROP_BOOL(bOverride_LensFlareBokehShape)
		PROP_BOOL(bOverride_LensFlareThreshold)
		PROP_BOOL(bOverride_VignetteIntensity)
		PROP_BOOL(bOverride_GrainIntensity)
		PROP_BOOL(bOverride_GrainJitter)
		PROP_BOOL(bOverride_AmbientOcclusionIntensity)
		PROP_BOOL(bOverride_AmbientOcclusionStaticFraction)
		PROP_BOOL(bOverride_AmbientOcclusionRadius)
		PROP_BOOL(bOverride_AmbientOcclusionFadeDistance)
		PROP_BOOL(bOverride_AmbientOcclusionFadeRadius)
		PROP_BOOL(bOverride_AmbientOcclusionDistance_DEPRECATED)
		PROP_BOOL(bOverride_AmbientOcclusionRadiusInWS)
		PROP_BOOL(bOverride_AmbientOcclusionPower)
		PROP_BOOL(bOverride_AmbientOcclusionBias)
		PROP_BOOL(bOverride_AmbientOcclusionQuality)
		PROP_BOOL(bOverride_AmbientOcclusionMipBlend)
		PROP_BOOL(bOverride_AmbientOcclusionMipScale)
		PROP_BOOL(bOverride_AmbientOcclusionMipThreshold)
		PROP_BOOL(bOverride_AmbientOcclusionTemporalBlendWeight)
		PROP_BOOL(bOverride_AnamorphicAspectRatio)
		PROP_BOOL(bOverride_AutoExposureCalibrationConstant)
		PROP_BOOL(bOverride_DepthOfFieldFocalRegion)
		PROP_BOOL(bOverride_DepthOfFieldNearTransitionRegion)
		PROP_BOOL(bOverride_DepthOfFieldFarTransitionRegion)
		PROP_BOOL(bOverride_DepthOfFieldScale)
		PROP_BOOL(bOverride_DepthOfFieldMaxBokehSize)
		PROP_BOOL(bOverride_DepthOfFieldNearBlurSize)
		PROP_BOOL(bOverride_DepthOfFieldFarBlurSize)
		PROP_BOOL(bOverride_MotionBlurAmount)
		PROP_BOOL(bOverride_MotionBlurMax)
		PROP_BOOL(bOverride_MotionBlurPerObjectSize)
		PROP_FLOAT(HistogramLogMin)
		PROP_FLOAT(HistogramLogMax)
		PROP_FLOAT(AutoExposureCalibrationConstant)
		PROP_FLOAT(LensFlareThreshold)
		PROP_FLOAT(GrainJitter)
		PROP_FLOAT(DepthOfFieldScale)
		PROP_FLOAT(DepthOfFieldMaxBokehSize)
		PROP_FLOAT(FilmSlope)
		PROP_FLOAT(AmbientCubemapIntensity)
		PROP_FLOAT(DepthOfFieldFocalDistance)
		PROP_FLOAT(DepthOfFieldDepthBlurRadius)
		PROP_FLOAT(SceneFringeIntensity)
		PROP_FLOAT(ChromaticAberrationStartOffset)
		PROP_FLOAT(VignetteIntensity)
		PROP_FLOAT(FilmToe)
		PROP_FLOAT(BloomSizeScale)
		PROP_FLOAT(AutoExposureBias)
		PROP_FLOAT(AmbientOcclusionIntensity)
		PROP_FLOAT(AmbientOcclusionRadius)
		PROP_FLOAT(IndirectLightingIntensity)
		PROP_FLOAT(ScreenSpaceReflectionIntensity)
		PROP_FLOAT(ScreenSpaceReflectionQuality)
		PROP_FLOAT(BloomIntensity)
		PROP_FLOAT(AutoExposureMinBrightness)
		PROP_FLOAT(AutoExposureMaxBrightness)
		PROP_FLOAT(AutoExposureSpeedUp)
		PROP_FLOAT(AutoExposureSpeedDown)
		PROP_FLOAT(LensFlareIntensity)
		PROP_FLOAT(LensFlareBokehSize)
		PROP_FLOAT(BloomIntensity)
		PROP_FLOAT(AutoExposureMinBrightness)
		PROP_FLOAT(AutoExposureMaxBrightness)
		PROP_FLOAT(AutoExposureSpeedUp)
		PROP_FLOAT(AutoExposureSpeedDown)
		PROP_FLOAT(LensFlareIntensity)
		PROP_FLOAT(LensFlareBokehSize)
		PROP_FLOAT(DepthOfFieldFstop)
		PROP_FLOAT(DepthOfFieldDepthBlurAmount)
		END_PROP_TABLE

public:
	friend FArchive& operator<<(FArchive& Ar, FPostProcessSettings& Settings)
	{
		return Ar
			<< Settings.LensFlareBokehShape
			<< Settings.AmbientCubemap
			<< Settings.ColorSaturation
			<< Settings.ColorContrast
			<< Settings.ColorGamma
			<< Settings.ColorGain
			<< Settings.ColorOffset
			<< Settings.ColorSaturationShadows
			<< Settings.ColorContrastShadows
			<< Settings.ColorGammaShadows
			<< Settings.ColorGainShadows
			<< Settings.ColorOffsetShadows
			<< Settings.ColorSaturationMidtones
			<< Settings.ColorContrastMidtones
			<< Settings.ColorGammaMidtones
			<< Settings.ColorGainMidtones
			<< Settings.ColorOffsetMidtones
			<< Settings.ColorSaturationHighlights
			<< Settings.ColorContrastHighlights
			<< Settings.ColorGammaHighlights
			<< Settings.ColorGainHighlights
			<< Settings.ColorOffsetHighlights
			<< Settings.bOverride_DepthOfFieldFstop
			<< Settings.bOverride_DepthOfFieldFocalDistance
			<< Settings.bOverride_DepthOfFieldDepthBlurRadius
			<< Settings.bOverride_DepthOfFieldDepthBlurAmount
			<< Settings.bOverride_DepthOfFieldMethod
			<< Settings.bOverride_MobileHQGaussian
			<< Settings.bOverride_IndirectLightingIntensity
			<< Settings.bOverride_ScreenSpaceReflectionIntensity
			<< Settings.bOverride_ScreenSpaceReflectionQuality
			<< Settings.bOverride_ScreenSpaceReflectionMaxRoughness
			<< Settings.bOverride_WhiteTemp
			<< Settings.bOverride_WhiteTint
			<< Settings.bOverride_ColorSaturation
			<< Settings.bOverride_ColorContrast
			<< Settings.bOverride_ColorGamma
			<< Settings.bOverride_ColorGain
			<< Settings.bOverride_ColorOffset
			<< Settings.bOverride_ColorSaturationShadows
			<< Settings.bOverride_ColorContrastShadows
			<< Settings.bOverride_ColorGammaShadows
			<< Settings.bOverride_ColorGainShadows
			<< Settings.bOverride_ColorOffsetShadows
			<< Settings.bOverride_ColorSaturationMidtones
			<< Settings.bOverride_ColorContrastMidtones
			<< Settings.bOverride_ColorGammaMidtones
			<< Settings.bOverride_ColorGainMidtones
			<< Settings.bOverride_ColorOffsetMidtones
			<< Settings.bOverride_ColorSaturationHighlights
			<< Settings.bOverride_ColorContrastHighlights
			<< Settings.bOverride_ColorGammaHighlights
			<< Settings.bOverride_ColorGainHighlights
			<< Settings.bOverride_ColorOffsetHighlights
			<< Settings.bOverride_ColorCorrectionShadowsMax
			<< Settings.bOverride_ColorCorrectionHighlightsMin
			<< Settings.bOverride_BlueCorrection
			<< Settings.bOverride_ExpandGamut
			<< Settings.bOverride_ToneCurveAmount
			<< Settings.bOverride_FilmWhitePoint
			<< Settings.bOverride_FilmSaturation
			<< Settings.bOverride_FilmChannelMixerRed
			<< Settings.bOverride_FilmChannelMixerGreen
			<< Settings.bOverride_FilmChannelMixerBlue
			<< Settings.bOverride_FilmContrast
			<< Settings.bOverride_FilmDynamicRange
			<< Settings.bOverride_FilmHealAmount
			<< Settings.bOverride_FilmToeAmount
			<< Settings.bOverride_FilmShadowTint
			<< Settings.bOverride_FilmShadowTintBlend
			<< Settings.bOverride_FilmShadowTintAmount
			<< Settings.bOverride_FilmSlope
			<< Settings.bOverride_FilmToe
			<< Settings.bOverride_FilmShoulder
			<< Settings.bOverride_FilmBlackClip
			<< Settings.bOverride_FilmWhiteClip
			<< Settings.bOverride_SceneColorTint
			<< Settings.bOverride_SceneFringeIntensity
			<< Settings.bOverride_ChromaticAberrationStartOffset
			<< Settings.bOverride_AmbientCubemapTint
			<< Settings.bOverride_AmbientCubemapIntensity
			<< Settings.bOverride_BloomMethod
			<< Settings.bOverride_BloomIntensity
			<< Settings.bOverride_BloomThreshold
			<< Settings.bOverride_Bloom1Tint
			<< Settings.bOverride_Bloom1Size
			<< Settings.bOverride_Bloom2Size
			<< Settings.bOverride_Bloom2Tint
			<< Settings.bOverride_Bloom3Tint
			<< Settings.bOverride_Bloom3Size
			<< Settings.bOverride_Bloom4Tint
			<< Settings.bOverride_Bloom4Size
			<< Settings.bOverride_Bloom5Tint
			<< Settings.bOverride_Bloom5Size
			<< Settings.bOverride_Bloom6Tint
			<< Settings.bOverride_Bloom6Size
			<< Settings.bOverride_BloomSizeScale
			<< Settings.bOverride_BloomConvolutionTexture
			<< Settings.bOverride_BloomConvolutionSize
			<< Settings.bOverride_BloomConvolutionCenterUV
			<< Settings.bOverride_BloomConvolutionPreFilter_DEPRECATED
			<< Settings.bOverride_BloomConvolutionPreFilterMin
			<< Settings.bOverride_BloomConvolutionPreFilterMax
			<< Settings.bOverride_BloomConvolutionPreFilterMult
			<< Settings.bOverride_BloomConvolutionBufferScale
			<< Settings.bOverride_BloomDirtMaskIntensity
			<< Settings.bOverride_BloomDirtMaskTint
			<< Settings.bOverride_BloomDirtMask
			<< Settings.bOverride_CameraShutterSpeed
			<< Settings.bOverride_CameraISO
			<< Settings.bOverride_AutoExposureMethod
			<< Settings.bOverride_AutoExposureLowPercent
			<< Settings.bOverride_AutoExposureHighPercent
			<< Settings.bOverride_AutoExposureMinBrightness
			<< Settings.bOverride_AutoExposureMaxBrightness
			<< Settings.bOverride_AutoExposureCalibrationConstant_DEPRECATED
			<< Settings.bOverride_AutoExposureSpeedUp
			<< Settings.bOverride_AutoExposureSpeedDown
			<< Settings.bOverride_AutoExposureBias
			<< Settings.bOverride_AutoExposureBiasCurve
			<< Settings.bOverride_AutoExposureMeterMask
			<< Settings.bOverride_AutoExposureApplyPhysicalCameraExposure
			<< Settings.bOverride_HistogramLogMin
			<< Settings.bOverride_HistogramLogMax
			<< Settings.bOverride_LensFlareIntensity
			<< Settings.bOverride_LensFlareTint
			<< Settings.bOverride_LensFlareTints
			<< Settings.bOverride_LensFlareBokehSize
			<< Settings.bOverride_LensFlareBokehShape
			<< Settings.bOverride_LensFlareThreshold
			<< Settings.bOverride_VignetteIntensity
			<< Settings.bOverride_GrainIntensity
			<< Settings.bOverride_GrainJitter
			<< Settings.bOverride_AmbientOcclusionIntensity
			<< Settings.bOverride_AmbientOcclusionStaticFraction
			<< Settings.bOverride_AmbientOcclusionRadius
			<< Settings.bOverride_AmbientOcclusionFadeDistance
			<< Settings.bOverride_AmbientOcclusionFadeRadius
			<< Settings.bOverride_AmbientOcclusionDistance_DEPRECATED
			<< Settings.bOverride_AmbientOcclusionRadiusInWS
			<< Settings.bOverride_AmbientOcclusionPower
			<< Settings.bOverride_AmbientOcclusionBias
			<< Settings.bOverride_AmbientOcclusionQuality
			<< Settings.bOverride_AmbientOcclusionMipBlend
			<< Settings.bOverride_AmbientOcclusionMipScale
			<< Settings.bOverride_AmbientOcclusionMipThreshold
			<< Settings.bOverride_AmbientOcclusionTemporalBlendWeight
			<< Settings.bOverride_AnamorphicAspectRatio
			<< Settings.bOverride_AutoExposureCalibrationConstant
			<< Settings.bOverride_DepthOfFieldFocalRegion
			<< Settings.bOverride_DepthOfFieldNearTransitionRegion
			<< Settings.bOverride_DepthOfFieldFarTransitionRegion
			<< Settings.bOverride_DepthOfFieldScale
			<< Settings.bOverride_DepthOfFieldMaxBokehSize
			<< Settings.bOverride_DepthOfFieldNearBlurSize
			<< Settings.bOverride_DepthOfFieldFarBlurSize
			<< Settings.bOverride_MotionBlurAmount
			<< Settings.bOverride_MotionBlurMax
			<< Settings.bOverride_MotionBlurPerObjectSize
			<< Settings.HistogramLogMin
			<< Settings.HistogramLogMax
			<< Settings.AutoExposureCalibrationConstant
			<< Settings.LensFlareThreshold
			<< Settings.GrainJitter
			<< Settings.DepthOfFieldScale
			<< Settings.DepthOfFieldMaxBokehSize
			<< Settings.SceneFringeIntensity
			<< Settings.ChromaticAberrationStartOffset
			<< Settings.VignetteIntensity
			<< Settings.FilmToe
			<< Settings.BloomSizeScale
			<< Settings.AutoExposureBias
			<< Settings.AmbientOcclusionIntensity
			<< Settings.AmbientOcclusionRadius
			<< Settings.IndirectLightingIntensity
			<< Settings.ScreenSpaceReflectionIntensity
			<< Settings.ScreenSpaceReflectionQuality
			<< Settings.BloomIntensity
			<< Settings.AutoExposureMinBrightness
			<< Settings.AutoExposureMaxBrightness
			<< Settings.AutoExposureSpeedUp
			<< Settings.AutoExposureSpeedDown
			<< Settings.LensFlareIntensity
			<< Settings.LensFlareBokehSize
			<< Settings.FilmSlope
			<< Settings.AmbientCubemapIntensity
			<< Settings.DepthOfFieldFocalDistance
			<< Settings.DepthOfFieldDepthBlurRadius
			<< Settings.DepthOfFieldDepthBlurAmount
			<< Settings.DepthOfFieldFstop;
	}
};

class UPostProcessVolume : public UObject
{
	DECLARE_CLASS(UPostProcessVolume, UObject);
public:

	FPostProcessSettings Settings;
	float Priority;
	float BlendRadius;
	byte BrushType;
	UBrush* Brush;
	UBrushComponent* BrushComponent;
	bool bHidden;
	bool bEnabled;
	bool bUnbound;
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingMethod;
	UBrushComponent* RootComponent;//same as SceneComponent in Landscape.h
	int32 PolyFlags;

	BEGIN_PROP_TABLE
		PROP_STRUC(Settings, FPostProcessSettings)
		PROP_FLOAT(Priority)
		PROP_FLOAT(BlendRadius)
		PROP_BYTE(BrushType)
		PROP_OBJ(Brush)
		PROP_OBJ(BrushComponent)
		PROP_BOOL(bHidden)
		PROP_BOOL(bEnabled)
		PROP_BOOL(bUnbound)
		PROP_ENUM2(SpawnCollisionHandlingMethod, ESpawnActorCollisionHandlingMethod)
		PROP_OBJ(RootComponent)
		PROP_INT(PolyFlags)
		END_PROP_TABLE

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

public:
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

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(USceneComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};*/

//TODO
/*
	FBodyInstance
	RuntimeFloatCurve
*/

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

public:
	virtual void Serialize(FArchive& Ar)
	{
		guard(USkeletalMeshComponent::Serialize);
		Super::Serialize(Ar);
		DROP_REMAINING_DATA(Ar);
		unguard;
	}
};

class USkeletalMeshActor : public UObject
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
};

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

public:
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

public:
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

public:
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

public:
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

public:
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

public:
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

public:
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

public:
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
*/

#define REGISTER_UNMAP_CLASSES4 \
 REGISTER_CLASS(UBrush) \
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
 REGISTER_CLASS(FPostProcessSettings) \
 REGISTER_CLASS(UPostProcessVolume) \
 REGISTER_CLASS(UPrecomputedVisibilityVolume) \
 REGISTER_CLASS(UEmitter) \
 REGISTER_CLASS(UParticleSystem) \
 REGISTER_CLASS(UParticleSystemComponent) \
 REGISTER_CLASS(UNiagaraComponent) \
 REGISTER_CLASS(UNiagaraActor) \
 REGISTER_CLASS(UDecalComponent) \
 REGISTER_CLASS(USkeletalMeshComponent) \
 REGISTER_CLASS(USkeletalMeshActor) \
 REGISTER_CLASS(UStaticMeshComponent) \
 REGISTER_CLASS(UStaticMeshActor) \
 REGISTER_CLASS(UWorldSettings) \
 REGISTER_CLASS(ULevel) \
 REGISTER_CLASS(USphereReflectionCaptureComponent) \
 REGISTER_CLASS(UPointLightComponent) \
 REGISTER_CLASS(UPointLight) \
 REGISTER_CLASS(USpotLightComponent) \
 REGISTER_CLASS(USpotLight)

#define REGISTER_LIGHT_ENUMS_U4 \
	REGISTER_ENUM(ELightUnits) \
	REGISTER_ENUM(ESpawnActorCollisionHandlingMethod)

#endif // UNREAL4


#endif // __UNMAP_H__