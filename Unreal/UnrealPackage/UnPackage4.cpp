#include "Core.h"

#if UNREAL4

#include "UnCore.h"
#include "UnPackage.h"
#include "UE4Version.h"

#include "FileSystem/GameFileSystem.h"		// just required for next header
#include "FileSystem/IOStoreFileSystem.h"	// for FindPackageById()

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

#if VALORANT
	if (Ar.Game == GAME_Valorant) Ar.Seek(Ar.Tell()+8); // no idea what these bytes are used for
#endif // VALORANT

	// guid
	Ar << Guid;

	if (Ar.ContainsEditorData())
	{
		if (Ar.ArVer >= VER_UE4_ADDED_PACKAGE_OWNER)
		{
			FGuid PersistentGuid;
			Ar << PersistentGuid;
		}

		if (Ar.ArVer >= VER_UE4_ADDED_PACKAGE_OWNER && Ar.ArVer < VER_UE4_NON_OUTER_PACKAGE_IMPORT)
		{
			FGuid OwnerPersistentGuid;
			Ar << OwnerPersistentGuid;
		}
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

	const char* ToString(const char** NameTable) const
	{
		if (ExtraIndex == 0)
			return NameTable[NameIndex];
		return appStrdupPool(va("%s_%d", NameTable[NameIndex], ExtraIndex - 1));
	}
};

struct FPackageObjectIndex
{
	uint64 Value;

	FORCEINLINE bool IsNull() const
	{
		return (Value >> 62) == 3; // FPackageObjectIndex::Null, original code check whole value to be -1
	}

	FORCEINLINE bool IsExport() const
	{
		return (Value >> 62) == 0; // FPackageObjectIndex::Export
	}

	FORCEINLINE bool IsScriptImport() const
	{
		return (Value >> 62) == 1;
	}

	FORCEINLINE bool IsImport() const
	{
		return (Value >> 62) == 2; // original function does '|| IsScriptImport()'
	}
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

struct FExportMapEntry
{
	uint64 CookedSerialOffset;
	uint64 CookedSerialSize;
	FMappedName ObjectName;
	FPackageObjectIndex OuterIndex;
	FPackageObjectIndex ClassIndex;
	FPackageObjectIndex SuperIndex;
	FPackageObjectIndex TemplateIndex;
	FPackageObjectIndex GlobalImportIndex;
	uint32 ObjectFlags;	// EObjectFlags
	uint8 FilterFlags;	// EExportFilterFlags: client/server flags
	uint8 Pad[3];
};

struct FExportBundleHeader
{
	uint32 FirstEntryIndex;
	uint32 EntryCount;
};

struct FExportBundleEntry
{
	enum EExportCommandType
	{
		ExportCommandType_Create,
		ExportCommandType_Serialize,
	};
	uint32 LocalExportIndex;
	uint32 CommandType;
};

struct FScriptObjectEntry
{
	FMappedName ObjectName; // FMinimalName
	FPackageObjectIndex GlobalIndex;
	FPackageObjectIndex OuterIndex;
	FPackageObjectIndex CDOClassIndex;
};

struct CImportTableErrorStats
{
	uint32 TotalPackages;
	uint32 MissedPackages;
	uint32 MissedImports;

	CImportTableErrorStats()
	: TotalPackages(0)
	, MissedPackages(0)
	, MissedImports(0)
	{}
};

static void LoadGraphData(const byte* Data, int DataSize, TArray<const CGameFileInfo*>& OutPackages, CImportTableErrorStats& Stats)
{
	guard(LoadGraphData);

	const byte* DataEnd = Data + DataSize;
	int32 PackageCount = *(int32*)Data;
	if (PackageCount == 0) return;
	Stats.TotalPackages += PackageCount;

	Data += sizeof(int32);
	OutPackages.Reserve(OutPackages.Num() + PackageCount);

	for (int PackageIndex = 0; PackageIndex < PackageCount; PackageIndex++)
	{
		FPackageId PackageId = *(FPackageId*)Data;
		Data += sizeof(FPackageId);
		int32 BundleCount = *(int32*)Data;
		Data += sizeof(int32);
		// Skip bundle information
		Data += BundleCount * (sizeof(int32) + sizeof(int32));

		// Find the package
		const CGameFileInfo* File = FindPackageById(PackageId);
		if (File)
		{
			OutPackages.Add(File);
		#if DEBUG_PACKAGE
			appPrintf("Depends: %s\n", *File->GetRelativeName());
		#endif
		}
		else
		{
		#if DEBUG_PACKAGE
			appPrintf("Can't locate package with Id %llX\n", PackageId);
		#endif
			Stats.MissedPackages++;
		}
	}

	assert(Data == DataEnd);

	unguard;
}

// Reference: AsyncLoading2.cpp, FAsyncPackage2::Event_ProcessPackageSummary()
void UnPackage::LoadPackageIoStore()
{
	guard(UnPackage::LoadPackageIoStore);

#if DEBUG_PACKAGE
	appPrintf("Loading AsyncPackage %s\n", *GetFilename());
#endif

	FPackageSummary Sum;
	Loader->Serialize(&Sum, sizeof(Sum));

	Summary.PackageFlags = Sum.PackageFlags;

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
	LoadNameTableIoStore(HeaderData + Sum.NameMapNamesOffset, NameCount, Sum.NameMapNamesSize);

	// Process export bundles

	// Compute number of export bundle headers, so we could locate entries. In UE4, this information
	// is stored in global package store, we're not loading it. The idea: compute number of bundle entries
	// which could fit the buffer. The start iterating data from the start, with processing bundle headers.
	// When we'll get the number of bundle entries accumulated from headers matching the size of remaining
	// buffer, stop.
	const FExportBundleHeader* BundleHeaders = (FExportBundleHeader*)(HeaderData + Sum.ExportBundlesOffset);
	int RemainingBundleEntryCount = (Sum.GraphDataOffset - Sum.ExportBundlesOffset) / sizeof(FExportBundleEntry);
	int FoundBundlesCount = 0;
	const FExportBundleHeader* CurrentBundleHeader = BundleHeaders;
	while (FoundBundlesCount < RemainingBundleEntryCount)
	{
		// This location is occupied by header, so it is not a bundle entry
		RemainingBundleEntryCount--;
		FoundBundlesCount += CurrentBundleHeader->EntryCount;
		CurrentBundleHeader++;
	}
	assert(FoundBundlesCount == RemainingBundleEntryCount);
	// Load export bundles into arrays
	TArray<FExportBundleHeader> BundleHeadersArray;
	TArray<FExportBundleEntry> BundleEntriesArray;
	CopyArrayView(BundleHeadersArray, BundleHeaders, CurrentBundleHeader - BundleHeaders);
	CopyArrayView(BundleEntriesArray, (FExportBundleEntry*)CurrentBundleHeader, FoundBundlesCount);

	// Load export table
	// UE4 doesn't compute export count, it uses it from global package store
	int ExportTableSize = Sum.ExportBundlesOffset - Sum.ExportMapOffset;
	assert(ExportTableSize % sizeof(FExportMapEntry) == 0);
	int ExportCount = ExportTableSize / sizeof(FExportMapEntry);
	LoadExportTableIoStore(HeaderData + Sum.ExportMapOffset, ExportCount, ExportTableSize, HeaderSize, BundleHeadersArray, BundleEntriesArray);

	// Load import table
	// Should scan graph first to get list of packages we depends on
	TStaticArray<const CGameFileInfo*, 32> ImportPackages;
	CImportTableErrorStats ErrorStats;
	LoadGraphData(HeaderData + Sum.GraphDataOffset, Sum.GraphDataSize, ImportPackages, ErrorStats);
	int ImportMapSize = Sum.ExportMapOffset - Sum.ImportMapOffset;
	int ImportCount = ImportMapSize / sizeof(FPackageObjectIndex);
	LoadImportTableIoStore(HeaderData + Sum.ImportMapOffset, ImportCount, ImportPackages, ErrorStats);

	// All headers were processed, delete the buffer
	delete[] HeaderData;

	if (ErrorStats.MissedPackages && ErrorStats.MissedImports)
	{
		appPrintf("%s: missed %d imports from %d/%d unknown packages\n",
			*GetFilename(),
			ErrorStats.MissedImports, ErrorStats.MissedPackages, ErrorStats.TotalPackages);
	}

	// Replace loader, so we'll be able to adjust offset in UnPackage::SetupReader()
	FArchive* NewLoader = new FReaderWrapper(Loader, 0);
	NewLoader->SetupFrom(*this);
	Loader = NewLoader;

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

// Reference: UnrealNames.cpp, LoadNameBatch()
void UnPackage::LoadNameTableIoStore(const byte* Data, int NameCount, int TableSize)
{
	guard(UnPackage::LoadNameTableIoStore);

	Summary.NameCount = NameCount;
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

// Forward
const char* FindScriptEntryName(const FPackageObjectIndex& ObjectIndex);

void UnPackage::LoadExportTableIoStore(const byte* Data, int ExportCount, int TableSize, int PackageHeaderSize,
	const TArray<FExportBundleHeader>& BundleHeaders, const TArray<FExportBundleEntry>& BundleEntries)
{
	guard(UnPackage::LoadExportTableIoStore);

	Summary.ExportCount = ExportCount;
	ExportTable = new FObjectExport[ExportCount];
	memset(ExportTable, 0, sizeof(FObjectExport) * ExportCount);

	ExportIndices_IOS = new FPackageObjectIndex[ExportCount];

	const FExportMapEntry* ExportEntries = (FExportMapEntry*)Data;

	// Export data is ordered according to export bundles, so we should do the processing in bundle order
	int32 CurrentExportOffset = PackageHeaderSize;
	for (const FExportBundleHeader& BundleHeader : BundleHeaders)
	{
		for (int EntryIndex = 0; EntryIndex < BundleHeader.EntryCount; EntryIndex++)
		{
			const FExportBundleEntry& Entry = BundleEntries[BundleHeader.FirstEntryIndex + EntryIndex];
			if (Entry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				unsigned ObjectIndex = Entry.LocalExportIndex;
				assert(ObjectIndex < ExportCount);

				const FExportMapEntry& E = ExportEntries[ObjectIndex];
				FObjectExport& Exp = ExportTable[ObjectIndex];

				// TODO: FExportMapEntry has FilterFlags which could affect inclusion of exports
				assert(E.CookedSerialOffset < 0x7FFFFFFF);
				assert(E.CookedSerialSize < 0x7FFFFFFF);
				Exp.SerialOffset = (int32)E.CookedSerialOffset;
				Exp.SerialSize = (int32)E.CookedSerialSize;
				Exp.ObjectName.Str = E.ObjectName.ToString(NameTable);
				Exp.ClassName_IO = FindScriptEntryName(E.ClassIndex);
				// Store "real" offset
				Exp.RealSerialOffset = CurrentExportOffset;
				CurrentExportOffset += Exp.SerialSize;
				// OuterIndex is just a position in current export table, pointing at outer object
				int64 OuterIndex = int64(E.OuterIndex.Value + 1);
				assert(OuterIndex >= 0 && OuterIndex <= ExportCount);
				Exp.PackageIndex = int(OuterIndex);

				// Save GlobalImportIndex in separate array
				ExportIndices_IOS[ObjectIndex] = E.GlobalImportIndex;
			#if DEBUG_PACKAGE
				appPrintf("Exp[%d]: %s'%s' Flags=%X SerialOff=%X RealOff=%X Id=%I64X Parent=%I64d\n", ObjectIndex, Exp.ClassName_IO, *Exp.ObjectName,
					E.ObjectFlags, Exp.SerialOffset, Exp.RealSerialOffset,
					E.GlobalImportIndex.Value, E.OuterIndex.Value);
			#endif
			}
		}
	}

	unguard;
}

struct ImportHelper
{
	const TArray<const CGameFileInfo*>& PackageFiles;
	TStaticArray<UnPackage*, 32> Packages;
	TStaticArray<int, 32> AllocatedPackageImports;
	FObjectImport* ImportTable;
	const FPackageObjectIndex* ImportMap;
	int ImportCount;
	int NextImportToCheck;

	ImportHelper(const TArray<const CGameFileInfo*>& InPackageFiles, FObjectImport* InImportTable, const FPackageObjectIndex* InImportMap, int InImportCount)
	: PackageFiles(InPackageFiles)
	, ImportTable(InImportTable)
	, ImportMap(InImportMap)
	, ImportCount(InImportCount)
	, NextImportToCheck(0)
	{
		int NumPackages = PackageFiles.Num();
		Packages.Reserve(NumPackages);
		AllocatedPackageImports.Init(-1, NumPackages);
		// Preload dependencies
		for (const CGameFileInfo* File : PackageFiles)
		{
			Packages.Add(UnPackage::LoadPackage(File, true));
		}
	}

	// Find or create FObjectImport for package specified by its index
	int GetPackageImportIndex(int PackageIndex)
	{
		int Index = AllocatedPackageImports[PackageIndex];
		if (Index >= 0)
		{
			// Already allocated
			return Index;
		}
		Index = FindNullImportEntry();
		AllocatedPackageImports[PackageIndex] = Index;

		const CGameFileInfo* PackageFile = PackageFiles[PackageIndex];
		FObjectImport& Imp = ImportTable[Index];
		// Get the package file name and cut extension
		FStaticString<MAX_PACKAGE_PATH> PackageName;
		PackageFile->GetRelativeName(PackageName);
		char* Ext = strrchr(&PackageName[0], '.');
		if (Ext) *Ext = 0;
		Imp.ObjectName.Str = appStrdupPool(*PackageName); // it should be already in pool
		Imp.ClassName.Str = "Package";

		return Index;
	}

	bool FindObjectInPackages(FPackageObjectIndex ObjectIndex, int& OutPackageIndex, const FObjectExport*& OutExportEntry) const
	{
		for (int PackageIndex = 0; PackageIndex < Packages.Num(); PackageIndex++)
		{
			const UnPackage* ImportPackage = Packages[PackageIndex];
			assert(ImportPackage);
			for (int i = 0; i < ImportPackage->Summary.ExportCount; i++)
			{
				if (ImportPackage->ExportIndices_IOS[i].Value == ObjectIndex.Value)
				{
					// Found
					OutPackageIndex = PackageIndex;
					OutExportEntry = &ImportPackage->ExportTable[i];
					return true;
				}
			}
		}
		return false;
	}

protected:
	// Find unused import map entry - there are always exists because IsStore generator
	// is making import map with same entries as in original cooked files, just replacing
	// non-object entries with Null.
	int FindNullImportEntry()
	{
		guard(ImportHelper::FindNullImportEntry);
		for (int Index = NextImportToCheck; Index < ImportCount; Index++)
		{
			if (ImportMap[Index].IsNull())
			{
				NextImportToCheck = Index + 1;
				return Index;
			}
		}
		appError("Unable to find Null import entry");
		unguard;
	}
};

void UnPackage::LoadImportTableIoStore(const byte* Data, int ImportCount, const TArray<const CGameFileInfo*>& ImportPackages, CImportTableErrorStats& ErrorStats)
{
	guard(UnPackage::LoadImportTableIoStore);

	// Import table is filled with object Ids, we should scan all dependencies to resolve them
	ImportTable = new FObjectImport[ImportCount];
	Summary.ImportCount = ImportCount;
	const FPackageObjectIndex* DataPtr = (FPackageObjectIndex*)Data;

	ImportHelper Helper(ImportPackages, ImportTable, DataPtr, ImportCount);

	for (int ImportIndex = 0; ImportIndex < ImportCount; ImportIndex++)
	{
		const FPackageObjectIndex& ObjectIndex = DataPtr[ImportIndex];

		FObjectImport& Imp = ImportTable[ImportIndex];
		if (ObjectIndex.IsNull())
			continue;

		if (ObjectIndex.IsScriptImport())
		{
			const char* Name = FindScriptEntryName(ObjectIndex);
			Imp.ObjectName.Str = Name;
			Imp.ClassName.Str = Name[0] == '/' ? "Package" : "Class";
			Imp.PackageIndex = 0;
		}
		else
		{
			assert(ObjectIndex.IsImport());
			const FObjectExport* Exp = NULL;
			int PackageIndex = 0;
			if (Helper.FindObjectInPackages(ObjectIndex, PackageIndex, Exp))
			{
#if DEBUG_PACKAGE
				appPrintf("Imp[%d]: %s %s\n", ImportIndex, *Exp->ObjectName, Exp->ClassName_IO);
#endif
				Imp.ObjectName.Str = *Exp->ObjectName;
				Imp.ClassName.Str = Exp->ClassName_IO;
				Imp.PackageIndex = - Helper.GetPackageImportIndex(PackageIndex) - 1;
			}
			else
			{
#if DEBUG_PACKAGE
				appPrintf("Unable to resolve import %llX\n", ObjectIndex);
#endif
				ErrorStats.MissedImports++;
			}
		}
	}

	unguard;
}

struct ObjectIndexHashEntry
{
	ObjectIndexHashEntry* Next;
	const char* Name;
	FPackageObjectIndex ObjectIndex;
};

#define OBJECT_HASH_BITS	12
#define OBJECT_HASH_MASK	((1 << OBJECT_HASH_BITS)-1)

static ObjectIndexHashEntry* ObjectHashHeads[1 << OBJECT_HASH_BITS];
static ObjectIndexHashEntry* ObjectHashStore = NULL;

FORCEINLINE int ObjectIndexToHash(const FPackageObjectIndex& ObjectIndex)
{
	return uint32(ObjectIndex.Value) >> (32 - OBJECT_HASH_BITS);
}

const char* FindScriptEntryName(const FPackageObjectIndex& ObjectIndex)
{
	int Hash = ObjectIndexToHash(ObjectIndex);
	for (const ObjectIndexHashEntry* Entry = ObjectHashHeads[Hash]; Entry; Entry = Entry->Next)
	{
		if (Entry->ObjectIndex.Value == ObjectIndex.Value)
		{
			return Entry->Name;
		}
	}
	return "None";
}

/*static*/ void UnPackage::LoadGlobalData4(FArchive* NameAr, FArchive* MetaAr, int NameCount)
{
	guard(UnPackage::LoadGlobalData4);

	// Load EIoChunkType::LoaderGlobalNames chunk
	// Similar to UnPackage::LoadNameTableIoStore
	int NameTableSize = NameAr->GetFileSize();
	byte* NameBuffer = new byte[NameTableSize];
	NameAr->Serialize(NameBuffer, NameTableSize);
	const char** GlobalNameTable = new const char* [NameCount];

	const byte* Data = NameBuffer;
	const byte* EndPosition = Data + NameTableSize;
	for (int i = 0; i < NameCount; i++)
	{
		FStaticString<MAX_FNAME_LEN> NameStr;
		SerializeFNameSerializedView(Data, NameStr);
		GlobalNameTable[i] = appStrdupPool(*NameStr);
	}
	assert(Data == EndPosition);
	delete NameBuffer;

	// Load EIoChunkType::LoaderInitialLoadMeta chunk, it contains "script" objects,
	// what means to us - names of built-in engine classes.
	int MetaBufferSize = MetaAr->GetFileSize();
	byte* MetaBuffer = new byte[MetaBufferSize];
	MetaAr->Serialize(MetaBuffer, MetaBufferSize);

	TArray<FScriptObjectEntry> ScriptObjects;
	// First 4 bytes are object count
	uint32 NumObjects = *(uint32*)MetaBuffer;
	CopyArrayView(ScriptObjects, MetaBuffer + 4, (MetaBufferSize - 4) / sizeof(FScriptObjectEntry));
	assert(NumObjects == ScriptObjects.Num());
	delete MetaBuffer;

	// Build GlobalIndex lookup hash and index to name mapping
	ObjectHashStore = new ObjectIndexHashEntry[NumObjects];
	memset(ObjectHashHeads, 0, sizeof(ObjectHashHeads));

	for (int i = 0; i < NumObjects; i++)
	{
		const FScriptObjectEntry& E = ScriptObjects[i];

		// Didn't find the code which maps FMinimalName to name table index, but the following
		// trick works fine:
		int NameIndex = (E.ObjectName.NameIndex) & 0x7fffffff;
		const char* ScriptName = GlobalNameTable[NameIndex];
	#if DEBUG_PACKAGE && 0
		appPrintf("Script: %s - [%X %X] [%X %X]\n", ScriptName,
			uint32(E.GlobalIndex.Value >> 62), uint32(E.GlobalIndex.Value & 0xFFFFFFFF),
			uint32(E.OuterIndex.Value >> 62), uint32(E.OuterIndex.Value & 0xFFFFFFFF));
		// Example output:
		// /Script/Engine - [1 DC7C0922] [3 FFFFFFFF] <- first [] used as FScriptObjectEntry::OuterIndex
		// SkeletalMesh - [1 CFCE1861] [1 DC7C0922] <- first [] referenced by FExportMapEntry::ClassIndex
	#endif

		ObjectIndexHashEntry& Entry = ObjectHashStore[i];
		Entry.Name = ScriptName;
		Entry.ObjectIndex = E.GlobalIndex;

		int Hash = ObjectIndexToHash(E.GlobalIndex);
		Entry.Next = ObjectHashHeads[Hash];
		ObjectHashHeads[Hash] = &Entry;
	}

	delete GlobalNameTable;

	unguard;
}

#endif // UNREAL4
