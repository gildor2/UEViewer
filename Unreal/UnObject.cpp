#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


//#define DEBUG_PROPS		1


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

UObject::~UObject()
{
//	printf("deleting %s\n", Name);
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


/*-----------------------------------------------------------------------------
	UObject loading from package
-----------------------------------------------------------------------------*/

int              UObject::GObjBeginLoadCount = 0;
TArray<UObject*> UObject::GObjLoaded;
TArray<UObject*> UObject::GObjObjects;


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
		GObjLoaded.Remove(0);
		//!! should sort by packages + package offset
		UnPackage *Package = Obj->Package;
		guard(LoadObject);
		Package->SetupReader(Obj->PackageIndex);
		printf("Loading %s %s from package %s\n", Obj->GetClassName(), Obj->Name, Package->Filename);
		Obj->Serialize(*Package);
		// check for unread bytes
		if (!Package->IsStopper())
			appError("%s::Serialize(%s): %d unread bytes",
				Obj->GetClassName(), Obj->Name,
				Package->GetStopper() - Package->Tell());
		LoadedObjects.AddItem(Obj);

		unguardf(("%s, pos=%X", Obj->Name, Package->Tell()));
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

static bool SerializeStruc(FArchive &Ar, void *Data, int Index, const char *StrucName)
{
	guard(SerializeStruc);
#define STRUC_TYPE(name)				\
	if (!strcmp(StrucName, #name))		\
	{									\
		Ar << ((F##name*)Data)[Index];	\
		return true;					\
	}
	STRUC_TYPE(Vector)
	STRUC_TYPE(Rotator)
	STRUC_TYPE(Color)
	return false;
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

	bool IsValid()
	{
		return strcmp(Name, "None") != 0;
	}

	friend FArchive& operator<<(FArchive &Ar, FPropertyTag &Tag)
	{
		guard(FPropertyTag<<);
		assert(Ar.IsLoading);		// saving is not supported

#if UC2
		if (Ar.ArVer >= 148 && Ar.ArVer < PACKAGE_V3)
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

		Ar << Tag.Name;
		if (!strcmp(Tag.Name, "None"))
			return Ar;

#if UNREAL3
		if (Ar.ArVer >= PACKAGE_V3)
		{
			FName PropType;
			Ar << PropType << Tag.DataSize << Tag.ArrayIndex;
			Tag.Type = MapTypeName(PropType);
			if (Tag.Type == NAME_StructProperty)
				Ar << Tag.StrucName;
			if (Tag.Type == NAME_BoolProperty)
				Ar << Tag.BoolValue;
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
	if (Ar.ArVer >= 322)
		Ar << NetIndex;
#endif

	const CTypeInfo *Type = GetTypeinfo();
	assert(Type);
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
				appNotify("WARNING: %s \"%s::%s\" was not found", GetTypeName(Tag.Type), Name, *Tag.Name);
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
		printf("  %s[%d] = " fmt "\n", *Tag.Name, Tag.ArrayIndex, value);
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
				assert(Tag.DataSize == 8);
				if (!strcmp(Prop->TypeName, "enum3")) //!! temp solution
				{
					TYPE("enum3");
					Ar << *((FName*)value);
				}
				else
				{
					TYPE("byte");
					FName EnumValue;
					Ar << EnumValue;
					//!! map string -> byte
					printf("EnumProp: %s = %s\n", *Tag.Name, *EnumValue);
				}
			}
			else if (!strcmp(Prop->TypeName, "enum3"))
			{
				// old XBox360 GoW: enums are byte
				byte b;
				Ar << b;
				printf("EnumProp: %s = %d\n", *Tag.Name, b);
			}
			else
#endif
			{
				TYPE("byte");
				Ar << PROP(byte);
			}
			PROP_DBG("%d", PROP(byte));
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
				FArray *Arr = (FArray*)value;
#define SIMPLE_ARRAY_TYPE(type) \
		if (!strcmp(Prop->TypeName, #type)) { Ar << *(TArray<type>*)Arr; }
				SIMPLE_ARRAY_TYPE(int)
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
#if UNREAL3
					if (Ar.ArVer >= 145) // PACKAGE_V3; UC2 ??
						Ar << DataCount;
					else
#endif
						Ar << AR_INDEX(DataCount);	//?? check - AR_INDEX or not
					//!! note: some structures should be serialized using SerializeStruc() (FVector etc)
					// find data typeinfo
					const CTypeInfo *ItemType = FindStructType(Prop->TypeName + 1);	//?? skip 'F' in name
					if (!ItemType)
						appError("Unknown structure type %s", Prop->TypeName);
					// prepare array
					Arr->Empty(DataCount, ItemType->SizeOf);
					Arr->Add(DataCount, ItemType->SizeOf);
					// serialize items
					byte *item = (byte*)Arr->GetData();
					for (int i = 0; i < DataCount; i++, item += ItemType->SizeOf)
					{
						ItemType->Constructor(item);		// fill default properties
						ItemType->SerializeProps(Ar, item);
					}
				}
			}
			break;

		case NAME_StructProperty:
			{
				if (strcmp(Prop->TypeName+1, *Tag.StrucName))
					appError("Struc property %s expected type %s but read %s", *Tag.Name, Prop->TypeName, *Tag.StrucName);
				if (SerializeStruc(Ar, value, Tag.ArrayIndex, Tag.StrucName))
				{
					PROP_DBG("(complex)", 0);
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

static CClassInfo* GClasses    = NULL;
static int         GClassCount = 0;

void RegisterClasses(CClassInfo *Table, int Count)
{
	assert(GClasses == NULL);		// no multiple tables
	GClasses    = Table;
	GClassCount = Count;
}


// may be useful
void UnregisterClass(const char *Name, bool WholeTree)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name) ||
			(WholeTree && (GClasses[i].TypeInfo()->IsA(Name))))
		{
			printf("Unregistered %s\n", GClasses[i].Name);
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
	guard(CreateClass);
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
		{
			if (!GClasses[i].TypeInfo) appError("No typeinfo for class");
			const CTypeInfo *Type = GClasses[i].TypeInfo();
			if (Type->IsClass() != ClassType) continue;
			return Type;
		}
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


bool IsKnownClass(const char *Name)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
			return true;
	return false;
}


bool CTypeInfo::IsA(const char *TypeName) const
{
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
		if (!strcmp(TypeName, Type->Name + 1))
			return true;
	return false;
}


const CPropInfo *CTypeInfo::FindProperty(const char *Name) const
{
	guard(CTypeInfo::FindProperty);
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
	{
		for (int i = 0; i < Type->NumProps; i++)
			if (!(strcmp(Type->Props[i].Name, Name)))
				return Type->Props + i;
	}
	return NULL;
	unguard;
}


void CTypeInfo::DumpProps(void *Data) const
{
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
	{
		if (!Type->NumProps) continue;
		printf("%s properties:\n", Type->Name);
		for (int PropIndex = 0; PropIndex < Type->NumProps; PropIndex++)
		{
			const CPropInfo *Prop = Type->Props + PropIndex;
			if (!Prop->TypeName)
			{
//				printf("  %3d: (dummy) %s\n", PropIndex, Prop->Name);
				continue;
			}
			printf("  %3d: %s %s", PropIndex, Prop->TypeName, Prop->Name);
			if (Prop->Count > 1)
				printf("[%d] = { ", Prop->Count);
			else
				printf(" = ");

			byte *value = (byte*)Data + Prop->Offset;

			//?? can support TArray props here (Prop->Count == -1)
			for (int ArrayIndex = 0; ArrayIndex < Prop->Count; ArrayIndex++)
			{
				if (ArrayIndex > 0) printf(", ");
#define IS(name)  strcmp(Prop->TypeName, #name) == 0
#define PROCESS(type, format, value) \
				if (IS(type)) { printf(format, value); }
				PROCESS(byte,     "%d", PROP(byte));
				PROCESS(int,      "%d", PROP(int));
				PROCESS(bool,     "%s", PROP(bool) ? "true" : "false");
				PROCESS(float,    "%g", PROP(float));
#if 1
				if (IS(UObject*))
				{
					UObject *obj = PROP(UObject*);
					if (obj)
						printf("%s'%s'", obj->GetClassName(), obj->Name);
					else
						printf("Null");
				}
#else
				PROCESS(UObject*, "%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
#endif
				PROCESS(FName,    "%s", *PROP(FName));
#if UNREAL3
				PROCESS(enum3,    "%s", *PROP(FName))
#endif
			}

			if (Prop->Count > 1)
				printf(" }\n");
			else
				printf("\n");
		}
	}
}
