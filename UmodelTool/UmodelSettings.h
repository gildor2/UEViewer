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

	void SetPath(const char* path);

	void Reset();
};

struct CExportSettings
{
	FString			ExportPath;
	bool			ExportMd5Mesh;

	CExportSettings()
	{
		Reset();
	}

	void SetPath(const char* path);

	void Reset();
	void Apply();
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
