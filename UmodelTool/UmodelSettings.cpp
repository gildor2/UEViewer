#include "Core.h"
#include "UnCore.h"
#include "UmodelSettings.h"

#include "Exporters/Exporters.h"	// appSetBaseExportDirectory

#if _WIN32
#include <direct.h>					// getcwd
#endif

#define CONFIG_FILE			"umodel.cfg"
#define EXPORT_DIRECTORY	"UmodelExport"
#define SAVE_DIRECTORY		"UmodelSaved"

// When bUseCwd is true, relative path will be computed based on current working directory.
// Currently this option is primarily intended for Linux as Windows version always use current directory as a base.
static void SetPathOption(FString& where, const char* value, bool bUseCwd = false)
{
	// determine whether absolute path is used
	FStaticString<512> value2;
	if (*value == '"')
	{
		// path enclosed into double quotes
		value2 = value+1;
		// remove closing quote
		value2.RemoveFromEnd("\"");
	}
	else
	{
		value2 = value;
	}

#if _WIN32
	int isAbsPath = (!value2.IsEmpty() && (value2[1] == ':'));
#else
	int isAbsPath = (!value2.IsEmpty() && (value2[0] == '~' || value2[0] == '/'));
#endif
	if (!isAbsPath)
	{
		// relative path, combine with working directory
		char path[512];
#if _WIN32
		if (!getcwd(ARRAY_ARG(path)))
			strcpy(path, ".");	// path is too long, or other error occurred
#else
		// for Unix OS, store everything to the HOME directory ("~/...")
		if (bUseCwd || (!value2.IsEmpty() && value2[0] == '.' && value2[1] == '/'))
		{
			// just use working dir
			strcpy(path, ".");
		}
		else if (const char* s = getenv("HOME"))
		{
			strcpy(path, s);
		}
		else
		{
			// fallback: when HOME variable is not registered, store to the path of umodel executable
			strcpy(path, ".");
		}
#endif

		if (value2.IsEmpty())
		{
			// empty path - use working directory
			value2 = path;
		}
		else
		{
			// relative path - prepend working directory
			char buffer[512];
			appSprintf(ARRAY_ARG(buffer), "%s/%s", path, *value2);
			value2 = buffer;
		}
	}

	char finalName[512];
	appStrncpyz(finalName, *value2, ARRAY_COUNT(finalName)-1);
	appNormalizeFilename(finalName);

	where = finalName;

	// strip possible trailing double quote
	int len = where.Len();
	if (len > 0 && where[len-1] == '"')
		where.RemoveAt(len-1);
}


void CStartupSettings::SetPath(const char* path)
{
	SetPathOption(GamePath, path, true);
}

void CStartupSettings::Reset()
{
	GameOverride = GAME_UNKNOWN;
	SetPath("");

	UseSkeletalMesh = true;
	UseAnimation = true;
	UseStaticMesh = true;
	UseTexture = true;
	UseMorphTarget = true;
	UseLightmapTexture = true;

	UseSound = false;
	UseScaleForm = false;
	UseFaceFx = false;

	PackageCompression = 0;
	Platform = PLATFORM_UNKNOWN;
}


void CExportSettings::SetPath(const char* path)
{
	SetPathOption(ExportPath, path);
}

void CExportSettings::Reset()
{
	SetPath(EXPORT_DIRECTORY);

	ExportDdsTexture = false;
	SkeletalMeshFormat = EExportMeshFormat::psk;
	StaticMeshFormat = EExportMeshFormat::psk;
	TextureFormat = ETextureExportFormat::tga;
	ExportMeshLods = false;
	SaveUncooked = false;
	SaveGroups = false;
	DontOverwriteFiles = false;
}

void CExportSettings::Apply()
{
	// Process export path. Do not assume that this is absolute path, so pass it
	// through SetPathOption().
	FString TmpExportPath;
	SetPathOption(TmpExportPath, *ExportPath);
	appSetBaseExportDirectory(*TmpExportPath);

	GNoTgaCompress = (TextureFormat == ETextureExportFormat::tga_uncomp);
	GExportPNG = (TextureFormat == ETextureExportFormat::png);
	GExportDDS = ExportDdsTexture;

	GExportLods = ExportMeshLods;
	GUncook = SaveUncooked;
	GUseGroups = SaveGroups;
	GDontOverwriteFiles = DontOverwriteFiles;
}

void CSavePackagesSettings::SetPath(const char* path)
{
	SetPathOption(SavePath, path);
}

void CSavePackagesSettings::Reset()
{
	SetPath(SAVE_DIRECTORY);
	KeepDirectoryStructure = true;
}

static void RegisterClasses()
{
	static bool registered = false;
	if (!registered)
	{
		BEGIN_CLASS_TABLE
			REGISTER_CLASS(CStartupSettings)
			REGISTER_CLASS(CExportSettings)
			REGISTER_CLASS(CSavePackagesSettings)
			REGISTER_CLASS(CUmodelSettings)
		END_CLASS_TABLE
		registered = true;
	}
}

void CUmodelSettings::Save()
{
	guard(CUmodelSettings::Save);

	RegisterClasses();

	FString ConfigFile;
	SetPathOption(ConfigFile, CONFIG_FILE);

	FArchive* Ar = new FFileWriter(*ConfigFile, FAO_TextFile|FAO_NoOpenError);
	if (!Ar->IsOpen())
	{
		delete Ar;
		appPrintf("Error creating file \"%s\" ...\n", *ConfigFile);
		return;
	}

	const CTypeInfo* TypeInfo = CUmodelSettings::StaticGetTypeinfo();
	TypeInfo->SaveProps(this, *Ar);

	delete Ar;

	unguard;
}

void CUmodelSettings::Load()
{
	guard(CUmodelSettings::Load);

	RegisterClasses();

	FString ConfigFile;
	SetPathOption(ConfigFile, CONFIG_FILE);

	FArchive* Ar = new FFileReader(*ConfigFile, FAO_NoOpenError); // can't use FAO_TextFile because of FFileReader::IsEof will not work fine with it
	if (!Ar->IsOpen())
	{
		delete Ar;
		return;
	}

	const CTypeInfo* TypeInfo = CUmodelSettings::StaticGetTypeinfo();
	if (!TypeInfo->LoadProps(this, *Ar))
	{
		appPrintf("There's an error in %s, ignoring configuration file\n", *ConfigFile);
		Reset();
	}

	delete Ar;

	unguard;
}
