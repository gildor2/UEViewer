#ifndef __UNSOUND_H__
#define __UNSOUND_H__

#include "UE4Version.h"

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
	FByteBulkData		CompressedWiiUData;
	FByteBulkData		CompressedIPhoneData;

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

		// We're just checking for end of object data, not checking the ArVer here.
		// The original USoundNodeWave::Serialize just have a few FByteBulkData serialize calls,
		// without any other field types. So we can simply process everything as a single array.
		FByteBulkData* Bulks[] = {
			&CompressedPCData,
			&CompressedXbox360Data,
			&CompressedPS3Data,
			&CompressedWiiUData,		// appeared in ArVer 845
			&CompressedIPhoneData,		// appeared in ArVer 851
		};

		for (FByteBulkData* Bulk : Bulks)
		{
			if (Ar.Tell() == Ar.GetStopper()) return; // no more data in this object
			Bulk->Serialize(Ar);
		}

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

struct FStreamedAudioChunk
{
	FByteBulkData		Data;
	int32				DataSize;
	int32				AudioDataSize;

	friend FArchive& operator<<(FArchive& Ar, FStreamedAudioChunk& Chunk)
	{
		guard(FStreamedAudioChunk<<);

		bool bCooked;
		Ar << bCooked;
		assert(bCooked);

		if (Ar.Game < GAME_UE4(4))
		{
			// No FStreamedAudioChunk before UE4.3
			// UE4.3: only bulk
			Chunk.Data.Serialize(Ar);
			Chunk.AudioDataSize = Chunk.DataSize = Chunk.Data.ElementCount;
		}
		else if (Ar.Game < GAME_UE4(19))
		{
			// UE4.4..UE4.18
			Chunk.Data.Serialize(Ar);
			Ar << Chunk.DataSize;
			Chunk.AudioDataSize = Chunk.DataSize;
		}
		else
		{
			// UE4.19+
			Chunk.Data.Serialize(Ar);
			Ar << Chunk.DataSize;
			Ar << Chunk.AudioDataSize;
		}

		return Ar;

		unguard;
	}
};

class USoundWave : public UObject // actual parent is USoundBase
{
	DECLARE_CLASS(USoundWave, UObject);
public:
	bool				bStreaming;
#if ARK
	bool				bReallyUseStreamingReserved;
#endif
	FByteBulkData		RawData;
	FGuid				CompressedDataGuid;
	TArray<FSoundFormatData> CompressedFormatData; // FFormatContainer in UE4

	FName				StreamedFormat;
	TArray<FStreamedAudioChunk> StreamingChunks;

	int32				SampleRate;
	int32				NumChannels;

	USoundWave()
	:	bStreaming(false)
	#if ARK
		, bReallyUseStreamingReserved(false)
	#endif
	{}

	BEGIN_PROP_TABLE
		PROP_INT(NumChannels)
		PROP_INT(SampleRate)
		PROP_BOOL(bStreaming)
#if ARK
		PROP_BOOL(bReallyUseStreamingReserved) // equivalent of bStreaming in ARK
#endif
		PROP_DROP(bHasVirtualizeWhenSilent)
		PROP_DROP(bVirtualizeWhenSilent)
		PROP_DROP(Duration)
		PROP_DROP(RawPCMDataSize) // not marked as UPROPERTY, but appears in UE4 sounds
	END_PROP_TABLE

	void Serialize(FArchive &Ar)
	{
		guard(USoundWave::Serialize);

		Super::Serialize(Ar);

#if ARK
		if (Ar.Game == GAME_Ark)
		{
			bStreaming = bReallyUseStreamingReserved;
		}
#endif

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
#if ARK
			if (Ar.Game == GAME_Ark)
			{
				// another bCooked. this one is relevant for streaming.
				// if negative, there's no data after it, but that won't happen with client assets.
				Ar << bCooked;
			}
#endif
			int32 NumChunks;
			FName AudioFormat;
			Ar << NumChunks << StreamedFormat;
			appPrintf("WARNING: USoundWave streaming data: %d chunks in format %s\n", NumChunks, *StreamedFormat); //??
			StreamingChunks.AddDefaulted(NumChunks);
			for (int i = 0; i < NumChunks; i++)
			{
				guard(Chunk);
				Ar << StreamingChunks[i];
				unguardf("%d", i);
			}
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
