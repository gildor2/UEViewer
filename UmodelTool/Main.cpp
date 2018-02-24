#include "Core.h"

#if _WIN32
#include <direct.h>					// getcwd
#include <signal.h>					// abort handler
#else
#include <unistd.h>					// getcwd
#endif

// Classes for registration
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnAnimNotify.h"

#include "UnMaterial2.h"
#include "UnMaterial3.h"

#include "UnMesh2.h"
#include "UnMesh3.h"
#include "UnMesh4.h"

#include "UnSound.h"
#include "UnThirdParty.h"

#include "Exporters/Exporters.h"

#if DECLARE_VIEWER_PROPS
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#endif

#include "GameDatabase.h"
#include "PackageUtils.h"

#include "UmodelApp.h"
#include "Version.h"
#include "MiscStrings.h"

#define APP_CAPTION					"UE Viewer"

//#define SHOW_HIDDEN_SWITCHES		1
//#define DUMP_MEM_ON_EXIT			1


UmodelSettings GSettings;


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
#if MKVSDC
	REGISTER_MESH_CLASSES_MK
#endif
END_CLASS_TABLE
#endif // UNREAL3
}


static void RegisterUnrealClasses4()
{
#if UNREAL4
BEGIN_CLASS_TABLE
	REGISTER_MESH_CLASSES_U4
	REGISTER_MATERIAL_CLASSES_U4
END_CLASS_TABLE
	REGISTER_MATERIAL_ENUMS_U4
#endif // UNREAL4
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
#if UNREAL4
	REGISTER_SOUND_CLASSES_UE4
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
	else if (game < GAME_UE4_BASE)
	{
		RegisterUnrealClasses3();
		RegisterUnreal3rdPartyClasses();
	}
	else
	{
		RegisterUnrealClasses4();
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

#if UNREAL4
static void ExportSkeletalMesh4(const USkeletalMesh4 *Mesh)
{
	assert(Mesh->ConvertedMesh);
	if (!GSettings.ExportMd5Mesh)
		ExportPsk(Mesh->ConvertedMesh);
	else
		ExportMd5Mesh(Mesh->ConvertedMesh);
}

static void ExportStaticMesh4(const UStaticMesh4 *Mesh)
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

#if UNREAL4
static void ExportSkeleton(const USkeleton *Skeleton)
{
	assert(Skeleton->ConvertedAnim);
	if (!GSettings.ExportMd5Mesh)
		ExportPsa(Skeleton->ConvertedAnim);
	else
		ExportMd5Anim(Skeleton->ConvertedAnim);
}
#endif // UNREAL4

static void RegisterExporters()
{
	RegisterExporter("SkeletalMesh",  ExportSkeletalMesh2);
	RegisterExporter("MeshAnimation", ExportMeshAnimation);
#if UNREAL3
	RegisterExporter("SkeletalMesh3", ExportSkeletalMesh3);
	RegisterExporter("AnimSet",       ExportAnimSet      );
#endif
	RegisterExporter("VertMesh",      Export3D           );
	RegisterExporter("StaticMesh",    ExportStaticMesh2  );
	RegisterExporter("Texture",       ExportTexture      );
	RegisterExporter("Sound",         ExportSound        );
#if UNREAL3
	RegisterExporter("StaticMesh3",   ExportStaticMesh3  );
	RegisterExporter("Texture2D",     ExportTexture      );
	RegisterExporter("SoundNodeWave", ExportSoundNodeWave);
	RegisterExporter("SwfMovie",      ExportGfx          );
	RegisterExporter("FaceFXAnimSet", ExportFaceFXAnimSet);
	RegisterExporter("FaceFXAsset",   ExportFaceFXAsset  );
#endif // UNREAL3
#if UNREAL4
	RegisterExporter("SkeletalMesh4", ExportSkeletalMesh4);
	RegisterExporter("StaticMesh4",   ExportStaticMesh4  );
	RegisterExporter("Skeleton",      ExportSkeleton     );
	RegisterExporter("SoundWave",     ExportSoundWave4   );
#endif // UNREAL4
	RegisterExporter("UnrealMaterial", ExportMaterial    );			// register this after Texture/Texture2D exporters
}


/*-----------------------------------------------------------------------------
	Initialization of class and export systems
-----------------------------------------------------------------------------*/

void InitClassAndExportSystems(int Game)
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
#if HAS_UI
			"       umodel [command] [options] <directory>\n"
#endif
			"\n"
			"    <package>       name of package to load - this could be a file name\n"
			"                    with or without extension, or wildcard\n"
			"    <object>        name of object to load\n"
			"    <class>         class of object to load (useful, when trying to load\n"
			"                    object with ambiguous name)\n"
#if HAS_UI
			"    <directory>     path to the game (see -path option)\n"
#endif
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
			"    -pkgver=nnn     override package version (advanced option!)\n"
			"    -pkg=package    load extra package (in addition to <package>)\n"
			"    -obj=object     specify object(s) to load\n"
#if HAS_UI
			"    -gui            force startup UI to appear\n" //?? debug-only option?
#endif
			"    -aes=key        provide AES decryption key for encrypted pak files,\n"
			"                    key is ASCII or hex string (hex format is 0xAABBCCDD)\n"
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
			"    -android        set platform to Android\n"
			"\n"
			"Viewer options:\n"
			"    -meshes         view meshes only\n"
			"    -materials      view materials only (excluding textures)\n"
			"    -anim=<set>     specify AnimSet to automatically attach to mesh\n"
 			"\n"
			"Export options:\n"
			"    -out=PATH       export everything into PATH instead of the current directory\n"
			"    -all            export all linked objects too\n"
			"    -uncook         use original package name as a base export directory (UE1-3)\n"
			"    -groups         use group names instead of class names for directories (UE1-3)\n"
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
			"For details and updates please visit %s\n", GUmodelHomepage
	);
}


static void PrintVersionInfo()
{
	appPrintf(
			"UE Viewer (UModel)\n" "%s\n" "%s\n" "%s\n",
			GBuildString, GCopyrightString, GUmodelHomepage
	);
}


/*-----------------------------------------------------------------------------
	Package helpers
-----------------------------------------------------------------------------*/

// Export all loaded objects.
bool ExportObjects(const TArray<UObject*> *Objects, IProgressCallback* progress)
{
	guard(ExportObjects);

	appPrintf("Exporting objects ...\n");

	// export object(s), if possible
	UnPackage* notifyPackage = NULL;
	bool hasObjectList = (Objects != NULL) && Objects->Num();

	//?? when 'Objects' passed, probably iterate over that list instead of GObjObjects
	for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
	{
		if (progress && !progress->Tick()) return false;
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
	const char* value2;

#if _WIN32
	int isAbsPath = (value[0] != 0) && (value[1] == ':');
#else
	int isAbsPath = (value[0] == '~' || value[0] == '/');
#endif
	if (isAbsPath)
	{
		value2 = value;
	}
	else
	{
		// relative path
		char path[512];
		if (!getcwd(ARRAY_ARG(path)))
			strcpy(path, ".");	// path is too long, or other error occured

		if (!value || !value[0])
		{
			value2 = path;
		}
		else
		{
			char buffer[512];
			appSprintf(ARRAY_ARG(buffer), "%s/%s", path, value);
			value2 = buffer;
		}
	}

	char finalName[512];
	appStrncpyz(finalName, value2, ARRAY_COUNT(finalName)-1);
	appNormalizeFilename(finalName);

	where = finalName;

	// strip possible trailing double quote
	int len = where.Len();
	if (len > 0 && where[len-1] == '"')
		where.RemoveAt(len-1);
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

	appPrintf("UModel: bad command line: %s\nTry \"umodel -help\" for more information.\n", buf);
	exit(1);
}

// forward
void UISetExceptionHandler(void (*Handler)());

static void ExceptionHandler()
{
	FFileWriter::CleanupOnError();
	if (GErrorHistory[0])
	{
//		appPrintf("ERROR: %s\n", GErrorHistory);
		appNotify("ERROR: %s\n", GErrorHistory);
	}
	else
	{
//		appPrintf("Unknown error\n");
		appNotify("Unknown error\n");
	}
	#if HAS_UI
	if (GApplication.GuiShown)
		GApplication.ShowErrorDialog();
	#endif // HAS_UI
	exit(1);
}

#if _WIN32
// AbortHandler on linux will cause infinite recurse, but works well on Windows
static void AbortHandler(int signal)
{
	appError("abort() called");
}
#endif

#if UNREAL4

int UE4UnversionedPackage(int verMin, int verMax)
{
#if HAS_UI
	int version = GApplication.ShowUE4UnversionedPackageDialog(verMin, verMax);
	if (version >= 0) return version;
#endif
	appError("Unversioned UE4 packages are not supported. Please restart UModel and select UE4 version in range %d-%d using UI or command line.", verMin, verMax);
	return -1;
}

static void CheckHexAesKey()
{
#define ishex(c)		( (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') )
#define hextodigit(c)	( (c >= 'a') ? c - 'a' + 10 : c - '0' )

	if (GAesKey.Len() < 3) return;
	const char* s = *GAesKey;

	// Hex key starts with "0x"
	if (*s++ != '0') return;
	if (tolower(*s++) != 'x') return;

	FString NewKey;
	NewKey.Empty(GAesKey.Len() / 2 + 1);

	int remains = GAesKey.Len() - 2;
	while (remains > 0)
	{
		uint8 b = 0;
		if ((remains & 1) == 0)
		{
			// this code will not be executed only in a case of odd character count, for the first char
			char c = tolower(*s++);
			if (!ishex(c)) return;
			b = hextodigit(c) << 4;
			remains--;
		}
		char c = tolower(*s++);
		if (!ishex(c)) return;
		b |= hextodigit(c);
		remains--;

		NewKey.AppendChar((char)b);
	}

	GAesKey = NewKey;
}

bool UE4EncryptedPak()
{
#if HAS_UI
	GAesKey = GApplication.ShowUE4AesKeyDialog();
	CheckHexAesKey();
	return GAesKey.Len() > 0;
#else
	return false;
#endif
}

#endif // UNREAL4


#define OPT_BOOL(name,var)				{ name, (byte*)&var, true  },
#define OPT_NBOOL(name,var)				{ name, (byte*)&var, false },
#define OPT_VALUE(name,var,value)		{ name, (byte*)&var, value },

int main(int argc, char **argv)
{
	appInitPlatform();

#if DO_GUARD
	TRY {
#endif

#if _WIN32
	signal(SIGABRT, AbortHandler);
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
	TArray<const char*> packagesToLoad, objectsToLoad;
	TArray<const char*> params;
	const char *attachAnimName = NULL;
	for (int arg = 1; arg < argc; arg++)
	{
		const char *opt = argv[arg];
		if (opt[0] != '-')
		{
			params.Add(opt);
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
			OPT_BOOL ("meshes",    GApplication.ShowMeshes)
			OPT_BOOL ("materials", GApplication.ShowMaterials)
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
			OPT_VALUE("android", GSettings.Platform, PLATFORM_ANDROID)
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
		else if (!strnicmp(opt, "pkgver=", 7))
		{
			int ver = atoi(opt+7);
			if (ver < 1)
			{
				appPrintf("ERROR: pkgver number is not valid: %s\n", opt+7);
				exit(0);
			}
			GForcePackageVersion = ver;
		}
		else if (!strnicmp(opt, "pkg=", 4))
		{
			const char *pkg = opt+4;
			packagesToLoad.Add(pkg);
		}
		else if (!strnicmp(opt, "obj=", 4))
		{
			const char *obj = opt+4;
			objectsToLoad.Add(obj);
		}
		else if (!strnicmp(opt, "anim=", 5))
		{
			const char *obj = opt+5;
			objectsToLoad.Add(obj);
			attachAnimName = obj;
		}
		else if (!stricmp(opt, "3rdparty"))
		{
			GSettings.UseScaleForm = GSettings.UseFaceFx = true;
		}
		else if (!strnicmp(opt, "aes=", 4))
		{
			GAesKey = opt+4;
			CheckHexAesKey();
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
			CommandLineError("invalid option: -%s", opt);
		}
	}

	// Parse UMODEL [package_name [obj_name [class_name]]]
	const char *argPkgName   = (params.Num() >= 1) ? params[0] : NULL;
	const char *argObjName   = (params.Num() >= 2) ? params[1] : NULL;
	const char *argClassName = (params.Num() >= 3) ? params[2] : NULL;
	if (params.Num() > 3)
	{
		CommandLineError("too many arguments, please check your command line.\nYou specified: package=%s, object=%s, class=%s",
			argPkgName, argObjName, argClassName);
	}

#if HAS_UI
	if (argPkgName && !argObjName && !argClassName && !hasRootDir)
	{
		// only 1 parameter has been specified - check if this is a directory name
		// note: this is only meaningful for UI version of umodel, because there's nothing to
		// do with directory without UI
		if (appGetFileType(argPkgName) == FS_DIR)
		{
			SetPathOption(GSettings.GamePath, argPkgName);
			hasRootDir = true;
			argPkgName = NULL;
		}
	}

	if (argc < 2 || (!hasRootDir && !argPkgName) || forceUI)
	{
		// fill game path with current directory, if it's empty - for easier work with UI
		if (GSettings.GamePath.IsEmpty())
			SetPathOption(GSettings.GamePath, "");
		//!! the same for -log option
		// no arguments provided - display startup options
		bool res = GApplication.ShowStartupDialog(GSettings);
		if (!res) exit(0);
		hasRootDir = true;
	}
#endif // HAS_UI

	// apply some GSettings
	GForceGame = GSettings.GameOverride;	// force game fore scanning any game files
	if (hasRootDir)
		appSetRootDirectory(*GSettings.GamePath);
	GForcePlatform = GSettings.Platform;
	GForceCompMethod = GSettings.PackageCompression;
	if (GSettings.ExportPath.IsEmpty())
		SetPathOption(GSettings.ExportPath, "UmodelExport");	//!! linux: ~/UmodelExport
	appSetBaseExportDirectory(*GSettings.ExportPath);

	TArray<UnPackage*> Packages;
	TArray<UObject*> Objects;

	if (argPkgName)
	{
		packagesToLoad.Insert(argPkgName, 0);
		// don't use argPkgName beyond this point
	}
	if (argObjName)
	{
		objectsToLoad.Insert(argObjName, 0);
		// don't use argObjName beyond this point
	}

#if !HAS_UI
	if (!packagesToLoad.Num() || !params.Num())
	{
		CommandLineError("package name was not specified.");
	}
#endif // !HAS_UI

	// load the package

#if PROFILE
	appResetProfiler();
#endif

	// setup NotifyInfo to describe package only
	if (packagesToLoad.Num() == 1)
		appSetNotifyHeader(argPkgName);

	// setup root directory
	if (!hasRootDir && packagesToLoad.Num())
	{
		//!! - place this code before UIStartupDialog, set game path in options so UI
		//!!   could pick it up
		//!! - appSetRootDirectory2(): replace with appGetRootDirectoryFromFile() + appSetRootDirectory()
		const char* packageName = packagesToLoad[0];
		if (strchr(packageName, '/') || strchr(packageName, '\\'))
		{
			// has path in filename
			appSetRootDirectory2(packageName);
		}
		else
		{
			// no path in filename
			appSetRootDirectory(".");		// scan for packages
		}
	}
	else if (!hasRootDir)
	{
		// no packages were specified
		appSetRootDirectory(".");			// scan for packages
	}

	// Try to load all packages first.
	// Note: in this code, packages will be loaded without creating any exported objects.
	for (int i = 0; i < packagesToLoad.Num(); i++)
	{
//		UnPackage *Package = UnPackage::LoadPackage(packagesToLoad[i]);
		TStaticArray<const CGameFileInfo*, 32> Files;
		appFindGameFiles(packagesToLoad[i], Files);

		if (!Files.Num())
		{
			appPrintf("WARNING: unable to find package %s\n", packagesToLoad[i]);
		}
		else
		{
			for (int j = 0; j < Files.Num(); j++)
			{
				UnPackage* Package = UnPackage::LoadPackage(Files[j]->RelativeName);
				Packages.Add(Package);
			}
		}
	}

#if !HAS_UI
	if (!Packages.Num())
	{
		CommandLineError("failed to load provided packages");
	}
#else
	if (!Packages.Num())
	{
		if (mainCmd != CMD_View)
		{
			CommandLineError("failed to load provided packages");
		}
		else
		{
			// open package selection UI
			if (!GApplication.ShowPackageUI())
				return 0;		// user has cancelled the dialog when it appears for the first time
			goto main_loop;
		}
	}
#endif // HAS_UI

	if (mainCmd == CMD_List)
	{
		guard(List);
		// dump package exports table
		UnPackage* MainPackage = Packages[0];	//!! TODO: may be work with multiple packages here - not hard, but will require additional output formatting
		for (int i = 0; i < MainPackage->Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = MainPackage->ExportTable[i];
			appPrintf("%4d %8X %8X %s %s\n", i, Exp.SerialOffset, Exp.SerialSize, MainPackage->GetObjectName(Exp.ClassIndex), *Exp.ObjectName);
		}
		unguard;
		return 0;
	}

	// register exporters and classes
	InitClassAndExportSystems(Packages[0]->Game);

	if (mainCmd == CMD_PkgInfo)
	{
		DisplayPackageStats(Packages);
		return 0;					// already displayed when loaded package; extend it?
	}

	// load requested objects if any, or fully load everything
	UObject::BeginLoad();
	if (objectsToLoad.Num())
	{
		// selectively load objects
		int totalFound = 0;
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

					// create object from package
					UObject *Obj = Package2->CreateExport(idx);
					if (Obj)
					{
						Objects.Add(Obj);
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
	}
	else
	{
		// fully load all packages
		for (int pkg = 0; pkg < Packages.Num(); pkg++)
			LoadWholePackage(Packages[pkg]);
	}
	UObject::EndLoad();

	if (!UObject::GObjObjects.Num() && !GApplication.GuiShown)
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
		ResetExportedList();
		if (!GApplication.GuiShown)
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

			GApplication.CreateVisualizer(ExpObj);
			if (GApplication.Viewer)
				GApplication.Viewer->Dump();								// dump info to console
		}
		return 0;
	}

	// find any object to display
	if (!GApplication.FindObjectAndCreateVisualizer(1, GApplication.GuiShown, true))	//!! don't need to pass GuiShown there
	{
		appPrintf("\nThe specified package(s) has no objects to diaplay.\n\n");
		goto no_objects;
	}

	// print mesh info
#	if TEST_FILES
	GApplication.Viewer->Test();
#	endif

	if (mainCmd == CMD_View)
	{
#if HAS_UI
		// Put argPkgName into package selection dialog, so when opening a package window for the first
		// time, currently opened package will be selected
		if (!GApplication.GuiShown) GApplication.SetPackage(Packages[0]);
#endif // HAS_UI
	main_loop:
		// show object
		vpInvertXAxis = true;
	#if HAS_UI
		UISetExceptionHandler(ExceptionHandler);
	#endif
		GApplication.VisualizerLoop(APP_CAPTION);
	}
#endif // RENDERING

//	ReleaseAllObjects();
#if DUMP_MEM_ON_EXIT
	//!! note: CUmodelApp is not destroyed here
	appPrintf("Memory: allocated " FORMAT_SIZE("d") " bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
	appDumpMemoryAllocations();
#endif

	unguardf("umodel_build=%s", STR(GIT_REVISION));	// using string constant to allow non-git builds (with GIT_REVISION 'unknown')

#if DO_GUARD
	} CATCH_CRASH {
		ExceptionHandler();
	}
#endif
	return 0;
}
