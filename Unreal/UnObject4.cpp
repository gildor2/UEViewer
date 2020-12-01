#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"

#if UNREAL4

/*-----------------------------------------------------------------------------
	UE4.26+ unversioned property table
-----------------------------------------------------------------------------*/

template<int Arg>
constexpr int MakeBitmask()
{
	static_assert(Arg >= 0 && Arg <= 31, "Bad 'Arg'");
	return 1 << Arg;
}

template<int Arg1, int Arg2, int... Args>
constexpr int MakeBitmask()
{
	return MakeBitmask<Arg1>() | MakeBitmask<Arg2, Args...>();
}

struct PropInfo
{
	const char* Name;
	int			Index;
	uint32		PropMask;
};

#define BEGIN(type)			{ type,  0,        0 },	// store class name as field name, index is not used
#define MAP(name,index)		{ #name, index,    0 },	// field specification
#define END					{ NULL,  0,        0 },	// end of class - mark with NULL name

#define DROP_INT8(index)	{ "#int8", index,  0 }, //todo: can use DROP_INT8_MASK for all INT8, just ensure it will work with Index >= 32
#define DROP_INT64(index)	{ "#int64", index, 0 },
#define DROP_VECTOR3(index)	{ "#vec3", index,  0 },
#define DROP_VECTOR4(index)	{ "#vec4", index,  0 },	// FQuat, FGuid etc

#define DROP_INT8_MASK(...)	{ "#int8", -1, MakeBitmask<__VA_ARGS__>() },

static const PropInfo info[] =
{
BEGIN("UStaticMesh4")
	DROP_INT64(0)						// FPerPlatformInt MinLOD - serialized as 2x int32, didn't find why
	MAP(StaticMaterials, 2)
	MAP(LightmapUVDensity, 3)
	DROP_INT8_MASK(9, 10, 11, 12, 13, 14, 15, 16)
	MAP(Sockets, 17)
	DROP_VECTOR3(18)					// FVector PositiveBoundsExtension
	DROP_VECTOR3(19)					// FVector NegativeBoundsExtension
	MAP(ExtendedBounds, 20)
	MAP(AssetUserData, 22)
END

BEGIN("USkeletalMesh4")
	MAP(Skeleton, 0)
	MAP(LODInfo, 7)
	DROP_INT8(14)						// uint8 bHasBeenSimplified:1
	MAP(bHasVertexColors, 15)
	MAP(SamplingInfo, 24)
	MAP(Sockets, 26)
END

BEGIN("UTexture2D")
	DROP_INT8(2)						// uint8 bTemporarilyDisableStreaming:1
	DROP_INT8(3)						// TEnumAsByte<enum TextureAddress> AddressX
	DROP_INT8(4)						// TEnumAsByte<enum TextureAddress> AddressY
	DROP_INT64(5)						// FIntPoint ImportedSize
	// Parent class UTexture, starts with index 6
	DROP_VECTOR4(6)						// FGuid LightingGuid
	// UTexture
	DROP_INT8_MASK(8, 9, 11, 14, 15, 16, 17, 18, 19)
	// UStreamableRenderAsset starts at index 21
	DROP_INT64(21)						// double ForceMipLevelsToBeResidentTimestamp
	DROP_INT8_MASK(25, 26, 27, 28, 29, 30)
END

BEGIN("FSkeletalMeshLODInfo")
	DROP_INT64(0)						// FPerPlatformFloat ScreenSize
	MAP(LODHysteresis, 1)
	MAP(LODMaterialMap, 2)
	MAP(BuildSettings, 3)
	MAP(ReductionSettings, 4)
	DROP_INT8_MASK(11, 12, 13, 14, 15)
END

BEGIN("FSkeletalMeshBuildSettings")
	DROP_INT8_MASK(0, 1, 2, 3, 4, 5, 6, 7)
END

BEGIN("FSkeletalMeshOptimizationSettings")
	DROP_INT8_MASK(0, 6, 7, 8, 9, 10, 11, 12, 16, 18, 19)
END
};

#undef MAP
#undef BEGIN
#undef END

const char* CTypeInfo::FindUnversionedProp(int PropIndex, int& OutArrayIndex) const
{
	guard(CTypeInfo::FindUnversionedProp);

	OutArrayIndex = 0;

	if (IsA("TextureCube4"))
	{
		// No properties, redirect to UTexture2D by adjusting the property index.
		// Add number of properties in UTexture2D
		PropIndex += 6;
	}

	// todo: can support parent classes (e.g. UStreamableRenderAsset) by recognizing and adjusting index, like we did for UTextureCube

	// Find a field
	const PropInfo* p;
	const PropInfo* end;

	p = info;
	end = info + ARRAY_COUNT(info);

	bool bClassFound = false;
	while (p < end)
	{
		// Note: StrucType could correspond to a few classes from the list about
		// because of inheritance, so don't "break" a loop when we've scanned some class, check
		// other classes too
		bool IsOurClass = IsA(p->Name + 1);

		while (++p < end && p->Name)
		{
			if (!IsOurClass) continue;
			if (p->PropMask)
			{
				if ((1 << PropIndex) & p->PropMask)
				{
					return p->Name;
				}
			}
			else if (p->Index == PropIndex)
			{
				//todo: not supporting arrays here, arrays relies on class' property table matching layout
				return p->Name;
			}
		}
		if (IsOurClass)
		{
			// the class has been verified, and we didn't find a property
			bClassFound = true;
			break;
		}
		// skip END marker
		p++;
	}

	if (bClassFound)
	{
		// We have a declaration of the class, so don't fall back to PROP declaration
		//todo: review later, actually can "return NULL" from inside the loop body
		return NULL;
	}

	// The property not found. Try using CTypeInfo properties, assuming their layout matches UE
	int CurrentPropIndex = 0;
	for (int Index = 0; Index < NumProps; Index++)
	{
		const CPropInfo& Prop = Props[Index];
		if (Prop.Count >= 2)
		{
			// Static array, should count each item as a separate property
			if (CurrentPropIndex + Prop.Count > PropIndex)
			{
				// The property is located inside this array
				OutArrayIndex = PropIndex - CurrentPropIndex;
				return Prop.Name;
			}
			CurrentPropIndex += Prop.Count;
		}
		else
		{
			// The same code, but works as Count == 1 for any values
			if (CurrentPropIndex == PropIndex)
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
