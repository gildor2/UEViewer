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
	GFullyLoadedPackages.Add(Package);

#if PROFILE
	appPrintProfiler();
#endif

	return true;

	unguardf("%s", Package->Name);
}

void ReleaseAllObjects()
{
	guard(ReleaseAllObjects);

#if 0
	appPrintf("Memory: allocated " FORMAT_SIZE("d") " bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
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
	appPrintf("Memory: allocated " FORMAT_SIZE("d") " bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
//	appDumpMemoryAllocations();

	unguard;
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

	// read a few first bytes as integers
	FArchive *Ar = appCreateFileReader(file);
	uint32 FileData[16];
	Ar->Serialize(FileData, sizeof(FileData));
	delete Ar;

	unsigned Tag = FileData[0];
	if (Tag == PACKAGE_FILE_TAG_REV)
	{
		// big-endian package
		appReverseBytes(&FileData, ARRAY_COUNT(FileData), sizeof(FileData[0]));
	}
	else if (Tag != PACKAGE_FILE_TAG)	//?? possibly Lineage2 file etc
	{
		//!! Use CreatePackageLoader() here to allow scanning of packages with custom header (Lineage etc);
		//!! do that only when something "strange" within data noticed.
		//!! Also, this function could react on custom package tags.
		return true;
	}
	uint32 Version = FileData[1];

	FileInfo Info;

#if UNREAL4
	if ((Version & 0xFFFFF000) == 0xFFFFF000)
	{
		// next fields are: int VersionUE3, Version, LicenseeVersion
		Info.Ver    = FileData[3];
		Info.LicVer = FileData[4];
	}
	else
#endif // UNREAL4
	{
		Info.Ver    = Version & 0xFFFF;
		Info.LicVer = Version >> 16;
	}

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
		Index = data.PkgInfo->Add(Info);
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
