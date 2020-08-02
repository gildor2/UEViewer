#include "Core.h"

#include "UnCore.h"
#include "UE4Version.h"
#include "UnObject.h"
#include "UnPackage.h"
#include "UnPackageUE3Reader.h"

#include "GameDatabase.h"		// for GetGameTag()


//#define DEBUG_PACKAGE			1
//#define PROFILE_PACKAGE_TABLES	1

#define MAX_FNAME_LEN			MAX_PACKAGE_PATH	// Maximal length of FName, used for stack variables

/*-----------------------------------------------------------------------------
	Unreal package structures
-----------------------------------------------------------------------------*/

void FPackageFileSummary::Serialize2(FArchive &Ar)
{
	guard(FPackageFileSummary::Serialize2);

#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell && Ar.ArLicenseeVer >= 83)
	{
		int32 unk8;
		Ar << unk8;
	}
#endif // SPLINTER_CELL

	Ar << PackageFlags;

#if LEAD
	if (Ar.Game == GAME_SplinterCellConv)
	{
		int32 unk10;
		Ar << unk10;		// some flags?
	}
#endif // LEAD

	Ar << NameCount << NameOffset << ExportCount << ExportOffset << ImportCount << ImportOffset;

#if LEAD
	if (Ar.Game == GAME_SplinterCellConv && Ar.ArLicenseeVer >= 48)
	{
		// this game has additional name table for some packages
		int32 ExtraNameCount, ExtraNameOffset;
		int32 unkC;
		Ar << ExtraNameCount << ExtraNameOffset;
		if (ExtraNameOffset < ImportOffset) ExtraNameCount = 0;
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
	if ((GForceGame == 0) && (PackageFlags & 0x10000) && (Ar.ArVer >= 0x80 && Ar.ArVer < 0x88))	//?? unknown upper limit; known lower limit: 0x80
	{
		// encrypted Ragnarok Online archive header (data taken by archive analysis)
		Ar.Game = GAME_Ragnarok2;
		NameCount    ^= 0xD97790C7 ^ 0x1C;
		NameOffset   ^= 0xF208FB9F ^ 0x40;
		ExportCount  ^= 0xEBBDE077 ^ 0x04;
		ExportOffset ^= 0xE292EC62 ^ 0x03E9E1;
		ImportCount  ^= 0x201DA87A ^ 0x05;
		ImportOffset ^= 0xA9B999DF ^ 0x003E9BE;
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
			Generations.Empty(1);
			FGenerationInfo gen;
			gen.ExportCount = ExportCount;
			gen.NameCount   = NameCount;
			Generations.Add(gen);
		}
	}
	else
	{
		Ar << Guid;
		// current generations code
	generations:
		// always used int for generation count (even for UE1-2)
		int32 Count;
		Ar << Count;
		Generations.Empty(Count);
		Generations.AddZeroed(Count);
		for (int i = 0; i < Count; i++)
			Ar << Generations[i];
	}

	unguard;
}


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
	if (Ar.Game == GAME_MassEffect3 && Ar.ArLicenseeVer >= 194 && (PackageFlags & 8))
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
	if (Ar.Game == GAME_Tera && (PackageFlags & 8)) NameCount -= NameOffset;
#endif

	if (Ar.ArVer >= 415)
		Ar << DependsOffset;

#if BIOSHOCK3
	if (Ar.Game == GAME_Bioshock3) goto read_unk38;
#endif

#if DUNDEF
	if (Ar.Game == GAME_DunDef)
	{
		if (PackageFlags & 8)
		{
			int32 unk38;
			Ar << unk38;
		}
	}
#endif // DUNDEF

	if (Ar.ArVer >= 623)
		Ar << f38 << f3C << f40;

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
	Ar << Guid;
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
		Ar << CompressionFlags << CompressedChunks;
	if (Ar.ArVer >= 482)
		Ar << U3unk60;
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
	if (!Package)
		return -1;

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

void FPackageFileSummary::Serialize4(FArchive &Ar)
{
	guard(FPackageFileSummary::Serialize4);

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

	static const int LatestSupportedLegacyVer = (int) -ARRAY_COUNT(legacyVerToEngineVer)+1;
//	appPrintf("%d -> %d %d < %d\n", LegacyVersion, -LegacyVersion - 1, legacyVerToEngineVer[-LegacyVersion - 1], LatestSupportedLegacyVer);
	if (LegacyVersion < LatestSupportedLegacyVer || LegacyVersion >= -1)	// -2 is supported
		appError("UE4 LegacyVersion: unsupported value %d", LegacyVersion);

	IsUnversioned = false;

	// read versions
	int32 VersionUE3, Version, LicenseeVersion;		// note: using int32 instead of uint16 as in UE1-UE3
	if (LegacyVersion != -4)						// UE4 had some changes for version -4, but these changes were reverted in -5 due to some problems
		Ar << VersionUE3;
	Ar << Version << LicenseeVersion;
	// VersionUE3 is ignored
	assert((Version & ~0xFFFF) == 0);
	assert((LicenseeVersion & ~0xFFFF) == 0);
	FileVersion     = Version & 0xFFFF;
	LicenseeVersion = LicenseeVersion & 0xFFFF;

	// store file version to archive
	Ar.ArVer         = FileVersion;
	Ar.ArLicenseeVer = LicenseeVersion;

	if (FileVersion == 0 && LicenseeVersion == 0)
		IsUnversioned = true;

	if (IsUnversioned && GForceGame == GAME_UNKNOWN)
	{
		int ver = -LegacyVersion - 1;
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

	if (LegacyVersion <= -2)
	{
		// CustomVersions array - not serialized to unversioned packages, and UE4 always consider
		// all custom versions to use highest available value. However this is used for versioned
		// packages: engine starts to use custom versions heavily starting with 4.12.
		CustomVersionContainer.Serialize(Ar, LegacyVersion);
	}

	// Conan Exiles: has TArray<int32[5]> inserted here. Also it has int32=0 before Tag

	Ar << HeadersSize;
	Ar << PackageGroup;
	Ar << PackageFlags;

	Ar << NameCount << NameOffset;

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

	Ar << ExportCount << ExportOffset << ImportCount << ImportOffset;
	Ar << DependsOffset;

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

	// guid
	Ar << Guid;

	if (Ar.ArVer >= VER_UE4_ADDED_PACKAGE_OWNER && Ar.ContainsEditorData())
	{
		FGuid PersistentGuid, OwnerPersistentGuid;
		Ar << PersistentGuid << OwnerPersistentGuid;
	}

	// generations
	int32 Count;
	Ar << Count;
	Generations.Empty(Count);
	Generations.AddZeroed(Count);
	for (int i = 0; i < Count; i++)
		Ar << Generations[i];

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
	Ar << CompressionFlags;
	Ar << CompressedChunks;

	int32 PackageSource;
	Ar << PackageSource;

#if ARK
	// Ark: Survival Evolved - starting with some version (March 2018?), serializaiton requires this:
	if (Ar.Game == GAME_Ark && Ar.ArLicenseeVer >= 10)
	{
		int64 unk;		// it seems this value is always 0x1234
		Ar << unk;
	}
#endif // ARK

	TArray<FString> AdditionalPackagesToCook;
	Ar << AdditionalPackagesToCook;

	if (LegacyVersion > -7)
	{
		int32 NumTextureAllocations;
		Ar << NumTextureAllocations;
		assert(NumTextureAllocations == 0); // actually this was an array before
	}

	if (Ar.ArVer >= VER_UE4_ASSET_REGISTRY_TAGS)
	{
		int32 AssetRegistryDataOffset;
		Ar << AssetRegistryDataOffset;
	}

#if GEARS4 || SEAOFTHIEVES
	if (Ar.Game == GAME_Gears4 || Ar.Game == GAME_SeaOfThieves) Ar.Seek(Ar.Tell()+6); // no idea what happens inside
#endif // GEARS4 || SEAOFTHIEVES

	if (Ar.ArVer >= VER_UE4_SUMMARY_HAS_BULKDATA_OFFSET)
	{
		Ar << BulkDataStartOffset;
//		appPrintf("Bulk offset: %llX\n", BulkDataStartOffset);
	}

	//!! other fields - useless for now

	unguard;
}

#endif // UNREAL4

FPackageFileSummary::FPackageFileSummary()
{
	memset(this, 0, sizeof(*this));
}

bool FPackageFileSummary::Serialize(FArchive &Ar)
{
	guard(FPackageFileSummary<<);
	assert(Ar.IsLoading);						// saving is not supported

	// read package tag
	Ar << Tag;

	// some games has special tag constants
#if SPECIAL_TAGS
	if (Tag == 0x9E2A83C2) goto tag_ok;			// Killing Floor
	if (Tag == 0x7E4A8BCA) goto tag_ok;			// iStorm
#endif // SPECIAL_TAGS
#if NURIEN
	if (Tag == 0xA94E6C81) goto tag_ok;			// Nurien
#endif
#if BATTLE_TERR
	if (Tag == 0xA1B2C93F)
	{
		Ar.Game = GAME_BattleTerr;
		goto tag_ok;							// Battle Territory Online
	}
#endif // BATTLE_TERR
#if LOCO
	if (Tag == 0xD58C3147)
	{
		Ar.Game = GAME_Loco;
		goto tag_ok;							// Land of Chaos Online
	}
#endif // LOCO
#if BERKANIX
	if (Tag == 0xF2BAC156)
	{
		Ar.Game = GAME_Berkanix;
		goto tag_ok;
	}
#endif // BERKANIX
#if HAWKEN
	if (Tag == 0xEA31928C)
	{
		Ar.Game = GAME_Hawken;
		goto tag_ok;
	}
#endif // HAWKEN
#if TAO_YUAN
	if (Tag == 0x12345678)
	{
		int tmp;			// some additional version?
		Ar << tmp;
		Ar.Game = GAME_TaoYuan;
		if (!GForceGame) GForceGame = GAME_TaoYuan;
		goto tag_ok;
	}
#endif // TAO_YUAN
#if STORMWAR
	if (Tag == 0xEC201133)
	{
		byte Count;
		Ar << Count;
		Ar.Seek(Ar.Tell() + Count);
		goto tag_ok;
	}
#endif // STORMWAR
#if GUNLEGEND
	if (Tag == 0x879A4B41)
	{
		Ar.Game = GAME_GunLegend;
		if (!GForceGame) GForceGame = GAME_GunLegend;
		goto tag_ok;
	}
#endif // GUNLEGEND
#if MMH7
	if (Tag == 0x4D4D4837)
	{
		Ar.Game = GAME_UE3;	// version conflict with Guilty Gear Xrd
		goto tag_ok;		// Might & Magic Heroes 7
	}
#endif // MMH7
#if DEVILS_THIRD
	if (Tag == 0x7BC342F0)
	{
		Ar.Game = GAME_DevilsThird;
		if (!GForceGame) GForceGame = GAME_DevilsThird;
		goto tag_ok;		// Devil's Third
	}
#endif // DEVILS_THIRD

	// support reverse byte order
	if (Tag != PACKAGE_FILE_TAG)
	{
		if (Tag != PACKAGE_FILE_TAG_REV)
		{
			UnPackage* file = Ar.CastTo<UnPackage>();
			appNotify("Wrong package tag (%08X) in file %s. Probably the file is encrypted.", Tag, file ? *file->GetFilename() : "(unknown)");
			return false;
		}
		Ar.ReverseBytes = true;
		Tag = PACKAGE_FILE_TAG;
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
		LegacyVersion = Version;
		Ar.Game = GAME_UE4_BASE;
		Serialize4(Ar);
		//!! note: UE4 requires different DetectGame way, perhaps it's not possible at all
		//!! (but can use PAK file names for game detection)
		return true;
	}
#endif // UNREAL4

#if UNREAL3
	if (Version == PACKAGE_FILE_TAG || Version == 0x20000)
		appError("Fully compressed package header?");
#endif // UNREAL3

	FileVersion     = Version & 0xFFFF;
	LicenseeVersion = Version >> 16;
	// store file version to archive (required for some structures, for UNREAL3 path)
	Ar.ArVer         = FileVersion;
	Ar.ArLicenseeVer = LicenseeVersion;
	// detect game
	Ar.DetectGame();
	Ar.OverrideVersion();

	// read other fields

#if UNREAL3
	if (Ar.Game >= GAME_UE3)
		Serialize3(Ar);
	else
#endif
		Serialize2(Ar);

#if DEBUG_PACKAGE
	appPrintf("EngVer:%d CookVer:%d CompF:%d CompCh:%d\n", EngineVersion, CookerVersion, CompressionFlags, CompressedChunks.Num());
	appPrintf("Names:%X[%d] Exports:%X[%d] Imports:%X[%d]\n", NameOffset, NameCount, ExportOffset, ExportCount, ImportOffset, ImportCount);
	appPrintf("HeadersSize:%X Group:%s DependsOffset:%X U60:%X\n", HeadersSize, *PackageGroup, DependsOffset, U3unk60);
#endif

	return true;

	unguardf("Ver=%d/%d", FileVersion, LicenseeVersion);
}


void FObjectExport::Serialize2(FArchive &Ar)
{
	guard(FObjectExport::Serialize2);

#if USE_COMPACT_PACKAGE_STRUCTS
	int32		SuperIndex;
	uint32		ObjectFlags;
#endif

#if PARIAH
	if (Ar.Game == GAME_Pariah)
	{
		Ar << ObjectName << AR_INDEX(SuperIndex) << PackageIndex << ObjectFlags;
		Ar << AR_INDEX(ClassIndex) << AR_INDEX(SerialSize);
		if (SerialSize)
			Ar << AR_INDEX(SerialOffset);
		return;
	}
#endif // PARIAH
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		int unkC, flags2, unk30;
		Ar << AR_INDEX(ClassIndex) << AR_INDEX(SuperIndex) << PackageIndex;
		if (Ar.ArVer >= 132) Ar << unkC;			// unknown
		Ar << ObjectName << ObjectFlags;
		if (Ar.ArLicenseeVer >= 40) Ar << flags2;	// qword flags
		Ar << AR_INDEX(SerialSize);
		if (SerialSize)
			Ar << AR_INDEX(SerialOffset);
		if (Ar.ArVer >= 130) Ar << unk30;			// unknown
		return;
	}
#endif // BIOSHOCK
#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 151)
	{
		int unk0C;
		Ar << AR_INDEX(ClassIndex) << AR_INDEX(SuperIndex) << PackageIndex;
		if (Ar.ArVer >= 159) Ar << AR_INDEX(unk0C);
		Ar << ObjectName << ObjectFlags << SerialSize << SerialOffset;
		return;
	}
#endif // SWRC
#if AA2
	if (Ar.Game == GAME_AA2)
	{
		int rnd;	// random value
		Ar << AR_INDEX(SuperIndex) << rnd << AR_INDEX(ClassIndex) << PackageIndex;
		Ar << ObjectFlags << ObjectName << AR_INDEX(SerialSize);	// ObjectFlags are serialized in different order, ObjectFlags are negated
		if (SerialSize)
			Ar << AR_INDEX(SerialOffset);
		ObjectFlags = ~ObjectFlags;
		return;
	}
#endif // AA2

	// generic UE1/UE2 code
	Ar << AR_INDEX(ClassIndex) << AR_INDEX(SuperIndex) << PackageIndex;
	Ar << ObjectName << ObjectFlags << AR_INDEX(SerialSize);
	if (SerialSize)
		Ar << AR_INDEX(SerialOffset);

	unguard;
}


#if UC2

void FObjectExport::Serialize2X(FArchive &Ar)
{
	guard(FObjectExport::Serialize2X);

#if USE_COMPACT_PACKAGE_STRUCTS
	int32		SuperIndex;
	uint32		ObjectFlags;
#endif

	Ar << ClassIndex << SuperIndex;
	if (Ar.ArVer >= 150)
	{
		int16 idx = PackageIndex;
		Ar << idx;
		PackageIndex = idx;
	}
	else
		Ar << PackageIndex;
	Ar << ObjectName << ObjectFlags << SerialSize;
	if (SerialSize)
		Ar << SerialOffset;
	// UC2 has strange thing here: indices are serialized as 4-byte int (instead of AR_INDEX),
	// but stored into 2-byte shorts
	ClassIndex = int16(ClassIndex);
	SuperIndex = int16(SuperIndex);

	unguard;
}

#endif // UC2


#if UNREAL3

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

#endif // UNREAL3


#if UNREAL4

void FObjectExport::Serialize4(FArchive &Ar)
{
	guard(FObjectExport::Serialize4);

#if USE_COMPACT_PACKAGE_STRUCTS
	// locally declare FObjectImport data which are stripped
	int32		SuperIndex;
	uint32		ObjectFlags;
	int32		Archetype;
	TArray<int32> NetObjectCount;
	FGuid		Guid;
	int32		PackageFlags;
	int32		TemplateIndex;
#endif // USE_COMPACT_PACKAGE_STRUCTS

	Ar << ClassIndex << SuperIndex;
	if (Ar.ArVer >= VER_UE4_TemplateIndex_IN_COOKED_EXPORTS) Ar << TemplateIndex;
	Ar << PackageIndex << ObjectName;
	if (Ar.ArVer < VER_UE4_REMOVE_ARCHETYPE_INDEX_FROM_LINKER_TABLES) Ar << Archetype;

	Ar << ObjectFlags;

	if (Ar.ArVer < VER_UE4_64BIT_EXPORTMAP_SERIALSIZES)
	{
		Ar << SerialSize << SerialOffset;
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
		SerialSize = (int32)SerialSize64;
		SerialOffset = (int32)SerialOffset64;
	}

	bool bForcedExport, bNotForClient, bNotForServer;
	Ar << bForcedExport << bNotForClient << bNotForServer;
	if (Ar.IsLoading)
	{
		ExportFlags = bForcedExport ? EF_ForcedExport : 0;	//?? change this
	}

	if (Ar.ArVer < VER_UE4_REMOVE_NET_INDEX) Ar << NetObjectCount;

	Ar << Guid << PackageFlags;

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

	unguard;
}

#endif // UNREAL4


FArchive& operator<<(FArchive &Ar, FObjectExport &E)
{
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		E.Serialize4(Ar);
		return Ar;
	}
#endif
#if UNREAL3
	if (Ar.Game >= GAME_UE3)
	{
		E.Serialize3(Ar);
		return Ar;
	}
#endif
#if UC2
	if (Ar.Engine() == GAME_UE2X)
	{
		E.Serialize2X(Ar);
		return Ar;
	}
#endif
	E.Serialize2(Ar);
	return Ar;
}


void FObjectImport::Serialize(FArchive& Ar)
{
	guard(FObjectImport::Serialize);

#if USE_COMPACT_PACKAGE_STRUCTS
	FName		ClassPackage;
#endif

#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 150)
	{
		int16 idx = PackageIndex;
		Ar << ClassPackage << ClassName << idx << ObjectName;
		PackageIndex = idx;
		return;
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
		Ar << ClassPackage << ClassName << unk << ObjectName << PackageIndex;
		return;
	}
#endif

	// this code is the same for all engine versions
	Ar << ClassPackage << ClassName << PackageIndex << ObjectName;

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		// MK X
		FGuid unk;
		Ar << unk;
	}
#endif // MKVSDC

	unguard;
}


/*-----------------------------------------------------------------------------
	Package loading (creation) / unloading
-----------------------------------------------------------------------------*/

UnPackage::UnPackage(const char *filename, const CGameFileInfo* fileInfo, bool silent)
:	Loader(NULL)
{
	guard(UnPackage::UnPackage);

#if PROFILE_PACKAGE_TABLES
	appResetProfiler();
#endif

	IsLoading = true;
	FileInfo = fileInfo;

	FArchive* baseLoader = NULL;
	if (FileInfo)
	{
		baseLoader = FileInfo->CreateReader();
	}
	else
	{
		// The file was not registered, so duplicate the file name
		FilenameNoInfo = appStrdupPool(appSkipRootDir(filename));
	}

	Loader = CreateLoader(filename, baseLoader);
	if (!Loader)
	{
		// File is too small
		return;
	}

	SetupFrom(*Loader);

	// read summary
	if (!Summary.Serialize(*this))
	{
		// Probably not a package
		return;
	}

	Loader->SetupFrom(*this);	// serialization of FPackageFileSummary could change some FArchive properties

#if !DEBUG_PACKAGE
	if (!silent)
#endif
	{
		PKG_LOG("Loading package: %s Ver: %d/%d ", *GetFilename(), Loader->ArVer, Loader->ArLicenseeVer);
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

	ReplaceLoader();

#if UNREAL3
	if (Game >= GAME_UE3 && Summary.CompressionFlags && Summary.CompressedChunks.Num())
	{
		FUE3ArchiveReader* UE3Loader = Loader->CastTo<FUE3ArchiveReader>();
		if (UE3Loader && UE3Loader->IsFullyCompressed)
			appError("Fully compressed package %s has additional compression table", filename);
		// replace Loader with special reader for compressed UE3 archives
		Loader = new FUE3ArchiveReader(Loader, Summary.CompressionFlags, Summary.CompressedChunks);
	}
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
	// Process Event Driven Loader packages: such packages are split into 2 pieces: .uasset with headers
	// and .uexp with object's data. At this moment we already have FPackageFileSummary fully loaded,
	// so we can replace loader with .uexp file - with providing correct position offset.
	if (Game >= GAME_UE4_BASE && Summary.HeadersSize == Loader->GetFileSize())
	{
		guard(FindUexp);
		char buf[MAX_PACKAGE_PATH];
		appStrncpyz(buf, filename, ARRAY_COUNT(buf));
		char* s = strrchr(buf, '.');
		if (!s)
		{
			s = strchr(buf, 0);
		}
		strcpy(s, ".uexp");
		// When finding, explicitly tell to use the same folder where .uasset file exists
		const CGameFileInfo *expInfo = CGameFileInfo::Find(buf, FileInfo ? FileInfo->FolderIndex : -1);
		if (expInfo)
		{
			// Open .exp file
			FArchive* expLoader = expInfo->CreateReader();
			// Replace loader with this file, but add offset so it will work like it is part of original uasset
			delete Loader;
			Loader = new FReaderWrapper(expLoader, -Summary.HeadersSize);
		}
		else
		{
			appPrintf("WARNING: it seems package %s has missing .uexp file\n", filename);
		}
		unguard;
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
	appPrintProfiler("Package loaded");
#endif

	unguardf("%s, ver=%d/%d, game=%s", filename, ArVer, ArLicenseeVer, GetGameTag(Game));
}


void UnPackage::LoadNameTable()
{
	guard(UnPackage::LoadNameTable);

	if (Summary.NameCount == 0) return;

	Seek(Summary.NameOffset);
	NameTable = new const char* [Summary.NameCount];
	FStaticString<MAX_FNAME_LEN> nameStr;

	for (int i = 0; i < Summary.NameCount; i++)
	{
		guard(Name);

#if UNREAL4
		if (Game >= GAME_UE4_BASE) goto ue4_name; // bypass all of pre-UE4 game checks
#endif

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
#if SPLINTER_CELL
			if (Game == GAME_SplinterCell && ArLicenseeVer >= 85)
			{
				char buf[256];
				byte len;
				int flags;
				*this << len;
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

		ue4_name:
			// Korean games sometimes uses Unicode strings ...
			*this << nameStr;
	#if AVA
			if (Game == GAME_AVA)
			{
				// strange code - package contains some bytes:
				// V(0) = len ^ 0x3E
				// V(i) = V(i-1) + 0x48 ^ 0xE1
				// Number of bytes = (len ^ 7) & 0xF
				int skip = nameStr.Len();
				skip = (skip ^ 7) & 0xF;
				Seek(Tell() + skip);
			}
	#endif // AVA

			// Verify name, some Korean games (B&S) has garbage there.
			// Use separate block to not mess with 'goto crossing variable initialization' error.
			{
				// Paragon has many names ended with '\n', so it's good idea to trim spaces
				nameStr.TrimStartAndEndInline();
				bool goodName = true;
				int numBadChars = 0;
				for (char c : nameStr.GetDataArray())
				{
					if (c < ' ' || c > 0x7F)
					{
						if (c == 0) break; // end of line is included into FString
						// unreadable character
						goodName = false;
						break;
					}
					if (c == '$') numBadChars++;		// unicode characters replaced with '$' in FString serializer
				}
				if (goodName && numBadChars)
				{
					int nameLen = nameStr.Len();
					if (nameLen >= 64) goodName = false;
					if (numBadChars >= nameLen / 2 && nameLen > 16) goodName = false;
				}
				if (!goodName)
				{
					// replace name
					appPrintf("WARNING: %s: fixing name %d (%s)\n", *GetFilename(), i, *nameStr);
					char buf[64];
					appSprintf(ARRAY_ARG(buf), "__name_%d__", i);
					nameStr = buf;
				}
			}

			// remember the name
	#if 0
			NameTable[i] = new char[name.Num()];
			strcpy(NameTable[i], *name);
	#else
			NameTable[i] = appStrdupPool(*nameStr);
	#endif

	#if UNREAL4
			if (Game >= GAME_UE4_BASE)
			{
		#if GEARS4 || DAYSGONE
				if (Game == GAME_Gears4 || Game == GAME_DaysGone) goto name_hashes;
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
					TrashLen = nameStr.Len() ^ 7;
				}
				else
				{
					TrashLen = nameStr.Len() ^ 6;
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
		Imp->Serialize(*this);
#if DEBUG_PACKAGE
		PKG_LOG("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName);
#endif
	}

	unguard;
}

// Game-specific de-obfuscation of export tables
void PatchBnSExports(FObjectExport *Exp, const FPackageFileSummary &Summary);
void PatchDunDefExports(FObjectExport *Exp, const FPackageFileSummary &Summary);

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
	}

#if BLADENSOUL
	if (Game == GAME_BladeNSoul && (Summary.PackageFlags & 0x08000000))
		PatchBnSExports(ExportTable, Summary);
#endif
#if DUNDEF
	if (Game == GAME_DunDef)
		PatchDunDefExports(ExportTable, Summary);
#endif

#if DEBUG_PACKAGE
	Exp = ExportTable;
	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
//		USE_COMPACT_PACKAGE_STRUCTS - makes impossible to dump full information
//		Perhaps add full support to extract.exe?
//		PKG_LOG("Export[%d]: %s'%s' offs=%08X size=%08X parent=%d flags=%08X:%08X, exp_f=%08X arch=%d\n", i, GetObjectName(Exp->ClassIndex),
//			*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->PackageIndex, Exp->ObjectFlags2, Exp->ObjectFlags, Exp->ExportFlags, Exp->Archetype);
		PKG_LOG("Export[%d]: %s'%s' offs=%08X size=%08X parent=%d  exp_f=%08X\n", i, GetObjectName(Exp->ClassIndex),
			*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->PackageIndex, Exp->ExportFlags);
	}
#endif // DEBUG_PACKAGE

	unguard;
}


UnPackage::~UnPackage()
{
	guard(UnPackage::~UnPackage);

	// Remove self from package table (it will be there even if package is not "valid")
	int i = PackageMap.FindItem(this);
	if (i != INDEX_NONE)
	{
		// Could be INDEX_NONE in a case of bad package
		PackageMap.RemoveAt(i);
	}
	// unlink package from CGameFileInfo
	if (FileInfo)
	{
		assert(FileInfo->Package == this || FileInfo->Package == NULL);
		const_cast<CGameFileInfo*>(FileInfo)->Package = NULL;
	}

	if (!IsValid())
	{
		// The package wasn't loaded, nothing to release in destructor. Also it is possible that
		// it has zero names/imports/exports (happens with some UE3 games).
		return;
	}

	// free tables
	if (Loader) delete Loader;
	delete NameTable;
	delete ImportTable;
	delete ExportTable;
#if UNREAL3
	if (DependsTable) delete DependsTable;
#endif

	unguard;
}

/*static*/ void UnPackage::UnloadPackage(UnPackage* package)
{
	if (package)
	{
		delete package;
	}
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

static TArray<UnPackage*> OpenReaders;

void UnPackage::SetupReader(int ExportIndex)
{
	guard(UnPackage::SetupReader);
	// open loader if it is closed
	if (!IsOpen())
	{
		Open();
		if (OpenReaders.Num() == 0)
		{
			OpenReaders.Empty(64);
		}
		OpenReaders.AddUnique(this);
	}
	// setup for object
	const FObjectExport &Exp = GetExport(ExportIndex);
	SetStopper(Exp.SerialOffset + Exp.SerialSize);
//	appPrintf("Setup for %s: %d + %d -> %d\n", *Exp.ObjectName, Exp.SerialOffset, Exp.SerialSize, Exp.SerialOffset + Exp.SerialSize);
	Seek(Exp.SerialOffset);
	unguard;
}

void UnPackage::CloseReader()
{
	guard(UnPackage::CloseReader);
#if 0
	FFileArchive* File = FindFileArchive(Loader);
	assert(File);
	if (File->IsOpen()) File->Close();
#else
	Loader->Close();
#endif
	unguardf("pkg=%s", *GetFilename());
}

void UnPackage::CloseAllReaders()
{
	guard(UnPackage::CloseAllReaders);

	for (UnPackage* p : OpenReaders)
	{
		p->CloseReader();
	}
	OpenReaders.Empty();

	unguard;
}


/*-----------------------------------------------------------------------------
	UObject* and FName serializers
-----------------------------------------------------------------------------*/

FArchive& UnPackage::operator<<(FName &N)
{
	guard(UnPackage::SerializeFName);

	assert(IsLoading);

	// Declare aliases for FName.Index and ExtraIndex to allow USE_COMPACT_PACKAGE_STRUCTS to work
#if !USE_COMPACT_PACKAGE_STRUCTS
	int32& N_Index = N.Index;
	#if UNREAL3 || UNREAL4
	int32& N_ExtraIndex = N.ExtraIndex;
	#endif
#else
	int32 N_Index = 0, N_ExtraIndex = 0;
#endif // USE_COMPACT_PACKAGE_STRUCTS

#if BIOSHOCK
	if (Game == GAME_Bioshock)
	{
		*this << AR_INDEX(N_Index) << N_ExtraIndex;
		if (N_ExtraIndex == 0)
		{
			N.Str = GetName(N_Index);
		}
		else
		{
			N.Str = appStrdupPool(va("%s%d", GetName(N_Index), N_ExtraIndex-1));	// without "_" char
		}
		return *this;
	}
#endif // BIOSHOCK

#if UC2
	if (Engine() == GAME_UE2X && ArVer >= 145)
	{
		*this << N_Index;
	}
	else
#endif // UC2
#if LEAD
	if (Game == GAME_SplinterCellConv && ArVer >= 64)
	{
		*this << N_Index;
	}
	else
#endif // LEAD
#if UNREAL3 || UNREAL4
	if (Engine() >= GAME_UE3)
	{
		*this << N_Index;
		if (Game >= GAME_UE4_BASE) goto extra_index;
	#if R6VEGAS
		if (Game == GAME_R6Vegas2)
		{
			N_ExtraIndex = N_Index >> 19;
			N_Index &= 0x7FFFF;
		}
	#endif // R6VEGAS
		if (ArVer >= 343)
		{
		extra_index:
			*this << N_ExtraIndex;
		}
	}
	else
#endif // UNREAL3 || UNREAL4
	{
		// UE1 and UE2
		*this << AR_INDEX(N_Index);
	}

	// Convert name index to string
#if UNREAL3 || UNREAL4
	if (N_ExtraIndex == 0)
	{
		N.Str = GetName(N_Index);
	}
	else
	{
		N.Str = appStrdupPool(va("%s_%d", GetName(N_Index), N_ExtraIndex-1));
	}
#else
	// no modern engines compiled
	N.Str = GetName(N_Index);
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
	guard(UnPackage::FindExport);

	// Do the first pass with comparing string pointers instead of using stricmp. We're using
	// global name pool for all name tables, so this should work in most cases.
	FastNameComparer cmp(name);
	for (int i = firstIndex; i < Summary.ExportCount; i++)
	{
		const FObjectExport &Exp = ExportTable[i];
		// compare object name
		if (cmp(Exp.ObjectName))
		{
			// if class name specified - compare it too
			const char *foundClassName = GetObjectName(Exp.ClassIndex);
			if (className && (foundClassName != className)) // pointer comparison again
				continue;
			return i;
		}
	}
	return INDEX_NONE;

	unguard;
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
#if UNREAL4
		if (Game >= GAME_UE4_BASE)
		{
			// UE4 usually has single object in package. Plus, each object import has a parent UPackage
			// describing where to get an object, but object export has no parent UObject - so depth
			// of import path is 1 step deeper than depth of export path, and CompareObjectPaths()
			// will always fail.
			return ObjIndex;
		}
#endif // UNREAL4
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

	// Get previously created object if any
	FObjectExport& Exp = GetExport(index);
	if (Exp.Object)
		return Exp.Object;


	// Check if this object just contains default properties
	bool shouldSkipObject = false;

	if (!strnicmp(Exp.ObjectName, "Default__", 9))
	{
		// Default properties are not supported -- this is a clean UObject format
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

	// Create empty object of desired class
	const char* ClassName = GetObjectName(Exp.ClassIndex);
	UObject* Obj = Exp.Object = CreateClass(ClassName);
	if (!Obj)
	{
		if (!IsSuppressedClass(ClassName))
		{
			appPrintf("WARNING: Unknown class \"%s\" for object \"%s\"\n", ClassName, *Exp.ObjectName);
		}
#if MAX_DEBUG
		else
		{
			appPrintf("SUPPRESSED: %s\n", ClassName);
		}
#endif
		return NULL;
	}

#if UNREAL3
	// For UE3 we may require finding object in another package
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

	// Setup constant object fields
	Obj->Package      = this;
	Obj->PackageIndex = index;
	Obj->Outer        = NULL;
	Obj->Name         = Exp.ObjectName;

	bool bLoad = true;
	if (GBeforeLoadObjectCallback)
		bLoad = GBeforeLoadObjectCallback(Obj);

	if (bLoad)
	{
		// Block UObject serialization
		UObject::BeginLoad();

		// Find and try to create outer object
		UObject* Outer = NULL;
		if (Exp.PackageIndex)
		{
			const FObjectExport &OuterExp = GetExport(Exp.PackageIndex - 1);
			Outer = OuterExp.Object;
			if (!Outer)
			{
				const char* OuterClassName = GetObjectName(OuterExp.ClassIndex);
				if (IsKnownClass(OuterClassName))			// avoid error message if class name is not registered
					Outer = CreateExport(Exp.PackageIndex - 1);
			}
		}
		Obj->Outer = Outer;

		// Add object to GObjLoaded for later serialization
		UObject::GObjLoaded.Add(Obj);

		// Perform serialization
		UObject::EndLoad();
	}

	return Obj;

	unguardf("%s:%d", *GetFilename(), index);
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
				{
					if (i > 32)
					{
						// This is definitely a startup package, reorder PackageMap to pick it up faster next time
						PackageMap.RemoveAtSwap(i);
						PackageMap.Insert(Package, 0);
					}
					break;			// found
				}
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
		Imp.Missing = true;
#if UNREAL4
		if (!strnicmp(PackageName, "/Script/", 8) || !strnicmp(PackageName, "/Engine/", 8))
		{
			// Ignore missing engine packages for UE4
			return NULL;
		}
#endif
		appPrintf("WARNING: Import(%s'%s'): package %s was not found\n", *Imp.ClassName, *Imp.ObjectName, PackageName);
		return NULL;
	}

	// create object
	return Package->CreateExport(ObjIndex);

	unguardf("%s:%d", *GetFilename(), index);
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

/*static*/ UnPackage *UnPackage::LoadPackage(const char *Name, bool silent)
{
	guard(UnPackage::LoadPackage(name));

	const char *LocalName = appSkipRootDir(Name);

	// Call CGameFileInfo::Find() first. This function is fast because it uses
	// hashing internally.
	const CGameFileInfo *info = CGameFileInfo::Find(LocalName);

	int i;

	if (info)
	{
		return LoadPackage(info, silent);
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
			if (!stricmp(LocalName, *PackageMap[i]->GetFilename()))
				return PackageMap[i];

		// Try to load package using file name.
		if (appFileExists(Name))
		{
			UnPackage* package = new UnPackage(Name, NULL, silent);
			if (!package->IsValid())
			{
				delete package;
				return NULL;
			}
			return package;
		}
		MissingPackages.Add(appStrdup(LocalName));
	}

	// The package is missing. Do not print any warnings: missing package is a normal situation
	// in UE3 cooked builds, so print warnings when needed at upper level.
//	appPrintf("WARNING: package %s was not found\n", Name);
	return NULL;

	unguardf("%s", Name);
}

/*static*/ UnPackage *UnPackage::LoadPackage(const CGameFileInfo* File, bool silent)
{
	guard(UnPackage::LoadPackage(info));
	PROFILE_LABEL(*File->GetRelativeName());

	if (File->IsPackage)
	{
		// Check if package was already loaded.
		if (File->Package)
			return File->Package;
		// Load the package with providing 'File' to constructor.
		UnPackage* package = new UnPackage(*File->GetRelativeName(), File, silent);
		if (!package->IsValid())
		{
			delete package;
			return NULL;
		}
		// Cache pointer in CGameFileInfo so next time it will be found quickly.
		const_cast<CGameFileInfo*>(File)->Package = package;
		return package;
	}
	return NULL;

	unguardf("%s", *File->GetRelativeName());
}
