#include "ObjectViewer.h"
#include "UnAnimNotify.h"


#define APP_CAPTION		"UT2 Mesh Viewer"


static CObjectViewer *Viewer;			// used from GlWindow callbacks


/*-----------------------------------------------------------------------------
	Table of known Unreal classes
-----------------------------------------------------------------------------*/

static void RegisterUnrealClasses()
{
BEGIN_CLASS_TABLE
	REGISTER_MATERIAL_CLASSES
	REGISTER_MESH_CLASSES
	REGISTER_ANIM_NOTIFY_CLASSES
END_CLASS_TABLE
}


/*-----------------------------------------------------------------------------
	Object list support
	change!!
-----------------------------------------------------------------------------*/

static bool CreateVisualizer(UObject *Obj, bool test = false);

inline bool ObjectSupported(UObject *Obj)
{
	return CreateVisualizer(Obj, true);
}

static int ObjIndex = 0;


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
	try {

	guard(Main);

	// display usage
	if (argc < 2)
	{
	help:
		printf( "Usage:\n"
				"  UnLoader [-dump|-check] [-path=PATH] <package> [<object> [<class>]]\n"
				"  UnLoader -list <package file>\n"
				"    PATH      = path to UT root directory\n"
				"    <package> = full package filename (path/name.ext) or short name\n"
		);
		exit(0);
	}

	// parse command line
	bool dump = false, view = true, listOnly = false;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!strcmp(opt, "dump"))
			{
				dump = true;
				view = false;
			}
			else if (!strcmp(opt, "check"))
			{
				dump = false;
				view = false;
			}
			else if (!strcmp(opt, "list"))
				listOnly = true;
			else if (!strncmp(opt, "path=", 5))
				UnPackage::SetSearchPath(opt+5);
			else
				goto help;
		}
		else
		{
			break;
		}
	}
	const char *argPkgName   = argv[arg];
	const char *argObjName   = NULL;
	const char *argClassName = NULL;
	if (arg < argc-1)
		argObjName   = argv[arg+1];
	if (arg < argc-2)
		argClassName = argv[arg+2];

	// prepare classes
	RegisterUnrealClasses();

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// load package
	UnPackage *Package;
	if (strchr(argPkgName, '.'))
		Package = new UnPackage(argPkgName);
	else
		Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		printf("Unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	if (listOnly)
	{
		guard(List);
		// dump package exports table
		for (int i = 0; i < Package->Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = Package->ExportTable[i];
			printf("%d %s %s\n", i, Package->GetObjectName(Exp.ClassIndex), *Exp.ObjectName);
		}
		unguard;
		return 0;
	}

	UObject *Obj = NULL;
	// get requested object info
	if (argObjName)
	{
		int idx = Package->FindExport(argObjName, argClassName);
		if (idx < 0)
			appError("Export \"%s\" was not found", argObjName);
		const char *className = Package->GetObjectName(Package->ExportTable[idx].ClassIndex);

		// setup NotifyInfo to describe object
		appSetNotifyHeader("%s:  %s'%s'", argPkgName, className, argObjName);
		// create object from package
		Obj = Package->CreateExport(idx);
	}
	else
	{
		guard(LoadWholePackage);
		// load whole package
		for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
		{
			if (!IsKnownClass(Package->GetObjectName(Package->GetExport(idx).ClassIndex)))
				continue;
			int TmpObjIdx = UObject::GObjObjects.Num();
			UObject *TmpObj = Package->CreateExport(idx);
			if (!Obj && ObjectSupported(TmpObj))
			{
				Obj = TmpObj;
				ObjIndex = TmpObjIdx;
			}
		}
		if (!Obj)
			Obj = UObject::GObjObjects[1];
		unguard;
	}

	CreateVisualizer(Obj);
	// print mesh info
#if TEST_FILES
	Viewer->Test();
#endif

	if (dump)
		Viewer->Dump();					// dump to console and exit
	if (view)
	{
		GL::invertXAxis = true;
		guard(MainLoop);
		VisualizerLoop(APP_CAPTION);	// show object
		unguard;
	}

	delete Viewer;
	delete Obj;

	unguard;

	} catch (...) {
		if (GErrorHistory[0])
		{
//			printf("ERROR: %s\n", GErrorHistory);
			appNotify("ERROR: %s\n", GErrorHistory);
		}
		else
		{
//			printf("Unknown error\n");
			appNotify("Unknown error\n");
		}
		exit(1);
	}
	return 0;
}


/*-----------------------------------------------------------------------------
	GlWindow callbacks
-----------------------------------------------------------------------------*/

static bool CreateVisualizer(UObject *Obj, bool test)
{
	guard(CreateVisualizer);
	if (!test)
		appSetNotifyHeader("%s:  %s'%s'", Obj->Package->Filename, Obj->GetClassName(), Obj->Name);
	// create viewer class
#define CLASS_VIEWER(UClass, CViewer)	\
	if (Obj->IsA(#UClass + 1))			\
	{									\
		if (!test)						\
			Viewer = new CViewer(static_cast<UClass*>(Obj)); \
		return true;					\
	}
	// create viewer for known class
	CLASS_VIEWER(UVertMesh,     CVertMeshViewer);
	CLASS_VIEWER(USkeletalMesh, CSkelMeshViewer);
	CLASS_VIEWER(UMaterial,     CMaterialViewer);
	// fallback for unknown class
	if (!test)
		Viewer = new CObjectViewer(Obj);
	return false;
#undef CLASS_VIEWER
	unguard;
}


void AppDrawFrame()
{
	guard(AppDrawFrame);
	Viewer->Draw3D();
	unguard;
}


void AppKeyEvent(int key)
{
	guard(AppKeyEvent);
	if (key == SPEC_KEY(PAGEDOWN) || key == SPEC_KEY(PAGEUP))
	{
		// browse loaded objects
		int looped = 0;
		UObject *Obj;
		while (true)
		{
			if (key == SPEC_KEY(PAGEDOWN))
			{
				ObjIndex++;
				if (ObjIndex >= UObject::GObjObjects.Num())
				{
					ObjIndex = 0;
					looped++;
				}
			}
			else
			{
				ObjIndex--;
				if (ObjIndex < 0)
				{
					ObjIndex = UObject::GObjObjects.Num()-1;
					looped++;
				}
			}
			if (looped > 1)
				return;		// prevent infinite loop
			Obj = UObject::GObjObjects[ObjIndex];
			if (ObjectSupported(Obj))
				break;
		}
		// change visualizer
		delete Viewer;
		CreateVisualizer(Obj);
		return;
	}
	if (key == 'd')
	{
		Viewer->Dump();
		return;
	}
	Viewer->ProcessKey(key);
	unguard;
}


void AppDisplayTexts(bool helpVisible)
{
	guard(AppDisplayTexts);
	if (helpVisible)
	{
		DrawTextLeft("PgUp/PgDn   browse objects");
		DrawTextLeft("D           dump info");
		Viewer->ShowHelp();
		DrawTextLeft("-----\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}
