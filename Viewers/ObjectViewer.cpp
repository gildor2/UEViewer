#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "UnPackage.h"			// for CObjectViewer::Draw2D()

#include "Exporters/Exporters.h"


CObjectViewer::CObjectViewer(UObject* Obj, CApplication *Win)
:	Object(Obj)
,	Window(Win)
{
	SetDistScale(1);
	ResetView();
	SetViewOffset(nullVec3);
}


void CObjectViewer::Dump()
{
	if (Object)
	{
		appPrintf("\nObject info:\n============\n");
		appPrintf("ClassName: %s ObjectName: %s\n", Object->GetClassName(), Object->Name);
		Object->GetTypeinfo()->DumpProps(Object);
	}
}


void CObjectViewer::Export()
{
	if (Object)
	{
		ExportObject(Object);
		ResetExportedList();
	}
}


void CObjectViewer::ProcessKey(int key)
{
	if (!Object) return;

	switch (key)
	{
	case 'x'|KEY_CTRL:
		Export();
		break;
	case 'd':
		Dump();
		break;
	}
}


void CObjectViewer::ShowHelp()
{
	if (!Object) return;

	DrawKeyHelp("D",      "dump info");
	DrawKeyHelp("Ctrl+X", "export object");
}


void CObjectViewer::Draw2D()
{
	if (!Object)
	{
		DrawTextLeft(S_RED "There's no visual object loaded now.");
		DrawTextLeft(S_RED "Press <O> to load a different package.");
		return;
	}

	const char *CookedPkgName   = Object->Package->Name;
	const char *UncookedPkgName = Object->GetUncookedPackageName();
	if (stricmp(CookedPkgName, UncookedPkgName) == 0)
		DrawTextLeft(S_GREEN "Package:" S_WHITE " %s", Object->Package->Filename);
	else
		DrawTextLeft(S_GREEN "Package:" S_WHITE " %s (%s)", Object->Package->Filename, UncookedPkgName);

	DrawTextLeft(S_GREEN "Class  :" S_WHITE " %s\n"
				 S_GREEN "Object :" S_WHITE " %s\n",
				 Object->GetRealClassName(), Object->Name);
}

#endif // RENDERING
