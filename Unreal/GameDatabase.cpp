#include "Core.h"
#include "UnCore.h"
#include "GameDatabase.h"

/*-----------------------------------------------------------------------------
	List of supported games
-----------------------------------------------------------------------------*/

#define G1(name)		{ name, NULL, GAME_UE1 }
#define G2(name)		{ name, NULL, GAME_UE2 }
#define G3(name)		{ name, NULL, GAME_UE3 }
#define G(name,s,e)		{ name, #s,   e        }
#define TABLE_END		{ NULL, NULL, 0        }


const GameInfo GListOfGames[] = {
	// Unreal engine 1
#if UNREAL1
		G("Unreal engine 1", ue1, GAME_UE1),
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
		G("Unreal engine 2", ue2, GAME_UE2),
#if UT2
		G("Unreal Tournament 2003,2004", ut2, GAME_UT2),
#endif
#if UC1
		G("Unreal Championship", uc1, GAME_UC1),
#endif
#if SPLINTER_CELL
		G("Splinter Cell 1-4", scell, GAME_SplinterCell),
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
#	if EOS
		G("Echo of Soul", eos, GAME_EOS),
#	endif
#	if SPECIAL_TAGS
		G2("Killing Floor"),
#	endif
#endif // UNREAL25

	// Unreal Engine 2X
#if UC2
		G("Unreal Championship 2: The Liandri Conflict", uc2, GAME_UC2),
#endif

	// Unreal engine 3
#if UNREAL3
		G("Unreal engine 3", ue3, GAME_UE3),
		G3("Unreal Tournament 3"),
		G3("Gears of War"),
#	if SUPPORT_XBOX360
		G3("Gears of War 2"),
		G3("Gears of War 3"),
		G("Gears of War: Judgment", gowj, GAME_GoWJ),
#	endif
#	if SUPPORT_IPHONE
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
		G("Mortal Kombat X", mk, GAME_MK),
#	endif
#	if TUROK
		G("Turok", turok, GAME_Turok),
#	endif
#	if FURY
		G("Fury", fury, GAME_Fury),
#	endif
#	if TNA_IMPACT
		G("TNA iMPACT!", tna, GAME_TNA),
		G("WWE All Stars", tna, GAME_TNA),
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
		G("Batman: Arkham Knight",  batman4, GAME_Batman4),
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
#	if ALICE
		G("Alice: Madness Returns", alice, GAME_Alice),
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
		G("Life is Strange", rem, GAME_RememberMe),
#	endif
#	if MARVEL_HEROES
		G("Marvel Heroes", mh, GAME_MarvelHeroes),
#	endif
#	if LOST_PLANET3
		G("Lost Planet 3", lp3, GAME_LostPlanet3),
		G("Yaiba: Ninja Gaiden Z", lp3, GAME_LostPlanet3),
#	endif
#	if XCOM
		G("The Bureau: XCOM Declassified", xcom, GAME_XcomB),
		G("XCOM 2", xcom2, GAME_Xcom2),
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
#	if VEC
		G("The Vanishing of Ethan Carter", vec, GAME_VEC),
#	endif
#	if DUST514
		G("Dust 514", dust514, GAME_Dust514),
#	endif
#	if GUILTY
		G("Guilty Gear Xrd", guilty, GAME_Guilty),
#	endif
#	if DUNDEF
		G("Dungeon Defenders", dundef, GAME_DunDef),
#	endif
#	if GIGANTIC
		G("Gigantic", gigantic, GAME_Gigantic),
#	endif
#	if METRO_CONF
		G("Metro Conflict", metroconf, GAME_MetroConflict),
#	endif
#	if SMITE
		G("SMITE", smite, GAME_Smite),
#	endif
#	if DEVILS_THIRD
		G("Devil's Third", dev3rd, GAME_DevilsThird),
#	endif
#endif // UNREAL3

	// Unreal engine 4
#if UNREAL4
		// Dummy tag for all UE4 versions
		{
			"Unreal engine 4.0-4." STR(LATEST_SUPPORTED_UE4_VERSION),
			"ue4.[0-" STR(LATEST_SUPPORTED_UE4_VERSION) "]",
			GAME_UE4(LATEST_SUPPORTED_UE4_VERSION+1)	// some invalid version number, but not zero - to show this game in "-help" output
		},
		// Add custom UE4 versions here
#	if GEARS4
		G("Gears of War 4", gears4, GAME_Gears4),
#	endif
#	if FRIDAY13
		G("Friday the 13th: The Game", friday13, GAME_Friday13),
#	endif
#	if TEKKEN7
		G("Tekken 7", tekken7, GAME_Tekken7),
#	endif
#	if LAWBREAKERS
		G("Lawbreakers", lawbr, GAME_Lawbreakers),
#	endif
#	if PARAGON
		G("Paragon", paragon, GAME_Paragon),
#	endif
#	if HIT
		G("Heroes of Incredible Tales", hit, GAME_HIT),
#	endif
#endif // UNREAL4

	// end marker
	TABLE_END
};

#undef G1
#undef G2
#undef G3
#undef G

const char *GetEngineName(int Game)
{
	Game &= GAME_ENGINE;

	switch (Game)
	{
	case GAME_UE1:
		return "Unreal engine 1";
	case GAME_UE2:
	case GAME_VENGEANCE:
		return "Unreal engine 2";
	case GAME_UE2X:
		return "Unreal engine 2X";
	case GAME_UE3:
	case GAME_MIDWAY3:
		return "Unreal engine 3";
	case GAME_UE4_BASE:
		return "Unreal engine 4";
	}
	return "Unknown UE";
}


void PrintGameList(bool tags)
{
	const char *oldTitle = NULL;
	int pos = 0;
#define LINEFEED 80

	int Count = ARRAY_COUNT(GListOfGames) - 1;	// exclude TABLE_END marker
	bool out = false;
	for (int i = 0; i < Count; i++)
	{
		const GameInfo &info = GListOfGames[i];
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
			appPrintf("\n %12s  %s", info.Switch ? info.Switch : "", info.Name);
			continue;
		}
		// simple game list
		if (!(info.Enum & ~GAME_ENGINE) && info.Switch) continue;	// skip simple GAME_UEn
		const char *name = info.Name;
		int len = strlen(name);
		bool needComma = (i < Count - 1) && (GetEngineName(GListOfGames[i+1].Enum) == title);
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


int FindGameTag(const char *name)
{
	int Count = ARRAY_COUNT(GListOfGames) - 1;	// exclude TABLE_END marker
	for (int i = 0; i < Count; i++)
	{
		const char *key = GListOfGames[i].Switch;
		if (!key) continue;
		if (!stricmp(key, name)) return GListOfGames[i].Enum;
	}
#if UNREAL4
	// For UE4 games we use procedurally generated tags
	if (!strnicmp(name, "ue4.", 4))
	{
		const char* ver = name + 4;
		for (const char* s = ver; *s; s++)
		{
			if (!isdigit(*s))
				return -1;
		}
		int nVer = atoi(ver);
		if (nVer > LATEST_SUPPORTED_UE4_VERSION)
		{
			appPrintf("ERROR: provided game tag for UE4 version %d (%s), latest supported version is %d\n", nVer, name, LATEST_SUPPORTED_UE4_VERSION);
			exit(1);
		}
		return GAME_UE4(nVer);
	}
#endif // UNREAL4
	return -1;
}

const char* GetGameTag(int gameEnum)
{
	static char buf[64];

	int Count = ARRAY_COUNT(GListOfGames) - 1;	// exclude TABLE_END marker
	const char* value = NULL;
	for (int i = 0; i < Count; i++)
	{
		if (GListOfGames[i].Enum == gameEnum)
		{
			value = GListOfGames[i].Switch;
			break;
		}
	}
#if UNREAL4
	if (!value && gameEnum >= GAME_UE4_BASE)
	{
		// generate tag
		int ue4ver = GAME_UE4_GET_MINOR(gameEnum);
		if (gameEnum == GAME_UE4(ue4ver))
		{
			// exactly matching, i.e. not a custom UE4 version
			appSprintf(ARRAY_ARG(buf), "ue4.%d", ue4ver);
			return buf;
		}
	}
#endif // UNREAL4

	if (!value)
	{
		appSprintf(ARRAY_ARG(buf), "%X", gameEnum);
		return buf;
	}

	return value;
}


/*-----------------------------------------------------------------------------
	Detecting game by package file version
-----------------------------------------------------------------------------*/

//!! Important notes about separation of this function from FArchive:
//!! - the easiest way: pass FArchive as argument
//!! - FArchive::Platform should be set outside, 'Game' could be set by CreateLoader()
//!!   by analyzing special package tag

void FArchive::DetectGame()
{
	if (GForcePlatform != PLATFORM_UNKNOWN)
		Platform = GForcePlatform;

	if (GForceGame != GAME_UNKNOWN)
	{
		Game = GForceGame;
		return;
	}

	// check if already detected game requires some additional logic
#if LINEAGE2 || EXTEEL
	if (Game == GAME_Lineage2)
	{
		if (ArLicenseeVer >= 1000)	// lineage LicenseeVer < 1000, exteel >= 1000
			Game = GAME_Exteel;
		return;
	}
#endif

	// skip autodetection when Ar.Game is explicitly set by SerializePackageFileSummary, when code detects custom package tag
	if (Game != GAME_UNKNOWN)
		return;

	// here Game == GAME_UNKNOWN
	int check = 0;					// number of detected games; should be 0 or 1, otherwise autodetect is failed
#define SET(game)	{ Game = game; check++; }

	/*-----------------------------------------------------------------------
	 * UE2 games
	 *-----------------------------------------------------------------------*/
	// Digital Extremes games
#if UT2
	if ( ((ArVer >= 117 && ArVer <= 119) && (ArLicenseeVer >= 25 && ArLicenseeVer <= 27)) ||
		  (ArVer == 120 && (ArLicenseeVer == 27 || ArLicenseeVer == 28)) ||
		 ((ArVer >= 121 && ArVer <= 128) && ArLicenseeVer == 29) )
		SET(GAME_UT2);
#endif
#if PARIAH
	if (ArVer == 119 && ArLicenseeVer == 0x9127)
		SET(GAME_Pariah);
#endif
#if UC1
	if (ArVer == 119 && (ArLicenseeVer == 28 || ArLicenseeVer == 30))
		SET(GAME_UC1);
#endif
#if UC2
	if (ArVer == 151 && (ArLicenseeVer == 0 || ArLicenseeVer == 1))
		SET(GAME_UC2);
#endif

#if LOCO
	if ((ArVer >= 131 && ArVer <= 134) && ArLicenseeVer == 29)
		SET(GAME_Loco);
#endif
#if SPLINTER_CELL
	if ( (ArVer == 100 && (ArLicenseeVer >= 9 && ArLicenseeVer <= 17)) ||		// Splinter Cell 1
		 (ArVer == 102 && (ArLicenseeVer >= 29 && ArLicenseeVer <= 28)) )		// Splinter Cell 2
		SET(GAME_SplinterCell);
#endif
#if SWRC
	if ( ArLicenseeVer == 1 && (
		(ArVer >= 133 && ArVer <= 148) || (ArVer >= 154 && ArVer <= 159)
		) )
		SET(GAME_RepCommando);
#endif
#if TRIBES3
	if ( ((ArVer == 129 || ArVer == 130) && (ArLicenseeVer >= 0x17 && ArLicenseeVer <= 0x1B)) ||
		 ((ArVer == 123) && (ArLicenseeVer >= 3    && ArLicenseeVer <= 0xF )) ||
		 ((ArVer == 126) && (ArLicenseeVer >= 0x12 && ArLicenseeVer <= 0x17)) )
		SET(GAME_Tribes3);
#endif
#if BIOSHOCK
	if ( (ArVer == 141 && (ArLicenseeVer == 56 || ArLicenseeVer == 57)) || //?? Bioshock and Bioshock 2
		 (ArVer == 142 && ArLicenseeVer == 56) ||					// Bioshock Remastered
		 (ArVer == 143 && ArLicenseeVer == 59) )					// Bioshock 2 multiplayer, Bioshock 2 Remastered
		SET(GAME_Bioshock);
#endif

	/*-----------------------------------------------------------------------
	 * UE3 games
	 *-----------------------------------------------------------------------*/
	// most UE3 games has single version for all packages
	// here is a list of such games, sorted by version
#if R6VEGAS
	if (ArVer == 241 && ArLicenseeVer == 71)	SET(GAME_R6Vegas2);
#endif
//#if ENDWAR
//	if (ArVer == 329 && ArLicenseeVer == 0)		SET(GAME_EndWar);	// LicenseeVer == 0
//#endif
#if STRANGLE
	if (ArVer == 375 && ArLicenseeVer == 25)	SET(GAME_Strangle);	//!! has extra tag
#endif
#if A51
	if (ArVer == 377 && ArLicenseeVer == 25)	SET(GAME_A51);		//!! has extra tag
#endif
#if WHEELMAN
	if (ArVer == 390 && ArLicenseeVer == 32)	SET(GAME_Wheelman);	//!! has extra tag
#endif
#if FURY
	if (ArVer == 407 && (ArLicenseeVer == 26 || ArLicenseeVer == 36)) SET(GAME_Fury);
#endif
#if MOHA
	if (ArVer == 421 && ArLicenseeVer == 11)	SET(GAME_MOHA);
#endif
#if UNDERTOW
//	if (ArVer == 435 && ArLicenseeVer == 0)		SET(GAME_Undertow);	// LicenseeVer==0!
#endif
#if MCARTA
	if (ArVer == 446 && ArLicenseeVer == 25)	SET(GAME_MagnaCarta);
#endif
#if AVA
	if (ArVer == 451 && (ArLicenseeVer >= 52 || ArLicenseeVer <= 53)) SET(GAME_AVA);
#endif
#if DOH
	if (ArVer == 455 && ArLicenseeVer == 90)	SET(GAME_DOH);
#endif
#if TLR
	if (ArVer == 507 && ArLicenseeVer == 11)	SET(GAME_TLR);
#endif
#if MEDGE
	if (ArVer == 536 && ArLicenseeVer == 43)	SET(GAME_MirrorEdge);
#endif
#if BLOODONSAND
	if (ArVer == 538 && ArLicenseeVer == 73)	SET(GAME_50Cent);
#endif
#if ARGONAUTS
	if (ArVer == 539 && (ArLicenseeVer == 43 || ArLicenseeVer == 47)) SET(GAME_Argonauts);	// Rise of the Argonauts, Thor: God of Thunder
#endif
#if ALPHA_PR
	if (ArVer == 539 && ArLicenseeVer == 91)	SET(GAME_AlphaProtocol);
#endif
#if APB
	if (ArVer == 547 && (ArLicenseeVer == 31 || ArLicenseeVer == 32)) SET(GAME_APB);
#endif
#if LEGENDARY
	if (ArVer == 567 && ArLicenseeVer == 39)	SET(GAME_Legendary);
#endif
//#if AA3
//	if (ArVer == 568 && ArLicenseeVer == 0)		SET(GAME_AA3);	//!! LicenseeVer == 0 ! bad!
//#endif
#if XMEN
	if (ArVer == 568 && ArLicenseeVer == 101)	SET(GAME_XMen);
#endif
#if CRIMECRAFT
	if (ArVer == 576 && ArLicenseeVer == 5)		SET(GAME_CrimeCraft);
#endif
#if BATMAN
	if (ArVer == 576 && ArLicenseeVer == 21)	SET(GAME_Batman);
#endif
#if DARKVOID
	if (ArVer == 576 && (ArLicenseeVer == 61 || ArLicenseeVer == 66)) SET(GAME_DarkVoid); // demo and release
#endif
#if MOH2010
	if (ArVer == 581 && ArLicenseeVer == 58)	SET(GAME_MOH2010);
#endif
#if SINGULARITY
	if (ArVer == 584 && ArLicenseeVer == 126)	SET(GAME_Singularity);
#endif
#if TRON
	if (ArVer == 648 && ArLicenseeVer == 3)		SET(GAME_Tron);
#endif
#if DCU_ONLINE
	if (ArVer == 648 && ArLicenseeVer == 6405)	SET(GAME_DCUniverse);
#endif
#if ENSLAVED
	if (ArVer == 673 && ArLicenseeVer == 2)		SET(GAME_Enslaved);
#endif
#if MORTALONLINE
	if (ArVer == 678 && ArLicenseeVer == 32771) SET(GAME_MortalOnline);
#endif
#if ALICE
	if (ArVer == 690 && ArLicenseeVer == 0)		SET(GAME_Alice); // only this game has LicenseeVer==0 here!
#endif
#if SHADOWS_DAMNED
	if (ArVer == 706 && ArLicenseeVer == 28)	SET(GAME_ShadowsDamned);
#endif
#if DUST514
	if (ArVer == 708 && ArLicenseeVer == 35)	SET(GAME_Dust514);
#endif
#if THIEF4
	if (ArVer == 721 && ArLicenseeVer == 148)	SET(GAME_Thief4);
#endif
#if BIOSHOCK3
	if (ArVer == 727 && ArLicenseeVer == 75)	SET(GAME_Bioshock3);
#endif
#if BULLETSTORM
	if (ArVer == 742 && ArLicenseeVer == 29)	SET(GAME_Bulletstorm);
#endif
#if ALIENS_CM
	if (ArVer == 787 && ArLicenseeVer == 47)	SET(GAME_AliensCM);
#endif
#if DISHONORED
	if (ArVer == 801 && ArLicenseeVer == 30)	SET(GAME_Dishonored);
#endif
#if TRIBES4
	if (ArVer == 805 && ArLicenseeVer == 2)		SET(GAME_Tribes4);
#endif
#if BATMAN
	if (ArVer == 805 && ArLicenseeVer == 101)	SET(GAME_Batman2);
	if ( (ArVer == 806 || ArVer == 807) &&
		 (ArLicenseeVer == 103 || ArLicenseeVer == 137 || ArLicenseeVer == 138) )
		SET(GAME_Batman3);
	if (ArVer == 863 && ArLicenseeVer == 32995)	SET(GAME_Batman4);
#endif
#if DMC
	if (ArVer == 845 && ArLicenseeVer == 4)		SET(GAME_DmC);
#endif
#if XCOM
	if (ArVer == 845 && (ArLicenseeVer >= 101 && ArLicenseeVer <= 107))	SET(GAME_Xcom2);
	if (ArVer == 849 && ArLicenseeVer == 32795)	SET(GAME_XcomB);
#endif
#if FABLE
	if ( (ArVer == 850 || ArVer == 860) && (ArLicenseeVer == 1017 || ArLicenseeVer == 26985) )	// 850 = Fable: The Journey, 860 = Fable Anniversary
		SET(GAME_Fable);
#endif
#if MURDERED
	if (ArVer == 860 && ArLicenseeVer == 93)	SET(GAME_Murdered);
#endif
#if LOST_PLANET3
	if (ArVer == 860 && (ArLicenseeVer == 97 || ArLicenseeVer == 98))	// 97 = Lost Planet 3, 98 = Yaiba: Ninja Gaiden Z
		SET(GAME_LostPlanet3);
#endif
#if GUILTY
	if (ArVer == 868 && ArLicenseeVer == 2)		SET(GAME_Guilty);
#endif
#if SPECIALFORCE2
	if (ArVer == 904 && (ArLicenseeVer == 9 || ArLicenseeVer == 14)) SET(GAME_SpecialForce2);
#endif

	// UE3 games with the various versions of files
#if TUROK
	if ( (ArVer == 374 && ArLicenseeVer == 16) ||
		 (ArVer == 375 && ArLicenseeVer == 19) ||
		 (ArVer == 392 && ArLicenseeVer == 23) ||
		 (ArVer == 393 && (ArLicenseeVer >= 27 && ArLicenseeVer <= 61)) )
		SET(GAME_Turok);
#endif
#if TNA_IMPACT
	if ((ArVer == 380 && ArLicenseeVer == 35) ||		// TNA Impact
		(ArVer == 398 && ArLicenseeVer == 37))			// WWE All Stars
		SET(GAME_TNA);		//!! has extra tag
#endif
#if MASSEFF
	if ((ArVer == 391 && ArLicenseeVer == 92) ||		// XBox 360 version
		(ArVer == 491 && ArLicenseeVer == 1008))		// PC version
		SET(GAME_MassEffect);
	if (ArVer == 512 && ArLicenseeVer == 130)
		SET(GAME_MassEffect2);
	if (ArVer == 684 && (ArLicenseeVer == 185 || ArLicenseeVer == 194)) // 185 = demo, 194 = release
		SET(GAME_MassEffect3);
#endif
#if MKVSDC
	if ( (ArVer == 402 && ArLicenseeVer == 30) ||		//!! has extra tag; MK vs DC
		 (ArVer == 472 && ArLicenseeVer == 46) ||		// Mortal Kombat
		 (ArVer == 573 && ArLicenseeVer == 49) ||		// Injustice: God Among Us
		 (ArVer == 677 && ArLicenseeVer == 157) )		// Mortal Kombat X
		SET(GAME_MK);
#endif
#if HUXLEY
	if ( (ArVer == 402 && (ArLicenseeVer == 0  || ArLicenseeVer == 10)) ||	//!! has extra tag
		 (ArVer == 491 && (ArLicenseeVer >= 13 && ArLicenseeVer <= 16)) ||
		 (ArVer == 496 && (ArLicenseeVer >= 16 && ArLicenseeVer <= 23)) )
		SET(GAME_Huxley);
#endif
#if FRONTLINES
	if ( (ArVer == 433 && ArLicenseeVer == 52) ||		// Frontlines: Fuel of War
		 (ArVer == 576 && ArLicenseeVer == 100) )		// Homefront
		SET(GAME_Frontlines);
#endif
#if ARMYOF2
	if ( (ArVer == 445 && ArLicenseeVer == 79)  ||		// Army of Two
		 (ArVer == 482 && ArLicenseeVer == 222) ||		// Army of Two: the 40th Day
		 (ArVer == 483 && ArLicenseeVer == 4317) )		// ...
		SET(GAME_ArmyOf2);
#endif
#if TRANSFORMERS
	if ( (ArVer == 511 && ArLicenseeVer == 39 ) ||		// The Bourne Conspiracy
		 (ArVer == 511 && ArLicenseeVer == 145) ||		// Transformers: War for Cybertron (PC version)
		 (ArVer == 511 && ArLicenseeVer == 144) ||		// Transformers: War for Cybertron (PS3 and XBox 360 version)
		 (ArVer == 537 && ArLicenseeVer == 174) ||		// Transformers: Dark of the Moon
		 (ArVer == 846 && ArLicenseeVer == 181) )		// Transformers: Fall of Cybertron
		SET(GAME_Transformers);
#endif
#if BORDERLANDS
	if ( (ArVer == 512 && ArLicenseeVer == 35) ||		// Brothers in Arms: Hell's Highway
		 (ArVer == 584 && (ArLicenseeVer == 57 || ArLicenseeVer == 58)) || // Borderlands: release and update
		 (ArVer == 832 && ArLicenseeVer == 46) )		// Borderlands 2
		SET(GAME_Borderlands);
#endif
#if TERA
	if ((ArVer == 568 && (ArLicenseeVer >= 9 && ArLicenseeVer <= 10)) ||
		(ArVer == 610 && (ArLicenseeVer >= 13 && ArLicenseeVer <= 14)))
		SET(GAME_Tera);
#endif
#if REMEMBER_ME
	if ((ArVer == 832 || ArVer == 893) && ArLicenseeVer == 21)	// Remember Me (832) or Life Is Strange (893)
		SET(GAME_RememberMe);
#endif
#if GIGANTIC
	if (ArVer == 867 && ArLicenseeVer == 9)
		SET(GAME_Gigantic);
#endif

	if (check > 1)
		appNotify("DetectGame collision: detected %d titles, Ver=%d, LicVer=%d", check, ArVer, ArLicenseeVer);

	if (Game == GAME_UNKNOWN)
	{
		// generic or unknown engine
		if (ArVer < PACKAGE_V2)
			Game = GAME_UE1;
		else if (ArVer < PACKAGE_V3)
			Game = GAME_UE2;
		else
			Game = GAME_UE3;
		// UE4 has version numbering from zero, plus is has "unversioned" packages, so GAME_UE4_BASE is set by
		// FPackageFileSummary serializer explicitly.
	}
#undef SET
}

#define OVERRIDE_ME1_LVER		90			// real version is 1008, which is greater than LicenseeVersion of Mass Effect 2 and 3
#define OVERRIDE_TRANSFORMERS3	566			// real version is 846
#define OVERRIDE_DUNDEF_VER		685			// smaller than 686 (for FStaticLODModel)
#define OVERRIDE_SF2_VER		700
#define OVERRIDE_SF2_VER2		710
#define OVERRIDE_LIS_VER		832			// >= 832 || < 858 (for UMaterial), < 841 (for USkeletalMesh)


struct UEVersionMap
{
	int		GameTag;
	int		PackageVersion;
};

#define G(game,ver)		{ game, ver },

static const UEVersionMap ueVersions[] =
{
#if ENDWAR
	G(GAME_EndWar, 224)
#endif
#if TERA
	G(GAME_Tera, 568)
#endif
#if HUNTED
	G(GAME_Hunted, 708)						// real version is 709, which is incorrect
#endif
#if DND
	G(GAME_DND, 673)						// real version is 674
#endif
	G(GAME_GoWJ, 828)						// real version is 846
};

#undef G

#if UNREAL4
static const int ue4Versions[] =
{
	VER_UE4_0, VER_UE4_1, VER_UE4_2, VER_UE4_3, VER_UE4_4,
	VER_UE4_5, VER_UE4_6, VER_UE4_7, VER_UE4_8, VER_UE4_9,
	VER_UE4_10, VER_UE4_11, VER_UE4_12, VER_UE4_13, VER_UE4_14,
	VER_UE4_15, VER_UE4_16, VER_UE4_17, VER_UE4_18, VER_UE4_19,
	// NEW_ENGINE_VERSION
};

staticAssert(ARRAY_COUNT(ue4Versions) == LATEST_SUPPORTED_UE4_VERSION + 1, "ue4Versions[] is outdated");
#endif // UNREAL4

void FArchive::OverrideVersion()
{
	if (GForcePackageVersion)
	{
		ArVer = GForcePackageVersion;
		return;
	}

#if UNREAL4
#if LAWBREAKERS
	if (Game == GAME_Lawbreakers)
	{
		// This game uses mostly UE4.13 structures, but has 4.14 package file format. So, game enum
		// is defined as GAME_UE4(13), but we're defining package version 4.14.
		ArVer = VER_UE4_14;
	}
	else
#endif // LAWBREAKERS
	if (Game >= GAME_UE4(0) && Game < GAME_UE4(LATEST_SUPPORTED_UE4_VERSION+1))
	{
		// Special path for UE4, when engine version is specified and packages are unversioned.
		if (ArVer == 0)
		{
			// Override version only if package is unversioned. Mixed versioned and unversioned packages could
			// appear in UE4 game when it has editor support (like UT4).
			ArVer = ue4Versions[GAME_UE4_GET_MINOR(Game)];
		}
		return;
	}
/*	else if (Game == GAME_UE4_BASE && ArVer != 0) //-- disabled because versioned packages provides FCustomVersion info
	{
		// Path for UE4 when packages are versioned.
		for (int i = ARRAY_COUNT(ue4Versions) - 1; i >= 0; i--)
		{
			printf("arv=%d ue4[%d]=%d\n", ArVer, i, ue4Versions[i]);
			if (ArVer >= ue4Versions[i])
			{
				Game = GAME_UE4(i);
				return;
			}
		}
		return;
	} */
#endif // UNREAL4

	// convert game tag to ArVer
	int OldVer  = ArVer;
	int OldLVer = ArLicenseeVer;

	for (int i = 0; i < ARRAY_COUNT(ueVersions); i++)
	{
		if (ueVersions[i].GameTag == Game)
		{
			ArVer = ueVersions[i].PackageVersion;
			break;
		}
	}

#if MASSEFF
	if (Game == GAME_MassEffect) ArLicenseeVer = OVERRIDE_ME1_LVER;
#endif
#if TRANSFORMERS
	if (Game == GAME_Transformers && ArLicenseeVer >= 181) ArVer = OVERRIDE_TRANSFORMERS3; // Transformers: Fall of Cybertron
#endif
#if SPECIALFORCE2
	if (Game == GAME_SpecialForce2)
	{
		// engine for this game is upgraded without changing ArVer, they have ArVer set too high and changind ArLicenseeVer only
		if (ArLicenseeVer >= 14)
			ArVer = OVERRIDE_SF2_VER2;
		else if (ArLicenseeVer == 9)
			ArVer = OVERRIDE_SF2_VER;
	}
#endif // SPECIALFORCE2
#if REMEMBER_ME
	if (Game == GAME_RememberMe)
	{
		if (ArVer > 832) // 832 = Remember Me, higher - Life is Strange
			ArVer = OVERRIDE_LIS_VER;
	}
#endif // REMEMBER_ME
#if DUNDEF
	if (Game == GAME_DunDef)
	{
		if (ArVer >= 686)
			ArVer = OVERRIDE_DUNDEF_VER;
	}
#endif // DUNDEF

	if ((ArVer != OldVer || ArLicenseeVer != OldLVer) && Game < GAME_UE4_BASE)
		appPrintf("Overrided version %d/%d -> %d/%d\n", OldVer, OldLVer, ArVer, ArLicenseeVer);
}
