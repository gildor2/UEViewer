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
	bool			UseVertMesh;
	bool			UseTexture;
	bool			UseMorphTarget;
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
		PROP_BOOL(UseVertMesh)
		PROP_BOOL(UseTexture)
		PROP_BOOL(UseMorphTarget)
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

enum class ETextureExportFormat : int
{
	tga,
	tga_uncomp,
	png,
};

struct CExportSettings
{
	DECLARE_STRUCT(CExportSettings);

	FString			ExportPath;
	bool			ExportDdsTexture;
	EExportMeshFormat SkeletalMeshFormat;
	EExportMeshFormat StaticMeshFormat;
	ETextureExportFormat TextureFormat;
	bool			ExportMeshLods;
	bool			SaveUncooked;
	bool			SaveGroups;
	bool			DontOverwriteFiles;

	BEGIN_PROP_TABLE
		PROP_STRING(ExportPath)
		PROP_BOOL(ExportDdsTexture)
		PROP_INT(SkeletalMeshFormat)
		PROP_INT(StaticMeshFormat)
		PROP_INT(TextureFormat)
		PROP_BOOL(ExportMeshLods)
		PROP_BOOL(SaveUncooked)
		PROP_BOOL(SaveGroups)
		PROP_BOOL(DontOverwriteFiles)
	END_PROP_TABLE

	CExportSettings()
	{
		Reset();
	}

	void SetPath(const char* path);

	void Reset();
	void Apply();
};

struct CSavePackagesSettings
{
	DECLARE_STRUCT(CSavePackagesSettings);

	FString			SavePath;
	bool			KeepDirectoryStructure;

	BEGIN_PROP_TABLE
		PROP_STRING(SavePath)
		PROP_BOOL(KeepDirectoryStructure)
	END_PROP_TABLE

	CSavePackagesSettings()
	{
		Reset();
	}

	void SetPath(const char* path);

	void Reset();
};

struct CUmodelSettings
{
	DECLARE_STRUCT(CUmodelSettings);

	bool			bShowExportOptions;
	bool			bShowSaveOptions;
	CStartupSettings Startup;
	CExportSettings  Export;
	CSavePackagesSettings SavePackages;

	BEGIN_PROP_TABLE
		PROP_STRUC(Export, CExportSettings)
		PROP_STRUC(SavePackages, CSavePackagesSettings)
//		PROP_STRUC(Startup, CStartupSettings) //!! remove
		PROP_BOOL(bShowExportOptions)
		PROP_BOOL(bShowSaveOptions)
	END_PROP_TABLE

	CUmodelSettings()
	{
		Reset();
	}

	void Reset()
	{
		bShowExportOptions = true;
		bShowSaveOptions = true;
		Startup.Reset();
		Export.Reset();
		SavePackages.Reset();
	}

	void Save();
	void Load();
};

extern CUmodelSettings GSettings;

#endif // __UMODEL_SETINGS_H__
