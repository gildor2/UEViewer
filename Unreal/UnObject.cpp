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

		unguardf(("%s, pos=%X", Obj->Name, Package->Tell()));
	}
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
				appNotify("WARNING: %s \"%s::%s\" was not found", GetTypeName(Tag.Type), GetClassName(), *Tag.Name);
			// skip property data
			Ar.Seek(StopPos);
			// serialize other properties
			continue;
		}
		// verify array index
		if (Tag.ArrayIndex >= Prop->Count)
			appError("Class \"%s\": %s %s[%d]: serializing index %d",
				GetClassName(), Prop->TypeName, Prop->Name, Prop->Count, Tag.ArrayIndex);
		byte *value = (byte*)this + Prop->Offset;

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
			TYPE("byte");
#if UNREAL3
			if (Tag.DataSize != 1)
			{
				assert(Tag.DataSize == 8);
				FName EnumValue;
				Ar << EnumValue;
				//!! map string -> byte
				printf("EnumProp: %s = %s\n", *Tag.Name, *EnumValue);
			}
			else
#endif
				Ar << PROP(byte);
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
			appError("Class property not implemented");
			break;

		case NAME_ArrayProperty:
			appError("Array property not implemented");
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
		if (Ar.Tell() != StopPos) appError("%s\'%s\'.%s: Pos-StopPos = %d", GetClassName(), Name, *Tag.Name, Ar.Tell() - StopPos);

		unguardf(("(%s.%s)", GetClassName(), *Tag.Name));
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
void UnregisterClass(const char *Name)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
		{
			printf("Unregistered %s\n", Name);
			// class was found
			if (i == GClassCount-1)
			{
				// last table entry
				GClassCount--;
				return;
			}
			memcpy(GClasses+i, GClasses+i+1, GClassCount-i-1);
			GClassCount--;
		}
}


UObject *CreateClass(const char *Name)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
		{
			UObject *Obj = GClasses[i].Constructor();
			// NOTE: do not add object to GObjObjects in UObject constructor
			// to allow runtime creation of objects without linked package
			// Really, should add to this list after loading from package
			// (in CreateExport/Import or after serialization)
			UObject::GObjObjects.AddItem(Obj);
			return Obj;
		}
	return NULL;
}


bool IsKnownClass(const char *Name)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
			return true;
	return false;
}


const CPropInfo *UObject::FindProperty(const char *Name) const
{
	for (int index = 0; /*empty*/; index++)
	{
		const CPropInfo *info = EnumProps(index);
		if (!info) break;		// all props enumerated
		if (!strcmp(info->Name, Name))
			return info;
	}
	return NULL;
}


void UObject::DumpProps() const
{
	for (int PropIndex = 0; /*empty*/; PropIndex++)
	{
		const CPropInfo *Prop = EnumProps(PropIndex);
		if (!Prop) break;		// all props enumerated
		if (!Prop->TypeName)
		{
			printf("%3d: (dummy) %s\n", PropIndex, Prop->Name);
			continue;
		}
		printf("%3d: %s %s", PropIndex, Prop->TypeName, Prop->Name);
		if (Prop->Count > 1)
			printf("[%d] = { ", Prop->Count);
		else
			printf(" = ");

		byte *value = (byte*)this + Prop->Offset;

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
		}

		if (Prop->Count > 1)
			printf(" }\n");
		else
			printf("\n");
	}
}
