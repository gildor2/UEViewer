#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnSound.h"

#include "Exporters.h"


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

	char filename[512];
	appSprintf(ARRAY_ARG(filename), "%s/%s.%s", GetExportPath(Obj), Obj->Name, ext);
	FFileReader Ar2(filename, false);

	Ar2.Serialize(Data, DataSize);
}


void ExportSound(const USound *Snd, FArchive &Ar)
{
	SaveSound(Snd, (void*)&Snd->RawData[0], Snd->RawData.Num(), "unk");
}


#if UNREAL3

void ExportSoundNodeWave(const USoundNodeWave *Snd, FArchive &Ar)
{
	// select bulk containing data
	const FByteBulkData *bulk = NULL;
	const char *ext = "unk";

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
	}
	else if (Snd->CompressedPS3Data.ElementCount)
	{
		bulk = &Snd->CompressedPS3Data;
		ext  = "ps3audio";
	}

	SaveSound(Snd, bulk->BulkData, bulk->ElementCount, ext);
}

#endif // UNREAL3
