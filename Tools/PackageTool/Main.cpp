#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "PackageUtils.h"

/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

#if UNREAL4

int UE4UnversionedPackage(int verMin, int verMax)
{
	appError("Unversioned UE4 packages are not supported. Please restart UModel and select UE4 version in range %d-%d using UI or command line.", verMin, verMax);
	return -1;
}

bool UE4EncryptedPak()
{
	return false;
}

#endif // UNREAL4


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
		printf(	"Unreal package scanner\n"
				"http://www.gildor.org/\n"
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
	TArray<FileInfo> PkgInfo;
	ScanPackages(PkgInfo);

	printf("Version summary:\n"
		   "%-9s  %-9s  %s   %s\n", "Ver", "LicVer", "Count", "Filename");
	for (int i = 0; i < PkgInfo.Num(); i++)
	{
		const FileInfo &Info = PkgInfo[i];
		printf("%3d (%3X)  %3d (%3X)  %4d    %s%s\n",
			Info.Ver, Info.Ver, Info.LicVer, Info.LicVer, Info.Count, Info.FileName,
			Info.Count > 1 && Info.FileName[0] ? "..." : "");
	}

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
