#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


#define DEBUG_PROPS		1


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

UObject::~UObject()
{
//	printf("deleting %s\n", Name);
	// remove self from package export table
	if (Package)
	{
		assert(Package->ExportTable[PackageIndex].Object == this);
		Package->ExportTable[PackageIndex].Object = NULL;
		Package = NULL;
	}
}


int              UObject::GObjBeginLoadCount = 0;
TArray<UObject*> UObject::GObjLoaded;


void UObject::BeginLoad()
{
	assert(GObjBeginLoadCount >= 0);
	GObjBeginLoadCount++;
}


void UObject::EndLoad()
{
	assert(GObjBeginLoadCount > 0);
	if (--GObjBeginLoadCount > 0)
		return;

	guard(UObject::EndLoad);
	// process GObjLoaded array
	// NOTE: while loading one array element, array may grow!
	for (int i = 0; i < GObjLoaded.Num(); i++)
	{
		UObject *Obj = GObjLoaded[i];
		guard(LoadObject);
		//!! should sort by packages
		UnPackage *Package = Obj->Package;
		Package->SetupReader(Obj->PackageIndex);
		printf("Loading %s %s from package %s\n", Obj->GetClassName(), Obj->Name, Package->SelfName);
		Obj->Serialize(*Package);
		// check for unread bytes
		if (!Package->IsStopper())
			appError("%s: extra bytes", Obj->Name);

		unguardf(("%s", Obj->Name));
	}
	GObjLoaded.Empty();
	unguard;
}


static void SerializeStruc(FArchive &Ar, void *Data, const char *StrucName)
{
#define STRUC_TYPE(name)				\
	if (!strcmp(StrucName, #name))		\
	{									\
		Ar << *((F##name*)Data);		\
		return;							\
	}
	STRUC_TYPE(Vector)
	STRUC_TYPE(Rotator)
	appError("Unknown structure class: %s", StrucName);
}


void UObject::Serialize(FArchive &Ar)
{
	guard(UObject::Serialize);
	// stack frame
//	assert(!(ObjectFlags & RF_HasStack));

	// property list
	while (true)
	{
		FName PropName;
		Ar << PropName;
		if (!strcmp(PropName, "None"))
			break;

		guard(ReadProperty);

		byte info;
		Ar << info;
		bool IsArray  = (info & 0x80) != 0;
		byte PropType = info & 0xF;
		// analyze 'size' field
		int  Size = 0;
		switch ((info >> 4) & 7)
		{
		case 0: Size = 1; break;
		case 1: Size = 2; break;
		case 2: Size = 4; break;
		case 3: Size = 12; break;
		case 4: Size = 16; break;
		case 5: Ar << *((byte*)&Size); break;
		case 6: Ar << *((word*)&Size); break;
		case 7: Ar << *((int *)&Size); break;
		}

		if (PropType != 3)			// 'bool' type has separate meaning of 'array' flag
		{
			assert(!IsArray);		//!! implement
		}

		const CPropInfo *Prop = FindProperty(PropName);
		if (!Prop)
			appError("Class \"%s\": property \"%s\" was not found", GetClassName(), *PropName);
		byte *value = (byte*)this + Prop->Offset;

#define TYPE(name) \
	if (strcmp(Prop->TypeName, name)) \
		appError("Property %s expected type %s by has %s", *PropName, Prop->TypeName, name)

#define PROP(type)		( *((type*)&value) )

#if DEBUG_PROPS
#	define PROP_DBG(fmt, value) \
		printf("  %s = " fmt "\n", *PropName, value);
#else
#	define PROP_DBG(fmt, value)
#endif

		switch (PropType)
		{
		case 1:		//------  byte
			TYPE("byte");
			Ar << PROP(byte);
			PROP_DBG("%d", PROP(byte));
			break;

		case 2:		//------  int
			TYPE("int");
			Ar << PROP(int);
			PROP_DBG("%d", PROP(int));
			break;

		case 3:		//------  bool
			TYPE("bool");
			PROP(bool) = IsArray;
			PROP_DBG("%s", PROP(bool) ? "true" : "false");
			break;

		case 4:		//------  float
			TYPE("float");
			Ar << PROP(float);
			PROP_DBG("%g", PROP(float));
			break;

		case 5:		//------  object
			TYPE("object");
			Ar << PROP(UObject*);
			PROP_DBG("%s", PROP(UObject*) ? PROP(UObject*)->Name : "Null");
			break;

		case 6:		//------  name
			TYPE("name");
			Ar << PROP(FName);
			PROP_DBG("%s", *PROP(FName));
			break;

		case 8:		//------  class
			appError("Class property not implemented");
			break;

		case 9:		//------  array
			appError("Array property not implemented");
			break;

		case 10:	//------ struct
			{
				// read structure name
				FName StrucName;
				Ar << StrucName;
				TYPE(*StrucName);
				SerializeStruc(Ar, value, StrucName);
				PROP_DBG("(complex)", 0);
			}
			break;

		case 13:	//------  str
			appError("String property not implemented");
			break;

		case 14:	//------  map
			appError("Map property not implemented");
			break;

		case 15:	//------  fixed array
			appError("FixedArray property not implemented");
			break;

		// reserved, but not implemented in unreal:
		case 7:		//------  string  => used str
		case 11:	//------  vector  => used structure"Vector"
		case 12:	//------  rotator => used structure"Rotator"
			appError("Unknown property");
			break;
		}

		unguardf(("%s", *PropName));
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


UObject *CreateClass(const char *Name)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
			return GClasses[i].Constructor();
	return NULL;
}


const CPropInfo *UObject::FindProperty(const CPropInfo *Table, int Count, const char *PropName)
{
	for (int i = 0; i < Count; i++, Table++)
		if (!strcmp(Table->Name, PropName))
			return Table;
	return NULL;
}
