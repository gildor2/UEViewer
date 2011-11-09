#ifndef __EXPORT_H__
#define __EXPORT_H__


// registration
typedef void (*ExporterFunc_t)(const UObject*, FArchive&);

void RegisterExporter(const char *ClassName, const char *FileExt, ExporterFunc_t Func);

// wrapper to avoid typecasts to ExporterFunc_t
// T should be an UObject-derived class
template<class T>
FORCEINLINE void RegisterExporter(const char *ClassName, const char *FileExt, void (*Func)(const T*, FArchive&))
{
	RegisterExporter(ClassName, FileExt, (ExporterFunc_t)Func);
}

bool ExportObject(const UObject *Obj);

// path
void appSetBaseExportDirectory(const char *Dir);
const char* GetExportPath(const UObject *Obj);

// configuration
extern bool GExportScripts;
extern bool GExportLods;
extern bool GExportPskx;
extern bool GNoTgaCompress;
extern bool GUncook;
extern bool GUseGroups;

// forwards
class UObject;
class USkeletalMesh;
class UVertMesh;
class UUnrealMaterial;
class USound;
class USoundNodeWave;
class USwfMovie;
class UFaceFXAnimSet;
class UFaceFXAsset;

class CAnimSet;
class CStaticMesh;

// ActorX
void ExportPsk(const USkeletalMesh *Mesh, FArchive &Ar);
void ExportPsa(const CAnimSet *Anim, FArchive &Ar);
void ExportStaticMesh(const CStaticMesh *Mesh, FArchive &Ar);
// MD5Mesh
void ExportMd5Mesh(const USkeletalMesh *Mesh, FArchive &Ar);
void ExportMd5Anim(const CAnimSet *Anim, FArchive &Ar);
// 3D
void Export3D (const UVertMesh *Mesh, FArchive &Ar);
// TGA
void ExportTga(const UUnrealMaterial *Tex, FArchive &Ar);
// UUnrealMaterial
void ExportMaterial(const UUnrealMaterial *Mat, FArchive &Ar = *GDummySave);
// sound
void ExportSound(const USound *Snd, FArchive &Ar);
void ExportSoundNodeWave(const USoundNodeWave *Snd, FArchive &Ar);
// third party
void ExportGfx(const USwfMovie *Swf, FArchive &Ar);
void ExportFaceFXAnimSet(const UFaceFXAnimSet *Fx, FArchive &Ar);
void ExportFaceFXAsset(const UFaceFXAsset *Fx, FArchive &Ar);

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
