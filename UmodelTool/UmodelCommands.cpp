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
	for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
	{
		if (progress && !progress->Tick()) return false;
		UObject* ExpObj = UObject::GObjObjects[idx];
		bool objectSelected = !hasObjectList || (Objects->FindItem(ExpObj) >= 0);

		if (!objectSelected) continue;

		if (notifyPackage != ExpObj->Package)
		{
			notifyPackage = ExpObj->Package;
			appSetNotifyHeader(notifyPackage->Filename);
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

	// We'll entirely unload all objects, so we should reset viewer. Note: CUmodelApp does this too,
	// however we're calling ExportPackages() from different code places, so call it anyway.
	GApplication.ReleaseViewerAndObjects();

	bool cancelled = false;

#if PROFILE
//	appResetProfiler(); -- there's nested appResetProfiler/appPrintProfiler calls, which are not supported
#endif

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
	ResetExportedList();

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
	TArray<ClassStats> stats;
	CollectPackageStats(Packages, stats);

	appPrintf("Class statistics:\n");
	for (int i = 0; i < stats.Num(); i++)
		appPrintf("%5d %s\n", stats[i].Count, stats[i].Name);
}
