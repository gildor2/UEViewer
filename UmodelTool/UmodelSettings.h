#ifndef __UMODEL_SETINGS_H__
#define __UMODEL_SETINGS_H__

#include "TypeInfo.h"

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
	DECLARE_STRUCT(CExportSettings);

	FString			ExportPath;
	bool			ExportMd5Mesh;

	BEGIN_PROP_TABLE
		PROP_STRING(ExportPath)
		PROP_BOOL(ExportMd5Mesh)
	END_PROP_TABLE

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
	DECLARE_STRUCT(CUmodelSettings);

	CStartupSettings Startup;
	CExportSettings  Export;

	BEGIN_PROP_TABLE
		PROP_STRUC(Export, CExportSettings)
	END_PROP_TABLE

	CUmodelSettings()
	{
		Reset();
	}

	void Reset()
	{
		Startup.Reset();
		Export.Reset();
	}

	void Save();
};

extern CUmodelSettings GSettings;

#endif // __UMODEL_SETINGS_H__
