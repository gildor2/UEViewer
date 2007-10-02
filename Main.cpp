#include "ObjectViewer.h"


#define APP_CAPTION		"UT2 Mesh Viewer"


static CObjectViewer *Viewer;			// used from GlWindow callbacks


/*-----------------------------------------------------------------------------
	Table of known Unreal classes
-----------------------------------------------------------------------------*/

static void RegisterUnrealClasses()
{
BEGIN_CLASS_TABLE
	REGISTER_CLASS(UMaterial)
	REGISTER_CLASS(USkeletalMesh)
	REGISTER_CLASS(UVertMesh)
	REGISTER_CLASS(UMeshAnimation)
END_CLASS_TABLE
}


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

void main(int argc, char **argv)
{
	// display usage
	if (argc < 2)
	{
	help:
		printf( "Usage:\n"
				"  UnLoader [-dump] <package file> <object name> [<class name>]\n"
				"  UnLoader -list <package file>\n");
		exit(0);
	}

	// parse command line
	bool dumpOnly = false, listOnly = false;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			if (!strcmp(argv[arg]+1, "dump"))
				dumpOnly = true;
			else if (!strcmp(argv[arg]+1, "list"))
				listOnly = true;
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

	// setup GNotifyInfo to describe package only
	GNotifyInfo = argPkgName;
	// load package
	UnPackage Pkg(argPkgName);

	if (listOnly)
	{
		// dump package exports table
		for (int i = 0; i < Pkg.Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = Pkg.ExportTable[i];
			printf("%d %s %s\n", i, Pkg.GetObjectName(Exp.ClassIndex), *Exp.ObjectName);
		}
		return;
	}

	// get requested object info
	int idx = Pkg.FindExport(argObjName, argClassName);
	if (idx < 0)
		appError("Export \"%s\" was not found", argObjName);
	const char *className = Pkg.GetObjectName(Pkg.ExportTable[idx].ClassIndex);

	// setup GNotifyInfo to describe object
	char nameBuf[256];
	appSprintf(ARRAY_ARG(nameBuf), "%s:  %s'%s'", argPkgName, className, argObjName);
	GNotifyInfo = nameBuf;

	// create object from package
	UObject *Obj = Pkg.CreateExport(idx);

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

	// print mesh info
	Viewer->Dump();

	if (!dumpOnly)
		VisualizerLoop(APP_CAPTION);

	delete Viewer;
	delete Obj;
}


/*-----------------------------------------------------------------------------
	GlWindow callbacks
-----------------------------------------------------------------------------*/

void AppDrawFrame()
{
	Viewer->Draw3D();
}


void AppKeyEvent(unsigned char key)
{
	Viewer->ProcessKey(key);
}


void AppDisplayTexts(bool helpVisible)
{
	if (helpVisible)
	{
		Viewer->ShowHelp();
		GL::textf("-----\n\n");		// divider
	}
	Viewer->Draw2D();
}
