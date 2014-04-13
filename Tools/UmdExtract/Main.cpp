#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnUbisoft.h"

#define DEF_UNP_DIR		"unpacked"
#define HOMEPAGE		"http://www.gildor.org/"


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

static char BaseDir[256];
static bool GUnpackUmd = false;

static bool ScanFile(const CGameFileInfo *file)
{
	guard(ScanFile);

	FArchive *Ar = appCreateFileReader(file);

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(file->RelativeName);

	FArchive *TmpAr = CreateUMDReader(Ar);
	if (!TmpAr)
	{
		// not UMD file
		delete Ar;
		return true;
	}

	printf("Processing: %s\n", file->RelativeName);
	Ar = TmpAr;		// original reader will be destroyed automatically with this reader

	if (!GUnpackUmd)
	{
#if 1
		ExtractUMDArchive(Ar, BaseDir);
#else
		appPrintf("Disabled!\n");
#endif
	}
	else
	{
		char FileName[512];
		appSprintf(ARRAY_ARG(FileName), "%s/%s", BaseDir, file->ShortFilename);
		SaveUMDArchive(Ar, FileName);
	}

	delete Ar;
	return true;

	unguardf(("%s", file->RelativeName));
}


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
		printf(	"Ubisoft Unreal Engine UMD Unpacker\n"
				"Usage: unumd [options]\n"
				"\n"
				"Options:\n"
				"    -path=PATH         set directory with UMD files\n"
				"    -out=PATH          extract everything into PATH, default is \"" DEF_UNP_DIR "\"\n"
				"    -unpack            unpack UMD file instead of extraction\n"
				"\n"
				"For details and updates please visit " HOMEPAGE "\n"
		);
		exit(0);
	}

	// parse command line
	strcpy(BaseDir, DEF_UNP_DIR);

	bool hasRootDir = false;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!strnicmp(opt, "path=", 5))
			{
				appSetRootDirectory(opt+5);
				hasRootDir = true;
			}
			else if (!strnicmp(opt, "out=", 4))
			{
				strcpy(BaseDir, opt+4);
			}
			else if (!stricmp(opt, "unpack"))
				GUnpackUmd = true;
			else
				goto help;
		}
		else
		{
			break;
		}
	}
	if (!hasRootDir) appSetRootDirectory(".");		// scan for packages from the current directory

	if (arg < argc) goto help;

	guard(ProcessFiles);

	// scan packages
	appEnumGameFiles(ScanFile, "umd");


	unguard;	// ProcessFiles

	unguard;	// Main

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
