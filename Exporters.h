#ifndef __EXPORT_H__
#define __EXPORT_H__

//?? move export interfaces to ObjectViewer?
void ExportPsk(USkeletalMesh *Mesh, FArchive &Ar);
void ExportPsa(UMeshAnimation *Anim, FArchive &Ar);
void ExportTga(UTexture *Tex, FArchive &Ar);


#endif // __EXPORT_H__
