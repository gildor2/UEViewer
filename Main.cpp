#include "Core.h"

//!! move UI code to separate cpp and simply call their functions
#if HAS_UI
#include "../UI/BaseDialog.h"
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

#define APP_CAPTION					"UE Viewer"
#define HOMEPAGE					"http://www.gildor.org/en/projects/umodel"

//#define SHOW_HIDDEN_SWITCHES		1

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


// index of the current object in UObject::GObjObjects array
static int ObjIndex = 0;


/*-----------------------------------------------------------------------------
	Exporters
-----------------------------------------------------------------------------*/

// wrappers
static void ExportPsk2(const USkeletalMesh *Mesh)
{
	assert(Mesh->ConvertedMesh);
	ExportPsk(Mesh->ConvertedMesh);
}

#if UNREAL3
static void ExportPsk3(const USkeletalMesh3 *Mesh)
{
	assert(Mesh->ConvertedMesh);
	ExportPsk(Mesh->ConvertedMesh);
}
#endif

static void ExportMd5Mesh2(const USkeletalMesh *Mesh)
{
	assert(Mesh->ConvertedMesh);
	ExportMd5Mesh(Mesh->ConvertedMesh);
}

#if UNREAL3
static void ExportMd5Mesh3(const USkeletalMesh3 *Mesh)
{
	assert(Mesh->ConvertedMesh);
	ExportMd5Mesh(Mesh->ConvertedMesh);
}
#endif

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
	ExportPsa(Anim->ConvertedAnim);
}

#if UNREAL3
static void ExportAnimSet(const UAnimSet *Anim)
{
	assert(Anim->ConvertedAnim);
	ExportPsa(Anim->ConvertedAnim);
}
#endif

static void ExportMd5Anim2(const UMeshAnimation *Anim)
{
	assert(Anim->ConvertedAnim);
	ExportMd5Anim(Anim->ConvertedAnim);
}

#if UNREAL3
static void ExportMd5Anim3(const UAnimSet *Anim)
{
	assert(Anim->ConvertedAnim);
	ExportMd5Anim(Anim->ConvertedAnim);
}
#endif


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


static const GameInfo games[] = {
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
#	if AA2
		G("America's Army 2", aa2, GAME_AA2),
#	endif
#	if VANGUARD
		G("Vanguard: Saga of Heroes", vang, GAME_Vanguard),
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
		G("Gears of War: Judgment", gowj, GAME_GoWJ),
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
		G("Mass Effect",   mass,  GAME_MassEffect ),
		G("Mass Effect 2", mass2, GAME_MassEffect2),
		G("Mass Effect 3", mass3, GAME_MassEffect3),
#	endif
#	if A51
		G("BlackSite: Area 51", a51, GAME_A51),
#	endif
#	if MKVSDC
		G("Mortal Kombat vs. DC Universe", mk, GAME_MK),
		G("Mortal Kombat", mk, GAME_MK),
		G("Injustice: Gods Among Us", mk, GAME_MK),
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
		G("Batman: Arkham Asylum",  batman,  GAME_Batman),
		G("Batman: Arkham City",    batman2, GAME_Batman2),
		G("Batman: Arkham Origins", batman3, GAME_Batman3),
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
		G("Borderlands",   border, GAME_Borderlands),
		G("Borderlands 2", border, GAME_Borderlands),
		G("Brothers in Arms: Hell's Highway", border, GAME_Borderlands),
#	endif
#	if ALIENS_CM
		G("Aliens: Colonial Marines", acm, GAME_AliensCM),
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
		G("Transformers: Fall of Cybertron", trans, GAME_Transformers),
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
#	if ARGONAUTS
		G("Rise of the Argonauts", argo, GAME_Argonauts),
		G("Thor: God of Thunder",  argo, GAME_Argonauts),
#	endif
#	if GUNLEGEND
		G("Gunslayer Legend", gunsl, GAME_GunLegend),
#	endif
#	if SPECIALFORCE2
		G("Special Force 2", sf2, GAME_SpecialForce2),
#	endif
#	if TRIBES4
		G("Tribes: Ascend", t4, GAME_Tribes4),
#	endif
#	if DISHONORED
		G("Dishonored", dis, GAME_Dishonored),
#	endif
#	if FABLE
		G("Fable: The Journey", fable, GAME_Fable),
		G("Fable Anniversary",  fable, GAME_Fable),
#	endif
#	if DMC
		G("DmC: Devil May Cry", dmc, GAME_DmC),
#	endif
#	if HAWKEN
		G3("Hawken"),
#	endif
#	if PLA
		G("Passion Leads Army", pla, GAME_PLA),
#	endif
#	if TAO_YUAN
		G("Tao Yuan", taoyuan, GAME_TaoYuan),
#	endif
#	if BIOSHOCK3
		G("Bioshock Infinite", bio3, GAME_Bioshock3),
#	endif
#	if REMEMBER_ME
		G("Remember Me", rem, GAME_RememberMe),
#	endif
#	if MARVEL_HEROES
		G("Marvel Heroes", mh, GAME_MarvelHeroes),
#	endif
#	if LOST_PLANET3
		G("Lost Planet 3", lp3, GAME_LostPlanet3),
		G("Yaiba: Ninja Gaiden Z", lp3, GAME_LostPlanet3),
#	endif
#	if XCOM_BUREAU
		G("The Bureau: XCOM Declassified", xcom, GAME_XcomB),
#	endif
#	if THIEF4
		G("Thief", thief4, GAME_Thief4),
#	endif
#	if MURDERED
		G("Murdered: Soul Suspect", murd, GAME_Murdered),
#	endif
#	if SOV
		G("Seal of Vajra", sov, GAME_SOV),
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
	exit(0);
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

#if HAS_UI
static void PrintVersionInfoCB(UIButton*)
{
	PrintVersionInfo();
}
#endif


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
	if (argc < 2) PrintUsage();

	// parse command line
	enum
	{
		CMD_View,
		CMD_Dump,
		CMD_Check,
		CMD_PkgInfo,
		CMD_List,
		CMD_Export,
		CMD_TagList,
		CMD_VersionInfo,
	};
	static byte mainCmd = CMD_View;
	static bool md5 = false, exprtAll = false, noMesh = false, noStat = false, noAnim = false,
		 noTex = false, noLightmap = false, regSounds = false, reg3rdparty = false, hasRootDir = false;
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
			OPT_VALUE("taglist", mainCmd, CMD_TagList)
			OPT_VALUE("pkginfo", mainCmd, CMD_PkgInfo)
			OPT_VALUE("list",    mainCmd, CMD_List)
			OPT_VALUE("version", mainCmd, CMD_VersionInfo)
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
			OPT_BOOL ("md5",     md5)
			OPT_BOOL ("lods",    GExportLods)
			OPT_BOOL ("uc",      GExportScripts)
			// disable classes
			OPT_BOOL ("nomesh",  noMesh)
			OPT_BOOL ("nostat",  noStat)
			OPT_BOOL ("noanim",  noAnim)
			OPT_BOOL ("notex",   noTex)
			OPT_BOOL ("nolightmap", noLightmap)
			OPT_BOOL ("sounds",  regSounds)
			OPT_BOOL ("3rdparty", reg3rdparty)
			OPT_BOOL ("dds",     GExportDDS)
			OPT_BOOL ("notgacomp", GNoTgaCompress)
			OPT_BOOL ("nooverwrite", GDontOverwriteFiles)
			// platform
			OPT_VALUE("ps3",     GForcePlatform, PLATFORM_PS3)
			OPT_VALUE("ios",     GForcePlatform, PLATFORM_IOS)
			// compression
			OPT_VALUE("lzo",     GForceCompMethod, COMPRESS_LZO )
			OPT_VALUE("zlib",    GForceCompMethod, COMPRESS_ZLIB)
			OPT_VALUE("lzx",     GForceCompMethod, COMPRESS_LZX )
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
				appPrintf("ERROR: unknown game tag \"%s\". Use -taglist option to display available tags.\n", opt+5);
				exit(0);
			}
			GForceGame = tag;
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
#if HAS_UI	//!! testing only
		else if (!stricmp(opt, "gui"))
		{
			UIBaseDialog dialog;
			UIGroup* testGroup1 = new UIGroup("Group #1");
			UIGroup* testGroup2 = new UIGroup("Group #2");
			UIGroup* testGroup3 = new UIGroup("Group #3");
			dialog.Add(testGroup1);
			testGroup1->Add(testGroup2);
			dialog.Add(testGroup3);

			UILabel* label1 = new UILabel("Test label 1", TA_Left);
			testGroup1->Add(label1);

			UIButton* button1 = new UIButton("Button 1");
			button1->SetCallback(BIND_FREE_CB(&PrintVersionInfoCB));
			testGroup1->Add(button1);

			UICheckbox* checkbox1 = new UICheckbox("Checkbox 1", true);
			testGroup1->Add(checkbox1);

			UILabel* label2 = new UILabel("Test label 2", TA_Right);
			testGroup2->Add(label2);

			UILabel* label3 = new UILabel("Test label 3", TA_Center);
			testGroup3->Add(label3);

			dialog.ShowDialog("Umodel Options", 400, 300);
			exit(1);
		}
#endif		//!! HAS_UI
		else
		{
			appPrintf("COMMAND LINE ERROR: unknown option: %s\n", argv[arg]);
			goto bad_params;
		}
	}

	if (mainCmd == CMD_VersionInfo)
	{
		PrintVersionInfo();
		return 0;
	}

	if (mainCmd == CMD_TagList)
	{
		PrintGameList(true);
		return 0;
	}

	if (!params.Num())
	{
	bad_pkg_name:
		appPrintf("COMMAND LINE ERROR: package name is not specified\n");
	bad_params:
		appPrintf("Use \"umodel\" without arguments to show command line help\n");;
		exit(1);
	}

	const char *argPkgName   = params[0];
	const char *argObjName   = (params.Num() >= 2) ? params[1] : NULL;
	const char *argClassName = (params.Num() >= 3) ? params[2] : NULL;
	if (params.Num() > 3)
	{
		appPrintf("COMMAND LINE ERROR: too many arguments. Check your command line.\nYou specified: package=%s, object=%s, class=%s\n", argPkgName, argObjName, argClassName);
		goto bad_params;
	}

	if (!argPkgName) goto bad_pkg_name;

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

	// load main package
	UnPackage *MainPackage = UnPackage::LoadPackage(argPkgName);
	if (!MainPackage)
	{
		appPrintf("ERROR: unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	// initialization

	// register exporters
	if (!md5)
	{
		RegisterExporter("SkeletalMesh",  ExportPsk2   );
		RegisterExporter("MeshAnimation", ExportMeshAnimation);
#if UNREAL3
		RegisterExporter("SkeletalMesh3", ExportPsk3   );
		RegisterExporter("AnimSet",       ExportAnimSet);
#endif
	}
	else
	{
		RegisterExporter("SkeletalMesh",  ExportMd5Mesh2);
		RegisterExporter("MeshAnimation", ExportMd5Anim2);
#if UNREAL3
		RegisterExporter("SkeletalMesh3", ExportMd5Mesh3);
		RegisterExporter("AnimSet",       ExportMd5Anim3);
#endif
	}
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

	// prepare classes
	// note: we are registering classes after loading package: in this case we can know engine version (1/2/3)
	RegisterCommonUnrealClasses();
	if (MainPackage->Game < GAME_UE3)
		RegisterUnrealClasses2();
	else
		RegisterUnrealClasses3();
	if (regSounds)   RegisterUnrealSoundClasses();
	if (reg3rdparty) RegisterUnreal3rdPartyClasses();

	// remove some class loaders when requisted by command line
	if (noAnim)
	{
		UnregisterClass("MeshAnimation", true);
		UnregisterClass("AnimSet",       true);
		UnregisterClass("AnimSequence",  true);
		UnregisterClass("AnimNotify",    true);
	}
	if (noMesh)
	{
		UnregisterClass("SkeletalMesh",       true);
		UnregisterClass("SkeletalMeshSocket", true);
	}
	if (noStat) UnregisterClass("StaticMesh",     true);
	if (noTex)  UnregisterClass("UnrealMaterial", true);
	if (noLightmap) UnregisterClass("LightMapTexture2D", true);

	// end of initialization

	if (mainCmd == CMD_PkgInfo)
		return 0;					// already displayed when loaded package; extend it?

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

#if BIOSHOCK
	if (MainPackage->Game == GAME_Bioshock)
	{
		//!! should change this code!
		CTypeInfo::RemapProp("UShader", "Opacity", "Opacity_Bio"); //!!
	}
#endif // BIOSHOCK

	// preload all extra packages first
	TArray<UnPackage*> Packages;
	Packages.AddItem(MainPackage);	// already loaded
	for (int i = 0; i < extraPackages.Num(); i++)
	{
		UnPackage *Package2 = UnPackage::LoadPackage(extraPackages[i]);
		if (!Package2)
			appPrintf("WARNING: unable to find/load package %s\n", extraPackages[i]);
		else
			Packages.AddItem(Package2);
	}

	TArray<UObject*> Objects;
	// get requested object info
	if (objectsToLoad.Num())
	{
		int totalFound = 0;
		for (int objIdx = 0; objIdx < objectsToLoad.Num(); objIdx++)
		{
			const char *objName   = objectsToLoad[objIdx];
			const char *className = (objIdx == 0) ? argClassName : NULL;
			int found = 0;
			for (int pkg = 0; pkg < Packages.Num(); pkg++)
			{
				UObject::BeginLoad();
				UnPackage *Package2 = Packages[pkg];
				// load specific object(s)
				int idx = -1;
				while (true)
				{
					idx = Package2->FindExport(objName, className, idx + 1);
					if (idx == INDEX_NONE) break;		// not found in this package

					found++;
					totalFound++;
					appPrintf("Export \"%s\" was found in package %s\n", objName, Package2->Name);

					const char *realClassName = Package2->GetObjectName(Package2->ExportTable[idx].ClassIndex);
					// setup NotifyInfo to describe object
					appSetNotifyHeader("%s:  %s'%s'", Package2->Name, realClassName, objName);
					// create object from package
					UObject *Obj = Package2->CreateExport(idx);
					if (Obj)
					{
						Objects.AddItem(Obj);
						if (objName == attachAnimName && (Obj->IsA("MeshAnimation") || Obj->IsA("AnimSet")))
							GForceAnimSet = Obj;
					}
				}
				UObject::EndLoad();
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
		{
			UnPackage *Package2 = Packages[pkg];
			guard(LoadWholePackage);
			UObject::BeginLoad();
			for (int idx = 0; idx < Package2->Summary.ExportCount; idx++)
			{
				if (!IsKnownClass(Package2->GetObjectName(Package2->GetExport(idx).ClassIndex)))
					continue;
				int TmpObjIdx = UObject::GObjObjects.Num();			// place where new object will be created
				/*UObject *TmpObj =*/ Package2->CreateExport(idx);
			}
			UObject::EndLoad();
			unguardf(("%s", Package2->Name));
		}
	}

	if (!UObject::GObjObjects.Num())
	{
		appPrintf("Specified package(s) has no supported objects\n");
		exit(1);
	}

#if PROFILE
	appPrintProfiler();
#endif

	if (mainCmd == CMD_Export)
	{
		appPrintf("Exporting objects ...\n");
		// export object(s), if possible
		UnPackage* notifyPackage = NULL;

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
			if (notifyPackage != ExpObj->Package)
			{
				notifyPackage = ExpObj->Package;
				appSetNotifyHeader(notifyPackage->Filename);
			}
			bool done = ExportObject(ExpObj);
			if (!done && Objects.Num() && (Objects.FindItem(ExpObj) >= 0))
			{
				// display warning message only when failed to export object, specified from command line
				appPrintf("ERROR: Export object %s: unsupported type %s\n", ExpObj->Name, ExpObj->GetClassName());
			}
		}
		return 0;
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
	bool created = false;
	for (int idx = 0; idx < UObject::GObjObjects.Num(); idx++)
		if (CreateVisualizer(UObject::GObjObjects[idx]))
		{
			ObjIndex = idx;
			created = true;
			break;
		}

	if (!created)
	{
		appPrintf("Package \"%s\" has no objects to display\n", argPkgName);	//!! list of packages
		return 0;
	}
	// print mesh info
#	if TEST_FILES
	Viewer->Test();
#	endif

	if (mainCmd == CMD_View)
	{
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
	for (int i = 0; i < Objects.Num(); i++)
		delete Objects[i];

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
	unguardf(("%s'%s'", Obj->GetClassName(), Obj->Name));
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
		UObject *Obj = UObject::GObjObjects[ObjIndex];
		TakeScreenshot(Obj->Name, true);
		GDoScreenshot = 0;
	}

	unguard;
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
		DrawKeyHelp("Ctrl+S",    "take screenshot");
		Viewer->ShowHelp();
		DrawTextLeft("-----\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}

#endif // RENDERING
