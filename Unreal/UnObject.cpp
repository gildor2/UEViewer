#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"

#include "GameDatabase.h"		// for GetGameTag()


//#define DEBUG_PROPS				1
//#define PROFILE_LOADING			1
//#define DEBUG_TYPES				1

#define DUMP_SHOW_PROP_INDEX	0
#define DUMP_SHOW_PROP_TYPE		0


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

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
	return Package->GetObjectName(Exp.ClassIndex);
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
		appStrncpyz(buf, Package->Filename, bufSize);
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

	// process GObjLoaded array
	// NOTE: while loading one array element, array may grow!
	TArray<UObject*> LoadedObjects;
	while (GObjLoaded.Num())
	{
		UObject *Obj = GObjLoaded[0];
		GObjLoaded.RemoveAt(0);
		//!! should sort by packages + package offset
		UnPackage *Package = Obj->Package;
		guard(LoadObject);
		Package->SetupReader(Obj->PackageIndex);
		appPrintf("Loading %s %s from package %s\n", Obj->GetClassName(), Obj->Name, Package->Filename);
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
#else
	#define UNVERS_STR		""
#endif

		unguardf("%s'%s.%s', pos=%X, ver=%d/%d%s, game=%s", Obj->GetClassName(), Package->Name, Obj->Name, Package->Tell(),
			Package->ArVer, Package->ArLicenseeVer, UNVERS_STR, GetGameTag(Package->Game));
	}
	// postload objects
	int i;
	guard(PostLoad);
	for (i = 0; i < LoadedObjects.Num(); i++)
		LoadedObjects[i]->PostLoad();
	unguardf("%s", LoadedObjects[i]->Name);
	// cleanup
	GObjLoaded.Empty();
	GObjBeginLoadCount--;		// decrement after loading
	appSetNotifyHeader(NULL);
	assert(GObjBeginLoadCount == 0);

	// close all opened file handles
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
#define STRUC_TYPE(name)				\
	if (!strcmp(StrucName, #name))		\
	{									\
		Ar << ((name*)Data)[Index];		\
		return true;					\
	}
	STRUC_TYPE(FVector)
	STRUC_TYPE(FRotator)
	STRUC_TYPE(FColor)
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		STRUC_TYPE(FBoxSphereBounds)
	}
#endif // MKVSDC
	if (Ar.ArVer >= 300)	// real version is unknown; native FLinearColor serializer does not work with EndWar
	{
		STRUC_TYPE(FLinearColor)
	}
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		/// reference: CoreUObject/Classes/Object.h
		/// objects with native serializer has "atomic" or "immutable" mark
		STRUC_TYPE(FIntPoint)
		STRUC_TYPE(FLinearColor)
	}
#endif // UNREAL4
	// Serialize nested property block
	const CTypeInfo *ItemType = FindStructType(StrucName);
	if (!ItemType) return false;
	ItemType->SerializeProps(Ar, (byte*)Data + Index * ItemType->SizeOf);
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
	NAME_AssetObjectProperty = 19,
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
	/// reference: CoreUObject/Public/UObject/UnrealNames.inl
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
	F(AssetObjectProperty),
	F(AssetSubclassProperty),
	F(EnumProperty),
#endif
#undef F
};

static int MapTypeName(const char *Name)
{
	guard(MapTypeName);
	for (int i = 0; i < ARRAY_COUNT(NameToIndex); i++)
		if (!stricmp(Name, NameToIndex[i].Name))
			return NameToIndex[i].Index;
	appError("MapTypeName: unknown type '%s'", Name);
	return 0;
	unguard;
}

static const char* GetTypeName(int Index)
{
	guard(GetTypeName);
	for (int i = 0; i < ARRAY_COUNT(NameToIndex); i++)
		if (Index == NameToIndex[i].Index)
			return NameToIndex[i].Name;
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
				// This code was bever executed in my tests
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
			else if (Tag.Type == NAME_ByteProperty)
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
				Ar << Tag.EnumName;
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
		bool OurClass = StrucType->IsA(p->Name);

		while (++p < end && p->Name)
		{
			if (!OurClass) continue;

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

	Type->SerializeProps(Ar, this);

#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		int bSerializeGuid;
		Ar << bSerializeGuid;
		if (bSerializeGuid)
		{
			FGuid guid;
			Ar << guid;
		}
	}
#endif // UNREAL4

	unguard;
}


void CTypeInfo::SerializeProps(FArchive &Ar, void *ObjectData) const
{
	guard(CTypeInfo::SerializeProps);

#define PROP(type)		( ((type*)value)[ArrayIndex] )

#if DEBUG_PROPS
#	define PROP_DBG(fmt, ...) \
		appPrintf("  %s[%d] = " fmt "\n", *Tag.Name, Tag.ArrayIndex, __VA_ARGS__);
#else
#	define PROP_DBG(fmt, ...)
#endif

	int PropTagPos;

	// property list
	while (true)
	{
		FPropertyTag Tag;
		PropTagPos = Ar.Tell();

#if BATMAN
		//!! try to move to separate function
		if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4)
		{
			// Batman 2 has more compact FPropertyTag implementation which uses serialization
			// by offset, not by name - this should work faster (in game) and use less disk space.
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
				// Property has extra information, it should serialized as in original engine.
				// Convert tag to the standard format.
				Tag.Name       = TagBat.PropertyName;
				Tag.Type       = TagBat.Type;
				Tag.DataSize   = TagBat.DataSize;
				Tag.ArrayIndex = TagBat.ArrayIndex;
				Tag.BoolValue  = TagBat.BoolValue;
				goto read_property;
			}
			else
			{
				static byte Dummy[16];
				byte *value = Dummy;
				const int ArrayIndex = 0; // used in PROP macro
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
//						appPrintf("... value = %s\n", *s);
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
					PROP_DBG("%g %g %g", FVECTOR_ARG(PROP(FVector)));
					break;

				case NAME_RotatorProperty:
					Ar << PROP(FRotator);
					PROP_DBG("%d %d %d", FROTATOR_ARG(PROP(FRotator)));
					break;

				case 17: // NAME_GuidProperty
					Ar << PROP(FGuid);
					PROP_DBG("%s", "(guid)");
					break;
				}
			}
			continue;
		}
#endif // BATMAN

		Ar << Tag;
		if (!Tag.IsValid())						// end marker
			break;

	read_property:
		guard(ReadProperty);

		const char *TypeName = "??";
	#if BATMAN
		if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4) // copy-paste of code above
		{
			if (Tag.Type < 16)
				TypeName = GetTypeName(Tag.Type);
			else if (Tag.Type == 16)
				TypeName = "ObjectNCRProperty";
			else if (Tag.Type == 17)
				TypeName = "GuidProperty";
		}
		else
	#endif // BATMAN
		{
			TypeName = GetTypeName(Tag.Type);
		}

#if DEBUG_PROPS
		appPrintf("-- %s \"%s::%s\" [%d] size=%d enum=%s struc=%s\n", TypeName, Name, *Tag.Name,
			Tag.ArrayIndex, Tag.DataSize, *Tag.EnumName, *Tag.StrucName);
#endif // DEBUG_PROPS

		int StopPos = Ar.Tell() + Tag.DataSize;	// for verification

		const CPropInfo *Prop = FindProperty(Tag.Name);
		if (!Prop || !Prop->TypeName)	// Prop->TypeName==NULL when declared with PROP_DROP() macro
		{
			if (!Prop)
				appPrintf("WARNING: %s \"%s::%s\" was not found\n", TypeName, Name, *Tag.Name);
#if DEBUG_PROPS
			appPrintf("  (skipping %s)\n", *Tag.Name);
#endif
		skip_property:
			if (Tag.Type == NAME_BoolProperty && Tag.DataSize != 0)
			{
				// BoolProperty has everything in FPropertyTag, but sometimes it has DataSize==4, what
				// causes error when trying to skip it. This is why we have a separate code here.
				appPrintf("WARNING: skipping bool property %s with Tag.Size=%d\n", Prop->Name, Tag.DataSize);
				continue;
			}
			// skip property data
			Ar.Seek(StopPos);
			// serialize other properties
			continue;
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

#define CHECK_TYPE(name) \
	if (strcmp(Prop->TypeName, name)) \
		appError("Property %s expected type %s but read %s", *Tag.Name, name, Prop->TypeName)

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
					CHECK_TYPE("byte");
					appNotify("EnumProp: %s = %s\n", *Tag.Name, *EnumValue);
//					PROP_DBG("%s", PROP(byte));
				}
			}
			else
#endif // UNREAL3
			{
				if (Prop->TypeName[0] != '#')
				{
					CHECK_TYPE("byte");
				}
				Ar << PROP(byte);
				PROP_DBG("%d", PROP(byte));
			}
			break;

		case NAME_IntProperty:
			CHECK_TYPE("int");
			Ar << PROP(int);
			PROP_DBG("%d", PROP(int));
			break;

		case NAME_BoolProperty:
			CHECK_TYPE("bool");
			PROP(bool) = Tag.BoolValue != 0;
			PROP_DBG("%s", PROP(bool) ? "true" : "false");
			break;

		case NAME_FloatProperty:
			CHECK_TYPE("float");
			Ar << PROP(float);
			PROP_DBG("%g", PROP(float));
			break;

		case NAME_ObjectProperty:
			CHECK_TYPE("UObject*");
			Ar << PROP(UObject*);
			PROP_DBG("%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
			break;

		case NAME_NameProperty:
			CHECK_TYPE("FName");
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
#define SIMPLE_ARRAY_TYPE(type) \
		if (!strcmp(Prop->TypeName, #type)) { Ar << *(TArray<type>*)Arr; }
				SIMPLE_ARRAY_TYPE(int)
				else SIMPLE_ARRAY_TYPE(bool)
				else SIMPLE_ARRAY_TYPE(byte)
				else SIMPLE_ARRAY_TYPE(float)
				else SIMPLE_ARRAY_TYPE(UObject*)
				else SIMPLE_ARRAY_TYPE(FName)
				else SIMPLE_ARRAY_TYPE(FVector)
				else SIMPLE_ARRAY_TYPE(FQuat)
#undef SIMPLE_ARRAY_TYPE
				else
				{
					// read data count
					int DataCount;
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
					//!! note: some structures should be serialized using SerializeStruc() (FVector etc)
					// find data typeinfo
					const CTypeInfo *ItemType = FindStructType(Prop->TypeName);
					if (!ItemType)
						appError("Unknown structure type %s", Prop->TypeName);
					// prepare array
					Arr->Empty(DataCount, ItemType->SizeOf);
					Arr->InsertZeroed(0, DataCount, ItemType->SizeOf);
					// serialize items
					byte *item = (byte*)Arr->GetData();
					for (int i = 0; i < DataCount; i++, item += ItemType->SizeOf)
					{
#if DEBUG_PROPS
						appPrintf("Item[%d]:\n", i);
#endif
						assert(ItemType->Constructor);
						ItemType->Constructor(item);		// fill default properties
						ItemType->SerializeProps(Ar, item);
					}
#if 1
					// fix for strange (UE?) bug - array[1] of empty structure has 1 extra byte
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
#endif // 1 -- fix
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
					//!! implement this (use FindStructType()->SerializeProps())
					appNotify("WARNING: Unknown structure type: %s", *Tag.StrucName);
					Ar.Seek(StopPos);
				}
			}
			break;

		case NAME_StrProperty:
			if (!stricmp(Prop->TypeName, "FName"))
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
				CHECK_TYPE("FString");
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
				// Possible bug: EnumProperty has DataSize=8, but really serializes nothing
				//http://www.gildor.org/smf/index.php/topic,6036.0.html
				StopPos = Ar.Tell();
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
#endif
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

		unguardf("(%s.%s, TagPos=%X)", Name, *Tag.Name, PropTagPos);
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	RTTI support
-----------------------------------------------------------------------------*/

#define MAX_CLASSES		256

static CClassInfo GClasses[MAX_CLASSES];
static int        GClassCount = 0;

void RegisterClasses(const CClassInfo *Table, int Count)
{
	if (Count <= 0) return;
	assert(GClassCount + Count < ARRAY_COUNT(GClasses));
	memcpy(GClasses + GClassCount, Table, Count * sizeof(GClasses[0]));
	GClassCount += Count;
#if DEBUG_TYPES
	appPrintf("*** Register: %d classes ***\n", Count);
	for (int i = GClassCount - Count; i < GClassCount; i++)
		appPrintf("[%d]:%s\n", i, GClasses[i].TypeInfo()->Name);
#endif
}


// may be useful
void UnregisterClass(const char *Name, bool WholeTree)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name + 1, Name) ||
			(WholeTree && (GClasses[i].TypeInfo()->IsA(Name))))
		{
#if DEBUG_TYPES
			appPrintf("Unregister %s\n", GClasses[i].Name);
#endif
			// class was found
			if (i == GClassCount-1)
			{
				// last table entry
				GClassCount--;
				return;
			}
			memcpy(GClasses+i, GClasses+i+1, (GClassCount-i-1) * sizeof(GClasses[0]));
			GClassCount--;
			i--;
		}
}


const CTypeInfo *FindClassType(const char *Name, bool ClassType)
{
	guard(FindClassType);
#if DEBUG_TYPES
	appPrintf("--- find %s %s ... ", ClassType ? "class" : "struct", Name);
#endif
	for (int i = 0; i < GClassCount; i++)
	{
		// skip 1st char only for ClassType==true?
		if (ClassType)
		{
			if (stricmp(GClasses[i].Name + 1, Name) != 0) continue;
		}
		else
		{
			if (stricmp(GClasses[i].Name, Name) != 0) continue;
		}

		if (!GClasses[i].TypeInfo) appError("No typeinfo for class");
		const CTypeInfo *Type = GClasses[i].TypeInfo();
		if (Type->IsClass() != ClassType) continue;
#if DEBUG_TYPES
		appPrintf("ok %s\n", Type->Name);
#endif
		return Type;
	}
#if DEBUG_TYPES
	appPrintf("failed!\n");
#endif
	return NULL;
	unguardf("%s", Name);
}


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


bool CTypeInfo::IsA(const char *TypeName) const
{
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
		if (!strcmp(TypeName, Type->Name + 1))
			return true;
	return false;
}


struct PropPatch
{
	const char *ClassName;
	const char *OldName;
	const char *NewName;
};

static TArray<PropPatch> Patches;

const CPropInfo *CTypeInfo::FindProperty(const char *Name) const
{
	guard(CTypeInfo::FindProperty);
	int i;
	// check for remap
	for (i = 0; i < Patches.Num(); i++)
	{
		const PropPatch &p = Patches[i];
		if (!stricmp(p.ClassName, this->Name) && !stricmp(p.OldName, Name))
		{
			Name = p.NewName;
			break;
		}
	}
	// find property
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
	{
		for (i = 0; i < Type->NumProps; i++)
			if (!(stricmp(Type->Props[i].Name, Name)))
				return Type->Props + i;
	}
	return NULL;
	unguard;
}


static void PrintIndent(int Value)
{
	for (int i = 0; i < Value; i++)
		appPrintf("    ");
}


struct CPropDump
{
	char				Name[64];
	char				Value[256];
	bool				IsArrayItem;
	TArray<CPropDump>	Nested;				// Value should be "" when Nested[] is not empty

	CPropDump()
	{
		memset(this, 0, sizeof(CPropDump));
	}

	void PrintTo(char *Dst, int DstSize, const char *fmt, va_list argptr)
	{
		int oldLen = strlen(Dst);
		int len = vsnprintf(Dst + oldLen, DstSize - oldLen, fmt, argptr);
		if (len < 0 || (len + oldLen >= DstSize))
		{
			// overflow
			memcpy(Dst + DstSize - 4, "...", 4);
		}
	}

	void PrintName(const char *fmt, ...)
	{
		va_list	argptr;
		va_start(argptr, fmt);
		PrintTo(ARRAY_ARG(Name), fmt, argptr);
		va_end(argptr);
	}

	void PrintValue(const char *fmt, ...)
	{
		va_list	argptr;
		va_start(argptr, fmt);
		PrintTo(ARRAY_ARG(Value), fmt, argptr);
		va_end(argptr);
	}
};


static void CollectProps(const CTypeInfo *Type, void *Data, CPropDump &Dump)
{
	for (/* empty */; Type; Type = Type->Parent)
	{
		if (!Type->NumProps) continue;

		for (int PropIndex = 0; PropIndex < Type->NumProps; PropIndex++)
		{
			const CPropInfo *Prop = Type->Props + PropIndex;
			if (!Prop->TypeName)
			{
//				appPrintf("  %3d: (dummy) %s\n", PropIndex, Prop->Name);
				continue;
			}
			CPropDump *PD = new (Dump.Nested) CPropDump;

			// name

#if DUMP_SHOW_PROP_INDEX
			PD->PrintName("(%d)", PropIndex);
#endif
#if DUMP_SHOW_PROP_TYPE
			PD->PrintName("%s ", (Prop->TypeName[0] != '#') ? Prop->TypeName : Prop->TypeName+1);	// skip enum marker
#endif
			PD->PrintName("%s", Prop->Name);

			// value

			byte *value = (byte*)Data + Prop->Offset;
			int PropCount = Prop->Count;

			bool IsArray = (PropCount > 1) || (PropCount == -1);
			if (PropCount == -1)
			{
				// TArray<> value
				FArray *Arr = (FArray*)value;
				value     = (byte*)Arr->GetData();
				PropCount = Arr->Num();
			}

			// find structure type
			const CTypeInfo *StrucType = FindStructType(Prop->TypeName);
			bool IsStruc = (StrucType != NULL);

			// formatting of property start
			if (IsArray)
			{
				PD->PrintName("[%d]", PropCount);
				if (!PropCount)
				{
					PD->PrintValue("{}");
					continue;
				}
			}

			// dump item(s)
			for (int ArrayIndex = 0; ArrayIndex < PropCount; ArrayIndex++)
			{
				CPropDump *PD2 = PD;
				if (IsArray)
				{
					// create nested CPropDump
					PD2 = new (PD->Nested) CPropDump;
					PD2->PrintName("%s[%d]", Prop->Name, ArrayIndex);
					PD2->IsArrayItem = true;
				}

				// note: ArrayIndex is used inside PROP macro

#define IS(name)  strcmp(Prop->TypeName, #name) == 0
#define PROCESS(type, format, value) \
				if (IS(type)) { PD2->PrintValue(format, value); }
				PROCESS(byte,     "%d", PROP(byte));
				PROCESS(int,      "%d", PROP(int));
				PROCESS(bool,     "%s", PROP(bool) ? "true" : "false");
				PROCESS(float,    "%g", PROP(float));
#if 1
				if (IS(UObject*))
				{
					UObject *obj = PROP(UObject*);
					if (obj)
					{
						char ObjName[256];
						obj->GetFullName(ARRAY_ARG(ObjName));
						PD2->PrintValue("%s'%s'", obj->GetClassName(), ObjName);
					}
					else
						PD2->PrintValue("None");
				}
#else
				PROCESS(UObject*, "%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
#endif
				PROCESS(FName,    "%s", *PROP(FName));
				if (Prop->TypeName[0] == '#')
				{
					// enum value
					const char *v = EnumToName(Prop->TypeName+1, *value);		// skip enum marker
					PD2->PrintValue("%s (%d)", v ? v : "<unknown>", *value);
				}
				if (IsStruc)
				{
					// this is a structure type
					CollectProps(StrucType, value + ArrayIndex * StrucType->SizeOf, *PD2);
				}
			} // ArrayIndex loop
		} // PropIndex loop
	} // Type->Parent loop
}


static void PrintProps(const CPropDump &Dump, int Indent)
{
	PrintIndent(Indent);

	int NumNestedProps = Dump.Nested.Num();
	if (NumNestedProps)
	{
		// complex property
		if (Dump.Name[0]) appPrintf("%s =", Dump.Name);	// root CPropDump will not have a name

		bool IsSimple = true;
		int TotalLen = 0;
		int i;

		// check whether we can display all nested properties in a single line or not
		for (i = 0; i < NumNestedProps; i++)
		{
			const CPropDump &Prop = Dump.Nested[i];
			if (Prop.Nested.Num())
			{
				IsSimple = false;
				break;
			}
			TotalLen += strlen(Prop.Value) + 2;
			if (!Prop.IsArrayItem)
				TotalLen += strlen(Prop.Name);
			if (TotalLen >= 80)
			{
				IsSimple = false;
				break;
			}
		}

		if (IsSimple)
		{
			// single-line value display
			appPrintf(" { ");
			for (i = 0; i < NumNestedProps; i++)
			{
				if (i) appPrintf(", ");
				const CPropDump &Prop = Dump.Nested[i];
				if (Prop.IsArrayItem)
					appPrintf("%s", Prop.Value);
				else
					appPrintf("%s=%s", Prop.Name, Prop.Value);
			}
			appPrintf(" }\n");
		}
		else
		{
			// complex value display
			appPrintf("\n");
			if (Indent > 0)
			{
				PrintIndent(Indent);
				appPrintf("{\n");
			}

			for (i = 0; i < NumNestedProps; i++)
				PrintProps(Dump.Nested[i], Indent+1);

			if (Indent > 0)
			{
				PrintIndent(Indent);
				appPrintf("}\n");
			}
		}
	}
	else
	{
		// single property
		if (Dump.Name[0]) appPrintf("%s = %s\n", Dump.Name, Dump.Value);
	}
}


void CTypeInfo::DumpProps(void *Data) const
{
	guard(CTypeInfo::DumpProps);
	CPropDump Dump;
	CollectProps(this, Data, Dump);
	PrintProps(Dump, 0);
	unguard;
}


void CTypeInfo::RemapProp(const char *ClassName, const char *OldName, const char *NewName) // static
{
	PropPatch *p = new (Patches) PropPatch;
	p->ClassName = ClassName;
	p->OldName   = OldName;
	p->NewName   = NewName;
}


#define MAX_ENUMS		32

struct enumInfo
{
	const char       *Name;
	const enumToStr  *Values;
	int              NumValues;
};

static enumInfo RegisteredEnums[MAX_ENUMS];
static int NumEnums = 0;

void RegisterEnum(const char *EnumName, const enumToStr *Values, int Count)
{
	guard(RegisterEnum);

	assert(NumEnums < MAX_ENUMS);
	enumInfo &Info = RegisteredEnums[NumEnums++];
	Info.Name      = EnumName;
	Info.Values    = Values;
	Info.NumValues = Count;

	unguard;
}

const enumInfo *FindEnum(const char *EnumName)
{
	for (int i = 0; i < NumEnums; i++)
		if (!strcmp(RegisteredEnums[i].Name, EnumName))
			return &RegisteredEnums[i];
	return NULL;
}

const char *EnumToName(const char *EnumName, int Value)
{
	const enumInfo *Info = FindEnum(EnumName);
	if (!Info) return NULL;				// enum was not found
	for (int i = 0; i < Info->NumValues; i++)
	{
		const enumToStr &V = Info->Values[i];
		if (V.value == Value)
			return V.name;
	}
	return NULL;						// no such value
}

int NameToEnum(const char *EnumName, const char *Value)
{
	const enumInfo *Info = FindEnum(EnumName);
	if (!Info) return ENUM_UNKNOWN;		// enum was not found
	for (int i = 0; i < Info->NumValues; i++)
	{
		const enumToStr &V = Info->Values[i];
		if (!stricmp(V.name, Value))
			return V.value;
	}
	return ENUM_UNKNOWN;				// no such value
}


/*-----------------------------------------------------------------------------
	Typeinfo for Core classes
-----------------------------------------------------------------------------*/

BEGIN_PROP_TABLE_EXTERNAL(FVector)
	PROP_FLOAT(X)
	PROP_FLOAT(Y)
	PROP_FLOAT(Z)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL(FRotator)
	PROP_INT(Yaw)
	PROP_INT(Pitch)
	PROP_INT(Roll)
END_PROP_TABLE_EXTERNAL

BEGIN_PROP_TABLE_EXTERNAL(FColor)
	PROP_BYTE(R)
	PROP_BYTE(G)
	PROP_BYTE(B)
	PROP_BYTE(A)
END_PROP_TABLE_EXTERNAL

#if UNREAL3

BEGIN_PROP_TABLE_EXTERNAL(FLinearColor)
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

BEGIN_PROP_TABLE_EXTERNAL(FIntPoint)
	PROP_INT(X)
	PROP_INT(Y)
END_PROP_TABLE_EXTERNAL

#endif // UNREAL4

void RegisterCoreClasses()
{
	BEGIN_CLASS_TABLE
		REGISTER_CLASS_EXTERNAL(FVector)
		REGISTER_CLASS_EXTERNAL(FRotator)
		REGISTER_CLASS_EXTERNAL(FColor)
	#if UNREAL3
		REGISTER_CLASS_EXTERNAL(FLinearColor)
		REGISTER_CLASS_EXTERNAL(FBoxSphereBounds)
	#endif
	#if UNREAL4
		REGISTER_CLASS_EXTERNAL(FIntPoint)
	#endif
	END_CLASS_TABLE
}
