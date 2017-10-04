#ifndef __UNSOUND_H__
#define __UNSOUND_H__


class USound : public UObject
{
	DECLARE_CLASS(USound, UObject);
public:
	float				f2C;
	FName				f5C;
	TLazyArray<byte>	RawData;

	void Serialize(FArchive &Ar)
	{
		guard(USound::Serialize);

		Super::Serialize(Ar);
		Ar << f5C;
#if UT2 || BATTLE_TERR || LOCO
		if ((Ar.Game == GAME_UT2 || Ar.Game == GAME_BattleTerr || Ar.Game == GAME_Loco) && Ar.ArLicenseeVer >= 2)
			Ar << f2C;
#endif
		Ar << RawData;

		unguard;
	}
};


#if UNREAL3

class USoundNodeWave : public UObject
{
	DECLARE_CLASS(USoundNodeWave, UObject);
public:
	FByteBulkData		RawData;
	FByteBulkData		CompressedPCData;
	FByteBulkData		CompressedXbox360Data;
	FByteBulkData		CompressedPS3Data;

	void Serialize(FArchive &Ar)
	{
		guard(USoundNodeWave::Serialize);

		Super::Serialize(Ar);
		RawData.Serialize(Ar);
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers)
		{
			DROP_REMAINING_DATA(Ar);
			return;
		}
#endif
		CompressedPCData.Serialize(Ar);
		CompressedXbox360Data.Serialize(Ar);
		CompressedPS3Data.Serialize(Ar);

		// some hack to support more games ...
		if (Ar.Tell() < Ar.GetStopper())
		{
			appPrintf("USoundNodeWave %s: dropping %d bytes\n", Name, Ar.GetStopper() - Ar.Tell());
//		skip_rest_quiet:
			DROP_REMAINING_DATA(Ar);
		}

#if 0
		appPrintf("Sound: raw(%d) pc(%d) xbox(%d) ps3(%d)\n",
			RawData.ElementCount,
			CompressedPCData.ElementCount,
			CompressedXbox360Data.ElementCount,
			CompressedPS3Data.ElementCount
		);
#endif

		unguard;
	}
};

#endif // UNREAL3


#if UNREAL4

struct FSoundFormatData // FFormatContainer item representation
{
	FName				FormatName;
	FByteBulkData		Data;

	friend FArchive& operator<<(FArchive& Ar, FSoundFormatData& D)
	{
		Ar << D.FormatName;
		D.Data.Serialize(Ar);
		appPrintf("Sound: Format=%s Data=%d\n", *D.FormatName, D.Data.ElementCount);
		return Ar;
	}
};

class USoundWave : public UObject // actual parent is USoundBase
{
	DECLARE_CLASS(USoundWave, UObject);
public:
	bool				bStreaming;
	FByteBulkData		RawData;
	FGuid				CompressedDataGuid;
	TArray<FSoundFormatData> CompressedFormatData; // FFormatContainer in UE4

	USoundWave()
	:	bStreaming(false)
	{}

	BEGIN_PROP_TABLE
		PROP_BOOL(bStreaming)
	END_PROP_TABLE

	void Serialize(FArchive &Ar)
	{
		guard(USoundWave::Serialize);

		Super::Serialize(Ar);

		bool bCooked;
		Ar << bCooked;

		if (Ar.ArVer >= VER_UE4_SOUND_COMPRESSION_TYPE_ADDED && FFrameworkObjectVersion::Get(Ar) < FFrameworkObjectVersion::RemoveSoundWaveCompressionName)
		{
			FName DummyCompressionName;
			Ar << DummyCompressionName;
		}

		bool bSupportsStreaming = true; // seems, Windows has streaming support, mobile - no

		if (!bStreaming)
		{
			if (bCooked)
			{
				Ar << CompressedFormatData;
			}
			else
			{
				RawData.Serialize(Ar);
			}
		}

		Ar << CompressedDataGuid;

		if (bStreaming)
		{
			//!! see FStreamedAudioPlatformData::Serialize (AudioDerivedData.cpp)
			int32 NumChunks;
			FName AudioFormat;
			Ar << NumChunks << AudioFormat;
			appNotify("USoundWave: streaming data: %d chunks in format %s\n", NumChunks, *AudioFormat);
		}

		// some hack to support more games ...
		if (Ar.Tell() < Ar.GetStopper())
		{
			appPrintf("USoundWave %s: dropping %d bytes\n", Name, Ar.GetStopper() - Ar.Tell());
//		skip_rest_quiet:
			DROP_REMAINING_DATA(Ar);
		}

		unguard;
	}
};

#endif // UNREAL4


#define REGISTER_SOUND_CLASSES		\
	REGISTER_CLASS(USound)

#define REGISTER_SOUND_CLASSES_UE3	\
	REGISTER_CLASS(USoundNodeWave)

#define REGISTER_SOUND_CLASSES_TRANS \
	REGISTER_CLASS_ALIAS(USoundNodeWave, USoundNodeWaveEx)

#define REGISTER_SOUND_CLASSES_UE4	\
	REGISTER_CLASS(USoundWave)

#endif // __UNSOUND_H__
