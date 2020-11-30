#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"

#if UNREAL4

/*-----------------------------------------------------------------------------
	UE4.26+ unversioned property table
-----------------------------------------------------------------------------*/

const char* CTypeInfo::FindUnversionedProp(int PropIndex, int& OutArrayIndex) const
{
	guard(CTypeInfo::FindUnversionedProp);

	OutArrayIndex = 0;

	struct OffsetInfo
	{
		const char* Name;
		int Index;
	};

#define BEGIN(type)			{ type,  0        },	// store class name as field name, index is not used
#define MAP(name,index)		{ #name, index    },	// field specification
#define END					{ NULL,  0        },	// end of class - mark with NULL name

#define DROP_INT8(index)	{ "#int8", index  },
#define DROP_INT64(index)	{ "#int64", index },
#define DROP_VECTOR3(index)	{ "#vec3", index  },
#define DROP_VECTOR4(index)	{ "#vec4", index  },	// FQuat, FGuid etc

	if (IsA("TextureCube4"))
	{
		// No properties, redirect to UTexture2D by adjusting the property index.
		// Add number of properties in UTexture2D
		PropIndex += 6;
	}

	// todo: can support parent classes (e.g. UStreamableRenderAsset) by recognizing and adjusting index, like we did for UTextureCube
	// todo: move to separate cpp
	static const OffsetInfo info[] =
	{
	BEGIN("StaticMesh4")
		DROP_INT64(0)						// FPerPlatformInt MinLOD - serialized as 2x int32, didn't find why
		MAP(StaticMaterials, 2)
		MAP(LightmapUVDensity, 3)
		DROP_INT8(9)						// uint8 bGenerateMeshDistanceField
		DROP_INT8(10)
		DROP_INT8(11)
		DROP_INT8(12)
		DROP_INT8(13)
		DROP_INT8(14)
		DROP_INT8(15)
		DROP_INT8(16)
		MAP(Sockets, 17)
		DROP_VECTOR3(18)					// FVector PositiveBoundsExtension
		DROP_VECTOR3(19)					// FVector NegativeBoundsExtension
		MAP(AssetUserData, 22)

		MAP(ExtendedBounds, 20)
	END

	BEGIN("Texture2D")
		DROP_INT8(2)						// uint8 bTemporarilyDisableStreaming:1
		DROP_INT8(3)						// TEnumAsByte<enum TextureAddress> AddressX
		DROP_INT8(4)						// TEnumAsByte<enum TextureAddress> AddressY
		DROP_INT64(5)						// FIntPoint ImportedSize
		// Parent class UTexture, starts with index 6
		DROP_VECTOR4(6)						// FGuid LightingGuid
		DROP_INT8(8)						// TEnumAsByte<enum TextureCompressionSettings> CompressionSettings
		DROP_INT8(9)						// TEnumAsByte<enum TextureFilter> Filter
		DROP_INT8(11)						// TEnumAsByte<enum TextureGroup> LODGroup
		DROP_INT8(14)						// uint8 SRGB:1
		DROP_INT8(15)						// uint8 bNoTiling:1
		DROP_INT8(16)
		DROP_INT8(17)
		DROP_INT8(18)
		DROP_INT8(19)
		// UStreamableRenderAsset starts at index 21
		DROP_INT64(21)						// double ForceMipLevelsToBeResidentTimestamp
		DROP_INT8(25)
		DROP_INT8(26)
		DROP_INT8(27)
		DROP_INT8(28)
		DROP_INT8(29)
		DROP_INT8(30)
	END
	};

#undef MAP
#undef BEGIN
#undef END

	// Find a field
	const OffsetInfo* p;
	const OffsetInfo* end;

	p = info;
	end = info + ARRAY_COUNT(info);

	bool bClassFound = false;
	while (p < end)
	{
		// Note: StrucType could correspond to a few classes from the list about
		// because of inheritance, so don't "break" a loop when we've scanned some class, check
		// other classes too
		bool IsOurClass = IsA(p->Name);

		while (++p < end && p->Name)
		{
			if (!IsOurClass) continue;
			if (p->Index == PropIndex)
			{
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
