#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnSound.h"

#include "Exporters.h"


#define XMA_EXPORT		1


static void SaveSound(const UObject *Obj, void *Data, int DataSize, const char *DefExt)
{
	// check for enough place for header
	if (DataSize < 16)
	{
		appPrintf("... empty sound %s ?\n", Obj->Name);
		return;
	}

	const char *ext = DefExt;

	if (!memcmp(Data, "OggS", 4))
		ext = "ogg";
	else if (!memcmp(Data, "RIFF", 4))
		ext = "wav";
	else if (!memcmp(Data, "FSB4", 4))
		ext = "fsb";		// FMOD sound bank
	else if (!memcmp(Data, "MSFC", 4))
		ext = "mp3";		// PS3 MP3 codec

	FArchive *Ar = CreateExportArchive(Obj, "%s.%s", Obj->Name, ext);
	if (Ar)
	{
		Ar->Serialize(Data, DataSize);
		delete Ar;
	}
}


#if XMA_EXPORT

static void WriteRiffHeader(FArchive &Ar, int FileLength)
{
	assert(!Ar.IsLoading);

	static const char *RIFF = "RIFF";
	Ar.Serialize((char*)RIFF, 4);

	Ar << FileLength;

	static const char *WAVE = "WAVE";
	Ar.Serialize((char*)WAVE, 4);
}


static void WriteRiffChunk(FArchive &Ar, const char *id, int len)
{
	Ar.Serialize((char*)id, 4);
	Ar << len;
}


struct FXmaInfoHeader
{
	int				WaveFormatLength;
	int				SeekTableSize;
	int				CompressedDataSize;

	friend FArchive& operator<<(FArchive &Ar, FXmaInfoHeader &H)
	{
		return Ar << H.WaveFormatLength << H.SeekTableSize << H.CompressedDataSize;
	}
};


// structure from DX10 audiodefs.h
struct WAVEFORMATEX
{
	uint16			wFormatTag;				// Integer identifier of the format
	uint16			nChannels;				// Number of audio channels
	unsigned		nSamplesPerSec;			// Audio sample rate
	unsigned		nAvgBytesPerSec;		// Bytes per second (possibly approximate)
	uint16			nBlockAlign;			// Size in bytes of a sample block (all channels)
	uint16			wBitsPerSample;			// Size in bits of a single per-channel sample
	uint16			cbSize;					// Bytes of extra data appended to this struct

	friend FArchive& operator<<(FArchive &Ar, WAVEFORMATEX &V)
	{
		return Ar << V.wFormatTag << V.nChannels << V.nSamplesPerSec << V.nAvgBytesPerSec
				  << V.nBlockAlign << V.wBitsPerSample << V.cbSize;
	}
};


// structure from DX10 xma2defs.h
struct XMA2WAVEFORMATEX
{
	WAVEFORMATEX	wfx;

	uint16			NumStreams;				// Number of audio streams (1 or 2 channels each)
	unsigned		ChannelMask;			// Spatial positions of the channels in this file,
											// stored as SPEAKER_xxx values (see audiodefs.h)
	unsigned		SamplesEncoded;			// Total number of PCM samples the file decodes to
	unsigned		BytesPerBlock;			// XMA block size (but the last one may be shorter)
	unsigned		PlayBegin;				// First valid sample in the decoded audio
	unsigned		PlayLength;				// Length of the valid part of the decoded audio
	unsigned		LoopBegin;				// Beginning of the loop region in decoded sample terms
	unsigned		LoopLength;				// Length of the loop region in decoded sample terms
	byte			LoopCount;				// Number of loop repetitions; 255 = infinite
	byte			EncoderVersion;			// Version of XMA encoder that generated the file
	uint16			BlockCount;				// XMA blocks in file (and entries in its seek table)

	friend FArchive& operator<<(FArchive &Ar, XMA2WAVEFORMATEX &V)
	{
		return Ar << V.wfx << V.NumStreams << V.ChannelMask << V.SamplesEncoded << V.BytesPerBlock
				  << V.PlayBegin << V.PlayLength << V.LoopBegin << V.LoopLength << V.LoopCount
				  << V.EncoderVersion << V.BlockCount;
	}
};


static bool SaveXMASound(const UObject *Obj, void *Data, int DataSize, const char *DefExt)
{
	// check for enough place for header
	if (DataSize < 16)
	{
		appPrintf("ERROR: %s'%s': empty data\n", Obj->GetClassName(), Obj->Name);
		return false;
	}

	FMemReader Reader(Data, DataSize);
	Reader.ReverseBytes = true;

	FXmaInfoHeader Hdr;
	Reader << Hdr;

	int ComputedDataSize = Reader.Tell() + Hdr.WaveFormatLength + Hdr.SeekTableSize + Hdr.CompressedDataSize;
	if (ComputedDataSize != DataSize)
	{
		if (ComputedDataSize > DataSize)
		{
			// does not fit into
			appPrintf("ERROR: %s'%s': wrong data\n", Obj->GetClassName(), Obj->Name);
			return false;
		}
		appPrintf("WARNING: %s'%s': wrong data\n", Obj->GetClassName(), Obj->Name);
	}

	// +4 bytes - RIFF "WAVE" id
	// +8 bytes - fmt or XMA2 chunk header
	// +8 bytes - data chunk header
	int ResultFileSize = Hdr.WaveFormatLength + /*??Hdr.SeekTableSize+*/ Hdr.CompressedDataSize + (4+8+8);
	FArchive *Ar;

	if (Hdr.WaveFormatLength == 0x34)						// sizeof(XMA2WAVEFORMATEX)
	{
		Ar = CreateExportArchive(Obj, "%s.%s", Obj->Name, DefExt);
		if (!Ar) return false;

		WriteRiffHeader(*Ar, ResultFileSize);
		WriteRiffChunk(*Ar, "fmt ", Hdr.WaveFormatLength);

		XMA2WAVEFORMATEX fmt;
		// read with conversion from big-endian to little-endian
		Reader << fmt;
		// write in little-endian format
		(*Ar) << fmt;
	}
	else if (Hdr.WaveFormatLength == 0x2C)					// sizeof(XMA2WAVEFORMAT)
	{
		Ar = CreateExportArchive(Obj, "%s.%s", Obj->Name, DefExt);
		if (!Ar) return false;

		WriteRiffHeader(*Ar, ResultFileSize);
		WriteRiffChunk(*Ar, "XMA2", Hdr.WaveFormatLength);

		// XMA2WAVEFORMAT should be stored in big-endian format, so no byte swapping performed
		Ar->Serialize((byte*)Data + Reader.Tell(), Hdr.WaveFormatLength);
		Reader.Seek(Reader.Tell() + Hdr.WaveFormatLength);	// skip WAVEFORMAT
	}
	else
	{
		appPrintf("ERROR: %s'%s': unknown XBox360 WAVEFORMAT - %X bytes\n", Obj->GetClassName(), Obj->Name, Hdr.WaveFormatLength);
		return false;
	}

	//?? create "seek chunk"
	// write data chunk
	WriteRiffChunk(*Ar, "data", Hdr.CompressedDataSize);
	Ar->Serialize((byte*)Data + Reader.Tell() + Hdr.SeekTableSize, Hdr.CompressedDataSize);

	// check correctness of ResultFileSize - should equal to file length -8 bytes (exclude RIFF header)
	assert(Ar->Tell() == ResultFileSize + 8);

	delete Ar;
	return true;
}

#endif // XMA_EXPORT


void ExportSound(const USound *Snd)
{
	SaveSound(Snd, (void*)&Snd->RawData[0], Snd->RawData.Num(), "unk");
}


#if UNREAL3

void ExportSoundNodeWave(const USoundNodeWave *Snd)
{
	// select bulk containing data
	const FByteBulkData *bulk = NULL;
	const char *ext = "unk";
	int extraHeaderSize = 0;

	if (Snd->RawData.ElementCount)
	{
		bulk = &Snd->RawData;
	}
	else if (Snd->CompressedPCData.ElementCount)
	{
		bulk = &Snd->CompressedPCData;
	}
	else if (Snd->CompressedXbox360Data.ElementCount)
	{
		bulk = &Snd->CompressedXbox360Data;
		ext  = "x360audio";
#if XMA_EXPORT
		if (SaveXMASound(Snd, bulk->BulkData, bulk->ElementCount, "xma")) return;
		// else - detect format by data tags, like for PC
#endif
	}
	else if (Snd->CompressedPS3Data.ElementCount)
	{
		bulk = &Snd->CompressedPS3Data;
		ext  = "ps3audio";
		extraHeaderSize = 16;
		//!! note: has up to 4 sounds in single object
		//!! bulk data starts with int32[4] holding sizes of all sounds
		//!! 0 means no data for particular object
		//!! data encoded in MP3 format
	}

	if (bulk)
	{
		SaveSound(Snd, OffsetPointer(bulk->BulkData, extraHeaderSize), bulk->ElementCount - extraHeaderSize, ext);
	}
}

#endif // UNREAL3

#if UNREAL4

void ExportSoundWave4(const USoundWave *Snd)
{
	// select bulk containing data
	const FByteBulkData *bulk = NULL;
	const char *ext = "unk";
	int extraHeaderSize = 0;

	if (Snd->RawData.ElementCount)
	{
		bulk = &Snd->RawData;
	}
	else if (Snd->CompressedFormatData.Num())
	{
		bulk = &Snd->CompressedFormatData[0].Data;
		ext = *Snd->CompressedFormatData[0].FormatName; // "OGG"
	}

	if (bulk)
	{
		SaveSound(Snd, bulk->BulkData, bulk->ElementCount, ext);
	}
}

#endif // UNREAL4
