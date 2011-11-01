#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "UnPackage.h"			// for CObjectViewer::Draw2D()


CObjectViewer::CObjectViewer(UObject *Obj)
:	Object(Obj)
{
	SetDistScale(1);
	ResetView();
	SetViewOffset(nullVec3);
}


void CObjectViewer::Dump()
{
	appPrintf("\nObject info:\n============\n");
	appPrintf("ClassName: %s ObjectName: %s\n", Object->GetClassName(), Object->Name);
	Object->GetTypeinfo()->DumpProps(Object);
}


void CObjectViewer::Draw2D()
{
	const char *CookedPkgName   = Object->Package->Name;
	const char *UncookedPkgName = Object->GetUncookedPackageName();
	if (stricmp(CookedPkgName, UncookedPkgName) == 0)
		DrawTextLeft(S_GREEN"Package:"S_WHITE" %s", Object->Package->Filename);
	else
		DrawTextLeft(S_GREEN"Package:"S_WHITE" %s (%s)", Object->Package->Filename, UncookedPkgName);

	DrawTextLeft(S_GREEN"Class  :"S_WHITE" %s\n"
				 S_GREEN"Object :"S_WHITE" %s\n",
				 Object->GetRealClassName(), Object->Name);
}

#endif // RENDERING
