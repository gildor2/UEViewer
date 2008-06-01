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

	friend FArchive& operator<<(FArchive &Ar, FGenerationInfo &I)
	{
		return Ar << I.ExportCount << I.NameCount;
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


struct FPackageFileSummary
{
	int			Tag;
	word		FileVersion;
	word		LicenseeVersion;
	int			PackageFlags;
	int			NameCount,   NameOffset;
	int			ExportCount, ExportOffset;
	int			ImportCount, ImportOffset;

	friend FArchive& operator<<(FArchive &Ar, FPackageFileSummary &S)
	{
		guard(FPackageFileSummary<<);
		assert(Ar.IsLoading);						// saving is not supported

		Ar << S.Tag << S.FileVersion << S.LicenseeVersion << S.PackageFlags;
		Ar << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset << S.ImportCount << S.ImportOffset;
#if SPLINTER_CELL
		if (S.LicenseeVersion >= 0x0C && S.LicenseeVersion <= 0x1C)
		{
			// SplinterCell
/*			int tmp;
			Ar << tmp;								// 0xFF0ADDE
			Ar << AR_INDEX(tmp);					// TArray<byte> -- some encoded computer information
			Ar.Seek(Ar.ArPos + tmp); */
			FString tmp;
			Ar << tmp;
		}
#endif
		if (S.FileVersion < 68)
		{
			int HeritageCount, HeritageOffset;
			Ar << HeritageCount << HeritageOffset;	// not used
		}
		else
		{
			FGuid tmp1;
			TArray<FGenerationInfo> tmp2;
			Ar << tmp1 << tmp2;
		}
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
	unsigned	ObjectFlags;
	int			SerialSize;
	int			SerialOffset;
	UObject		*Object;					// not serialized, filled by object loader

	friend FArchive& operator<<(FArchive &Ar, FObjectExport &E)
	{
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


class UnPackage : public FFileReader
{
public:
	char					Filename[256];
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
		ArStopper = Exp.SerialOffset + Exp.SerialSize;
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
		return -1;
	}

	UObject* CreateExport(int index);
	UObject* CreateImport(int index);

	// FArchive interface
	virtual FArchive& operator<<(FName &N)
	{
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

private:
	// package list
	struct PackageEntry
	{
		char		Name[64];			// short name, without extemsion
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
