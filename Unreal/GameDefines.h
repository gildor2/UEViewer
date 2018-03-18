#ifndef __GAME_DEFINES_H__
#define __GAME_DEFINES_H__

// Turn on/off different engine versions support
#define UNREAL1			1
#define UNREAL25		1
#define UNREAL3			1
#define UNREAL4			1

// UE2 (supported by default)
#define UT2				1
//#define PARIAH			1		// not supported, because of major serializer incompatibility
#define SPLINTER_CELL	1
#define LINEAGE2		1
#define SWRC			1		// Star Wars: Republic Commando
#define LOCO			1		// Land of Chaos Online
#define BATTLE_TERR		1		// Battle Territory Online
#define XIII			1

// requires UNREAL1
#define DEUS_EX			1
#define RUNE			1
#define UNDYING			1

// requires UNREAL25
#define TRIBES3			1
#define SWAT4			1
#define RAGNAROK2		1
#define EXTEEL			1
#define AA2				1		// America's Army 2
#define VANGUARD		1		// Vanguard: Saga of Heroes
#define LEAD			1		// UbiSoft LEAD Engine (Splinter Cell: Conviction)
#define EOS				1		// Echo of Soul

// UE2X
#define UC1				1
#define UC2				1

// requires UNREAL3
#define SUPPORT_XBOX360	1		// XBox360 support
#define SUPPORT_IPHONE	1		// iPhone/iPad support
#define SUPPORT_ANDROID	1		// Android support

#define ENDWAR			1		// EndWar
#define BIOSHOCK		1		//!! requires UNREAL3 and TRIBES3
#define DOH				1		// Destroy All Humans
#define ARMYOF2			1		// Army of Two
#define MASSEFF			1		// Mass Effect
#define MEDGE			1		// Mirror's Edge
#define TLR				1		// The Last Remnant
#define TUROK			1
#define R6VEGAS			1		// Rainbow 6 Vegas 2
#define XMEN			1		// XMen: Wolverine
#define FURY			1
#define MCARTA			1		// Magna Carta 2
#define BATMAN			1		// Batman: Arkham Asylum
#define CRIMECRAFT		1		// Crime Craft
#define AVA				1		// AVA Online
#define FRONTLINES		1		// Frontlines: Fuel of War and Homefront
#define BLOODONSAND		1		// 50 Cent: Blood on the Sand
#define BORDERLANDS		1		// Borderlands, Brothers in Arms: Hell's Highway
#define ALIENS_CM		1		// Aliens: Colonial Marines
#define DARKVOID		1		// Dark Void
#define HUXLEY			1
#define AA3				1		// America's Army 3
#define LEGENDARY		1		// Legendary: Pandora's Box
#define TERA			1		// TERA: The Exiled Realm of Arborea
#define BLADENSOUL		1		// Blade & Soul
#define APB				1		// All Points Bulletin
#define ALPHA_PR		1		// Alpha Protocol
#define TRANSFORMERS	1		// Transformers: War for Cybertron, The Bourne Conspiracy, Transformers: Dark of the Moon
#define MORTALONLINE	1		// Mortal Online
#define ENSLAVED		1
#define MOHA			1		// Medal of Honor: Airborne
#define MOH2010			1		// Medal of Honor 2010 (Singleplayer)
#define BERKANIX		1
#define DCU_ONLINE		1		// DC Universe Online
#define BULLETSTORM		1
#define UNDERTOW		1
#define SINGULARITY		1
#define TRON			1		// TRON: Evolution
#define NURIEN			1
#define HUNTED			1		// Hunted: The Demon's Forge
#define DND				1		// Dungeons & Dragons: Daggerdale
#define SHADOWS_DAMNED	1		// Shadows of the Damned
#define ARGONAUTS		1		// Rise of the Argonauts, Thor: God of Thunder
#define GUNLEGEND		1		// Gunslayer Legend
#define SPECIALFORCE2	1		// Special Force 2
#define TAO_YUAN		1
#define TRIBES4			1		// Tribes: Ascend
#define DISHONORED		1
#define FABLE			1		// Fable: The Journey, Fable Anniversary
#define DMC				1		// DmC: Devil May Cry
#define HAWKEN			1
#define STORMWAR		1		// Storm Warriors Online
#define PLA				1		// Passion Leads Army
#define BIOSHOCK3		1		// Bioshock Infinite
#define REMEMBER_ME		1
//#define MARVEL_HEROES	1		-- disabled: were used for custom TFC, which didn't work anyway, plus that code requires update after changes in game engine
#define LOST_PLANET3	1
#define XCOM			1		// The Bureau: XCOM Declassified, XCOM 2
#define THIEF4			1		// Thief
#define MURDERED		1		// Murdered: Soul Suspect
#define SOV				1		// Seal of Vajra
#define DUST514			1		// Dust 514
#define VEC				1		// The Vanishing of Ethan Carter
#define GUILTY			1		// Guilty Gear Xrd
#define ALICE			1		// Alice: Madness Returns
#define GIGANTIC		1
#define MMH7			1		// Might and Magic Heroes 7
#define METRO_CONF		1		// Metro Conflict
#define SMITE			1
#define DUNDEF			1		// Dungeon Defenders
#define DEVILS_THIRD	1		// Devil's Third
//#define USE_XDK			1		// use some proprietary code for XBox360 support

// Midway UE3 games -- make common define ??
#define A51				1		// Blacksite: Area 51
#define WHEELMAN		1		//?? incomplete
#define MKVSDC			1		// Mortal Kombat vs. DC Universe, Mortal Kombat, Injustice: Gods Among Us
#define STRANGLE		1		// Stranglehold
#define TNA_IMPACT		1		// TNA iMPACT!

// UE4
#define GEARS4			1		// Gears of War 4
#define FRIDAY13		1		// Friday the 13th: The Game
#define TEKKEN7			1		// Tekken 7
#define LAWBREAKERS		1		// Lawbreakers
#define PARAGON			1		// Paragon
#define HIT				1		// Heroes of Incredible Tales

#define SPECIAL_TAGS	1		// games with different PACKAGE_FILE_TAG

#endif // __GAME_DEFINES_H__
