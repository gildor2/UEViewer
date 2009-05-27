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

#endif // __EXPORT_H__
