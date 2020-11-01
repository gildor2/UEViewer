#include "Core.h"
#include "UnCore.h"
#include "UnPackage.h"

#if UNREAL4

#include "UE4Version.h"

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

void UnPackage::LoadNameTable4()
{
	guard(UnPackage::LoadNameTable4);

	FStaticString<MAX_FNAME_LEN> nameStr;

	// Process version outside of the loop
	bool bHasNameHashes = (ArVer >= VER_UE4_NAME_HASHES_SERIALIZED);
#if GEARS4 || DAYSGONE
	if (Game == GAME_Gears4 || Game == GAME_DaysGone) bHasNameHashes = true;
#endif

	for (int i = 0; i < Summary.NameCount; i++)
	{
		guard(Name);

#if 1
		*this << nameStr;

		// Paragon has many names ended with '\n', so it's good idea to trim spaces
		nameStr.TrimStartAndEndInline();

		// Remember the name
		NameTable[i] = appStrdupPool(*nameStr);
#else
		char buf[MAX_FNAME_LEN];
		int32 Len;
		*this << Len;
		if (Len > 0)
		{
			this->Serialize(buf, Len);
		}
		else
		{
			// Unicode name: just make a dummy one and seek
			appSprintf(ARRAY_ARG(buf), "unicode_%d", i);
			this->Seek(this->Tell() - Len * 2);
		}
		NameTable[i] = appStrdupPool(buf);
#endif

		if (bHasNameHashes)
		{
#if 1
			this->Seek(this->Tell() + 4); // works faster
#else
			int16 NonCasePreservingHash, CasePreservingHash;
			*this << NonCasePreservingHash << CasePreservingHash;
#endif
		}

#if DEBUG_PACKAGE
		PKG_LOG("Name[%d]: \"%s\"\n", i, NameTable[i]);
#endif
		unguardf("%d", i);
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	IO Store AsyncPackage
-----------------------------------------------------------------------------*/

struct FMappedName
{
	uint32 NameIndex;
	uint32 ExtraIndex;
};

struct FPackageSummary
{
	FMappedName Name;
	FMappedName SourceName;
	uint32 PackageFlags;
	uint32 CookedHeaderSize;
	int32 NameMapNamesOffset;
	int32 NameMapNamesSize;
	int32 NameMapHashesOffset;
	int32 NameMapHashesSize;
	int32 ImportMapOffset;
	int32 ExportMapOffset;
	int32 ExportBundlesOffset;
	int32 GraphDataOffset;
	int32 GraphDataSize;
	int32 Pad;
};

void UnPackage::LoadPackageIoStore()
{
	guard(UnPackage::LoadPackageIoStore);

	FPackageSummary Sum;
	Loader->Serialize(&Sum, sizeof(Sum));

	// Rewind Loader to zero and load whole header (including summary)
	int HeaderSize = Sum.GraphDataOffset + Sum.GraphDataSize;
	byte* HeaderData = new byte[HeaderSize];
	Loader->Seek(0);
	Loader->Serialize(HeaderData, HeaderSize);

	FMemReader Reader(HeaderData, HeaderSize);
	Reader.SetupFrom(*Loader);

	// Load name table
	// Sum.NameMapHashesOffset points to 'uint64 HashVersion' followed by hashes.
	// Current HashVersion = 0xC1640000.
	int NameCount = Sum.NameMapHashesSize / sizeof(uint64) - 1;
//	Reader.Seek(Sum.NameMapNamesOffset);
	LoadNameTableIoStore(HeaderData + Sum.NameMapNamesOffset, NameCount, Sum.NameMapNamesSize);

	delete[] HeaderData;

	unguard;
}

static void SerializeFNameSerializedView(const byte*& Data, FString& Str)
{
	// FSerializedNameHeader
	int Len = ((Data[0] & 0x7F) << 8) | Data[1];
	bool isUnicode = (Data[0] & 0x80) != 0;
	assert(!isUnicode);
	Data += 2;

	Str.GetDataArray().SetNumUninitialized(Len + 1);
	memcpy(&Str[0], Data, Len);
	Str[Len] = 0;
	Data += Len;
}

void UnPackage::LoadNameTableIoStore(const byte* Data, int NameCount, int TableSize)
{
	guard(UnPackage::LoadNameTableIoStore);

	NameTable = new const char* [NameCount];

	const byte* EndPosition = Data + TableSize;
	for (int i = 0; i < NameCount; i++)
	{
		FStaticString<MAX_FNAME_LEN> NameStr;
		SerializeFNameSerializedView(Data, NameStr);
		NameTable[i] = appStrdupPool(*NameStr);
	}
	assert(Data == EndPosition);

	unguard;
}

#endif // UNREAL4
