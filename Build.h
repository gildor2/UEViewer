#define DO_GUARD		1
#define RENDERING		1
#define PROFILE			1

//#define PRIVATE_BUILD	1

// Support for other games

// Turn on/off different engine versions support
#define UNREAL1			1
#define UNREAL25		1
#define UNREAL3			1

// UE2 (supported by default)
#define UT2				1
//#define PARIAH			1		// not supported, because of major serializer incompatibility
#define SPLINTER_CELL	1
#define LINEAGE2		1

// requires UNREAL1
#define DEUS_EX			1
#define RUNE			1

// requires UNREAL25
#define TRIBES3			1
///#define RAGNAROK2		1
#define EXTEEL			1

// UE2X
#define UC2				1

// requires UNREAL3
#define XBOX360			1		// XBox360 resources
#define BIOSHOCK		1		//!! requires UNREAL3 and TRIBES3
#define A51				1		// Blacksite: Area 51
#define WHEELMAN		1
#define MKVSDC			1		// Mortal Kombat vs. DC Universe
#define ARMYOF2			1		// Army of Two
#define MASSEFF			1		// Mass Effect
#define MEDGE			1		// Mirror's Edge
#define TLR				1		// The Last Remnant
///#define HUXLEY			1
//#define USE_XDK			1		// use some proprietary code for XBox360 support

// some private games
#if PRIVATE_BUILD
#	define RAGNAROK2	1
#	define HUXLEY		1
#endif
