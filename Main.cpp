#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnAnimNotify.h"

#include "UnMaterial2.h"
#include "UnMaterial3.h"

#include "UnMesh.h"
#include "UnMesh2.h"
#include "UnMesh3.h"

#include "UnSound.h"
#include "UnThirdParty.h"

#include "Viewers/ObjectViewer.h"
#include "Exporters/Exporters.h"

#if DECLARE_VIEWER_PROPS
#include "StaticMesh.h"
#endif

#define APP_CAPTION		"UE Viewer"
#define HOMEPAGE		"http://www.gildor.org/en/projects/umodel"


#if RENDERING
static CObjectViewer *Viewer;			// used from GlWindow callbacks
static bool showMeshes = false;
static bool showMaterials = false;
#endif


/*-----------------------------------------------------------------------------
	Table of known Unreal classes
-----------------------------------------------------------------------------*/

static void RegisterCommonUnrealClasses()
{
	// classes and structures
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
#if BIOSHOCK
	REGISTER_MATERIAL_CLASSES_BIO
	REGISTER_MESH_CLASSES_BIO
#endif
#if UNREAL3
	REGISTER_MATERIAL_CLASSES_U3		//!! needed for Bioshock 2 too
#endif
#if TUROK
	REGISTER_MESH_CLASSES_TUROK
#endif
#if MASSEFF
	REGISTER_MESH_CLASSES_MASSEFF
#endif
#if DCU_ONLINE
	REGISTER_MATERIAL_CLASSES_DCUO
#endif

#if DECLARE_VIEWER_PROPS
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
	REGISTER_MESH_CLASSES_U1_A
END_CLASS_TABLE
}

#if UNREAL3

static void RegisterUnrealClasses3()
{
BEGIN_CLASS_TABLE
//	REGISTER_MATERIAL_CLASSES_U3 -- registered for Bioshock in RegisterCommonUnrealClasses()
	REGISTER_MESH_CLASSES_U3
	REGISTER_MESH_CLASSES_U3_A	//!! remove after refactoring
END_CLASS_TABLE
}

#endif // UNREAL3


static void RegisterUnrealSoundClasses()
{
BEGIN_CLASS_TABLE
	REGISTER_SOUND_CLASSES
#if UNREAL3
	REGISTER_SOUND_CLASSES_UE3
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


/*-----------------------------------------------------------------------------
	Object list support
	change!!
-----------------------------------------------------------------------------*/


#if RENDERING

static bool CreateVisualizer(UObject *Obj, bool test = false);

inline bool ObjectSupported(UObject *Obj)
{
	return CreateVisualizer(Obj, true);
}

#else

inline bool ObjectSupported(UObject *Obj)
{
	return true;
}

#endif // RENDERING

static int ObjIndex = 0;


/*-----------------------------------------------------------------------------
	Exporters
-----------------------------------------------------------------------------*/

#define MAX_EXPORTERS		16

bool GExportScripts = false;
bool GExportLods    = false;
bool GExportPskx    = false;

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

	static UniqueNameList ExportedNames;

	for (int i = 0; i < numExporters; i++)
	{
		const CExporterInfo &Info = exporters[i];
		if (Obj->IsA(Info.ClassName))
		{
			const char *ExportPath = GetExportPath(Obj);
			const char *ClassName  = Obj->GetClassName();
			// get name uniqie index
			char uniqueName[256];
			appSprintf(ARRAY_ARG(uniqueName), "%s/%s.%s", ExportPath, Obj->Name, ClassName);
			int uniqieIdx = ExportedNames.RegisterName(uniqueName);
			const char *OriginalName = NULL;
			if (uniqieIdx >= 2)
			{
				appSprintf(ARRAY_ARG(uniqueName), "%s_%d", Obj->Name, uniqieIdx);
				appPrintf("Duplicate name %s found for class %s, renaming to %s\n", Obj->Name, ClassName, uniqueName);
				//?? HACK: temporary replace object name with unique one
				OriginalName = Obj->Name;
				Obj->Name    = uniqueName;
			}

			if (Info.FileExt)
			{
				char filename[512];
				appSprintf(ARRAY_ARG(filename), "%s/%s.%s", ExportPath, Obj->Name, Info.FileExt);
				FFileReader Ar(filename, false);
				Ar.ArVer = 128;			// less than UE3 version (required at least for VJointPos structure)
				Info.Func(Obj, Ar);
			}
			else
			{
				Info.Func(Obj, *GDummySave);
			}

			//?? restore object name
			if (OriginalName) Obj->Name = OriginalName;
			return true;
		}
	}
	return false;

	unguardf(("%s'%s'", Obj->GetClassName(), Obj->Name));
}


// wrappers
static void ExportStaticMesh2(const UStaticMesh *Mesh, FArchive &Ar)
{
	assert(Mesh->ConvertedMesh);
	ExportStaticMesh(Mesh->ConvertedMesh, Ar);
}

static void ExportStaticMesh3(const UStaticMesh3 *Mesh, FArchive &Ar)
{
	assert(Mesh->ConvertedMesh);
	ExportStaticMesh(Mesh->ConvertedMesh, Ar);
}

static void ExportMeshAnimation(const UMeshAnimation *Anim, FArchive &Ar)
{
	assert(Anim->ConvertedAnim);
	ExportPsa(Anim->ConvertedAnim, Ar);
}

static void ExportAnimSet(const UAnimSet *Anim, FArchive &Ar)
{
	assert(Anim->ConvertedAnim);
	ExportPsa(Anim->ConvertedAnim, Ar);
}

static void ExportMd5Anim2(const UMeshAnimation *Anim, FArchive &Ar)
{
	assert(Anim->ConvertedAnim);
	ExportMd5Anim(Anim->ConvertedAnim, Ar);
}

static void ExportMd5Anim3(const UAnimSet *Anim, FArchive &Ar)
{
	assert(Anim->ConvertedAnim);
	ExportMd5Anim(Anim->ConvertedAnim, Ar);
}


/*-----------------------------------------------------------------------------
	List of supported games
-----------------------------------------------------------------------------*/

struct GameInfo
{
	const char *Name;
	const char *Switch;
	int			Enum;
};

#define G1(name)		{ name, NULL, GAME_UE1 }
#define G2(name)		{ name, NULL, GAME_UE2 }
#define G3(name)		{ name, NULL, GAME_UE3 }
#define G(name,s,e)		{ name, #s,   e        }


static GameInfo games[] = {
	// Unreal Engine 1
#if UNREAL1
		G("Unreal Engine 1", ue1, GAME_UE1),
		G1("Unreal 1"),
		G1("Unreal Tournament 1 (UT99)"),
		G1("The Wheel of Time"),
	#if DEUS_EX
		G1("DeusEx"),
	#endif
	#if RUNE
		G1("Rune"),
	#endif
	#if UNDYING
		G("Undying", undying, GAME_Undying),
	#endif
#endif // UNREAL1

	// Unreal Engine 2
		G("Unreal Engine 2", ue2, GAME_UE2),
#if UT2
		G("Unreal Tournament 2003,2004", ut2, GAME_UT2),
#endif
#if UC1
		G("Unreal Championship", uc1, GAME_UC1),
#endif
#if SPLINTER_CELL
		G("Splinter Cell 1,2", scell, GAME_SplinterCell),
#endif
#if LEAD
		G("Splinter Cell: Conviction", scconv, GAME_SplinterCellConv),
#endif
#if LINEAGE2
		G("Lineage 2", l2, GAME_Lineage2),
#endif
#if LOCO
		G("Land of Chaos Online (LOCO)", loco, GAME_Loco),
#endif
#if BATTLE_TERR
		G("Battle Territory Online", bterr, GAME_BattleTerr),
#endif
#if SWRC
		G("Star Wars: Republic Commando", swrc, GAME_RepCommando),
#endif
#if XIII
		G("XIII", xiii, GAME_XIII),
#endif
#if UNREAL25
		G2("UE2Runtime"),
#	if TRIBES3
		G("Tribes: Vengeance", t3, GAME_Tribes3),
#	endif
#	if SWAT4
		G("SWAT 4", swat4, GAME_Swat4),
#	endif
#	if BIOSHOCK
		G("Bioshock, Bioshock 2", bio, GAME_Bioshock),
#	endif
#	if RAGNAROK2
		G("Ragnarok Online 2", rag2, GAME_Ragnarok2),
#	endif
#	if EXTEEL
		G("Exteel", extl, GAME_Exteel),
#	endif
#	if SPECIAL_TAGS
		G2("Killing Floor"),
#	endif
#endif // UNREAL25

	// Unreal Engine 2X
#if UC2
		G("Unreal Championship 2: The Liandri Conflict", uc2, GAME_UC2),
#endif

	// Unreal Engine 3
#if UNREAL3
		G("Unreal Engine 3", ue3, GAME_UE3),
		G3("Unreal Tournament 3"),
		G3("Gears of War"),
#	if XBOX360
		G3("Gears of War 2"),
		G3("Gears of War 3"),
#	endif
#	if IPHONE
		G3("Infinity Blade"),
#	endif
#	if BULLETSTORM
		G("Bulletstorm", bs, GAME_Bulletstorm),
#	endif
#	if ENDWAR
		G("EndWar", endwar, GAME_EndWar),
#	endif
#	if R6VEGAS
		G("Rainbow 6: Vegas 2", r6v2, GAME_R6Vegas2),
#	endif
#	if MASSEFF
		G("Mass Effect", mass, GAME_MassEffect),
		G("Mass Effect 2", mass2, GAME_MassEffect2),
#	endif
#	if A51
		G("BlackSite: Area 51", a51, GAME_A51),
#	endif
#	if MKVSDC
		G("Mortal Kombat vs. DC Universe", mk, GAME_MK),
		G("Mortal Kombat", mk, GAME_MK),
#	endif
#	if TUROK
		G("Turok", turok, GAME_Turok),
#	endif
#	if FURY
		G("Fury", fury, GAME_Fury),
#	endif
#	if TNA_IMPACT
		G("TNA iMPACT!", tna, GAME_TNA),
#	endif
#	if STRANGLE
		G("Stranglehold", strang, GAME_Strangle),
#	endif
#	if ARMYOF2
		G("Army of Two", ao2, GAME_ArmyOf2),
#	endif
#	if DOH
		G("Destroy All Humans", doh, GAME_DOH),
#	endif
#	if HUXLEY
		G("Huxley", huxley, GAME_Huxley),
#	endif
#	if TLR
		G("The Last Remnant", tlr, GAME_TLR),
#	endif
#	if MEDGE
		G("Mirror's Edge", medge, GAME_MirrorEdge),
#	endif
#	if XMEN
		G("X-Men Origins: Wolverine", xmen, GAME_XMen),
#	endif
#	if MCARTA
		G("Magna Carta 2", mcarta, GAME_MagnaCarta),
#	endif
#	if BATMAN
		G("Batman: Arkham Asylum", batman, GAME_Batman),
#	endif
#	if CRIMECRAFT
		G("Crime Craft", crime, GAME_CrimeCraft),
#	endif
#	if AVA
		G("AVA Online", ava, GAME_AVA),
#	endif
#	if FRONTLINES
		G("Frontlines: Fuel of War", frontl, GAME_Frontlines),
		G("Homefront",               frontl, GAME_Frontlines),
#	endif
#	if BLOODONSAND
		G("50 Cent: Blood on the Sand", 50cent, GAME_50Cent),
#	endif
#	if BORDERLANDS
		G("Borderlands", border, GAME_Borderlands),
#	endif
#	if DARKVOID
		G("Dark Void", darkv, GAME_DarkVoid),
#	endif
#	if LEGENDARY
		G("Legendary: Pandora's Box", leg, GAME_Legendary),
#	endif
#	if TERA
		G("TERA: The Exiled Realm of Arborea", tera, GAME_Tera),
#	endif
#	if BLADENSOUL
		G("Blade & Soul", bns, GAME_BladeNSoul),
#	endif
#	if ALPHA_PR
		G("Alpha Protocol", alpha, GAME_AlphaProtocol),
#	endif
#	if APB
		G("All Points Bulletin", apb, GAME_APB),
#	endif
#	if TRANSFORMERS
		G("The Bourne Conspiracy",           trans, GAME_Transformers),
		G("Transformers: War for Cybertron", trans, GAME_Transformers),
		G("Transformers: Dark of the Moon",  trans, GAME_Transformers),
#	endif
#	if AA3
		G("America's Army 3", aa3, GAME_AA3),
#	endif
#	if MORTALONLINE
		G("Mortal Online", mo, GAME_MortalOnline),
#	endif
#	if ENSLAVED
		G("Enslaved: Odyssey to the West", ens, GAME_Enslaved),
#	endif
#	if MOHA
		G("Medal of Honor: Airborne", moha, GAME_MOHA),
#	endif
#	if MOH2010
		G("Medal of Honor 2010", moh2010, GAME_MOH2010),
#	endif
#	if BERKANIX
		G("Berkanix", berk, GAME_Berkanix),
#	endif
#	if UNDERTOW
		G("Undertow", undertow, GAME_Undertow),
#	endif
#	if SINGULARITY
		G("Singularity", sing, GAME_Singularity),
#	endif
#	if NURIEN
		G3("Nurien"),
#	endif
#	if HUNTED
		G("Hunted: The Demon's Forge", hunt, GAME_Hunted),
#	endif
#	if DND
		G("Dungeons & Dragons: Daggerdale", dnd, GAME_DND),
#	endif
#	if SHADOWS_DAMNED
		G("Shadows of the Damned", shad, GAME_ShadowsDamned),
#	endif
#endif // UNREAL3
};

static const char *GetEngineName(int Game)
{
	Game &= GAME_ENGINE;

	switch (Game)
	{
	case GAME_UE1:
		return "Unreal Engine 1";
	case GAME_UE2:
	case GAME_VENGEANCE:
		return "Unreal Engine 2";
	case GAME_UE2X:
		return "Unreal Engine 2X";
	case GAME_UE3:
	case GAME_MIDWAY3:
		return "Unreal Engine 3";
	}
	return "Unknown UE";
}


static void PrintGameList(bool tags = false)
{
	const char *oldTitle = NULL;
	int pos = 0;
#define LINEFEED 80

	int Count = ARRAY_COUNT(games);
	bool out = false;
	for (int i = 0; i < Count; i++)
	{
		const GameInfo &info = games[i];
		if (tags && !info.Switch) continue;
		// engine title
		const char *title = GetEngineName(info.Enum);
		if (title != oldTitle)
		{
			appPrintf("%s%s:", out ? "\n\n" : "", title);
			pos = LINEFEED;
		}
		oldTitle = title;
		out = true;
		// game info
		if (tags)
		{
			appPrintf("\n %8s  %s", info.Switch ? info.Switch : "", info.Name);
			continue;
		}
		// simple game list
		if (!(info.Enum & ~GAME_ENGINE) && info.Switch) continue;	// skip simple GAME_UEn
		const char *name = info.Name;
		int len = strlen(name);
		bool needComma = (i < Count - 1) && (GetEngineName(games[i+1].Enum) == title);
		if (needComma) len += 2;
		if (pos >= LINEFEED - len)
		{
			appPrintf("\n  ");
			pos = 2;
		}
		appPrintf("%s%s", name, needComma ? ", " : "");
		pos += len;
	}
	appPrintf("\n");
}


static int FindGameTag(const char *name)
{
	for (int i = 0; i < ARRAY_COUNT(games); i++)
	{
		const char *key = games[i].Switch;
		if (!key) continue;
		if (!stricmp(key, name)) return games[i].Enum;
	}
	return -1;
}


/*-----------------------------------------------------------------------------
	Usage information
-----------------------------------------------------------------------------*/

static void Usage()
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
			"    -view           (default) visualize object; when <object> is not specified,\n"
			"                    will load whole package\n"
			"    -list           list contents of package\n"
			"    -export         export specified object or whole package\n"
			"    -taglist        list of tags to override game autodetection\n"
			"\n"
			"Developer commands:\n"
			"    -dump           dump object information to console\n"
			"    -check          check some assumptions, no other actions performed\n"
			"    -pkginfo        load package and display its information\n"
			"\n"
			"Options:\n"
			"    -path=PATH      path to game installation directory; if not specified,\n"
			"                    program will search for packages in current directory\n"
			"    -game=tag       override game autodetection (see -taglist for variants)\n"
			"    -pkg=package    load extra package (in addition to <package>)\n"
			"\n"
			"Compatibility options:\n"
			"    -nomesh         disable loading of SkeletalMesh classes in a case of\n"
			"                    unsupported data format\n"
			"    -noanim         disable loading of MeshAnimation classes\n"
			"    -nostat         disable loading of StaticMesh class\n"
			"    -notex          disable loading of Material classes\n"
			"    -sounds         allow export of sounds\n"
			"    -3rdparty       allow 3rd party asset export (ScaleForm)\n"
			"    -lzo|lzx|zlib   force compression method for fully-compressed packages\n"
			"\n"
			"Platform selection:\n"
			"    -ps3            override platform autodetection to PS3\n"
			"    -ios            set platform to iOS (iPhone/iPad)\n"
			"\n"
			"Viewer options:\n"
			"    -meshes         view meshes only\n"
			"    -materials      view materials only (excluding textures)\n"
 			"\n"
			"Export options:\n"
			"    -out=PATH       export everything into PATH instead of the current directory\n"
			"    -all            export all linked objects too\n"
			"    -uncook         use original package name as a base export directory\n"
			"    -groups         use group names instead of class names for directories\n"
			"    -uc             create unreal script when possible\n"
			"    -pskx           use pskx/psax format for skeletal mesh\n"
			"    -md5            use md5mesh/md5anim format for skeletal mesh\n"
			"    -lods           export all available mesh LOD levels\n"
			"    -notgacomp      disable TGA compression\n"
			"\n"
			"Supported resources for export:\n"
			"    SkeletalMesh    exported as ActorX psk file or MD5Mesh\n"
			"    MeshAnimation   exported as ActorX psa file or MD5Anim\n"
			"    VertMesh        exported as Unreal 3d file\n"
			"    StaticMesh      exported as psk file with no skeleton (pskx)\n"
			"    Texture         exported in tga format\n"
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
	exit(0);
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

#define OPT_BOOL(name,var)				{ name, (byte*)&var, true  },
#define OPT_VALUE(name,var,value)		{ name, &var,        value },


int main(int argc, char **argv)
{
#if DO_GUARD
	TRY {
#endif

	guard(Main);

	// display usage
	if (argc < 2) Usage();

	// parse command line
	enum
	{
		CMD_View,
		CMD_Dump,
		CMD_Check,
		CMD_PkgInfo,
		CMD_List,
		CMD_Export,
		CMD_TagList
	};
	static byte mainCmd = CMD_View;
	static bool md5 = false, exprtAll = false, noMesh = false, noStat = false, noAnim = false,
		 noTex = false, regSounds = false, reg3rdparty = false, hasRootDir = false;
	TArray<const char*> extraPackages;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			// simple options
			static const OptionInfo options[] =
			{
				OPT_VALUE("view",    mainCmd, CMD_View)
				OPT_VALUE("dump",    mainCmd, CMD_Dump)
				OPT_VALUE("check",   mainCmd, CMD_Check)
				OPT_VALUE("export",  mainCmd, CMD_Export)
				OPT_VALUE("taglist", mainCmd, CMD_TagList)
				OPT_VALUE("pkginfo", mainCmd, CMD_PkgInfo)
				OPT_VALUE("list",    mainCmd, CMD_List)
#if RENDERING
				OPT_BOOL ("meshes",    showMeshes)
				OPT_BOOL ("materials", showMaterials)
#endif
				OPT_BOOL ("all",     exprtAll)
				OPT_BOOL ("uncook",  GUncook)
				OPT_BOOL ("groups",  GUseGroups)
				OPT_BOOL ("pskx",    GExportPskx)
				OPT_BOOL ("md5",     md5)
				OPT_BOOL ("lods",    GExportLods)
				OPT_BOOL ("uc",      GExportScripts)
				// disable classes
				OPT_BOOL ("nomesh",  noMesh)
				OPT_BOOL ("nostat",  noStat)
				OPT_BOOL ("noanim",  noAnim)
				OPT_BOOL ("notex",   noTex)
				OPT_BOOL ("sounds",  regSounds)
				OPT_BOOL ("notgacomp", GNoTgaCompress)
				OPT_BOOL ("3rdparty", reg3rdparty)
				// platform
				OPT_VALUE("ps3",     GForcePlatform, PLATFORM_PS3)
				OPT_VALUE("ios",     GForcePlatform, PLATFORM_IOS)
				// compression
				OPT_VALUE("lzo",     GForceCompMethod, COMPRESS_LZO )	//!! add these options to the "extract" and "decompress"
				OPT_VALUE("zlib",    GForceCompMethod, COMPRESS_ZLIB)
				OPT_VALUE("lzx",     GForceCompMethod, COMPRESS_LZX )
			};
			if (ProcessOption(ARRAY_ARG(options), opt))
				continue;
			// more complex options
			if (!strnicmp(opt, "path=", 5))
			{
				appSetRootDirectory(opt+5);
				hasRootDir = true;
			}
			else if (!strnicmp(opt, "out=", 4))
			{
				appSetBaseExportDirectory(opt+4);
			}
			else if (!strnicmp(opt, "game=", 5))
			{
				int tag = FindGameTag(opt+5);
				if (tag == -1)
				{
					appPrintf("ERROR: unknown game tag \"%s\".", opt+5);
					PrintGameList(true);
					exit(0);
				}
				GForceGame = tag;
			}
			else if (!strnicmp(opt, "pkg=", 4))
			{
				const char *pkg = opt+4;
				extraPackages.AddItem(pkg);
			}
			else
				Usage();
		}
		else
		{
			break;
		}
	}

	if (mainCmd == CMD_TagList)
	{
		PrintGameList(true);
		return 0;
	}

	if (arg >= argc) Usage();									// no space for package name

	const char *argPkgName   = argv[arg++];
	if (!argPkgName) Usage();
	const char *argObjName   = NULL;
	const char *argClassName = NULL;
	if (arg < argc)
		argObjName   = argv[arg++];
	if (arg < argc)
		argClassName = argv[arg++];
	if (arg != argc) Usage();									// extera argument found

	// load the package

#if PROFILE
	appResetProfiler();
#endif

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// load package
	UnPackage *Package;
	if (strchr(argPkgName, '/') || strchr(argPkgName, '\\'))
	{
		// has path in filename
		if (!hasRootDir) appSetRootDirectory2(argPkgName);
	}
	else
	{
		// no path in filename
		if (!hasRootDir) appSetRootDirectory(".");		// scan for packages
	}

	Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		appPrintf("ERROR: unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	// register exporters
	if (!md5 && !GExportPskx)
	{
		EXPORTER("SkeletalMesh",  "psk",     ExportPsk          );
		EXPORTER("MeshAnimation", "psa",     ExportMeshAnimation);		//!! always use psa
#if UNREAL3
		EXPORTER("AnimSet",       "psa",     ExportAnimSet      );		//!! ..
#endif
	}
	else if (!md5) // && GExportPskx
	{
		EXPORTER("SkeletalMesh",  "pskx",    ExportPsk          );
		EXPORTER("MeshAnimation", "psax",    ExportMeshAnimation);		//!! remove this
#if UNREAL3
		EXPORTER("AnimSet",       "psax",    ExportAnimSet      );		//!! ..
#endif
	}
	else
	{
		EXPORTER("SkeletalMesh",  "md5mesh", ExportMd5Mesh);
		EXPORTER("MeshAnimation", NULL,      ExportMd5Anim2);	// separate file for each animation track
#if UNREAL3
		EXPORTER("AnimSet",       NULL,      ExportMd5Anim3);	// ...
#endif
	}
	EXPORTER("VertMesh",      NULL,   Export3D  );				// will generate 2 files
	EXPORTER("StaticMesh",    "pskx", ExportStaticMesh2);
	EXPORTER("Texture",       "tga",  ExportTga );
	EXPORTER("Sound",         NULL,   ExportSound);
#if UNREAL3
	EXPORTER("StaticMesh3",   "pskx", ExportStaticMesh3);
	EXPORTER("Texture2D",     "tga",  ExportTga );
	EXPORTER("SoundNodeWave", NULL,   ExportSoundNodeWave);
	EXPORTER("SwfMovie",      "gfx",  ExportGfx );
	EXPORTER("FaceFXAnimSet", "fxa",  ExportFaceFXAnimSet);
	EXPORTER("FaceFXAsset",   "fxa",  ExportFaceFXAsset  );
#endif // UNREAL3

	// prepare classes
	// note: we are registering classes after loading package: in this case we can know engine version (1/2/3)
	RegisterCommonUnrealClasses();
	if (Package->Game < GAME_UE3)
		RegisterUnrealClasses2();
	else
		RegisterUnrealClasses3();
	if (regSounds)   RegisterUnrealSoundClasses();
	if (reg3rdparty) RegisterUnreal3rdPartyClasses();

	// remove some class loaders when requisted by command line
	if (noAnim)
	{
		UnregisterClass("MeshAnimation", true);
		UnregisterClass("AnimSequence");
		UnregisterClass("AnimNotify", true);
	}
	if (noMesh)
	{
		UnregisterClass("SkeletalMesh", true);
		UnregisterClass("SkeletalMeshSocket", true);
	}
	if (noStat) UnregisterClass("StaticMesh",     true);
	if (noTex)  UnregisterClass("UnrealMaterial", true);

	if (mainCmd == CMD_PkgInfo)
		return 0;

	if (mainCmd == CMD_List)
	{
		guard(List);
		// dump package exports table
		for (int i = 0; i < Package->Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = Package->ExportTable[i];
			appPrintf("%4d %8X %8X %s %s\n", i, Exp.SerialOffset, Exp.SerialSize, Package->GetObjectName(Exp.ClassIndex), *Exp.ObjectName);
		}
		unguard;
		return 0;
	}

#if BIOSHOCK
	if (Package->Game == GAME_Bioshock)
	{
		//!! should change this code!
		CTypeInfo::RemapProp("UShader", "Opacity", "Opacity_Bio"); //!!
	}
#endif // BIOSHOCK

	UObject *Obj = NULL;
	// get requested object info
	if (argObjName)
	{
		// load specific object(s)
		int found = 0;
		int idx = -1;
		while (true)
		{
			idx = Package->FindExport(argObjName, argClassName, idx + 1);
			if (idx == INDEX_NONE) break;
			found++;

			const char *className = Package->GetObjectName(Package->ExportTable[idx].ClassIndex);
			// setup NotifyInfo to describe object
			appSetNotifyHeader("%s:  %s'%s'", argPkgName, className, argObjName);
			// create object from package
			Obj = Package->CreateExport(idx);
		}
		if (!found)
		{
			appPrintf("Export \"%s\" was not found in package \"%s\"\n", argObjName, argPkgName);
			exit(1);
		}
		appPrintf("Found %d objects with a name \"%s\"\n", found, argObjName);
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
				appPrintf("Package \"%s\" has no supported objects\n", argPkgName);
				exit(1);
			}
			Obj = UObject::GObjObjects[0];
		}
		// load additional packages
		for (int i = 0; i < extraPackages.Num(); i++)
		{
			UnPackage *Package2 = UnPackage::LoadPackage(extraPackages[i]);
			if (!Package)
			{
				appPrintf("WARNING: unable to find/load package %s\n", extraPackages[i]);
				continue;
			}
			// create exports (the same code as above)
			for (int idx = 0; idx < Package2->Summary.ExportCount; idx++)
			{
				if (IsKnownClass(Package2->GetObjectName(Package2->GetExport(idx).ClassIndex)))
					Package2->CreateExport(idx);
			}
		}
		unguard;
	}
	if (!Obj) return 0;					// object was not created

#if PROFILE
	appPrintProfiler();
#endif

	if (mainCmd == CMD_Export)
	{
		appPrintf("Exporting objects ...\n");
		// export object(s), if possible
		bool oneObjectOnly = (argObjName != NULL && !exprtAll);
		UnPackage* notifyPackage = NULL;
		for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
		{
			UObject* ExpObj = UObject::GObjObjects[idx];
			if (!exprtAll && ExpObj->Package != Package)	// refine object by package
				continue;
			if (notifyPackage != ExpObj->Package)
			{
				notifyPackage = ExpObj->Package;
				appSetNotifyHeader(notifyPackage->Filename);
			}
			if (ExportObject(ExpObj))
			{
				appPrintf("Exported %s %s\n", ExpObj->GetClassName(), ExpObj->Name);
			}
			else if (argObjName && ExpObj == Obj)
			{
				// display warning message only when failed to export object, specified from command line
				appPrintf("ERROR: Export object %s: unsupported type %s\n", ExpObj->Name, ExpObj->GetClassName());
			}
			if (oneObjectOnly) break;
		}
		return 0;
	}

#if RENDERING
	if (!CreateVisualizer(Obj))
	{
		appPrintf("Package \"%s\" has no objects to display\n", argPkgName);
		return 0;
	}
	// print mesh info
#	if TEST_FILES
	Viewer->Test();
#	endif

	if (mainCmd == CMD_Dump)
		Viewer->Dump();					// dump info to console

	if (mainCmd == CMD_View)
	{
		// show object
		vpInvertXAxis = true;
		guard(MainLoop);
		VisualizerLoop(APP_CAPTION);
		unguard;
	}
#endif // RENDERING

	// cleanup
#if RENDERING
	delete Viewer;
#endif
	delete Obj;

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
		UClass *Obj2 = static_cast<UClass*>(Obj); 	\
		if (!test) Viewer = new CViewer(Obj2->ConvertedMesh); \
		return true;								\
	}
	// create viewer for known class
	bool showAll = !(showMeshes || showMaterials);
	if (showMeshes || showAll)
	{
		CLASS_VIEWER(UVertMesh,       CVertMeshViewer, true);
		CLASS_VIEWER(USkeletalMesh,   CSkelMeshViewer, true);
		MESH_VIEWER (UStaticMesh,     CStatMeshViewer      );
#if UNREAL3
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
	unguard;
}


void AppDrawFrame()
{
	guard(AppDrawFrame);
	Viewer->Draw3D();
	unguard;
}

#if 0

class FArchiveGetDepends : public FArchive
{
public:
	TArray<UObject*>	Depends;

	FArchiveGetDepends(UObject *Object)
	{
		IsLoading = false;
		Depends.AddItem(Object);
		Object->Serialize(*this);
	}

	virtual void Seek(int Pos)
	{}
	virtual void Serialize(void *data, int size)
	{}
	virtual FArchive& operator<<(UObject *&Obj)
	{
		guard(FArchiveGetDepends::operator<<(UObject*));
//		printf("? Obj: %s'%s'\n", Obj->GetClassName(), Obj->Name);
		if (Depends.FindItem(Obj) == INDEX_NONE)
		{
//			printf("... new, parsing\n");
			Depends.AddItem(Obj);
			Obj->Serialize(*this);
//			printf("... end parsing\n");
		}
//		else printf("... found\n");
		return *this;
		unguard;
	}
};

#endif


static void TakeScreenshot(const char *ObjectName)
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
	FFileReader Ar(filename, false);
	int width, height;
	GetWindowSize(width, height);

	byte *pic = new byte [width * height * 4];
	glFinish();
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pic);
	WriteTGA(Ar, width, height, pic);
	delete pic;
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
		CreateVisualizer(Obj);
		return;
	}
	if (key == 'd')
	{
		Viewer->Dump();
		return;
	}
#if 0
	//!! disabled: Object.Serialize(SomeArchive) requires UObject writting properties capability
	if (key == ('x'|KEY_CTRL))
	{
		assert(Viewer->Object);
		FArchiveGetDepends Deps(Viewer->Object);
		//!! export here
	}
#endif
	if (key == ('s'|KEY_CTRL))
	{
		UObject *Obj = UObject::GObjObjects[ObjIndex];
		TakeScreenshot(Obj->Name);
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
		DrawKeyHelp("PgUp/PgDn", "browse objects");
		DrawKeyHelp("D",         "dump info");
#if 0
		DrawKeyHelp("Ctrl+X",    "export object");
#endif
		DrawKeyHelp("Ctrl+S",    "take screenshot");
		Viewer->ShowHelp();
		DrawTextLeft("-----\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}

#endif // RENDERING
