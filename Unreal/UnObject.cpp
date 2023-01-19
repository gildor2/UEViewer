#include "Core.h"
#include "UnCore.h"
#include "UE4Version.h"
#include "UnObject.h"
#include "UnrealPackage/UnPackage.h"

#include "GameDatabase.h"		// for GetGameTag()


//#define DEBUG_PROPS				1
//#define PROFILE_LOADING			1

#define DUMP_SHOW_PROP_INDEX	0
#define DUMP_SHOW_PROP_TYPE		0


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

bool (*GBeforeLoadObjectCallback)(UObject*) = NULL;

UObject::UObject()
:	PackageIndex(INDEX_NONE)
{
//	appPrintf("creating (%p)\n", this);
}

UObject::~UObject()
{
//	appPrintf("deleting %s (%p) - package %s, index %d\n", Name, this, Package ? Package->Name : "None", PackageIndex);
	// remove self from GObjObjects
	GObjObjects.RemoveSingle(this);
	// remove self from package export table
	// note: we using PackageIndex==INDEX_NONE when creating dummy object, not exported from
	// any package, but which still belongs to this package (for example check Rune's
	// USkelModel)
	if (Package && PackageIndex != INDEX_NONE)
	{
		FObjectExport &Exp = Package->GetExport(PackageIndex);
		assert(Exp.Object == this);
		Exp.Object = NULL;
		Package = NULL;
	}
}


const char *UObject::GetRealClassName() const
{
	if (!Package || (PackageIndex == INDEX_NONE)) return GetClassName();
	const FObjectExport &Exp = Package->GetExport(PackageIndex);
	return Package->GetClassNameFor(Exp);
}

const char *UObject::GetPackageName() const
{
	if (!Package) return "None";
	return Package->Name;
}

const char *UObject::GetUncookedPackageName() const
{
	if (!Package) return "None";
	return Package->GetUncookedPackageName(PackageIndex);
}


// Package.Group.Object
void UObject::GetFullName(char *buf, int bufSize, bool IncludeObjectName, bool IncludeCookedPackageName, bool ForcePackageName) const
{
	guard(UObject::GetFullName);
	if (!Package)
	{
		buf[0] = 0;
		return;
	}
#if UNREAL4
	if (Package != NULL && Package->Game >= GAME_UE4_BASE)
	{
		// UE4 has full package path in its name, use it
		appStrncpyz(buf, *Package->GetFilename(), bufSize);
		char* s = strrchr(buf, '.');
		if (!s)
		{
			s = buf + strlen(buf);
			*s = '.';
		}
		s++;
		appStrncpyz(s, Name, bufSize - (s - buf));
		return;
	}
#endif // UNREAL4
	if (PackageIndex == INDEX_NONE)
	{
		// This is a dummy generated export
		appSprintf(buf, bufSize, "%s.%s", Package->Name, Name);
		return;
	}
	const FObjectExport &Exp = Package->GetExport(PackageIndex);
	if (!Exp.PackageIndex && ForcePackageName)
	{
		// no Outer - use the package name for this
		assert(IncludeObjectName);	//!! IncludeObjectName is ignored here!
		appSprintf(buf, bufSize, "%s.%s", Package->Name, Name);
		return;
	}
	Package->GetFullExportName(Exp, buf, bufSize, IncludeObjectName, IncludeCookedPackageName);
	unguard;
}

const FArchive* UObject::GetPackageArchive() const
{
	return Package ? static_cast<const FArchive*>(Package) : NULL;
}

int UObject::GetGame() const
{
	return Package ? Package->Game : GAME_UNKNOWN;
}

int UObject::GetArVer() const
{
	return Package ? Package->ArVer : 0;
}

int UObject::GetLicenseeVer() const
{
	return Package ? Package->ArLicenseeVer : 0;
}


/*-----------------------------------------------------------------------------
	UObject loading from package
-----------------------------------------------------------------------------*/

int              UObject::GObjBeginLoadCount = 0;
TArray<UObject*> UObject::GObjLoaded;
TArray<UObject*> UObject::GObjObjects;
UObject         *UObject::GLoadingObj = NULL;


void UObject::BeginLoad()
{
	assert(GObjBeginLoadCount >= 0);
	GObjBeginLoadCount++;
}


void UObject::EndLoad()
{
	assert(GObjBeginLoadCount > 0);
	if (GObjBeginLoadCount > 1)
	{
		GObjBeginLoadCount--;
		return;
	}

	guard(UObject::EndLoad);

	// Process GObjLoaded array. Note that GObjLoaded may receive new element during PostLoad call
	// (e.g. UMaterial3::ScanUE4Textures() does that), so we should use an outer loop to ensure
	// all objects will be loaded.
	while (GObjLoaded.Num())
	{
		TArray<UObject*> LoadedObjects;
		while (GObjLoaded.Num())
		{
			UObject *Obj = GObjLoaded[0];
			GObjLoaded.RemoveAt(0);
			UnPackage *Package = Obj->Package;

			guard(LoadObject);
			PROFILE_LABEL(Obj->GetClassName());

			Package->SetupReader(Obj->PackageIndex);
			if (!(Obj->GetTypeinfo()->TypeFlags & TYPE_SilentLoad))
			{
				appPrintf("Loading %s %s from package %s\n", Obj->GetClassName(), Obj->Name, *Package->GetFilename());
			}
			// setup NotifyInfo to describe object
			appSetNotifyHeader("Loading object %s'%s.%s'", Obj->GetClassName(), Package->Name, Obj->Name);
#if PROFILE_LOADING
			appResetProfiler();
#endif
			GLoadingObj = Obj;
			Obj->Serialize(*Package);
			GLoadingObj = NULL;
#if PROFILE_LOADING
			appPrintProfiler();
#endif
			// check for unread bytes
			if (!Package->IsStopper())
				appError("%s::Serialize(%s): %d unread bytes",
					Obj->GetClassName(), Obj->Name,
					Package->GetStopper() - Package->Tell());
			LoadedObjects.Add(Obj);

#if UNREAL4
	#define UNVERS_STR		(Package->Game >= GAME_UE4_BASE && Package->Summary.IsUnversioned) ? " (unversioned)" : ""
	#define EDITOR_STR		(Package->ContainsEditorData()) ? " (editor)" : ""
#else
	#define UNVERS_STR		""
	#define EDITOR_STR		""
#endif

			unguardf("%s'%s.%s', pos=%X, ver=%d/%d%s%s, game=%s", Obj->GetClassName(), Package->Name, Obj->Name, Package->Tell(),
				Package->ArVer, Package->ArLicenseeVer, UNVERS_STR, EDITOR_STR, GetGameTag(Package->Game));
		}
		// postload objects
		for (UObject* Obj : LoadedObjects)
		{
			guard(PostLoad);
			Obj->PostLoad();
			unguardf("%s", Obj->Name);
		}
	}

	// Cleanup
	GObjBeginLoadCount--;		// decrement after loading
	appSetNotifyHeader(NULL);
	assert(GObjBeginLoadCount == 0);
	// Close all opened file handles
	UnPackage::CloseAllReaders();

	unguard;
}


/*-----------------------------------------------------------------------------
	Properties support
-----------------------------------------------------------------------------*/

//?? add argument "Count" and use it for array<> serialization
//?? note: keep old code for TArray<FVector> etc - it is faster (used in UE3)
static bool SerializeStruc(FArchive &Ar, void *Data, int Index, const char *StrucName)
{
	guard(SerializeStruc);

	bool bForceTaggedSerialize = false;

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677 && !strcmp(StrucName, "FBoxSphereBounds"))
	{
		// Native serializer for FBoxSphereBounds in MK
		Ar << *(FBoxSphereBounds*)Data;
		return true;
	}
#endif // MKVSDC
	if (Ar.ArVer < 300 && !strcmp(StrucName, "FLinearColor"))
	{
		// real version is unknown; native FLinearColor serializer does not work with EndWar
		bForceTaggedSerialize = true;
	}

	// Serialize nested property block
	const CTypeInfo *ItemType = FindStructType(StrucName);
	if (!ItemType) return false;
	if (ItemType->NativeSerializer)
	{
		guard(NativeSerializer);
	#if DEBUG_PROPS
		appPrintf(">>> Native: %s\n", ItemType->Name);
	#endif
		ItemType->NativeSerializer(Ar, (byte*)Data + Index * ItemType->SizeOf);
		unguard;
	}
	else
	{
		ItemType->SerializeUnrealProps(Ar, (byte*)Data + Index * ItemType->SizeOf);
	}
	return true;

	unguardf("%s", StrucName);
}


enum EPropType // hardcoded in Unreal
{
	NAME_ByteProperty = 1,
	NAME_IntProperty,
	NAME_BoolProperty,
	NAME_FloatProperty,
	NAME_ObjectProperty,
	NAME_NameProperty,
	NAME_StringProperty,		// NAME_DelegateProperty in UE3 and UE4
	NAME_ClassProperty,
	NAME_ArrayProperty,
	NAME_StructProperty,
	NAME_VectorProperty,
	NAME_RotatorProperty,
	NAME_StrProperty,
	NAME_MapProperty,			// NAME_TextProperty in UE4
	NAME_FixedArrayProperty,	// NAME_InterfaceProperty in UE3
#if UNREAL4
	NAME_MulticastDelegateProperty = 16,
	NAME_WeakObjectProperty = 17,
	NAME_LazyObjectProperty = 18,
	NAME_SoftObjectProperty = 19,
	NAME_AssetObjectProperty = 19, // replaced with SoftObjectProperty in 4.18
	NAME_UInt64Property = 20,
	NAME_UInt32Property = 21,
	NAME_UInt16Property = 22,
	NAME_Int64Property = 23,
	// missing NAME_Int32Property - because of NAME_IntProperty?
	NAME_Int16Property = 25,
	NAME_Int8Property = 26,
	NAME_AssetSubclassProperty = 27,
	NAME_MapProperty_UE4 = 28,
	NAME_SetProperty = 29,
#endif // UNREAL4
	// property remaps from one engine to another
#if UNREAL3 || UNREAL4
	NAME_DelegateProperty = 7,
	NAME_InterfaceProperty = 15,
#endif
#if UNREAL4
	/// reference: Core/Public/UObject/UnrealNames.inl
	// Note: real enum indices doesn't matter in UE4 because type serialized as real FName
	NAME_TextProperty = 14,
	NAME_AttributeProperty,
	NAME_EnumProperty,
#endif
};

//?? use _ENUM/_E macros
static const struct
{
	int			Index;
	const char *Name;
} NameToIndex[] = {
#define F(Name)		{ NAME_##Name, #Name }
	F(ByteProperty),
	F(IntProperty),
	F(BoolProperty),
	F(FloatProperty),
	F(ObjectProperty),
	F(NameProperty),
	F(StringProperty),
	F(ClassProperty),
	F(ArrayProperty),
	F(StructProperty),
	F(VectorProperty),
	F(RotatorProperty),
	F(StrProperty),
	F(MapProperty),
	F(FixedArrayProperty),
#if UNREAL3
	F(DelegateProperty),
	F(InterfaceProperty),
#endif
#if UNREAL4
	F(AttributeProperty),
	F(SoftObjectProperty),
	F(AssetObjectProperty),
	F(AssetSubclassProperty),
	F(EnumProperty),
	F(UInt64Property),
	F(UInt32Property),
	F(UInt16Property),
	F(Int64Property),
	F(Int16Property),
	F(Int8Property),
	F(SetProperty),
#endif
#undef F
};

// Find NAME_... value according to FName value. Will return -1 if not found.
static int MapTypeName(const char *Name)
{
	guard(MapTypeName);
	for (int i = 0; i < ARRAY_COUNT(NameToIndex); i++)
		if (!stricmp(Name, NameToIndex[i].Name))
			return NameToIndex[i].Index;
	return INDEX_NONE;
	unguard;
}

static const char* GetTypeName(int Index)
{
	guard(GetTypeName);
	for (int i = 0; i < ARRAY_COUNT(NameToIndex); i++)
		if (Index == NameToIndex[i].Index)
			return NameToIndex[i].Name;
	if (Index == INDEX_NONE)
		return "(unknown)";
	appError("GetTypeName: unknown type index %d", Index);
	return NULL;
	unguard;
}


struct FPropertyTag
{
	FName		Name;
	int			Type;				// FName in Unreal Engine; always equals to one of hardcoded type names ?
	FName		StrucName;
	int			ArrayIndex;
	int			DataSize;
	int			BoolValue;
	FName		EnumName;			// UE3 ver >= 633
#if UNREAL4
	FName		InnerType;			// UE4 ver >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS
	FName		ValueType;			// UE4 ver >= VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT
#endif // UNREAL4

	bool IsValid()
	{
		return stricmp(Name, "None") != 0;
	}

	friend FArchive& operator<<(FArchive &Ar, FPropertyTag &Tag)
	{
		guard(FPropertyTag<<);
		assert(Ar.IsLoading);		// saving is not supported

#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 148)
		{
			assert(Ar.IsLoading);
			byte UseObject;
			int  Object;			// really, UObject*
			Ar << UseObject;
			if (UseObject)
			{
				// This code was never executed in my tests
				Ar << Object;
				if (!Object)
				{
					Tag.Name.Str = "None";
					return Ar;
				}
				// now, should continue serialization, skipping Name serialization (not implemented right now, so - appError)
				appError("Unknown field: Object #%d", Object);
			}
		}
#endif // UC2

#if WHEELMAN
		if (Ar.Game == GAME_Wheelman /* && MidwayVer >= 11 */)
		{
			FName PropType;
			int unk1C;
			Ar << PropType << Tag.Name << Tag.StrucName << Tag.DataSize << unk1C << Tag.BoolValue << Tag.ArrayIndex;
			Tag.Type = MapTypeName(PropType);
			if (Tag.Type < 0)
			{
				appError("MapTypeName: unknown type '%s' for property '%s'", *PropType, *Tag.Name);
			}
			// has special situation: PropType="None", Name="SerializedGroup", StrucName=Name of 1st serialized field,
			// DataSize = size of UObject block; possible, unk1C = offset of 1st serialized field
			return Ar;
		}
#endif // WHEELMAN

		Ar << Tag.Name;
		if (!stricmp(Tag.Name, "None"))
			return Ar;

#if UNREAL4
		if (Ar.Game >= GAME_UE4_BASE)
		{
			FName PropType;
			Ar << PropType << Tag.DataSize << Tag.ArrayIndex;

			// type-specific serialization
			Tag.Type = MapTypeName(PropType);
			if (Tag.Type < 0)
			{
				appNotify("WARNING: MapTypeName: unknown type '%s' for property '%s'", *PropType, *Tag.Name);
			}
			if (Tag.Type == NAME_StructProperty)
			{
				Ar << Tag.StrucName;
				if (Ar.ArVer >= VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
				{
					FGuid StructGuid;
					Ar << StructGuid;
				}
			}
			else if (Tag.Type == NAME_BoolProperty)
			{
				Ar << (byte&)Tag.BoolValue;		// byte
			}
			else if (Tag.Type == NAME_ByteProperty || Tag.Type == NAME_EnumProperty)
			{
				Ar << Tag.EnumName;
			}
			else if ((Tag.Type == NAME_ArrayProperty) && (Ar.ArVer >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS))
			{
				Ar << Tag.InnerType;
			}

			if (Ar.ArVer >= VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT)
			{
				if (Tag.Type == NAME_SetProperty)
				{
					Ar << Tag.InnerType;
				}
				else if (Tag.Type == NAME_MapProperty_UE4)
				{
					Ar << Tag.InnerType << Tag.ValueType;
				}
			}

			// serialize property Guid (seem used only for blueprint)
			if (Ar.ArVer >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG)
			{
				byte HasPropertyGuid;
				FGuid PropertyGuid;
				Ar << HasPropertyGuid;
				if (HasPropertyGuid) Ar << PropertyGuid;
			}

			return Ar;
		}
#endif // UNREAL4

#if UNREAL3
		if (Ar.Game >= GAME_UE3)
		{
			FName PropType;
			Ar << PropType << Tag.DataSize << Tag.ArrayIndex;
	#if MKVSDC
			if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
			{
				// MK X
				if (PropType == "EnumProperty")
				{
					Tag.Type = NAME_ByteProperty;
					return Ar;
				}
				else if (PropType == "BoolProperty")
				{
					Tag.Type = NAME_BoolProperty;
					Ar << Tag.BoolValue;			// as original code, but 'int' value (would be serialized as byte otherwise)
					return Ar;
				}
			}
	#endif // MKVSDC
			Tag.Type = MapTypeName(PropType);
			if (Tag.Type < 0)
			{
				appError("MapTypeName: unknown type '%s' for property '%s'", *PropType, *Tag.Name);
			}
			if (Tag.Type == NAME_StructProperty)
				Ar << Tag.StrucName;
			else if (Tag.Type == NAME_BoolProperty)
			{
				if (Ar.ArVer < 673)
					Ar << Tag.BoolValue;			// int
				else
					Ar << (byte&)Tag.BoolValue;		// byte
			}
			else if (Tag.Type == NAME_ByteProperty && Ar.ArVer >= 633)
			{
				bool bHasEnumName = true;
	#if GEARSU
				if (Ar.Game == GAME_GoWU) bHasEnumName = false;
	#endif

				if (bHasEnumName)
				{
					Ar << Tag.EnumName;
				}
			}
			return Ar;
		}
#endif // UNREAL3

		// UE1 and UE2
		byte info;
		Ar << info;

		bool IsArray = (info & 0x80) != 0;

		Tag.Type = info & 0xF;
		// serialize structure type name
//		Tag.StrucName = NAME_None;
		if (Tag.Type == NAME_StructProperty)
			Ar << Tag.StrucName;

		// analyze 'size' field
		Tag.DataSize = 0;
		switch ((info >> 4) & 7)
		{
		case 0: Tag.DataSize = 1; break;
		case 1: Tag.DataSize = 2; break;
		case 2: Tag.DataSize = 4; break;
		case 3: Tag.DataSize = 12; break;
		case 4: Tag.DataSize = 16; break;
		case 5: Ar << *((byte*)&Tag.DataSize); break;
		case 6: Ar << *((uint16*)&Tag.DataSize); break;
		case 7: Ar << *((int32*)&Tag.DataSize); break;
		}

		Tag.ArrayIndex = 0;
		if (Tag.Type != NAME_BoolProperty && IsArray)	// 'bool' type has own meaning of 'array' flag
		{
			// read array index
			byte b;
			Ar << b;
			if (b < 128)
				Tag.ArrayIndex = b;
			else
			{
				byte b2;
				Ar << b2;
				if (b & 0x40)			// really, (b & 0xC0) == 0xC0
				{
					byte b3, b4;
					Ar << b3 << b4;
					Tag.ArrayIndex = ((b << 24) | (b2 << 16) | (b3 << 8) | b4) & 0x3FFFFF;
				}
				else
					Tag.ArrayIndex = ((b << 8) | b2) & 0x3FFF;
			}
		}
		// boolean value
		Tag.BoolValue = 0;
		if (Tag.Type == NAME_BoolProperty)
			Tag.BoolValue = IsArray;

		return Ar;

		unguard;
	}
};


#if BATMAN

struct FPropertyTagBat2
{
	int16		Type;				// property type - used int16 instead of FName, 0 = end of property table
	uint16		Offset;				// property offset in serialized class
	// following fields are used when the property is serialized by name similar to original FPropertyTag
	FName		PropertyName;
	int			DataSize;
	int			ArrayIndex;
	byte		BoolValue;

	friend FArchive &operator<<(FArchive &Ar, FPropertyTagBat2 &Tag)
	{
		Ar << Tag.Type;
		if (!Tag.Type) return Ar;
		if (Ar.Game == GAME_Batman3 && Ar.ArLicenseeVer >= 104) goto named_prop; // Batman3 Online has old version of FPropertyTag
		Ar << Tag.Offset;
		if (Tag.Type == NAME_IntProperty || Tag.Type == NAME_FloatProperty || Tag.Type == NAME_NameProperty || Tag.Type == NAME_VectorProperty ||
			Tag.Type == NAME_RotatorProperty || Tag.Type == NAME_StrProperty || Tag.Type == 16 /*NAME_ObjectNCRProperty*/)
		{
		simple_prop:
			// property serialized by offset
			Tag.PropertyName.Str = "None";
			Tag.DataSize = Tag.ArrayIndex = 0;
			return Ar;
		}

		if (Ar.Game == GAME_Batman4 && Tag.Type == NAME_BoolProperty) goto simple_prop;	// serialized as 4 byte int, i.e. a bunch of boolean properties

	named_prop:
		// property serialized by name
		Ar << Tag.PropertyName << Tag.DataSize << Tag.ArrayIndex;
		if (Tag.Type == NAME_BoolProperty)
			Ar << Tag.BoolValue;
		return Ar;
	}
};


static bool FindPropBat2(const CTypeInfo *StrucType, const FPropertyTagBat2 &TagBat, FPropertyTag &Tag, int Game)
{
	struct OffsetInfo
	{
		const char	*Name;
		int			Offset;
	};

#define BEGIN(type)			{ type,  0    },		// store class name as field name, offset is not used
#define MAP(name,offs)		{ #name, offs },		// field specification
#define END					{ NULL,  0    },		// end of class - mark with NULL name

	static const OffsetInfo info[] =
	{

	BEGIN("Texture2D")
		MAP(SizeX, 0xE0)
		MAP(SizeY, 0xE4)
		MAP(TextureFileCacheName, 0x104)
//		MAP(OriginalSizeX, 0xE8)
//		MAP(OriginalSizeY, 0xEC)
	END

	BEGIN("Material3")
		MAP(OpacityMaskClipValue, 0x1A8)
	END

	BEGIN("MaterialInstance")
		MAP(Parent, 0x60)
	END

	BEGIN("ScalarParameterValue")
		MAP(ParameterName, 0)
		MAP(ParameterValue, 8)
	END

	BEGIN("TextureParameterValue")
		MAP(ParameterName, 0)
		MAP(ParameterValue, 8)
	END

	BEGIN("VectorParameterValue")
		MAP(ParameterName, 0)
		MAP(ParameterValue, 8)
	END

	BEGIN("AnimSequence")
		MAP(SequenceName, 0x2C)
		MAP(SequenceLength, 0x44)
		MAP(NumFrames, 0x48)
		MAP(RateScale, 0x4C)	//?? not checked
	END

	BEGIN("SkeletalMeshLODInfo")
		MAP(DisplayFactor, 0)
		MAP(LODHysteresis, 4)
	END

	BEGIN("SkeletalMeshSocket")
		MAP(SocketName, 0x2C)
		MAP(BoneName, 0x34)
		MAP(RelativeLocation, 0x3C)
		MAP(RelativeRotation, 0x48)
		MAP(RelativeScale, 0x54)
	END

	}; // end of info[]

	static const OffsetInfo info4[] =
	{

	BEGIN("Texture2D")
		MAP(SizeX, 0x144)
		MAP(SizeY, 0x148)
		MAP(TextureFileCacheName, 0x180)
//		MAP(OriginalSizeX, 0x14C)
//		MAP(OriginalSizeY, 0x150)
	END

	BEGIN("MaterialInstance")
		MAP(Parent, 0x84)
	END

	BEGIN("ScalarParameterValue")
		MAP(ParameterName, 0)
		MAP(ParameterValue, 8)
	END

	BEGIN("TextureParameterValue")
		MAP(ParameterName, 0)
		MAP(ParameterValue, 8)
	END

	BEGIN("VectorParameterValue")
		MAP(ParameterName, 0)
		MAP(ParameterValue, 8)
	END

	BEGIN("AnimSequence")
		MAP(SequenceName, 0x54)
		MAP(SequenceLength, 0x9C)
		MAP(NumFrames, 0xA0)
		MAP(RateScale, 0xA4)
	END

	//!! add Material.TwoSided, BlendMode, ...

	}; // end of info4[]

#undef MAP
#undef BEGIN
#undef END

	// prepare Tag
	Tag.Type       = TagBat.Type;
	Tag.Name.Str   = "unk";
	Tag.DataSize   = 0;			// unset
	Tag.ArrayIndex = 0;

	// find a field
	const OffsetInfo *p;
	const OffsetInfo *end;

	if (Game < GAME_Batman4)
	{
		p = info;
		end = info + ARRAY_COUNT(info);
	}
	else
	{
		assert(Game == GAME_Batman4);
		p = info4;
		end = info4 + ARRAY_COUNT(info4);
	}

	while (p < end)
	{
		// Note: StrucType could correspond to a few classes from the list about
		// because of inheritance, so don't "break" a loop when we've scanned some class, check
		// other classes too
		bool IsOurClass = StrucType->IsA(p->Name);

		while (++p < end && p->Name)
		{
			if (!IsOurClass) continue;

//			appPrintf("      ... check %s\n", p->Name);
			if (p->Offset == TagBat.Offset)
			{
				// found it
				Tag.Name.Str   = p->Name;
				Tag.Type       = TagBat.Type;
				Tag.DataSize   = 0;			// unset
				Tag.ArrayIndex = 0;
				return true;
			}
		}
		if (IsOurClass) break;				// the class has been verified, and we didn't find a property
		p++;								// skip END marker
	}

	return false;
}


#endif // BATMAN


void UObject::Serialize(FArchive &Ar)
{
	guard(UObject::Serialize);
	// stack frame
//	assert(!(ObjectFlags & RF_HasStack));

#if TRIBES3
	TRIBES_HDR(Ar, 0x19);
#endif

#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		if (Ar.ArVer < VER_UE4_REMOVE_NET_INDEX)
			goto net_index;
		goto no_net_index;
	}
#endif // UNREAL4

#if UNREAL3

#	if WHEELMAN || MKVSDC
	if ( ((Ar.Game == GAME_Wheelman || Ar.Game == GAME_TNA) && Ar.ArVer >= 385) ||
		  (Ar.Game == GAME_MK && Ar.ArVer >= 446) )
		goto no_net_index;
#	endif
#	if MURDERED
	if (Ar.Game == GAME_Murdered && Ar.ArLicenseeVer >= 93)
		goto no_net_index;
#	endif
#	if VEC
	if (Ar.Game == GAME_VEC)
	{
		FString str1, str2;
		if (Ar.ArVer >= 869) Ar << str1;
		if (Ar.ArVer >= 871) Ar << str2;
	}
#	endif // VEC
#	if BATMAN
	if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 203)
		goto no_net_index;
#	endif // BATMAN
	if (Ar.ArVer >= 322)
	{
	net_index:
		Ar << NetIndex;
	}
no_net_index:
#	if DCU_ONLINE
	if (Ar.Game == GAME_DCUniverse && NetIndex != -1)
		Ar.Seek(Ar.Tell() + 28);		// does help in some (not all) situations
#	endif

#endif // UNREAL3

	const CTypeInfo *Type = GetTypeinfo();
	assert(Type);

	if (Type->IsA("Class"))		// no properties for UClass
		return;

#if UNREAL4
	if ((Ar.Game >= GAME_UE4(26)) && (Package->Summary.PackageFlags & PKG_UnversionedProperties))
	{
	#if DEBUG_PROPS
		DUMP_ARC_BYTES(Ar, 512, "ALL");
	#endif
		Type->SerializeUnversionedProperties4(Ar, this);
	}
	else
#endif // UNREAL4
	{
		Type->SerializeUnrealProps(Ar, this);
	}

#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		// PossiblySerializeObjectGuid()
		int bSerializeGuid;
		Ar << bSerializeGuid;
		if (Ar.Game >= GAME_UE4(26) && (Package->Summary.PackageFlags & PKG_UnversionedProperties))
		{
			if (bSerializeGuid != 0 && bSerializeGuid != 1)
				appError("Unversioned properties problem");
		}
		if (bSerializeGuid)
		{
			FGuid guid;
			Ar << guid;
		}
	}
#endif // UNREAL4

	unguard;
}


// References in UE4:
// UStruct::SerializeVersionedTaggedProperties()
//  -> FPropertyTag::SerializeTaggedProperty()
//  -> FXxxProperty::SerializeItem()
void CTypeInfo::SerializeUnrealProps(FArchive& Ar, void* ObjectData) const
{
	guard(CTypeInfo::SerializeUnrealProps);

#define PROP(type)		( ((type*)value)[ArrayIndex] )

#if DEBUG_PROPS
#	define PROP_DBG(fmt, ...) \
		appPrintf("  %s[%d] = " fmt "\n", *Tag.Name, Tag.ArrayIndex, __VA_ARGS__);
#else
#	define PROP_DBG(fmt, ...)
#endif

#if BATMAN
	if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4)
	{
		SerializeBatmanProps(Ar, ObjectData);
		return;
	}
#endif // BATMAN

	if (NativeSerializer)
	{
		guard(NativeSerializer);
	#if DEBUG_PROPS
		appPrintf(">>> Native: %s\n", Name);
	#endif
		NativeSerializer(Ar, ObjectData);
		return;
		unguard;
	}

#if DEBUG_PROPS
	appPrintf(">>> Enter struct: %s\n", Name);
#endif

	// property list
	while (true)
	{
		FPropertyTag Tag;
		int PropTagPos = Ar.Tell();

		Ar << Tag;
		if (!Tag.IsValid())						// end marker
			break;

		ReadUnrealProperty(Ar, Tag, ObjectData, PropTagPos);
	}

#if DEBUG_PROPS
	appPrintf("<<< End of struct: %s\n", Name);
#endif

	unguard;
}


void CTypeInfo::ReadUnrealProperty(FArchive& Ar, FPropertyTag& Tag, void* ObjectData, int PropTagPos) const
{
	guard(CTypeInfo::ReadUnrealProperty);

	const char* DbgTypeName = "??";
#if BATMAN
	if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4) // copy-paste of code above
	{
		if (Tag.Type < 16)
			DbgTypeName = GetTypeName(Tag.Type);
		else if (Tag.Type == 16)
			DbgTypeName = "ObjectNCRProperty";
		else if (Tag.Type == 17)
			DbgTypeName = "GuidProperty";
	}
	else
#endif // BATMAN
	{
		DbgTypeName = GetTypeName(Tag.Type);
	}

#if DEBUG_PROPS
	appPrintf("-- %s \"%s::%s\" [%d] size=%d enum=%s struc=%s\n", DbgTypeName, Name, *Tag.Name,
		Tag.ArrayIndex, Tag.DataSize, *Tag.EnumName, *Tag.StrucName);
#endif // DEBUG_PROPS

	int StopPos = Ar.Tell() + Tag.DataSize;	// for verification

	const CPropInfo *Prop = FindProperty(Tag.Name);
	if (!Prop || Prop->Count == 0)	// Prop->Count==0 when declared with PROP_DROP() macro
	{
		if (!Prop)
			appPrintf("%s: unknown %s %s\n", DbgTypeName, Name, *Tag.Name); // notify about the unknown property
#if DEBUG_PROPS
		appPrintf("  (skipping %s)\n", *Tag.Name);
#endif
	skip_property:
		if (Tag.Type == NAME_BoolProperty && Tag.DataSize != 0)
		{
			// BoolProperty has everything in FPropertyTag, but sometimes it has DataSize==4, what
			// causes error when trying to skip it. This is why we have a separate code here.
			appPrintf("WARNING: skipping BoolProperty %s with Tag.Size=%d\n", *Tag.Name, Tag.DataSize);
			return;
		}
		// skip property data
		Ar.Seek(StopPos);
		// serialize other properties
		return;
	}

	// verify array index
	if (Tag.Type == NAME_ArrayProperty)
	{
		if (!(Prop->Count == -1 && Tag.ArrayIndex == 0))
		{
			appPrintf("ERROR: Struct \"%s\": %s %s has Prop.Count=%d (should be -1) and ArrayIndex=%d (should be 0). Skipping property.\n",
				Name, Prop->TypeName, Prop->Name, Prop->Count, Tag.ArrayIndex);
			goto skip_property;
		}
	}
	else if (Tag.ArrayIndex < 0 || Tag.ArrayIndex >= Prop->Count)
	{
		appError("Struct \"%s\": %s %s[%d]: invalid index %d",
			Name, Prop->TypeName, Prop->Name, Prop->Count, Tag.ArrayIndex);
	}
	byte *value = (byte*)ObjectData + Prop->Offset;

// If we'll put PropType namespace values into .cpp, we'll have identical constants, so we could
// simply use (TypeField == Constant). However this breaks compile-time creation of CTypeInfo
// property lists, they will be created at runtime. So, let's use strcmp at runtime instead.
#define COMPARE_TYPE(TypeField, Constant)	( strcmp(TypeField, Constant) == 0 )

#define CHECK_TYPE(TypeConst) \
	if (!COMPARE_TYPE(Prop->TypeName, TypeConst)) \
		appError("Property %s expected type %s but read %s", *Tag.Name, TypeConst, Prop->TypeName)

	int ArrayIndex = Tag.ArrayIndex;
	switch (Tag.Type)
	{
	case NAME_ByteProperty:
#if UNREAL3
		if (Tag.DataSize != 1)
		{
			// modern UE3 enum saved as FName
			assert(Tag.DataSize == 8);
			FName EnumValue;
			Ar << EnumValue;
			if (Prop->TypeName[0] == '#')
			{
				int tmpInt = NameToEnum(Prop->TypeName+1, *EnumValue);
				if (tmpInt == ENUM_UNKNOWN)
					appNotify("unknown member %s of enum %s", *EnumValue, Prop->TypeName+1);
				assert(tmpInt >= 0 && tmpInt <= 255);
				*value = tmpInt;
				PROP_DBG("%s", *EnumValue);
			}
			else
			{
				// error: this property is not marked using PROP_ENUM2
				CHECK_TYPE(PropType::Byte);
				appNotify("EnumProp: %s = %s\n", *Tag.Name, *EnumValue);
//				PROP_DBG("%s", PROP(byte));
			}
		}
		else
#endif // UNREAL3
		{
			if (Prop->TypeName[0] != '#')
			{
				CHECK_TYPE(PropType::Byte);
			}
			Ar << PROP(byte);
			PROP_DBG("%d", PROP(byte));
		}
		break;

	case NAME_IntProperty:
		CHECK_TYPE(PropType::Int);
		Ar << PROP(int);
		PROP_DBG("%d", PROP(int));
		break;

	case NAME_BoolProperty:
		CHECK_TYPE(PropType::Bool);
		PROP(bool) = Tag.BoolValue != 0;
		PROP_DBG("%s", PROP(bool) ? "true" : "false");
		break;

	case NAME_FloatProperty:
		CHECK_TYPE(PropType::Float);
		Ar << PROP(float);
		PROP_DBG("%g", PROP(float));
		break;

	case NAME_ObjectProperty:
		CHECK_TYPE(PropType::UObject);
		Ar << PROP(UObject*);
		PROP_DBG("%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
		break;

	case NAME_NameProperty:
		CHECK_TYPE(PropType::FName);
		Ar << PROP(FName);
		PROP_DBG("%s", *PROP(FName));
		break;

	case NAME_ClassProperty:
		appError("Class property is not implemented");
		break;

	case NAME_ArrayProperty:
		{
#if DEBUG_PROPS
			appPrintf("  %s[] = {\n", *Tag.Name);
#endif
			FArray *Arr = (FArray*)value;

#define SIMPLE_ARRAY_TYPE(TypeConst, CppType) \
		if (COMPARE_TYPE(Prop->TypeName, TypeConst)) { Ar << *(TArray<CppType>*)Arr; }

			SIMPLE_ARRAY_TYPE(PropType::Int, int)
			else SIMPLE_ARRAY_TYPE(PropType::Bool, bool)
			else SIMPLE_ARRAY_TYPE(PropType::Byte, byte)
			else SIMPLE_ARRAY_TYPE(PropType::Float, float)
			else SIMPLE_ARRAY_TYPE(PropType::UObject, UObject*)
			else SIMPLE_ARRAY_TYPE(PropType::FName, FName)
			else SIMPLE_ARRAY_TYPE(PropType::FVector, FVector)
			else SIMPLE_ARRAY_TYPE("FQuat", FQuat)
			else
			{
				// Read data count like generic TArray serializer does
				int32 DataCount;
#if UC2
				if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145)
				{
					Ar << DataCount;
				}
				else
#endif // UC2
#if UNREAL3
				if (Ar.Engine() >= GAME_UE3)
				{
					Ar << DataCount;
				}
				else
#endif // UNREAL3
				{
					// UE1 and UE2 code
					Ar << AR_INDEX(DataCount);
				}
	#if UNREAL4
				if (Ar.Engine() >= GAME_UE4_BASE && Ar.ArVer >= VER_UE4_INNER_ARRAY_TAG_INFO)
				{
					// Serialize InnerTag, used for some verification
					FPropertyTag InnerTag;
					Ar << InnerTag;
				}
	#endif // UNREAL4

				// Find data typeinfo
				//!! note: some structures should be serialized using SerializeStruc() (FVector etc)
				const CTypeInfo *ItemType = FindStructType(Prop->TypeName);
				if (!ItemType)
				{
					appPrintf("WARNING: structure type %s is unknown, skipping array %s::%s\n", Prop->TypeName, Name, Prop->Name);
					Ar.Seek(StopPos);
				}
				else if (DataCount)
				{
					// Prepare array
					Arr->Empty(DataCount, ItemType->SizeOf);
					Arr->InsertZeroed(0, DataCount, ItemType->SizeOf);

	#if UNREAL4
					// Second queue of "simple serializers", but with InnerTag in UE4.12+
					if (!stricmp(Prop->TypeName, "FLinearColor"))
					{
						// Reference: FMaterialCachedParameters::VectorValues
						FLinearColor* p = (FLinearColor*)Arr->GetData();
						for (int i = 0; i < DataCount; i++)
							Ar << *p++;
					}
					else
	#endif // UNREAL4
					{
						// Serialize items
						byte *item = (byte*)Arr->GetData();
						for (int i = 0; i < DataCount; i++, item += ItemType->SizeOf)
						{
	#if DEBUG_PROPS
							appPrintf("Item[%d]:\n", i);
	#endif
							if (ItemType->Constructor)
								ItemType->Constructor(item);		// fill default properties
							else
								memset(item, 0, ItemType->SizeOf);	// no constructor, just initialize with zeros

							ItemType->SerializeUnrealProps(Ar, item);
						}
#if 1
						// Fix for strange (UE?) bug - array[1] of empty structure has 1 extra byte
						// or size is incorrect - 1 byte smaller than should be (?)
						// Note: such properties cannot be dropped (PROP_DROP) - invalid Tag.Size !
						int Pos = Ar.Tell();
						if (Pos + 1 == StopPos && DataCount == 1)
						{
	#if DEBUG_PROPS
							appNotify("%s.%s: skipping 1 byte for array property", Name, *Tag.Name);
	#endif
							Ar.Seek(StopPos);
						}
						else if (Pos > StopPos)
						{
	#if DEBUG_PROPS
							appNotify("%s.%s: bad size (%d byte less) for array property", Name, *Tag.Name, Pos - StopPos);
	#endif
							StopPos = Pos;
						}
					}
#endif // 1 -- end of fix
				}
			}
#if DEBUG_PROPS
			appPrintf("  } // count=%d\n", Arr->Num());
#endif
		}
		break;

	case NAME_StructProperty:
		{
#if MKVSDC
			if (Ar.Game == GAME_MK && Ar.ArVer >= 677 && (*Tag.StrucName)[0] == 'F')
				Tag.StrucName.Str++;		// Tag.StrucName points to 'FStrucName' instead of 'StrucName'
#endif // MKVSDC

#if UNREAL4
			// Some type remap
			if (Ar.Game >= GAME_UE4_BASE)
			{
				if (stricmp(Prop->TypeName, PropType::FRotator4) == 0)
				{
					SerializeStruc(Ar, value, Tag.ArrayIndex, "FRotator4");
					break;
				}
			}
#endif // UNREAL4

			if (stricmp(Prop->TypeName+1, *Tag.StrucName) != 0 && stricmp(*Tag.StrucName, "None") != 0) // Tag.StrucName could be unknown in Batman2
			{
				appNotify("Struc property %s expected type %s but read %s", *Tag.Name, Prop->TypeName, *Tag.StrucName);
				Ar.Seek(StopPos);
			}
			else if (SerializeStruc(Ar, value, Tag.ArrayIndex, Prop->TypeName))
			{
				PROP_DBG("(struct:%s)", *Tag.StrucName);
				int Pos = Ar.Tell();
				if (Pos > StopPos)
				{
#if DEBUG_PROPS
					appNotify("%s.%s: bad size (%d byte less) for struct property", Name, *Tag.Name, Pos - StopPos);
#endif
					StopPos = Pos;
				}
			}
			else
			{
				//!! implement this (use FindStructType()->SerializeUnrealProps())
				appNotify("WARNING: Unknown structure type: %s", *Tag.StrucName);
				Ar.Seek(StopPos);
			}
		}
		break;

	case NAME_StrProperty:
		if (COMPARE_TYPE(Prop->TypeName, PropType::FName))
		{
			// serialize FString to FName
			// MK X, "TextureFileCacheName": UE3 has it as FName, but MK X as FString
			FString tmpStr;
			Ar << tmpStr;
			PROP(FName) = *tmpStr;
			PROP_DBG("%s", *PROP(FName));
		}
		else
		{
			// serialize FString
			CHECK_TYPE(PropType::FString);
			Ar << PROP(FString);
			PROP_DBG("%s", *PROP(FString));
		}
		break;

	case NAME_MapProperty:
		appError("Map property not implemented");
		break;

	case NAME_FixedArrayProperty:
		appError("FixedArray property not implemented");
		break;

#if UNREAL4
	case NAME_EnumProperty:
		{
			FName EnumValue;
			Ar << EnumValue;
			assert(Prop->TypeName[0] == '#')
			int tmpInt = NameToEnum(Prop->TypeName+1, *EnumValue);
			if (tmpInt == ENUM_UNKNOWN)
				appNotify("unknown member %s of enum %s", *EnumValue, Prop->TypeName+1);
			assert(tmpInt >= 0 && tmpInt <= 255);
			*value = tmpInt;
			PROP_DBG("%s", *EnumValue);
		}
		break;
#endif // UNREAL4

	// reserved, but not implemented in unreal:
	case NAME_StringProperty:	//------  string  => used str
	case NAME_VectorProperty:	//------  vector  => used structure"Vector"
	case NAME_RotatorProperty:	//------  rotator => used structure"Rotator"
#if BATMAN
		if (Ar.Game == GAME_Batman3)
		{
			if (Tag.Type == NAME_VectorProperty)
			{
				FVector& prop = PROP(FVector);
				Ar << prop;
				PROP_DBG("%g %g %g", prop.X, prop.Y, prop.Z);
				break;
			}
			else if (Tag.Type == NAME_RotatorProperty)
			{
				FRotator& prop = PROP(FRotator);
				Ar << prop;
				PROP_DBG("%g %g %g", prop.Yaw, prop.Pitch, prop.Roll);
				break;
			}
		}
#endif // BATMAN
		appError("Unknown property type %d, name %s", Tag.Type, *Tag.Name);
		break;
	}
	// verification
	int Pos = Ar.Tell();
	if (Pos != StopPos)
	{
#if DEBUG_PROPS
		appPrintf("Serialized size mismatch: Pos=%X, Stop=%X\n", Pos, StopPos);
		// show property dump
		Ar.Seek(PropTagPos);
		DUMP_ARC_BYTES(Ar, StopPos - PropTagPos, "Property bytes");
#endif
		appError("%s\'%s\'.%s: Property read error: %d unread bytes", Name, UObject::GLoadingObj->Name, *Tag.Name, StopPos - Pos);
	}

	unguardf("(%s.%s, Type=%d, Size=%d, TagPos=%X)", Name, *Tag.Name, Tag.Type, Tag.DataSize, PropTagPos);
}


#if BATMAN

void CTypeInfo::SerializeBatmanProps(FArchive& Ar, void* ObjectData) const
{
	guard(CTypeInfo::SerializeBatmanProps);

	// property list
	while (true)
	{
		// Batman 2 has more compact FPropertyTag implementation which uses serialization
		// by offset, not by name - this should work faster (in game) and use less disk space.
		int PropTagPos = Ar.Tell();
		FPropertyTagBat2 TagBat;
		Ar << TagBat;
		if (!TagBat.Type)			// end marker
			break;

#if DEBUG_PROPS
		const char *TypeName = "??";
		if (TagBat.Type < 16)
			TypeName = GetTypeName(TagBat.Type);
		else if (TagBat.Type == 16)
			TypeName = "ObjectNCRProperty";
		else if (TagBat.Type == 17)
			TypeName = "GuidProperty";
		appPrintf("Prop: type=%d (%s) offset=0x%X size=%d propName=%s\n", TagBat.Type, TypeName, TagBat.Offset, TagBat.DataSize, *TagBat.PropertyName);
#endif // DEBUG_PROPS
		if (stricmp(TagBat.PropertyName, "None") != 0)
		{
			// Property has extra information, it should be serialized as in original engine.
			// Convert tag to the standard format.
			FPropertyTag UnrealTag;
			UnrealTag.Name       = TagBat.PropertyName;
			UnrealTag.Type       = TagBat.Type;
			UnrealTag.DataSize   = TagBat.DataSize;
			UnrealTag.ArrayIndex = TagBat.ArrayIndex;
			UnrealTag.BoolValue  = TagBat.BoolValue;
			ReadUnrealProperty(Ar, UnrealTag, ObjectData, PropTagPos);
			continue;
		}

		static byte Dummy[16];
		byte *value = Dummy;
		const int ArrayIndex = 0;	// used in PROP macro
		FPropertyTag Tag;			// the variable name 'Tag' used in PROP_DBG
		if (FindPropBat2(this, TagBat, Tag, Ar.Game))
		{
			const CPropInfo *Prop = FindProperty(Tag.Name);
			if (Prop)
				value = (byte*)ObjectData + Prop->Offset;
		}

		// offset-based ("simple property") serialization
		switch (TagBat.Type)
		{
		case NAME_FloatProperty:
			Ar << PROP(float);
			PROP_DBG("%g", PROP(float));
			break;

		case NAME_NameProperty:
			Ar << PROP(FName);
			PROP_DBG("%s", *PROP(FName));
			break;

		case NAME_StrProperty:
			{
				//!! implement (needs inplace constructor/destructor, will not work for Dummy[])
				FString s;
				Ar << s;
//				appPrintf("... value = %s\n", *s);
			}
			break;

		case 16: // NAME_ObjectNCRProperty
			Ar << PROP(UObject*);
			PROP_DBG("%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
			break;

		case NAME_BoolProperty:
			{
				int BoolValue;
				Ar << BoolValue;
				PROP(bool) = (BoolValue != 0);
				PROP_DBG("%08X", BoolValue);
				// Ugly hack to support Batman4 bool properties
				if (Ar.Game == GAME_Batman4)
				{
					struct BoolPropInfo
					{
						const char* ClassName;
						const char* PropName;
						uint16		PropOffset;
						uint16		PropMask;
					};
					static const BoolPropInfo BoolProps[] =
					{
						{ "SkeletalMesh3", "bHasVertexColors", 0x528, 2 },
						{ "AnimSet", "bAnimRotationOnly", 0x5C, 1 },
					};
					for (int i = 0; i < ARRAY_COUNT(BoolProps); i++)
					{
						const BoolPropInfo& Info = BoolProps[i];
						if (TagBat.Offset == Info.PropOffset && IsA(Info.ClassName))
						{
							const CPropInfo* Prop2 = FindProperty(Info.PropName);
							if (Prop2)
							{
								byte* value = (byte*)ObjectData + Prop2->Offset;
								bool b = (BoolValue & Info.PropMask) != 0;
								*value = (byte)b;
								PROP_DBG("B4 %s=%d\n", Info.PropName, b);
							}
							break;
						}
					}
				}
			}
			break;

		case NAME_IntProperty:
			Ar << PROP(int);
			PROP_DBG("%d", PROP(int));
			break;

		case NAME_VectorProperty:
			Ar << PROP(FVector);
			PROP_DBG("%g %g %g", VECTOR_ARG(PROP(FVector)));
			break;

		case NAME_RotatorProperty:
			Ar << PROP(FRotator); // UE4 has the same memory layout, just int32 is replaced with float, so don't bother about different serializer
			PROP_DBG("%d %d %d", FROTATOR_ARG(PROP(FRotator)));
			break;

		case 17: // NAME_GuidProperty
			Ar << PROP(FGuid);
			PROP_DBG("%s", "(guid)");
			break;
		}
	}

	unguard;
}

#endif // BATMAN

#if UNREAL4

/*-----------------------------------------------------------------------------
	UE4.26+ unversioned properties
-----------------------------------------------------------------------------*/

struct FUnversionedHeader
{
	struct FFragment
	{
		int SkipNum;
		bool bHasAnyZeroes;
		bool bIsLast;
		uint8 ValueCount;

		void Unpack(uint16 Packed)
		{
			SkipNum       =  Packed & 0x7f;
			bHasAnyZeroes = (Packed & 0x80) != 0;
			bIsLast       = (Packed & 0x100) != 0;
			ValueCount    =  Packed >> 9;
		}
	};

	// Serialized values
	TArray<FFragment> Fragments;
	TArray<uint8> ZeroMask;

	// Values for property iteration
	int CurrentPropIndex;
	int CurrentFragmentIndex;
	int PropsLeftInFragment;
	int ZeroMaskIndex;
	const FFragment* CurrentFragment;

	void Load(FArchive& Ar)
	{
		guard(FUnversionedHeader::Load);

		FFragment Fragment;
		Fragment.bIsLast = false;
		int ZeroMaskSize = 0;

		while (!Fragment.bIsLast)
		{
			uint16 Packed;
			Ar << Packed;
			// Unpack data
			Fragment.Unpack(Packed);
			Fragments.Add(Fragment);
			if (Fragment.bHasAnyZeroes)
			{
				ZeroMaskSize += Fragment.ValueCount;
			}
		#if DEBUG_PROPS
			appPrintf("Frag: skip %d, %d props, zeros=%d, last=%d\n", Fragment.SkipNum, Fragment.ValueCount, Fragment.bHasAnyZeroes, Fragment.bIsLast);
		#endif
		}

		if (ZeroMaskSize)
		{
			int NumBytes;
			if (ZeroMaskSize <= 8)
				NumBytes = 1;
			else if (ZeroMaskSize <= 16)
				NumBytes = 2;
			else
				NumBytes = ((ZeroMaskSize + 31) >> 5) * 4; // round to uint32 size
			ZeroMask.SetNum(NumBytes);
			Ar.Serialize(&ZeroMask[0], NumBytes);
		}

		// Initialize iterator
		CurrentPropIndex = -0;
		CurrentFragmentIndex = -1;
		PropsLeftInFragment = 0;
		ZeroMaskIndex = 0;
		CurrentFragment = NULL;

		unguard;
	}

	// Property iteration
	bool GetNextProperty(int& PropIndex, bool& bIsZeroed)
	{
		guard(FUnversionedHeader::GetNextProperty);

		// Switch to next fragment if needed
		if (PropsLeftInFragment == 0)
		{
			while (true)
			{
				if (++CurrentFragmentIndex == Fragments.Num())
				{
					return false;
				}
				CurrentFragment = &Fragments[CurrentFragmentIndex];
				CurrentPropIndex += CurrentFragment->SkipNum;
				// There's a loop till fragment has any value, in a case that SkipNum doesn't fit 7 bits value
				if (CurrentFragment->ValueCount > 0)
				{
					PropsLeftInFragment = CurrentFragment->ValueCount;
					break;
				}
			}
		}

		if (CurrentFragment->bHasAnyZeroes)
		{
			bIsZeroed = (ZeroMask[ZeroMaskIndex >> 3] & (1 << (ZeroMaskIndex & 7))) != 0;
			ZeroMaskIndex++;
		}
		else
		{
			bIsZeroed = false;
		}

		PropIndex = CurrentPropIndex++;
		PropsLeftInFragment--;
		return true;

		unguard;
	}
};

// References in UE4:
// SerializeUnversionedProperties()
//  -> FUnversionedHeader
//  -> FUnversionedPropertySerializer
//  -> FLinkWalkingSchemaIterator
void CTypeInfo::SerializeUnversionedProperties4(FArchive& Ar, void* ObjectData) const
{
	guard(CTypeInfo::SerializeUnversionedProperties4);

#undef PROP_DBG
#if DEBUG_PROPS
#	define PROP_DBG(fmt, ...) \
		appPrintf("  %s[%d] = " fmt "\n", PropName, ArrayIndex, __VA_ARGS__);
#else
#	define PROP_DBG(fmt, ...)
#endif

	if (NativeSerializer)
	{
		guard(NativeSerializer);
	#if DEBUG_PROPS
		appPrintf(">>> Native: %s\n", Name);
	#endif
		NativeSerializer(Ar, ObjectData);
		return;
		unguard;
	}

#if DEBUG_PROPS
	appPrintf(">>> Enter struct: %s\n", Name);
	DUMP_ARC_BYTES(Ar, 32, "Header");
#endif

	FUnversionedHeader Header;
	Header.Load(Ar);

	int PropIndex;
	bool bIsZeroedProp;
	while (Header.GetNextProperty(PropIndex, bIsZeroedProp))
	{
	#if DEBUG_PROPS
		appPrintf("Prop: %d (zeroed=%d)\n", PropIndex, bIsZeroedProp);
	#endif
		int ArrayIndex = 0;
		const char* PropName = FindUnversionedProp(PropIndex, ArrayIndex, Ar.Game);
	#if DEBUG_PROPS
		DUMP_ARC_BYTES(Ar, 32, "-> ...");
	#endif
		if (bIsZeroedProp)
		{
		#if DEBUG_PROPS
			appPrintf("  skip zero prop %d (%s)\n", PropIndex, PropName);
		#endif
			continue;
		}

		if (!PropName)
		{
			// Skip the property as if it is int32 or float
		#if DEBUG_PROPS
			appPrintf("  unknown prop %d, skip as int32\n", PropIndex);
		#endif
			Ar.Seek(Ar.Tell() + 4);
			continue;
		}

		if (PropName[0] == '#')
		{
		#if DEBUG_PROPS
			appPrintf("  dropping %s\n", PropName + 1);
		#endif
			// Special marker, skipping property of known size
			if (!strcmp(PropName, "#int8"))
			{
				Ar.Seek(Ar.Tell() + 1);
			}
			else if (!strcmp(PropName, "#int64"))
			{
				Ar.Seek(Ar.Tell() + 8);
			}
			else if (!strcmp(PropName, "#vec3"))
			{
				Ar.Seek(Ar.Tell() + 12);
			}
			else if (!strcmp(PropName, "#vec4"))
			{
				Ar.Seek(Ar.Tell() + 16);
			}
			else if (!strcmp(PropName, "#arr_int32"))
			{
				int32 Len;
				Ar << Len;
				if (Len > 0)
					Ar.Seek(Ar.Tell() + Len * 4);
			}
			else
			{
				appError("Unknown marker: %s", PropName);
			}
			continue;
		}

		const CPropInfo* Prop = FindProperty(PropName);
		if (!Prop) appError("Property not found: %s\n", PropName);

		byte* value = (byte*)ObjectData + Prop->Offset; // used in PROP macro

		if (Prop->Count == -1)
		{
			guard(HandleTArray);
			// TArray
			// Reference: SerializeStruc(Ar, value, Tag.ArrayIndex, Prop->TypeName))
			// todo: try reusing code between versioned and unversioned properties
			FArray* Arr = (FArray*)value;

			bool bSerialized = true;
			SIMPLE_ARRAY_TYPE(PropType::Int, int)
			else SIMPLE_ARRAY_TYPE(PropType::Bool, bool)
			else SIMPLE_ARRAY_TYPE(PropType::Byte, byte)
			else SIMPLE_ARRAY_TYPE(PropType::Float, float)
			else SIMPLE_ARRAY_TYPE(PropType::UObject, UObject*)
			else SIMPLE_ARRAY_TYPE(PropType::FName, FName)
			else SIMPLE_ARRAY_TYPE(PropType::FVector, FVector)
			else SIMPLE_ARRAY_TYPE("FQuat", FQuat)
			else bSerialized = false;
			if (bSerialized)
			{
		#if DEBUG_PROPS
				appPrintf("  %s[%d] {}\n", PropName, Arr->Num());
		#endif
				continue;
			}

			const CTypeInfo* ItemType = FindStructType(Prop->TypeName);
			if (!ItemType)
				appError("Unknown structure type %s", Prop->TypeName);

			int32 DataCount;
			Ar << DataCount;
		#if DEBUG_PROPS
			appPrintf("  %s[%d] = {\n", PropName, DataCount);
		#endif

			// Prepare array
			Arr->Empty(DataCount, ItemType->SizeOf);
			Arr->InsertZeroed(0, DataCount, ItemType->SizeOf);

			// Serialize items
			byte* Item = (byte*)Arr->GetData();
			for (int i = 0; i < DataCount; i++, Item += ItemType->SizeOf)
			{
		#if DEBUG_PROPS
				appPrintf("Item[%d]:\n", i);
		#endif
				if (ItemType->Constructor)
					ItemType->Constructor(Item);		// fill default properties
				else
					memset(Item, 0, ItemType->SizeOf);	// no constructor, just initialize with zeros

				ItemType->SerializeUnversionedProperties4(Ar, Item);
			}
		#if DEBUG_PROPS
			appPrintf("  } // end of array\n", PropName, DataCount);
		#endif
			continue;
			unguard;
		}

		if (Prop->Count == 0)
		{
			guard(HandleDrop);
			// Handle PROP_DROP macro
		#if DEBUG_PROPS
			appPrintf("Drop %s\n", Prop->Name);
		#endif
			// Assume DROP_PROP without a type specifier
			if (!Prop->TypeName ||
				COMPARE_TYPE(Prop->TypeName, PropType::Int) ||
				COMPARE_TYPE(Prop->TypeName, PropType::Float) ||
				COMPARE_TYPE(Prop->TypeName, PropType::UObject))
			{
				Ar.Seek(Ar.Tell() + 4);
			}
			else if (COMPARE_TYPE(Prop->TypeName, PropType::Byte) ||
				COMPARE_TYPE(Prop->TypeName, PropType::Bool))
			{
				Ar.Seek(Ar.Tell() + 1);
			}
			else if (COMPARE_TYPE(Prop->TypeName, PropType::FVector4))
			{
				Ar.Seek(Ar.Tell() + 16);
			}
			else if (COMPARE_TYPE(Prop->TypeName, PropType::FName))
			{
				Ar.Seek(Ar.Tell() + 8);
			}
			else if (COMPARE_TYPE(Prop->TypeName, PropType::FString))
			{
				int32 Len;
				Ar << Len;
				if (Len > 0)
					Ar.Seek(Ar.Tell() + Len);
				else if (Len < 0)
					Ar.Seek(Ar.Tell() - Len * 2);
			}
			else
			{
				const CTypeInfo* ItemType = FindStructType(Prop->TypeName);
				if (!ItemType)
					appError("PROP_DROP(%s::%s) with unknown type %s", Name, Prop->Name, Prop->TypeName);
			#if DEBUG_PROPS
				appPrintf("  dropping struct %s\n", Prop->TypeName);
			#endif
				ItemType->SerializeUnversionedProperties4(Ar, NULL);
			}
			continue;
			unguard;
		}

		guard(HandleSimpleProp);
		assert(Prop->Count >= 1);

		if (Prop->TypeName == NULL)
		{
			// Declared as PROP_DROP, consider int32 serializer
	#if DEBUG_PROPS
			appPrintf("  (skipping %s as int32)\n", PropName);
	#endif
			Ar.Seek(Ar.Tell() + 4);
		}
		else if (COMPARE_TYPE(Prop->TypeName, PropType::Bool))
		{
			Ar << PROP(byte); // 1-byte boolean (actually UE4 has cases for different bool sizes)
			PROP_DBG("%d", PROP(int));
		}
		else if (COMPARE_TYPE(Prop->TypeName, PropType::Int))
		{
			Ar << PROP(int);
			PROP_DBG("%d", PROP(int));
		}
		else if (COMPARE_TYPE(Prop->TypeName, PropType::Float))
		{
			Ar << PROP(float);
			PROP_DBG("%g", PROP(float));
		}
		else if (COMPARE_TYPE(Prop->TypeName, PropType::UObject))
		{
			Ar << PROP(UObject*);
			PROP_DBG("%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
		}
		else if (COMPARE_TYPE(Prop->TypeName, PropType::FName))
		{
			Ar << PROP(FName);
			PROP_DBG("%s", *PROP(FName));
		}
		//todo: use NATIVE_SERIALIZER!
		else if (COMPARE_TYPE(Prop->TypeName, PropType::FString))
		{
			Ar << PROP(FString);
			PROP_DBG("%s", *PROP(FString));
		}
		else if (COMPARE_TYPE(Prop->TypeName, PropType::FVector))
		{
			Ar << PROP(FVector);
			PROP_DBG("%g %g %g", VECTOR_ARG(PROP(FVector)));
		}
		else if (COMPARE_TYPE(Prop->TypeName, "FLinearColor"))
		{
			Ar << PROP(FLinearColor);
			PROP_DBG("%g %g %g %g", FCOLOR_ARG(PROP(FLinearColor)));
		}
		else if (Prop->TypeName[0] == '#' && Prop->TypeName[1] == 'E')
		{
			// enum, serialize as byte
			uint8& p = PROP(uint8);
			Ar << p;
		#if DEBUG_PROPS
			const char* EnumName = EnumToName(Prop->TypeName + 1, p);
			appPrintf("  %s = %d (enum %s::%s)\n", PropName, p, Prop->TypeName + 1, EnumName ? EnumName : "<unknown>");
		#endif
		}
		else
		{
			const CTypeInfo* ItemType = FindStructType(Prop->TypeName);
			if (!ItemType)
				appError("Unknown property type %s (%s[%d] -> %s)\n", Prop->TypeName, Name, PropIndex, PropName);
			ItemType->SerializeUnversionedProperties4(Ar, value + ArrayIndex * ItemType->SizeOf);
		}
		unguardf("Count=%d, Index=%d", Prop->Count, ArrayIndex);
	}

#if DEBUG_PROPS
	appPrintf("<<< End of struct: %s\n", Name);
#endif

	unguardf("Type=%s", Name);
}

#endif // UNREAL4

/*-----------------------------------------------------------------------------
	UObject creation
-----------------------------------------------------------------------------*/

UObject *CreateClass(const char *Name)
{
	guard(CreateClass);

	const CTypeInfo *Type = FindClassType(Name);
	if (!Type) return NULL;

	UObject *Obj = (UObject*)appMalloc(Type->SizeOf);
	assert(Type->Constructor);
	Type->Constructor(Obj);
	// NOTE: do not add object to GObjObjects in UObject constructor
	// to allow runtime creation of objects without linked package
	// Really, should add to this list after loading from package
	// (in CreateExport/Import or after serialization)
	UObject::GObjObjects.Add(Obj);
	return Obj;

	unguardf("%s", Name);
}


/*-----------------------------------------------------------------------------
	Typeinfo for Core classes
-----------------------------------------------------------------------------*/

// Native serializers are defined in UE4 as "atomic" or "immutable" mark,
// or with WithSerializer type trait.
// reference: CoreUObject/Classes/Object.h
// Modern UE4 native serializers are defined in CoreUObject/Private/UObject/Property.cpp

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FVector)
	PROP_FLOAT(X)
	PROP_FLOAT(Y)
	PROP_FLOAT(Z)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FQuat)
	PROP_FLOAT(X)
	PROP_FLOAT(Y)
	PROP_FLOAT(Z)
	PROP_FLOAT(W)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FRotator)
	PROP_INT(Yaw)
	PROP_INT(Pitch)
	PROP_INT(Roll)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FColor)
	PROP_BYTE(R)
	PROP_BYTE(G)
	PROP_BYTE(B)
	PROP_BYTE(A)
END_PROP_TABLE_EXTERNAL

#if UNREAL3

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FLinearColor)
	PROP_FLOAT(R)
	PROP_FLOAT(G)
	PROP_FLOAT(B)
	PROP_FLOAT(A)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL(FBoxSphereBounds)
	PROP_VECTOR(Origin)
	PROP_VECTOR(BoxExtent)
	PROP_FLOAT(SphereRadius)
END_PROP_TABLE_EXTERNAL

#endif // UNREAL3

#if UNREAL4

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FRotator4)
	PROP_FLOAT(Yaw)
	PROP_FLOAT(Pitch)
	PROP_FLOAT(Roll)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL(FTransform)
	PROP_STRUC(Rotation, FQuat)
	PROP_VECTOR(Translation)
	PROP_VECTOR(Scale3D)
END_PROP_TABLE_EXTERNAL

#endif // UNREAL4

#if UNREAL4

BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(FIntPoint)
	PROP_INT(X)
	PROP_INT(Y)
END_PROP_TABLE_EXTERNAL

#endif // UNREAL4

void RegisterCoreClasses()
{
	BEGIN_CLASS_TABLE
		REGISTER_CLASS_EXTERNAL(FVector)
		REGISTER_CLASS_EXTERNAL(FQuat)
		REGISTER_CLASS_EXTERNAL(FRotator)
		REGISTER_CLASS_EXTERNAL(FColor)
	#if UNREAL3
		REGISTER_CLASS_EXTERNAL(FLinearColor)
		REGISTER_CLASS_EXTERNAL(FBoxSphereBounds)
	#endif
	#if UNREAL4
		REGISTER_CLASS_EXTERNAL(FRotator4)
		REGISTER_CLASS_EXTERNAL(FTransform)
		REGISTER_CLASS_EXTERNAL(FIntPoint)
	#endif
	END_CLASS_TABLE
}
