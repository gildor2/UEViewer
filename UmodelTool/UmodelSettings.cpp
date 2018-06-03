#include "Core.h"
#include "UnCore.h"
#include "UmodelSettings.h"

#include "Exporters/Exporters.h"	// appSetBaseExportDirectory

#if _WIN32
#include <direct.h>					// getcwd
#else
#include <unistd.h>					// getcwd
#endif


static void SetPathOption(FString& where, const char* value)
{
	// determine whether absolute path is used
	const char* value2;

#if _WIN32
	int isAbsPath = (value[0] != 0) && (value[1] == ':');
#else
	int isAbsPath = (value[0] == '~' || value[0] == '/');
#endif
	if (isAbsPath)
	{
		value2 = value;
	}
	else
	{
		// relative path
		char path[512];
		if (!getcwd(ARRAY_ARG(path)))
			strcpy(path, ".");	// path is too long, or other error occured

		if (!value || !value[0])
		{
			value2 = path;
		}
		else
		{
			char buffer[512];
			appSprintf(ARRAY_ARG(buffer), "%s/%s", path, value);
			value2 = buffer;
		}
	}

	char finalName[512];
	appStrncpyz(finalName, value2, ARRAY_COUNT(finalName)-1);
	appNormalizeFilename(finalName);

	where = finalName;

	// strip possible trailing double quote
	int len = where.Len();
	if (len > 0 && where[len-1] == '"')
		where.RemoveAt(len-1);
}


void CStartupSettings::SetPath(const char* path)
{
	SetPathOption(GamePath, path);
}

void CStartupSettings::Reset()
{
	GameOverride = GAME_UNKNOWN;
	SetPath("");

	UseSkeletalMesh = true;
	UseAnimation = true;
	UseStaticMesh = true;
	UseTexture = true;
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
	SetPath("UmodelExport");
	ExportMd5Mesh = false;
}

void CExportSettings::Apply()
{
	appSetBaseExportDirectory(*GSettings.Export.ExportPath);
}
