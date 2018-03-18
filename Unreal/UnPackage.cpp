#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"

#include "GameDatabase.h"		// for GetGameTag()

byte GForceCompMethod = 0;		// COMPRESS_...


//#define DEBUG_PACKAGE			1
//#define PROFILE_PACKAGE_TABLES	1

#define MAX_FNAME_LEN			MAX_PACKAGE_PATH

/*-----------------------------------------------------------------------------
	Unreal package structures
-----------------------------------------------------------------------------*/

static void SerializePackageFileSummary2(FArchive &Ar, FPackageFileSummary &S)
{
	guard(SerializePackageFileSummary2);

#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell && Ar.ArLicenseeVer >= 83)
	{
		int32 unk8;
		Ar << unk8;
	}
#endif // SPLINTER_CELL

	Ar << S.PackageFlags;

#if LEAD
	if (Ar.Game == GAME_SplinterCellConv)
	{
		int32 unk10;
		Ar << unk10;		// some flags?
	}
#endif // LEAD

	Ar << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset << S.ImportCount << S.ImportOffset;

#if LEAD
	if (Ar.Game == GAME_SplinterCellConv && Ar.ArLicenseeVer >= 48)
	{
		// this game has additional name table for some packages
		int32 ExtraNameCount, ExtraNameOffset;
		int32 unkC;
		Ar << ExtraNameCount << ExtraNameOffset;
		if (ExtraNameOffset < S.ImportOffset) ExtraNameCount = 0;
		if (Ar.ArLicenseeVer >= 85) Ar << unkC;
		goto generations;	// skip Guid
	}
#endif // LEAD
#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell)
	{
		int32 tmp1;
		TArray<byte> tmp2;
		Ar << tmp1;								// 0xFF0ADDE
		Ar << tmp2;
	}
#endif // SPLINTER_CELL
#if RAGNAROK2
	if ((GForceGame == 0) && (S.PackageFlags & 0x10000) && (Ar.ArVer >= 0x80 && Ar.ArVer < 0x88))	//?? unknown upper limit; known lower limit: 0x80
	{
		// encrypted Ragnarok Online archive header (data taken by archive analysis)
		Ar.Game = GAME_Ragnarok2;
		S.NameCount    ^= 0xD97790C7 ^ 0x1C;
		S.NameOffset   ^= 0xF208FB9F ^ 0x40;
		S.ExportCount  ^= 0xEBBDE077 ^ 0x04;
		S.ExportOffset ^= 0xE292EC62 ^ 0x03E9E1;
		S.ImportCount  ^= 0x201DA87A ^ 0x05;
		S.ImportOffset ^= 0xA9B999DF ^ 0x003E9BE;
		return;									// other data is useless for us, and they are encrypted too
	}
#endif // RAGNAROK2
#if EOS
	if (Ar.Game == GAME_EOS && Ar.ArLicenseeVer >= 49) goto generations;
#endif

	// Guid and generations
	if (Ar.ArVer < 68)
	{
		// old generations code
		int32 HeritageCount, HeritageOffset;
		Ar << HeritageCount << HeritageOffset;	// not used
		if (Ar.IsLoading)
		{
			S.Generations.Empty(1);
			FGenerationInfo gen;
			gen.ExportCount = S.ExportCount;
			gen.NameCount   = S.NameCount;
			S.Generations.Add(gen);
		}
	}
	else
	{
		Ar << S.Guid;
		// current generations code
	generations:
		// always used int for generation count (even for UE1-2)
		int32 Count;
		Ar << Count;
		S.Generations.Empty(Count);
		S.Generations.AddZeroed(Count);
		for (int i = 0; i < Count; i++)
			Ar << S.Generations[i];
	}

	unguard;
}


#if UNREAL3

static void SerializePackageFileSummary3(FArchive &Ar, FPackageFileSummary &S)
{
	guard(SerializePackageFileSummary3);

#if BATMAN
	if (Ar.Game == GAME_Batman4)
	{
		Ar.ArLicenseeVer  &= 0x7FFF;			// higher bit is used for something else, and it's set to 1
		S.LicenseeVersion &= 0x7FFF;
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
		Ar << S.HeadersSize;
	else
		S.HeadersSize = 0;

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
		Ar << S.PackageGroup;

	Ar << S.PackageFlags;

#if MASSEFF
	if (Ar.Game == GAME_MassEffect3 && Ar.ArLicenseeVer >= 194 && (S.PackageFlags & 8))
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
		Ar << S.NameCount << NameOffset64 << S.ExportCount << ExportOffset64 << S.ImportCount << ImportOffset64;
		S.NameOffset   = (int)NameOffset64;
		S.ExportOffset = (int)ExportOffset64;
		S.ImportOffset = (int)ImportOffset64;
		Ar << unk84 << unk112 << unk136 << unk120 << unk128 << unk132 << unk144;
		Ar << unk152;
		Ar << unk168;

		Ar << S.EngineVersion << S.CompressionFlags << S.CompressedChunks;

		// drop everything else in FPackageFileSummary
		return;

		unguard;
	}
#endif // MKVSDC

	Ar << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset;

#if APB
	if (Ar.Game == GAME_APB)
	{
		int32 unk2C;
		float unk30[5];
		if (Ar.ArLicenseeVer >= 29) Ar << unk2C;
		if (Ar.ArLicenseeVer >= 28) Ar << unk30[0] << unk30[1] << unk30[2] << unk30[3] << unk30[4];
	}
#endif // APB

	Ar << S.ImportCount << S.ImportOffset;

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
			Ar << S.Guid;
			S.DependsOffset = 0;
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
	if (Ar.Game == GAME_Tera && (S.PackageFlags & 8)) S.NameCount -= S.NameOffset;
#endif

	if (Ar.ArVer >= 415)
		Ar << S.DependsOffset;

#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3) goto read_unk38;
#endif

#if DUNDEF
	if (Ar.Game == GAME_DunDef)
	{
		if (S.PackageFlags & 8)
		{
			int32 unk38;
			Ar << unk38;
		}
	}
#endif // DUNDEF

	if (Ar.ArVer >= 623)
		Ar << S.f38 << S.f3C << S.f40;

#if TRANSFORMERS
	if (Ar.Game == GAME_Transformers && Ar.ArVer >= 535) goto read_unk38;
#endif

	if (Ar.ArVer >= 584)
	{
	read_unk38:
		int32 unk38;
		Ar << unk38;
	}

	// Guid and generations
	Ar << S.Guid;
	int32 Count;
	Ar << Count;

#if APB
	if (Ar.Game == GAME_APB && Ar.ArLicenseeVer >= 32)
	{
		FGuid Guid2;
		Ar << Guid2;
	}
#endif // APB

	S.Generations.Empty(Count);
	S.Generations.AddZeroed(Count);
	for (int i = 0; i < Count; i++)
		Ar << S.Generations[i];

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
		Ar << S.EngineVersion;
cooker_version:
	if (Ar.ArVer >= 277)
		Ar << S.CookerVersion;

#if MASSEFF
	// ... MassEffect has some additional structure here ...
	if (Ar.Game >= GAME_MassEffect && Ar.Game <= GAME_MassEffect3)
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
		Ar << S.CompressionFlags << S.CompressedChunks;
	if (Ar.ArVer >= 482)
		Ar << S.U3unk60;
//	if (Ar.ArVer >= 516)
//		Ar << some array ... (U3unk70)

	// ... MassEffect has additional field here ...
	// if (Ar.Game == GAME_MassEffect() && Ar.ArLicenseeVer >= 44) serialize 1*int

	unguard;
}

#endif // UNREAL3


#if UNREAL4

struct FEnumCustomVersion
{
	int32			Tag;
	int32			Version;

	friend FArchive& operator<<(FArchive& Ar, FEnumCustomVersion& V)
	{
		return Ar << V.Tag << V.Version;
	}
};

struct FGuidCustomVersion
{
	FGuid			Key;
	int32			Version;
	FString			FriendlyName;

	friend FArchive& operator<<(FArchive& Ar, FGuidCustomVersion& V)
	{
		return Ar << V.Key << V.Version << V.FriendlyName;
	}
};

void FCustomVersionContainer::Serialize(FArchive& Ar, int LegacyVersion)
{
	if (LegacyVersion == -2)
	{
		// Before 4.0 release: ECustomVersionSerializationFormat::Enums in Core/Private/Serialization/CustomVersion.cpp
		TArray<FEnumCustomVersion> VersionsEnum;
		Ar << VersionsEnum;
	}
	else if (LegacyVersion < -2 && LegacyVersion >= -5)
	{
		// 4.0 .. 4.10: ECustomVersionSerializationFormat::Guids
		TArray<FGuidCustomVersion> VersionsGuid;
		Ar << VersionsGuid;
	}
	else
	{
		// Starting with 4.11: ECustomVersionSerializationFormat::Optimized
		Ar << Versions;
	}
}

int GetUE4CustomVersion(const FArchive& Ar, const FGuid& Guid)
{
	guard(GetUE4CustomVersion);
	const UnPackage* Package = Ar.CastTo<UnPackage>();
	assert(Package);
	const FPackageFileSummary &S = Package->Summary;
	for (int i = 0; i < S.CustomVersionContainer.Versions.Num(); i++)
	{
		const FCustomVersion& V = S.CustomVersionContainer.Versions[i];
		if (V.Key == Guid)
		{
			return V.Version;
		}
	}
	return -1;
	unguard;
}

static void SerializePackageFileSummary4(FArchive &Ar, FPackageFileSummary &S)
{
	guard(SerializePackageFileSummary4);

	// LegacyVersion: contains negative value.
	// LegacyVersion to engine version map:
	static const int legacyVerToEngineVer[] =
	{
		-1,		// -1
		-1,		// -2 -> older than UE4.0, no mapping
		0,		// -3 -> UE4.0
		7,		// -4 -> UE4.7
		7,		// -5 ...
		11,		// -6
		14,		// -7
		// add new versions above this line
		LATEST_SUPPORTED_UE4_VERSION+1				// this line here is just to make code below simpler
	};

	static const int LatestSupportedLegacyVer = -ARRAY_COUNT(legacyVerToEngineVer)+1;
//	appPrintf("%d -> %d %d < %d\n", S.LegacyVersion, -S.LegacyVersion - 1, legacyVerToEngineVer[-S.LegacyVersion - 1], LatestSupportedLegacyVer);
	if (S.LegacyVersion < LatestSupportedLegacyVer || S.LegacyVersion >= -1)	// -2 is supported
		appError("UE4 LegacyVersion: unsupported value %d", S.LegacyVersion);

	S.IsUnversioned = false;

	// read versions
	int32 VersionUE3, Version, LicenseeVersion;		// note: using int32 instead of uint16 as in UE1-UE3
	if (S.LegacyVersion != -4)						// UE4 had some changes for version -4, but these changes were reverted in -5 due to some problems
		Ar << VersionUE3;
	Ar << Version << LicenseeVersion;
	// VersionUE3 is ignored
	assert((Version & ~0xFFFF) == 0);
	assert((LicenseeVersion & ~0xFFFF) == 0);
	S.FileVersion     = Version & 0xFFFF;
	S.LicenseeVersion = LicenseeVersion & 0xFFFF;

	// store file version to archive
	Ar.ArVer         = S.FileVersion;
	Ar.ArLicenseeVer = S.LicenseeVersion;

	if (S.FileVersion == 0 && S.LicenseeVersion == 0)
		S.IsUnversioned = true;

	if (S.IsUnversioned && GForceGame == GAME_UNKNOWN)
	{
		int ver = -S.LegacyVersion - 1;
		int verMin = legacyVerToEngineVer[ver];
		int verMax = legacyVerToEngineVer[ver+1] - 1;
		int selectedVersion;
		if (verMax < verMin)
		{
			// if LegacyVersion exactly matches single engine version, don't show any UI
			selectedVersion = verMin;
		}
		else
		{
			// display UI if it is supported
			selectedVersion = UE4UnversionedPackage(verMin, verMax);
			assert(selectedVersion >= 0 && selectedVersion <= LATEST_SUPPORTED_UE4_VERSION);
		}
		GForceGame = GAME_UE4(selectedVersion);
	}

	// detect game
	Ar.DetectGame();
	Ar.OverrideVersion();

	if (S.LegacyVersion <= -2)
	{
		// CustomVersions array - not serialized to unversioned packages, and UE4 always consider
		// all custom versions to use highest available value. However this is used for versioned
		// packages: engine starts to use custom versions heavily starting with 4.12.
		S.CustomVersionContainer.Serialize(Ar, S.LegacyVersion);
	}

	Ar << S.HeadersSize;
	Ar << S.PackageGroup;
	Ar << S.PackageFlags;

	Ar << S.NameCount << S.NameOffset;

	if (Ar.ArVer >= VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID && Ar.ContainsEditorData())
	{
		FString LocalizationId;
		Ar << LocalizationId;
	}

	if (Ar.ArVer >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
	{
		int32 GatherableTextDataCount, GatherableTextDataOffset;
		Ar << GatherableTextDataCount << GatherableTextDataOffset;
	}

	Ar << S.ExportCount << S.ExportOffset << S.ImportCount << S.ImportOffset;
	Ar << S.DependsOffset;

	if (Ar.ArVer >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
	{
		int32 StringAssetReferencesCount, StringAssetReferencesOffset;
		Ar << StringAssetReferencesCount << StringAssetReferencesOffset;
	}

	if (Ar.ArVer >= VER_UE4_ADDED_SEARCHABLE_NAMES)
	{
		int32 SearchableNamesOffset;
		Ar << SearchableNamesOffset;
	}

	// there's a thumbnail table in source packages with following layout
	// * package headers
	// * thumbnail data - sequence of FObjectThumbnail
	// * thumbnail metadata - array of small headers pointed to 'thumbnail data' objects
	//   (this metadata is what FPackageFileSummary::ThumbnailTableOffset points to)
	int32 ThumbnailTableOffset;
	Ar << ThumbnailTableOffset;

	// guid and generations
	Ar << S.Guid;
	int32 Count;
	Ar << Count;
	S.Generations.Empty(Count);
	S.Generations.AddZeroed(Count);
	for (int i = 0; i < Count; i++)
		Ar << S.Generations[i];

	// engine version
	if (Ar.ArVer >= VER_UE4_ENGINE_VERSION_OBJECT)
	{
		FEngineVersion engineVersion; // empty for cooked packages, so don't store it anywhere ...
		Ar << engineVersion;
#if DEBUG_PACKAGE
		appPrintf("version: %d.%d.%d changelist %d (branch %s)\n", engineVersion.Major, engineVersion.Minor, engineVersion.Patch,
			engineVersion.Changelist, *engineVersion.Branch);
#endif
	}
	else
	{
		int32 changelist;
		Ar << changelist;
	}

	if (Ar.ArVer >= VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION)
	{
		FEngineVersion CompatibleVersion;
		Ar << CompatibleVersion;
	}

	// compression structures
	Ar << S.CompressionFlags;
	Ar << S.CompressedChunks;

	int32 PackageSource;
	Ar << PackageSource;

	TArray<FString> AdditionalPackagesToCook;
	Ar << AdditionalPackagesToCook;

	if (S.LegacyVersion > -7)
	{
		int32 NumTextureAllocations;
		Ar << NumTextureAllocations;
	}

	if (Ar.ArVer >= VER_UE4_ASSET_REGISTRY_TAGS)
	{
		int32 AssetRegistryDataOffset;
		Ar << AssetRegistryDataOffset;
	}

	if (Ar.ArVer >= VER_UE4_SUMMARY_HAS_BULKDATA_OFFSET)
	{
		Ar << S.BulkDataStartOffset;
//		appPrintf("Bulk offset: %llX\n", S.BulkDataStartOffset);
	}

	//!! other fields - useless for now

	unguard;
}

#endif // UNREAL4


FArchive& operator<<(FArchive &Ar, FPackageFileSummary &S)
{
	guard(FPackageFileSummary<<);
	assert(Ar.IsLoading);						// saving is not supported

	// read package tag
	Ar << S.Tag;

	// some games has special tag constants
#if SPECIAL_TAGS
	if (S.Tag == 0x9E2A83C2) goto tag_ok;		// Killing Floor
	if (S.Tag == 0x7E4A8BCA) goto tag_ok;		// iStorm
#endif // SPECIAL_TAGS
#if NURIEN
	if (S.Tag == 0xA94E6C81) goto tag_ok;		// Nurien
#endif
#if BATTLE_TERR
	if (S.Tag == 0xA1B2C93F)
	{
		Ar.Game = GAME_BattleTerr;
		goto tag_ok;							// Battle Territory Online
	}
#endif // BATTLE_TERR
#if LOCO
	if (S.Tag == 0xD58C3147)
	{
		Ar.Game = GAME_Loco;
		goto tag_ok;							// Land of Chaos Online
	}
#endif // LOCO
#if BERKANIX
	if (S.Tag == 0xF2BAC156)
	{
		Ar.Game = GAME_Berkanix;
		goto tag_ok;
	}
#endif // BERKANIX
#if HAWKEN
	if (S.Tag == 0xEA31928C)
	{
		Ar.Game = GAME_Hawken;
		goto tag_ok;
	}
#endif // HAWKEN
#if TAO_YUAN
	if (S.Tag == 0x12345678)
	{
		int tmp;			// some additional version?
		Ar << tmp;
		Ar.Game = GAME_TaoYuan;
		if (!GForceGame) GForceGame = GAME_TaoYuan;
		goto tag_ok;
	}
#endif // TAO_YUAN
#if STORMWAR
	if (S.Tag == 0xEC201133)
	{
		byte Count;
		Ar << Count;
		Ar.Seek(Ar.Tell() + Count);
		goto tag_ok;
	}
#endif // STORMWAR
#if GUNLEGEND
	if (S.Tag == 0x879A4B41)
	{
		Ar.Game = GAME_GunLegend;
		if (!GForceGame) GForceGame = GAME_GunLegend;
		goto tag_ok;
	}
#endif // GUNLEGEND
#if MMH7
	if (S.Tag == 0x4D4D4837)
	{
		Ar.Game = GAME_UE3;	// version conflict with Guilty Gear Xrd
		goto tag_ok;		// Might & Magic Heroes 7
	}
#endif // MMH7
#if DEVILS_THIRD
	if (S.Tag == 0x7BC342F0)
	{
		Ar.Game = GAME_DevilsThird;
		if (!GForceGame) GForceGame = GAME_DevilsThird;
		goto tag_ok;		// Devil's Third
	}
#endif // DEVILS_THIRD

	// support reverse byte order
	if (S.Tag != PACKAGE_FILE_TAG)
	{
		if (S.Tag != PACKAGE_FILE_TAG_REV)
			appError("Wrong tag in package: %08X", S.Tag);
		Ar.ReverseBytes = true;
		S.Tag = PACKAGE_FILE_TAG;
	}

	// read version
tag_ok:
	unsigned int Version;
	Ar << Version;

#if UNREAL4
	// UE4 has negative version value, growing from -1 towards negative direction. This value is followed
	// by "UE3 Version", "UE4 Version" and "Licensee Version" (parsed in SerializePackageFileSummary4).
	// The value is used as some version for package header, and it's not changed frequently. We can't
	// expect these values to have large values in the future. The code below checks this value for
	// being less than zero, but allows UE1-UE3 LicenseeVersion up to 32767.
	if ((Version & 0xFFFFF000) == 0xFFFFF000)
	{
		S.LegacyVersion = Version;
		Ar.Game = GAME_UE4_BASE;
		SerializePackageFileSummary4(Ar, S);
		//!! note: UE4 requires different DetectGame way, perhaps it's not possible at all
		//!! (but can use PAK file names for game detection)
		return Ar;
	}
#endif // UNREAL4

#if UNREAL3
	if (Version == PACKAGE_FILE_TAG || Version == 0x20000)
		appError("Fully compressed package header?");
#endif // UNREAL3

	S.FileVersion     = Version & 0xFFFF;
	S.LicenseeVersion = Version >> 16;
	// store file version to archive (required for some structures, for UNREAL3 path)
	Ar.ArVer         = S.FileVersion;
	Ar.ArLicenseeVer = S.LicenseeVersion;
	// detect game
	Ar.DetectGame();
	Ar.OverrideVersion();

	// read other fields

#if UNREAL3
	if (Ar.Game >= GAME_UE3)
		SerializePackageFileSummary3(Ar, S);
	else
#endif
		SerializePackageFileSummary2(Ar, S);

#if DEBUG_PACKAGE
	appPrintf("EngVer:%d CookVer:%d CompF:%d CompCh:%d\n", S.EngineVersion, S.CookerVersion, S.CompressionFlags, S.CompressedChunks.Num());
	appPrintf("Names:%X[%d] Exports:%X[%d] Imports:%X[%d]\n", S.NameOffset, S.NameCount, S.ExportOffset, S.ExportCount, S.ImportOffset, S.ImportCount);
	appPrintf("HeadersSize:%X Group:%s DependsOffset:%X U60:%X\n", S.HeadersSize, *S.PackageGroup, S.DependsOffset, S.U3unk60);
#endif

	return Ar;
	unguardf("Ver=%d/%d", S.FileVersion, S.LicenseeVersion);
}


static void SerializeObjectExport2(FArchive &Ar, FObjectExport &E)
{
	guard(SerializeObjectExport2);

#if PARIAH
	if (Ar.Game == GAME_Pariah)
	{
		Ar << E.ObjectName << AR_INDEX(E.SuperIndex) << E.PackageIndex << E.ObjectFlags;
		Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SerialSize);
		if (E.SerialSize)
			Ar << AR_INDEX(E.SerialOffset);
		return;
	}
#endif // PARIAH
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		int unkC, flags2, unk30;
		Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SuperIndex) << E.PackageIndex;
		if (Ar.ArVer >= 132) Ar << unkC;			// unknown
		Ar << E.ObjectName << E.ObjectFlags;
		if (Ar.ArLicenseeVer >= 40) Ar << flags2;	// qword flags
		Ar << AR_INDEX(E.SerialSize);
		if (E.SerialSize)
			Ar << AR_INDEX(E.SerialOffset);
		if (Ar.ArVer >= 130) Ar << unk30;			// unknown
		return;
	}
#endif // BIOSHOCK
#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 151)
	{
		int unk0C;
		Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SuperIndex) << E.PackageIndex;
		if (Ar.ArVer >= 159) Ar << AR_INDEX(unk0C);
		Ar << E.ObjectName << E.ObjectFlags << E.SerialSize << E.SerialOffset;
		return;
	}
#endif // SWRC
#if AA2
	if (Ar.Game == GAME_AA2)
	{
		int rnd;	// random value
		Ar << AR_INDEX(E.SuperIndex) << rnd << AR_INDEX(E.ClassIndex) << E.PackageIndex;
		Ar << E.ObjectFlags << E.ObjectName << AR_INDEX(E.SerialSize);	// ObjectFlags are serialized in different order, ObjectFlags are negated
		if (E.SerialSize)
			Ar << AR_INDEX(E.SerialOffset);
		E.ObjectFlags = ~E.ObjectFlags;
		return;
	}
#endif // AA2

	// generic UE1/UE2 code
	Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SuperIndex) << E.PackageIndex;
	Ar << E.ObjectName << E.ObjectFlags << AR_INDEX(E.SerialSize);
	if (E.SerialSize)
		Ar << AR_INDEX(E.SerialOffset);

	unguard;
}


#if UC2

static void SerializeObjectExport2X(FArchive &Ar, FObjectExport &E)
{
	guard(SerializeObjectExport2X);

	Ar << E.ClassIndex << E.SuperIndex;
	if (Ar.ArVer >= 150)
	{
		int16 idx = E.PackageIndex;
		Ar << idx;
		E.PackageIndex = idx;
	}
	else
		Ar << E.PackageIndex;
	Ar << E.ObjectName << E.ObjectFlags << E.SerialSize;
	if (E.SerialSize)
		Ar << E.SerialOffset;
	// UC2 has strange thing here: indices are serialized as 4-byte int (instead of AR_INDEX),
	// but stored into 2-byte shorts
	E.ClassIndex = int16(E.ClassIndex);
	E.SuperIndex = int16(E.SuperIndex);

	unguard;
}

#endif // UC2


#if UNREAL3

static void SerializeObjectExport3(FArchive &Ar, FObjectExport &E)
{
	guard(SerializeObjectExport3);

#if USE_COMPACT_PACKAGE_STRUCTS
	#define LOC(name) TMP_##name
	// locally declare FObjectImport data which are stripped
	unsigned	LOC(ObjectFlags2);
	int			LOC(Archetype);
	FGuid		LOC(Guid);
	int			LOC(U3unk6C);
	TStaticArray<int, 16> LOC(NetObjectCount);
#else
	#define LOC(name) E.name
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
		Ar << E.ObjectName << E.PackageIndex << E.ClassIndex << E.SuperIndex << LOC(Archetype);
		Ar << E.ObjectFlags << LOC(ObjectFlags2) << E.SerialSize << E.SerialOffset;
		Ar << tmp2; // zero ?
		Ar << tmp3; // -1 ?
		Ar.Seek(Ar.Tell() + 0x14);	// skip raw version of ComponentMap
		Ar << E.ExportFlags;
		Ar.Seek(Ar.Tell() + 0xC); // skip raw version of NetObjectCount
		Ar << LOC(Guid);
		return;
	}
#endif // WHEELMAN

	Ar << E.ClassIndex << E.SuperIndex << E.PackageIndex << E.ObjectName;
	if (Ar.ArVer >= 220) Ar << LOC(Archetype);

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

	Ar << E.ObjectFlags;
	if (Ar.ArVer >= 195) Ar << LOC(ObjectFlags2);	// qword flags after version 195
	Ar << E.SerialSize;
	if (E.SerialSize || Ar.ArVer >= 249)
		Ar << E.SerialOffset;

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

	if (Ar.ArVer < 543)
	{
	ue3_component_map:
		TStaticMap<FName, int, 16> tmpComponentMap;
		Ar << tmpComponentMap;
	}
ue3_export_flags:
	if (Ar.ArVer >= 247) Ar << E.ExportFlags;

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
		Ar << LOC(Guid);
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

	if (Ar.ArVer >= 322) Ar << LOC(NetObjectCount) << LOC(Guid);

#if UNDERTOW
	if (Ar.Game == GAME_Undertow && Ar.ArVer >= 431) Ar << LOC(U3unk6C);	// partially upgraded?
#endif
#if ARMYOF2
	if (Ar.Game == GAME_ArmyOf2) return;
#endif

	if (Ar.ArVer >= 475) Ar << LOC(U3unk6C);

#if AA3
	if (Ar.Game == GAME_AA3)
	{
		// deobfuscate data
		E.ClassIndex   ^= AA3Obfuscator;
		E.SuperIndex   ^= AA3Obfuscator;
		E.PackageIndex ^= AA3Obfuscator;
		LOC(Archetype) ^= AA3Obfuscator;
		E.SerialSize   ^= AA3Obfuscator;
		E.SerialOffset ^= AA3Obfuscator;
	}
#endif // AA3
#if THIEF4
	if (Ar.Game == GAME_Thief4)
	{
		int unk5C;
		if (E.ExportFlags & 8) Ar << unk5C;
	}
#endif // THIEF4

#undef LOC
	unguard;
}

#endif // UNREAL3


#if UNREAL4

static void SerializeObjectExport4(FArchive &Ar, FObjectExport &E)
{
	guard(SerializeObjectExport4);

#if USE_COMPACT_PACKAGE_STRUCTS
	#define LOC(name) TMP_##name
	// locally declare FObjectImport data which are stripped
	int32		LOC(Archetype);
	TArray<int32> LOC(NetObjectCount);
	FGuid		LOC(Guid);
	int32		LOC(PackageFlags);
	int32		LOC(TemplateIndex);
#else
	#define LOC(name) E.name
#endif // USE_COMPACT_PACKAGE_STRUCTS

	Ar << E.ClassIndex << E.SuperIndex;
	if (Ar.ArVer >= VER_UE4_TemplateIndex_IN_COOKED_EXPORTS) Ar << LOC(TemplateIndex);
	Ar << E.PackageIndex << E.ObjectName;
	if (Ar.ArVer < VER_UE4_REMOVE_ARCHETYPE_INDEX_FROM_LINKER_TABLES) Ar << LOC(Archetype);

	Ar << E.ObjectFlags;

	if (Ar.ArVer < VER_UE4_64BIT_EXPORTMAP_SERIALSIZES)
	{
		Ar << E.SerialSize << E.SerialOffset;
	}
	else
	{
		// 64-bit offsets
		int64 SerialSize64, SerialOffset64;
		Ar << SerialSize64 << SerialOffset64;
		//!! perhaps use uint32 for size/offset, but should review all relative code to not send negative values due to casting of uint32 to int32
		//!! currently limited by 0x80000000
		assert(SerialSize64 < 0x80000000);
		assert(SerialOffset64 < 0x80000000);
		E.SerialSize = (int32)SerialSize64;
		E.SerialOffset = (int32)SerialOffset64;
	}

	bool bForcedExport, bNotForClient, bNotForServer;
	Ar << bForcedExport << bNotForClient << bNotForServer;
	if (Ar.IsLoading)
	{
		E.ExportFlags = bForcedExport ? EF_ForcedExport : 0;	//?? change this
	}

	if (Ar.ArVer < VER_UE4_REMOVE_NET_INDEX) Ar << LOC(NetObjectCount);

	Ar << LOC(Guid) << LOC(PackageFlags);

	bool bNotForEditorGame;
	if (Ar.ArVer >= VER_UE4_LOAD_FOR_EDITOR_GAME)
		Ar << bNotForEditorGame;

	bool bIsAsset;
	if (Ar.ArVer >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
		Ar << bIsAsset;

	if (Ar.ArVer >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
	{
		int32 FirstExportDependency, SerializationBeforeSerializationDependencies, CreateBeforeSerializationDependencies;
		int32 SerializationBeforeCreateDependencies, CreateBeforeCreateDependencies;
		Ar << FirstExportDependency << SerializationBeforeSerializationDependencies << CreateBeforeSerializationDependencies;
		Ar << SerializationBeforeCreateDependencies << CreateBeforeCreateDependencies;
	}

#undef LOC

	unguard;
}

#endif // UNREAL4


FArchive& operator<<(FArchive &Ar, FObjectExport &E)
{
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		SerializeObjectExport4(Ar, E);
		return Ar;
	}
#endif
#if UNREAL3
	if (Ar.Game >= GAME_UE3)
	{
		SerializeObjectExport3(Ar, E);
		return Ar;
	}
#endif
#if UC2
	if (Ar.Engine() == GAME_UE2X)
	{
		SerializeObjectExport2X(Ar, E);
		return Ar;
	}
#endif
	SerializeObjectExport2(Ar, E);
	return Ar;
}


FArchive& operator<<(FArchive &Ar, FObjectImport &I)
{
	guard(SerializeFObjectImport);

#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 150)
	{
		int16 idx = I.PackageIndex;
		Ar << I.ClassPackage << I.ClassName << idx << I.ObjectName;
		I.PackageIndex = idx;
		return Ar;
	}
#endif // UC2
#if PARIAH
	if (Ar.Game == GAME_Pariah)
		return Ar << I.PackageIndex << I.ObjectName << I.ClassPackage << I.ClassName;
#endif
#if AA2
	if (Ar.Game == GAME_AA2)
	{
		byte unk;	// serialized length of ClassName string?
		return Ar << I.ClassPackage << I.ClassName << unk << I.ObjectName << I.PackageIndex;
	}
#endif

	// this code is the same for all engine versions
	Ar << I.ClassPackage << I.ClassName << I.PackageIndex << I.ObjectName;

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		// MK X
		FGuid unk;
		Ar << unk;
	}
#endif // MKVSDC

	return Ar;

	unguard;
}


/*-----------------------------------------------------------------------------
	Lineage2 file reader
-----------------------------------------------------------------------------*/

#if LINEAGE2 || EXTEEL

#define LINEAGE_HEADER_SIZE		28

class FFileReaderLineage : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderLineage, FReaderWrapper);
public:
	FFileReaderLineage(FArchive *File, int Key)
	:	FReaderWrapper(File, LINEAGE_HEADER_SIZE)
	,	XorKey(Key)
	{
		Game = GAME_Lineage2;
		Seek(0);		// skip header
	}

	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);
		if (XorKey)
		{
			int i;
			byte *p;
			for (i = 0, p = (byte*)data; i < size; i++, p++)
				*p ^= XorKey;
		}
	}

protected:
	byte		XorKey;
};

#endif // LINEAGE2 || EXTEEL


/*-----------------------------------------------------------------------------
	Battle Territory Online
-----------------------------------------------------------------------------*/

#if BATTLE_TERR

class FFileReaderBattleTerr : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderBattleTerr, FReaderWrapper);
public:
	FFileReaderBattleTerr(FArchive *File)
	:	FReaderWrapper(File)
	{
		Game = GAME_BattleTerr;
	}

	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);

		int i;
		byte *p;
		for (i = 0, p = (byte*)data; i < size; i++, p++)
		{
			byte b = *p;
			int shift;
			byte v;
			for (shift = 1, v = b & (b - 1); v; v = v & (v - 1))	// shift = number of identity bits in 'v' (but b=0 -> shift=1)
				shift++;
			b = ROL8(b, shift);
			*p = b;
		}
	}
};

#endif // BATTLE_TERR


/*-----------------------------------------------------------------------------
	Blade & Soul
-----------------------------------------------------------------------------*/

#if AA2

class FFileReaderAA2 : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderAA2, FReaderWrapper);
public:
	FFileReaderAA2(FArchive *File)
	:	FReaderWrapper(File)
	{}

	virtual void Serialize(void *data, int size)
	{
		int StartPos = Reader->Tell();
		Reader->Serialize(data, size);

		int i;
		byte *p;
		for (i = 0, p = (byte*)data; i < size; i++, p++)
		{
			byte b = *p;
		#if 0
			// used with ArraysAGPCount != 0
			int shift;
			byte v;
			for (shift = 1, v = b & (b - 1); v; v = v & (v - 1))	// shift = number of identity bits in 'v' (but b=0 -> shift=1)
				shift++;
			b = ROR8(b, shift);
		#else
			// used with ArraysAGPCount == 0
			int PosXor = StartPos + i;
			PosXor = (PosXor >> 8) ^ PosXor;
			b ^= (PosXor & 0xFF);
			if (PosXor & 2)
			{
				b = ROL8(b, 1);
			}
		#endif
			*p = b;
		}
	}
};

#endif // AA2


/*-----------------------------------------------------------------------------
	Blade & Soul
-----------------------------------------------------------------------------*/

#if BLADENSOUL

class FFileReaderBnS : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderBnS, FReaderWrapper);
public:
	FFileReaderBnS(FArchive *File)
	:	FReaderWrapper(File)
	{
		Game = GAME_BladeNSoul;
	}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		// Note: similar code exists in DecryptBladeAndSoul()
		int i;
		byte *p;
		static const char *key = "qiffjdlerdoqymvketdcl0er2subioxq";
		for (i = 0, p = (byte*)data; i < size; i++, p++, Pos++)
		{
			*p ^= key[Pos % 32];
		}
	}
};


static void DecodeBnSPointer(int &Value, unsigned Code1, unsigned Code2, int Index)
{
	unsigned tmp1 = ROR32(Value, (Index + Code2) & 0x1F);
	unsigned tmp2 = ROR32(Code1, Index % 32);
	Value = tmp2 ^ tmp1;
}


static void PatchBnSExports(FObjectExport *Exp, const FPackageFileSummary &Summary)
{
	unsigned Code1 = ((Summary.HeadersSize & 0xFF) << 24) |
					 ((Summary.NameCount   & 0xFF) << 16) |
					 ((Summary.NameOffset  & 0xFF) << 8)  |
					 ((Summary.ExportCount & 0xFF));
	unsigned Code2 = (Summary.ExportOffset + Summary.ImportCount + Summary.ImportOffset) & 0x1F;

	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		DecodeBnSPointer(Exp->SerialSize,   Code1, Code2, i);
		DecodeBnSPointer(Exp->SerialOffset, Code1, Code2, i);
	}
}

#endif // BLADENSOUL

#if DUNDEF

static void PatchDunDefExports(FObjectExport *Exp, const FPackageFileSummary &Summary)
{
	// Dungeon Defenders has nullified ExportOffset entries starting from some version.
	// Let's recover them.
	int CurrentOffset = Summary.HeadersSize;
	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		if (Exp->SerialOffset == 0)
			Exp->SerialOffset = CurrentOffset;
		CurrentOffset = Exp->SerialOffset + Exp->SerialSize;
	}
}

#endif // DUNDEF


/*-----------------------------------------------------------------------------
	Nurien
-----------------------------------------------------------------------------*/

#if NURIEN

class FFileReaderNurien : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderNurien, FReaderWrapper);
public:
	int			Threshold;

	FFileReaderNurien(FArchive *File)
	:	FReaderWrapper(File)
	,	Threshold(0x7FFFFFFF)
	{}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		// only first Threshold bytes are compressed (package headers)
		if (Pos >= Threshold) return;

		int i;
		byte *p;
		static const byte key[] = {
			0xFE, 0xF2, 0x35, 0x2E, 0x12, 0xFF, 0x47, 0x8A,
			0xE1, 0x2D, 0x53, 0xE2, 0x21, 0xA3, 0x74, 0xA8
		};
		for (i = 0, p = (byte*)data; i < size; i++, p++, Pos++)
		{
			if (Pos >= Threshold) return;
			*p ^= key[Pos & 0xF];
		}
	}

	virtual void SetStartingPosition(int pos)
	{
		Threshold = pos;
	}
};

#endif // NURIEN


/*-----------------------------------------------------------------------------
	UE3 compressed file reader
-----------------------------------------------------------------------------*/

#if UNREAL3

class FUE3ArchiveReader : public FArchive
{
	DECLARE_ARCHIVE(FUE3ArchiveReader, FArchive);
public:
	FArchive				*Reader;

	bool					IsFullyCompressed;

	// compression data
	int						CompressionFlags;
	TArray<FCompressedChunk> CompressedChunks;
	// own file positions, overriding FArchive's one (because parent class is
	// used for compressed data)
	int						Stopper;
	int						Position;
	// decompression buffer
	byte					*Buffer;
	int						BufferSize;
	int						BufferStart;
	int						BufferEnd;
	// chunk
	const FCompressedChunk	*CurrentChunk;
	FCompressedChunkHeader	ChunkHeader;
	int						ChunkDataPos;

	int						PositionOffset;

	FUE3ArchiveReader(FArchive *File, int Flags, const TArray<FCompressedChunk> &Chunks)
	:	Reader(File)
	,	IsFullyCompressed(false)
	,	CompressionFlags(Flags)
	,	Buffer(NULL)
	,	BufferSize(0)
	,	BufferStart(0)
	,	BufferEnd(0)
	,	CurrentChunk(NULL)
	,	PositionOffset(0)
	{
		guard(FUE3ArchiveReader::FUE3ArchiveReader);
		CopyArray(CompressedChunks, Chunks);
		SetupFrom(*File);
		assert(CompressionFlags);
		assert(CompressedChunks.Num());
		unguard;
	}

	virtual ~FUE3ArchiveReader()
	{
		if (Buffer) delete[] Buffer;
		if (Reader) delete Reader;
	}

	virtual bool IsCompressed() const
	{
		return true;
	}

	virtual void Serialize(void *data, int size)
	{
		guard(FUE3ArchiveReader::Serialize);

		if (Stopper > 0 && Position + size > Stopper)
			appError("Serializing behind stopper (%X+%X > %X)", Position, size, Stopper);

		while (true)
		{
			// check for valid buffer
			if (Position >= BufferStart && Position < BufferEnd)
			{
				int ToCopy = BufferEnd - Position;						// available size
				if (ToCopy > size) ToCopy = size;						// shrink by required size
				memcpy(data, Buffer + Position - BufferStart, ToCopy);	// copy data
				// advance pointers/counters
				Position += ToCopy;
				size     -= ToCopy;
				data     = OffsetPointer(data, ToCopy);
				if (!size) return;										// copied enough
			}
			// here: data/size points outside of loaded Buffer
			PrepareBuffer(Position);
			assert(Position >= BufferStart && Position < BufferEnd);	// validate PrepareBuffer()
		}

		unguard;
	}

	void PrepareBuffer(int Pos)
	{
		guard(FUE3ArchiveReader::PrepareBuffer);
		// find compressed chunk
		const FCompressedChunk *Chunk = NULL;
		for (int ChunkIndex = 0; ChunkIndex < CompressedChunks.Num(); ChunkIndex++)
		{
			Chunk = &CompressedChunks[ChunkIndex];
			if (Pos < Chunk->UncompressedOffset + Chunk->UncompressedSize)
				break;
		}
		assert(Chunk); // should be at least 1 chunk in CompressedChunks

		// DC Universe has uncompressed package headers but compressed remaining package part
		if (Pos < Chunk->UncompressedOffset)
		{
			if (Buffer) delete[] Buffer;
			int Size = Chunk->CompressedOffset;
			Buffer      = new byte[Size];
			BufferSize  = Size;
			BufferStart = 0;
			BufferEnd   = Size;
			Reader->Seek(0);
			Reader->Serialize(Buffer, Size);
			return;
		}

		if (Chunk != CurrentChunk)
		{
			// serialize compressed chunk header
			Reader->Seek(Chunk->CompressedOffset);
#if BIOSHOCK
			if (Game == GAME_Bioshock)
			{
				// read block size
				int CompressedSize;
				*Reader << CompressedSize;
				// generate ChunkHeader
				ChunkHeader.Blocks.Empty(1);
				FCompressedChunkBlock *Block = new (ChunkHeader.Blocks) FCompressedChunkBlock;
				Block->UncompressedSize = 32768;
				if (ArLicenseeVer >= 57)		//?? Bioshock 2; no version code found
					*Reader << Block->UncompressedSize;
				Block->CompressedSize = CompressedSize;
			}
			else
#endif // BIOSHOCK
			{
				if (Chunk->CompressedSize != Chunk->UncompressedSize)
					*Reader << ChunkHeader;
				else
				{
					// have seen such block in Borderlands: chunk has CompressedSize==UncompressedSize
					// and has no compression; no such code in original engine
					ChunkHeader.BlockSize = -1;	// mark as uncompressed (checked below)
					ChunkHeader.Sum.CompressedSize = ChunkHeader.Sum.UncompressedSize = Chunk->UncompressedSize;
					ChunkHeader.Blocks.Empty(1);
					FCompressedChunkBlock *Block = new (ChunkHeader.Blocks) FCompressedChunkBlock;
					Block->UncompressedSize = Block->CompressedSize = Chunk->UncompressedSize;
				}
			}
			ChunkDataPos = Reader->Tell();
			CurrentChunk = Chunk;
		}
		// find block in ChunkHeader.Blocks
		int ChunkPosition = Chunk->UncompressedOffset;
		int ChunkData     = ChunkDataPos;
		assert(ChunkPosition <= Pos);
		const FCompressedChunkBlock *Block = NULL;
		for (int BlockIndex = 0; BlockIndex < ChunkHeader.Blocks.Num(); BlockIndex++)
		{
			Block = &ChunkHeader.Blocks[BlockIndex];
			if (ChunkPosition + Block->UncompressedSize > Pos)
				break;
			ChunkPosition += Block->UncompressedSize;
			ChunkData     += Block->CompressedSize;
		}
		assert(Block);
		// read compressed data
		//?? optimize? can share compressed buffer and decompressed buffer between packages
		byte *CompressedBlock = new byte[Block->CompressedSize];
		Reader->Seek(ChunkData);
		Reader->Serialize(CompressedBlock, Block->CompressedSize);
		// prepare buffer for decompression
		if (Block->UncompressedSize > BufferSize)
		{
			if (Buffer) delete[] Buffer;
			Buffer = new byte[Block->UncompressedSize];
			BufferSize = Block->UncompressedSize;
		}
		// decompress data
		guard(DecompressBlock);
		if (ChunkHeader.BlockSize != -1)	// my own mark
			appDecompress(CompressedBlock, Block->CompressedSize, Buffer, Block->UncompressedSize, CompressionFlags);
		else
		{
			// no compression
			assert(Block->CompressedSize == Block->UncompressedSize);
			memcpy(Buffer, CompressedBlock, Block->CompressedSize);
		}
		unguardf("block=%X+%X", ChunkData, Block->CompressedSize);
		// setup BufferStart/BufferEnd
		BufferStart = ChunkPosition;
		BufferEnd   = ChunkPosition + Block->UncompressedSize;
		// cleanup
		delete[] CompressedBlock;
		unguard;
	}

	// position controller
	virtual void Seek(int Pos)
	{
		Position = Pos - PositionOffset;
	}
	virtual int Tell() const
	{
		return Position + PositionOffset;
	}
	virtual int GetFileSize() const
	{
		// this function is meaningful for the decompressor tool
		guard(FUE3ArchiveReader::GetFileSize);
		const FCompressedChunk &Chunk = CompressedChunks[CompressedChunks.Num() - 1];
#if BIOSHOCK
		if (Game == GAME_Bioshock && ArLicenseeVer >= 57)		//?? Bioshock 2; no version code found
		{
			// Bioshock 2 has added "UncompressedSize" for block, so we must read it
			int OldPos = Reader->Tell();
			int CompressedSize, UncompressedSize;
			Reader->Seek(Chunk.CompressedOffset);
			*Reader << CompressedSize << UncompressedSize;
			Reader->Seek(OldPos);	// go back
			return Chunk.UncompressedOffset + UncompressedSize;
		}
#endif // BIOSHOCK
		return Chunk.UncompressedOffset + Chunk.UncompressedSize + PositionOffset;
		unguard;
	}
	virtual void SetStopper(int Pos)
	{
		Stopper = Pos;
	}
	virtual int GetStopper() const
	{
		return Stopper;
	}
	virtual bool IsOpen() const
	{
		return Reader->IsOpen();
	}
	virtual bool Open()
	{
		return Reader->Open();
	}

	virtual void Close()
	{
		Reader->Close();
		if (Buffer)
		{
			delete[] Buffer;
			Buffer = NULL;
			BufferStart = BufferEnd = BufferSize = 0;
		}
		CurrentChunk = NULL;
	}

	void ReplaceLoaderWithOffset(FArchive* file, int offset)
	{
		if (Reader) delete Reader;
		Reader = file;
		PositionOffset = offset;
	}
};

#endif // UNREAL3

/*-----------------------------------------------------------------------------
	Package loading (creation) / unloading
-----------------------------------------------------------------------------*/

//!! Move CreateLoader and all loaders to a separate h/cpp.
//!! Note: it's not enough just to expose CreateLoader to .h file - some loaders
//!! are accrssed from UnPackage constructor (see all CastTo functions).
//!! Also, 2 loaders are created directly from UnPackage constructor, after
//!! FPackageFileSummary surialization - perhaps it's possible to move them
//!! into CreateLoader too.

FArchive* UnPackage::CreateLoader(const char* filename, FArchive* baseLoader)
{
	guard(UnPackage::CreateLoader);

	// setup FArchive
	FArchive* Loader = (baseLoader) ? baseLoader : new FFileReader(filename);

#if LINEAGE2 || EXTEEL || BATTLE_TERR || NURIEN || BLADENSOUL
	int checkDword;
	*Loader << checkDword;

	#if LINEAGE2 || EXTEEL
	if (checkDword == ('L' | ('i' << 16)))	// unicode string "Lineage2Ver111"
	{
		// this is a Lineage2 package
		Loader->Seek(LINEAGE_HEADER_SIZE);
		// here is a encrypted by 'xor' standard FPackageFileSummary
		// to get encryption key, can check 1st byte
		byte b;
		*Loader << b;
		// for Ver111 XorKey==0xAC for Lineage or ==0x42 for Exteel, for Ver121 computed from filename
		byte XorKey = b ^ (PACKAGE_FILE_TAG & 0xFF);
		// replace Loader
		Loader = new FFileReaderLineage(Loader, XorKey);
	}
	else
	#endif // LINEAGE2 || EXTEEL
	#if BATTLE_TERR
	if (checkDword == 0x342B9CFC)
	{
		// replace Loader
		Loader = new FFileReaderBattleTerr(Loader);
	}
	#endif // BATTLE_TERR
	#if NURIEN
	if (checkDword == 0xB01F713F)
	{
		// replace loader
		Loader = new FFileReaderNurien(Loader);
	}
	#endif // NURIEN
	#if BLADENSOUL
	if (checkDword == 0xF84CEAB0)
	{
		if (!GForceGame) GForceGame = GAME_BladeNSoul;
		Loader = new FFileReaderBnS(Loader);
	}
	#endif // BLADENSOUL
	Loader->Seek(0);	// seek back to header
#endif // complex

#if UNREAL3
	// code for fully compressed packages support
	//!! rewrite this code, merge with game autodetection
	int checkDword1, checkDword2;
	*Loader << checkDword1;
	if (checkDword1 == PACKAGE_FILE_TAG_REV)
	{
		Loader->ReverseBytes = true;
		if (GForcePlatform == PLATFORM_UNKNOWN)
			Loader->Platform = PLATFORM_XBOX360;			// default platform for "ReverseBytes" mode is PLATFORM_XBOX360
	}
	*Loader << checkDword2;
	Loader->Seek(0);
	if (checkDword2 == PACKAGE_FILE_TAG || checkDword2 == 0x20000 || checkDword2 == 0x10000)	// seen 0x10000 in Enslaved PS3
	{
		//!! NOTES:
		//!! 1)	GOW1/X360 byte-order logic is failed with Core.u and Ungine.u: package header is little-endian,
		//!!	but internal structures (should be) big endian; crashed on decompression: should use LZX, but
		//!!	used ZLIB
		//!! 2)	MKvsDC/X360 Core.u and Engine.u uses LZO instead of LZX
		guard(ReadFullyCompressedHeader);
		// this is a fully compressed package
		FCompressedChunkHeader H;
		*Loader << H;
		TArray<FCompressedChunk> Chunks;
		FCompressedChunk *Chunk = new (Chunks) FCompressedChunk;
		Chunk->UncompressedOffset = 0;
		Chunk->UncompressedSize   = H.Sum.UncompressedSize;
		Chunk->CompressedOffset   = 0;
		Chunk->CompressedSize     = H.Sum.CompressedSize;
		byte CompMethod = GForceCompMethod;
		if (!CompMethod)
			CompMethod = (Loader->Platform == PLATFORM_XBOX360) ? COMPRESS_LZX : COMPRESS_FIND;
		FUE3ArchiveReader* UE3Loader = new FUE3ArchiveReader(Loader, CompMethod, Chunks);
		UE3Loader->IsFullyCompressed = true;
		Loader = UE3Loader;
		unguard;
	}
#endif // UNREAL3

	return Loader;

	unguardf("%s", filename);
}

UnPackage::UnPackage(const char *filename, FArchive *baseLoader, bool silent)
:	Loader(NULL)
{
	guard(UnPackage::UnPackage);

#if PROFILE_PACKAGE_TABLES
	appResetProfiler();
#endif

	IsLoading = true;
	Filename = appStrdupPool(appSkipRootDir(filename));
	Loader = CreateLoader(filename, baseLoader);
	SetupFrom(*Loader);

	// read summary
	*this << Summary;
	Loader->SetupFrom(*this);	// serialization of FPackageFileSummary could change some FArchive properties

#if !DEBUG_PACKAGE
	if (!silent)
#endif
	{
		PKG_LOG("Loading package: %s Ver: %d/%d ", Filename, Loader->ArVer, Loader->ArLicenseeVer);
			// don't use 'Summary.FileVersion, Summary.LicenseeVersion' because UE4 has overrides for unversioned packages
#if UNREAL3
		if (Game >= GAME_UE3)
		{
			PKG_LOG("Engine: %d ", Summary.EngineVersion);
			FUE3ArchiveReader* UE3Loader = Loader->CastTo<FUE3ArchiveReader>();
			if (UE3Loader && UE3Loader->IsFullyCompressed)
				PKG_LOG("[FullComp] ");
		}
#endif // UNREAL3
#if UNREAL4
		if (Game >= GAME_UE4_BASE && Summary.IsUnversioned)
			PKG_LOG("[Unversioned] ");
#endif // UNREAL4
		PKG_LOG("Names: %d Exports: %d Imports: %d Game: %X\n", Summary.NameCount, Summary.ExportCount, Summary.ImportCount, Game);
	}

#if DEBUG_PACKAGE
	appPrintf("Flags: %X, Name offset: %X, Export offset: %X, Import offset: %X\n", Summary.PackageFlags, Summary.NameOffset, Summary.ExportOffset, Summary.ImportOffset);
	for (int i = 0; i < Summary.CompressedChunks.Num(); i++)
	{
		const FCompressedChunk &ch = Summary.CompressedChunks[i];
		appPrintf("chunk[%d]: comp=%X+%X, uncomp=%X+%X\n", i, ch.CompressedOffset, ch.CompressedSize, ch.UncompressedOffset, ch.UncompressedSize);
	}
#endif // DEBUG_PACKAGE

	//!! try to separate the following code too
#if BIOSHOCK
	if ((Game == GAME_Bioshock) && (Summary.PackageFlags & 0x20000))
	{
		// Bioshock has a special flag indicating compression. Compression table follows the package summary.
		// Read compression tables.
		int NumChunks, i;
		TArray<FCompressedChunk> Chunks;
		*this << NumChunks;
		Chunks.Empty(NumChunks);
		int UncompOffset = Tell() - 4;				//?? there should be a flag signalling presence of compression structures, because of "Tell()-4"
		for (i = 0; i < NumChunks; i++)
		{
			int Offset;
			*this << Offset;
			FCompressedChunk *Chunk = new (Chunks) FCompressedChunk;
			Chunk->UncompressedOffset = UncompOffset;
			Chunk->UncompressedSize   = 32768;
			Chunk->CompressedOffset   = Offset;
			Chunk->CompressedSize     = 0;			//?? not used
			UncompOffset             += 32768;
		}
		// Replace Loader for reading compressed Bioshock archives.
		Loader = new FUE3ArchiveReader(Loader, COMPRESS_ZLIB, Chunks);
		Loader->SetupFrom(*this);
	}
#endif // BIOSHOCK

#if AA2
	if (Game == GAME_AA2)
	{
		// America's Army 2 has encryption after FPackageFileSummary
		if (ArLicenseeVer >= 19)
		{
			int IsEncrypted;
			*this << IsEncrypted;
			if (IsEncrypted) Loader = new FFileReaderAA2(Loader);
		}
	}
#endif // AA2
	//!! end of loader substitution


#if UNREAL3
	if (Game >= GAME_UE3 && Summary.CompressionFlags && Summary.CompressedChunks.Num())
	{
		FUE3ArchiveReader* UE3Loader = Loader->CastTo<FUE3ArchiveReader>();
		if (UE3Loader && UE3Loader->IsFullyCompressed)
			appError("Fully compressed package %s has additional compression table", filename);
		// replace Loader with special reader for compressed UE3 archives
		Loader = new FUE3ArchiveReader(Loader, Summary.CompressionFlags, Summary.CompressedChunks);
	}
	#if NURIEN
	FFileReaderNurien* NurienReader = Loader->CastTo<FFileReaderNurien>();
	if (NurienReader)
		NurienReader->Threshold = Summary.HeadersSize;
	#endif // NURIEN
#endif // UNREAL3

	LoadNameTable();
	LoadImportTable();
	LoadExportTable();

#if UNREAL3 && !USE_COMPACT_PACKAGE_STRUCTS			// we can serialize dependencies when needed
	if (Game == GAME_DCUniverse || Game == GAME_Bioshock3) goto no_depends;		// has non-standard checks
	if (Summary.DependsOffset)						// some games are patrially upgraded: ArVer >= 415, but no depends table
	{
		guard(ReadDependsTable);
		Seek(Summary.DependsOffset);
		FObjectDepends *Dep = DependsTable = new FObjectDepends[Summary.ExportCount];
		for (int i = 0; i < Summary.ExportCount; i++, Dep++)
		{
			*this << *Dep;
/*			if (Dep->Objects.Num())
			{
				const FObjectExport &Exp = ExportTable[i];
				appPrintf("Depends for %s'%s' = %d\n", GetObjectName(Exp.ClassIndex),
					*Exp.ObjectName, Dep->Objects[i]);
			} */
		}
		unguard;
	}
no_depends: ;
#endif // UNREAL3 && !USE_COMPACT_PACKAGE_STRUCTS

#if UNREAL4
	// Process Event Driven Loader packages: such packages are split into 2 pieces: .uasser with headers
	// and .uexp with object's data.
	if (Game >= GAME_UE4_BASE && Summary.HeadersSize == Loader->GetFileSize())
	{
		char buf[MAX_PACKAGE_PATH];
		appStrncpyz(buf, filename, ARRAY_COUNT(buf));
		char* s = strrchr(buf, '.');
		if (!s)
		{
			s = strchr(buf, 0);
		}
		strcpy(s, ".uexp");
		const CGameFileInfo *expInfo = appFindGameFile(buf);
		if (expInfo)
		{
			// Open .exp file
			FArchive* expLoader = appCreateFileReader(expInfo);
			// Replace loader with this file, but add offset so it will work like it is part of original uasset
			delete Loader;
			Loader = new FReaderWrapper(expLoader, -Summary.HeadersSize);
		}
		else
		{
			appPrintf("WARNING: it seems package %s has missing .uexp file\n", filename);
		}
	}
#endif // UNREAL4

	// add self to package map
	char buf[MAX_PACKAGE_PATH];
	const char *s = strrchr(filename, '/');
	if (!s) s = strrchr(filename, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = filename;
	appStrncpyz(buf, s, ARRAY_COUNT(buf));
	char *s2 = strchr(buf, '.');
	if (s2) *s2 = 0;
	Name = appStrdupPool(buf);
	PackageMap.Add(this);

	// Release package file handle
	CloseReader();

#if PROFILE_PACKAGE_TABLES
	appPrintProfiler();
#endif

	unguardf("%s, ver=%d/%d, game=%s", filename, ArVer, ArLicenseeVer, GetGameTag(Game));
}


void UnPackage::LoadNameTable()
{
	guard(UnPackage::LoadNameTable);

	if (Summary.NameCount == 0) return;

	Seek(Summary.NameOffset);
	NameTable = new const char* [Summary.NameCount];
	for (int i = 0; i < Summary.NameCount; i++)
	{
		guard(Name);
		if ((ArVer < 64) && (Game < GAME_UE4_BASE)) // UE4 has restarted versioning from 0
		{
			char buf[MAX_FNAME_LEN];
			int len;
			for (len = 0; len < ARRAY_COUNT(buf); len++)
			{
				char c;
				*this << c;
				buf[len] = c;
				if (!c) break;
			}
			assert(len < ARRAY_COUNT(buf));
			NameTable[i] = appStrdupPool(buf);
			// skip object flags
			int tmp;
			*this << tmp;
		}
#if UC1 || PARIAH
		else if (Game == GAME_UC1 && ArLicenseeVer >= 28)
		{
		uc1_name:
			// used uint16 + char[] instead of FString
			char buf[MAX_FNAME_LEN];
			uint16 len;
			*this << len;
			assert(len < ARRAY_COUNT(buf));
			Serialize(buf, len+1);
			NameTable[i] = appStrdupPool(buf);
			// skip object flags
			int tmp;
			*this << tmp;
		}
	#if PARIAH
		else if (Game == GAME_Pariah && ((ArLicenseeVer & 0x3F) >= 28)) goto uc1_name;
	#endif
#endif // UC1 || PARIAH
		else
		{
			FStaticString<MAX_FNAME_LEN> name;

#if SPLINTER_CELL
			if (Game == GAME_SplinterCell && ArLicenseeVer >= 85)
			{
				char buf[MAX_FNAME_LEN];
				byte len;
				int flags;
				*this << len;
				assert(len < ARRAY_COUNT(buf));
				Serialize(buf, len+1);
				NameTable[i] = appStrdupPool(buf);
				*this << flags;
				goto done;
			}
#endif // SPLINTER_CELL
#if LEAD
			if (Game == GAME_SplinterCellConv && ArVer >= 68)
			{
				char buf[MAX_FNAME_LEN];
				int len;
				*this << AR_INDEX(len);
				assert(len < ARRAY_COUNT(buf));
				Serialize(buf, len);
				buf[len] = 0;
				NameTable[i] = appStrdupPool(buf);
				goto done;
			}
#endif // LEAD
#if AA2
			if (Game == GAME_AA2)
			{
				guard(AA2_FName);
				char buf[MAX_FNAME_LEN];
				int len;
				*this << AR_INDEX(len);
				// read as unicode string and decrypt
				assert(len <= 0);
				len = -len;
				assert(len < ARRAY_COUNT(buf));
				char* d = buf;
				byte shift = 5;
				for (int j = 0; j < len; j++, d++)
				{
					uint16 c;
					*this << c;
					uint16 c2 = ROR16(c, shift);
					assert(c2 < 256);
					*d = c2 & 0xFF;
					shift = (c - 5) & 15;
				}
				NameTable[i] = appStrdupPool(buf);
				int unk;
				*this << AR_INDEX(unk);
				unguard;
				goto dword_flags;
			}
#endif // AA2
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
				char buf[MAX_FNAME_LEN];
				byte len;
				*this << len;
				assert(len < ARRAY_COUNT(buf));
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

			// Korean games sometimes uses Unicode strings ...
			*this << name;
	#if AVA
			if (Game == GAME_AVA)
			{
				// strange code - package contains some bytes:
				// V(0) = len ^ 0x3E
				// V(i) = V(i-1) + 0x48 ^ 0xE1
				// Number of bytes = (len ^ 7) & 0xF
				int skip = name.Len();
				skip = (skip ^ 7) & 0xF;
				Seek(Tell() + skip);
			}
	#endif // AVA

			// Verify name, some Korean games (B&S) has garbage there.
			// Use separate block to not mess with 'goto crossing variable initialization' error.
			{
				bool goodName = true;
				int numBadChars = 0;
				for (int j = 0; j < name.Len(); j++)
				{
					char c = name[j];
					if (c < ' ' || c > 0x7F)
					{
						// unreadable character
						goodName = false;
						break;
					}
					if (c == '$') numBadChars++;		// unicode characters replaced with '$' in FString serializer
				}
				if (numBadChars && name.Len() >= 64) goodName = false;
				if (numBadChars >= name.Len() / 2 && name.Len() > 16) goodName = false;
				if (!goodName)
				{
					// replace name
					appPrintf("WARNING: %s: fixing name %d\n", Filename, i);
					char buf[64];
					appSprintf(ARRAY_ARG(buf), "__name_%d__", i);
					name = buf;
				}
			}

			// remember the name
	#if 0
			NameTable[i] = new char[name.Num()];
			strcpy(NameTable[i], *name);
	#else
			NameTable[i] = appStrdupPool(*name);
	#endif

	#if UNREAL4
			if (Game >= GAME_UE4_BASE)
			{
		#if GEARS4
				if (Game == GAME_Gears4) goto name_hashes;
		#endif
				if (ArVer >= VER_UE4_NAME_HASHES_SERIALIZED)
				{
				name_hashes:
					int16 NonCasePreservingHash, CasePreservingHash;
					*this << NonCasePreservingHash << CasePreservingHash;
				}
				// skip object flags
				goto done;
			}
	#endif
	#if UNREAL3
		#if BIOSHOCK
			if (Game == GAME_Bioshock) goto qword_flags;
		#endif
		#if WHEELMAN
			if (Game == GAME_Wheelman) goto dword_flags;
		#endif
		#if MASSEFF
			if (Game >= GAME_MassEffect && Game <= GAME_MassEffect3)
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
					TrashLen = name.Len() ^ 7;
				}
				else
				{
					TrashLen = name.Len() ^ 6;
				}
				this->Seek(this->Tell() + (TrashLen & 0xF));
			}
		#endif // METRO_CONF
			if (Game >= GAME_UE3 && ArVer >= 195)
			{
			qword_flags:
				// object flags are 64-bit in UE3, skip additional 32 bits
				int64 flags64;
				*this << flags64;
				goto done;
			}
	#endif // UNREAL3

		dword_flags:
			int flags32;
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


void UnPackage::LoadImportTable()
{
	guard(UnPackage::LoadImportTable);

	if (Summary.ImportCount == 0) return;

	Seek(Summary.ImportOffset);
	FObjectImport *Imp = ImportTable = new FObjectImport[Summary.ImportCount];
	for (int i = 0; i < Summary.ImportCount; i++, Imp++)
	{
		*this << *Imp;
#if DEBUG_PACKAGE
		PKG_LOG("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName);
#endif
	}

	unguard;
}


void UnPackage::LoadExportTable()
{
	// load exports table
	guard(UnPackage::LoadExportTable);

	if (Summary.ExportCount == 0) return;

	Seek(Summary.ExportOffset);
	FObjectExport *Exp = ExportTable = new FObjectExport[Summary.ExportCount];
	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		*this << *Exp;
#if DEBUG_PACKAGE
//		USE_COMPACT_PACKAGE_STRUCTS - makes impossible to dump full information
//		Perhaps add full support to extract.exe?
//		PKG_LOG("Export[%d]: %s'%s' offs=%08X size=%08X parent=%d flags=%08X:%08X, exp_f=%08X arch=%d\n", i, GetObjectName(Exp->ClassIndex),
//			*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->PackageIndex, Exp->ObjectFlags2, Exp->ObjectFlags, Exp->ExportFlags, Exp->Archetype);
		PKG_LOG("Export[%d]: %s'%s' offs=%08X size=%08X parent=%d flags=%08X, exp_f=%08X\n", i, GetObjectName(Exp->ClassIndex),
			*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->PackageIndex, Exp->ObjectFlags, Exp->ExportFlags);
#endif
	}

#if BLADENSOUL
	if (Game == GAME_BladeNSoul && (Summary.PackageFlags & 0x08000000))
		PatchBnSExports(ExportTable, Summary);
#endif
#if DUNDEF
	if (Game == GAME_DunDef)
		PatchDunDefExports(ExportTable, Summary);
#endif

	unguard;
}


UnPackage::~UnPackage()
{
	guard(UnPackage::~UnPackage);
	// free resources
	if (Loader) delete Loader;
	delete NameTable;
	delete ImportTable;
	delete ExportTable;
#if UNREAL3
	if (DependsTable) delete DependsTable;
#endif
	// remove self from package table
	int i = PackageMap.FindItem(this);
	assert(i != INDEX_NONE);
	PackageMap.RemoveAt(i);
	unguard;
}

#if 0
// Commented, not used
// Find file archive inside a package loader
static FFileArchive* FindFileArchive(FArchive* Ar)
{
#if UNREAL3
	FUE3ArchiveReader* ArUE3 = Ar->CastTo<FUE3ArchiveReader>();
	if (ArUE3) Ar = ArUE3->Reader;
#endif

	FReaderWrapper* ArWrap = Ar->CastTo<FReaderWrapper>();
	if (ArWrap) Ar = ArWrap->Reader;

	return Ar->CastTo<FFileArchive>();
}
#endif

void UnPackage::SetupReader(int ExportIndex)
{
	guard(UnPackage::SetupReader);
	// open loader if it is closed
#if 0
	FFileArchive* File = FindFileArchive(Loader);
	assert(File);
	if (!File->IsOpen()) File->Open();
#else
	if (!Loader->IsOpen()) Loader->Open();
#endif
	// setup for object
	const FObjectExport &Exp = GetExport(ExportIndex);
	SetStopper(Exp.SerialOffset + Exp.SerialSize);
	Seek(Exp.SerialOffset);
	unguard;
}

void UnPackage::CloseReader()
{
#if 0
	FFileArchive* File = FindFileArchive(Loader);
	assert(File);
	if (File->IsOpen()) File->Close();
#else
	Loader->Close();
#endif
}

void UnPackage::CloseAllReaders()
{
	for (int i = 0; i < PackageMap.Num(); i++)
	{
		UnPackage* p = PackageMap[i];
		p->CloseReader();
	}
}


/*-----------------------------------------------------------------------------
	UObject* and FName serializers
-----------------------------------------------------------------------------*/

FArchive& UnPackage::operator<<(FName &N)
{
	guard(UnPackage::SerializeFName);

	assert(IsLoading);

#if BIOSHOCK
	if (Game == GAME_Bioshock)
	{
		*this << AR_INDEX(N.Index) << N.ExtraIndex;
		if (N.ExtraIndex == 0)
		{
			N.Str = GetName(N.Index);
		}
		else
		{
			N.Str = appStrdupPool(va("%s%d", GetName(N.Index), N.ExtraIndex-1));	// without "_" char
		}
		return *this;
	}
#endif // BIOSHOCK

#if UC2
	if (Engine() == GAME_UE2X && ArVer >= 145)
	{
		*this << N.Index;
	}
	else
#endif // UC2
#if LEAD
	if (Game == GAME_SplinterCellConv && ArVer >= 64)
	{
		*this << N.Index;
	}
	else
#endif // LEAD
#if UNREAL3 || UNREAL4
	if (Engine() >= GAME_UE3)
	{
		*this << N.Index;
		if (Game >= GAME_UE4_BASE) goto extra_index;
	#if R6VEGAS
		if (Game == GAME_R6Vegas2)
		{
			N.ExtraIndex = N.Index >> 19;
			N.Index &= 0x7FFFF;
		}
	#endif // R6VEGAS
		if (ArVer >= 343)
		{
		extra_index:
			*this << N.ExtraIndex;
		}
	}
	else
#endif // UNREAL3 || UNREAL4
	{
		// UE1 and UE2
		*this << AR_INDEX(N.Index);
	}

	// Convert name index to string
#if UNREAL3 || UNREAL4
	if (N.ExtraIndex == 0)
	{
		N.Str = GetName(N.Index);
	}
	else
	{
		N.Str = appStrdupPool(va("%s_%d", GetName(N.Index), N.ExtraIndex-1));
	}
#else
	// no modern engines compiled
	N.Str = GetName(N.Index);
#endif // UNREAL3 || UNREAL4

	return *this;

	unguardf("pos=%08X", Tell());
}

FArchive& UnPackage::operator<<(UObject *&Obj)
{
	guard(UnPackage::SerializeUObject);

	assert(IsLoading);
	int index;
#if UC2
	if (Engine() == GAME_UE2X && ArVer >= 145)
		*this << index;
	else
#endif
#if UNREAL3 || UNREAL4
	if (Engine() >= GAME_UE3)
		*this << index;
	else
#endif // UNREAL3 || UNREAL4
		*this << AR_INDEX(index);

	if (index < 0)
	{
//		const FObjectImport &Imp = GetImport(-index-1);
//		appPrintf("PKG: Import[%s,%d] OBJ=%s CLS=%s\n", GetObjectName(Imp.PackageIndex), index, *Imp.ObjectName, *Imp.ClassName);
		Obj = CreateImport(-index-1);
	}
	else if (index > 0)
	{
//		const FObjectExport &Exp = GetExport(index-1);
//		appPrintf("PKG: Export[%d] OBJ=%s CLS=%s\n", index, *Exp.ObjectName, GetObjectName(Exp.ClassIndex));
		Obj = CreateExport(index-1);
	}
	else // index == 0
	{
		Obj = NULL;
	}
	return *this;

	unguard;
}

/*-----------------------------------------------------------------------------
	Loading particular import or export package entry
-----------------------------------------------------------------------------*/

int UnPackage::FindExport(const char *name, const char *className, int firstIndex) const
{
	for (int i = firstIndex; i < Summary.ExportCount; i++)
	{
		const FObjectExport &Exp = ExportTable[i];
		// compare object name
		if (stricmp(Exp.ObjectName, name) != 0)
			continue;
		// if class name specified - compare it too
		const char *foundClassName = GetObjectName(Exp.ClassIndex);
		if (className && stricmp(foundClassName, className) != 0)
			continue;
		return i;
	}
	return INDEX_NONE;
}


bool UnPackage::CompareObjectPaths(int PackageIndex, UnPackage *RefPackage, int RefPackageIndex) const
{
	guard(UnPackage::CompareObjectPaths);

/*	appPrintf("Compare %s.%s [%d] with %s.%s [%d]\n",
		Name, GetObjectName(PackageIndex), PackageIndex,
		RefPackage->Name, RefPackage->GetObjectName(RefPackageIndex), RefPackageIndex
	); */

	while (PackageIndex || RefPackageIndex)
	{
		const char *PackageName, *RefPackageName;

		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else if (PackageIndex > 0)
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
			PackageName  = Name;

		if (RefPackageIndex < 0)
		{
			const FObjectImport &Rec = RefPackage->GetImport(-RefPackageIndex-1);
			RefPackageIndex = Rec.PackageIndex;
			RefPackageName  = Rec.ObjectName;
		}
		else if (RefPackageIndex > 0)
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = RefPackage->GetExport(RefPackageIndex-1);
			RefPackageIndex = Rec.PackageIndex;
			RefPackageName  = Rec.ObjectName;
		}
		else
			RefPackageName  = RefPackage->Name;
//		appPrintf("%20s -- %20s\n", PackageName, RefPackageName);
		if (stricmp(RefPackageName, PackageName) != 0) return false;
	}

	return true;

	unguard;
}


int UnPackage::FindExportForImport(const char *ObjectName, const char *ClassName, UnPackage *ImporterPackage, int ImporterIndex)
{
	guard(FindExportForImport);

	int ObjIndex = -1;
	while (true)
	{
		// iterate all objects with the same name and class
		ObjIndex = FindExport(ObjectName, ClassName, ObjIndex + 1);
		if (ObjIndex == INDEX_NONE)
			break;				// not found
		if (Game >= GAME_UE4_BASE)
		{
			// UE4 usually has single object in package. Plus, each object import has a parent UPackage
			// describing where to get an object, but object export has no parent UObject - so depth
			// of import path is 1 step deeper than depth of export path, and CompareObjectPaths()
			// will always fail.
			return ObjIndex;
		}
		// a few objects in package could have the same name and class but resides in different groups,
		// so compare full object paths for sure
		if (CompareObjectPaths(ObjIndex+1, ImporterPackage, -1-ImporterIndex))
			return ObjIndex;	// found
	}

	return INDEX_NONE;			// not found

	unguard;
}


UObject* UnPackage::CreateExport(int index)
{
	guard(UnPackage::CreateExport);

	// create empty object
	FObjectExport &Exp = GetExport(index);
	if (Exp.Object)
		return Exp.Object;


	// check if this object actually contains only default properties and nothing more
	bool shouldSkipObject = false;

	if (!strnicmp(Exp.ObjectName, "Default__", 9))
	{
		// default properties are not supported -- this is a clean UObject format
		shouldSkipObject = true;
	}
#if UNREAL4
	if (Game >= GAME_UE4_BASE)
	{
		// This could be a blueprint - it contains objects which are marked as 'StaticMesh' class,
		// but really containing nothing. Such objects are always contained inside some Default__... parent.
		const char* OuterName = GetObjectPackageName(Exp.PackageIndex);
		if (OuterName && !strnicmp(OuterName, "Default__", 9))
		{
			shouldSkipObject = true;
		}
	}
#endif // UNREAL4

	if (shouldSkipObject)
	{
		return NULL;
	}

	const char *ClassName = GetObjectName(Exp.ClassIndex);
	UObject *Obj = Exp.Object = CreateClass(ClassName);
	if (!Obj)
	{
		appPrintf("WARNING: Unknown class \"%s\" for object \"%s\"\n", ClassName, *Exp.ObjectName);
		return NULL;
	}
#if UNREAL3
	if (Game >= GAME_UE3 && (Exp.ExportFlags & EF_ForcedExport)) // ExportFlags appeared in ArVer=247
	{
		// find outermost package
		if (Exp.PackageIndex)
		{
			int PackageIndex = Exp.PackageIndex - 1;			// subtract 1, because 0 = no parent
			while (true)
			{
				const FObjectExport &Exp2 = GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;
				PackageIndex = Exp2.PackageIndex - 1;			// subtract 1 ...
			}
/*!! -- should display this message during serialization in UObject::EndLoad()
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			assert(Exp2.ExportFlags & EF_ForcedExport);
			const char *PackageName = Exp2.ObjectName;
			appPrintf("Forced export (%s): %s'%s.%s'\n", Name, ClassName, PackageName, *Exp.ObjectName);
*/
		}
	}
#endif // UNREAL3
	UObject::BeginLoad();

	// find outer object
	UObject *Outer = NULL;
	if (Exp.PackageIndex)
	{
		const FObjectExport &OuterExp = GetExport(Exp.PackageIndex - 1);
		Outer = OuterExp.Object;
		if (!Outer)
		{
			const char *OuterClassName = GetObjectName(OuterExp.ClassIndex);
			if (IsKnownClass(OuterClassName))			// avoid error message if class name is not registered
				Outer = CreateExport(Exp.PackageIndex - 1);
		}
	}

	// setup constant object fields
	Obj->Package      = this;
	Obj->PackageIndex = index;
	Obj->Outer        = Outer;
	Obj->Name         = Exp.ObjectName;

	// add object to GObjLoaded for later serialization
	UObject::GObjLoaded.Add(Obj);

	// perform serialization
	UObject::EndLoad();
	return Obj;

	unguardf("%s:%d", Filename, index);
}


UObject* UnPackage::CreateImport(int index)
{
	guard(UnPackage::CreateImport);

	FObjectImport &Imp = GetImport(index);
	if (Imp.Missing) return NULL;	// error message already displayed for this entry

	// load package
	const char *PackageName = GetObjectPackageName(Imp.PackageIndex);
	UnPackage *Package = LoadPackage(PackageName);
	int ObjIndex = INDEX_NONE;

	if (Package)
	{
		// has package with exactly this name - so this is either UE < 3 or non-cooked UE3 export
		ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
		if (ObjIndex == INDEX_NONE)
		{
			appPrintf("WARNING: Import(%s) was not found in package %s\n", *Imp.ObjectName, PackageName);
			Imp.Missing = true;
			return NULL;
		}
	}
#if UNREAL3
	// try to find import in startup package
	else if (Engine() >= GAME_UE3 && Engine() < GAME_UE4_BASE)
	{
		// check startup package
		Package = LoadPackage(GStartupPackage);
		if (Package)
			ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
		// look in other loaded packages
		if (ObjIndex == INDEX_NONE)
		{
			//?? speedup this search when many packages are loaded (not tested, perhaps works well enough)
			UnPackage *SkipPackage = Package;	// Package = either startup package or NULL
			for (int i = 0; i < PackageMap.Num(); i++)
			{
				Package = PackageMap[i];
				//appPrintf("check for %s in %s\n", *Imp.ObjectName, Package->Name); - don't spam, could slowdown when many packages loaded!
				if (Package == this || Package == SkipPackage)
					continue;		// already checked
				ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
				if (ObjIndex != INDEX_NONE)
					break;			// found
			}
		}
		if (ObjIndex == INDEX_NONE)
		{
			appPrintf("WARNING: Import(%s.%s) was not found\n", PackageName, *Imp.ObjectName);
			Imp.Missing = true;
			return NULL;
		}
	}
#endif // UNREAL3

	// at this point we have either Package == NULL (not found) or Package != NULL and ObjIndex is valid

	if (!Package)
	{
		appPrintf("WARNING: Import(%s'%s'): package %s was not found\n", *Imp.ClassName, *Imp.ObjectName, PackageName);
		Imp.Missing = true;
		return NULL;
	}

	// create object
	return Package->CreateExport(ObjIndex);

	unguardf("%s:%d", Filename, index);
}


// get outermost package name
//?? this function is not correct, it is used in package exporter tool only
const char *UnPackage::GetObjectPackageName(int PackageIndex) const
{
	guard(UnPackage::GetObjectPackageName);

	const char *PackageName = NULL;
	while (PackageIndex)
	{
		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
	}
	return PackageName;

	unguard;
}


// get full object path in a form
// "OutermostPackage.Package1...PackageN.ObjectName"
void UnPackage::GetFullExportName(const FObjectExport &Exp, char *buf, int bufSize, bool IncludeObjectName, bool IncludeCookedPackageName) const
{
	guard(UnPackage::GetFullExportNameBase);

	const char *PackageNames[256];
	const char *PackageName;
	int NestLevel = 0;

	// get object name
	if (IncludeObjectName)
	{
		PackageName = Exp.ObjectName;
		PackageNames[NestLevel++] = PackageName;
	}

	// gather nested package names (object parents)
	int PackageIndex = Exp.PackageIndex;
	while (PackageIndex)
	{
		assert(NestLevel < ARRAY_COUNT(PackageNames));
		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
#if UNREAL3
			if (PackageIndex == 0 && (Rec.ExportFlags && EF_ForcedExport) && !IncludeCookedPackageName)
				break;		// do not add cooked package name
#endif
		}
		PackageNames[NestLevel++] = PackageName;
	}
	// concatenate package names in reverse order (from root to object)
	*buf = 0;
	for (int i = NestLevel-1; i >= 0; i--)
	{
		const char *PackageName = PackageNames[i];
		char *dst = strchr(buf, 0);
		appSprintf(dst, bufSize - (dst - buf), "%s%s", PackageName, i > 0 ? "." : "");
	}

	unguard;
}


const char *UnPackage::GetUncookedPackageName(int PackageIndex) const
{
	guard(UnPackage::GetUncookedPackageName);

#if UNREAL3
	if (PackageIndex != INDEX_NONE)
	{
		const FObjectExport &Exp = GetExport(PackageIndex);
		if (Game >= GAME_UE3 && (Exp.ExportFlags & EF_ForcedExport))
		{
			// find outermost package
			while (true)
			{
				const FObjectExport &Exp2 = GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;			// get parent (UPackage)
				PackageIndex = Exp2.PackageIndex - 1;	// subtract 1 from package index
			}
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			return *Exp2.ObjectName;
		}
	}
#endif // UNREAL3
	return Name;

	unguard;
}


/*-----------------------------------------------------------------------------
	Searching for package and maintaining package list
-----------------------------------------------------------------------------*/

TArray<UnPackage*>	UnPackage::PackageMap;
TArray<char*>		MissingPackages;

UnPackage *UnPackage::LoadPackage(const char *Name, bool silent)
{
	guard(UnPackage::LoadPackage);

	const char *LocalName = appSkipRootDir(Name);

	// Call appFindGameFile() first. This function is fast because it uses
	// hashing internally.
	const CGameFileInfo *info = appFindGameFile(LocalName);

	int i;

	if (info && info->IsPackage)
	{
		// Check if package was already loaded.
		if (info->Package)
			return info->Package;
		// Load the package.
		UnPackage* package = new UnPackage(info->RelativeName, appCreateFileReader(info), silent);
		// Cache pointer in CGameFileInfo so next time it will be found quickly.
		const_cast<CGameFileInfo*>(info)->Package = package;
		return package;
	}
	else
	{
		// The file was not found in registered game files. Probably the file name
		// was specified fully qualified, with full path name, outside of root game path.
		// This is rare situation, so we can allow a bit unoptimized code here - linear search
		// for package inside a PackageMap array.

		// Check in missing package names. This check will allow to print "missing package"
		// warning only once.
		for (i = 0; i < MissingPackages.Num(); i++)
			if (!stricmp(LocalName, MissingPackages[i]))
				return NULL;
		// Check in loaded packages list. This is done to prevent loading the same package
		// twice when this function is called with a different filename qualifiers:
		// "path/package.ext", "package.ext", "package"
		for (i = 0; i < PackageMap.Num(); i++)
			if (!stricmp(LocalName, PackageMap[i]->Filename))
				return PackageMap[i];
		// Try to load package.
		if (appFileExists(Name))
			return new UnPackage(Name, NULL, silent);
	}

	// The package is missing. Do not print any warnings: missing package is a normal situation
	// in UE3 cooked builds, so print warnings when needed at upper level.
//	appPrintf("WARNING: package %s was not found\n", Name);
	MissingPackages.Add(appStrdup(LocalName));
	return NULL;

	unguardf("%s", Name);
}
