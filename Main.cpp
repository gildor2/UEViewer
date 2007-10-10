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
	Main function
-----------------------------------------------------------------------------*/

void main(int argc, char **argv)
{
	try {

	guard(Main);

	// display usage
	if (argc < 2)
	{
	help:
		printf( "Usage:\n"
				"  UnLoader [-dump|-check] [-path=PATH] <package file> <object name> [<class name>]\n"
				"  UnLoader -list <package file>\n"
				"    PATH = path to UT root directory\n"
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
	const char *argObjName   = argv[arg+1];
	const char *argClassName = NULL;
	if (arg < argc-2)
		argClassName = argv[arg+2];

	// prepare classes
	RegisterUnrealClasses();

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// load package
	UnPackage Pkg(argPkgName);

	if (listOnly)
	{
		guard(List);
		// dump package exports table
		for (int i = 0; i < Pkg.Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = Pkg.ExportTable[i];
			printf("%d %s %s\n", i, Pkg.GetObjectName(Exp.ClassIndex), *Exp.ObjectName);
		}
		unguard;
		return;
	}

	// get requested object info
	int idx = Pkg.FindExport(argObjName, argClassName);
	if (idx < 0)
		appError("Export \"%s\" was not found", argObjName);
	const char *className = Pkg.GetObjectName(Pkg.ExportTable[idx].ClassIndex);

	// setup NotifyInfo to describe object
	appSetNotifyHeader("%s:  %s'%s'", argPkgName, className, argObjName);

	// create object from package
	UObject::BeginLoad();	//!!!
	UObject *Obj = Pkg.CreateExport(idx);
	UObject::EndLoad();		//!!!

	guard(CreateVisualizer);
	// create viewer class
	if (!strcmp(className, "VertMesh"))
	{
		Viewer = new CVertMeshViewer(static_cast<UVertMesh*>(Obj));
	}
	else if (!strcmp(className, "SkeletalMesh"))
	{
		Viewer = new CSkelMeshViewer(static_cast<USkeletalMesh*>(Obj));
	}
	else
	{
		Viewer = new CObjectViewer(Obj);
	}
	unguard;

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
}


/*-----------------------------------------------------------------------------
	GlWindow callbacks
-----------------------------------------------------------------------------*/

void AppDrawFrame()
{
	guard(AppDrawFrame);
	Viewer->Draw3D();
	unguard;
}


void AppKeyEvent(unsigned char key)
{
	guard(AppKeyEvent);
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
		GL::text("D           dump info\n");
		Viewer->ShowHelp();
		GL::text("-----\n\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}
