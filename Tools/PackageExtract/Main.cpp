#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"

#include <direct.h>					// for mkdir()


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
	_mkdir(buf);
	// process objects
	for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		FObjectExport &Exp = Package->ExportTable[idx];
		// prepare file
		const char *ClassName = Package->GetObjectName(Exp.ClassIndex);
		const char *Outer     = Package->GetObjectName(Exp.PackageIndex);
		char buf2[512];
		appSprintf(ARRAY_ARG(buf2), "%s/%s.%s.%s", buf, Outer, *Exp.ObjectName, ClassName);
		FILE *f = fopen(buf2, "wb");
		assert(f);
		// read data
		byte *data = new byte[Exp.SerialSize];
		Package->Seek(Exp.SerialOffset);
		Package->Serialize(data, Exp.SerialSize);
		// write data
		fwrite(data, Exp.SerialSize, 1, f);
		// cleanup
		delete data;
		fclose(f);
	}
	//!! also write import/export/name table
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
