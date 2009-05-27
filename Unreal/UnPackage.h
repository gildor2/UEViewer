#ifndef __UNPACKAGE_H__
#define __UNPACKAGE_H__


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

RAW_TYPE(FCompressedChunk)

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

#if MASSEFF

struct FStringArrayMap // unknown name; TMap<FString, TArray<FString> > ?
{
	FString			Key;
	TArray<FString> Value;

	friend FArchive& operator<<(FArchive &Ar, FStringArrayMap &P)
	{
		return Ar << P.Key << P.Value;
	}
};

#endif // MASSEFF


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
	FString		PackageGroup;		// "None" or directory name
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
		Ar << S.Tag;
		if (S.Tag != PACKAGE_FILE_TAG)
		{
			if (S.Tag != PACKAGE_FILE_TAG_REV)
				appError("Wrong tag in package");
			Ar.ReverseBytes = true;
			S.Tag = PACKAGE_FILE_TAG;
		}
		int Version;
		Ar << Version;
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
		// read other fields
#if HUXLEY
		if (Ar.IsHuxley && Ar.ArLicenseeVer >= 8)
		{
			int skip;
			Ar << skip;								// 0xFEFEFEFE
			if (Ar.ArLicenseeVer >= 17)
			{
				int unk8;							// unknown used field
				Ar << unk8;
			}
		}
#endif // HUXLEY
#if UNREAL3
//		if (S.FileVersion >= PACKAGE_V3)
//		{
			if (S.FileVersion >= 249)
				Ar << S.HeadersSize;
			else
				S.HeadersSize = 0;
	// NOTE: A51 and MKVSDC has exactly the same code paths!
	#if A51 || WHEELMAN || MKVSDC					//?? special define ?
			int midwayVer = 0;
			if ((Ar.IsA51 || Ar.IsWheelman || Ar.IsMK) && S.LicenseeVersion >= 2)	//?? Wheelman not checked
			{
				int Tag;							// Tag == "A52 ", "MK8 " or "WMAN"
				Ar << Tag << midwayVer;
			}
	#endif // MIDWAY
			if (S.FileVersion >= 269)
				Ar << S.PackageGroup;
//		}
#endif // UNREAL3
		Ar << S.PackageFlags << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset << S.ImportCount << S.ImportOffset;
#if UNREAL3
	#if MKVSDC
		if (Ar.IsMK)
		{
			int unk3C, unk40;
			if (midwayVer >= 16)
				Ar << unk3C;
			if (Ar.ArVer >= 391)
				Ar << unk40;
		}
	#endif // MKVSDC
	#if WHEELMAN
		if (Ar.IsWheelman && midwayVer >= 23)
		{
			int unk3C;
			Ar << unk3C;
		}
	#endif // WHEELMAN
		if (S.FileVersion >= 415) // PACKAGE_V3
			Ar << S.DependsOffset;
#endif // UNREAL3
#if SPLINTER_CELL
		if (Ar.IsSplinterCell)
		{
			int tmp1;
			FString tmp2;
			Ar << tmp1;								// 0xFF0ADDE
			Ar << tmp2;
		}
#endif // SPLINTER_CELL
#if RAGNAROK2
		if (S.PackageFlags & 0x10000 && (S.FileVersion >= 0x81 && S.FileVersion < 0x88))	//?? unknown upper limit; known: 0x81
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
	#if MASSEFF
			// ... MassEffect has some additional structure here ...
			if (Ar.IsMassEffect)
			{
				int unk1, unk2, unk3[2], unk4[2];
				if (Ar.ArLicenseeVer >= 16) Ar << unk1;					// random value
				if (Ar.ArLicenseeVer >= 32) Ar << unk2;					// unknown
				if (Ar.ArLicenseeVer >= 35)
				{
					TArray<FStringArrayMap> unk5;
					Ar << unk5;
				}
				if (Ar.ArLicenseeVer >= 37) Ar << unk3[0] << unk3[1];	// 2 ints: 1, 0
				if (Ar.ArLicenseeVer >= 39) Ar << unk4[0] << unk4[1];	// 2 ints: -1, -1
			}
	#endif // MASSEFF
			if (S.FileVersion >= 334)
				Ar << S.CompressionFlags << S.CompressedChunks;
			if (S.FileVersion >= 482)
				Ar << S.U3unk60;
//			if (S.FileVersion >= 516)
//				Ar << some array ... (U3unk70)
			// ... MassEffect has additional field here ...
			// if (Ar.IsMassEffect() && Ar.ArLicenseeVer >= 44) serialize 1*int
//		}
//		printf("EngVer:%d CookVer:%d CompF:%d CompCh:%d\n", S.EngineVersion, S.CookerVersion, S.CompressionFlags, S.CompressedChunks.Num());
//		printf("Names:%X[%d] Exports:%X[%d] Imports:%X[%d]\n", S.NameOffset, S.NameCount, S.ExportOffset, S.ExportCount, S.ImportOffset, S.ImportCount);
//		printf("HeadersSize:%X Group:%s DependsOffset:%X U60:%X\n", S.HeadersSize, *S.PackageGroup, S.DependsOffset, S.U3unk60);
#endif // UNREAL3
		return Ar;
		unguardf(("Ver=%d/%d", S.FileVersion, S.LicenseeVersion));
	}
};

#if UNREAL3
#define EF_ForcedExport		1
// other values: 2
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
			Ar << E.ObjectName << E.Archetype << E.ObjectFlags << E.ObjectFlags2 << E.SerialSize;
			if (E.SerialSize || Ar.ArVer >= 249)
				Ar << E.SerialOffset;
#	if HUXLEY
			if (Ar.IsHuxley && Ar.ArLicenseeVer >= 22)
			{
				int unk24;
				Ar << unk24;
			}
#	endif
			if (Ar.ArVer < 543)
				Ar << E.ComponentMap;
			if (Ar.ArVer >= 247)
				Ar << E.ExportFlags;
			if (Ar.ArVer >= 322)
				Ar << E.NetObjectCount << E.Guid;
			if (Ar.ArVer >= 475)
				Ar << E.U3unk6C;
			return Ar;
		}
#endif // UNREAL3
#if UC2
		if (Ar.ArVer >= 145) // && < PACKAGE_V3
		{
			Ar << E.ClassIndex << E.SuperIndex;
			if (Ar.ArVer >= 150)
			{
				short idx = E.PackageIndex;
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
			E.ClassIndex = short(E.ClassIndex);
			E.SuperIndex = short(E.SuperIndex);
			return Ar;
		}
#endif // UC2
#if PARIAH
		if (Ar.IsPariah)
		{
			Ar << E.ObjectName << AR_INDEX(E.SuperIndex) << E.PackageIndex << E.ObjectFlags;
			Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SerialSize);
			if (E.SerialSize)
				Ar << AR_INDEX(E.SerialOffset);
			return Ar;
		}
#endif // PARIAH
		// generic UE1/UE2 code
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
#if UC2
		if (Ar.ArVer >= 150 && Ar.ArVer < PACKAGE_V3)	// UC2 ??
		{
			short idx = I.PackageIndex;
			Ar << I.ClassPackage << I.ClassName << idx << I.ObjectName;
			I.PackageIndex = idx;
			return Ar;
		}
#endif // UC2
#if PARIAH
		if (Ar.IsPariah)
			return Ar << I.PackageIndex << I.ObjectName << I.ClassPackage << I.ClassName;
#endif
		return Ar << I.ClassPackage << I.ClassName << I.PackageIndex << I.ObjectName;
	}
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
	char					Filename[256];
	FArchive				*Loader;
	// package header
	FPackageFileSummary		Summary;
	// tables
	char					**NameTable;
	FObjectImport			*ImportTable;
	FObjectExport			*ExportTable;
#if UNREAL3
	FObjectDepends			*DependsTable;
#endif

	UnPackage(const char *filename, FArchive *Ar = NULL);
	~UnPackage();

	// load package using short name (without path and extension)
	static UnPackage *LoadPackage(const char *Name);

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
		if (index < 0 || index >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", Filename, index);
		return ImportTable[index];
	}

	FObjectExport& GetExport(int index) // not 'const'
	{
		if (index < 0 || index >= Summary.ExportCount)
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
		if (ArVer >= 145)				// PACKAGE_V3, but have version in UC2
		{
			*this << N.Index;
			if (ArVer >= PACKAGE_V3)
				*this << N.Flags;
		}
		else
#endif
			*this << AR_INDEX(N.Index);
		N.Str = GetName(N.Index);
		return *this;
	}

	virtual FArchive& operator<<(UObject *&Obj)
	{
		int index;
#if UNREAL3
		if (ArVer >= 145)				 // PACKAGE_V3, but has in UC2
			*this << index;
		else
#endif
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
