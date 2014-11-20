#ifndef __UMODEL_SETINGS_H__
#define __UMODEL_SETINGS_H__

struct UmodelSettings
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

	// export options
	FString			ExportPath;
	bool			ExportMd5Mesh;

	UmodelSettings()
	{
		SetDefaults();
	}

	void SetDefaults()
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

		// export options

		ExportMd5Mesh = false;
	}
};

extern UmodelSettings GSettings;

#endif // __UMODEL_SETINGS_H__
