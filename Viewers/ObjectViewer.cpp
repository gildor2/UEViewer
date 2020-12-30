#include "Core.h"
#include "UnCore.h"

#if RENDERING

#include "UnObject.h"
#include "ObjectViewer.h"
#include "UnrealPackage/UnPackage.h"	// for CObjectViewer::Draw2D( - UnPackage::Name
#include "UnrealPackage/PackageUtils.h"

#include "Exporters/Exporters.h"

CObjectViewer::CObjectViewer(const UObject* Obj, CApplication *Win, bool InNonVisualObject)
: Object(Obj)
, Window(Win)
, JumpAfterFrame(NULL)
, bHasVisualObject(!InNonVisualObject)
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
	}
}


void CObjectViewer::ProcessKey(unsigned key)
{
	guard(CObjectViewer::ProcessKey);
	if (!Object) return;

	switch (key)
	{
	case 'd':
		Dump();
		break;
	}
	unguard;
}


void CObjectViewer::ShowHelp()
{
	if (!Object) return;

	DrawKeyHelp("D", "dump info");
}

static CObjectViewer* GCurrentViewer = NULL;

static void DrawPropTextCallback(const char* LineText)
{
	guard(DrawPropTextCallback);

	const char* s = strchr(LineText, '\'');
	if (!s)
	{
		DrawTextLeft(S_WHITE "%s", LineText);
		return;
	}

	// Find a space on the left, there should be a class name
	const char* ClassName = s - 1;
	while (ClassName > LineText && ClassName[-1] != ' ')
		ClassName--;
	FStaticString<64> ClassStr(s - ClassName, ClassName);
	const char* s2 = strchr(s + 1, '\'');
	assert(s2);
	assert(s2[1] == 0);
	FStaticString<256> ObjectNameStr(s2 - s - 1, s + 1);
	FStaticString<64> Before(ClassName - LineText, LineText);

	if (DrawTextH(ETextAnchor::TopLeft, NULL, S_WHITE "%s" S_HYPERLINK("%s'%s'"), *Before, *ClassStr, *ObjectNameStr))
	{
		// Clicked, find an object
		for (UObject* Object : UObject::GObjObjects)
		{
			// The same code is used in TypeInfo.cpp, CollectProps(), UObject* property handler
			if (strcmp(Object->GetClassName(), *ClassStr) != 0)
				continue;
			char ObjName[256];
			Object->GetFullName(ARRAY_ARG(ObjName));
			if (strcmp(ObjName, *ObjectNameStr) != 0)
				continue;
			GCurrentViewer->JumpTo(Object);
			break;
		}
	}

	unguard;
}

void CObjectViewer::Draw2D()
{
	if (!Object)
	{
		if (GFullyLoadedPackages.Num() == 0)
		{
			// Nothing loaded, display a different message
			DrawTextLeft(S_RED "There's no packages has been loaded.");
			DrawTextLeft(S_RED "Press <O> to select and load a package.");
			return;
		}
		DrawTextLeft(S_RED "There's no visual objects loaded now.");
		DrawTextLeft(S_RED "Press <O> to load a different package.");

		TArray<ClassStats> stats;
		CollectPackageStats(GFullyLoadedPackages, stats);

		DrawTextLeft(S_GREEN"\nClass statistics:");
		for (int i = 0; i < stats.Num(); i++)
			DrawTextLeft("%5d %s", stats[i].Count, stats[i].Name);

		return;
	}

	const char *CookedPkgName   = Object->Package->Name;
	const char *UncookedPkgName = Object->GetUncookedPackageName();
	if (stricmp(CookedPkgName, UncookedPkgName) == 0)
		DrawTextLeft(S_GREEN "Package :" S_WHITE " %s", *Object->Package->GetFilename());
	else
		DrawTextLeft(S_GREEN "Package :" S_WHITE " %s (%s)", *Object->Package->GetFilename(), UncookedPkgName);

	DrawTextLeft(S_GREEN "Class   :" S_WHITE " %s\n"
				 S_GREEN "Object  :" S_WHITE " %s",
				 Object->GetRealClassName(), Object->Name);

	// get group name
	if (Object->Package && Object->Package->Game < GAME_UE4_BASE)
	{
		char group[512];
		Object->GetFullName(ARRAY_ARG(group), false, false);
		if (group[0])
		{
			DrawTextLeft(S_GREEN "Group   :" S_WHITE " %s", group);
		}
	}

	DrawTextLeft("");
	GCurrentViewer = this;

	if (!bHasVisualObject)
	{
		DrawTextLeft(S_WHITE "Object Properties:\n");
		auto DrawTextCallback = [](const char* LineText)
		{
			DrawTextLeft("%s", LineText);
		};
		Object->GetTypeinfo()->DumpProps(Object, DrawPropTextCallback);
	}

	GCurrentViewer = NULL;
}

#if HAS_UI

UIMenuItem* CObjectViewer::GetObjectMenu(UIMenuItem* menu)
{
	return menu;
}

#endif // HAS_UI

#endif // RENDERING
