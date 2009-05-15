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

struct FileInfo
{
	int		Ver;
	int		LicVer;
	int		Count;
};

static int InfoCmp(const FileInfo *p1, const FileInfo *p2)
{
	int dif = p1->Ver - p2->Ver;
	if (dif) return dif;
	return p1->LicVer - p2->LicVer;
}

TArray<FileInfo> PkgInfo;

static bool ScanPackage(const CGameFileInfo *file)
{
	guard(ScanPackage);

	FArchive *Ar = appCreateFileReader(file);

	int Tag, FileVer;
	*Ar << Tag;
	if (Tag == PACKAGE_FILE_TAG_REV)
		Ar->ReverseBytes = true;
	else if (Tag != PACKAGE_FILE_TAG)	//?? possibly Lineage2 file etc
		return true;
	*Ar << FileVer;

	FileInfo Info;
	Info.Ver    = FileVer & 0xFFFF;
	Info.LicVer = FileVer >> 16;
	Info.Count  = 0;
//	printf("%s - %d/%d\n", file->RelativeName, Info.Ver, Info.LicVer);
	int Index = INDEX_NONE;
	for (int i = 0; i < PkgInfo.Num(); i++)
	{
		FileInfo &Info2 = PkgInfo[i];
		if (Info2.Ver == Info.Ver && Info2.LicVer == Info.LicVer)
		{
			Index = i;
			break;
		}
	}
	if (Index == INDEX_NONE)
		Index = PkgInfo.AddItem(Info);
	PkgInfo[Index].Count++;

	delete Ar;
	return true;

	unguardf(("%s", file->RelativeName));
}


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
		printf(	"Unreal package scanner\n"
				"Usage: pkgtool <path to game files>\n"
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
	const char *argPkgDir = argv[arg];
	if (!argPkgDir) goto help;

	appSetRootDirectory(argPkgDir);

	// scan packages
	appEnumGameFiles(ScanPackage);

	PkgInfo.Sort(InfoCmp);
	printf("Version summary:\n"
		   "%-9s  %-9s  %s\n", "Ver", "LicVer", "Count");
	for (int i = 0; i < PkgInfo.Num(); i++)
	{
		const FileInfo &Info = PkgInfo[i];
		printf("%3d (%3X)  %3d (%3X)  %d\n",
			Info.Ver, Info.Ver, Info.LicVer, Info.LicVer, Info.Count);
	}

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
