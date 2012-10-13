#include "Core.h"
#include "UnCore.h"
#include "UnPackage.h"

#define MAKE_DIRS		1
//#define DISABLE_WRITE	1		// for quick testing of extraction

#define HOMEPAGE		"http://www.gildor.org/"


/*-----------------------------------------------------------------------------
	Service functions
-----------------------------------------------------------------------------*/

static void GetFullExportFileName(const FObjectExport &Exp, UnPackage *Package, char *buf, int bufSize)
{
	// get full path
	Package->GetFullExportName(Exp, buf, bufSize);
	char *dst;
	// replace '.' with '/' and find the string end
	for (dst = buf; *dst; dst++)
		if (*dst == '.') *dst = '/';
	// and append class name as file extension
	assert(*dst == 0);
	const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
	appSprintf(dst, bufSize - (dst - buf), ".%s", ClassName);
}


static void GetFullExportName(const FObjectExport &Exp, UnPackage *Package, char *buf, int bufSize)
{
	// put class name
	const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
	appSprintf(buf, bufSize, "%s'", ClassName);
	// get full path
	char *dst = strchr(buf, 0);
	Package->GetFullExportName(Exp, dst, bufSize - (dst - buf));
	// append "'"
	dst = strchr(buf, 0);
	appSprintf(dst, bufSize - (dst - buf), "'");
}


static TArray<FString> filters;

static bool FilterClass(const char *ClassName)	//?? check logic: filter = pass or reject
{
	if (!filters.Num()) return true;

	bool filter = false;
	for (int fidx = 0; fidx < filters.Num(); fidx++)
	{
		if (!stricmp(filters[fidx], ClassName))
			return true;
	}
	return false;
}


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
		printf(	"Unreal Engine package extractor\n"
				"Usage: extract [command] [options] <package filename>\n"
				"\n"
				"Commands:\n"
				"    -extract           (default) extract package\n"
				"    -list              list contents of package\n"
				"\n"
				"Options:\n"
				"    -filter=<value>    add filter for output types\n"
				"    -out=PATH          extract everything into PATH instead of the current directory\n"
				"    -lzo|lzx|zlib      force compression method for fully-compressed packages\n"
				"\n"
				"Platform selection:\n"
				"    -ps3               override platform autodetection to PS3\n"
				"\n"
				"For details and updates please visit " HOMEPAGE "\n"
		);
		exit(0);
	}

	// parse command line
	enum
	{
		CMD_Extract,
		CMD_List
	};

	static byte mainCmd = CMD_Extract;
	char BaseDir[256];
	strcpy(BaseDir, ".");

	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!stricmp(opt, "extract"))
				mainCmd = CMD_Extract;
			else if (!stricmp(opt, "list"))
				mainCmd = CMD_List;
			else if (!strnicmp(opt, "filter=", 7))
			{
				FString* S = new (filters) FString(opt+7);
			}
			else if (!strnicmp(opt, "out=", 4))
			{
				strcpy(BaseDir, opt+4);
			}
			else if (!stricmp(opt, "lzo"))
				GForceCompMethod = COMPRESS_LZO;
			else if (!stricmp(opt, "zlib"))
				GForceCompMethod = COMPRESS_ZLIB;
			else if (!stricmp(opt, "lzx"))
				GForceCompMethod = COMPRESS_LZX;
			else if (!stricmp(opt, "ps3"))
				GForcePlatform = PLATFORM_PS3;
			else
				goto help;
		}
		else
		{
			break;
		}
	}
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

	int idx;

	if (mainCmd == CMD_List)
	{
		for (idx = 0; idx < Package->Summary.ExportCount; idx++)
		{
			FObjectExport &Exp = Package->ExportTable[idx];
			const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
			if (!FilterClass(ClassName)) continue;
			char objName[1024];
			GetFullExportName(Exp, Package, ARRAY_ARG(objName));
			printf("%5d  %s\n", idx, objName);
		}
		return 0;
	}

	// extract package name, create directory for it
	char PkgName[256];
	const char *s = strrchr(argPkgName, '/');
	if (!s) s = strrchr(argPkgName, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = argPkgName;
	appStrncpyz(PkgName, s, ARRAY_COUNT(PkgName));
	char *s2 = strchr(PkgName, '.');
	if (s2) *s2 = 0;
	// extract objects and write export table
	FILE *f;
	char buf2[1024];
	guard(ExtractObjects);
	appSprintf(ARRAY_ARG(buf2), "%s/%s/ExportTable.txt", BaseDir, PkgName);
	appMakeDirectoryForFile(buf2);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		FObjectExport &Exp = Package->ExportTable[idx];
		const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
		if (!FilterClass(ClassName)) continue;
		// prepare file
#if !MAKE_DIRS
		char buf3[1024];
		buf3[0] = 0;
		if (Exp.PackageIndex) //?? GetObjectName() will return "Class" for index=0 ...
		{
			const char *Outer = Package->GetObjectName(Exp.PackageIndex);
			appSprintf(ARRAY_ARG(buf3), "%s.", Outer);
		}
		fprintf(f, "%d = %s'%s%s'\n", idx, ClassName, buf3, *Exp.ObjectName);
		appSprintf(ARRAY_ARG(buf2), "%s/%s/%s%s.%s", BaseDir, buf, buf3, *Exp.ObjectName, ClassName);
#else
		char objName[1024];
		GetFullExportName(Exp, Package, ARRAY_ARG(objName));
		fprintf(f, "%d = %s\n", idx, objName);
		GetFullExportFileName(Exp, Package, ARRAY_ARG(objName));
		appSprintf(ARRAY_ARG(buf2), "%s/%s/%s", BaseDir, PkgName, objName);
#endif
		guard(WriteFile);
		// read data
		byte *data = new byte[Exp.SerialSize];
		if (Exp.SerialSize)
		{
			Package->Seek(Exp.SerialOffset);
			Package->Serialize(data, Exp.SerialSize);
		}
#if !DISABLE_WRITE
		appMakeDirectoryForFile(buf2);
		FILE *f2 = fopen(buf2, "wb");
		if (!f2)
		{
			//!! note: cannot create file with name "con" (any extension)
			printf("%d/%d: unable to create file %s\n", idx, Package->Summary.ExportCount, buf2);
			continue;
		}
		// write data
		fwrite(data, Exp.SerialSize, 1, f2);
		fclose(f2);
#endif // !DISABLE_WRITE
		// cleanup
		delete data;
		unguardf(("file=%s", buf2));
		// notification
		printf("Done: %d/%d ...\r", idx, Package->Summary.ExportCount);
	}
	fclose(f);
	printf("Done ...             \n");
	unguard;
	// write name table
	guard(WriteNameTable);
	appSprintf(ARRAY_ARG(buf2), "%s/%s/NameTable.txt", BaseDir, PkgName);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.NameCount; idx++)
		fprintf(f, "%d = \"%s\"\n", idx, Package->NameTable[idx]);
	fclose(f);
	unguard;
	// write import table
	guard(WriteImportTable);
	appSprintf(ARRAY_ARG(buf2), "%s/%s/ImportTable.txt", BaseDir, PkgName);
	f = fopen(buf2, "w");
	assert(f);
	for (idx = 0; idx < Package->Summary.ImportCount; idx++)
	{
		const FObjectImport &Imp = Package->GetImport(idx);
		const char *PackageName = Package->GetObjectPackageName(Imp.PackageIndex);
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
