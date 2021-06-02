#ifndef __UNTHIRDPARTY_H__
#define __UNTHIRDPARTY_H__


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

	virtual void Serialize(FArchive &Ar)
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

	virtual void Serialize(FArchive &Ar)
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

#define REGISTER_3RDP_CLASSES		\
	REGISTER_CLASS(USwfMovie)		\
	REGISTER_CLASS_ALIAS(USwfMovie, UGFxMovieInfo) \
	REGISTER_CLASS(UFaceFXAnimSet)	\
	REGISTER_CLASS(UFaceFXAsset)


#endif // UNREAL3

#endif // __UNTHIRDPARTY_H__
