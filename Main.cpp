#include <SDL/SDL_syswm.h>			// for SDL_SysWMinfo
#undef UnregisterClass

#include "Core.h"

#if _WIN32
#include <direct.h>					// getcwd
#else
#include <unistd.h>					// getcwd
#endif

#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnAnimNotify.h"

#include "UnMaterial2.h"
#include "UnMaterial3.h"

#include "UnMesh2.h"
#include "UnMesh3.h"

#include "UnSound.h"
#include "UnThirdParty.h"

#include "Viewers/ObjectViewer.h"
#include "Exporters/Exporters.h"

#if DECLARE_VIEWER_PROPS
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#endif

#include "GameDatabase.h"
#include "UmodelSettings.h"

#include "PackageUtils.h"

//!! move UI code to separate cpp and simply call their functions
#if HAS_UI
#include "UI/BaseDialog.h"
#include "UI/StartupDialog.h"
#include "UI/PackageDialog.h"
#include "UI/ProgressDialog.h"
#endif // HAS_UI

#define APP_CAPTION					"UE Viewer"
#define HOMEPAGE					"http://www.gildor.org/en/projects/umodel"

//#define SHOW_HIDDEN_SWITCHES		1
//#define DUMP_MEM_ON_EXIT			1

#if RENDERING
static CObjectViewer *Viewer;			// used from GlWindow callbacks
static bool showMeshes = false;
static bool showMaterials = false;
#endif

class CUmodelApp : public CApplication
{
	virtual void Draw3D(float TimeDelta);
	virtual void DrawTexts(bool helpVisible);
	virtual void BeforeSwap();
	virtual void ProcessKey(int key, bool isDown);
};

static CUmodelApp GApplication;

static UmodelSettings GSettings;


/*-----------------------------------------------------------------------------
	Table of known Unreal classes
-----------------------------------------------------------------------------*/

static void RegisterCommonUnrealClasses()
{
	// classes and structures
	RegisterCoreClasses();
BEGIN_CLASS_TABLE

	REGISTER_MATERIAL_CLASSES
	REGISTER_ANIM_NOTIFY_CLASSES
#if BIOSHOCK
	REGISTER_MATERIAL_CLASSES_BIO
	REGISTER_MESH_CLASSES_BIO
#endif
#if SPLINTER_CELL
	REGISTER_MATERIAL_CLASSES_SCELL
#endif
#if UNREAL3
	REGISTER_MATERIAL_CLASSES_U3		//!! needed for Bioshock 2 too
#endif

#if DECLARE_VIEWER_PROPS
	REGISTER_SKELMESH_VCLASSES
	REGISTER_STATICMESH_VCLASSES
#endif // DECLARE_VIEWER_PROPS

END_CLASS_TABLE
	// enumerations
	REGISTER_MATERIAL_ENUMS
#if UNREAL3
	REGISTER_MATERIAL_ENUMS_U3
	REGISTER_MESH_ENUMS_U3
#endif
}


static void RegisterUnrealClasses2()
{
BEGIN_CLASS_TABLE
	REGISTER_MESH_CLASSES_U2
#if UNREAL1
	REGISTER_MESH_CLASSES_U1
#endif
#if RUNE
	REGISTER_MESH_CLASSES_RUNE
#endif
END_CLASS_TABLE
}


static void RegisterUnrealClasses3()
{
#if UNREAL3
BEGIN_CLASS_TABLE
//	REGISTER_MATERIAL_CLASSES_U3 -- registered for Bioshock in RegisterCommonUnrealClasses()
	REGISTER_MESH_CLASSES_U3
#if TUROK
	REGISTER_MESH_CLASSES_TUROK
#endif
#if MASSEFF
	REGISTER_MESH_CLASSES_MASSEFF
#endif
#if DCU_ONLINE
	REGISTER_MATERIAL_CLASSES_DCUO
#endif
#if TRANSFORMERS
	REGISTER_MESH_CLASSES_TRANS
#endif
END_CLASS_TABLE
#endif // UNREAL3
}



static void RegisterUnrealSoundClasses()
{
BEGIN_CLASS_TABLE
	REGISTER_SOUND_CLASSES
#if UNREAL3
	REGISTER_SOUND_CLASSES_UE3
#endif
#if TRANSFORMERS
	REGISTER_SOUND_CLASSES_TRANS
#endif
END_CLASS_TABLE
}


static void RegisterUnreal3rdPartyClasses()
{
#if UNREAL3
BEGIN_CLASS_TABLE
	REGISTER_3RDP_CLASSES
END_CLASS_TABLE
#endif
}

static void RegisterClasses(int game)
{
	// prepare classes
	// note: we are registering classes after loading package: in this case we can know engine version (1/2/3)
	RegisterCommonUnrealClasses();
	if (game < GAME_UE3)
	{
		RegisterUnrealClasses2();
	}
	else
	{
		RegisterUnrealClasses3();
		RegisterUnreal3rdPartyClasses();
	}
	if (GSettings.UseSound) RegisterUnrealSoundClasses();

	// remove some class loaders when requisted by command line
	if (!GSettings.UseAnimation)
	{
		UnregisterClass("MeshAnimation", true);
		UnregisterClass("AnimSet",       true);
		UnregisterClass("AnimSequence",  true);
		UnregisterClass("AnimNotify",    true);
	}
	if (!GSettings.UseSkeletalMesh)
	{
		UnregisterClass("SkeletalMesh",       true);
		UnregisterClass("SkeletalMeshSocket", true);
	}
	if (!GSettings.UseStaticMesh) UnregisterClass("StaticMesh", true);
	if (!GSettings.UseTexture) UnregisterClass("UnrealMaterial", true);
	if (!GSettings.UseLightmapTexture) UnregisterClass("LightMapTexture2D", true);
	if (!GSettings.UseScaleForm) UnregisterClass("SwfMovie", true);
	if (!GSettings.UseFaceFx)
	{
		UnregisterClass("FaceFXAnimSet", true);
		UnregisterClass("FaceFXAsset", true);
	}
}


/*-----------------------------------------------------------------------------
	Exporters
-----------------------------------------------------------------------------*/

// wrappers
static void ExportSkeletalMesh2(const USkeletalMesh *Mesh)
{
	assert(Mesh->ConvertedMesh);
	if (!GSettings.ExportMd5Mesh)
		ExportPsk(Mesh->ConvertedMesh);
	else
		ExportMd5Mesh(Mesh->ConvertedMesh);
}

#if UNREAL3
static void ExportSkeletalMesh3(const USkeletalMesh3 *Mesh)
{
	assert(Mesh->ConvertedMesh);
	if (!GSettings.ExportMd5Mesh)
		ExportPsk(Mesh->ConvertedMesh);
	else
		ExportMd5Mesh(Mesh->ConvertedMesh);
}
#endif // UNREAL3

static void ExportStaticMesh2(const UStaticMesh *Mesh)
{
	assert(Mesh->ConvertedMesh);
	ExportStaticMesh(Mesh->ConvertedMesh);
}

#if UNREAL3
static void ExportStaticMesh3(const UStaticMesh3 *Mesh)
{
	assert(Mesh->ConvertedMesh);
	ExportStaticMesh(Mesh->ConvertedMesh);
}
#endif

static void ExportMeshAnimation(const UMeshAnimation *Anim)
{
	assert(Anim->ConvertedAnim);
	if (!GSettings.ExportMd5Mesh)
		ExportPsa(Anim->ConvertedAnim);
	else
		ExportMd5Anim(Anim->ConvertedAnim);
}

#if UNREAL3
static void ExportAnimSet(const UAnimSet *Anim)
{
	assert(Anim->ConvertedAnim);
	if (!GSettings.ExportMd5Mesh)
		ExportPsa(Anim->ConvertedAnim);
	else
		ExportMd5Anim(Anim->ConvertedAnim);
}
#endif // UNREAL3

static void RegisterExporters()
{
	RegisterExporter("SkeletalMesh",  ExportSkeletalMesh2);
	RegisterExporter("MeshAnimation", ExportMeshAnimation);
#if UNREAL3
	RegisterExporter("SkeletalMesh3", ExportSkeletalMesh3);
	RegisterExporter("AnimSet",       ExportAnimSet      );
#endif
	RegisterExporter("VertMesh",      Export3D         );
	RegisterExporter("StaticMesh",    ExportStaticMesh2);
	RegisterExporter("Texture",       ExportTexture    );
	RegisterExporter("Sound",         ExportSound      );
#if UNREAL3
	RegisterExporter("StaticMesh3",   ExportStaticMesh3  );
	RegisterExporter("Texture2D",     ExportTexture      );
	RegisterExporter("SoundNodeWave", ExportSoundNodeWave);
	RegisterExporter("SwfMovie",      ExportGfx          );
	RegisterExporter("FaceFXAnimSet", ExportFaceFXAnimSet);
	RegisterExporter("FaceFXAsset",   ExportFaceFXAsset  );
#endif // UNREAL3
	RegisterExporter("UnrealMaterial", ExportMaterial);			// register this after Texture/Texture2D exporters
}


/*-----------------------------------------------------------------------------
	Initialization of class and export systems
-----------------------------------------------------------------------------*/

static void InitClassAndExportSystems(int Game)
{
	static bool initialized = false;
	if (initialized) return;
	initialized = true;

	RegisterExporters();
	RegisterClasses(Game);
#if BIOSHOCK
	if (Game == GAME_Bioshock)
	{
		//!! should change this code!
		CTypeInfo::RemapProp("UShader", "Opacity", "Opacity_Bio"); //!!
	}
#endif // BIOSHOCK
}

/*-----------------------------------------------------------------------------
	Usage information
-----------------------------------------------------------------------------*/

static void PrintUsage()
{
	appPrintf(
			"UE viewer / exporter\n"
			"Usage: umodel [command] [options] <package> [<object> [<class>]]\n"
			"\n"
			"    <package>       name of package to load, without file extension\n"
			"    <object>        name of object to load\n"
			"    <class>         class of object to load (useful, when trying to load\n"
			"                    object with ambiguous name)\n"
			"\n"
			"Commands:\n"
			"    -view           (default) visualize object; when no <object> specified\n"
			"                    will load whole package\n"
			"    -list           list contents of package\n"
			"    -export         export specified object or whole package\n"
			"    -taglist        list of tags to override game autodetection\n"
			"    -version        display umodel version information\n"
			"    -help           display this help page\n"
			"\n"
			"Developer commands:\n"
			"    -log=file       write log to the specified file\n"
			"    -dump           dump object information to console\n"
			"    -pkginfo        load package and display its information\n"
#if SHOW_HIDDEN_SWITCHES
			"    -check          check some assumptions, no other actions performed\n"
#	if VSTUDIO_INTEGRATION
			"    -debug          invoke system crash handler on errors\n"
#	endif
#endif // SHOW_HIDDEN_SWITCHES
			"\n"
			"Options:\n"
			"    -path=PATH      path to game installation directory; if not specified,\n"
			"                    program will search for packages in current directory\n"
			"    -game=tag       override game autodetection (see -taglist for variants)\n"
			"    -pkg=package    load extra package (in addition to <package>)\n"
			"    -obj=object     specify object(s) to load\n"
#if HAS_UI
			"    -gui            force startup UI to appear\n" //?? debug-only option?
#endif
			"\n"
			"Compatibility options:\n"
			"    -nomesh         disable loading of SkeletalMesh classes in a case of\n"
			"                    unsupported data format\n"
			"    -noanim         disable loading of MeshAnimation classes\n"
			"    -nostat         disable loading of StaticMesh class\n"
			"    -notex          disable loading of Material classes\n"
			"    -nolightmap     disable loading of Lightmap textures\n"
			"    -sounds         allow export of sounds\n"
			"    -3rdparty       allow 3rd party asset export (ScaleForm, FaceFX)\n"
			"    -lzo|lzx|zlib   force compression method for fully-compressed packages\n"
			"\n"
			"Platform selection:\n"
			"    -ps3            override platform autodetection to PS3\n"
			"    -ios            set platform to iOS (iPhone/iPad)\n"
			"\n"
			"Viewer options:\n"
			"    -meshes         view meshes only\n"
			"    -materials      view materials only (excluding textures)\n"
			"    -anim=<set>     specify AnimSet to automatically attach to mesh\n"
 			"\n"
			"Export options:\n"
			"    -out=PATH       export everything into PATH instead of the current directory\n"
			"    -all            export all linked objects too\n"
			"    -uncook         use original package name as a base export directory\n"
			"    -groups         use group names instead of class names for directories\n"
			"    -uc             create unreal script when possible\n"
//			"    -pskx           use pskx format for skeletal mesh\n"
			"    -md5            use md5mesh/md5anim format for skeletal mesh\n"
			"    -lods           export all available mesh LOD levels\n"
			"    -dds            export textures in DDS format whenever possible\n"
			"    -notgacomp      disable TGA compression\n"
			"    -nooverwrite    prevent existing files from being overwritten (better\n"
			"                    performance)\n"
			"\n"
			"Supported resources for export:\n"
			"    SkeletalMesh    exported as ActorX psk file or MD5Mesh\n"
			"    MeshAnimation   exported as ActorX psa file or MD5Anim\n"
			"    VertMesh        exported as Unreal 3d file\n"
			"    StaticMesh      exported as psk file with no skeleton (pskx)\n"
			"    Texture         exported in tga or dds format\n"
			"    Sounds          file extension depends on object contents\n"
			"    ScaleForm       gfx\n"
			"    FaceFX          fxa\n"
			"    Sound           exported \"as is\"\n"
			"\n"
			"List of supported games:\n"
	);

	PrintGameList();

	appPrintf(
			"\n"
			"For details and updates please visit " HOMEPAGE "\n"
	);
}


static void PrintVersionInfo()
{
	appPrintf(
			"UE viewer (UMODEL)\n"
			"This version was built " __DATE__ "\n"
			"(c)2007-2014 Konstantin Nosov (Gildor)\n"
			HOMEPAGE "\n"
	);
}


/*-----------------------------------------------------------------------------
	Package helpers
-----------------------------------------------------------------------------*/

// Export all loaded objects.
#if HAS_UI
static bool ExportObjects(const TArray<UObject*> *Objects = NULL, UIProgressDialog* progress = NULL)
#else
static bool ExportObjects(const TArray<UObject*> *Objects = NULL)
#endif
{
	guard(ExportObjects);

	appPrintf("Exporting objects ...\n");

	appSetBaseExportDirectory(GSettings.ExportPath);

	// export object(s), if possible
	UnPackage* notifyPackage = NULL;
	bool hasObjectList = (Objects != NULL) && Objects->Num();

	//?? when 'Objects' passed, probably iterate over that list instead of GObjObjects
	for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
	{
#if HAS_UI
		if (progress && !progress->Tick()) return false;
#endif
		UObject* ExpObj = UObject::GObjObjects[idx];
		bool objectSelected = !hasObjectList || (Objects->FindItem(ExpObj) >= 0);

		if (!objectSelected) continue;

		if (notifyPackage != ExpObj->Package)
		{
			notifyPackage = ExpObj->Package;
			appSetNotifyHeader(notifyPackage->Filename);
		}

		bool done = ExportObject(ExpObj);

		if (!done && hasObjectList)
		{
			// display warning message only when failed to export object, specified from command line
			appPrintf("ERROR: Export object %s: unsupported type %s\n", ExpObj->Name, ExpObj->GetClassName());
		}
	}

	return true;

	unguard;
}


struct ClassStats
{
	const char*	Name;
	int			Count;

	ClassStats()
	{}

	ClassStats(const char* name)
	:	Name(name)
	,	Count(0)
	{}
};

static int CompareClassStats(const ClassStats* p1, const ClassStats* p2)
{
	return stricmp(p1->Name, p2->Name);
}

void DisplayPackageStats(const TArray<UnPackage*> &Packages)
{
	guard(DisplayPackageStats);

	TArray<ClassStats> stats;
	stats.Empty(256);

	for (int i = 0; i < Packages.Num(); i++)
	{
		UnPackage* pkg = Packages[i];
		for (int j = 0; j < pkg->Summary.ExportCount; j++)
		{
			const FObjectExport &Exp = pkg->ExportTable[j];
			const char* className = pkg->GetObjectName(Exp.ClassIndex);
			ClassStats* found = NULL;
			for (int k = 0; k < stats.Num(); k++)
				if (stats[k].Name == className)
				{
					found = &stats[k];
					break;
				}
			if (!found)
				found = new (stats) ClassStats(className);
			found->Count++;
		}
	}
	stats.Sort(CompareClassStats);
	appPrintf("Class statistics:\n");
	for (int i = 0; i < stats.Num(); i++)
		appPrintf("%5d %s\n", stats[i].Count, stats[i].Name);

	unguard;
}


/*-----------------------------------------------------------------------------
	Object visualizer support
-----------------------------------------------------------------------------*/

#if RENDERING

static bool CreateVisualizer(UObject *Obj, bool test = false);

inline bool ObjectSupported(UObject *Obj)
{
	return CreateVisualizer(Obj, true);
}

// index of the current object in UObject::GObjObjects array
static int ObjIndex = 0;

// dir = 1 - forward direction for search, dir = -1 - backward.
// When forceVisualizer is true, dummy visualizer will be created if no supported object found
static bool FindObjectAndCreateVisualizer(int dir, bool forceVisualizer = false, bool newPackage = false)
{
	if (newPackage)
	{
		assert(dir > 0); // just in case
		ObjIndex = -1;
	}

	int looped = 0;
	UObject *Obj;
	while (true)
	{
		if (dir > 0)
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
		if (looped > 1 || UObject::GObjObjects.Num() == 0)
		{
			if (forceVisualizer)
			{
				CreateVisualizer(NULL);
				appPrintf("\nThe specified package(s) has no supported objects.\n\n");
				DisplayPackageStats(GFullyLoadedPackages);
				return true;
			}
			return false;
		}
		Obj = UObject::GObjObjects[ObjIndex];
		if (ObjectSupported(Obj))
			break;
	}
	// change visualizer
	CreateVisualizer(Obj);
	return true;
}

#if HAS_UI
static UIPackageDialog GPackageDialog;

static HWND GetSDLWindowHandle(SDL_Window* window)
{
	if (!window) return 0;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(window, &info);
	return info.info.win.window;
}

// This function will return 'false' when dialog has popped up and cancelled. If
// user performs some action, and then pop up the dialog again - the function will
// always return true.
static bool ShowPackageUI()
{
	UIBaseDialog::SetMainWindow(GetSDLWindowHandle(GApplication.GetWindow()));

	static bool firstDialogCancelled = true;

	// When we're doing export, then switching back to GUI, then pressing "Esc",
	// we can't return to the visualizer which was used before doing export because
	// all object was unloaded. In this case, code will set 'packagesChanged' flag
	// to true, causing re-initialization of browser list.
	bool packagesChanged = false;

	while (true)
	{
		UIPackageDialog::EResult mode = GPackageDialog.Show();
		if (mode == UIPackageDialog::CANCEL)
		{
			if (packagesChanged)
				FindObjectAndCreateVisualizer(1, true, true);
			return !firstDialogCancelled;
		}

		UIProgressDialog progress;
		progress.Show(mode == UIPackageDialog::EXPORT ? "Exporting packages" : "Loading packages");
		bool cancelled = false;

		progress.SetDescription("Scanning package");
		TStaticArray<UnPackage*, 256> Packages;
		for (int i = 0; i < GPackageDialog.SelectedPackages.Num(); i++)
		{
			const char* pkgName = *GPackageDialog.SelectedPackages[i];
			if (!progress.Progress(pkgName, i, GPackageDialog.SelectedPackages.Num()))
			{
				cancelled = true;
				break;
			}
			UnPackage* package = UnPackage::LoadPackage(pkgName);	// should always return non-NULL
			if (package) Packages.AddItem(package);
		}
		if (cancelled)
		{
			progress.CloseDialog();
			continue;
		}

		if (!Packages.Num()) break;			// should not happen

		firstDialogCancelled = false;

		// register exporters and classes (will be performed only once); use any package
		// to detect an engine version
		InitClassAndExportSystems(Packages[0]->Game);

		// here we're in visualize mode

		// check whether we need to perform package unloading
		bool needReload = false;
		for (int i = 0; i < GFullyLoadedPackages.Num(); i++)
		{
			if (Packages.FindItem(GFullyLoadedPackages[i]) < 0)
			{
				// One of currently loaded packages is not needed anymore. We can't safely
				// unload only one package because it could be linked by other loaded packages.
				// So, unload everything.
				needReload = true;
				break;
			}
		}

		if (needReload || mode == UIPackageDialog::EXPORT)
		{
			// destroy a viewer before releasing packages
			CSkelMeshViewer::UntagAllMeshes();
			delete Viewer;
			Viewer = NULL;

			packagesChanged = true;
			ReleaseAllObjects();
		}

		if (mode == UIPackageDialog::EXPORT)
		{
			progress.SetDescription("Exporting package");
			// for each package: load a package, export, then release
			for (int i = 0; i < Packages.Num(); i++)
			{
				UnPackage* package = Packages[i];
				if (!progress.Progress(package->Name, i, Packages.Num()))
				{
					cancelled = true;
					break;
				}
				if (!LoadWholePackage(package, &progress))
				{
					cancelled = true;
					break;
				}
				if (!ExportObjects(NULL, &progress))
				{
					cancelled = true;
					break;
				}
				ReleaseAllObjects();
			}
			// cleanup
			//!! unregister all exported objects
			if (cancelled)
			{
				ReleaseAllObjects();
				//!! message box
				appPrintf("Operation interrupted by user.\n");
			}
			progress.CloseDialog();
			continue;		// after export, show the dialog again
		}

		// fully load all selected packages
		progress.SetDescription("Loading package");
		for (int i = 0; i < Packages.Num(); i++)
		{
			UnPackage* package = Packages[i];
			if (!progress.Progress(package->Name, i, Packages.Num()))
			{
				cancelled = true;
				break;
			}
			if (!LoadWholePackage(package, &progress))
			{
				cancelled = true;
				break;
			}
		}

		if (cancelled)
		{
			//!! message box
			appPrintf("Operation interrupted by user.\n");
		}

		progress.CloseDialog();

		if (packagesChanged || !Viewer)
		{
			FindObjectAndCreateVisualizer(1, true, true);
			packagesChanged = false;
		}
		break;
	}

	return true;
}

#endif // HAS_UI

#else // RENDERING

inline bool ObjectSupported(UObject *Obj)
{
	return true;
}

#endif // RENDERING


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

struct OptionInfo
{
	const char	*name;
	byte		*variable;
	byte		value;
};

static bool ProcessOption(const OptionInfo *Info, int Count, const char *Option)
{
	for (int i = 0; i < Count; i++)
	{
		const OptionInfo& c = Info[i];
		if (stricmp(c.name, Option) != 0) continue;
		*c.variable = c.value;
		return true;
	}
	return false;
}

static void SetPathOption(FString& where, const char* value)
{
	// determine whether absolute path is used
#if _WIN32
	int isAbsPath = (value[0] != 0) && (value[1] == ':');
#else
	int isAbsPath = (value[0] == '~' || value[0] == '/');
#endif
	if (isAbsPath)
	{
		where = value;
		return;
	}
	char path[512];
	getcwd(ARRAY_ARG(path));

	if (!value || !value[0])
	{
		where = path;
		return;
	}

	char buffer[512];
	appSprintf(ARRAY_ARG(buffer), "%s/%s", path, value);
	where = buffer;
}

// Display error message about wrong command line and then exit.
static void CommandLineError(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

	appPrintf("%s\nTry \"umodel -help\" for more information.\n", buf);
	exit(1);
}


#define OPT_BOOL(name,var)				{ name, (byte*)&var, true  },
#define OPT_NBOOL(name,var)				{ name, (byte*)&var, false },
#define OPT_VALUE(name,var,value)		{ name, (byte*)&var, value },

int main(int argc, char **argv)
{
	appInitPlatform();

#if DO_GUARD
	TRY {
#endif

	guard(Main);

	// display usage
#if !HAS_UI
	if (argc < 2)
	{
		PrintUsage();
		exit(0);
	}
#endif // HAS_UI

	// parse command line
	enum
	{
		CMD_View,
		CMD_Dump,
		CMD_Check,
		CMD_PkgInfo,
		CMD_List,
		CMD_Export,
	};

	static byte mainCmd = CMD_View;
	static bool exprtAll = false, hasRootDir = false, forceUI = false;
	TArray<const char*> extraPackages, objectsToLoad;
	TArray<const char*> params;
	const char *attachAnimName = NULL;
	for (int arg = 1; arg < argc; arg++)
	{
		const char *opt = argv[arg];
		if (opt[0] != '-')
		{
			params.AddItem(opt);
			continue;
		}

		opt++;			// skip '-'
		// simple options
		static const OptionInfo options[] =
		{
			OPT_VALUE("view",    mainCmd, CMD_View)
			OPT_VALUE("dump",    mainCmd, CMD_Dump)
			OPT_VALUE("check",   mainCmd, CMD_Check)
			OPT_VALUE("export",  mainCmd, CMD_Export)
			OPT_VALUE("pkginfo", mainCmd, CMD_PkgInfo)
			OPT_VALUE("list",    mainCmd, CMD_List)
#if VSTUDIO_INTEGRATION
			OPT_BOOL ("debug",   GUseDebugger)
#endif
#if RENDERING
			OPT_BOOL ("meshes",    showMeshes)
			OPT_BOOL ("materials", showMaterials)
#endif
			OPT_BOOL ("all",     exprtAll)
			OPT_BOOL ("uncook",  GUncook)
			OPT_BOOL ("groups",  GUseGroups)
//			OPT_BOOL ("pskx",    GExportPskx)	// -- may be useful in a case of more advanced mesh format
			OPT_BOOL ("md5",     GSettings.ExportMd5Mesh)
			OPT_BOOL ("lods",    GExportLods)
			OPT_BOOL ("uc",      GExportScripts)
			// disable classes
			OPT_NBOOL("nomesh",  GSettings.UseSkeletalMesh)
			OPT_NBOOL("nostat",  GSettings.UseStaticMesh)
			OPT_NBOOL("noanim",  GSettings.UseAnimation)
			OPT_NBOOL("notex",   GSettings.UseTexture)
			OPT_NBOOL("nolightmap", GSettings.UseLightmapTexture)
			OPT_BOOL ("sounds",  GSettings.UseSound)
			OPT_BOOL ("dds",     GExportDDS)
			OPT_BOOL ("notgacomp", GNoTgaCompress)
			OPT_BOOL ("nooverwrite", GDontOverwriteFiles)
#if HAS_UI
			OPT_BOOL ("gui",     forceUI)
#endif
			// platform
			OPT_VALUE("ps3",     GSettings.Platform, PLATFORM_PS3)
			OPT_VALUE("ios",     GSettings.Platform, PLATFORM_IOS)
			// compression
			OPT_VALUE("lzo",     GSettings.PackageCompression, COMPRESS_LZO )
			OPT_VALUE("zlib",    GSettings.PackageCompression, COMPRESS_ZLIB)
			OPT_VALUE("lzx",     GSettings.PackageCompression, COMPRESS_LZX )
		};
		if (ProcessOption(ARRAY_ARG(options), opt))
			continue;
		// more complex options
		if (!strnicmp(opt, "log=", 4))
		{
			appOpenLogFile(opt+4);
		}
		else if (!strnicmp(opt, "path=", 5))
		{
			SetPathOption(GSettings.GamePath, opt+5);
			hasRootDir = true;
		}
		else if (!strnicmp(opt, "out=", 4))
		{
			SetPathOption(GSettings.ExportPath, opt+4);
		}
		else if (!strnicmp(opt, "game=", 5))
		{
			int tag = FindGameTag(opt+5);
			if (tag == -1)
			{
				appPrintf("ERROR: unknown game tag \"%s\". Use -taglist option to display available tags.\n", opt+5);
				exit(0);
			}
			GSettings.GameOverride = tag;
		}
		else if (!strnicmp(opt, "pkg=", 4))
		{
			const char *pkg = opt+4;
			extraPackages.AddItem(pkg);
		}
		else if (!strnicmp(opt, "obj=", 4))
		{
			const char *obj = opt+4;
			objectsToLoad.AddItem(obj);
		}
		else if (!strnicmp(opt, "anim=", 5))
		{
			const char *obj = opt+5;
			objectsToLoad.AddItem(obj);
			attachAnimName = obj;
		}
		else if (!stricmp(opt, "3rdparty"))
		{
			GSettings.UseScaleForm = GSettings.UseFaceFx = true;
		}
		// information commands
		else if (!stricmp(opt, "taglist"))
		{
			PrintGameList(true);
			return 0;
		}
		else if (!stricmp(opt, "help"))
		{
			PrintUsage();
			return 0;
		}
		else if (!stricmp(opt, "version"))
		{
			PrintVersionInfo();
			return 0;
		}
		else
		{
			CommandLineError("umodel: invalid option: %s", opt);
		}
	}

	const char *argPkgName   = (params.Num() >= 1) ? params[0] : NULL;
	const char *argObjName   = (params.Num() >= 2) ? params[1] : NULL;
	const char *argClassName = (params.Num() >= 3) ? params[2] : NULL;
	if (params.Num() > 3)
	{
		CommandLineError("umodel: too many arguments, please check your command line.\nYou specified: package=%s, object=%s, class=%s",
			argPkgName, argObjName, argClassName);
	}

	bool guiShown = false;
#if HAS_UI
	if (argc < 2 || (!hasRootDir && !argPkgName) || forceUI)
	{
		// fill game path with current directory, if it's empty - for easier work with UI
		if (GSettings.GamePath.IsEmpty())
			SetPathOption(GSettings.GamePath, "");
		//!! the same for -log option
		// no arguments provided - display startup options
		UIStartupDialog dialog(GSettings);
		bool res = dialog.Show();
		if (!res) exit(0);
		hasRootDir = true;
		guiShown = true;
	}
#endif // HAS_UI

	// apply some GSettings
	if (hasRootDir)
		appSetRootDirectory(GSettings.GamePath);
	GForceGame = GSettings.GameOverride;
	GForcePlatform = GSettings.Platform;
	GForceCompMethod = GSettings.PackageCompression;
	if (GSettings.ExportPath.IsEmpty())
		SetPathOption(GSettings.ExportPath, "UmodelExport");	//!! linux: ~/UmodelExport

	TArray<UnPackage*> Packages;
	TArray<UObject*> Objects;

#if !HAS_UI
	if (!argPkgName || !params.Num())
	{
		CommandLineError("umodel: package name was not specified.");
	}
#else
	if (!argPkgName)
	{
		if (!ShowPackageUI())
			return 0;		// user has cancelled the dialog when it appears for the first time

		guiShown = true;
		goto main_loop;
	}
#endif // !HAS_UI

	if (argObjName)
	{
		objectsToLoad.Insert(0);
		objectsToLoad[0] = argObjName;
	}

	// load the package

#if PROFILE
	appResetProfiler();
#endif

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// setup root directory
	if (!hasRootDir)
	{
		//!! - place this code before UIStartupDialog, set game path in options so UI
		//!!   could pick it up
		//!! - appSetRootDirectory2(): replace with appGetRootDirectoryFromFile() + appSetRootDirectory()
		if (strchr(argPkgName, '/') || strchr(argPkgName, '\\'))
		{
			// has path in filename
			appSetRootDirectory2(argPkgName);
		}
		else
		{
			// no path in filename
			appSetRootDirectory(".");		// scan for packages
		}
	}

	// load main package
	UnPackage *MainPackage = UnPackage::LoadPackage(argPkgName);
	if (!MainPackage)
	{
		appPrintf("ERROR: unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	if (mainCmd == CMD_List)
	{
		guard(List);
		// dump package exports table
		for (int i = 0; i < MainPackage->Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = MainPackage->ExportTable[i];
			appPrintf("%4d %8X %8X %s %s\n", i, Exp.SerialOffset, Exp.SerialSize, MainPackage->GetObjectName(Exp.ClassIndex), *Exp.ObjectName);
		}
		unguard;
		return 0;
	}

	// register exporters and classes
	InitClassAndExportSystems(MainPackage->Game);

	// preload all extra packages first
	Packages.AddItem(MainPackage);	// already loaded
	for (int i = 0; i < extraPackages.Num(); i++)
	{
		UnPackage *Package2 = UnPackage::LoadPackage(extraPackages[i]);
		if (!Package2)
			appPrintf("WARNING: unable to find/load package %s\n", extraPackages[i]);
		else
			Packages.AddItem(Package2);
	}

	if (mainCmd == CMD_PkgInfo)
	{
		DisplayPackageStats(Packages);
		return 0;					// already displayed when loaded package; extend it?
	}

	// get requested object info
	if (objectsToLoad.Num())
	{
		// selectively load objects
		int totalFound = 0;
		UObject::BeginLoad();
		for (int objIdx = 0; objIdx < objectsToLoad.Num(); objIdx++)
		{
			const char *objName   = objectsToLoad[objIdx];
			const char *className = (objIdx == 0) ? argClassName : NULL;
			int found = 0;
			for (int pkg = 0; pkg < Packages.Num(); pkg++)
			{
				UnPackage *Package2 = Packages[pkg];
				// load specific object(s)
				int idx = -1;
				while (true)
				{
					idx = Package2->FindExport(objName, className, idx + 1);
					if (idx == INDEX_NONE) break;		// not found in this package

					found++;
					totalFound++;
					appPrintf("Export \"%s\" was found in package \"%s\"\n", objName, Package2->Filename);

					const char *realClassName = Package2->GetObjectName(Package2->ExportTable[idx].ClassIndex);
					// create object from package
					UObject *Obj = Package2->CreateExport(idx);
					if (Obj)
					{
						Objects.AddItem(Obj);
						if (objName == attachAnimName && (Obj->IsA("MeshAnimation") || Obj->IsA("AnimSet")))
							GForceAnimSet = Obj;
					}
				}
				if (found) break;
			}
			if (!found)
			{
				appPrintf("Export \"%s\" was not found in specified package(s)\n", objName);
				exit(1);
			}
		}
		appPrintf("Found %d object(s)\n", totalFound);
		UObject::EndLoad();
	}
	else
	{
		// fully load all packages
		for (int pkg = 0; pkg < Packages.Num(); pkg++)
			LoadWholePackage(Packages[pkg]);
	}

	if (!UObject::GObjObjects.Num() && !guiShown)
	{
		appPrintf("\nThe specified package(s) has no supported objects.\n\n");
	no_objects:
		appPrintf("Selected package(s):\n");
		for (int i = 0; i < Packages.Num(); i++)
			appPrintf("  %s\n", Packages[i]->Filename);
		appPrintf("\n");
		// display list of classes
		DisplayPackageStats(Packages);
		return 0;
	}

#if PROFILE
	appPrintProfiler();
#endif

	if (mainCmd == CMD_Export)
	{
		ExportObjects(exprtAll ? NULL : &Objects);
		if (!guiShown)
			return 0;
		// switch to a viewer in GUI mode
		mainCmd = CMD_View;
	}

#if RENDERING
	if (mainCmd == CMD_Dump)
	{
		// dump object(s)
		for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
		{
			UObject* ExpObj = UObject::GObjObjects[idx];
			if (!exprtAll)
			{
				if (Packages.FindItem(ExpObj->Package) < 0)					// refine object by package
					continue;
				if (objectsToLoad.Num() && (Objects.FindItem(ExpObj) < 0))	// refine object by name
					continue;
			}

			CreateVisualizer(ExpObj);
			if (Viewer)
			{
				Viewer->Dump();								// dump info to console
				delete Viewer;
				Viewer = NULL;
			}
		}
		return 0;
	}

	// find any object to display
	if (!FindObjectAndCreateVisualizer(1, guiShown, true))
	{
		appPrintf("\nThe specified package(s) has no objects to diaplay.\n\n");
		goto no_objects;
	}

	// print mesh info
#	if TEST_FILES
	Viewer->Test();
#	endif

	if (mainCmd == CMD_View)
	{
	main_loop:
		// show object
		vpInvertXAxis = true;
		guard(MainLoop);
		VisualizerLoop(APP_CAPTION, &GApplication);
		unguard;
	}
#endif // RENDERING

	// cleanup
#if RENDERING
	delete Viewer;
#endif

#if DUMP_MEM_ON_EXIT
	appPrintf("Memory: allocated %d bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
	appDumpMemoryAllocations();
#endif
//	ReleaseAllObjects();

	unguard;

#if DO_GUARD
	} CATCH_CRASH {
		if (GErrorHistory[0])
		{
//			appPrintf("ERROR: %s\n", GErrorHistory);
			appNotify("ERROR: %s\n", GErrorHistory);
		}
		else
		{
//			appPrintf("Unknown error\n");
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

#if RENDERING

static bool CreateVisualizer(UObject *Obj, bool test)
{
	guard(CreateVisualizer);

	if (!test && Viewer)
	{
		if (Viewer->Object == Obj) return true;	// object is not changed
		delete Viewer;
		Viewer = NULL;
	}

	if (!Obj)
	{
		// dummy visualizer
		Viewer = new CObjectViewer(NULL);
		return true;
	}

	if (!test)
		appSetNotifyHeader("%s:  %s'%s'", Obj->Package->Filename, Obj->GetClassName(), Obj->Name);
	// create viewer class
#define CLASS_VIEWER(UClass, CViewer, extraCheck)	\
	if (Obj->IsA(#UClass + 1))						\
	{												\
		UClass *Obj2 = static_cast<UClass*>(Obj); 	\
		if (!(extraCheck)) return false;			\
		if (!test) Viewer = new CViewer(Obj2);		\
		return true;								\
	}
#define MESH_VIEWER(UClass, CViewer)				\
	if (Obj->IsA(#UClass + 1))						\
	{												\
		if (!test)									\
		{											\
			UClass *Obj2 = static_cast<UClass*>(Obj); \
			if (!Obj2->ConvertedMesh)				\
				Viewer = new CObjectViewer(Obj);	\
			else									\
				Viewer = new CViewer(Obj2->ConvertedMesh); \
		}											\
		return true;								\
	}
	// create viewer for known class
	bool showAll = !(showMeshes || showMaterials);
	if (showMeshes || showAll)
	{
		CLASS_VIEWER(UVertMesh,       CVertMeshViewer, true);
		MESH_VIEWER (USkeletalMesh,   CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh,     CStatMeshViewer      );
#if UNREAL3
		MESH_VIEWER (USkeletalMesh3,  CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh3,    CStatMeshViewer      );
#endif
	}
	if (showMaterials || showAll)
	{
		CLASS_VIEWER(UUnrealMaterial, CMaterialViewer, !showMaterials || !Obj2->IsTexture());
	}
	// fallback for unknown class
	if (!test)
	{
		Viewer = new CObjectViewer(Obj);
	}
	return false;
#undef CLASS_VIEWER
	unguardf("%s'%s'", Obj->GetClassName(), Obj->Name);
}


static void TakeScreenshot(const char *ObjectName, bool CatchAlpha)
{
	char filename[256];
	appSprintf(ARRAY_ARG(filename), "Screenshots/%s.tga", ObjectName);
	int retry = 1;
	while (true)
	{
		FILE *f = fopen(filename, "r");
		if (!f) break;
		fclose(f);
		// if file exists, append index
		retry++;
		appSprintf(ARRAY_ARG(filename), "Screenshots/%s_%02d.tga", ObjectName, retry);
	}
	appPrintf("Writting screenshot %s\n", filename);
	appMakeDirectoryForFile(filename);
	FFileWriter Ar(filename);
	int width, height;
	GetWindowSize(width, height);

	byte *pic = new byte [width * height * 4];
	glFinish();
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pic);

	if (CatchAlpha)
	{
		/*
		NOTES:
		- this will work in GL1 mode only, GL2 has depth buffer somewhere in CFramebuffer;
		  we are copying depth to the main framebuffer in bloom shader
		- rendering using black background for better semi-transparency
		- processing semi-transparency with alpha channel in framebuffer will not work because
		  some parts of image could have different blending modes (blend, add etc)
		- some translucent parts could be painted with no depthwrite, this will produce black
		  areas! (example: bloom has no depthwrite, it is screen-space effect)
		*/
		float *picDepth = new float [width * height];
		glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, picDepth);
		for (int i = 0; i < width * height; i++)
		{
			float v = picDepth[i];
			pic[i * 4 + 3] = (v == 1) ? 0 : 255;
		}
		delete picDepth;
	}

	WriteTGA(Ar, width, height, pic);
	delete pic;
}


static int GDoScreenshot = 0;

void CUmodelApp::Draw3D(float TimeDelta)
{
	UObject *Obj = (ObjIndex < UObject::GObjObjects.Num()) ? UObject::GObjObjects[ObjIndex] : NULL;

	guard(CUmodelApp::Draw3D);

	bool AlphaBgShot = GDoScreenshot >= 2;
	if (AlphaBgShot)
	{
		// screenshot with transparent background
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// draw the frame
	Viewer->Draw3D(TimeDelta);

	if (AlphaBgShot)
	{
		// take screenshot without 2D texts and with transparency
		TakeScreenshot(Obj->Name, true);
		GDoScreenshot = 0;
	}

	unguardf("Obj=%s'%s'", Obj ? Obj->GetClassName() : "None", Obj ? Obj->Name : "None");
}


void CUmodelApp::BeforeSwap()
{
	guard(CUmodelApp::BeforeSwap);

	if (GDoScreenshot)
	{
		// take regular screenshot
		UObject *Obj = UObject::GObjObjects[ObjIndex];
		TakeScreenshot(Obj->Name, false);
		GDoScreenshot = 0;
	}

	unguard;
}


void CUmodelApp::ProcessKey(int key, bool isDown)
{
	guard(CUmodelApp::ProcessKey);

	if (!isDown)
	{
		Viewer->ProcessKeyUp(key);
		return;
	}

	if (key == SPEC_KEY(PAGEDOWN) || key == SPEC_KEY(PAGEUP))
	{
		FindObjectAndCreateVisualizer((key == SPEC_KEY(PAGEDOWN)) ? 1 : -1);
		return;
	}
	if (key == ('s'|KEY_CTRL))
	{
		GDoScreenshot = 1;
		return;
	}
	if (key == ('s'|KEY_ALT))
	{
		GDoScreenshot = 2;
		return;
	}
#if HAS_UI
	if (key == 'o')
	{
		ShowPackageUI();
		return;
	}
#endif // HAS_UI
	Viewer->ProcessKey(key);

	unguard;
}


void CUmodelApp::DrawTexts(bool helpVisible)
{
	guard(CUmodelApp::DrawTexts);
	CApplication::DrawTexts(helpVisible);
	if (helpVisible)
	{
		DrawKeyHelp("PgUp/PgDn", "browse objects");
#if HAS_UI
		DrawKeyHelp("O",         "open package");
#endif
		DrawKeyHelp("Ctrl+S",    "take screenshot");
		Viewer->ShowHelp();
		DrawTextLeft("-----\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}

#endif // RENDERING
