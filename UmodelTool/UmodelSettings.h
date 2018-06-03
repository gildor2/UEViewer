#ifndef __UMODEL_SETINGS_H__
#define __UMODEL_SETINGS_H__

struct CStartupSettings
{
	FString			GamePath;
	int				GameOverride;

	// enable/disable particular classes
	bool			UseSkeletalMesh;
	bool			UseAnimation;
	bool			UseStaticMesh;
	bool			UseTexture;
	bool			UseLightmapTexture;
	bool			UseSound;
	bool			UseScaleForm;
	bool			UseFaceFx;

	// other compatibility options
	int				PackageCompression;
	int				Platform;

	CStartupSettings()
	{
		Reset();
	}

	void Reset()
	{
		//!! WARNING: if this function will be called from anything else but constructor,
		//!! should empty string values as well
		assert(GamePath.IsEmpty());

		GameOverride = GAME_UNKNOWN;
		GamePath = "";

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
};

struct CExportSettings
{
	FString			ExportPath;
	bool			ExportMd5Mesh;

	CExportSettings()
	{
		Reset();
	}

	void Reset()
	{
		ExportMd5Mesh = false;
	}
};

struct CUmodelSettings
{
	CStartupSettings Startup;
	CExportSettings  Export;

	CUmodelSettings()
	{
		Reset();
	}

	void Reset()
	{
		Startup.Reset();
		Export.Reset();
	}
};

extern CUmodelSettings GSettings;

#endif // __UMODEL_SETINGS_H__
