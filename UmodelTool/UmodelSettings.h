#ifndef __UMODEL_SETINGS_H__
#define __UMODEL_SETINGS_H__

#include "TypeInfo.h"

struct CStartupSettings
{
	DECLARE_STRUCT(CStartupSettings);

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

	BEGIN_PROP_TABLE
		PROP_STRING(GamePath)
		PROP_INT(GameOverride)
		PROP_BOOL(UseSkeletalMesh)
		PROP_BOOL(UseAnimation)
		PROP_BOOL(UseStaticMesh)
		PROP_BOOL(UseTexture)
		PROP_BOOL(UseLightmapTexture)
		PROP_BOOL(UseSound)
		PROP_BOOL(UseScaleForm)
		PROP_BOOL(UseFaceFx)
		PROP_INT(PackageCompression)
		PROP_INT(Platform)
	END_PROP_TABLE

	CStartupSettings()
	{
		Reset();
	}

	void SetPath(const char* path);

	void Reset();
};

enum class EExportMeshFormat : int
{
	psk,
	md5,
	gltf,
};

struct CExportSettings
{
	DECLARE_STRUCT(CExportSettings);

	FString			ExportPath;
	bool			ExportDdsTexture;
	EExportMeshFormat SkeletalMeshFormat;
	EExportMeshFormat StaticMeshFormat;
	bool			ExportMeshLods;
	bool			SaveUncooked;
	bool			SaveGroups;

	BEGIN_PROP_TABLE
		PROP_STRING(ExportPath)
		PROP_BOOL(ExportDdsTexture)
		PROP_INT(SkeletalMeshFormat)
		PROP_INT(StaticMeshFormat)
		PROP_BOOL(ExportMeshLods)
		PROP_BOOL(SaveUncooked)
		PROP_BOOL(SaveGroups)
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
//		PROP_STRUC(Startup, CStartupSettings) //!! remove
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
	void Load();
};

extern CUmodelSettings GSettings;

#endif // __UMODEL_SETINGS_H__
