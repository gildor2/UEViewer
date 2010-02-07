#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"

#define MAKE_DIRS		1

/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
#if DO_GUARD
	TRY {
#endif

	guard(Main);

	// display usage
	if (argc < 2)
	{
	help:
		printf(	"Unreal package extractor\n"
				"Usage: extract <package filename>\n"
		);
		exit(0);
	}

	// parse command line
//	bool dump = false, view = true, exprt = false, listOnly = false, noAnim = false, pkgInfo = false;
	int arg = 1;
/*	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!stricmp(opt, "dump"))
			{
			}
			else if (!stricmp(opt, "check"))
			{
			}
			else
				goto help;
		}
		else
		{
			break;
		}
	} */
	const char *argPkgName = argv[arg];
	if (!argPkgName) goto help;

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// load package
	UnPackage *Package;
	if (strchr(argPkgName, '.'))
		Package = new UnPackage(argPkgName);
	else
		Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		printf("ERROR: Unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	guard(ProcessPackage);
	// extract package name, create directory for it
	char PkgName[256];
	const char *s = strrchr(argPkgName, '/');
	if (!s) s = strrchr(argPkgName, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = argPkgName;
	appStrncpyz(PkgName, s, ARRAY_COUNT(PkgName));
	char *s2 = strchr(PkgName, '.');
	if (s2) *s2 = 0;
	appMakeDirectory(PkgName);
	// extract objects and write export table
	FILE *f;
	char buf2[1024];
	int idx;
	guard(ExtractObjects);
	appSprintf(ARRAY_ARG(buf2), "%s/ExportTable.txt", PkgName);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		FObjectExport &Exp = Package->ExportTable[idx];
		// prepare file
		const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
#if !MAKE_DIRS
		char buf3[256];
		buf3[0] = 0;
		if (Exp.PackageIndex) //?? GetObjectName() will return "Class" for index=0 ...
		{
			const char *Outer = Package->GetObjectName(Exp.PackageIndex);
			appSprintf(ARRAY_ARG(buf3), "%s.", Outer);
		}
		fprintf(f, "%d = %s'%s%s'\n", idx, ClassName, buf3, *Exp.ObjectName);
		appSprintf(ARRAY_ARG(buf2), "%s/%s%s.%s", buf, buf3, *Exp.ObjectName, ClassName);
#else
		int PackageIndices[256];
		int NestLevel = 0;
		int PackageIndex = Exp.PackageIndex;
		// gather nest packages
		while (PackageIndex)
		{
			PackageIndices[NestLevel++] = PackageIndex;
			assert(NestLevel < ARRAY_COUNT(PackageIndices));
			//!! duplicated code
			if (PackageIndex < 0)
			{
				const FObjectImport &Rec = Package->GetImport(-PackageIndex-1);
				PackageIndex = Rec.PackageIndex;
			}
			else
			{
				// possible for UE3 forced exports
				const FObjectExport &Rec = Package->GetExport(PackageIndex-1);
				PackageIndex = Rec.PackageIndex;
			}
		}
		// collect packages in reverse order (from root to object)
		fprintf(f, "%d = %s'", idx, ClassName);
		appSprintf(ARRAY_ARG(buf2), "%s/", PkgName);
		for (int i = NestLevel-1; i >= 0; i--)
		{
			int PackageIndex = PackageIndices[i];
			const char *PackageName = NULL;
			//!! duplicate code
			if (PackageIndex < 0)
			{
				const FObjectImport &Rec = Package->GetImport(-PackageIndex-1);
				PackageName = Rec.ObjectName;
			}
			else
			{
				// possible for UE3 forced exports
				const FObjectExport &Rec = Package->GetExport(PackageIndex-1);
				PackageName = Rec.ObjectName;
			}
			fprintf(f, "%s.", PackageName);
			char *dst = strchr(buf2, 0);
			appSprintf(dst, ARRAY_COUNT(buf2) - (dst - buf2), "%s/", PackageName);
		}
		fprintf(f, "%s'\n",  *Exp.ObjectName);
		char *dst = strchr(buf2, 0);
		//!! use UniqueNameList for names
		appSprintf(dst, ARRAY_COUNT(buf2) - (dst - buf2), "%s.%s", *Exp.ObjectName, ClassName);
		appMakeDirectoryForFile(buf2);
#endif
		guard(WriteFile);
		FILE *f2 = fopen(buf2, "wb");
		if (!f2)
		{
			//!! note: cannot create file with name "con" (any extension)
			printf("%d/%d: unable to create file %s\n", idx, Package->Summary.ExportCount, buf2);
			continue;
		}
		// read data
		byte *data = new byte[Exp.SerialSize];
		Package->Seek(Exp.SerialOffset);
		Package->Serialize(data, Exp.SerialSize);
		// write data
		fwrite(data, Exp.SerialSize, 1, f2);
		// cleanup
		delete data;
		fclose(f2);
		unguardf(("file=%s", buf2));
		// notification
		printf("Done: %d/%d ...\r", idx, Package->Summary.ExportCount);
	}
	fclose(f);
	printf("Done ...             \n");
	unguard;
	// write name table
	guard(WriteNameTable);
	appSprintf(ARRAY_ARG(buf2), "%s/NameTable.txt", PkgName);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.NameCount; idx++)
		fprintf(f, "%d = \"%s\"\n", idx, Package->NameTable[idx]);
	fclose(f);
	unguard;
	// write import table
	guard(WriteImportTable);
	appSprintf(ARRAY_ARG(buf2), "%s/ImportTable.txt", PkgName);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.ImportCount; idx++)
	{
		const FObjectImport &Imp = Package->GetImport(idx);
		// find root package
		//!! code from UnPackage.cpp, should generalize
		int PackageIndex = Imp.PackageIndex;
		const char *PackageName = NULL;
		//!! duplicated code
		while (PackageIndex)
		{
			if (PackageIndex < 0)
			{
				const FObjectImport &Rec = Package->GetImport(-PackageIndex-1);
				PackageIndex = Rec.PackageIndex;
				PackageName  = Rec.ObjectName;
			}
			else
			{
				// possible for UE3 forced exports
				const FObjectExport &Rec = Package->GetExport(PackageIndex-1);
				PackageIndex = Rec.PackageIndex;
				PackageName  = Rec.ObjectName;
			}
		}
		if (PackageName)
			fprintf(f, "%d = %s'%s.%s'\n", idx, *Imp.ClassName, PackageName, *Imp.ObjectName);
		else
			fprintf(f, "%d = %s'%s'\n", idx, *Imp.ClassName, *Imp.ObjectName);
	}
	fclose(f);
	unguard;

	unguard;

	unguard;

#if DO_GUARD
	} CATCH {
		if (GErrorHistory[0])
		{
//			printf("ERROR: %s\n", GErrorHistory);
			appNotify("ERROR: %s\n", GErrorHistory);
		}
		else
		{
//			printf("Unknown error\n");
			appNotify("Unknown error\n");
		}
		exit(1);
	}
#endif
	return 0;
}
