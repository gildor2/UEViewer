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


class UnPackage : public FFileReader
{
public:
	char					SelfName[256];		//?? rename
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

	void SetupReader(int ExportIndex)
	{
		if (ExportIndex < 0 || ExportIndex >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", SelfName, ExportIndex);
		const FObjectExport &Exp = ExportTable[ExportIndex];
		// setup FArchive
		ArStopper = Exp.SerialOffset + Exp.SerialSize;
		Seek(Exp.SerialOffset);
	}

	const char* GetName(int index)
	{
		if (index < 0 || index >= Summary.NameCount)
			appError("Package \"%s\": wrong name index %d", SelfName, index);
		return NameTable[index];
	}

	const FObjectImport& GetImport(int index)
	{
		if (index >= Summary.ImportCount)
			appError("Package \"%s\": wrong import index %d", SelfName, index);
		return ImportTable[index];
	}

	const FObjectExport& GetExport(int index)
	{
		if (index >= Summary.ExportCount)
			appError("Package \"%s\": wrong export index %d", SelfName, index);
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

	UObject* CreateExport(int index)
	{
		// create empty object
		const FObjectExport &Exp = GetExport(index);
		const char *ClassName = GetObjectName(Exp.ClassIndex);
		UObject *Obj = CreateClass(ClassName);
		if (!Obj)
			appError("Unknown object class: %s'%s'", ClassName, *Exp.ObjectName);
		Obj->Package      = this;
		Obj->PackageIndex = index;
		Obj->Name         = Exp.ObjectName;
		// serialize object
		//!! code below should be executed later (after current object serialization
		//!! finished, if one)
		SetupReader(index);
		Obj->Serialize(*this);
		// check for unread bytes
		if (!IsStopper()) appError("%s: extra bytes", Obj->Name);
		return Obj;
	}

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
			printf("PKG: Import[%s,%d] OBJ=%s CLS=%s\n", GetObjectName(Imp.PackageIndex), index, *Imp.ObjectName, *Imp.ClassName);
			//!! create object
		}
		else if (index > 0)
		{
			const FObjectExport &Exp = GetExport(index-1);
			printf("PKG: Export[%d] OBJ=%s CLS=%s\n", index, *Exp.ObjectName, GetObjectName(Exp.ClassIndex));
			//!! create object
		}
		else // index == 0
		{
			Obj = NULL;
		}
		return *this;
	}
};


#endif // __UNPACKAGE_H__
