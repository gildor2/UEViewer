#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMaterial3.h"

/*-----------------------------------------------------------------------------
	UTexture/UTexture2D (Unreal engine 4)
-----------------------------------------------------------------------------*/

#if UNREAL4

void UTexture3::Serialize4(FArchive& Ar)
{
	guard(UTexture3::Serialize4);
	Super::Serialize(Ar);				// UObject

	FStripDataFlags StripFlags(Ar);

	assert(Ar.ArVer >= VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)

	if (!StripFlags.IsEditorDataStripped())
	{
		SourceArt.Serialize(Ar);
	}
	unguard;
}


void UTexture2D::Serialize4(FArchive& Ar)
{
	guard(UTexture2D::Serialize4);

	Super::Serialize4(Ar);

	FStripDataFlags StripFlags(Ar);		// note: these flags are used for pre-VER_UE4_TEXTURE_SOURCE_ART_REFACTOR versions

	int bCooked = 0;
	if (Ar.ArVer >= VER_UE4_ADD_COOKED_TO_TEXTURE2D) Ar << bCooked;

	if (Ar.ArVer < VER_UE4_TEXTURE_SOURCE_ART_REFACTOR)
		appError("VER_UE4_TEXTURE_SOURCE_ART_REFACTOR");

	if (bCooked)
	{
		//!! C:\Projects\Epic\UnrealEngine4\Engine\Source\Runtime\Engine\Private\TextureDerivedData.cpp
		//!! - UTexture::SerializeCookedPlatformData
		//!! - FTexturePlatformData::SerializeCooked
		//!! CubeMap has "Slices" - images are placed together into single bitmap?
		appError("SerializeCookedData");
	}

	unguard;
}

#endif // UNREAL4
