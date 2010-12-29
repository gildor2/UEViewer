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

	friend FArchive& operator<<(FArchive &Ar, FPackageFileSummary &S)
	{
		guard(FPackageFileSummary<<);
		assert(Ar.IsLoading);						// saving is not supported

		// read tag and version
		Ar << S.Tag;
#if SPECIAL_TAGS
		if (S.Tag == 0xA94E6C81) goto tag_ok;		// Nurien
		if (S.Tag == 0x9E2A83C2) goto tag_ok;		// Killing Floor
#endif // SPECIAL_TAGS
#if BATTLE_TERR
		if (S.Tag == 0xA1B2C93F)
		{
			Ar.Game = GAME_BattleTerr;
			goto tag_ok;		// Battle Territory Online
		}
#endif // BATTLE_TERR
#if LOCO
		if (S.Tag == 0xD58C3147)
		{
			Ar.Game = GAME_Loco;
			goto tag_ok;		// Land of Chaos Online
		}
#endif // LOCO
#if BERKANIX
		if (S.Tag == 0xF2BAC156)
		{
			//?? check later: probably this game has only a custom package tag
			Ar.Game = GAME_Berkanix;
			goto tag_ok;
		}
#endif // BERKANIX

		if (S.Tag != PACKAGE_FILE_TAG)
		{
			if (S.Tag != PACKAGE_FILE_TAG_REV)
				appError("Wrong tag in package");
			Ar.ReverseBytes = true;
			S.Tag = PACKAGE_FILE_TAG;
		}
	tag_ok:
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
#if SPLINTER_CELL
		if (Ar.Game == GAME_SplinterCell && Ar.ArLicenseeVer >= 83)
		{
			int unk8;
			Ar << unk8;
		}
#endif // SPLINTER_CELL
#if R6VEGAS
		if (Ar.Game == GAME_R6Vegas2)
		{
			int unk8, unkC;
			if (Ar.ArLicenseeVer >= 48) Ar << unk8;
			if (Ar.ArLicenseeVer >= 49) Ar << unkC;
		}
#endif // R6VEGAS
#if HUXLEY
		if (Ar.Game == GAME_Huxley && Ar.ArLicenseeVer >= 8)
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
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 55)
		{
			int unk8;								// always 0x4BF1EB6B?
			Ar << unk8;
		}
#endif // TRANSFORMERS
#if MORTALONLINE
		if (Ar.Game == GAME_MortalOnline && Ar.ArLicenseeVer >= 1)
		{
			int unk8;
			Ar << unk8;								// always 0?
		}
#endif
#if UNREAL3
//		if (Ar.ArVer >= PACKAGE_V3)
//		{
			if (Ar.ArVer >= 249)
				Ar << S.HeadersSize;
			else
				S.HeadersSize = 0;
	// NOTE: A51 and MKVSDC has exactly the same code paths!
	#if A51 || WHEELMAN || MKVSDC || STRANGLE || TNA_IMPACT				//?? special define ?
			int midwayVer = 0;
			if (Ar.Engine() == GAME_MIDWAY3 && Ar.ArLicenseeVer >= 2)	//?? Wheelman not checked
			{
				int Tag;							// Tag == "A52 ", "MK8 ", "WMAN", "WOO " (Stranglehold), "EPIC" or "TNA "
				int unk10;
				Ar << Tag << midwayVer;
				if (Ar.Game == GAME_Strangle && midwayVer >= 256)
					Ar << unk10;
			}
	#endif // MIDWAY
			if (Ar.ArVer >= 269)
				Ar << S.PackageGroup;
//		}
#endif // UNREAL3
		Ar << S.PackageFlags << S.NameCount << S.NameOffset << S.ExportCount << S.ExportOffset;
#if APB
		if (Ar.Game == GAME_APB)
		{
			int unk2C;
			float unk30[5];
			if (Ar.ArLicenseeVer >= 29) Ar << unk2C;
			if (Ar.ArLicenseeVer >= 28) Ar << unk30[0] << unk30[1] << unk30[2] << unk30[3] << unk30[4];
		}
#endif // APB
		Ar << S.ImportCount << S.ImportOffset;
#if UNREAL3
	#if MKVSDC
		if (Ar.Game == GAME_MK)
		{
			int unk3C, unk40;
			if (midwayVer >= 16)
				Ar << unk3C;
			if (Ar.ArVer >= 391)
				Ar << unk40;
		}
	#endif // MKVSDC
	#if WHEELMAN
		if (Ar.Game == GAME_Wheelman && midwayVer >= 23)
		{
			int unk3C;
			Ar << unk3C;
		}
	#endif // WHEELMAN
	#if STRANGLE
		if (Ar.Game == GAME_Strangle && Ar.ArVer >= 375)
		{
			int unk40;
			Ar << unk40;
		}
	#endif // STRANGLE
	#if TERA
		// de-obfuscate NameCount for Tera
		if (Ar.Game == GAME_Tera && (S.PackageFlags & 8)) S.NameCount -= S.NameOffset;
	#endif
		if (Ar.ArVer >= 415) // PACKAGE_V3
			Ar << S.DependsOffset;
		if (Ar.ArVer >= 623)
			Ar << S.f38 << S.f3C << S.f40;
#endif // UNREAL3
#if SPLINTER_CELL
		if (Ar.Game == GAME_SplinterCell)
		{
			int tmp1;
			TArray<byte> tmp2;
			Ar << tmp1;								// 0xFF0ADDE
			Ar << tmp2;
		}
#endif // SPLINTER_CELL
#if RAGNAROK2
		if (S.PackageFlags & 0x10000 && (Ar.ArVer >= 0x80 && Ar.ArVer < 0x88))	//?? unknown upper limit; known lower limit: 0x80
		{
			// encrypted Ragnarok Online archive header (data taken by archive analysis)
			Ar.Game = GAME_Ragnarok2;
			S.NameCount    ^= 0xD97790C7 ^ 0x1C;
			S.NameOffset   ^= 0xF208FB9F ^ 0x40;
			S.ExportCount  ^= 0xEBBDE077 ^ 0x04;
			S.ExportOffset ^= 0xE292EC62 ^ 0x03E9E1;
			S.ImportCount  ^= 0x201DA87A ^ 0x05;
			S.ImportOffset ^= 0xA9B999DF ^ 0x003E9BE;
			return Ar;								// other data is useless for us, and they are encrypted too
		}
#endif // RAGNAROK2
		if (Ar.ArVer < 68)
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
#if UNREAL3
			if (Ar.ArVer >= 584)
			{
				int unk38;
				Ar << unk38;
			}
#endif // UNREAL3
			Ar << S.Guid;
#if BIOSHOCK
			if (Ar.Game == GAME_Bioshock)
			{
				// Bioshock uses AR_INDEX for array size, but int for generations
				int Count;
				Ar << Count;
				S.Generations.Empty(Count);
				S.Generations.Add(Count);
				for (int i = 0; i < Count; i++)
					Ar << S.Generations[i];
			}
			else
#endif // BIOSHOCK
			{
				Ar << S.Generations;
			}
		}
#if UNREAL3
//		if (Ar.ArVer >= PACKAGE_V3)
//		{
			if (Ar.ArVer >= 245)
				Ar << S.EngineVersion;
			if (Ar.ArVer >= 277)
				Ar << S.CookerVersion;
	#if MASSEFF
			// ... MassEffect has some additional structure here ...
			if (Ar.Game == GAME_MassEffect || Ar.Game == GAME_MassEffect2)
			{
				int unk1, unk2, unk3[2], unk4[2];
				if (Ar.ArLicenseeVer >= 16) Ar << unk1;					// random value
				if (Ar.ArLicenseeVer >= 32) Ar << unk2;					// unknown
				if (Ar.ArLicenseeVer >= 35 && Ar.Game != GAME_MassEffect2) // MassEffect2 has upper version limit: 113
				{
					TMap<FString, TArray<FString> > unk5;
					Ar << unk5;
				}
				if (Ar.ArLicenseeVer >= 37) Ar << unk3[0] << unk3[1];	// 2 ints: 1, 0
				if (Ar.ArLicenseeVer >= 39) Ar << unk4[0] << unk4[1];	// 2 ints: -1, -1
			}
	#endif // MASSEFF
			if (Ar.ArVer >= 334)
				Ar << S.CompressionFlags << S.CompressedChunks;
			if (Ar.ArVer >= 482)
				Ar << S.U3unk60;
//			if (Ar.ArVer >= 516)
//				Ar << some array ... (U3unk70)
			// ... MassEffect has additional field here ...
			// if (Ar.Game == GAME_MassEffect() && Ar.ArLicenseeVer >= 44) serialize 1*int
//		}
	#if 0
		printf("EngVer:%d CookVer:%d CompF:%d CompCh:%d\n", S.EngineVersion, S.CookerVersion, S.CompressionFlags, S.CompressedChunks.Num());
		printf("Names:%X[%d] Exports:%X[%d] Imports:%X[%d]\n", S.NameOffset, S.NameCount, S.ExportOffset, S.ExportCount, S.ImportOffset, S.ImportCount);
		printf("HeadersSize:%X Group:%s DependsOffset:%X U60:%X\n", S.HeadersSize, *S.PackageGroup, S.DependsOffset, S.U3unk60);
	#endif
#endif // UNREAL3
		return Ar;
		unguardf(("Ver=%d/%d", S.FileVersion, S.LicenseeVersion));
	}
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
	TMap<FName, int> ComponentMap;
	TArray<int>	NetObjectCount;				// generations
	FGuid		Guid;
	int			U3unk6C;
#endif // UNREAL3

	friend FArchive& operator<<(FArchive &Ar, FObjectExport &E)
	{
#if UNREAL3
		if (Ar.Game >= GAME_UE3)
		{
#	if AA3
			int AA3Obfuscator = 0;
			if (Ar.Game == GAME_AA3)
			{
				//!! if (Package->Summary.PackageFlags & 0x200000)
				Ar << AA3Obfuscator;	// random value
			}
#	endif // AA3
#	if WHEELMAN
			if (Ar.Game == GAME_Wheelman)
			{
				// Wheelman has special code for quick serialization of FObjectExport struc
				// using a single Serialize(&S, 0x64) call
				// Ar.MidwayVer >= 22; when < 22 => standard version w/o ObjectFlags
				int tmp1, tmp2, tmp3;
				Ar << tmp1;	// 0 or 1
				Ar << E.ObjectName << E.PackageIndex << E.ClassIndex << E.SuperIndex << E.Archetype;
				Ar << E.ObjectFlags << E.ObjectFlags2 << E.SerialSize << E.SerialOffset;
				Ar << tmp2; // zero ?
				Ar << tmp3; // -1 ?
				Ar.Seek(Ar.Tell() + 0x14);	// skip raw version of ComponentMap
				Ar << E.ExportFlags;
				Ar.Seek(Ar.Tell() + 0xC); // skip raw version of NetObjectCount
				Ar << E.Guid;
				return Ar;
			}
#	endif // WHEELMAN
			Ar << E.ClassIndex << E.SuperIndex << E.PackageIndex << E.ObjectName;
			if (Ar.ArVer >= 220) Ar << E.Archetype;
			Ar << E.ObjectFlags;
			if (Ar.ArVer >= 195) Ar << E.ObjectFlags2;	// qword flags after version 195
			Ar << E.SerialSize;
			if (E.SerialSize || Ar.ArVer >= 249)
				Ar << E.SerialOffset;
#	if HUXLEY
			if (Ar.Game == GAME_Huxley && Ar.ArLicenseeVer >= 22)
			{
				int unk24;
				Ar << unk24;
			}
#	endif // HUXLEY
#	if ALPHA_PR
			if (Ar.Game == GAME_AlphaProtocol && Ar.ArLicenseeVer >= 53) goto ue3_export_flags;	// no ComponentMap
#	endif
#	if TRANSFORMERS
			if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 37) goto ue3_export_flags;	// no ComponentMap
#	endif
			if (Ar.ArVer < 543)  Ar << E.ComponentMap;
		ue3_export_flags:
			if (Ar.ArVer >= 247) Ar << E.ExportFlags;
#	if TRANSFORMERS
			if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 116)
			{
				// version prior 116
				bool someFlag;
				Ar << someFlag;
				if (!someFlag) return Ar;
				// else - continue serialization of remaining fields
			}
#	endif // TRANSFORMERS
			if (Ar.ArVer >= 322) Ar << E.NetObjectCount << E.Guid;
#	if ARMYOF2
			if (Ar.Game == GAME_ArmyOf2) return Ar;
#	endif
			if (Ar.ArVer >= 475) Ar << E.U3unk6C;
#	if AA3
			if (Ar.Game == GAME_AA3)
			{
				// deobfuscate data
				E.ClassIndex   ^= AA3Obfuscator;
				E.SuperIndex   ^= AA3Obfuscator;
				E.PackageIndex ^= AA3Obfuscator;
				E.Archetype    ^= AA3Obfuscator;
				E.SerialSize   ^= AA3Obfuscator;
				E.SerialOffset ^= AA3Obfuscator;
			}
#	endif // AA3
			return Ar;
		}
#endif // UNREAL3
#if UC2
		if (Ar.Engine() == GAME_UE2X)
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
		if (Ar.Game == GAME_Pariah)
		{
			Ar << E.ObjectName << AR_INDEX(E.SuperIndex) << E.PackageIndex << E.ObjectFlags;
			Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SerialSize);
			if (E.SerialSize)
				Ar << AR_INDEX(E.SerialOffset);
			return Ar;
		}
#endif // PARIAH
#if BIOSHOCK
		if (Ar.Game == GAME_Bioshock)
		{
			int unkC, flags2, unk30;
			Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SuperIndex) << E.PackageIndex;
			if (Ar.ArVer >= 132) Ar << unkC;			// unknown
			Ar << E.ObjectName << E.ObjectFlags;
			if (Ar.ArLicenseeVer >= 40) Ar << flags2;	// qword flags
			Ar << AR_INDEX(E.SerialSize);
			if (E.SerialSize)
				Ar << AR_INDEX(E.SerialOffset);
			if (Ar.ArVer >= 130) Ar << unk30;			// unknown
			return Ar;
		}
#endif // BIOSHOCK
#if SWRC
		if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 151)
		{
			int unk0C;
			Ar << AR_INDEX(E.ClassIndex) << AR_INDEX(E.SuperIndex) << E.PackageIndex;
			if (Ar.ArVer >= 159) Ar << AR_INDEX(unk0C);
			Ar << E.ObjectName << E.ObjectFlags << E.SerialSize << E.SerialOffset;
			return Ar;
		}
#endif // SWRC
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
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 150)
		{
			short idx = I.PackageIndex;
			Ar << I.ClassPackage << I.ClassName << idx << I.ObjectName;
			I.PackageIndex = idx;
			return Ar;
		}
#endif // UC2
#if PARIAH
		if (Ar.Game == GAME_Pariah)
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
	char					Filename[256];			// full name with path and extension
	char					Name[64];				// short name
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

	int FindExport(const char *name, const char *className = NULL, int firstIndex = 0)
	{
		for (int i = firstIndex; i < Summary.ExportCount; i++)
		{
			const FObjectExport &Exp = ExportTable[i];
			// compare object name
			if (strcmp(Exp.ObjectName, name) != 0)
				continue;
			// if class name specified - compare it too
			const char *foundClassName = GetObjectName(Exp.ClassIndex);
			if (className && strcmp(foundClassName, className) != 0)
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
		guard(UnPackage::SerializeFName);

		assert(IsLoading);
#if BIOSHOCK
		if (Game == GAME_Bioshock)
		{
			*this << AR_INDEX(N.Index) << N.ExtraIndex;
			N.Str = GetName(N.Index);
			N.AppendIndexBio();
			return *this;
		}
		else
#endif // BIOSHOCK
#if UC2
		if (Engine() == GAME_UE2X && ArVer >= 145)
			*this << N.Index;
		else
#endif // UC2
#if UNREAL3
		if (Engine() >= GAME_UE3)
		{
			*this << N.Index;
	#if R6VEGAS
			if (Game == GAME_R6Vegas2)
			{
				N.ExtraIndex = N.Index >> 19;
				N.Index &= 0x7FFFF;
			}
	#endif // R6VEGAS
			if (ArVer >= 343) *this << N.ExtraIndex;
		}
		else
#endif // UNREAL3
			*this << AR_INDEX(N.Index);
		N.Str = GetName(N.Index);
#if UNREAL3
		N.AppendIndex();
#endif
		return *this;

		unguardf(("pos=%08X", Tell()));
	}

	virtual FArchive& operator<<(UObject *&Obj)
	{
		guard(UnPackage::SerializeUObject);

		assert(IsLoading);
		int index;
#if UC2
		if (Engine() == GAME_UE2X && ArVer >= 145)
			*this << index;
		else
#endif
#if UNREAL3
		if (Engine() >= GAME_UE3)
			*this << index;
		else
#endif // UNREAL3
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

		unguard;
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
