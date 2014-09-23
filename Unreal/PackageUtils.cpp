#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"

#if HAS_UI
#include "../UI/BaseDialog.h"
#include "../UI/ProgressDialog.h"
#endif

#include "PackageUtils.h"

/*-----------------------------------------------------------------------------
	Package loader/unloader
-----------------------------------------------------------------------------*/

TArray<UnPackage*> GFullyLoadedPackages;
#if HAS_UI
bool LoadWholePackage(UnPackage* Package, UIProgressDialog* progress)
#else
bool LoadWholePackage(UnPackage* Package)
#endif
{
	guard(LoadWholePackage);

	if (GFullyLoadedPackages.FindItem(Package) >= 0) return true;	// already loaded

#if PROFILE
	appResetProfiler();
#endif

	UObject::BeginLoad();
	for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		if (!IsKnownClass(Package->GetObjectName(Package->GetExport(idx).ClassIndex)))
			continue;
#if HAS_UI
		if (progress && !progress->Tick()) return false;
#endif
		Package->CreateExport(idx);
	}
	UObject::EndLoad();
	GFullyLoadedPackages.AddItem(Package);

#if PROFILE
	appPrintProfiler();
#endif

	return true;

	unguardf("%s", Package->Name);
}

void ReleaseAllObjects()
{
#if 0
	appPrintf("Memory: allocated %d bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
	appDumpMemoryAllocations();
#endif
	for (int i = UObject::GObjObjects.Num() - 1; i >= 0; i--)
		delete UObject::GObjObjects[i];
	UObject::GObjObjects.Empty();

	GFullyLoadedPackages.Empty();

#if 0
	// verify that all object pointers were set to NULL
	for (int i = 0; i < UnPackage::PackageMap.Num(); i++)
	{
		UnPackage* p = UnPackage::PackageMap[i];
		for (int j = 0; j < p->Summary.ExportCount; j++)
		{
			FObjectExport& Exp = p->ExportTable[j];
			if (Exp.Object) printf("! %s %d\n", p->Name, j);
		}
	}
#endif
	appPrintf("Memory: allocated %d bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
//	appDumpMemoryAllocations();
}


/*-----------------------------------------------------------------------------
	Package scanner
-----------------------------------------------------------------------------*/

static int InfoCmp(const FileInfo *p1, const FileInfo *p2)
{
	int dif = p1->Ver - p2->Ver;
	if (dif) return dif;
	return p1->LicVer - p2->LicVer;
}

static bool ScanPackage(const CGameFileInfo *file, TArray<FileInfo> &PkgInfo)
{
	guard(ScanPackage);

	FArchive *Ar = appCreateFileReader(file);

	unsigned Tag, FileVer;
	*Ar << Tag;
	if (Tag == PACKAGE_FILE_TAG_REV)
		Ar->ReverseBytes = true;
	else if (Tag != PACKAGE_FILE_TAG)	//?? possibly Lineage2 file etc
		return true;
	*Ar << FileVer;

	FileInfo Info;
	Info.Ver    = FileVer & 0xFFFF;
	Info.LicVer = FileVer >> 16;
	Info.Count  = 0;
	strcpy(Info.FileName, file->RelativeName);
//	printf("%s - %d/%d\n", file->RelativeName, Info.Ver, Info.LicVer);
	int Index = INDEX_NONE;
	for (int i = 0; i < PkgInfo.Num(); i++)
	{
		FileInfo &Info2 = PkgInfo[i];
		if (Info2.Ver == Info.Ver && Info2.LicVer == Info.LicVer)
		{
			Index = i;
			break;
		}
	}
	if (Index == INDEX_NONE)
		Index = PkgInfo.AddItem(Info);
	// update info
	PkgInfo[Index].Count++;
	// combine filename
	char *s = PkgInfo[Index].FileName;
	char *d = Info.FileName;
	while (*s == *d && *s != 0)
	{
		s++;
		d++;
	}
	*s = 0;

	delete Ar;
	return true;

	unguardf("%s", file->RelativeName);
}


void ScanPackages(TArray<FileInfo>& info)
{
	info.Empty();
	appEnumGameFiles(ScanPackage, info);
	info.Sort(InfoCmp);
}
