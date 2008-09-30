#ifndef __EXPORT_H__
#define __EXPORT_H__

void ExportPsk(const USkeletalMesh *Mesh, FArchive &Ar);
void ExportPsa(const UMeshAnimation *Anim, FArchive &Ar);
void ExportTga(const UTexture *Tex, FArchive &Ar);


#endif // __EXPORT_H__
