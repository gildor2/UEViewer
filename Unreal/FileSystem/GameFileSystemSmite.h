#include "Core.h"
#include "UnCore.h"

#if SMITE
struct FSmiteFile {
    int32 tier;
    int32 size;
    int32 offset;
    int32 blob_size;
    FGuid guid;

	friend FArchive& operator<<(FArchive &Ar, FSmiteFile &H)
	{
        return Ar << H.tier << H.size << H.offset << H.blob_size << H.guid;
    }
};

struct FSmiteManifest
{
    TMap<FGuid, TArray<FSmiteFile>> Files;
    TMap<FGuid, FString> BulkFiles;

	void Serialize(FArchive& Ar)
	{
        Ar << Files;
        Ar << BulkFiles;
	}
};

void LoadSmiteManifest(const CGameFileInfo* info);

FMemReader* GetSmiteBlob(const char* name, int name_len, int level, const char* ext);

#endif // SMITE
