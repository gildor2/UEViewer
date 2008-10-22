#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


/*-----------------------------------------------------------------------------
	Package loading (creation) / unloading
-----------------------------------------------------------------------------*/

UnPackage::UnPackage(const char *filename)
:	FFileReader(filename)
{
	guard(UnPackage::UnPackage);

	appStrncpyz(Filename, filename, ARRAY_COUNT(Filename));

	// read summary
	*this << Summary;
	if (Summary.Tag != PACKAGE_FILE_TAG)
		appError("Wrong tag in package %s\n", Filename);
	ArVer         = Summary.FileVersion;
	ArLicenseeVer = Summary.LicenseeVersion;
	PKG_LOG(("Loading package: %s Ver: %d/%d Names: %d Exports: %d Imports: %d\n", Filename,
		Summary.FileVersion, Summary.LicenseeVersion,
		Summary.NameCount, Summary.ExportCount, Summary.ImportCount));

	// read name table
	if (Summary.NameCount > 0)
	{
		Seek(Summary.NameOffset);
		NameTable = new char*[Summary.NameCount];
		for (int i = 0; i < Summary.NameCount; i++)
		{
			if (Summary.FileVersion < 64)
			{
				char buf[256];
				int len;
				for (len = 0; len < ARRAY_COUNT(buf); len++)
				{
					char c;
					*this << c;
					buf[len] = c;
					if (!c) break;
				}
				assert(len < ARRAY_COUNT(buf));
				NameTable[i] = strdup(buf);
				// skip object flags
				int tmp;
				*this << tmp;
			}
			else
			{
				int len;
				*this << AR_INDEX(len);
				char *s = NameTable[i] = new char[len];
				while (len-- > 0)
					*this << *s++;
//				PKG_LOG(("Name[%d]: \"%s\"\n", i, NameTable[i]));
				// skip object flags
				int tmp;
				*this << tmp;
			}
		}
	}

	// load import table
	if (Summary.ImportCount > 0)
	{
		Seek(Summary.ImportOffset);
		FObjectImport *Imp = ImportTable = new FObjectImport[Summary.ImportCount];
		for (int i = 0; i < Summary.ImportCount; i++, Imp++)
		{
			*this << *Imp;
//			PKG_LOG(("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName));
		}
	}

	// load exports table
	if (Summary.ExportCount > 0)
	{
		Seek(Summary.ExportOffset);
		FObjectExport *Exp = ExportTable = new FObjectExport[Summary.ExportCount];
		for (int i = 0; i < Summary.ExportCount; i++, Exp++)
		{
			*this << *Exp;
//			PKG_LOG(("Export[%d]: %s'%s' offs=%08X size=%08X\n", i, GetObjectName(Exp->ClassIndex),
//				*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize));
		}
	}

	// add self to package map
	PackageEntry &Info = PackageMap[PackageMap.Add()];
	char buf[256];
	const char *s = strrchr(filename, '/');
	if (!s) s = strrchr(filename, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = filename;
	appStrncpyz(buf, s, ARRAY_COUNT(buf));
	char *s2 = strchr(buf, '.');
	if (s2) *s2 = 0;
	appStrncpyz(Info.Name, buf, ARRAY_COUNT(Info.Name));
	Info.Package = this;

	unguardf(("%s", filename));
}


UnPackage::~UnPackage()
{
	// free resources
	int i;
	for (i = 0; i < Summary.NameCount; i++)
		free(NameTable[i]);
	delete NameTable;
	delete ImportTable;
	delete ExportTable;
	// remove self from package table
	for (i = 0; i < PackageMap.Num(); i++)
		if (PackageMap[i].Package == this)
		{
			PackageMap.Remove(i);
			break;
		}
}


/*-----------------------------------------------------------------------------
	Loading particular import or export package entry
-----------------------------------------------------------------------------*/

UObject* UnPackage::CreateExport(int index)
{
	guard(UnPackage::CreateExport);

	// create empty object
	FObjectExport &Exp = GetExport(index);
	if (Exp.Object)
		return Exp.Object;

	const char *ClassName = GetObjectName(Exp.ClassIndex);
	UObject *Obj = Exp.Object = CreateClass(ClassName);
	if (!Obj)
	{
		printf("WARNING: Unknown class \"%s\" for object \"%s\"\n", ClassName, *Exp.ObjectName);
		return NULL;
	}
	UObject::BeginLoad();

	// setup constant object fields
	Obj->Package      = this;
	Obj->PackageIndex = index;
	Obj->Name         = Exp.ObjectName;
	// add object to GObjLoaded for later serialization
	UObject::GObjLoaded.AddItem(Obj);

	UObject::EndLoad();
	return Obj;

	unguardf(("%s:%d", Filename, index));
}


UObject* UnPackage::CreateImport(int index)
{
	guard(UnPackage::CreateImport);

	const FObjectImport &Imp = GetImport(index);

	// find root package
	int PackageIndex = Imp.PackageIndex;
	const char *PackageName = NULL;
	while (PackageIndex)
	{
		const FObjectImport &Rec = GetImport(-PackageIndex-1);
		PackageIndex = Rec.PackageIndex;
		PackageName  = Rec.ObjectName;
	}
	// load package
	UnPackage  *Package = LoadPackage(PackageName);

	if (!Package)
	{
		printf("WARNING: Import(%s): package %s was not found\n", *Imp.ObjectName, PackageName);
		return NULL;
	}
	//!! use full object path
	// find object in loaded package export table
	int NewIndex = Package->FindExport(Imp.ObjectName, Imp.ClassName);
	if (NewIndex == INDEX_NONE)
	{
		printf("WARNING: Import(%s) was not found in package %s\n", *Imp.ObjectName, PackageName);
		return NULL;
	}
	// create object
	return Package->CreateExport(NewIndex);

	unguardf(("%s:%d", Filename, index));
}


/*-----------------------------------------------------------------------------
	Searching for package and maintaining package list
-----------------------------------------------------------------------------*/

TArray<UnPackage::PackageEntry> UnPackage::PackageMap;
char							UnPackage::SearchPath[256];


void UnPackage::SetSearchPath(const char *Path)
{
	appStrncpyz(SearchPath, Path, ARRAY_COUNT(SearchPath));
}


static const char *PackageExtensions[] =
{
	"u", "ut2", "utx", "uax", "usx", "ukx"
#if RUNE
	, "ums"
#endif
#if TRIBES3
	, "pkg"
#endif
};

static const char *PackagePaths[] =
{
	"",
	"Animations/",
	"Maps/",
	"Sounds/",
	"StaticMeshes/",
	"System/",
	"Textures/"
};


UnPackage *UnPackage::LoadPackage(const char *Name)
{
	// check in loaded packages list
	for (int i = 0; i < PackageMap.Num(); i++)
		if (!stricmp(Name, PackageMap[i].Name))
			return PackageMap[i].Package;

	// find package file
	for (int path = 0; path < ARRAY_COUNT(PackagePaths); path++)
		for (int ext = 0; ext < ARRAY_COUNT(PackageExtensions); ext++)
		{
			// build filename
			char	buf[256];
			appSprintf(ARRAY_ARG(buf), "%s%s" "%s%s" ".%s",
				SearchPath, SearchPath[0] ? "/" : "",
				PackagePaths[path],
				Name,
				PackageExtensions[ext]);
//			printf("... check: %s\n", buf);
			// check file existance
			if (FILE *f = fopen(buf, "rb"))
			{
				fclose(f);
				return new UnPackage(buf);
			}
		}
	return NULL;
}
