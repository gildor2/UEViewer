#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"

#include "PackageUtils.h"
#include "Exporters/Exporters.h"
#include "UmodelApp.h"


bool ExportObjects(const TArray<UObject*> *Objects, IProgressCallback* progress)
{
	guard(ExportObjects);

	// Do not print anything to a log and do not do anything if there's no objects loaded
	if (UObject::GObjObjects.Num() == 0) return true;

	appPrintf("Exporting objects ...\n");

	// export object(s), if possible
	UnPackage* notifyPackage = NULL;
	bool hasObjectList = (Objects != NULL) && Objects->Num();

	//?? when 'Objects' passed, probably iterate over that list instead of GObjObjects
	for (UObject* ExpObj : UObject::GObjObjects)
	{
		if (progress && !progress->Tick()) return false;
		bool objectSelected = !hasObjectList || (Objects->FindItem(ExpObj) >= 0);

		if (!objectSelected) continue;

		if (notifyPackage != ExpObj->Package)
		{
			notifyPackage = ExpObj->Package;
			appSetNotifyHeader(*notifyPackage->GetFilename());
		}

		bool done = ExportObject(ExpObj);

		if (!done && hasObjectList)
		{
			// display warning message only when failed to export object, specified from command line
			appPrintf("ERROR: Export object %s: unsupported type %s\n", ExpObj->Name, ExpObj->GetClassName());
		}
	}

	return true;

	unguard;
}


bool ExportPackages(const TArray<UnPackage*>& Packages, IProgressCallback* Progress)
{
	guard(ExportPackages);

	// Register exporters and classes (will be performed only once); use any package
	// to detect an engine version
	InitClassAndExportSystems(Packages[0]->Game);

#if RENDERING
	// We'll entirely unload all objects, so we should reset viewer. Note: CUmodelApp does this too,
	// however we're calling ExportPackages() from different code places, so call it anyway.
	GApplication.ReleaseViewerAndObjects();
#endif

	bool cancelled = false;

#if PROFILE
//	appResetProfiler(); -- there's nested appResetProfiler/appPrintProfiler calls, which are not supported
#endif

	BeginExport(true);

	// For each package: load a package, export, then release
	for (int i = 0; i < Packages.Num(); i++)
	{
		UnPackage* package = Packages[i];

		// Update progress dialog
		if (Progress && !Progress->Progress(package->Name, i, Packages.Num()))
		{
			cancelled = true;
			break;
		}
		// Load
		if (!LoadWholePackage(package, Progress))
		{
			cancelled = true;
			break;
		}
		// Export
		if (!ExportObjects(NULL, Progress))
		{
			cancelled = true;
			break;
		}
		// Release
		ReleaseAllObjects();
	}

	// Cleanup
	EndExport(true);

#if PROFILE
//	appPrintProfiler();
#endif

	if (cancelled)
	{
		ReleaseAllObjects();
		//!! message box
		appPrintf("Operation interrupted by user.\n");
	}

	return !cancelled;

	unguard;
}


void DisplayPackageStats(const TArray<UnPackage*> &Packages)
{
	if (Packages.Num() == 0)
	{
		appPrintf("Nothing has been loaded\n");
		return;
	}
	TArray<ClassStats> stats;
	CollectPackageStats(Packages, stats);

	appPrintf("Class statistics:\n");
	for (int i = 0; i < stats.Num(); i++)
		appPrintf("%5d %s\n", stats[i].Count, stats[i].Name);
}


static void CopyStream(FArchive *Src, FILE *Dst, int Count)
{
	guard(CopyStream);

	byte buffer[16384];

	while (Count > 0)
	{
		int Size = min(Count, sizeof(buffer));
		Src->Serialize(buffer, Size);
		if (fwrite(buffer, Size, 1, Dst) != 1) appError("Write failed");
		Count -= Size;
	}

	unguard;
}

void SavePackages(const TArray<const CGameFileInfo*>& Packages, IProgressCallback* Progress)
{
	guard(SavePackages);

	for (int i = 0; i < Packages.Num(); i++)
	{
		const CGameFileInfo* mainFile = Packages[i];

		assert(mainFile);
		FStaticString<MAX_PACKAGE_PATH> RelativeName;
		mainFile->GetRelativeName(RelativeName);
		if (Progress && !Progress->Progress(*RelativeName, i, Packages.Num()))
			break;

		// Find all files with the same name and different extension (e.g. ".uexp", ".ubulk")
		TStaticArray<const CGameFileInfo*, 32> allFiles;
		allFiles.Add(mainFile);
		mainFile->FindOtherFiles(allFiles);

		for (const CGameFileInfo* file : allFiles)
		{
			FArchive *Ar = file->CreateReader();
			if (Ar)
			{
				guard(SaveFile);
				// prepare destination file
				char OutFile[2048];
				FStaticString<MAX_PACKAGE_PATH> Name;
				if (GSettings.SavePackages.KeepDirectoryStructure)
				{
					file->GetRelativeName(Name);
					appSprintf(ARRAY_ARG(OutFile), "%s/%s", *GSettings.SavePackages.SavePath, *Name);
				}
				else
				{
					file->GetCleanName(Name);
					appSprintf(ARRAY_ARG(OutFile), "%s/%s", *GSettings.SavePackages.SavePath, *Name);
				}
				appMakeDirectoryForFile(OutFile);
				FILE *out = fopen(OutFile, "wb");
				// copy data
				CopyStream(Ar, out, Ar->GetFileSize());
				// cleanup
				delete Ar;
				fclose(out);
				unguardf("%s", *file->GetRelativeName());
			}
		}
	}

	unguard;
}
