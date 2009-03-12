#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"

#if _WIN32
#include <direct.h>					// for mkdir()
#else
#include <sys/stat.h>				// for mkdir()
#endif


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
#if DO_GUARD
	try {
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
	bool dump = false, view = true, exprt = false, listOnly = false, noAnim = false, pkgInfo = false;
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
	char buf[256];
	const char *s = strrchr(argPkgName, '/');
	if (!s) s = strrchr(argPkgName, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = argPkgName;
	appStrncpyz(buf, s, ARRAY_COUNT(buf));
	char *s2 = strchr(buf, '.');
	if (s2) *s2 = 0;
#if _WIN32
	_mkdir(buf);
#else
	mkdir(buf, S_IRWXU);
#endif
	// extract objects and write export table
	FILE *f;
	char buf2[512];
	int idx;
	guard(ExtractObjects);
	appSprintf(ARRAY_ARG(buf2), "%s/ExportTable.txt", buf);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		FObjectExport &Exp = Package->ExportTable[idx];
		// prepare file
		const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
		char buf3[256];
		buf3[0] = 0;
		if (Exp.PackageIndex) //?? GetObjectName() will return "Class" for index=0 ...
		{
			const char *Outer = Package->GetObjectName(Exp.PackageIndex);
			appSprintf(ARRAY_ARG(buf3), "%s.", Outer);
		}
		fprintf(f, "%d = %s'%s%s'\n", idx, ClassName, buf3, *Exp.ObjectName);
		appSprintf(ARRAY_ARG(buf2), "%s/%s%s.%s", buf, buf3, *Exp.ObjectName, ClassName);
		FILE *f2 = fopen(buf2, "wb");
		assert(f2);
		// read data
		byte *data = new byte[Exp.SerialSize];
		Package->Seek(Exp.SerialOffset);
		Package->Serialize(data, Exp.SerialSize);
		// write data
		fwrite(data, Exp.SerialSize, 1, f2);
		// cleanup
		delete data;
		fclose(f2);
		// notification
		printf("Done: %d/%d ...\r", idx, Package->Summary.ExportCount);
	}
	fclose(f);
	printf("Done ...             \n");
	unguard;
	// write name table
	guard(WriteNameTable);
	appSprintf(ARRAY_ARG(buf2), "%s/NameTable.txt", buf);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.NameCount; idx++)
		fprintf(f, "%d = \"%s\"\n", idx, Package->NameTable[idx]);
	fclose(f);
	unguard;
	// write import table
	guard(WriteImportTable);
	appSprintf(ARRAY_ARG(buf2), "%s/ImportTable.txt", buf);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.ImportCount; idx++)
	{
		const FObjectImport &Imp = Package->GetImport(idx);
		// find root package
		//!! code from UnPackage.cpp, should generalize
		int PackageIndex = Imp.PackageIndex;
		const char *PackageName = NULL;
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
#if UNREAL3
				// possible for UE3 forced exports
				const FObjectExport &Rec = Package->GetExport(PackageIndex-1);
				PackageIndex = Rec.PackageIndex;
				PackageName  = Rec.ObjectName;
#else
				appError("Wrong package index: %d", PackageIndex);
#endif // UNREAL3
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
	} catch (...) {
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
