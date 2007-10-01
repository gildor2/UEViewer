#include "Core.h"

#include "UnCore.h"
#include "UnMesh.h"
#include "UnPackage.h"

#include "GlWindow.h"
#include "ObjectViewer.h"


#define APP_CAPTION		"UT2 Mesh Viewer"


static CObjectViewer *Viewer;

static const char* MeshName   = "";		//??


void main(int argc, char **argv)
{
	if (argc < 2)
	{
	help:
		printf( "Usage:\n"
				"  UnLoader [-dump] <.ukx file> <mesh name>\n"
				"  UnLoader -list <.ukx file>\n");
		exit(0);
	}

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

	GNotifyInfo = argv[arg];
	UnPackage Pkg(argv[arg]);

	if (listOnly)
	{
		for (int i = 0; i < Pkg.Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = Pkg.ExportTable[i];
			printf("%d %s %s\n", i, Pkg.GetClassName(Exp.ClassIndex), Pkg.GetName(Exp.ObjectName));
		}
		return;
	}

	int idx = Pkg.FindExport(argv[arg+1]);
	if (idx < 0)
		appError("Export \"%s\" was not found", argv[arg+1]);
	printf("Export \"%s\" was found at index %d\n", argv[arg+1], idx);
	const char *className = Pkg.GetClassName(Pkg.ExportTable[idx].ClassIndex);

	FArchive Ar;
	Pkg.SetupReader(idx, Ar);

	char nameBuf[256];
	appSprintf(ARRAY_ARG(nameBuf), "%s, %s (%s)", argv[arg], argv[arg+1], className);
	GNotifyInfo = MeshName = nameBuf;

	// check mesh type, create mesh and viewer
	ULodMesh *Mesh = NULL;
	if (!strcmp(className, "VertMesh"))
	{
		Mesh     = new UVertMesh;
		Viewer   = new CVertMeshViewer(static_cast<UVertMesh*>(Mesh));
	}
	else if (!strcmp(className, "SkeletalMesh"))
	{
		Mesh     = new USkeletalMesh;
		Viewer   = new CSkelMeshViewer(static_cast<USkeletalMesh*>(Mesh));
	}
	else
		appError("Unknown class: %s", className);

	// serialize mesh
	Mesh->Serialize(Ar);

	// check for unread bytes
	if (!Ar.IsStopper()) appError("extra bytes!");

	// print mesh info
	Viewer->Dump();

	if (!dumpOnly)
		VisualizerLoop(APP_CAPTION);

	delete Viewer;
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
	GL::textf("Mesh: %s\n", MeshName);	//??
}
