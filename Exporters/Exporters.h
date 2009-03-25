#ifndef __EXPORT_H__
#define __EXPORT_H__

extern bool GExportScripts;

// ActorX
void ExportPsk(const USkeletalMesh *Mesh, FArchive &Ar);
void ExportPsa(const UMeshAnimation *Anim, FArchive &Ar);
// MD5Mesh
void ExportMd5Mesh(const USkeletalMesh *Mesh, FArchive &Ar);
void ExportMd5Anim(const UMeshAnimation *Anim, FArchive &Ar);
// 3D
void Export3D (const UVertMesh *Mesh, FArchive &Ar);
// TGA
void ExportTga(const UUnrealMaterial *Tex, FArchive &Ar);


#endif // __EXPORT_H__
