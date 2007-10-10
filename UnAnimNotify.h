#ifndef __UNANIMNOTIFY_H__
#define __UNANIMNOTIFY_H__


/*-----------------------------------------------------------------------------
	UAnimNotify objects
-----------------------------------------------------------------------------*/

class UAnimNotify : public UObject
{
	DECLARE_CLASS(UAnimNotify, UObject);
public:
	int				Revision;
};


class UAnimNotify_Script : public UAnimNotify
{
	DECLARE_CLASS(UAnimNotify_Script, UAnimNotify);
public:
	FName			NotifyName;

	BEGIN_PROP_TABLE
		PROP_NAME(NotifyName)
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

	UAnimNotify_Sound()
	:	Radius(0)
	,	Volume(1.0f)
	{}
	BEGIN_PROP_TABLE
		PROP_OBJ(Sound)
		PROP_FLOAT(Volume)
		PROP_INT(Radius)
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


#endif // __UNANIMNOTIFY_H__
