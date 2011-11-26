#ifndef __EXPORT_H__
#define __EXPORT_H__


// registration
typedef void (*ExporterFunc_t)(const UObject*);

void RegisterExporter(const char *ClassName, ExporterFunc_t Func);

// wrapper to avoid typecasts to ExporterFunc_t
// T should be an UObject-derived class
template<class T>
FORCEINLINE void RegisterExporter(const char *ClassName, void (*Func)(const T*))
{
	RegisterExporter(ClassName, (ExporterFunc_t)Func);
}

bool ExportObject(const UObject *Obj);

// path
void appSetBaseExportDirectory(const char *Dir);
const char* GetExportPath(const UObject *Obj);

// Create file for saving UObject.
// File will be placed in directory selected by GetExportPath(), name is computed from fmt+varargs.
// Function may return NULL.
FArchive *CreateExportArchive(const UObject *Obj, const char *fmt, ...);

// configuration
extern bool GExportScripts;
extern bool GExportLods;
extern bool GExportPskx;
extern bool GNoTgaCompress;
extern bool GUncook;
extern bool GUseGroups;
extern bool GDontOverwriteFiles;

// forwards
class UObject;
class UVertMesh;
class UUnrealMaterial;
class USound;
class USoundNodeWave;
class USwfMovie;
class UFaceFXAnimSet;
class UFaceFXAsset;

class CSkeletalMesh;
class CAnimSet;
class CStaticMesh;

// ActorX
void ExportPsk(const CSkeletalMesh *Mesh);
void ExportPsa(const CAnimSet *Anim);
void ExportStaticMesh(const CStaticMesh *Mesh);
// MD5Mesh
void ExportMd5Mesh(const CSkeletalMesh *Mesh);
void ExportMd5Anim(const CAnimSet *Anim);
// 3D
void Export3D (const UVertMesh *Mesh);
// TGA
void ExportTga(const UUnrealMaterial *Tex);
// UUnrealMaterial
void ExportMaterial(const UUnrealMaterial *Mat);
// sound
void ExportSound(const USound *Snd);
void ExportSoundNodeWave(const USoundNodeWave *Snd);
// third party
void ExportGfx(const USwfMovie *Swf);
void ExportFaceFXAnimSet(const UFaceFXAnimSet *Fx);
void ExportFaceFXAsset(const UFaceFXAsset *Fx);

// service functions
//?? place implementation to cpp?
struct UniqueNameList
{
	struct Item
	{
		char Name[256];
		int  Count;
	};
	TArray<Item> Items;

	int RegisterName(const char *Name)
	{
		for (int i = 0; i < Items.Num(); i++)
		{
			Item &V = Items[i];
			if (strcmp(V.Name, Name) != 0) continue;
			return ++V.Count;
		}
		Item *N = new (Items) Item;
		appStrncpyz(N->Name, Name, ARRAY_COUNT(N->Name));
		N->Count = 1;
		return 1;
	}
};

void WriteTGA(FArchive &Ar, int width, int height, byte *pic);


#endif // __EXPORT_H__
