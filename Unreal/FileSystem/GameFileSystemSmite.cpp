#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"
#include "GameFileSystemSmite.h"
#include <md5/md5.h>

#if SMITE
static FSmiteManifest* GSmiteManifest = NULL;

void LoadSmiteManifest(const CGameFileInfo* info) {
	guard(LoadSmiteManifest);

	appPrintf("Loading Smite manifest %s...\n", *info->GetRelativeName());

	FArchive* loader = info->CreateReader();
	assert(loader);
	loader->Game = GAME_Smite;
    if(GSmiteManifest != nullptr) {
        delete GSmiteManifest;
    }
	GSmiteManifest = new FSmiteManifest;
    GSmiteManifest->Serialize(*loader);
    delete loader;

	unguard;
}


FMemReader* GetSmiteBlob(const char* name, int name_len, int level, const char* ext) {
    guard(GetSmiteBlob);

    if(GSmiteManifest == nullptr) {
        return nullptr;
    }

    MD5Context ctx;
    md5Init(&ctx);
    md5Update(&ctx, (unsigned char*)name, name_len);
    md5Finalize(&ctx);

    TArray<FSmiteFile>* item = GSmiteManifest->Files.Find(*reinterpret_cast<FGuid*>(ctx.digest));
    if(item == nullptr) {
        return nullptr;
    }

    int i;
    for(i = 0; i < item->Num(); i++) {
        FSmiteFile* entry = item->GetData() + i;
        if(entry->tier == level) {
            FString* bulk = GSmiteManifest->BulkFiles.Find(entry->guid);
            char filename[512];
	        appSprintf(ARRAY_ARG(filename), "%s.%s", bulk->GetDataArray().GetData(), ext);
            const CGameFileInfo* info = CGameFileInfo::Find(filename);
            if(info == nullptr) {
                appNotify("Smite: can't find tfc %s for %s", filename, name);
                return nullptr;
            }

            FArchive* Ar = info->CreateReader();
            Ar->Game = GAME_Smite;
            Ar->Seek(entry->offset);
            byte *data = (byte*)appMalloc(entry->blob_size);
            Ar->Serialize(data, entry->blob_size);
            delete Ar;
            FMemReader* MemAr = new FMemReader(data, entry->blob_size);
            MemAr->Game = GAME_Smite;
            return MemAr;
        }
    }
    unguard;

    return nullptr;
}
#endif // SMITE
