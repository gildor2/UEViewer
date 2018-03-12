#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"		// for Package->Name

#include "Exporters.h"


// configuration variables
bool GExportScripts      = false;
bool GExportLods         = false;
bool GDontOverwriteFiles = false;


/*-----------------------------------------------------------------------------
	Exporter function management
-----------------------------------------------------------------------------*/

#define MAX_EXPORTERS		20

struct CExporterInfo
{
	const char		*ClassName;
	ExporterFunc_t	Func;
};

static CExporterInfo exporters[MAX_EXPORTERS];
static int numExporters = 0;

void RegisterExporter(const char *ClassName, ExporterFunc_t Func)
{
	guard(RegisterExporter);
	assert(numExporters < MAX_EXPORTERS);
	CExporterInfo &Info = exporters[numExporters];
	Info.ClassName = ClassName;
	Info.Func      = Func;
	numExporters++;
	unguard;
}


// List of already exported objects

#define EXPORTED_LIST_HASH_SIZE		4096

struct ExportedObjectEntry
{
	const UnPackage* Package;
	int				ExportIndex;
	int				HashNext;

	ExportedObjectEntry()
	{}

	ExportedObjectEntry(const UObject* Obj)
	:	Package(Obj->Package)
	,	ExportIndex(Obj->PackageIndex)
	,	HashNext(0)
	{}

	int GetHash() const
	{
		return ( ((size_t)Package >> 3) ^ ExportIndex ^ (ExportIndex << 4) ) & (EXPORTED_LIST_HASH_SIZE - 1);
	}
};

static TArray<ExportedObjectEntry> ProcessedObjects;
static int ProcessedObjectHash[EXPORTED_LIST_HASH_SIZE];

void ResetExportedList()
{
	ProcessedObjects.Empty(1024);
}

// return 'false' if object already registered
static bool RegisterProcessedObject(const UObject* Obj)
{
	guard(RegisterProcessedObject);

	if (Obj->Package == NULL || Obj->PackageIndex < 0)
	{
		// this object was generated; always export it to not write a more complex code here
		// Example: UMaterialWithPolyFlags
		return true;
	}

	if (ProcessedObjects.Num() == 0)
	{
		// we're adding first item here, initialize hash with -1
		memset(ProcessedObjectHash, -1, sizeof(ProcessedObjectHash));
	}

	ExportedObjectEntry exp(Obj);
	int h = exp.GetHash();

//	appPrintf("Register: %s/%s/%s (%d) : ", Obj->Package->Name, Obj->GetClassName(), Obj->Name, ProcessedObjects.Num());

	int newIndex = -1;
	const ExportedObjectEntry* expEntry;
	for (newIndex = ProcessedObjectHash[h]; newIndex >= 0; newIndex = expEntry->HashNext)
	{
//		appPrintf("-- %d ", newIndex);
		expEntry = &ProcessedObjects[newIndex];
		if ((expEntry->Package == exp.Package) && (expEntry->ExportIndex == exp.ExportIndex))
		{
//			appPrintf("-> FOUND\n");
			return false;		// the object already exists
		}
	}

	// not registered yet
	newIndex = ProcessedObjects.Add(exp);
	ProcessedObjects[newIndex].HashNext = ProcessedObjectHash[h];
	ProcessedObjectHash[h] = newIndex;
//	appPrintf("-> none\n");

	return true;

	unguard;
}

struct UniqueNameList
{
	UniqueNameList()
	{
		Items.Empty(1024);
	}

	struct Item
	{
		FString Name;
		int Count;
	};
	TArray<Item> Items;

	int RegisterName(const char *Name)
	{
		for (int i = 0; i < Items.Num(); i++)
		{
			Item &V = Items[i];
			if (V.Name == Name)
			{
				return ++V.Count;
			}
		}
		Item *N = new (Items) Item;
		N->Name = Name;
		N->Count = 1;
		return 1;
	}
};

bool ExportObject(const UObject *Obj)
{
	guard(ExportObject);

	if (!Obj) return false;
	if (strnicmp(Obj->Name, "Default__", 9) == 0)	// default properties object, nothing to export
		return true;

	static UniqueNameList ExportedNames;

	// check for duplicate object export
	if (!RegisterProcessedObject(Obj)) return true;

	for (int i = 0; i < numExporters; i++)
	{
		const CExporterInfo &Info = exporters[i];
		if (Obj->IsA(Info.ClassName))
		{
			char ExportPath[1024];
			strcpy(ExportPath, GetExportPath(Obj));
			const char *ClassName  = Obj->GetClassName();
			// check for duplicate name
			// get name unique index
			char uniqueName[256];
			appSprintf(ARRAY_ARG(uniqueName), "%s/%s.%s", ExportPath, Obj->Name, ClassName);
			int uniqieIdx = ExportedNames.RegisterName(uniqueName);
			const char *OriginalName = NULL;
			if (uniqieIdx >= 2)
			{
				appSprintf(ARRAY_ARG(uniqueName), "%s_%d", Obj->Name, uniqieIdx);
				appPrintf("Duplicate name %s found for class %s, renaming to %s\n", Obj->Name, ClassName, uniqueName);
				//?? HACK: temporary replace object name with unique one
				OriginalName = Obj->Name;
				const_cast<UObject*>(Obj)->Name = uniqueName;
			}

			appPrintf("Exporting %s %s to %s\n", Obj->GetClassName(), Obj->Name, ExportPath);
			Info.Func(Obj);

			//?? restore object name
			if (OriginalName) const_cast<UObject*>(Obj)->Name = OriginalName;
			return true;
		}
	}
	return false;

	unguardf("%s'%s'", Obj->GetClassName(), Obj->Name);
}


/*-----------------------------------------------------------------------------
	Export path functions
-----------------------------------------------------------------------------*/

static char BaseExportDir[512];

bool GUncook    = false;
bool GUseGroups = false;

void appSetBaseExportDirectory(const char *Dir)
{
	strcpy(BaseExportDir, Dir);
}


const char* GetExportPath(const UObject *Obj)
{
	guard(GetExportPath);

	static char buf[1024]; // will be returned outside

	if (!BaseExportDir[0])
		appSetBaseExportDirectory(".");	// to simplify code

#if UNREAL4
	if (Obj->Package && Obj->Package->Game >= GAME_UE4_BASE)
	{
		// Special path for UE4 games - its packages are usually have 1 asset per file, plus
		// package names could be duplicated across directory tree, with use of full package
		// paths to identify packages.
		const char* PackageName = Obj->Package->Filename;
		// Package name could be:
		// a) /(GameName|Engine)/Content/... - when loaded from pak file
		// b) [[GameName/]Content/]... - when not packaged to pak file
		if (PackageName[0] == '/') PackageName++;
		if (!strnicmp(PackageName, "Content/", 8))
		{
			PackageName += 8;
		}
		else
		{
			const char* s = strchr(PackageName, '/');
			if (s && !strnicmp(s+1, "Content/", 8))
			{
				// skip 'Content'
				PackageName = s + 9;
			}
		}
		appSprintf(ARRAY_ARG(buf), "%s/%s", BaseExportDir, PackageName);
		// Check if object's name is the same as uasset name, or if it is the same as uasset with added "_suffix".
		// Suffix may be added by ExportObject (see 'uniqieIdx').
		int len = strlen(Obj->Package->Name);
		if (!strnicmp(Obj->Name, Obj->Package->Name, len) && (Obj->Name[len] == 0 || Obj->Name[len] == '_'))
		{
			// Object's name matches with package name, so don't create a directory for it.
			// Strip package name, leave only path.
			char* s = strrchr(buf, '/');
			if (s) *s = 0;
		}
		else
		{
			// Multiple objects could be placed in this package. Strip only package's extension.
			char* s = strrchr(buf, '.');
			if (s) *s = 0;
		}
		return buf;
	}
#endif // UNREAL4

	const char *PackageName = "None";
	if (Obj->Package)
	{
		PackageName = (GUncook) ? Obj->GetUncookedPackageName() : Obj->Package->Name;
	}

	static char group[512];
	if (GUseGroups)
	{
		// get group name
		// include cooked package name when not uncooking
		Obj->GetFullName(ARRAY_ARG(group), false, !GUncook);
		// replace all '.' with '/'
		for (char *s = group; *s; s++)
			if (*s == '.') *s = '/';
	}
	else
	{
		strcpy(group, Obj->GetClassName());
	}

	appSprintf(ARRAY_ARG(buf), "%s/%s%s%s", BaseExportDir, PackageName,
		(group[0]) ? "/" : "", group);
	return buf;

	unguard;
}


const char* GetExportFileName(const UObject *Obj, const char *fmt, va_list args)
{
	guard(GetExportFileName);

	char fmtBuf[256];
	int len = vsnprintf(ARRAY_ARG(fmtBuf), fmt, args);
	if (len < 0 || len >= sizeof(fmtBuf) - 1) return NULL;

	static char buffer[1024];
	appSprintf(ARRAY_ARG(buffer), "%s/%s", GetExportPath(Obj), fmtBuf);
	return buffer;

	unguard;
}


const char* GetExportFileName(const UObject *Obj, const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	const char *filename = GetExportFileName(Obj, fmt, argptr);
	va_end(argptr);

	return filename;
}


bool CheckExportFilePresence(const UObject *Obj, const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	const char *filename = GetExportFileName(Obj, fmt, argptr);
	va_end(argptr);

	if (!filename) return false;
	return appFileExists(filename);
}


FArchive *CreateExportArchive(const UObject *Obj, const char *fmt, ...)
{
	guard(CreateExportArchive);

	va_list	argptr;
	va_start(argptr, fmt);
	const char *filename = GetExportFileName(Obj, fmt, argptr);
	va_end(argptr);

	if (!filename) return NULL;

	if (GDontOverwriteFiles)
	{
		// check file presence
		if (appFileExists(filename)) return NULL;
	}

//	appPrintf("... writing %s'%s' to %s ...\n", Obj->GetClassName(), Obj->Name, filename);

	appMakeDirectoryForFile(filename);
	FFileWriter *Ar = new FFileWriter(filename, FRO_NoOpenError);
	if (!Ar->IsOpen())
	{
		appPrintf("Error opening file \"%s\" ...\n", filename);
		delete Ar;
		return NULL;
	}

	Ar->ArVer = 128;			// less than UE3 version (required at least for VJointPos structure)

	return Ar;

	unguard;
}
