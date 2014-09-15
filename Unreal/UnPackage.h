#ifndef __UNPACKAGE_H__
#define __UNPACKAGE_H__


#if 1
#	define PKG_LOG(x)		appPrintf x
#else
#	define PKG_LOG(x)
#endif


struct FGenerationInfo
{
	int			ExportCount, NameCount;
#if UNREAL3
	int			NetObjectCount;
#endif

	friend FArchive& operator<<(FArchive &Ar, FGenerationInfo &I)
	{
		Ar << I.ExportCount << I.NameCount;
#if UNREAL4
		if (Ar.Game >= GAME_UE4 && Ar.ArVer >= VER_UE4_REMOVE_NET_INDEX) return Ar;
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
	int			UncompressedOffset;
	int			UncompressedSize;
	int			CompressedOffset;
	int			CompressedSize;

	friend FArchive& operator<<(FArchive &Ar, FCompressedChunk &C)
	{
		guard(FCompressedChunk<<);
		Ar << C.UncompressedOffset << C.UncompressedSize << C.CompressedOffset << C.CompressedSize;
#if BULLETSTORM
		if (Ar.Game == GAME_Bulletstorm && Ar.ArLicenseeVer >= 21)
		{
			int unk10;			// unused?
			Ar << unk10;
			assert(unk10 == 1);
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
	word		Major, Minor, Patch;
	int			Changelist;
	FString		Branch;

	friend FArchive& operator<<(FArchive& Ar, FEngineVersion& V)
	{
		return Ar << V.Major << V.Minor << V.Patch << V.Changelist << V.Branch;
	}
};

#endif // UNREAL4


struct FPackageFileSummary
{
	int			Tag;
#if UNREAL4
	int			LegacyVersion;
#endif
	word		FileVersion;
	word		LicenseeVersion;
	int			PackageFlags;
	int			NameCount,   NameOffset;
	int			ExportCount, ExportOffset;
	int			ImportCount, ImportOffset;
	FGuid		Guid;
	TArray<FGenerationInfo> Generations;
#if UNREAL3
	int			HeadersSize;		// used by UE3 for precaching name table
	FString		PackageGroup;		// "None" or directory name
	int			DependsOffset;		// number of items = ExportCount
	int			f38;
	int			f3C;
	int			f40;
	int			EngineVersion;
	int			CookerVersion;
	int			CompressionFlags;
	TArray<FCompressedChunk> CompressedChunks;
	int			U3unk60;
#endif // UNREAL3

	friend FArchive& operator<<(FArchive &Ar, FPackageFileSummary &S);
};

#if UNREAL3
#define EF_ForcedExport		1
// other noticed values: 2
#endif

struct FObjectExport
{
	int			ClassIndex;					// object reference
	int			SuperIndex;					// object reference
	int			PackageIndex;				// object reference
	FName		ObjectName;
	int			SerialSize;
	int			SerialOffset;
	UObject		*Object;					// not serialized, filled by object loader
	unsigned	ObjectFlags;
#if UNREAL3
	unsigned	ObjectFlags2;				// really, 'int64 ObjectFlags'
	int			Archetype;
	unsigned	ExportFlags;				// EF_* flags
//	TMap<FName, int> ComponentMap;			-- this field was removed from UE3, so serialize it as a temporaty variable when needed
	TArray<int>	NetObjectCount;				// generations
	FGuid		Guid;
	int			U3unk6C;					//!! PackageFlags
#endif // UNREAL3

	friend FArchive& operator<<(FArchive &Ar, FObjectExport &E);
};


struct FObjectImport
{
	FName		ClassPackage;
	FName		ClassName;
	int			PackageIndex;
	FName		ObjectName;
	bool		Missing;			// not serialized

	friend FArchive& operator<<(FArchive &Ar, FObjectImport &I);
};

#if UNREAL3

struct FObjectDepends
{
	TArray<int>	Objects;

	friend FArchive& operator<<(FArchive &Ar, FObjectDepends &D)
	{
		return Ar << D.Objects;
	}
};

#endif // UNREAL3


// In Unreal Engine class with similar functionality named "ULinkerLoad"
class UnPackage : public FArchive
{
public:
	char					Filename[256];			// full name with path and extension
	char					Name[64];				// short name
	FArchive				*Loader;
	// package header
	FPackageFileSummary		Summary;
	// tables
	const char				**NameTable;
	FObjectImport			*ImportTable;
	FObjectExport			*ExportTable;
#if UNREAL3
	FObjectDepends			*DependsTable;
#endif

	UnPackage(const char *filename, FArchive *baseLoader = NULL);
	~UnPackage();

	// Load package using short name (without path and extension). When the package
	// is already loaded, this function will symply return a pointer to earlier
	// loaded UnPackage.
	static UnPackage *LoadPackage(const char *Name);

	static FArchive* CreateLoader(const char* filename, FArchive* baseLoader = NULL);

	void SetupReader(int ExportIndex)
	{
		guard(UnPackage::SetupReader);
		const FObjectExport &Exp = GetExport(ExportIndex);
		SetStopper(Exp.SerialOffset + Exp.SerialSize);
		Seek(Exp.SerialOffset);
		unguard;
	}

	const char* GetName(int index)
	{
		if (index < 0 || index >= Summary.NameCount)
			appError("Package \"%s\": wrong name index %d", Filename, index);
		return NameTable[index];
	}

	FObjectImport& GetImport(int index)
	{
		if (index < 0 || index >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", Filename, index);
		return ImportTable[index];
	}

	const FObjectImport& GetImport(int index) const // duplicate for 'const' package
	{
		if (index < 0 || index >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", Filename, index);
		return ImportTable[index];
	}

	FObjectExport& GetExport(int index)
	{
		if (index < 0 || index >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", Filename, index);
		return ExportTable[index];
	}

	// "const" version of the above function
	const FObjectExport& GetExport(int index) const
	{
		if (index < 0 || index >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", Filename, index);
		return ExportTable[index];
	}

	const char* GetObjectName(int PackageIndex) const	//?? GetExportClassName()
	{
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

	virtual void Serialize(void *data, int size)
	{
		Loader->Serialize(data, size);
	}
	virtual void Seek(int Pos)
	{
		Loader->Seek(Pos);
	}
	virtual int  Tell() const
	{
		return Loader->Tell();
	}
	virtual void SetStopper(int Pos)
	{
		Loader->SetStopper(Pos);
	}
	virtual int  GetStopper() const
	{
		return Loader->GetStopper();
	}
	virtual int GetFileSize() const
	{
		return Loader->GetFileSize();
	}

private:
	static TArray<UnPackage*> PackageMap;
};


// VC7 has strange bug with friend functions, declared in class
// Here is a workaround
#if _MSC_VER >= 1300 && _MSC_VER < 1400
template<class T>
FArchive& operator<<(UnPackage &Ar, T &V)
{
	return static_cast<FArchive&>(Ar) << V;
}
#endif


#endif // __UNPACKAGE_H__
