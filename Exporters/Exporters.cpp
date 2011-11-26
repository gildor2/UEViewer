#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"		// for Package->Name

#include "Exporters.h"


// configuration variables
bool GExportScripts      = false;
bool GExportLods         = false;
bool GExportPskx         = false;
bool GDontOverwriteFiles = false;


/*-----------------------------------------------------------------------------
	Exporter function management
-----------------------------------------------------------------------------*/

#define MAX_EXPORTERS		16

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


static TArray<const UObject*> ProcessedObjects;

bool ExportObject(const UObject *Obj)
{
	guard(ExportObject);

	if (!Obj) return false;

	static UniqueNameList ExportedNames;

	// check for duplicate object export
	if (ProcessedObjects.FindItem(Obj) != INDEX_NONE) return true;
	ProcessedObjects.AddItem(Obj);

	for (int i = 0; i < numExporters; i++)
	{
		const CExporterInfo &Info = exporters[i];
		if (Obj->IsA(Info.ClassName))
		{
			char ExportPath[1024];
			strcpy(ExportPath, GetExportPath(Obj));
			const char *ClassName  = Obj->GetClassName();
			// check for duplicate name
			// get name uniqie index
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

	unguardf(("%s'%s'", Obj->GetClassName(), Obj->Name));
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
	if (!BaseExportDir[0])
		appSetBaseExportDirectory(".");	// to simplify code

	const char *PackageName = (GUncook) ? Obj->GetUncookedPackageName() : Obj->Package->Name;

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

	static char buf[1024];
	appSprintf(ARRAY_ARG(buf), "%s/%s%s%s", BaseExportDir, PackageName,
		(group[0]) ? "/" : "", group);
	return buf;
}


FArchive *CreateExportArchive(const UObject *Obj, const char *fmt, ...)
{
	guard(CreateExportArchive);

	va_list	argptr;
	va_start(argptr, fmt);
	char fmtBuf[256];
	int len = vsnprintf(ARRAY_ARG(fmtBuf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(fmtBuf) - 1) return NULL;

	char buffer[1024];
	appSprintf(ARRAY_ARG(buffer), "%s/%s", GetExportPath(Obj), fmtBuf);

	if (GDontOverwriteFiles)
	{
		// check file presence
		FILE *f = fopen(buffer, "r");
		if (f)
		{
			fclose(f);
			return NULL;
		}
	}

//	appPrintf("... writting %s'%s' to %s ...\n", Obj->GetClassName(), Obj->Name, buffer);

	appMakeDirectoryForFile(buffer);
	FFileReader *Ar = new FFileReader(buffer, false, true);	// open file for writting, disable error when open is failed
	if (!Ar->IsOpen())
	{
		appPrintf("Error opening file \"%s\" ...\n", buffer);
		delete Ar;
		return NULL;
	}

	Ar->ArVer = 128;			// less than UE3 version (required at least for VJointPos structure)

	return Ar;

	unguard;
}
