#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnrealPackage/UnPackage.h"	// for Package->Name

#include "Exporters.h"

#include "Parallel.h"

// configuration variables
bool GExportScripts      = false;
bool GExportLods         = false;
bool GDontOverwriteFiles = false;

bool GExportInProgress   = false;

bool GDummyExport        = false;


/*-----------------------------------------------------------------------------
	Exporter function management
-----------------------------------------------------------------------------*/

#define MAX_EXPORTERS		20
//#define DEBUG_DUP_FINDER	1

struct CExporterInfo
{
	const char* ClassName;
	ExporterFunc_t	Func;
};

static CExporterInfo exporters[MAX_EXPORTERS];
static int numExporters = 0;

void RegisterExporter(const char* ClassName, ExporterFunc_t Func)
{
	guard(RegisterExporter);
	assert(numExporters < MAX_EXPORTERS);
	CExporterInfo& Info = exporters[numExporters];
	Info.ClassName = ClassName;
	Info.Func = Func;
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

struct ExportContext
{
	const UObject* LastExported;
	TArray<ExportedObjectEntry> Objects;
	int ObjectHash[EXPORTED_LIST_HASH_SIZE];
	unsigned long startTime;
	int NumSkippedObjects;

	ExportContext()
	{
		Reset();
	}

	void BeginExport()
	{
		Objects.Empty(1024);
	}

	void Reset()
	{
		LastExported = NULL;
		NumSkippedObjects = 0;
		Objects.Empty();
		memset(ObjectHash, -1, sizeof(ObjectHash));
	}

	bool ItemExists(const UObject* Obj)
	{
		guard(ExportContext::ItemExists);

		ExportedObjectEntry item(Obj);
		int h = item.GetHash();
//		appPrintf("Register: %s/%s/%s (%d) : ", Obj->Package->Name, Obj->GetClassName(), Obj->Name, ProcessedObjects.Num());

		//todo: in general, this is a logic of 'TSet<UObject*>'
		int newIndex = -1;
		const ExportedObjectEntry* expEntry;
		for (newIndex = ObjectHash[h]; newIndex >= 0; newIndex = expEntry->HashNext)
		{
//			appPrintf("-- %d ", newIndex);
			expEntry = &Objects[newIndex];
			if ((expEntry->Package == item.Package) && (expEntry->ExportIndex == item.ExportIndex))
			{
//				appPrintf("-> FOUND\n");
				return true;		// the object already exists
			}
		}
		return false;

		unguard;
	}

	// Return 'false' if object already exists in a list, otherwise adds it and returns 'true'
	bool AddItem(const UObject* Obj)
	{
		guard(ExportContext::AddItem);

		if (ItemExists(Obj))
			return false;

		// not registered yet
		ExportedObjectEntry item(Obj);
		int h = item.GetHash();
		int newIndex = Objects.Add(item);
		Objects[newIndex].HashNext = ObjectHash[h];
		ObjectHash[h] = newIndex;
//		appPrintf("-> none\n");

		return true;

		unguard;
	}
};

static ExportContext ctx;

static bool OnObjectLoad(UObject* Obj)
{
	guard(OnObjectLoad);

	assert(Obj);
	const char* Class = Obj->GetClassName();

	if (strncmp(Class, "Texture", 7) == 0)
	{
		// For UE3/UE4, the same texture may be reused many times from different materials.
		// In test case, 580 textures were loaded 18000 times. This callback allows to replace
		// these textures with dummies as soon as they've got exported
		bool result = IsObjectExported(Obj) == false;
//		if (!result) appPrintf("SKIP: %s\n", Obj->Name);
		return result;
	}
	return true;

	unguard;
}

void BeginExport(bool bBatch)
{
	GExportInProgress = bBatch; // only signal that we're doing export if not doing that from the viewer
	GBeforeLoadObjectCallback = OnObjectLoad;
	ctx.BeginExport();
	ctx.startTime = appMilliseconds();
}

void EndExport(bool profile)
{
//	assert(GExportInProgress); - in non-batch export this might be 'false'

#if THREADING
	// Wait for all workers to complete
	ThreadPool::WaitForCompletion();
#endif

	GExportInProgress = false;
	GBeforeLoadObjectCallback = NULL;

	if (profile)
	{
		assert(ctx.startTime);
		unsigned long elapsedTime = appMilliseconds() - ctx.startTime;
		appPrintf("Exported %d/%d objects in %.1f sec\n", ctx.Objects.Num() - ctx.NumSkippedObjects, ctx.Objects.Num(), elapsedTime / 1000.0f);
	}
	ctx.startTime = 0;

	ctx.Reset();
}

// return 'false' if object already registered
//todo: make a method of 'ctx' as this function is 1) almost empty, 2) not public
static bool RegisterProcessedObject(const UObject* Obj)
{
	if (Obj->Package == NULL || Obj->PackageIndex < 0)
	{
		// this object was generated; always export it to not write a more complex code here
		// Example: UMaterialWithPolyFlags
		return true;
	}

	return ctx.AddItem(Obj);
}

bool IsObjectExported(const UObject* Obj)
{
	return ctx.ItemExists(Obj);
}

//todo: move to ExportContext and reset with ctx.Reset()?

// Use CRC32 for hashing, from zlib
extern "C" unsigned long crc32(unsigned long crc, const byte* buf, unsigned int len);

struct CUniqueNameList
{
	typedef uint32 Hash_t;

	CUniqueNameList()
	{
		Items.Empty(1024);
	}

	struct Item
	{
		Item()
		{
			memset(this, 0, sizeof(*this));
		}

		FString			Name;
		int				Count;
		Hash_t			FirstHash;		// use separate Hash_t to avoid using TArray as much as possible
		TArray<Hash_t>	OtherHashes;
	};
	TArray<Item> Items;

	static Hash_t GetHash(const byte* Buf, int Size)
	{
		Hash_t crc = crc32(0, NULL, 0);
		crc = crc32(crc, Buf, (unsigned)Size);
		return crc;
	}

	int RegisterName(const char* Name, const TArray<byte>& Meta)
	{
		guard(CUniqueNameList::RegisterName);

		// Get hash of the object. Value 0 will mean "no metadata present".
		Hash_t Hash = 0;
		if (Meta.Num() != 0)
		{
			Hash = GetHash(Meta.GetData(), Meta.Num());
		}

		// Find the object using name id
		Item* foundItem = NULL;
		for (Item& V : Items)
		{
			if (V.Name == Name)
			{
				foundItem = &V;
				break;
			}
		}

		if (foundItem == NULL)
		{
			// New item
			Item *N = new (Items) Item();
			N->Name = Name;
			N->Count = 1;
			N->FirstHash = Hash;
			// Return '1' indicating that this is first appearance of the object name
			return 1;
		}

		// Object with same name already exists, walk over hashes to find match
		if (Hash == 0)
		{
			// No hash, treat all objects as unique
			return ++foundItem->Count;
		}

		if (foundItem->FirstHash == Hash)
			return 1;

		for (int i = 0; i < foundItem->OtherHashes.Num(); i++)
		{
			if (foundItem->OtherHashes[i] == Hash)
				return i + 2;	// found this hash
		}
		// This hash wasn't found
		foundItem->OtherHashes.Add(Hash);
		foundItem->Count++;
		assert(foundItem->Count == foundItem->OtherHashes.Num() + 1);
		return foundItem->Count;

		unguard;
	}
};

bool ExportObject(const UObject *Obj)
{
	guard(ExportObject);
	PROFILE_LABEL(Obj->GetClassName());

	if (!Obj) return false;
	if (strnicmp(Obj->Name, "Default__", 9) == 0)	// default properties object, nothing to export
		return true;

	// Check if exactly the same object was already exported. It refers to package.object and not
	// taking into account that the same object may reside in different packages in cooked UE3 game.
	if (IsObjectExported(Obj))
		return true;

	static CUniqueNameList ExportedNames;

	// For "uncook", different packages may have copies of the same object, which are stored with different quality.
	// For example, Gears3 has anim sets which cooked with different tracks into different maps. To be able to export
	// all versions of the file, we're adding unique numeric suffix for that. UE2 and UE4 doesn't require that.
	bool bAddUniqueSuffix = false;
	if (GUncook && Obj->Package && (Obj->Package->Game >= GAME_UE3) && (Obj->Package->Game < GAME_UE4_BASE))
	{
		bAddUniqueSuffix = true;
	}

	for (int i = 0; i < numExporters; i++)
	{
		const CExporterInfo &Info = exporters[i];
		if (Obj->IsA(Info.ClassName))
		{
			char ExportPath[1024];
			strcpy(ExportPath, GetExportPath(Obj));
			const char* ClassName = Obj->GetClassName();

			// Check for duplicate name
			const char* OriginalName = NULL;
			if (bAddUniqueSuffix)
			{
				// Get object's unique key from its name
				char uniqueName[1024];
				appSprintf(ARRAY_ARG(uniqueName), "%s/%s.%s", ExportPath, Obj->Name, ClassName);

				// Get object metadata for better detection of duplicates
				FMemWriter MetaCollector;
				Obj->GetMetadata(MetaCollector);

				// Add unique numeric suffix when needed
				const TArray<byte>& Meta = MetaCollector.GetData();
				int uniqueIdx = ExportedNames.RegisterName(uniqueName, Meta);
#if DEBUG_DUP_FINDER
				char buf[512];
				appSprintf(ARRAY_ARG(buf), "%s -> %d", uniqueName, uniqueIdx);
				if (Meta.Num())
				{
					DUMP_MEM_BYTES(&Meta[0], Meta.Num(), buf);
				}
#endif // DEBUG_DUP_FINDER
				if (uniqueIdx >= 2)
				{
					// Find existing object name with same metadata, or register a new name
					appSprintf(ARRAY_ARG(uniqueName), "%s_%d", Obj->Name, uniqueIdx);
					appPrintf("Duplicate name %s found for class %s, renaming to %s\n", Obj->Name, ClassName, uniqueName);
					//?? HACK: temporary replace object name with unique one
					OriginalName = Obj->Name;
					const_cast<UObject*>(Obj)->Name = uniqueName;
				}
			}

			// Do the export with saving current "LastExported" value. This will fix an issue when object exporter
			// will call another ExportObject function then continue exporting - without the fix, calling CreateExportArchive()
			// will always fail because code will recognize object as exported for 2nd time.
			const UObject* saveLastExported = ctx.LastExported;
			Info.Func(Obj);
			ctx.LastExported = saveLastExported;

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

bool GUncook = false;
bool GUseGroups = false;

void appSetBaseExportDirectory(const char* Dir)
{
	strcpy(BaseExportDir, Dir);
}


const char* GetExportPath(const UObject* Obj)
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
		FString PackageNameStr = *Obj->Package->GetFilename();
		const char* PackageName = *PackageNameStr;
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
		// Suffix may be added by ExportObject (see 'uniqueIdx').
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

	const char* PackageName = "None";
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
		for (char* s = group; *s; s++)
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


const char* GetExportFileName(const UObject* Obj, const char* fmt, va_list args)
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


const char* GetExportFileName(const UObject* Obj, const char* fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	const char* filename = GetExportFileName(Obj, fmt, argptr);
	va_end(argptr);

	return filename;
}


//todo: the function is not used
bool CheckExportFilePresence(const UObject* Obj, const char* fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	const char* filename = GetExportFileName(Obj, fmt, argptr);
	va_end(argptr);

	if (!filename) return false;
	return appFileExists(filename);
}


FArchive* CreateExportArchive(const UObject* Obj, EFileArchiveOptions FileOptions, const char* fmt, ...)
{
	guard(CreateExportArchive);

	bool bNewObject = false;
	if (ctx.LastExported != Obj)
	{
		// Exporting a new object, should perform some actions
		if (!RegisterProcessedObject(Obj))
			return NULL; // already exported
		bNewObject = true;
		ctx.LastExported = Obj;
	}

	va_list	argptr;
	va_start(argptr, fmt);
	const char* filename = GetExportFileName(Obj, fmt, argptr);
	va_end(argptr);

	if (!filename) return NULL;

	if (bNewObject)
	{
		// Check for file overwrite only when "new" object is saved. When saving 2nd part of the object - keep
		// overwrite logic for upper code level. If 1st object part was successfully created, then allow creation
		// of the 2nd part even if "don't overwrite" is enabled, and 2nd file already exists.
		if ((GDontOverwriteFiles && appFileExists(filename)) == false)
		{
			appPrintf("Exporting %s %s to %s\n", Obj->GetClassName(), Obj->Name, filename);
		}
		else
		{
			appPrintf("Export: file already exists %s\n", filename);
			ctx.NumSkippedObjects++;
			return NULL;
		}
	}

	if (GDummyExport)
	{
		return new FDummyArchive();
	}

	appMakeDirectoryForFile(filename);
	FFileWriter *Ar = new FFileWriter(filename, EFileArchiveOptions::NoOpenError | FileOptions);
	if (!Ar->IsOpen())
	{
		appPrintf("Error creating file \"%s\" ...\n", filename);
		delete Ar;
		return NULL;
	}

	Ar->ArVer = 128;			// less than UE3 version (required at least for VJointPos structure)

	return Ar;

	unguard;
}
