#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


//#define DEBUG_PROPS			1
//#define PROFILE_LOADING		1
//#define DEBUG_TYPES				1

#define DUMP_SHOW_PROP_INDEX	0
#define DUMP_SHOW_PROP_TYPE		0


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

UObject::~UObject()
{
//	appPrintf("deleting %s\n", Name);
	// remove self from GObjObjects
	for (int i = 0; i < GObjObjects.Num(); i++)
		if (GObjObjects[i] == this)
		{
			GObjObjects.Remove(i);
			break;
		}
	// remove self from package export table
	// note: we using PackageIndex==INDEX_NONE when creating dummy object, not exported from
	// any package, but which still belongs to this package (for example check Rune's
	// USkelModel)
	if (Package && PackageIndex != INDEX_NONE)
	{
		assert(Package->ExportTable[PackageIndex].Object == this);
		Package->ExportTable[PackageIndex].Object = NULL;
		Package = NULL;
	}
}


const char *UObject::GetRealClassName() const
{
	if (!Package) return GetClassName();
	const FObjectExport &Exp = Package->GetExport(PackageIndex);
	return Package->GetObjectName(Exp.ClassIndex);
}


const char *UObject::GetUncookedPackageName() const
{
	if (!Package) return "None";
	return Package->GetUncookedPackageName(PackageIndex);
}


// Package.Group.Object
void UObject::GetFullName(char *buf, int bufSize, bool IncludeObjectName, bool IncludeCookedPackageName, bool ForcePackageName) const
{
	if (!Package)
	{
		buf[0] = 0;
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

//#if PROFILE_LOADING
//	appPrintProfiler();
//#endif
	guard(UObject::EndLoad);
	// process GObjLoaded array
	// NOTE: while loading one array element, array may grow!
	TArray<UObject*> LoadedObjects;
	while (GObjLoaded.Num())
	{
		UObject *Obj = GObjLoaded[0];
		GObjLoaded.Remove(0);
		//!! should sort by packages + package offset
		UnPackage *Package = Obj->Package;
		guard(LoadObject);
		Package->SetupReader(Obj->PackageIndex);
		appPrintf("Loading %s %s from package %s\n", Obj->GetClassName(), Obj->Name, Package->Filename);
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
		LoadedObjects.AddItem(Obj);

		unguardf(("%s'%s.%s', pos=%X, ver=%d/%d, game=%X", Obj->GetClassName(), Package->Name, Obj->Name, Package->Tell(), Package->ArVer, Package->ArLicenseeVer, Package->Game));
	}
	// postload objects
	int i;
	guard(PostLoad);
	for (i = 0; i < LoadedObjects.Num(); i++)
		LoadedObjects[i]->PostLoad();
	unguardf(("%s", LoadedObjects[i]->Name));
	// cleanup
	GObjLoaded.Empty();
	GObjBeginLoadCount--;		// decrement after loading
	assert(GObjBeginLoadCount == 0);
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
	if (Ar.ArVer >= 300)	// real version is unknown; native FLinearColor serializer does not work with EndWar
	{
		STRUC_TYPE(FLinearColor)
	}
	const CTypeInfo *ItemType = FindStructType(StrucName);
	if (!ItemType) return false;
	ItemType->SerializeProps(Ar, (byte*)Data + Index * ItemType->SizeOf);
	return true;
	unguardf(("%s", StrucName));
}


enum EPropType // hardcoded in Unreal
{
	NAME_ByteProperty = 1,
	NAME_IntProperty,
	NAME_BoolProperty,
	NAME_FloatProperty,
	NAME_ObjectProperty,
	NAME_NameProperty,
	NAME_StringProperty,		// missing in UE3
	NAME_ClassProperty,
	NAME_ArrayProperty,
	NAME_StructProperty,
	NAME_VectorProperty,
	NAME_RotatorProperty,
	NAME_StrProperty,
	NAME_MapProperty,
	NAME_FixedArrayProperty,	// missing in UE3
#if UNREAL3
	NAME_DelegateProperty,		// equals to NAME_StringProperty = 7
	NAME_InterfaceProperty		// euqals to NAME_FixedArrayProperty = 15
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
	F(InterfaceProperty)
#endif
#undef F
};

static int MapTypeName(const char *Name)
{
	guard(MapTypeName);
	for (int i = 0; i < ARRAY_COUNT(NameToIndex); i++)
		if (!strcmp(Name, NameToIndex[i].Name))
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
				Ar << Object;
				if (!Object)
					Tag.Name.Str = "None";
					return Ar;
				// now, should continue serialization, skipping Name serialization
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

#if UNREAL3
		if (Ar.Game >= GAME_UE3)
		{
			FName PropType;
			Ar << PropType << Tag.DataSize << Tag.ArrayIndex;
			Tag.Type = MapTypeName(PropType);
			if (Tag.Type == NAME_StructProperty)
				Ar << Tag.StrucName;
			if (Tag.Type == NAME_BoolProperty)
			{
				if (Ar.ArVer < 673)
					Ar << Tag.BoolValue;			// int
				else
					Ar << (byte&)Tag.BoolValue;		// byte
			}
			if (Tag.Type == NAME_ByteProperty && Ar.ArVer >= 633)
				Ar << Tag.EnumName;
			return Ar;
		}
#endif // UNREAL3

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
		case 6: Ar << *((word*)&Tag.DataSize); break;
		case 7: Ar << *((int *)&Tag.DataSize); break;
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


void UObject::Serialize(FArchive &Ar)
{
	guard(UObject::Serialize);
	// stack frame
//	assert(!(ObjectFlags & RF_HasStack));

#if TRIBES3
	TRIBES_HDR(Ar, 0x19);
#endif

#if UNREAL3

#	if WHEELMAN || MKVSDC
	if ( (Ar.Game == GAME_Wheelman && Ar.ArVer >= 385) ||
		 (Ar.Game == GAME_MK && Ar.ArVer >= 446) )
		goto no_net_index;
#	endif
	if (Ar.ArVer >= 322)
		Ar << NetIndex;
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

	unguard;
}


void CTypeInfo::SerializeProps(FArchive &Ar, void *ObjectData) const
{
	guard(CTypeInfo::SerializeProps);

	// property list
	while (true)
	{
		FPropertyTag Tag;
		Ar << Tag;
		if (!Tag.IsValid())
			break;

		guard(ReadProperty);

		int StopPos = Ar.Tell() + Tag.DataSize;	// for verification

		const CPropInfo *Prop = FindProperty(Tag.Name);
		if (!Prop || !Prop->TypeName)	// Prop->TypeName==NULL when declared with PROP_DROP() macro
		{
			if (!Prop)
				appPrintf("WARNING: %s \"%s::%s\" was not found\n", GetTypeName(Tag.Type), Name, *Tag.Name);
#if DEBUG_PROPS
			appPrintf("  (skipping %s)\n", *Tag.Name);
#endif
			// skip property data
			Ar.Seek(StopPos);
			// serialize other properties
			continue;
		}
		// verify array index
		if (Tag.Type == NAME_ArrayProperty)
		{
			if (!(Prop->Count == -1 && Tag.ArrayIndex == 0))
				appError("Struct \"%s\": %s %s has Prop.Count=%d (should be -1) and ArrayIndex=%d (should be 0)",
					Name, Prop->TypeName, Prop->Name, Prop->Count, Tag.ArrayIndex);
		}
		else if (Tag.ArrayIndex >= Prop->Count)
			appError("Struct \"%s\": %s %s[%d]: serializing index %d",
				Name, Prop->TypeName, Prop->Name, Prop->Count, Tag.ArrayIndex);
		byte *value = (byte*)ObjectData + Prop->Offset;

#define TYPE(name) \
	if (strcmp(Prop->TypeName, name)) \
		appError("Property %s expected type %s but read %s", *Tag.Name, Prop->TypeName, name)

#define PROP(type)		( ((type*)value)[ArrayIndex] )

#if DEBUG_PROPS
#	define PROP_DBG(fmt, value) \
		appPrintf("  %s[%d] = " fmt "\n", *Tag.Name, Tag.ArrayIndex, value);
#else
#	define PROP_DBG(fmt, value)
#endif

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
					TYPE("byte");
					appNotify("EnumProp: %s = %s\n", *Tag.Name, *EnumValue);
//					PROP_DBG("%s", PROP(byte));
				}
			}
			else
#endif
			{
				if (Prop->TypeName[0] != '#')
				{
					TYPE("byte");
				}
				Ar << PROP(byte);
				PROP_DBG("%d", PROP(byte));
			}
			break;

		case NAME_IntProperty:
			TYPE("int");
			Ar << PROP(int);
			PROP_DBG("%d", PROP(int));
			break;

		case NAME_BoolProperty:
			TYPE("bool");
			PROP(bool) = Tag.BoolValue != 0;
			PROP_DBG("%s", PROP(bool) ? "true" : "false");
			break;

		case NAME_FloatProperty:
			TYPE("float");
			Ar << PROP(float);
			PROP_DBG("%g", PROP(float));
			break;

		case NAME_ObjectProperty:
			TYPE("UObject*");
			Ar << PROP(UObject*);
			PROP_DBG("%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
			break;

		case NAME_NameProperty:
			TYPE("FName");
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
					if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) Ar << DataCount;
					else
#endif
#if UNREAL3
					if (Ar.Engine() >= GAME_UE3) Ar << DataCount;
					else
#endif
						Ar << AR_INDEX(DataCount);
					//!! note: some structures should be serialized using SerializeStruc() (FVector etc)
					// find data typeinfo
					const CTypeInfo *ItemType = FindStructType(Prop->TypeName);
					if (!ItemType)
						appError("Unknown structure type %s", Prop->TypeName);
					// prepare array
					Arr->Empty(DataCount, ItemType->SizeOf);
					Arr->Add(DataCount, ItemType->SizeOf);
					// serialize items
					byte *item = (byte*)Arr->GetData();
					for (int i = 0; i < DataCount; i++, item += ItemType->SizeOf)
					{
#if DEBUG_PROPS
						appPrintf("Item[%d]:\n", i);
#endif
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
				if (strcmp(Prop->TypeName+1, *Tag.StrucName))
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
			appError("String property not implemented");
			break;

		case NAME_MapProperty:
			appError("Map property not implemented");
			break;

		case NAME_FixedArrayProperty:
			appError("FixedArray property not implemented");
			break;

		// reserved, but not implemented in unreal:
		case NAME_StringProperty:	//------  string  => used str
		case NAME_VectorProperty:	//------  vector  => used structure"Vector"
		case NAME_RotatorProperty:	//------  rotator => used structure"Rotator"
			appError("Unknown property");
			break;
		}
		if (Ar.Tell() != StopPos) appError("%s\'%s\'.%s: Pos-StopPos = %d", Name, Name, *Tag.Name, Ar.Tell() - StopPos);

		unguardf(("(%s.%s)", Name, *Tag.Name));
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	RTTI support
-----------------------------------------------------------------------------*/

#define MAX_CLASSES		256

static CClassInfo GClasses[MAX_CLASSES];
static int        GClassCount = 0;

void RegisterClasses(CClassInfo *Table, int Count)
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
#if DEBUG_TYPES
	appPrintf("--- find %s %s ... ", ClassType ? "class" : "struct", Name);
#endif
	guard(CreateClass);
	for (int i = 0; i < GClassCount; i++)
	{
		// skip 1st char only for ClassType==true?
		if (ClassType)
		{
			if (strcmp(GClasses[i].Name + 1, Name) != 0) continue;
		}
		else
		{
			if (strcmp(GClasses[i].Name, Name) != 0) continue;
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
	unguardf(("%s", Name));
}


UObject *CreateClass(const char *Name)
{
	guard(CreateClass);

	const CTypeInfo *Type = FindClassType(Name);
	if (!Type) return NULL;

	UObject *Obj = (UObject*)appMalloc(Type->SizeOf);
	Type->Constructor(Obj);
	// NOTE: do not add object to GObjObjects in UObject constructor
	// to allow runtime creation of objects without linked package
	// Really, should add to this list after loading from package
	// (in CreateExport/Import or after serialization)
	UObject::GObjObjects.AddItem(Obj);
	return Obj;

	unguardf(("%s", Name));
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
		if (!strcmp(p.ClassName, this->Name) && !strcmp(p.OldName, Name))
		{
			Name = p.NewName;
			break;
		}
	}
	// find property
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
	{
		for (i = 0; i < Type->NumProps; i++)
			if (!(strcmp(Type->Props[i].Name, Name)))
				return Type->Props + i;
	}
	return NULL;
	unguard;
}


static void PrintIndent(int Value, bool HasIndex = false)
{
#if DUMP_SHOW_PROP_INDEX
	if (!HasIndex) appPrintf("  .....");	// skip "[NNN]"
#endif
	for (int i = 0; i <= Value; i++)
		appPrintf("    ");
}

void CTypeInfo::DumpProps(void *Data, int Indent) const
{
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
	{
		if (!Type->NumProps) continue;
		if (!Indent) appPrintf("%s properties:\n", Type->Name);
		for (int PropIndex = 0; PropIndex < Type->NumProps; PropIndex++)
		{
			const CPropInfo *Prop = Type->Props + PropIndex;
			if (!Prop->TypeName)
			{
//				appPrintf("  %3d: (dummy) %s\n", PropIndex, Prop->Name);
				continue;
			}
#if DUMP_SHOW_PROP_INDEX
			appPrintf("  [%3d]", PropIndex);
			PrintIndent(Indent, true);
#else
			PrintIndent(Indent);
#endif
#if DUMP_SHOW_PROP_TYPE
			appPrintf("%s ", (Prop->TypeName[0] != '#') ? Prop->TypeName : Prop->TypeName+1);	// skip enum marker
#endif
			appPrintf("%s", Prop->Name);

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
				appPrintf("[%d] =", PropCount);
				if (!PropCount)
				{
					appPrintf(" {}\n");
					continue;
				}
				// first item
				if (!IsStruc)
				{
					appPrintf(" { ");
				}
				else
				{
					appPrintf("\n");
					PrintIndent(Indent); appPrintf("{");
				}
			}
			else
			{
				appPrintf(" = ");
			}

			// dump item(s)
			for (int ArrayIndex = 0; ArrayIndex < PropCount; ArrayIndex++)
			{
				// note: ArrayIndex is used inside PROP macro

				// print separator between array items
				if (ArrayIndex > 0 && !IsStruc) appPrintf(", ");

#define IS(name)  strcmp(Prop->TypeName, #name) == 0
#define PROCESS(type, format, value) \
				if (IS(type)) { appPrintf(format, value); }
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
						appPrintf("%s'%s'", obj->GetClassName(), ObjName);
					}
					else
						appPrintf("Null");
				}
#else
				PROCESS(UObject*, "%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
#endif
				PROCESS(FName,    "%s", *PROP(FName));
				if (Prop->TypeName[0] == '#')
				{
					// enum value
					const char *v = EnumToName(Prop->TypeName+1, *value);		// skip enum marker
					appPrintf("%s (%d)", v ? v : "<unknown>", *value);
				}
				if (IsStruc)
				{
					// this is a structure type
					int NestIndent = (IsArray) ? Indent+1 : Indent;
					if (ArrayIndex == 0) appPrintf("\n");
					if (IsArray)
					{
						PrintIndent(NestIndent);
						appPrintf("%s[%d] =\n", Prop->Name, ArrayIndex);
					}
					PrintIndent(NestIndent); appPrintf("{\n");
					StrucType->DumpProps(value + ArrayIndex * StrucType->SizeOf, NestIndent+1);
					PrintIndent(NestIndent); appPrintf("}\n");
				}
			}

			// formatting of property end
			if (IsArray)
			{
				if (!IsStruc)
				{
					appPrintf(" }\n");		// no indent
				}
				else
				{
					PrintIndent(Indent); appPrintf("}\n");	// indent
				}
			}
			else if (!IsStruc)
			{
				appPrintf("\n");
			}
		}
	}
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
