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
	PROFILE_LABEL(*Package->GetFilename());

	if (GFullyLoadedPackages.FindItem(Package) >= 0) return true;	// already loaded

#if 0 // PROFILE -- disabled, appears useless - always says "0.00 MBytes serialized in 0 calls"
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

#if 0 // PROFILE
	appPrintProfiler("Full package loaded");
#endif

	return true;

	unguardf("%s", *Package->GetFilename());
}

void ReleaseAllObjects()
{
	guard(ReleaseAllObjects);

	if (!UObject::GObjObjects.Num()) return;

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

	// Print currently used memory statistics, but only if it differs from previous one.
	// This lets to avoid console spam when doing export of packages which has nothing exportable inside.
	static size_t lastAllocsSize = 0;
	static int lastAllocsCount = 0;
	if (GTotalAllocationSize != lastAllocsSize || GTotalAllocationCount != lastAllocsCount)
	{
		lastAllocsSize = GTotalAllocationSize;
		lastAllocsCount = GTotalAllocationCount;
		appPrintf("Memory: allocated " FORMAT_SIZE("d") " bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
	}
//	appDumpMemoryAllocations();

	unguard;
}


/*-----------------------------------------------------------------------------
	Package version scanner
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

static bool ScanPackage(const CGameFileInfo *file, ScanPackageData &data)
{
	guard(ScanPackage);

	if (data.Progress)
	{
		FStaticString<MAX_PACKAGE_PATH> RelativeName;
		file->GetRelativeName(RelativeName);
		if (!data.Progress->Progress(*RelativeName, data.Index++, GNumPackageFiles))
		{
			data.Cancelled = true;
			return false;
		}
	}

	// read a few first bytes as integers
	FArchive *Ar = file->CreateReader();
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
	FStaticString<MAX_PACKAGE_PATH> RelativeName;
	file->GetRelativeName(RelativeName);
	strcpy(Info.FileName, *RelativeName);
//	printf("%s - %d/%d\n", *RelativeName, Info.Ver, Info.LicVer);
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

	unguardf("%s", *file->GetRelativeName());
}


bool ScanPackageVersions(TArray<FileInfo>& info, IProgressCallback* progress)
{
	info.Empty();
	ScanPackageData data;
	data.PkgInfo = &info;
	data.Progress = progress;
	appEnumGameFiles(ScanPackage, data);
	info.Sort([](const FileInfo& p1, const FileInfo& p2) -> int
		{
			int dif = p1.Ver - p2.Ver;
			if (dif) return dif;
			return p1.LicVer - p2.LicVer;
		});

	return !data.Cancelled;
}


/*-----------------------------------------------------------------------------
	Package content
-----------------------------------------------------------------------------*/

static void ScanPackageExports(UnPackage* package, CGameFileInfo* file)
{
	for (int idx = 0; idx < package->Summary.ExportCount; idx++)
	{
		const char* ObjectClass = package->GetObjectName(package->GetExport(idx).ClassIndex);

		if (!stricmp(ObjectClass, "SkeletalMesh") || !stricmp(ObjectClass, "DestructibleMesh"))
			file->NumSkeletalMeshes++;
		else if (!stricmp(ObjectClass, "StaticMesh"))
			file->NumStaticMeshes++;
		else if (!stricmp(ObjectClass, "Animation") || !stricmp(ObjectClass, "MeshAnimation") || !stricmp(ObjectClass, "AnimSequence")) // whole AnimSet count for UE2 and number of sequences for UE3+
			file->NumAnimations++;
		else if (!strnicmp(ObjectClass, "Texture", 7))
			file->NumTextures++;
	}
/*	for (int j = 0; j < package->Summary.NameCount; j++)
	{
		if (!stricmp(package->NameTable[j], "PF_BC6H") || !stricmp(package->NameTable[j], "PF_FloatRGBA"))
		{
			printf("%s : %s\n", package->NameTable[j], package->Filename);
		}
	} */
}

bool ScanContent(const TArray<const CGameFileInfo*>& Packages, IProgressCallback* Progress)
{
#if PROFILE
	appResetProfiler();
#endif
	bool cancelled = false;
	bool scanned = false; // says if anywhing was scanned or not, just for profiler message

	// Preallocate PackageMap
	UnPackage::ReservePackageMap(Packages.Num());

	for (int i = 0; i < Packages.Num(); i++)
	{
		CGameFileInfo* file = const_cast<CGameFileInfo*>(Packages[i]);		// we'll modify this structure here
		if (file->IsPackageScanned) continue;

		// Update progress dialog
		FStaticString<MAX_PACKAGE_PATH> RelativeName;
		file->GetRelativeName(RelativeName);
		if (Progress && !Progress->Progress(*RelativeName, i, Packages.Num()))
		{
			cancelled = true;
			break;
		}

		file->IsPackageScanned = true;

		if (file->Package)
		{
			// package already loaded
			ScanPackageExports(file->Package, file);
		}
		else
		{
			UnPackage* package = UnPackage::LoadPackage(file, /*silent=*/ true);	// should always return non-NULL
			if (!package) continue;		// should not happen
			ScanPackageExports(package, file);
		#if 0
			// this code is disabled: it works, however we're going to use ScanContent not just to get objects counts,
			// but also for collecting object references

			// now unload package to not waste memory
			UnPackage::UnloadPackage(package);
			assert(file->Package == NULL);
		#endif
		}
		scanned = true;
	}
#if 0
	void PrintStringHashDistribution();
	PrintStringHashDistribution();
#endif
#if PROFILE
	if (scanned)
		appPrintProfiler("Scanned packages");
#endif
	return !cancelled;
}


/*-----------------------------------------------------------------------------
	Class statistics
-----------------------------------------------------------------------------*/

void CollectPackageStats(const TArray<UnPackage*> &Packages, TArray<ClassStats>& Stats)
{
	guard(CollectPackageStats);

	Stats.Empty(256);

	for (int i = 0; i < Packages.Num(); i++)
	{
		UnPackage* pkg = Packages[i];
		for (int j = 0; j < pkg->Summary.ExportCount; j++)
		{
			const FObjectExport &Exp = pkg->ExportTable[j];
			const char* className = pkg->GetObjectName(Exp.ClassIndex);
			ClassStats* found = NULL;
			for (int k = 0; k < Stats.Num(); k++)
				if (Stats[k].Name == className)
				{
					found = &Stats[k];
					break;
				}
			if (!found)
				found = new (Stats) ClassStats(className);
			found->Count++;
		}
	}
	Stats.Sort([](const ClassStats& p1, const ClassStats& p2) -> int
		{
			return stricmp(p1.Name, p2.Name);
		});

	unguard;
}
