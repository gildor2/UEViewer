#ifndef __UNPACKAGE_H__
#define __UNPACKAGE_H__


#define PACKAGE_FILE_TAG 0x9E2A83C1

#if 1
#	define PKG_LOG(x)		printf x
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
#if UNREAL3
		if (Ar.ArVer >= 322) // PACKAGE_V3
			Ar << I.NetObjectCount;
#endif
		return Ar;
	}
};


class FGuid
{
public:
	unsigned	A, B, C, D;

	friend FArchive& operator<<(FArchive &Ar, FGuid &G)
	{
		return Ar << G.A << G.B << G.C << G.D;
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
		return Ar << C.UncompressedOffset << C.UncompressedSize << C.CompressedOffset << C.CompressedSize;
	}
};

struct FComponentMapPair
{
	FName		Name;
	int			Value;

	friend FArchive& operator<<(FArchive &Ar, FComponentMapPair &P)
	{
		return Ar << P.Name << P.Value;
	}
};

#endif // UNREAL3


struct FPackageFileSummary
{
	int			Tag;
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
	FString		U3unk10;			// seems, always "None"
	int			DependsOffset;		// number of items = ExportCount
	int			EngineVersion;
	int			CookerVersion;
	int			CompressionFlags;
	TArray<FCompressedChunk> CompressedChunks;
	int			U3unk60;
#endif // UNREAL3

	friend FArchive& operator<<(FArchive &Ar, FPackageFileSummary &S)
	{
		guard(FPackageFileSummary<<);
		assert(Ar.IsLoading);						// saving is not supported

		// read tag and version
		Ar << S.Tag << S.FileVersion << S.LicenseeVersion;
		// store file version to archive (required for some structures, for UNREAL3 path)
		Ar.ArVer         = S.FileVersion;
		Ar.ArLicenseeVer = S.LicenseeVersion;
		// read other fields
#if UNREAL3
//		if (S.FileVersion >= PACKAGE_V3)
//		{
			if (S.FileVersion >= 249)
				Ar << S.HeadersSize;
			else
				S.HeadersSize = 0;
			if (S.FileVersion >= 269)
			{
				Ar << S.U3unk10;
				if (strcmp(*S.U3unk10, "None") != 0) appNotify("U3unk10 = \"%s\"", *S.U3unk10);
			}
//		}
#endif
		Ar << S.PackageFlags << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset << S.ImportCount << S.ImportOffset;
#if UNREAL3
		if (S.FileVersion >= 415) // PACKAGE_V3
			Ar << S.DependsOffset;
#endif
#if SPLINTER_CELL
		if (S.LicenseeVersion >= 0x0C && S.LicenseeVersion <= 0x1C &&
			(S.FileVersion == 100 || S.FileVersion == 102))
		{
			// SplinterCell
			int tmp1;
			FString tmp2;
			Ar << tmp1;								// 0xFF0ADDE
			Ar << tmp2;
		}
#endif // SPLINTER_CELL
#if RAGNAROK2
		if (S.PackageFlags & 0x10000 && S.FileVersion >= 0x81)
		{
			// encrypted Ragnarok Online archive header (data taken by archive analysis)
			Ar.IsRagnarok2 = 1;
			S.NameCount    ^= 0xD97790C7 ^ 0x1C;
			S.NameOffset   ^= 0xF208FB9F ^ 0x40;
			S.ExportCount  ^= 0xEBBDE077 ^ 0x04;
			S.ExportOffset ^= 0xE292EC62 ^ 0x03E9E1;
			S.ImportCount  ^= 0x201DA87A ^ 0x05;
			S.ImportOffset ^= 0xA9B999DF ^ 0x003E9BE;
			return Ar;								// other data is useless for us, and they are encrypted too
		}
#endif // RAGNAROK2
		if (S.FileVersion < 68)
		{
			int HeritageCount, HeritageOffset;
			Ar << HeritageCount << HeritageOffset;	// not used
			if (Ar.IsLoading)
			{
				S.Generations.Empty(1);
				FGenerationInfo gen;
				gen.ExportCount = S.ExportCount;
				gen.NameCount   = S.NameCount;
				S.Generations.AddItem(gen);
			}
		}
		else
		{
			Ar << S.Guid << S.Generations;
		}
#if UNREAL3
//		if (S.FileVersion >= PACKAGE_V3)
//		{
			if (S.FileVersion >= 245)
				Ar << S.EngineVersion;
			if (S.FileVersion >= 277)
				Ar << S.CookerVersion;
			// ... MassEffect has some additional structure here ...
			/*	if (Ar.IsMassEffect()) -- requires initializing of IsMassEffect before summary completely readed !!
				// NOTE: should detect version after reading all required fields: FileVersion, LicenseeVersion, EngineVersion etc ...
				{
					if (Ar.ArLicenseeVer >= 16) serialize 1*int - random value
					if (Ar.ArLicenseeVer >= 32) serialize 1*int
					if (Ar.ArLicenseeVer >= 35) some arrays ...
					if (Ar.ArLicenseeVer >= 37) serialize 2*int
					if (Ar.ArLicenseeVer >= 39) serialize 2*int
				}
			*/
			if (S.FileVersion >= 334)
				Ar << S.CompressionFlags << S.CompressedChunks;
			if (S.FileVersion >= 482)
				Ar << S.U3unk60;
			// ... MassEffect has additional field here ...
			// if (Ar.IsMassEffect() && Ar.ArLicenseeVer >= 44) serialize 1*int
//		}
//		printf("EngVer:%d CookVer:%d CompF:%d CompCh:%d\n", S.EngineVersion, S.CookerVersion, S.CompressionFlags, S.CompressedChunks.Num());
//		printf("Names:%X[%d] Exports:%X[%d] Imports:%X[%d]\n", S.NameOffset, S.NameCount, S.ExportOffset, S.ExportCount, S.ImportOffset, S.ImportCount);
//		printf("HeadersSize:%X U10:%s DependsOffset:%X U60:%X\n", S.HeadersSize, *S.U3unk10, S.DependsOffset, S.U3unk60);
#endif // UNREAL3
		return Ar;
		unguard;
	}
};


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
	unsigned	ExportFlags;
	TArray<FComponentMapPair> ComponentMap;	// TMap<FName, int>
	TArray<int>	NetObjectCount;				// generations
	FGuid		Guid;
	int			U3unk6C;
#endif // UNREAL3

	friend FArchive& operator<<(FArchive &Ar, FObjectExport &E)
	{
#if UNREAL3
		if (Ar.ArVer >= PACKAGE_V3)
		{
			Ar << E.ClassIndex << E.SuperIndex << E.PackageIndex;
			Ar << E.ObjectName << E.Archetype << E.ObjectFlags << E.ObjectFlags2;
			Ar << E.SerialSize << E.SerialOffset;
			Ar << E.ComponentMap << E.ExportFlags << E.NetObjectCount;
			Ar << E.Guid << E.U3unk6C;
			return Ar;
		}
#endif // UNREAL3
		Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SuperIndex) << E.PackageIndex;
		Ar << E.ObjectName << E.ObjectFlags << AR_INDEX(E.SerialSize);
		if (E.SerialSize)
			Ar << AR_INDEX(E.SerialOffset);
		return Ar;
	}
};


struct FObjectImport
{
	FName		ClassPackage;
	FName		ClassName;
	int			PackageIndex;
	FName		ObjectName;

	friend FArchive& operator<<(FArchive &Ar, FObjectImport &I)
	{
		return Ar << I.ClassPackage << I.ClassName << I.PackageIndex << I.ObjectName;
	}
};


// In Unreal Engine class with similar functionality named "ULinkerLoad"
class UnPackage : public FArchive
{
public:
	char					Filename[256];
	FArchive				*Loader;
	// package header
	FPackageFileSummary		Summary;
	// tables
	char					**NameTable;
	FObjectImport			*ImportTable;
	FObjectExport			*ExportTable;

	UnPackage(const char *filename);
	~UnPackage();

	// load package using short name (without path and extension)
	static UnPackage *LoadPackage(const char *Name);
	static void       SetSearchPath(const char *Path);

	void SetupReader(int ExportIndex)
	{
		guard(UnPackage::SetupReader);

		if (ExportIndex < 0 || ExportIndex >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", Filename, ExportIndex);
		const FObjectExport &Exp = ExportTable[ExportIndex];
		// setup FArchive
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

	const FObjectImport& GetImport(int index)
	{
		if (index >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", Filename, index);
		return ImportTable[index];
	}

	FObjectExport& GetExport(int index) // not 'const'
	{
		if (index >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", Filename, index);
		return ExportTable[index];
	}

	const char* GetObjectName(int i)	//?? GetExportClassName()
	{
		if (i < 0)
		{
			//?? should point to 'Class' object
			return GetImport(-i-1).ObjectName;
		}
		else if (i > 0)
		{
			//?? should point to 'Class' object
			return GetExport(i-1).ObjectName;
		}
		else // i == 0
		{
			return "Class";
		}
	}

	int FindExport(const char *name, const char *className = NULL)
	{
		for (int i = 0; i < Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = ExportTable[i];
			// compare object name
			if (strcmp(Exp.ObjectName, name) != 0)
				continue;
			// if class name specified - compare it too
			if (className && strcmp(GetObjectName(Exp.ClassIndex), className) != 0)
				continue;
			return i;
		}
		return INDEX_NONE;
	}

	UObject* CreateExport(int index);
	UObject* CreateImport(int index);

	// FArchive interface
	virtual FArchive& operator<<(FName &N)
	{
#if UNREAL3
		if (ArVer >= PACKAGE_V3)
			*this << N.Index << N.Flags;
		else
#endif
			*this << AR_INDEX(N.Index);
		N.Str = GetName(N.Index);
		return *this;
	}

	virtual FArchive& operator<<(UObject *&Obj)
	{
		int index;
		*this << AR_INDEX(index);
		if (index < 0)
		{
			const FObjectImport &Imp = GetImport(-index-1);
//			printf("PKG: Import[%s,%d] OBJ=%s CLS=%s\n", GetObjectName(Imp.PackageIndex), index, *Imp.ObjectName, *Imp.ClassName);
			Obj = CreateImport(-index-1);
		}
		else if (index > 0)
		{
			const FObjectExport &Exp = GetExport(index-1);
//			printf("PKG: Export[%d] OBJ=%s CLS=%s\n", index, *Exp.ObjectName, GetObjectName(Exp.ClassIndex));
			Obj = CreateExport(index-1);
		}
		else // index == 0
		{
			Obj = NULL;
		}
		return *this;
	}

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

private:
	// package list
	struct PackageEntry
	{
		char		Name[64];			// short name, without extension
		UnPackage	*Package;
	};
	static TArray<PackageEntry> PackageMap;
	static char SearchPath[256];
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
