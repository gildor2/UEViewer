#include "ObjectViewer.h"
#include "UnAnimNotify.h"
#include "Exporters.h"


#define APP_CAPTION		"Unreal Model Viewer"


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
		printf(	"Unreal model viewer / exporter\n"
				"Usage: umodel [command] [options] <package> [<object> [<class>]]\n"
				"\n"
				"    <package>       name of package to load, without file extension\n"
				"    <object>        name of object to load\n"
				"    <class>         class of object to load (useful, when trying to load\n"
				"                    object with ambiguous name)\n"
				"\n"
				"Commands:\n"
				"    (default)       visualize object; when <object> not specified, will load\n"
				"                    whole package\n"
				"    -list           list contents of package\n"
				"    -export         export specified object or whole package\n"
				"\n"
				"Developer commands:\n"
				"    -dump           dump object information to console\n"
				"    -check          check some assumptions, no other actions performed\n"
				"\n"
				"Options:\n"
				"    -path=PATH      path to UT installation directory; if not specified,\n"
				"                    program will search for packages in current directory\n"
				"\n"
				"Supported resources for export:\n"
				"    SkeletalMesh    exported as ActorX psk file\n"
				"    MeshAnimation   exported as ActorX psa file\n"
				"    Texture         exported in tga format\n"
				"\n"
				"Supported games:\n"
				"    Unreal 1, Unreal Tournament 1\n"
				"    DeusEx\n"
				"    Unreal Tournament 2003,2004\n"
				"    Splinter Cell 1,2\n"
				"\n"
				"For details and updates please visit http://www.gildor.org/projects/umodel\n"
		);
		exit(0);
	}

	// parse command line
	bool dump = false, view = true, exprt = false, listOnly = false;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!strcmp(opt, "dump"))
			{
				dump  = true;
				view  = false;
				exprt = false;
			}
			else if (!strcmp(opt, "check"))
			{
				dump  = false;
				view  = false;
				exprt = false;
			}
			else if (!strcmp(opt, "export"))
			{
				dump  = false;
				view  = false;
				exprt = true;
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
		{
			printf("Export \"%s\" was not found in package \"%s\"\n", argObjName, argPkgName);
			exit(1);
		}
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
		{
			if (!UObject::GObjObjects.Num())
			{
				printf("Package \"%s\" has no supported objects\n", argPkgName);
				exit(1);
			}
			Obj = UObject::GObjObjects[1];
		}
		unguard;
	}
	if (!Obj) return 0;					// object was not created

	CreateVisualizer(Obj);
	// print mesh info
#if TEST_FILES
	Viewer->Test();
#endif

	if (dump)
		Viewer->Dump();					// dump info to console

	if (exprt)
	{
		// export object, if possible
		for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
		{
			Obj = UObject::GObjObjects[idx];
			printf("Exporting %s ...\n", Obj->Name);
			if (Obj->IsA("SkeletalMesh"))
			{
				char filename[64];
				appSprintf(ARRAY_ARG(filename), "%s.psk", Obj->Name);
				FFileReader Ar(filename, false);
				ExportPsk(static_cast<USkeletalMesh*>(Obj), Ar);
			}
			else if (Obj->IsA("MeshAnimation"))
			{
				char filename[64];
				appSprintf(ARRAY_ARG(filename), "%s.psa", Obj->Name);
				FFileReader Ar(filename, false);
				ExportPsa(static_cast<UMeshAnimation*>(Obj), Ar);
			}
			else if (Obj->IsA("Texture"))
			{
				char filename[64];
				appSprintf(ARRAY_ARG(filename), "%s.tga", Obj->Name);
				FFileReader Ar(filename, false);
				ExportTga(static_cast<UTexture*>(Obj), Ar);
			}
			else
			{
				printf("ERROR: exporting object %s: unsupported type %s\n", Obj->Name, Obj->GetClassName());
			}
			if (argObjName) break;		// export specified (1st) object only
		}
	}

	if (view)
	{
		// show object
		vpInvertXAxis = true;
		guard(MainLoop);
		VisualizerLoop(APP_CAPTION);
		unguard;
	}

	// cleanup
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
	{
		Viewer = new CObjectViewer(Obj);
	}
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
