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

	UmodelSettings()
	{
		SetDefaults();
	}

	void SetDefaults()
	{
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

#endif // __UMODEL_SETINGS_H__
