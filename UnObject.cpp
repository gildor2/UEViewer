#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


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
	// process GObjLoaded array
	// NOTE: while loading one array element, array may grow!
	for (int i = 0; i < GObjLoaded.Num(); i++)
	{
		//!! should sort by packages
		UObject *Obj = GObjLoaded[i];
		UnPackage *Package = Obj->Package;
		Package->SetupReader(Obj->PackageIndex);
		printf("Loading %s %s from package %s\n", Obj->GetClassName(), Obj->Name, Package->SelfName);
		Obj->Serialize(*Package);
		// check for unread bytes
		if (!Package->IsStopper())
			appError("%s: extra bytes", Obj->Name);
	}
	GObjLoaded.Empty();
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
