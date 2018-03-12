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

// This function will clear list of already exported objects
void ResetExportedList();

bool ExportObject(const UObject *Obj);

// path
void appSetBaseExportDirectory(const char *Dir);
const char* GetExportPath(const UObject *Obj);

const char* GetExportFileName(const UObject *Obj, const char *fmt, ...);
bool CheckExportFilePresence(const UObject *Obj, const char *fmt, ...);

// Create file for saving UObject.
// File will be placed in directory selected by GetExportPath(), name is computed from fmt+varargs.
// Function may return NULL.
FArchive *CreateExportArchive(const UObject *Obj, const char *fmt, ...);

// configuration
extern bool GExportScripts;
extern bool GExportLods;
extern bool GNoTgaCompress;
extern bool GExportDDS;
extern bool GUncook;
extern bool GUseGroups;
extern bool GDontOverwriteFiles;

// forwards
class UObject;
class UVertMesh;
class UUnrealMaterial;
class USound;
class USoundNodeWave;
class USoundWave;
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
void ExportTexture(const UUnrealMaterial *Tex);
// UUnrealMaterial
void ExportMaterial(const UUnrealMaterial *Mat);
// sound
void ExportSound(const USound *Snd);
void ExportSoundNodeWave(const USoundNodeWave *Snd);
void ExportSoundWave4(const USoundWave *Snd);
// third party
void ExportGfx(const USwfMovie *Swf);
void ExportFaceFXAnimSet(const UFaceFXAnimSet *Fx);
void ExportFaceFXAsset(const UFaceFXAsset *Fx);

void WriteTGA(FArchive &Ar, int width, int height, byte *pic);


#endif // __EXPORT_H__
