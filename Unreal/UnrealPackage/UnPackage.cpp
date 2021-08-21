#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"
#include "UnPackageUE3Reader.h"
#include "UE4Version.h"			// for VER_UE4_NON_OUTER_PACKAGE_IMPORT

#include "GameDatabase.h"		// for GetGameTag()

//#define PROFILE_PACKAGE_TABLES	1

/*-----------------------------------------------------------------------------
	Unreal package structures
-----------------------------------------------------------------------------*/

bool FPackageFileSummary::Serialize(FArchive &Ar)
{
	guard(FPackageFileSummary<<);
	assert(Ar.IsLoading);						// saving is not supported

	// read package tag
	Ar << Tag;

	// some games has special tag constants
#if SPECIAL_TAGS
	if (Tag == 0x9E2A83C2) goto tag_ok;			// Killing Floor
	if (Tag == 0x7E4A8BCA) goto tag_ok;			// iStorm
#endif // SPECIAL_TAGS
#if NURIEN
	if (Tag == 0xA94E6C81) goto tag_ok;			// Nurien
#endif
#if BATTLE_TERR
	if (Tag == 0xA1B2C93F)
	{
		Ar.Game = GAME_BattleTerr;
		goto tag_ok;							// Battle Territory Online
	}
#endif // BATTLE_TERR
#if LOCO
	if (Tag == 0xD58C3147)
	{
		Ar.Game = GAME_Loco;
		goto tag_ok;							// Land of Chaos Online
	}
#endif // LOCO
#if BERKANIX
	if (Tag == 0xF2BAC156)
	{
		Ar.Game = GAME_Berkanix;
		goto tag_ok;
	}
#endif // BERKANIX
#if HAWKEN
	if (Tag == 0xEA31928C)
	{
		Ar.Game = GAME_Hawken;
		goto tag_ok;
	}
#endif // HAWKEN
#if TAO_YUAN
	if (Tag == 0x12345678)
	{
		int tmp;			// some additional version?
		Ar << tmp;
		Ar.Game = GAME_TaoYuan;
		if (!GForceGame) GForceGame = GAME_TaoYuan;
		goto tag_ok;
	}
#endif // TAO_YUAN
#if STORMWAR
	if (Tag == 0xEC201133)
	{
		byte Count;
		Ar << Count;
		Ar.Seek(Ar.Tell() + Count);
		goto tag_ok;
	}
#endif // STORMWAR
#if GUNLEGEND
	if (Tag == 0x879A4B41)
	{
		Ar.Game = GAME_GunLegend;
		if (!GForceGame) GForceGame = GAME_GunLegend;
		goto tag_ok;
	}
#endif // GUNLEGEND
#if MMH7
	if (Tag == 0x4D4D4837)
	{
		Ar.Game = GAME_UE3;	// version conflict with Guilty Gear Xrd
		goto tag_ok;		// Might & Magic Heroes 7
	}
#endif // MMH7
#if DEVILS_THIRD
	if (Tag == 0x7BC342F0)
	{
		Ar.Game = GAME_DevilsThird;
		if (!GForceGame) GForceGame = GAME_DevilsThird;
		goto tag_ok;		// Devil's Third
	}
#endif // DEVILS_THIRD

	// support reverse byte order
	if (Tag != PACKAGE_FILE_TAG)
	{
		if (Tag != PACKAGE_FILE_TAG_REV)
		{
			UnPackage* file = Ar.CastTo<UnPackage>();
			appNotify("Wrong package tag (%08X) in file %s. Probably the file is encrypted.", Tag, file ? *file->GetFilename() : "(unknown)");
			return false;
		}
		Ar.ReverseBytes = true;
		Tag = PACKAGE_FILE_TAG;
	}

	// read version
tag_ok:
	unsigned int Version;
	Ar << Version;

#if UNREAL4
	// UE4 has negative version value, growing from -1 towards negative direction. This value is followed
	// by "UE3 Version", "UE4 Version" and "Licensee Version" (parsed in SerializePackageFileSummary4).
	// The value is used as some version for package header, and it's not changed frequently. We can't
	// expect these values to have large values in the future. The code below checks this value for
	// being less than zero, but allows UE1-UE3 LicenseeVersion up to 32767.
	if ((Version & 0xFFFFF000) == 0xFFFFF000)
	{
		LegacyVersion = Version;
		Ar.Game = GAME_UE4_BASE;
		Serialize4(Ar);
		//!! note: UE4 requires different DetectGame way, perhaps it's not possible at all
		//!! (but can use PAK file names for game detection)
		return true;
	}
#endif // UNREAL4

#if UNREAL3
	if (Version == PACKAGE_FILE_TAG || Version == 0x20000)
		appError("Fully compressed package header?");
#endif // UNREAL3

	FileVersion     = Version & 0xFFFF;
	LicenseeVersion = Version >> 16;
	// store file version to archive (required for some structures, for UNREAL3 path)
	Ar.ArVer         = FileVersion;
	Ar.ArLicenseeVer = LicenseeVersion;
	// detect game
	Ar.DetectGame();
	Ar.OverrideVersion();

	// read other fields

#if UNREAL3
	if (Ar.Game >= GAME_UE3)
		Serialize3(Ar);
	else
#endif
		Serialize2(Ar);

#if DEBUG_PACKAGE
	appPrintf("EngVer:%d CookVer:%d CompF:%d CompCh:%d\n", EngineVersion, CookerVersion, CompressionFlags, CompressedChunks.Num());
	appPrintf("Names:%X[%d] Exports:%X[%d] Imports:%X[%d]\n", NameOffset, NameCount, ExportOffset, ExportCount, ImportOffset, ImportCount);
	appPrintf("HeadersSize:%X Group:%s DependsOffset:%X U60:%X\n", HeadersSize, *PackageGroup, DependsOffset, U3unk60);
#endif

	return true;

	unguardf("Ver=%d/%d", FileVersion, LicenseeVersion);
}

FArchive& operator<<(FArchive &Ar, FObjectExport &E)
{
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		E.Serialize4(Ar);
		return Ar;
	}
#endif
#if UNREAL3
	if (Ar.Game >= GAME_UE3)
	{
		E.Serialize3(Ar);
		return Ar;
	}
#endif
#if UC2
	if (Ar.Engine() == GAME_UE2X)
	{
		E.Serialize2X(Ar);
		return Ar;
	}
#endif
	E.Serialize2(Ar);
	return Ar;
}


void FObjectImport::Serialize(FArchive& Ar)
{
	guard(FObjectImport::Serialize);

#if USE_COMPACT_PACKAGE_STRUCTS
	FName		ClassPackage;
#endif

#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 150)
	{
		int16 idx = PackageIndex;
		Ar << ClassPackage << ClassName << idx << ObjectName;
		PackageIndex = idx;
		return;
	}
#endif // UC2
#if PARIAH
	if (Ar.Game == GAME_Pariah)
		return Ar << I.PackageIndex << I.ObjectName << I.ClassPackage << I.ClassName;
#endif
#if AA2
	if (Ar.Game == GAME_AA2)
	{
		byte unk;	// serialized length of ClassName string?
		Ar << ClassPackage << ClassName << unk << ObjectName << PackageIndex;
		return;
	}
#endif

	// this code is the same for all engine versions
	Ar << ClassPackage << ClassName << PackageIndex << ObjectName;

#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE && Ar.ArVer >= VER_UE4_NON_OUTER_PACKAGE_IMPORT && Ar.ContainsEditorData())
	{
		FName PackageName;
		Ar << PackageName;
	}
#endif // UNREAL4

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
	{
		// MK X
		FGuid unk;
		Ar << unk;
	}
#endif // MKVSDC

	unguard;
}


/*-----------------------------------------------------------------------------
	Package loading (creation) / unloading
-----------------------------------------------------------------------------*/

UnPackage::UnPackage(const char *filename, const CGameFileInfo* fileInfo, bool silent)
:	Loader(NULL)
#if UNREAL4
,	ExportIndices_IOS(NULL)
#endif
{
	guard(UnPackage::UnPackage);

	IsLoading = true;
	FileInfo = fileInfo;

	FArchive* baseLoader = NULL;
	if (FileInfo)
	{
		baseLoader = FileInfo->CreateReader(true);
		if (!baseLoader)
		{
			// Failed to open the file
			return;
		}
	}
	else
	{
		// The file was not registered, so duplicate the file name
		FilenameNoInfo = appStrdupPool(appSkipRootDir(filename));
	}

	Loader = CreateLoader(filename, baseLoader);
	if (!Loader)
	{
		// Bad file, can't open
		return;
	}

	SetupFrom(*Loader);

#if UNREAL4
	if (FileInfo && FileInfo->IsIOStoreFile())
	{
		// Not called (forced UE4.26):
		// - May be: UE4UnversionedPackage(verMin, verMax)
		// - Ar.DetectGame();
		this->ArVer = 0;
		this->ArLicenseeVer = 0;
		this->Game = GForceGame ? GForceGame : GAME_UE4(26); // appeared in UE4.26
		OverrideVersion();
		// Register package before loading, because it is possible that during
		// loading of import table we'll load other packages to resolve dependencies,
		// and circular dependencies are possible
		RegisterPackage(filename);
		LoadPackageIoStore();
		// Release package file handle
		CloseReader();
		if (!IsValid())
		{
			UnregisterPackage();
		}
		return;
	}
#endif // UNREAL4

	// read summary
	if (!Summary.Serialize(*this))
	{
		// Probably not a package
		CloseReader();
		return;
	}

#if PROFILE_PACKAGE_TABLES
	appResetProfiler();
#endif

	Loader->SetupFrom(*this);	// serialization of FPackageFileSummary could change some FArchive properties

#if !DEBUG_PACKAGE
	if (!silent)
#endif
	{
		// Logging. I don't want to drop it entirely because this will reduce error checking possibilities.
		// Using "\r" at the end of line won't speed up things (on Windows), so keep using "\n".
		// Combine full log message before sending to appPrintf, it works faster than doing multiple appPrintf calls.
		char LogBuffer[1024];
		int LogUsed = 0;
	#define ADD_LOG(...) LogUsed += appSprintf(LogBuffer + LogUsed, ARRAY_COUNT(LogBuffer) - LogUsed,  __VA_ARGS__);
		ADD_LOG("Loading package: %s Ver: %d/%d ", *GetFilename(), Loader->ArVer, Loader->ArLicenseeVer);
			// don't use 'Summary.FileVersion, Summary.LicenseeVersion' because UE4 has overrides for unversioned packages
#if UNREAL3
		if (Game >= GAME_UE3)
		{
			ADD_LOG("Engine: %d ", Summary.EngineVersion);
			FUE3ArchiveReader* UE3Loader = Loader->CastTo<FUE3ArchiveReader>();
			if (UE3Loader && UE3Loader->IsFullyCompressed)
				ADD_LOG("[FullComp] ");
		}
#endif // UNREAL3
#if UNREAL4
		if (Game >= GAME_UE4_BASE && Summary.IsUnversioned)
			ADD_LOG("[Unversioned] ");
#endif // UNREAL4
		ADD_LOG("Names: %d Exports: %d Imports: %d Game: %X\n", Summary.NameCount, Summary.ExportCount, Summary.ImportCount, Game);
		// Flush text
	#undef ADD_LOG
		PKG_LOG("%s", LogBuffer);
	}

#if DEBUG_PACKAGE
	appPrintf("Flags: %X, Name offset: %X, Export offset: %X, Import offset: %X\n", Summary.PackageFlags, Summary.NameOffset, Summary.ExportOffset, Summary.ImportOffset);
	for (int i = 0; i < Summary.CompressedChunks.Num(); i++)
	{
		const FCompressedChunk &ch = Summary.CompressedChunks[i];
		appPrintf("chunk[%d]: comp=%X+%X, uncomp=%X+%X\n", i, ch.CompressedOffset, ch.CompressedSize, ch.UncompressedOffset, ch.UncompressedSize);
	}
#endif // DEBUG_PACKAGE

	//?? TODO: it is not "replacing", it's WRAPPING around existing loader
	ReplaceLoader();

#if UNREAL3
	//?? TODO: could be part of ReplaceLoader()
	if (Game >= GAME_UE3 && Summary.CompressionFlags && Summary.CompressedChunks.Num())
	{
		FUE3ArchiveReader* UE3Loader = Loader->CastTo<FUE3ArchiveReader>();
		if (UE3Loader && UE3Loader->IsFullyCompressed)
			appError("Fully compressed package %s has additional compression table", filename);
		// replace Loader with special reader for compressed UE3 archives
		Loader = new FUE3ArchiveReader(Loader, Summary.CompressionFlags, Summary.CompressedChunks);
	}
#endif // UNREAL3

	LoadNameTable();
	LoadImportTable();
	LoadExportTable();

#if UNREAL4
	// Process Event Driven Loader packages: such packages are split into 2 pieces: .uasset with headers
	// and .uexp with object's data. At this moment we already have FPackageFileSummary fully loaded,
	// so we can replace loader with .uexp file - with providing correct position offset.
	if (Game >= GAME_UE4_BASE && Summary.HeadersSize == Loader->GetFileSize())
	{
		guard(FindUexp);
		char buf[MAX_PACKAGE_PATH];
		appStrncpyz(buf, filename, ARRAY_COUNT(buf));
		char* s = strrchr(buf, '.');
		if (!s)
		{
			s = strchr(buf, 0);
		}
		strcpy(s, ".uexp");
		// When finding, explicitly tell to use the same folder where .uasset file exists
		const CGameFileInfo *expInfo = CGameFileInfo::Find(buf, FileInfo ? FileInfo->FolderIndex : -1);
		if (expInfo)
		{
			// Open .exp file
			FArchive* expLoader = expInfo->CreateReader();
			// Replace loader with this file, but add offset so it will work like it is part of original uasset
			delete Loader;
			Loader = new FReaderWrapper(expLoader, -Summary.HeadersSize);
		}
		else
		{
			appPrintf("WARNING: it seems package %s has missing .uexp file\n", filename);
		}
		unguard;
	}
#endif // UNREAL4

	RegisterPackage(filename);

	// Release package file handle
	CloseReader();

#if PROFILE_PACKAGE_TABLES
	appPrintProfiler("Package loaded");
#endif

	unguardf("%s, ver=%d/%d, game=%s", filename, ArVer, ArLicenseeVer, GetGameTag(Game));
}

void UnPackage::RegisterPackage(const char* filename)
{
	// Register self to package map.
	// First, strip path and extension from the name.
	char buf[MAX_PACKAGE_PATH];
	const char *s = strrchr(filename, '/');
	if (!s) s = strrchr(filename, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = filename;
	appStrncpyz(buf, s, ARRAY_COUNT(buf));
	char *s2 = strchr(buf, '.');
	if (s2) *s2 = 0;
	Name = appStrdupPool(buf);
	// ... then add 'this'
	PackageMap.Add(this);

	// Cache pointer in CGameFileInfo so next time it will be found quickly.
	if (FileInfo)
	{
		const_cast<CGameFileInfo*>(FileInfo)->Package = this;
	}
}

void UnPackage::UnregisterPackage()
{
	// Remove self from package table (it will be there even if package is not "valid")
	int i = PackageMap.FindItem(this);
	if (i != INDEX_NONE)
	{
		// Could be INDEX_NONE in a case of bad package
		PackageMap.RemoveAt(i);
	}
	// unlink package from CGameFileInfo
	if (FileInfo)
	{
		assert(FileInfo->Package == this || FileInfo->Package == NULL);
		const_cast<CGameFileInfo*>(FileInfo)->Package = NULL;
	}
}

bool UnPackage::VerifyName(FString& nameStr, int nameIndex)
{
	// Verify name, some Korean games (B&S) has garbage in FName (unicode?)
	bool goodName = true;
	int numBadChars = 0;
	for (char c : nameStr.GetDataArray())
	{
		if (c < ' ' || c > 0x7F)
		{
			if (c == 0) break; // end of line is included into FString
			// unreadable character
			goodName = false;
			break;
		}
		if (c == '$') numBadChars++;		// unicode characters replaced with '$' in FString serializer
	}
	if (goodName && numBadChars)
	{
		int nameLen = nameStr.Len();
		if (nameLen >= 64) goodName = false;
		if (numBadChars >= nameLen / 2 && nameLen > 16) goodName = false;
	}
	if (!goodName)
	{
		// replace name
		appPrintf("WARNING: %s: fixing name %d (%s)\n", *GetFilename(), nameIndex, *nameStr);
		char buf[64];
		appSprintf(ARRAY_ARG(buf), "__name_%d__", nameIndex);
		nameStr = buf;
	}
	return goodName;
}

void UnPackage::LoadNameTable()
{
	if (Summary.NameCount == 0) return;

	Seek(Summary.NameOffset);
	NameTable = new const char* [Summary.NameCount];

#if UNREAL4
	if (Game >= GAME_UE4_BASE)
	{
		LoadNameTable4();
		return;
	}
#endif
#if UNREAL3
	if (Game >= GAME_UE3)
	{
		LoadNameTable3();
		return;
	}
#endif
	LoadNameTable2();
}

void UnPackage::LoadImportTable()
{
	guard(UnPackage::LoadImportTable);

	if (Summary.ImportCount == 0) return;

	Seek(Summary.ImportOffset);
	FObjectImport *Imp = ImportTable = new FObjectImport[Summary.ImportCount];
	for (int i = 0; i < Summary.ImportCount; i++, Imp++)
	{
		Imp->Serialize(*this);
#if DEBUG_PACKAGE
		PKG_LOG("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName);
#endif
	}

	unguard;
}

// Game-specific de-obfuscation of export tables
void PatchBnSExports(FObjectExport *Exp, const FPackageFileSummary &Summary);
void PatchDunDefExports(FObjectExport *Exp, const FPackageFileSummary &Summary);

void UnPackage::LoadExportTable()
{
	// load exports table
	guard(UnPackage::LoadExportTable);

	if (Summary.ExportCount == 0) return;

	Seek(Summary.ExportOffset);
	FObjectExport *Exp = ExportTable = new FObjectExport[Summary.ExportCount];
	memset(ExportTable, 0, sizeof(FObjectExport) * Summary.ExportCount);
	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		*this << *Exp;
	}

#if BLADENSOUL
	if (Game == GAME_BladeNSoul && (Summary.PackageFlags & 0x08000000))
		PatchBnSExports(ExportTable, Summary);
#endif
#if DUNDEF
	if (Game == GAME_DunDef)
		PatchDunDefExports(ExportTable, Summary);
#endif

#if DEBUG_PACKAGE
	Exp = ExportTable;
	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
//		USE_COMPACT_PACKAGE_STRUCTS - makes impossible to dump full information
//		Perhaps add full support to extract.exe?
//		PKG_LOG("Export[%d]: %s'%s' offs=%08X size=%08X parent=%d flags=%08X:%08X, exp_f=%08X arch=%d\n", i, GetClassNameFor(*Exp),
//			*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->PackageIndex, Exp->ObjectFlags2, Exp->ObjectFlags, Exp->ExportFlags, Exp->Archetype);
		PKG_LOG("Export[%d]: %s'%s' offs=%08X size=%08X parent=%d  exp_f=%08X\n", i, GetClassNameFor(*Exp),
			*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->PackageIndex, Exp->ExportFlags);
	}
#endif // DEBUG_PACKAGE

	unguard;
}


UnPackage::~UnPackage()
{
	guard(UnPackage::~UnPackage);

	UnregisterPackage();

	if (Loader) delete Loader;

	if (!IsValid())
	{
		// The package wasn't loaded, nothing to release in destructor. Also it is possible that
		// it has zero names/imports/exports (happens with some UE3 games).
		return;
	}

	// free tables
	delete[] NameTable;
	delete[] ImportTable;
	delete[] ExportTable;
#if UNREAL4
	delete[] ExportIndices_IOS;
#endif

	unguard;
}

/*static*/ void UnPackage::UnloadPackage(UnPackage* package)
{
	if (package)
	{
		delete package;
	}
}


#if 0
// Commented, not used
// Find file archive inside a package loader
static FFileArchive* FindFileArchive(FArchive* Ar)
{
#if UNREAL3
	FUE3ArchiveReader* ArUE3 = Ar->CastTo<FUE3ArchiveReader>();
	if (ArUE3) Ar = ArUE3->Reader;
#endif

	FReaderWrapper* ArWrap = Ar->CastTo<FReaderWrapper>();
	if (ArWrap) Ar = ArWrap->Reader;

	return Ar->CastTo<FFileArchive>();
}
#endif

static TArray<UnPackage*> OpenReaders;

void UnPackage::SetupReader(int ExportIndex)
{
	guard(UnPackage::SetupReader);
	// open loader if it is closed
	if (!IsOpen())
	{
		Open();
		if (OpenReaders.Num() == 0)
		{
			OpenReaders.Empty(64);
		}
		OpenReaders.AddUnique(this);
	}
	// setup for object
	const FObjectExport &Exp = GetExport(ExportIndex);

#if UNREAL4
	if (Exp.RealSerialOffset)
	{
		FReaderWrapper* Wrapper = Loader->CastTo<FReaderWrapper>();
		Wrapper->ArPosOffset = Exp.RealSerialOffset - Exp.SerialOffset;
	}
#endif // UNREAL4

	SetStopper(Exp.SerialOffset + Exp.SerialSize);
//	appPrintf("Setup for %s: %d + %d -> %d\n", *Exp.ObjectName, Exp.SerialOffset, Exp.SerialSize, Exp.SerialOffset + Exp.SerialSize);
	Seek(Exp.SerialOffset);
	unguard;
}

void UnPackage::CloseReader()
{
	guard(UnPackage::CloseReader);
#if 0
	FFileArchive* File = FindFileArchive(Loader);
	assert(File);
	if (File->IsOpen()) File->Close();
#else
	Loader->Close();
#endif
	unguardf("pkg=%s", *GetFilename());
}

void UnPackage::CloseAllReaders()
{
	guard(UnPackage::CloseAllReaders);

	for (UnPackage* p : OpenReaders)
	{
		p->CloseReader();
	}
	OpenReaders.Empty();

	unguard;
}


/*-----------------------------------------------------------------------------
	UObject* and FName serializers
-----------------------------------------------------------------------------*/

FArchive& UnPackage::operator<<(FName &N)
{
	guard(UnPackage::SerializeFName);

	assert(IsLoading);

	// Declare aliases for FName.Index and ExtraIndex to allow USE_COMPACT_PACKAGE_STRUCTS to work
#if !USE_COMPACT_PACKAGE_STRUCTS
	int32& N_Index = N.Index;
	#if UNREAL3 || UNREAL4
	int32& N_ExtraIndex = N.ExtraIndex;
	#endif
#else
	int32 N_Index = 0, N_ExtraIndex = 0;
#endif // USE_COMPACT_PACKAGE_STRUCTS

#if BIOSHOCK
	if (Game == GAME_Bioshock)
	{
		*this << AR_INDEX(N_Index) << N_ExtraIndex;
		if (N_ExtraIndex == 0)
		{
			N.Str = GetName(N_Index);
		}
		else
		{
			N.Str = appStrdupPool(va("%s%d", GetName(N_Index), N_ExtraIndex-1));	// without "_" char
		}
		return *this;
	}
#endif // BIOSHOCK

#if UC2
	if (Engine() == GAME_UE2X && ArVer >= 145)
	{
		*this << N_Index;
	}
	else
#endif // UC2
#if LEAD
	if (Game == GAME_SplinterCellConv && ArVer >= 64)
	{
		*this << N_Index;
	}
	else
#endif // LEAD
#if UNREAL3 || UNREAL4
	if (Engine() >= GAME_UE3)
	{
		*this << N_Index;
		if (Game >= GAME_UE4_BASE) goto extra_index;
	#if R6VEGAS
		if (Game == GAME_R6Vegas2)
		{
			N_ExtraIndex = N_Index >> 19;
			N_Index &= 0x7FFFF;
		}
	#endif // R6VEGAS
		if (ArVer >= 343)
		{
		extra_index:
			*this << N_ExtraIndex;
		}
	}
	else
#endif // UNREAL3 || UNREAL4
	{
		// UE1 and UE2
		*this << AR_INDEX(N_Index);
	}

	// Convert name index to string
#if UNREAL3 || UNREAL4
	if (N_ExtraIndex == 0)
	{
		N.Str = GetName(N_Index);
	}
	else
	{
		N.Str = appStrdupPool(va("%s_%d", GetName(N_Index), N_ExtraIndex-1));
	}
#else
	// no modern engines compiled
	N.Str = GetName(N_Index);
#endif // UNREAL3 || UNREAL4

	return *this;

	unguardf("pos=%08X", Tell());
}

FArchive& UnPackage::operator<<(UObject *&Obj)
{
	guard(UnPackage::SerializeUObject);

	assert(IsLoading);
	int index;
#if UC2
	if (Engine() == GAME_UE2X && ArVer >= 145)
		*this << index;
	else
#endif
#if UNREAL3 || UNREAL4
	if (Engine() >= GAME_UE3)
		*this << index;
	else
#endif // UNREAL3 || UNREAL4
		*this << AR_INDEX(index);

	if (index < 0)
	{
//		const FObjectImport &Imp = GetImport(-index-1);
//		appPrintf("PKG: Import[%s,%d] OBJ=%s CLS=%s\n", GetObjectName(Imp.PackageIndex), index, *Imp.ObjectName, *Imp.ClassName);
		Obj = CreateImport(-index-1);
	}
	else if (index > 0)
	{
//		const FObjectExport &Exp = GetExport(index-1);
//		appPrintf("PKG: Export[%d] OBJ=%s CLS=%s\n", index, *Exp.ObjectName, GetClassNameFor(Exp));
		Obj = CreateExport(index-1);
	}
	else // index == 0
	{
		Obj = NULL;
	}
	return *this;

	unguard;
}

/*-----------------------------------------------------------------------------
	Loading particular import or export package entry
-----------------------------------------------------------------------------*/

int UnPackage::FindExport(const char *name, const char *className, int firstIndex) const
{
	guard(UnPackage::FindExport);

	FastNameComparer cmp(name);
	for (int i = firstIndex; i < Summary.ExportCount; i++)
	{
		const FObjectExport &Exp = ExportTable[i];
		// compare object name
		if (cmp(Exp.ObjectName))
		{
			// if class name specified - compare it too
			const char* foundClassName = GetClassNameFor(Exp);
			if (className && stricmp(foundClassName, className) != 0)
				continue;
			return i;
		}
	}
	return INDEX_NONE;

	unguard;
}


bool UnPackage::CompareObjectPaths(int PackageIndex, UnPackage *RefPackage, int RefPackageIndex) const
{
	guard(UnPackage::CompareObjectPaths);

/*	appPrintf("Compare %s.%s [%d] with %s.%s [%d]\n",
		Name, GetObjectName(PackageIndex), PackageIndex,
		RefPackage->Name, RefPackage->GetObjectName(RefPackageIndex), RefPackageIndex
	); */

	while (PackageIndex || RefPackageIndex)
	{
		const char *PackageName, *RefPackageName;

		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else if (PackageIndex > 0)
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
			PackageName  = Name;

		if (RefPackageIndex < 0)
		{
			const FObjectImport &Rec = RefPackage->GetImport(-RefPackageIndex-1);
			RefPackageIndex = Rec.PackageIndex;
			RefPackageName  = Rec.ObjectName;
		}
		else if (RefPackageIndex > 0)
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = RefPackage->GetExport(RefPackageIndex-1);
			RefPackageIndex = Rec.PackageIndex;
			RefPackageName  = Rec.ObjectName;
		}
		else
			RefPackageName  = RefPackage->Name;
//		appPrintf("%20s -- %20s\n", PackageName, RefPackageName);
		if (stricmp(RefPackageName, PackageName) != 0) return false;
	}

	return true;

	unguard;
}


int UnPackage::FindExportForImport(const char *ObjectName, const char *ClassName, UnPackage *ImporterPackage, int ImporterIndex)
{
	guard(FindExportForImport);

	int ObjIndex = -1;
	while (true)
	{
		// iterate all objects with the same name and class
		ObjIndex = FindExport(ObjectName, ClassName, ObjIndex + 1);
		if (ObjIndex == INDEX_NONE)
			break;				// not found
#if UNREAL4
		if (Game >= GAME_UE4_BASE)
		{
			// UE4 usually has single object in package. Plus, each object import has a parent UPackage
			// describing where to get an object, but object export has no parent UObject - so depth
			// of import path is 1 step deeper than depth of export path, and CompareObjectPaths()
			// will always fail.
			return ObjIndex;
		}
#endif // UNREAL4
		// a few objects in package could have the same name and class but resides in different groups,
		// so compare full object paths for sure
		if (CompareObjectPaths(ObjIndex+1, ImporterPackage, -1-ImporterIndex))
			return ObjIndex;	// found
	}

	return INDEX_NONE;			// not found

	unguard;
}


UObject* UnPackage::CreateExport(int index)
{
	guard(UnPackage::CreateExport);

	// Get previously created object if any
	FObjectExport& Exp = GetExport(index);
	if (Exp.Object)
		return Exp.Object;


	// Check if this object just contains default properties
	bool shouldSkipObject = false;

	if (!strnicmp(Exp.ObjectName, "Default__", 9))
	{
		// Default properties are not supported -- this is a clean UObject format
		shouldSkipObject = true;
	}
#if UNREAL4
	if (Game >= GAME_UE4_BASE)
	{
		// This could be a blueprint - it contains objects which are marked as 'StaticMesh' class,
		// but really containing nothing. Such objects are always contained inside some Default__... parent.
		const char* OuterName = GetObjectPackageName(Exp.PackageIndex);
		if (OuterName && !strnicmp(OuterName, "Default__", 9))
		{
			shouldSkipObject = true;
		}
	}
#endif // UNREAL4

	if (shouldSkipObject)
	{
		return NULL;
	}

	// Create empty object of desired class
	const char* ClassName = GetClassNameFor(Exp);
	UObject* Obj = Exp.Object = CreateClass(ClassName);
	if (!Obj)
	{
		if (!IsSuppressedClass(ClassName))
		{
			appPrintf("WARNING: Unknown class \"%s\" for object \"%s\"\n", ClassName, *Exp.ObjectName);
		}
#if MAX_DEBUG
		else
		{
			appPrintf("SUPPRESSED: %s\n", ClassName);
		}
#endif
		return NULL;
	}

#if UNREAL3
	// For UE3 we may require finding object in another package
	if (Game >= GAME_UE3 && (Exp.ExportFlags & EF_ForcedExport)) // ExportFlags appeared in ArVer=247
	{
		// find outermost package
		if (Exp.PackageIndex)
		{
			int PackageIndex = Exp.PackageIndex - 1;			// subtract 1, because 0 = no parent
			while (true)
			{
				const FObjectExport &Exp2 = GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;
				PackageIndex = Exp2.PackageIndex - 1;			// subtract 1 ...
			}
/*!! -- should display this message during serialization in UObject::EndLoad()
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			assert(Exp2.ExportFlags & EF_ForcedExport);
			const char *PackageName = Exp2.ObjectName;
			appPrintf("Forced export (%s): %s'%s.%s'\n", Name, ClassName, PackageName, *Exp.ObjectName);
*/
		}
	}
#endif // UNREAL3

	// Setup constant object fields
	Obj->Package      = this;
	Obj->PackageIndex = index;
	Obj->Outer        = NULL;
	Obj->Name         = Exp.ObjectName;

	bool bLoad = true;
	if (GBeforeLoadObjectCallback)
		bLoad = GBeforeLoadObjectCallback(Obj);

	if (bLoad)
	{
		// Block UObject serialization
		UObject::BeginLoad();

		// Find and try to create outer object
		UObject* Outer = NULL;
		if (Exp.PackageIndex)
		{
			const FObjectExport &OuterExp = GetExport(Exp.PackageIndex - 1);
			Outer = OuterExp.Object;
			if (!Outer)
			{
				const char* OuterClassName = GetClassNameFor(OuterExp);
				if (IsKnownClass(OuterClassName))			// avoid error message if class name is not registered
					Outer = CreateExport(Exp.PackageIndex - 1);
			}
		}
		Obj->Outer = Outer;

		// Add object to GObjLoaded for later serialization
		UObject::GObjLoaded.Add(Obj);

		// Perform serialization
		UObject::EndLoad();
	}

	return Obj;

	unguardf("%s:%d", *GetFilename(), index);
}


UObject* UnPackage::CreateImport(int index)
{
	guard(UnPackage::CreateImport);

	FObjectImport &Imp = GetImport(index);
	if (Imp.Missing) return NULL;	// error message already displayed for this entry

	// load package
	const char* PackageName = GetObjectPackageName(Imp.PackageIndex);
#if UNREAL4
	if (!PackageName)
	{
		// This could happen with UE4 IoStore structures, if PackageId was not found
		return NULL;
	}
#endif
	UnPackage *Package = LoadPackage(PackageName);
	int ObjIndex = INDEX_NONE;

	if (Package)
	{
		// has package with exactly this name - so this is either UE < 3 or non-cooked UE3 export
		ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
		if (ObjIndex == INDEX_NONE)
		{
			appPrintf("WARNING: Import(%s) was not found in package %s\n", *Imp.ObjectName, PackageName);
			Imp.Missing = true;
			return NULL;
		}
	}
#if UNREAL3
	// try to find import in startup package
	else if (Engine() >= GAME_UE3 && Engine() < GAME_UE4_BASE)
	{
		// check startup package
		Package = LoadPackage(GStartupPackage);
		if (Package)
			ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
		// look in other loaded packages
		if (ObjIndex == INDEX_NONE)
		{
			//?? speedup this search when many packages are loaded (not tested, perhaps works well enough)
			UnPackage *SkipPackage = Package;	// Package = either startup package or NULL
			for (int i = 0; i < PackageMap.Num(); i++)
			{
				Package = PackageMap[i];
				//appPrintf("check for %s in %s\n", *Imp.ObjectName, Package->Name); - don't spam, could slowdown when many packages loaded!
				if (Package == this || Package == SkipPackage)
					continue;		// already checked
				ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
				if (ObjIndex != INDEX_NONE)
				{
					if (i > 32)
					{
						// This is definitely a startup package, reorder PackageMap to pick it up faster next time
						PackageMap.RemoveAtSwap(i);
						PackageMap.Insert(Package, 0);
					}
					break;			// found
				}
			}
		}
		if (ObjIndex == INDEX_NONE)
		{
			appPrintf("WARNING: Import(%s.%s) was not found\n", PackageName, *Imp.ObjectName);
			Imp.Missing = true;
			return NULL;
		}
	}
#endif // UNREAL3

	// at this point we have either Package == NULL (not found) or Package != NULL and ObjIndex is valid

	if (!Package)
	{
		Imp.Missing = true;
#if UNREAL4
		if (!strnicmp(PackageName, "/Script/", 8) || !strnicmp(PackageName, "/Engine/", 8))
		{
			// Ignore missing engine packages for UE4
			return NULL;
		}
#endif
		appPrintf("WARNING: Import(%s'%s'): package %s was not found\n", *Imp.ClassName, *Imp.ObjectName, PackageName);
		return NULL;
	}

	// create object
	return Package->CreateExport(ObjIndex);

	unguardf("%s:%d", *GetFilename(), index);
}


// get outermost package name
//?? this function is not correct, it is used in package exporter tool only
const char *UnPackage::GetObjectPackageName(int PackageIndex) const
{
	guard(UnPackage::GetObjectPackageName);

	const char *PackageName = NULL;
	while (PackageIndex)
	{
		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
	}
	return PackageName;

	unguard;
}


void UnPackage::GetFullExportName(const FObjectExport &Exp, char *buf, int bufSize, bool IncludeObjectName, bool IncludeCookedPackageName) const
{
	guard(UnPackage::GetFullExportNameBase);

	const char *PackageNames[256];
	const char *PackageName;
	int NestLevel = 0;

	// get object name
	if (IncludeObjectName)
	{
		PackageName = Exp.ObjectName;
		PackageNames[NestLevel++] = PackageName;
	}

	// gather nested package names (object parents)
	int PackageIndex = Exp.PackageIndex;
	while (PackageIndex)
	{
		assert(NestLevel < ARRAY_COUNT(PackageNames));
		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
		{
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
#if UNREAL3
			if (PackageIndex == 0 && (Rec.ExportFlags && EF_ForcedExport) && !IncludeCookedPackageName)
				break;		// do not add cooked package name
#endif
		}
		PackageNames[NestLevel++] = PackageName;
	}
	// concatenate package names in reverse order (from root to object)
	*buf = 0;
	for (int i = NestLevel-1; i >= 0; i--)
	{
		const char *PackageName = PackageNames[i];
		char *dst = strchr(buf, 0);
		appSprintf(dst, bufSize - (dst - buf), "%s%s", PackageName, i > 0 ? "." : "");
	}

	unguard;
}


const char *UnPackage::GetUncookedPackageName(int PackageIndex) const
{
	guard(UnPackage::GetUncookedPackageName);

#if UNREAL3
	if (PackageIndex != INDEX_NONE)
	{
		const FObjectExport &Exp = GetExport(PackageIndex);
		if (Game >= GAME_UE3 && (Exp.ExportFlags & EF_ForcedExport))
		{
			// find outermost package
			while (true)
			{
				const FObjectExport &Exp2 = GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;			// get parent (UPackage)
				PackageIndex = Exp2.PackageIndex - 1;	// subtract 1 from package index
			}
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			return *Exp2.ObjectName;
		}
	}
#endif // UNREAL3
	return Name;

	unguard;
}


/*-----------------------------------------------------------------------------
	Searching for package and maintaining package list
-----------------------------------------------------------------------------*/

TArray<UnPackage*>	UnPackage::PackageMap;
TArray<char*>		MissingPackages;

/*static*/ UnPackage *UnPackage::LoadPackage(const char *Name, bool silent)
{
	guard(UnPackage::LoadPackage(name));

	const char *LocalName = appSkipRootDir(Name);

	// Call CGameFileInfo::Find() first. This function is fast because it uses
	// hashing internally.
	const CGameFileInfo *info = CGameFileInfo::Find(LocalName);

	int i;

	if (info)
	{
		return LoadPackage(info, silent);
	}
	else
	{
		// The file was not found in registered game files. Probably the file name
		// was specified fully qualified, with full path name, outside of root game path.
		// This is rare situation, so we can allow a bit unoptimized code here - linear search
		// for package inside a PackageMap array.

		// Check in missing package names. This check will allow to print "missing package"
		// warning only once.
		for (i = 0; i < MissingPackages.Num(); i++)
			if (!stricmp(LocalName, MissingPackages[i]))
				return NULL;
		// Check in loaded packages list. This is done to prevent loading the same package
		// twice when this function is called with a different filename qualifiers:
		// "path/package.ext", "package.ext", "package"
		for (i = 0; i < PackageMap.Num(); i++)
			if (!stricmp(LocalName, *PackageMap[i]->GetFilename()))
				return PackageMap[i];

		// Try to load package using file name.
		if (appFileExists(Name))
		{
			UnPackage* package = new UnPackage(Name, NULL, silent);
			if (!package->IsValid())
			{
				delete package;
				return NULL;
			}
			return package;
		}
		MissingPackages.Add(appStrdup(LocalName));
	}

	// The package is missing. Do not print any warnings: missing package is a normal situation
	// in UE3 cooked builds, so print warnings when needed at upper level.
//	appPrintf("WARNING: package %s was not found\n", Name);
	return NULL;

	unguardf("%s", Name);
}

/*static*/ UnPackage *UnPackage::LoadPackage(const CGameFileInfo* File, bool silent)
{
	guard(UnPackage::LoadPackage(info));
	PROFILE_LABEL(*File->GetRelativeName());

	if (File->IsPackage())
	{
		// Check if package was already loaded.
		if (File->Package)
			return File->Package;
		// Load the package with providing 'File' to constructor.
		UnPackage* package = new UnPackage(*File->GetRelativeName(), File, silent);
		if (!package->IsValid())
		{
			delete package;
			return NULL;
		}
		return package;
	}
	return NULL;

	unguardf("%s", *File->GetRelativeName());
}
