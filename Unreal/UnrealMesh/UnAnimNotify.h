#ifndef __UNANIM_NOTIFY_H__
#define __UNANIM_NOTIFY_H__


/*-----------------------------------------------------------------------------
	UAnimNotify objects
-----------------------------------------------------------------------------*/

class UAnimNotify : public UObject
{
	DECLARE_CLASS(UAnimNotify, UObject);
	TYPE_FLAGS(TYPE_SilentLoad);
public:
	int				Revision;
};


class UAnimNotify_Script : public UAnimNotify
{
	DECLARE_CLASS(UAnimNotify_Script, UAnimNotify);
public:
	FName			NotifyName;
#if UNREAL3
	FName			NotifyTickName;
	FName			NotifyEndName;
#endif

	BEGIN_PROP_TABLE
		PROP_NAME(NotifyName)
#if UNREAL3
		PROP_NAME(NotifyTickName)
		PROP_NAME(NotifyEndName)
#endif
	END_PROP_TABLE
};


class UAnimNotify_Effect : public UAnimNotify
{
	DECLARE_CLASS(UAnimNotify_Effect, UAnimNotify);
public:
	UObject			*EffectClass;	// UClass*
	FName			Bone;
	FVector			OffsetLocation;
	FRotator		OffsetRotation;
	bool			Attach;
	FName			Tag;
	float			DrawScale;
	FVector			DrawScale3D;
#if LINEAGE2
	bool			TrailCamera;
	bool			IndependentRotation;
	float			EffectScale;
#endif

	UAnimNotify_Effect()
	:	DrawScale(1.0f)
	{
		DrawScale3D.Set(1, 1, 1);
	}
	BEGIN_PROP_TABLE
		PROP_OBJ(EffectClass)
		PROP_NAME(Bone)
		PROP_VECTOR(OffsetLocation)
		PROP_ROTATOR(OffsetRotation)
		PROP_BOOL(Attach)
		PROP_NAME(Tag)
		PROP_FLOAT(DrawScale)
		PROP_VECTOR(DrawScale3D)
#if LINEAGE2
		PROP_BOOL(TrailCamera)
		PROP_BOOL(IndependentRotation)
		PROP_FLOAT(EffectScale)
#endif
	END_PROP_TABLE
};


class UAnimNotify_DestroyEffect : public UAnimNotify
{
	DECLARE_CLASS(UAnimNotify_DestroyEffect, UAnimNotify);
public:
	FName			DestroyTag;
	bool			bExpireParticles;

	UAnimNotify_DestroyEffect()
	:	bExpireParticles(true)
	{}
	BEGIN_PROP_TABLE
		PROP_NAME(DestroyTag)
		PROP_BOOL(bExpireParticles)
	END_PROP_TABLE
};


class UAnimNotify_Sound : public UAnimNotify
{
	DECLARE_CLASS(UAnimNotify_Sound, UAnimNotify);
public:
	UObject			*Sound;			// USound*
	float			Volume;
	int				Radius;
#if LINEAGE2
	int				Random;
	UObject			*DefaultWalkSound[3];
	UObject			*DefaultRunSound[3];
	UObject			*GrassWalkSound[3];
	UObject			*GrassRunSound[3];
	UObject			*WaterWalkSound[3];
	UObject			*WaterRunSound[3];
	UObject			*DefaultActorWalkSound[3];
	UObject			*DefaultActorRunSound[3];
#endif

	UAnimNotify_Sound()
	:	Radius(0)
	,	Volume(1.0f)
	{}
	BEGIN_PROP_TABLE
		PROP_OBJ(Sound)
		PROP_FLOAT(Volume)
		PROP_INT(Radius)
#if LINEAGE2
		PROP_INT(Random)
		PROP_OBJ(DefaultWalkSound)
		PROP_OBJ(DefaultRunSound)
		PROP_OBJ(GrassWalkSound)
		PROP_OBJ(GrassRunSound)
		PROP_OBJ(WaterWalkSound)
		PROP_OBJ(WaterRunSound)
		PROP_OBJ(DefaultActorWalkSound)
		PROP_OBJ(DefaultActorRunSound)
#endif
#if RAGNAROK2
		PROP_DROP(SoundInfo)
#endif
#if UNREAL3
		PROP_DROP(SoundCue)
		PROP_DROP(bFollowActor)
		PROP_DROP(BoneName)
		PROP_DROP(bIgnoreIfActorHidden)
		PROP_DROP(VolumeMultiplier)
		PROP_DROP(PitchMultiplier)
#endif
#if BLADENSOUL
		PROP_DROP(SoundVolume)
#endif
#if BATMAN
		PROP_DROP(CharacterFilter_Enabled)
		PROP_DROP(CharacterFilter)
		PROP_DROP(EventName)
		PROP_DROP(CharacterFilters)
#endif
	END_PROP_TABLE
};


class UAnimNotify_Scripted : public UAnimNotify // abstract class
{
	DECLARE_CLASS(UAnimNotify_Scripted, UAnimNotify);
};


class UAnimNotify_Trigger : public UAnimNotify_Scripted
{
	DECLARE_CLASS(UAnimNotify_Trigger, UAnimNotify_Scripted);
public:
	FName			EventName;

	BEGIN_PROP_TABLE
		PROP_NAME(EventName)
	END_PROP_TABLE
};


#define REGISTER_ANIM_NOTIFY_CLASSES			\
	REGISTER_CLASS(UAnimNotify)					\
	REGISTER_CLASS(UAnimNotify_Script)			\
	REGISTER_CLASS(UAnimNotify_Effect)			\
	REGISTER_CLASS(UAnimNotify_DestroyEffect)	\
	REGISTER_CLASS(UAnimNotify_Sound)			\
	REGISTER_CLASS(UAnimNotify_Scripted)		\
	REGISTER_CLASS(UAnimNotify_Trigger)


#endif // __UNANIM_NOTIFY_H__
