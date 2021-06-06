#include "Core.h"
#include "UnCore.h"
#include "UnPackage.h"

#if UNREAL3

void FPackageFileSummary::Serialize3(FArchive &Ar)
{
	guard(FPackageFileSummary::Serialize3);

#if BATMAN
	if (Ar.Game == GAME_Batman4)
	{
		Ar.ArLicenseeVer  &= 0x7FFF;			// higher bit is used for something else, and it's set to 1
		LicenseeVersion &= 0x7FFF;
	}
#endif // BATMAN
#if R6VEGAS
	if (Ar.Game == GAME_R6Vegas2)
	{
		int32 unk8, unkC;
		if (Ar.ArLicenseeVer >= 48) Ar << unk8;
		if (Ar.ArLicenseeVer >= 49) Ar << unkC;
	}
#endif // R6VEGAS
#if HUXLEY
	if (Ar.Game == GAME_Huxley && Ar.ArLicenseeVer >= 8)
	{
		int32 skip;
		Ar << skip;								// 0xFEFEFEFE
		if (Ar.ArLicenseeVer >= 17)
		{
			int unk8;							// unknown used field
			Ar << unk8;
		}
	}
#endif // HUXLEY
#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers)
	{
		if (Ar.ArLicenseeVer >= 181)
		{
			int32 unk1, unk2[3];
			Ar << unk1 << unk2[0] << unk2[1] << unk2[2];
		}
		if (Ar.ArLicenseeVer >= 55)
		{
			int32 unk8;							// always 0x4BF1EB6B? (not true for later game versions)
			Ar << unk8;
		}
	}
#endif // TRANSFORMERS
#if MORTALONLINE
	if (Ar.Game == GAME_MortalOnline && Ar.ArLicenseeVer >= 1)
	{
		int32 unk8;
		Ar << unk8;								// always 0?
	}
#endif
#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3 && Ar.ArLicenseeVer >= 66)
	{
		int32 unkC;
		Ar << unkC;
	}
#endif

	if (Ar.ArVer >= 249)
		Ar << HeadersSize;
	else
		HeadersSize = 0;

	// NOTE: A51 and MKVSDC has exactly the same code paths!
#if A51 || WHEELMAN || MKVSDC || STRANGLE || TNA_IMPACT			//?? special define ?
	int midwayVer = 0;
	if (Ar.Engine() == GAME_MIDWAY3 && Ar.ArLicenseeVer >= 2)	//?? Wheelman not checked
	{
		// Tag == "A52 ", "MK8 ", "MK  ", "WMAN", "WOO " (Stranglehold), "EPIC", "TNA ", "KORE", "MK10"
		int32 Tag;
		int32 unk10;
		FGuid unkGuid;
		Ar << Tag << midwayVer;
		if (Ar.Game == GAME_Strangle && midwayVer >= 256)
			Ar << unk10;
		if (Ar.Game == GAME_MK && Ar.ArVer >= 668) // MK X
			Ar << unk10;
		if (Ar.Game == GAME_MK && Ar.ArVer >= 596)
			Ar << unkGuid;
	}
#endif // MIDWAY

	if (Ar.ArVer >= 269)
		Ar << PackageGroup;

	Ar << PackageFlags;

#if MASSEFF
	if ((((Ar.Game == GAME_MassEffect3 || Ar.Game == GAME_MassEffectLE) && Ar.ArLicenseeVer >= 194)) && (PackageFlags & PKG_Cooked)) // ME3 or ME3LE
	{
		int32 unk88;
		Ar << unk88;
	}
#endif // MASSEFF
#if HAWKEN
	if (Ar.Game == GAME_Hawken && Ar.ArLicenseeVer >= 2)
	{
		int32 unkVer;
		Ar << unkVer;
	}
#endif // HAWKEN
#if GIGANTIC
	if (Ar.Game == GAME_Gigantic && Ar.ArLicenseeVer >= 2)
	{
		int32 unk;
		Ar << unk;
	}
#endif // GIGANTIC

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		guard(SerializeMK10);

		// MK X, no explicit version
		int64 NameOffset64, ExportOffset64, ImportOffset64;
		int64 unk112, unk136, unk120, unk144;
		int   unk84, unk128, unk132, unk168;
		FGuid unk152;
		Ar << NameCount << NameOffset64 << ExportCount << ExportOffset64 << ImportCount << ImportOffset64;
		NameOffset   = (int)NameOffset64;
		ExportOffset = (int)ExportOffset64;
		ImportOffset = (int)ImportOffset64;
		Ar << unk84 << unk112 << unk136 << unk120 << unk128 << unk132 << unk144;
		Ar << unk152;
		Ar << unk168;

		Ar << EngineVersion << CompressionFlags << CompressedChunks;

		// drop everything else in FPackageFileSummary
		return;

		unguard;
	}
#endif // MKVSDC

	Ar << NameCount << NameOffset << ExportCount << ExportOffset;

#if APB
	if (Ar.Game == GAME_APB)
	{
		int32 unk2C;
		float unk30[5];
		if (Ar.ArLicenseeVer >= 29) Ar << unk2C;
		if (Ar.ArLicenseeVer >= 28) Ar << unk30[0] << unk30[1] << unk30[2] << unk30[3] << unk30[4];
	}
#endif // APB

	Ar << ImportCount << ImportOffset;

#if MKVSDC
	if (Ar.Game == GAME_MK)
	{
		int32 unk3C, unk40, unk54;
		int32 unk44, unk48, unk4C;
		if (Ar.ArVer >= 524)		// Injustice
			Ar << unk3C;
		if (midwayVer >= 16)
			Ar << unk3C;
		if (Ar.ArVer >= 391)
			Ar << unk40;
		if (Ar.ArVer >= 482)		// Injustice
			Ar << unk44 << unk48 << unk4C;
		if (Ar.ArVer >= 484)		// Injustice
			Ar << unk54;
		if (Ar.ArVer >= 472)
		{
			// Mortal Kombat, Injustice:
			// - no DependsOffset
			// - no generations (since version 446)
			Ar << Guid;
			DependsOffset = 0;
			goto engine_version;
		}
	}
#endif // MKVSDC
#if WHEELMAN
	if (Ar.Game == GAME_Wheelman && midwayVer >= 23)
	{
		int32 unk3C;
		Ar << unk3C;
	}
#endif // WHEELMAN
#if STRANGLE
	if (Ar.Game == GAME_Strangle && Ar.ArVer >= 375)
	{
		int32 unk40;
		Ar << unk40;
	}
#endif // STRANGLE
#if TERA
	// de-obfuscate NameCount for Tera
	if (Ar.Game == GAME_Tera && (PackageFlags & PKG_Cooked)) NameCount -= NameOffset;
#endif

	if (Ar.ArVer >= 415)
		Ar << DependsOffset;

#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3) goto read_unk38;
#endif

#if DUNDEF
	if (Ar.Game == GAME_DunDef)
	{
		if (PackageFlags & PKG_Cooked)
		{
			int32 unk38;
			Ar << unk38;
		}
	}
#endif // DUNDEF

	if (Ar.ArVer >= 623)
		Ar << f38 << f3C << f40;

#if GEARSU
	if (Ar.Game == GAME_GoWU) goto guid;
#endif

#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers && Ar.ArVer >= 535) goto read_unk38;
#endif

	if (Ar.ArVer >= 584)
	{
	read_unk38:
		int32 unk38;
		Ar << unk38;
	}

	// GUID
guid:
	Ar << Guid;

	// Generations
	int32 Count;
	Ar << Count;

#if APB
	if (Ar.Game == GAME_APB && Ar.ArLicenseeVer >= 32)
	{
		FGuid Guid2;
		Ar << Guid2;
	}
#endif // APB

	Generations.Empty(Count);
	Generations.AddZeroed(Count);
	for (int i = 0; i < Count; i++)
		Ar << Generations[i];

#if ALIENS_CM
	if (Ar.Game == GAME_AliensCM)
	{
		uint16 unk64, unk66, unk68;		// complex EngineVersion?
		Ar << unk64 << unk66 << unk68;
		goto cooker_version;
	}
#endif // ALIENS_CM

engine_version:
	if (Ar.ArVer >= 245)
		Ar << EngineVersion;
cooker_version:
	if (Ar.ArVer >= 277)
		Ar << CookerVersion;

#if MASSEFF
	// ... MassEffect has some additional structure here ...
	if (Ar.Game >= GAME_MassEffect && Ar.Game <= GAME_MassEffectLE)
	{
		int32 unk1, unk2, unk3[2], unk4[2];
		if (Ar.ArLicenseeVer >= 16 && Ar.ArLicenseeVer < 136)
			Ar << unk1;					// random value, ME1&2
		if (Ar.ArLicenseeVer >= 32 && Ar.ArLicenseeVer < 136)
			Ar << unk2;					// unknown, ME1&2
		if (Ar.ArLicenseeVer >= 35 && Ar.ArLicenseeVer < 113)	// ME1
		{
			TMap<FString, TArray<FString> > unk5;
			Ar << unk5;
		}
		if (Ar.ArLicenseeVer >= 37)
			Ar << unk3[0] << unk3[1];	// 2 ints: 1, 0
		if (Ar.ArLicenseeVer >= 39 && Ar.ArLicenseeVer < 136)
			Ar << unk4[0] << unk4[1];	// 2 ints: -1, -1 (ME1&2)
	}
#endif // MASSEFF

	if (Ar.ArVer >= 334)
		Ar << CompressionFlags << CompressedChunks;
	if (Ar.ArVer >= 482)
		Ar << U3unk60;
//	if (Ar.ArVer >= 516)
//		Ar << some array ... (U3unk70)

	// ... MassEffect has additional field here ...
	// if (Ar.Game == GAME_MassEffect() && Ar.ArLicenseeVer >= 44) serialize 1*int

	unguard;
}

void FObjectExport::Serialize3(FArchive &Ar)
{
	guard(FObjectExport::Serialize3);

#if USE_COMPACT_PACKAGE_STRUCTS
	// locally declare FObjectImport data which are stripped
	int32		SuperIndex;
	uint32		ObjectFlags;
	unsigned	ObjectFlags2;
	int			Archetype;
	FGuid		Guid;
	int			U3unk6C;
	TStaticArray<int, 16> NetObjectCount;
#endif // USE_COMPACT_PACKAGE_STRUCTS

#if AA3
	int AA3Obfuscator = 0;
	if (Ar.Game == GAME_AA3)
	{
		//!! if (Package->Summary.PackageFlags & 0x200000)
		Ar << AA3Obfuscator;	// random value
	}
#endif // AA3

#if WHEELMAN
	if (Ar.Game == GAME_Wheelman)
	{
		// Wheelman has special code for quick serialization of FObjectExport struc
		// using a single Serialize(&S, 0x64) call
		// Ar.MidwayVer >= 22; when < 22 => standard version w/o ObjectFlags
		int tmp1, tmp2, tmp3;
		Ar << tmp1;	// 0 or 1
		Ar << ObjectName << PackageIndex << ClassIndex << SuperIndex << Archetype;
		Ar << ObjectFlags << ObjectFlags2 << SerialSize << SerialOffset;
		Ar << tmp2; // zero ?
		Ar << tmp3; // -1 ?
		Ar.Seek(Ar.Tell() + 0x14);	// skip raw version of ComponentMap
		Ar << ExportFlags;
		Ar.Seek(Ar.Tell() + 0xC); // skip raw version of NetObjectCount
		Ar << Guid;
		return;
	}
#endif // WHEELMAN

	Ar << ClassIndex << SuperIndex << PackageIndex << ObjectName;
	if (Ar.ArVer >= 220) Ar << Archetype;

#if BATMAN
	if ((Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4) && Ar.ArLicenseeVer >= 89)
	{
		int unk18;
		Ar << unk18;
	}
#endif
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 573)	// Injustice, version unknown
	{
		int unk18;
		Ar << unk18;
		if (Ar.ArVer >= 677)
		{
			// MK X, version unknown
			int unk1, unk2, unk3, unk4;
			Ar << unk1 << unk2 << unk3 << unk4;
		}
	}
#endif

	Ar << ObjectFlags;
	if (Ar.ArVer >= 195) Ar << ObjectFlags2;	// qword flags after version 195
	Ar << SerialSize;
	if (SerialSize || Ar.ArVer >= 249)
		Ar << SerialOffset;

#if GEARSU
	if (Ar.Game == GAME_GoWU)
	{
		int32 unk;
		Ar << unk;
		if (unk) Ar.Seek(Ar.Tell() + unk * 12);
	}
#endif // GEARSU

#if HUXLEY
	if (Ar.Game == GAME_Huxley && Ar.ArLicenseeVer >= 22)
	{
		int unk24;
		Ar << unk24;
	}
#endif // HUXLEY
#if ALPHA_PR
	if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 53) goto ue3_export_flags;	// no ComponentMap
#endif
#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 37) goto ue3_export_flags;	// no ComponentMap
#endif
#if MKVSDC
	if (Ar.Game == GAME_MK)
	{
		if (Ar.ArVer >= 677)
		{
			// MK X has 64-bit SerialOffset, skip HIDWORD
			int SerialOffsetUpper;
			Ar << SerialOffsetUpper;
			assert(SerialOffsetUpper == 0);
		}
		if (Ar.ArVer >= 573) goto ue3_component_map; // Injustice, version unknown
	}
#endif // MKVSDC
#if ROCKET_LEAGUE
	if (Ar.Game == GAME_RocketLeague && Ar.ArLicenseeVer >= 22)
	{
		// Rocket League has 64-bit SerialOffset in LicenseeVer >= 22, skip HIDWORD
		int32 SerialOffsetUpper;
		Ar << SerialOffsetUpper;
		assert(SerialOffsetUpper == 0);
	}
#endif // ROCKET_LEAGUE

	if (Ar.ArVer < 543)
	{
	ue3_component_map:
		TStaticMap<FName, int, 16> tmpComponentMap;
		Ar << tmpComponentMap;
	}
ue3_export_flags:
	if (Ar.ArVer >= 247) Ar << ExportFlags;

#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 116)
	{
		// version prior 116
		byte someFlag;
		Ar << someFlag;
		if (!someFlag) return;
		// else - continue serialization of remaining fields
	}
#endif // TRANSFORMERS
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 446)
	{
		// removed generations (NetObjectCount)
		Ar << Guid;
		return;
	}
#endif // MKVSDC
#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3)
	{
		int flag;
		Ar << flag;
		if (!flag) return;				// stripped some fields
	}
#endif // BIOSHOCK3

	if (Ar.ArVer >= 322) Ar << NetObjectCount << Guid;

#if UNDERTOW
	if (Ar.Game == GAME_Undertow && Ar.ArVer >= 431) Ar << U3unk6C;	// partially upgraded?
#endif
#if ARMYOF2
	if (Ar.Game == GAME_ArmyOf2) return;
#endif

	if (Ar.ArVer >= 475) Ar << U3unk6C;

#if AA3
	if (Ar.Game == GAME_AA3)
	{
		// deobfuscate data
		ClassIndex   ^= AA3Obfuscator;
		SuperIndex   ^= AA3Obfuscator;
		PackageIndex ^= AA3Obfuscator;
		Archetype    ^= AA3Obfuscator;
		SerialSize   ^= AA3Obfuscator;
		SerialOffset ^= AA3Obfuscator;
	}
#endif // AA3
#if THIEF4
	if (Ar.Game == GAME_Thief4)
	{
		int unk5C;
		if (ExportFlags & 8) Ar << unk5C;
	}
#endif // THIEF4

#undef LOC
	unguard;
}

void UnPackage::LoadNameTable3()
{
	guard(UnPackage::LoadNameTable3);

	FStaticString<MAX_FNAME_LEN> nameStr;

	for (int i = 0; i < Summary.NameCount; i++)
	{
		guard(Name);

#if DCU_ONLINE
		if (Game == GAME_DCUniverse)		// no version checking
		{
			char buf[MAX_FNAME_LEN];
			int len;
			*this << len;
			assert(len > 0 && len < 0x3FF);	// requires extra code
			assert(len < ARRAY_COUNT(buf));
			Serialize(buf, len);
			buf[len] = 0;
			NameTable[i] = appStrdupPool(buf);
			goto qword_flags;
		}
#endif // DCU_ONLINE
#if R6VEGAS
		if (Game == GAME_R6Vegas2 && ArLicenseeVer >= 71)
		{
			char buf[256];
			byte len;
			*this << len;
			Serialize(buf, len);
			buf[len] = 0;
			NameTable[i] = appStrdupPool(buf);
			goto done;
		}
#endif // R6VEGAS
#if TRANSFORMERS
		if (Game == GAME_Transformers && ArLicenseeVer >= 181) // Transformers: Fall of Cybertron; no real version in code
		{
			char buf[MAX_FNAME_LEN];
			int len;
			*this << len;
			assert(len < ARRAY_COUNT(buf));
			Serialize(buf, len);
			buf[len] = 0;
			NameTable[i] = appStrdupPool(buf);
			goto qword_flags;
		}
#endif // TRANSFORMERS

		*this << nameStr;

#if AVA
		if (Game == GAME_AVA)
		{
			// Strange code - package contains some bytes:
			// V(0) = len ^ 0x3E
			// V(i) = V(i-1) + 0x48 ^ 0xE1
			// Number of bytes = (len ^ 7) & 0xF
			int skip = nameStr.Len();
			skip = (skip ^ 7) & 0xF;
			Seek(Tell() + skip);
		}
#endif // AVA

		VerifyName(nameStr, i);

		// Remember the name
		NameTable[i] = appStrdupPool(*nameStr);

#if WHEELMAN
		if (Game == GAME_Wheelman) goto dword_flags;
#endif
#if MASSEFF
		if (Game >= GAME_MassEffect && Game <= GAME_MassEffectLE)
		{
			if (ArLicenseeVer >= 142) goto done;			// ME3, no flags
			if (ArLicenseeVer >= 102) goto dword_flags;		// ME2
		}
#endif // MASSEFF
#if MKVSDC
		if (Game == GAME_MK && ArVer >= 677) goto done;		// no flags for MK X
#endif
#if METRO_CONF
		if (Game == GAME_MetroConflict)
		{
			int TrashLen = 0;
			if (ArLicenseeVer < 3)
			{
			}
			else if (ArLicenseeVer < 16)
			{
				TrashLen = nameStr.Len() ^ 7;
			}
			else
			{
				TrashLen = nameStr.Len() ^ 6;
			}
			this->Seek(this->Tell() + (TrashLen & 0xF));
		}
#endif // METRO_CONF

		// Generic UE3
		if (ArVer >= 195)
		{
		qword_flags:
			// Object flags are 64-bit in UE3
			uint64 flags64;
			*this << flags64;
		}
		else
		{
		dword_flags:
			uint32 flags32;
			*this << flags32;
		}

	done: ;
#if DEBUG_PACKAGE
		PKG_LOG("Name[%d]: \"%s\"\n", i, NameTable[i]);
#endif
		unguardf("%d", i);
	}

	unguard;
}


#endif // UNREAL3
