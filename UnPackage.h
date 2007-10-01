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
		assert(Ar.IsLoading);						// saving is not supported

		Ar << S.Tag << S.FileVersion << S.LicenseeVersion << S.PackageFlags;
		Ar << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset << S.ImportCount << S.ImportOffset;
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
//	UObject		*Object;

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


class UnPackage
{
public:
	char					SelfName[256];
	FArchive				Ar;
	// package header
	FPackageFileSummary		Summary;
	// tables
	char					**NameTable;
	FObjectImport			*ImportTable;
	FObjectExport			*ExportTable;

	UnPackage(const char *filename);

	~UnPackage()
	{
		int i;
		for (i = 0; i < Summary.NameCount; i++)
			free(NameTable[i]);
		delete NameTable;
		delete ImportTable;
		delete ExportTable;
	}

	void SetupReader(int ExportIndex, FArchive &Ar)
	{
		if (ExportIndex < 0 || ExportIndex >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", SelfName, ExportIndex);
		const FObjectExport &Exp = ExportTable[ExportIndex];

		FILE *f = fopen(SelfName, "rb");
		if (!f)
			appError("Unable to open package file %s\n", SelfName);
		Ar.Setup(f, true);
		Ar.ArVer     = Summary.FileVersion;
		Ar.ArStopper = Exp.SerialOffset + Exp.SerialSize;
		Ar.Seek(Exp.SerialOffset);
	}

	const char* GetName(const FName& AName)
	{
		return GetName(AName.Index);
	}

	const char* GetName(int index)
	{
		if (index < 0 || index >= Summary.NameCount)
			appError("Package \"%s\": wrong name index %d", SelfName, index);
		return NameTable[index];
	}

	const char* GetClassName(int i)
	{
		if (i < 0)
		{
			// index in import table
			i = -i-1;
			if (i >= Summary.ImportCount)
				appError("Package \"%s\": wrong import index %d", SelfName, i);
			// should point to 'Class' object
			return GetName(ImportTable[i].ObjectName);
		}
		else
		{
			// index in export table
			if (i >= Summary.ExportCount)
				appError("Package \"%s\": wrong export index %d", SelfName, i);
			// should point to 'Class' object
			return GetName(ExportTable[i].ObjectName);
		}
	}

	int FindExport(const char* name)
	{
		for (int i = 0; i < Summary.ExportCount; i++)
		{
			if (!strcmp(GetName(ExportTable[i].ObjectName), name))
				return i;
		}
		return -1;
	}
};


#endif // __UNPACKAGE_H__
