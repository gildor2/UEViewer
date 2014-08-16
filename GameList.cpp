#include "Core.h"
#include "UnCore.h"
#include "GameList.h"

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

	// Unreal engine 3
#if UNREAL3
		G("Unreal engine 3", ue3, GAME_UE3),
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

	// Unreal engine 4
#if UNREAL4
		G("Unreal engine 4", ue4, GAME_UE4),
#endif // UNREAL4

	// end marker
	TABLE_END
};

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
	case GAME_UE4:
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
			appPrintf("\n %8s  %s", info.Switch ? info.Switch : "", info.Name);
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
	return -1;
}
