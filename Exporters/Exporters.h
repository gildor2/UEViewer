#ifndef __EXPORT_H__
#define __EXPORT_H__

extern bool GExportScripts;

void ExportPsk(const USkeletalMesh *Mesh, FArchive &Ar);
void ExportPsa(const UMeshAnimation *Anim, FArchive &Ar);
void Export3D (const UVertMesh *Mesh, FArchive &Ar);
void ExportTga(const UUnrealMaterial *Tex, FArchive &Ar);


#endif // __EXPORT_H__
