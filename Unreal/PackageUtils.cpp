#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"

#include "PackageUtils.h"

/*-----------------------------------------------------------------------------
	Package loader/unloader
-----------------------------------------------------------------------------*/

TArray<UnPackage*> GFullyLoadedPackages;
bool LoadWholePackage(UnPackage* Package, IProgressCallback* progress)
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
		if (progress && !progress->Tick()) return false;
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

struct ScanPackageData
{
	ScanPackageData()
	:	Progress(NULL)
	,	Cancelled(false)
	,	Index(0)
	{}

	TArray<FileInfo>*	PkgInfo;
	IProgressCallback*	Progress;
	bool				Cancelled;
	int					Index;
};

static int InfoCmp(const FileInfo *p1, const FileInfo *p2)
{
	int dif = p1->Ver - p2->Ver;
	if (dif) return dif;
	return p1->LicVer - p2->LicVer;
}

static bool ScanPackage(const CGameFileInfo *file, ScanPackageData &data)
{
	guard(ScanPackage);

	if (data.Progress)
	{
		if (!data.Progress->Progress(file->RelativeName, data.Index++, GNumPackageFiles))
		{
			data.Cancelled = true;
			return false;
		}
	}

	//!! use CreatePackageLoader() here to allow scanning of packages
	//!! with custom header (Lineage etc)
	FArchive *Ar = appCreateFileReader(file);

	unsigned Tag, FileVer;
	*Ar << Tag;
	if (Tag == PACKAGE_FILE_TAG_REV)
	{
		Ar->ReverseBytes = true;
	}
	else if (Tag != PACKAGE_FILE_TAG)	//?? possibly Lineage2 file etc
	{
		delete Ar;
		return true;
	}
	*Ar << FileVer;

	FileInfo Info;
	Info.Ver    = FileVer & 0xFFFF;
	Info.LicVer = FileVer >> 16;
	Info.Count  = 0;
	strcpy(Info.FileName, file->RelativeName);
//	printf("%s - %d/%d\n", file->RelativeName, Info.Ver, Info.LicVer);
	int Index = INDEX_NONE;
	for (int i = 0; i < data.PkgInfo->Num(); i++)
	{
		FileInfo &Info2 = (*data.PkgInfo)[i];
		if (Info2.Ver == Info.Ver && Info2.LicVer == Info.LicVer)
		{
			Index = i;
			break;
		}
	}
	if (Index == INDEX_NONE)
		Index = data.PkgInfo->AddItem(Info);
	// update info
	FileInfo& fileInfo = (*data.PkgInfo)[Index];
	fileInfo.Count++;
	// combine filename
	char *s = fileInfo.FileName;
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


bool ScanPackages(TArray<FileInfo>& info, IProgressCallback* progress)
{
	info.Empty();
	ScanPackageData data;
	data.PkgInfo = &info;
	data.Progress = progress;
	appEnumGameFiles(ScanPackage, data);
	info.Sort(InfoCmp);

	return !data.Cancelled;
}
