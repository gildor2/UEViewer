#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnAnimNotify.h"

#include "Viewers/ObjectViewer.h"
#include "Exporters/Exporters.h"


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
#if UNREAL1
	REGISTER_MESH_CLASSES_U1
#endif
#if RUNE
	REGISTER_MESH_CLASSES_RUNE
#endif
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
	Exporters
-----------------------------------------------------------------------------*/

#define MAX_EXPORTERS		16

typedef void (*ExporterFunc_t)(UObject*, FArchive&);

struct CExporterInfo
{
	const char		*ClassName;
	const char		*FileExt;
	ExporterFunc_t	Func;
};

static CExporterInfo exporters[MAX_EXPORTERS];
static int numExporters = 0;

static void RegisterExporter(const char *ClassName, const char *FileExt, ExporterFunc_t Func)
{
	guard(RegisterExporter);
	assert(numExporters < MAX_EXPORTERS);
	CExporterInfo &Info = exporters[numExporters];
	Info.ClassName = ClassName;
	Info.FileExt   = FileExt;
	Info.Func      = Func;
	numExporters++;
	unguard;
}

#define EXPORTER(class,ext,func)		RegisterExporter(class, ext, (ExporterFunc_t)func)


static bool ExportObject(UObject *Obj)
{
	guard(ExportObject);

	for (int i = 0; i < numExporters; i++)
	{
		const CExporterInfo &Info = exporters[i];
		if (Obj->IsA(Info.ClassName))
		{
			if (Info.FileExt)
			{
				char filename[64];
				appSprintf(ARRAY_ARG(filename), "%s.%s", Obj->Name, Info.FileExt);
				FFileReader Ar(filename, false);
				Info.Func(Obj, Ar);
			}
			else
			{
				Info.Func(Obj, *GDummySave);
			}
			return true;
		}
	}
	return false;

	unguardf(("%s'%s'", Obj->GetClassName(), Obj->Name));
}


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
#if DO_GUARD
	try {
#endif

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
				"    -pkginfo        load package and display its information\n"
				"\n"
				"Options:\n"
				"    -path=PATH      path to UT installation directory; if not specified,\n"
				"                    program will search for packages in current directory\n"
				"    -noanim         disable loading of MeshAnimation classes in a case of\n"
				"                    unsupported data format\n"
				"\n"
				"Supported resources for export:\n"
				"    SkeletalMesh    exported as ActorX psk file\n"
				"    MeshAnimation   exported as ActorX psa file\n"
				"    VertMesh        exported as Unreal 3d file\n"
				"    Texture         exported in tga format\n"
				"\n"
				//?? separate option for this list?
				//?? this list looks ugly ... apply some formatting?
				"List of supported games:\n"
#if UNREAL1
				"Unreal Engine 1:\n"
				"    Unreal 1, Unreal Tournament 1, The Wheel of Time"
	#if DEUS_EX
				", DeusEx"
	#endif
	#if RUNE
				", Rune"
	#endif
				"\n"
#endif // UNREAL1
				"Unreal Engine 2:\n"
#if UT2
				"    Unreal Tournament 2003,2004\n"
#endif
#if SPLINTER_CELL
				"    Splinter Cell 1,2\n"
#endif
#if LINEAGE2
				"    Lineage 2 Gracia\n"
#endif
//				"Unreal Engine 2.5:\n"
#if UNREAL25
				"    UE2Runtime, Harry Potter and the Prisoner of Azkaban\n"
#endif
#if TRIBES3
				"    Tribes: Vengeance\n"
#endif
#if RAGNAROK2
				"    Ragnarok Online 2\n"
#endif
				"\n"
				"For details and updates please visit http://www.gildor.org/projects/umodel\n"
		);
		exit(0);
	}

	// parse command line
	bool dump = false, view = true, exprt = false, listOnly = false, noAnim = false, pkgInfo = false;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!stricmp(opt, "dump"))
			{
				dump  = true;
				view  = false;
				exprt = false;
			}
			else if (!stricmp(opt, "check"))
			{
				dump  = false;
				view  = false;
				exprt = false;
			}
			else if (!stricmp(opt, "export"))
			{
				dump  = false;
				view  = false;
				exprt = true;
			}
			else if (!stricmp(opt, "pkginfo"))
				pkgInfo = true;
			else if (!stricmp(opt, "list"))
				listOnly = true;
			else if (!stricmp(opt, "noanim"))
				noAnim = true;
			else if (!strnicmp(opt, "path=", 5))
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
	if (!argPkgName) goto help;
	const char *argObjName   = NULL;
	const char *argClassName = NULL;
	if (arg < argc-1)
		argObjName   = argv[arg+1];
	if (arg < argc-2)
		argClassName = argv[arg+2];

	// register exporters
	EXPORTER("SkeletalMesh",  "psk", ExportPsk);
	EXPORTER("MeshAnimation", "psa", ExportPsa);
	EXPORTER("VertMesh",      NULL,  Export3D );
	EXPORTER("Texture",       "tga", ExportTga);

	// prepare classes
	RegisterUnrealClasses();
	// remove UMeshAnimation loader when requisted by command line
	if (noAnim)
	{
		UnregisterClass("MeshAnimation");
		UnregisterClass("Animation");
	}

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

	if (pkgInfo)
		return 0;

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
		if (idx == INDEX_NONE)
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
		appSetNotifyHeader(NULL);
		// export object, if possible
		for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
		{
			Obj = UObject::GObjObjects[idx];
			printf("Exporting %s ...\n", Obj->Name);
			if (!ExportObject(Obj))
				printf("ERROR: exporting object %s: unsupported type %s\n", Obj->Name, Obj->GetClassName());
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

#if DO_GUARD
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
#endif
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
