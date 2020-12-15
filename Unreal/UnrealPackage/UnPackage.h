#ifndef __UNPACKAGE_H__
#define __UNPACKAGE_H__


#define MAX_FNAME_LEN			1024	// UE4: NAME_SIZE in NameTypes.h
//#define DEBUG_PACKAGE			1

#if 1
#	define PKG_LOG(...)		appPrintf(__VA_ARGS__)
#else
#	define PKG_LOG(...)
#endif


#if UNREAL4
// Callback called when unversioned package was found.
int UE4UnversionedPackage(int verMin, int verMax);
#endif


struct FGenerationInfo
{
	int32		ExportCount, NameCount;
#if UNREAL3
	int32		NetObjectCount;
#endif

	friend FArchive& operator<<(FArchive &Ar, FGenerationInfo &I)
	{
		Ar << I.ExportCount << I.NameCount;
#if UNREAL4
		if (Ar.Game >= GAME_UE4_BASE && Ar.ArVer >= 196 /*VER_UE4_REMOVE_NET_INDEX*/) return Ar;
#endif
#if UNREAL3
		if (Ar.ArVer >= 322) // PACKAGE_V3
			Ar << I.NetObjectCount;
#endif
		return Ar;
	}
};


#if UNREAL3

struct FCompressedChunk
{
	int32		UncompressedOffset;
	int32		UncompressedSize;
	int32		CompressedOffset;
	int32		CompressedSize;

	friend FArchive& operator<<(FArchive &Ar, FCompressedChunk &C)
	{
		guard(FCompressedChunk<<);

#if MKVSDC || ROCKET_LEAGUE
		if ((Ar.Game == GAME_MK && Ar.ArVer >= 677) || (Ar.Game == GAME_RocketLeague && Ar.ArLicenseeVer >= 22))
		{
			// MK X and Rocket League has 64-bit file offsets
			int64 UncompressedOffset64, CompressedOffset64;
			Ar << UncompressedOffset64 << C.UncompressedSize << CompressedOffset64 << C.CompressedSize;
			C.UncompressedOffset = (int)UncompressedOffset64;
			C.CompressedOffset   = (int)CompressedOffset64;
			return Ar;
		}
#endif // MKVSDC

		Ar << C.UncompressedOffset << C.UncompressedSize << C.CompressedOffset << C.CompressedSize;

#if BULLETSTORM
		if (Ar.Game == GAME_Bulletstorm && Ar.ArLicenseeVer >= 21)
		{
			int32 unk10;		// unused? could be 0 or 1
			Ar << unk10;
		}
#endif // BULLETSTORM

		return Ar;

		unguard;
	}
};

//RAW_TYPE(FCompressedChunk) -- Bulletstorm has modifications in this structure

#endif // UNREAL3


#if UNREAL4

struct FEngineVersion
{
	uint16		Major, Minor, Patch;
	int32		Changelist;
	FString		Branch;

	friend FArchive& operator<<(FArchive& Ar, FEngineVersion& V)
	{
		return Ar << V.Major << V.Minor << V.Patch << V.Changelist << V.Branch;
	}
};

struct FCustomVersion
{
	FGuid			Key;
	int32			Version;

	friend FArchive& operator<<(FArchive& Ar, FCustomVersion& V)
	{
		return Ar << V.Key << V.Version;
	}
};

struct FCustomVersionContainer
{
	TArray<FCustomVersion> Versions;

	void Serialize(FArchive& Ar, int LegacyVersion);
};

#endif // UNREAL4


struct FPackageFileSummary
{
	uint32		Tag;
#if UNREAL4
	int32		LegacyVersion;
	bool		IsUnversioned;
	FCustomVersionContainer CustomVersionContainer;
#endif
	uint16		FileVersion;
	uint16		LicenseeVersion;
	int32		PackageFlags;
	int32		NameCount,   NameOffset;
	int32		ExportCount, ExportOffset;
	int32		ImportCount, ImportOffset;
	FGuid		Guid;
	TArray<FGenerationInfo> Generations;
#if UNREAL3
	int32		HeadersSize;		// used by UE3 for precaching name table
	FString		PackageGroup;		// "None" or directory name
	int32		DependsOffset;		// number of items = ExportCount
	int32		f38;
	int32		f3C;
	int32		f40;
	int32		EngineVersion;
	int32		CookerVersion;
	int32		CompressionFlags;
	TArray<FCompressedChunk> CompressedChunks;
	int32		U3unk60;
#endif // UNREAL3
#if UNREAL4
	int64		BulkDataStartOffset;
#endif

	FPackageFileSummary()
	{
		memset(this, 0, sizeof(*this));
	}

	bool Serialize(FArchive &Ar);

private:
	// Engine-specific serializers
	void Serialize2(FArchive& Ar);
	void Serialize3(FArchive& Ar);
	void Serialize4(FArchive& Ar);
};

#if UNREAL3
#define EF_ForcedExport		1
// other noticed values: 2
#endif

struct FObjectExport
{
	int32		ClassIndex;					// object reference
	int32		PackageIndex;				// object reference
	FName		ObjectName;
	int32		SerialSize;
	int32		SerialOffset;
	UObject		*Object;					// not serialized, filled by object loader
#if !USE_COMPACT_PACKAGE_STRUCTS
	int32		SuperIndex;					// object reference
	uint32		ObjectFlags;
#endif
#if UNREAL3
	uint32		ExportFlags;				// EF_* flags
	#if !USE_COMPACT_PACKAGE_STRUCTS
	uint32		ObjectFlags2;				// really, 'uint64 ObjectFlags'
	int32		Archetype;
//	TMap<FName, int> ComponentMap;			-- this field was removed from UE3, so serialize it as a temporary variable when needed
	TArray<int32> NetObjectCount;			// generations
	FGuid		Guid;
	int32		PackageFlags;
	int32		U3unk6C;
	int32		TemplateIndex;				// UE4
	#endif // USE_COMPACT_PACKAGE_STRUCTS
#endif // UNREAL3

#if UNREAL4
	// In UE4.26 IoStore package structure is different, 'ClassIndex' is replaced with global
	// script object index.
	const char* ClassName_IO;
	// IoStore has reordered objects, but preserves "CookedSerialOffset" in export table.
	// We need to serialize data from the "read" offset, but set up the loader so it will
	// think that offset is like in original package.
	uint32		RealSerialOffset;
#endif

	friend FArchive& operator<<(FArchive &Ar, FObjectExport &E);

private:
	// Engine-specific serializers
	void Serialize2(FArchive& Ar);
	void Serialize2X(FArchive& Ar);
	void Serialize3(FArchive& Ar);
	void Serialize4(FArchive& Ar);
};


struct FObjectImport
{
#if !USE_COMPACT_PACKAGE_STRUCTS
	FName		ClassPackage;
#endif
	FName		ClassName;
	int32		PackageIndex;
	FName		ObjectName;
	bool		Missing;					// not serialized

	void Serialize(FArchive& Ar);
};


// In Unreal Engine class with similar functionality has name "ULinkerLoad" (and renamed to "FLinkerLoad" in UE4)
class UnPackage : public FArchive
{
	DECLARE_ARCHIVE(UnPackage, FArchive);
protected:
	const char*				FilenameNoInfo;		// full name with path and extension
public:
	const char*				Name;				// short name without extension
	const CGameFileInfo*	FileInfo;
	FArchive*				Loader;

	// Package structures
	FPackageFileSummary		Summary;
	const char**			NameTable;
	FObjectImport*			ImportTable;
	FObjectExport*			ExportTable;
#if UNREAL4
	struct FPackageObjectIndex* ExportIndices_IOS;
#endif

protected:
	UnPackage(const char *filename, const CGameFileInfo* fileInfo = NULL, bool silent = false);
	~UnPackage();

public:
	FString GetFilename() const
	{
		return FileInfo ? FileInfo->GetRelativeName() : FilenameNoInfo;
	}

	// Check if valid package has been created by constructor
	bool IsValid() const { return Summary.NameCount > 0; }

	// Load package using short name (without path and extension) or full path name.
	// When the package is already loaded, this function will simply return a pointer
	// to previously loaded UnPackage.
	static UnPackage* LoadPackage(const char* Name, bool silent = false);
	// Load package using existing CGameFileInfo
	static UnPackage* LoadPackage(const CGameFileInfo* File, bool silent = false);
	// We've protected UnPackage's destructor, however it is possible to use UnloadPackage to fully destroy it.
	// This call is just more noticeable in code than use of 'operator delete'.
	static void UnloadPackage(UnPackage* package);

	FORCEINLINE static void ReservePackageMap(int count)
	{
		PackageMap.Reserve(count);
	}

	static const TArray<UnPackage*>& GetPackageMap()
	{
		return PackageMap;
	}

#if UNREAL4
	// Loaded for global IO Store container data
	static void LoadGlobalData4(FArchive* NameAr, FArchive* MetaAr, int NameCount);
#endif

protected:
	// Create loader FArchive for package
	static FArchive* CreateLoader(const char* filename, FArchive* baseLoader = NULL);
	// Change loader for games with tricky package data
	void ReplaceLoader();

public:
	// Prepare for serialization of particular object. Will open a reader if it was
	// closed before.
	void SetupReader(int ExportIndex);
	// Close reader when not needed anymore. Could be reopened again with SetupReader().
	void CloseReader();

	static void CloseAllReaders();

	const char* GetName(int index)
	{
		if (unsigned(index) >= Summary.NameCount)
			appError("Package \"%s\": wrong name index %d", *GetFilename(), index);
		return NameTable[index];
	}

	FObjectImport& GetImport(int index)
	{
		if (unsigned(index) >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", *GetFilename(), index);
		return ImportTable[index];
	}

	const FObjectImport& GetImport(int index) const // duplicate for 'const' package
	{
		if (unsigned(index) >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", *GetFilename(), index);
		return ImportTable[index];
	}

	FObjectExport& GetExport(int index)
	{
		if (unsigned(index) >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", *GetFilename(), index);
		return ExportTable[index];
	}

	// "const" version of the above function
	const FObjectExport& GetExport(int index) const
	{
		if (unsigned(index) >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", *GetFilename(), index);
		return ExportTable[index];
	}

	const char* GetObjectName(int PackageIndex) const	//?? GetExportClassName()
	{
		guard(UnPackage::GetObjectName);
		if (PackageIndex < 0)
		{
			//?? should point to 'Class' object
			return GetImport(-PackageIndex-1).ObjectName;
		}
		else if (PackageIndex > 0)
		{
			//?? should point to 'Class' object
			return GetExport(PackageIndex-1).ObjectName;
		}
		else // PackageIndex == 0
		{
			return "Class";
		}
		unguardf("Index=%d", PackageIndex);
	}

	const char* GetClassNameFor(const FObjectExport& Exp) const
	{
#if UNREAL4
		// Allow to explicitly provide the class name
		if (Exp.ClassName_IO) return Exp.ClassName_IO;
#endif
		return GetObjectName(Exp.ClassIndex);
	}

	int FindExport(const char *name, const char *className = NULL, int firstIndex = 0) const;
	int FindExportForImport(const char *ObjectName, const char *ClassName, UnPackage *ImporterPackage, int ImporterIndex);
	bool CompareObjectPaths(int PackageIndex, UnPackage *RefPackage, int RefPackageIndex) const;

	UObject* CreateExport(int index);
	UObject* CreateImport(int index);

	const char *GetObjectPackageName(int PackageIndex) const;
	// get object name including all outers (class name is not included)
	void GetFullExportName(const FObjectExport &Exp, char *buf, int bufSize, bool IncludeObjectName = true, bool IncludeCookedPackageName = true) const;
	const char *GetUncookedPackageName(int PackageIndex) const;

	// FArchive interface
	virtual FArchive& operator<<(FName &N);
	virtual FArchive& operator<<(UObject *&Obj);

	virtual bool IsCompressed() const
	{
		return Loader->IsCompressed();
	}
#if UNREAL4
	virtual bool ContainsEditorData() const
	{
		if (Game < GAME_UE4_BASE) return false;
		if (Summary.IsUnversioned) return false;		// unversioned packages definitely has no editor data
		if (Summary.PackageFlags & PKG_FilterEditorOnly) return false;
		return true;
	}
#endif // UNREAL4
	virtual void Serialize(void *data, int size)
	{
		Loader->Serialize(data, size);
	}
	virtual void Seek(int Pos)
	{
		Loader->Seek(Pos);
	}
	virtual int Tell() const
	{
		return Loader->Tell();
	}
	virtual void SetStopper(int Pos)
	{
		Loader->SetStopper(Pos);
	}
	virtual int GetStopper() const
	{
		return Loader->GetStopper();
	}
	virtual int GetFileSize() const
	{
		return Loader->GetFileSize();
	}
	virtual bool IsOpen() const
	{
		return Loader->IsOpen();
	}
	virtual bool Open()
	{
		return Loader->Open();
	}
	virtual void Close()
	{
		Loader->Close();
	}

private:
	void RegisterPackage(const char* filename);
	void UnregisterPackage();

	void LoadNameTable();
	void LoadNameTable2();
	void LoadNameTable3();
	void LoadNameTable4();
	bool VerifyName(FString& nameStr, int nameIndex);

	void LoadImportTable();
	void LoadExportTable();

#if UNREAL4
	// IsStore AsyncPackage support
	void LoadPackageIoStore();
	void LoadNameTableIoStore(const byte* Data, int NameCount, int TableSize);
	void LoadExportTableIoStore(
		const byte* Data, int ExportCount, int TableSize, int PackageHeaderSize,
		const TArray<struct FExportBundleHeader>& BundleHeaders, const TArray<struct FExportBundleEntry>& BundleEntries);
	void LoadImportTableIoStore(const byte* Data, int ImportCount, const TArray<const CGameFileInfo*>& ImportPackages, struct CImportTableErrorStats& ErrorStats);
#endif // UNREAL4

	static TArray<UnPackage*> PackageMap;
};

#endif // __UNPACKAGE_H__
