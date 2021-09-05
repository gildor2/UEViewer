#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"

#if UNREAL4

/*-----------------------------------------------------------------------------
	UE4.26+ unversioned property table
-----------------------------------------------------------------------------*/

// Make a bitmask from provided bit values.
template<int Arg>
constexpr uint32 MakeBitmask()
{
	static_assert(Arg >= 0 && Arg <= 31, "Bad 'Arg'");
	return 1 << Arg;
}

template<int Arg1, int Arg2, int... Args>
constexpr uint32 MakeBitmask()
{
	return MakeBitmask<Arg1>() | MakeBitmask<Arg2, Args...>();
}

// Subtract Offset from each input constant and make a bitmask from it.
// Bitmask from a single constant makes nothing. Bitmask of <N,N+1> will result 1.
template<int Offset>
constexpr int MakeBitmaskWithOffset()
{
    return 0;
}

template<int Offset, int Arg2, int... Args>
constexpr int MakeBitmaskWithOffset()
{
	return MakeBitmask<Arg2 - (Offset + 1), Args - (Offset + 1)...>();
}

struct PropInfo
{
	// Name of the property, should exist in class's property table (BEGIN_PROP_TABLE...)
	// If starts with '#', the property is ignored, and the following string is a property's type name.
	// If starts with '!', the property is ignored, and PropIndex defines the engine version constant.
	const char* Name;
	// Index of the property.
	int			PropIndex;
	// When PropMask is not zero, more than one property is specified within this entry.
	// PropMask of value '1' means 2 properties: { PropIndex, PropIndex + 1 }
	uint32		PropMask;
};

#define BEGIN(type)					{ type,  0,        0 },	// store class name as field name, index is not used
#define MAP(name,index)				{ #name, index,    0 },	// field specification
#define END							{ NULL,  0,        0 },	// end of class - mark with NULL name

#define VERSION_BLOCK(version)		{ "!",   version,  0 },	// begin engine-specific block

// Multi-property "drop" instructions
#if _MSC_VER
#define DROP_INT8(index, ...)		{ "#int8", index, MakeBitmaskWithOffset<index, __VA_ARGS__>() },
#define DROP_INT64(index, ...)		{ "#int64", index, MakeBitmaskWithOffset<index, __VA_ARGS__>() },
#define DROP_VECTOR3(index, ...)	{ "#vec3", index,  MakeBitmaskWithOffset<index, __VA_ARGS__>() },
#define DROP_VECTOR4(index, ...)	{ "#vec4", index,  MakeBitmaskWithOffset<index, __VA_ARGS__>() },	// FQuat, FGuid etc
#else
// gcc doesn't like empty __VA_ARGS__
#define DROP_INT8(index, ...)		{ "#int8", index, MakeBitmaskWithOffset<index __VA_OPT__(,) __VA_ARGS__>() },
#define DROP_INT64(index, ...)		{ "#int64", index, MakeBitmaskWithOffset<index __VA_OPT__(,) __VA_ARGS__>() },
#define DROP_VECTOR3(index, ...)	{ "#vec3", index,  MakeBitmaskWithOffset<index __VA_OPT__(,) __VA_ARGS__>() },
#define DROP_VECTOR4(index, ...)	{ "#vec4", index,  MakeBitmaskWithOffset<index __VA_OPT__(,) __VA_ARGS__>() },	// FQuat, FGuid etc
#endif

#define DROP_OBJ_ARRAY(index)		{ "#arr_int32", index, 0 }, // TArray<UObject*>

// Class table. If class is missing here, it's assumed that its property list matches property table
// (declared with BEGIN_PROP_TABLE).
// Declaration rules:
// - Classes should go in order: child, parent, parent's parent ... If not, the parent
//   class will be captured before the child, and the property index won't match.
// - If property is not listed, SerializeUnversionedProperties4 will assume the property is int32.
// - VERSION_BLOCK has strict syntax
//   - no multiple fields in DROP_.. macros
//   - VERSION_BLOCK with higher version (newer one) should go before lower (older) version
//   - VERSION_BLOCK should be ended with constant 0
//   - all properties inside of the VERSION_BLOCK should go in ascending order

static const PropInfo PropData[] =
{
// UStaticMesh
BEGIN("UStaticMesh4")
	VERSION_BLOCK(GAME_UE4(27))
		DROP_INT8(14)					// uint8 bSupportRayTracing
	VERSION_BLOCK(0)
	DROP_INT64(0)						// FPerPlatformInt MinLOD - serialized as 2x int32, didn't find why
	MAP(StaticMaterials, 2)
	MAP(LightmapUVDensity, 3)
	DROP_INT8(9, 10, 11, 12, 13, 14, 15, 16)
	MAP(Sockets, 17)
	DROP_VECTOR3(18)					// FVector PositiveBoundsExtension
	DROP_VECTOR3(19)					// FVector NegativeBoundsExtension
	MAP(ExtendedBounds, 20)
	DROP_OBJ_ARRAY(22)					// TArray<UAssetUserData*> AssetUserData
END

// USkeletalMesh
BEGIN("USkeletalMesh4")
	VERSION_BLOCK(GAME_UE4(27))
		DROP_INT8(21)					// uint8 bSupportRayTracing
	VERSION_BLOCK(0)
	MAP(Skeleton, 0)
	DROP_VECTOR3(3)						// PositiveBoundsExtension
	DROP_VECTOR3(4)						// PositiveBoundsExtension
	MAP(Materials, 5)
	MAP(LODInfo, 7)
	DROP_INT64(8)						// FPerPlatformInt MinLod
	DROP_INT64(9)						// FPerPlatformBool DisableBelowMinLodStripping
	DROP_INT8(10, 11, 12, 13, 14, 16)
	MAP(bHasVertexColors, 15)
	MAP(MorphTargets, 21)
	DROP_OBJ_ARRAY(23)					// TArray<UClothingAssetBase*> MeshClothingAssets
	MAP(SamplingInfo, 24)
	DROP_OBJ_ARRAY(25)					// TArray<UAssetUserData*> AssetUserData
	MAP(Sockets, 26)
	MAP(SkinWeightProfiles, 27)
END

BEGIN("USkeleton")
	MAP(BoneTree, 0)
	DROP_VECTOR4(2)						// FGuid VirtualBoneGuid
	MAP(VirtualBones, 3)
	MAP(Sockets, 4)
	DROP_OBJ_ARRAY(6)					// TArray<UBlendProfile*> BlendProfiles
	MAP(SlotGroups, 7)
	DROP_OBJ_ARRAY(8)					// TArray<UAssetUserData*> AssetUserData
END

// UAnimSequence
BEGIN("UAnimSequence4")
	VERSION_BLOCK(GAME_UE4(27))
		MAP(RetargetSourceAssetReferencePose, 9)
	VERSION_BLOCK(0)
	MAP(NumFrames, 0)
	MAP(TrackToSkeletonMapTable, 1)
	MAP(AdditiveAnimType, 4)
	MAP(RefPoseType, 5)
	MAP(RefPoseSeq, 6)
	MAP(RefFrameIndex, 7)
	MAP(RetargetSource, 8)
	MAP(Interpolation, 9)
	MAP(bEnableRootMotion, 10)
	DROP_INT8(11, 12, 13, 14)
	MAP(AuthoredSyncMarkers, 15)
	MAP(BakedPerBoneCustomAttributeData, 16)
END

BEGIN("UAnimationAsset")
	MAP(Skeleton, 0)
	DROP_OBJ_ARRAY(1)					// MetaData
	DROP_OBJ_ARRAY(2)					// AssetUserData
END

// UTexture2D
BEGIN("UTexture2D")
	DROP_INT8(2)						// uint8 bTemporarilyDisableStreaming:1
	DROP_INT8(3)						// TEnumAsByte<enum TextureAddress> AddressX
	DROP_INT8(4)						// TEnumAsByte<enum TextureAddress> AddressY
	DROP_INT64(5)						// FIntPoint ImportedSize
END

BEGIN("UTexture3")
	DROP_VECTOR4(0)						// FGuid LightingGuid
	DROP_INT8(2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13)
	DROP_INT64(6)						// FPerPlatformFloat Downscale
	DROP_OBJ_ARRAY(14)					// AssetUserData
END

// UMaterialInstance
BEGIN("UMaterialInstance")
	MAP(Parent, 9)
	DROP_INT8(10, 11)
	MAP(ScalarParameterValues, 12)
	MAP(VectorParameterValues, 13)
	MAP(TextureParameterValues, 14)
	MAP(RuntimeVirtualTextureParameterValues, 15)
	MAP(FontParameterValues, 16)
	MAP(BasePropertyOverrides, 17)
	MAP(StaticParameters, 18)
	DROP_OBJ_ARRAY(20)					//todo: TArray<UObject*> CachedReferencedTextures
END

// UMaterial
BEGIN("UMaterial3")
	// known properties
	MAP(BlendMode, 17)
	MAP(TwoSided, 33)
	MAP(bDisableDepthTest, 48)
	// todo: native serializers
	// todo: FScalarMaterialInput: (10, 11, 12, 25, 27)
	// todo: FVectorMaterialInput: (13, 14, 24)
	// todo: FColorMaterialInput: (15)
	// todo: MAP(CachedExpressionData, 116)
	//?? FShadingModelMaterialInput 28
	//?? FLinearColor 46
	// int8 properties
	DROP_INT8(16, 17, 18, 19, 20, 21)
	DROP_INT8(29, 30, 31, 32, 34, 35, 36, 37, 38)
	DROP_INT8(48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59)
	DROP_INT8(60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70)
	DROP_INT8(75, 76, 77, 78, 79, 80, 82, 83, 84, 85)
	DROP_INT8(86, 87, 88, 89, 90, 91, 92, 93, 94, 95)
	DROP_INT8(96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111)
	// other
	DROP_VECTOR4(114)					// FGuid StateId
END

BEGIN("UMaterialInterface")
	MAP(LightmassSettings, 1)
	MAP(TextureStreamingData, 2)
	DROP_OBJ_ARRAY(3)					// TArray<UAssetUserData*> AssetUserData
END

BEGIN("UStreamableRenderAsset")
	DROP_INT64(0)						// double ForceMipLevelsToBeResidentTimestamp
	DROP_INT8(4, 5, 6, 7, 8, 9)
END

BEGIN("FSkeletalMeshLODInfo")
	DROP_INT64(0)						// FPerPlatformFloat ScreenSize
	MAP(LODHysteresis, 1)
	MAP(LODMaterialMap, 2)
	MAP(BuildSettings, 3)
	MAP(ReductionSettings, 4)
	MAP(BonesToRemove, 5)
	MAP(BonesToPrioritize, 6)
	MAP(SourceImportFilename, 10)
	DROP_INT8(11, 12, 13, 14, 15)
END

BEGIN("FSkeletalMeshBuildSettings")
	DROP_INT8(0, 1, 2, 3, 4, 5, 6, 7)
END

BEGIN("FSkeletalMeshOptimizationSettings")
	DROP_INT8(0, 6, 7, 8, 9, 10, 11, 12, 16, 18, 19)
END

BEGIN("FSkinWeightProfileInfo")
	MAP(Name, 0)
	DROP_INT64(1)						// FPerPlatformBool DefaultProfile
	DROP_INT64(2)						// FPerPlatformInt DefaultProfileFromLODIndex
END

BEGIN("FMaterialInstanceBasePropertyOverrides")
	MAP(bOverride_OpacityMaskClipValue, 0)
	MAP(bOverride_BlendMode, 1)
	DROP_INT8(2, 3, 4, 7, 8, 10)
	MAP(bOverride_TwoSided, 5)
	MAP(TwoSided, 6)
	MAP(BlendMode, 9)
	MAP(OpacityMaskClipValue, 11)
END

BEGIN("FLightmassMaterialInterfaceSettings")
	DROP_INT8(3, 4, 5, 6, 7)
END

BEGIN("FBoneNode")
	DROP_INT64(0)						// FName Name_DEPRECATED
	MAP(TranslationRetargetingMode, 2)	// TEnumAsByte<EBoneTranslationRetargetingMode::Type> TranslationRetargetingMode
END
};

#undef MAP
#undef BEGIN
#undef END

struct ParentInfo
{
	const char* ThisName;
	const char* ParentName; // this could be a UE4 class name which is NOT defined in this table
	int NumProps;
	int MinEngineVersion = 0; // allow changing type information with newer engine versions
};

// Declaration rules:
// - Parent classes should be defined after children, so the whole table could be iterated with a single pass.
// - If there's engine-specific class declaration, newer engine declarations should go before the older ones.

static const ParentInfo ParentData[] =
{
	// Texture classes
	{ "UTextureCube4", "UTexture3", 0 },
	{ "UTexture2D", "UTexture3", 6 },
	{ "UTexture3", "UStreamableRenderAsset", 15 },
	// Mesh classes
	{ "USkeletalMesh4", "UStreamableRenderAsset", 29, GAME_UE4(27) },
	{ "USkeletalMesh4", "UStreamableRenderAsset", 28 },
	{ "UStaticMesh4", "UStreamableRenderAsset", 26, GAME_UE4(27) },
	{ "UStaticMesh4", "UStreamableRenderAsset", 25 },
	// Material classes
	{ "UMaterialInstanceConstant", "UMaterialInstance", 1 }, // contains just a single UObject* property
	{ "UMaterialInstance", "UMaterialInterface", 21 },
	{ "UMaterial3", "UMaterialInterface", 119, GAME_UE4(27) },
	{ "UMaterial3", "UMaterialInterface", 117 },
	// Animation classes
	{ "UAnimSequence4", "UAnimSequenceBase", 18, GAME_UE4(27) },
	{ "UAnimSequence4", "UAnimSequenceBase", 17 },
	{ "UAnimSequenceBase", "UAnimationAsset", 4 },
	// Structures
	{ "FStaticSwitchParameter", "FStaticParameterBase", 1 },
	{ "FStaticComponentMaskParameter", "FStaticParameterBase", 4 },
	{ "FStaticTerrainLayerWeightParameter", "FStaticParameterBase", 2 },
	{ "FStaticMaterialLayersParameter", "FStaticParameterBase", 1 },
	{ "FAnimNotifyEvent", "FAnimLinkableElement", 17 },
};

struct FindPropInfo
{
	const char* TypeName;
	const CTypeInfo* Type;
	int PropIndex;

	FindPropInfo()
	: TypeName(NULL)
	, Type(NULL)
	, PropIndex(INDEX_NONE)
	{}
};

static void FindTypeForProperty(FindPropInfo& Info, int Game)
{
	//todo: Can optimize with checking if class name starts with 'U', but: CTypeInfo::Name skips 'U', plus there could be FSomething (structs).
	//todo: Can cache information about parent type. Store NULL to avoid iteration of the whole table when no parent information stored (e.g. UStreamableRenderAsset).
	for (const ParentInfo& Parent : ParentData)
	{
		if (Info.PropIndex < Parent.NumProps)
		{
			// Fast reject
			continue;
		}
		if (Parent.MinEngineVersion && Game < Parent.MinEngineVersion)
		{
			// This is a type info for a newer engine
			continue;
		}
		// Compare types: use exact name comparison insted of CurrentType->IsA(...), so we could
		// skip some type information in PropData[]
		if (strcmp(Info.TypeName, Parent.ThisName) == 0)
		{
			// Types are matching, redirect Info to parent type
			Info.TypeName = Parent.ParentName;
			Info.Type = FindStructType(Info.TypeName);
			Info.PropIndex -= Parent.NumProps;
		}
	}
}

/*-----------------------------------------------------------------------------
	Interface function for finding a property by its index
-----------------------------------------------------------------------------*/

const char* CTypeInfo::FindUnversionedProp(int InPropIndex, int& OutArrayIndex, int InGame) const
{
	guard(CTypeInfo::FindUnversionedProp);

	OutArrayIndex = 0;

	// Find type info for this property, and offset its index to match class inheritance
	FindPropInfo FoundProp;
	FoundProp.TypeName = Name;
	FoundProp.Type = this;
	FoundProp.PropIndex = InPropIndex;
	FindTypeForProperty(FoundProp, InGame);

	// Now we have FoundProp filled with type information. The desired property is inside this class,
	// all parent classes are resolved. Find a field itself.
	const PropInfo* p;
	const PropInfo* end;

	p = PropData;
	end = PropData + ARRAY_COUNT(PropData);

	while (p < end)
	{
		// Use exact name comparison to allow intermediate parents which aren't declared in PropData[].
		// Doing the same in FindTypeForProperty().
		if (stricmp(p->Name, FoundProp.TypeName) != 0)
		{
			// A different class, skip its declaration.
			while (++p < end && p->Name != NULL)
			{}
			// skip the END marker and continue with next type map
			p++;
			continue;
		}

		bool bPatchingPropIndex = false;
		int NumInsertedProps = 0;

		// Loop over PropInfo entries, either find a property or an end of the current class information.
		while (++p < end && p->Name != NULL)
		{
			if (p->Name[0] == '!')
			{
				// Adjust the property index so it will match previous engine version
				FoundProp.PropIndex -= NumInsertedProps;
				NumInsertedProps = 0;
				// This is the engine version block
				int RequiredVersion = p->PropIndex;
				bPatchingPropIndex = false;
				if (InGame < RequiredVersion)
				{
					// Skip the whole block for the engine which is newer than loaded asset use
					while (++p < end && p->Name != NULL && p->Name[0] != '!')
					{}
					// A step back, so next '++p' in outer loop will check this entry again
					p--;
				}
				else if (RequiredVersion != 0)
				{
					// The engine version matches, so the following block will probably insert new properties
					bPatchingPropIndex = true;
				}
				continue;
			}

			if (p->PropIndex <= FoundProp.PropIndex && bPatchingPropIndex)
			{
				// This property was inserted in this engine version, so we'll need to adjust
				// property index at the end of version block to match previous engine version.
				NumInsertedProps++;
			}

			if (p->PropMask)
			{
				assert(bPatchingPropIndex == false);				// no support for masks inside VERSION_BLOCK
				// It is used only for DROP_... macros.
				uint32 IndexWithOffset = FoundProp.PropIndex - p->PropIndex;
				if (IndexWithOffset > 32)
				{
					// Note: negative values will appear as a large uint.
					continue;
				}
				if ((IndexWithOffset == 0) ||						// we're implicitly storing first property
					((1 << (IndexWithOffset - 1)) & p->PropMask))	// and explicitly up to 32 properties more
				{
					return p->Name;
				}
			}
			else if (p->PropIndex == FoundProp.PropIndex)
			{
				// Found a matching property.
				//todo: we're not supporting arrays here, arrays relies on property table to match layout of CTypeInfo declaration
				return p->Name;
			}
		}

		// The class has been verified, and we didn't find a property. Don't fall to CTypeInfo PROP declarations.
		return NULL;
	}

	// Type information was not found. Try using CTypeInfo properties, assuming their layout matches UE4.
	int CurrentPropIndex = 0;
	if (FoundProp.Type == NULL) appError("Enumerating properties of unknown type %s", FoundProp.TypeName);
	for (int Index = 0; Index < FoundProp.Type->NumProps; Index++)
	{
		const CPropInfo& Prop = FoundProp.Type->Props[Index];
		if (Prop.Count >= 2)
		{
			// Static array, should count each item as a separate property
			if (CurrentPropIndex + Prop.Count > FoundProp.PropIndex)
			{
				// The property is located inside this array
				OutArrayIndex = FoundProp.PropIndex - CurrentPropIndex;
				return Prop.Name;
			}
			CurrentPropIndex += Prop.Count;
		}
		else
		{
			// The same code, but works as Count == 1 for any values
			if (CurrentPropIndex == FoundProp.PropIndex)
			{
				return Prop.Name;
			}
			CurrentPropIndex++;
		}
	}

	return NULL;
	unguard;
}

#endif // UNREAL4
