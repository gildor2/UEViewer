#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#if GEARS4

struct FGears4AssetEntry
{
	FString AssetName;
	int32 AssetSize;
	int32 p2, p3;			// p2 -> Array1 index
	uint8 p4;

	friend FArchive& operator<<(FArchive& Ar, FGears4AssetEntry& E)
	{
		return Ar << E.AssetName << E.AssetSize << E.p2 << E.p3 << E.p4;
	}
};

struct FGears4BundleItem
{
	int32 AssetIndex;
	int32 AssetSize;

	friend FArchive& operator<<(FArchive& Ar, FGears4BundleItem& E)
	{
		return Ar << E.AssetIndex << E.AssetSize;
	}
};

SIMPLE_TYPE(FGears4BundleItem, int32)

struct FGears4Bundle
{
	int32 SomeNum;				// -> index at Assets (note: Assets.X is the same, but array!)
	TArray<FGears4BundleItem> Assets;	// TArray { int1,int2 }: int1 -> index at Assets, int2 = AssetSize
	TArray<int32> p2;

	int32 GetBundleSize() const
	{
		int32 size = 0;
		for (int i = 0; i < Assets.Num(); i++)
			size += Assets[i].AssetSize;
		return size;
	}

	friend FArchive& operator<<(FArchive& Ar, FGears4Bundle& E)
	{
		Ar << E.SomeNum;
		Ar << E.Assets;
		Ar << E.p2;
		return Ar;
	}
};

struct FGears4Manifest
{
	// same size of arrays
	TArray<FGears4AssetEntry> Assets;
	TArray<FIntPoint> Array4;	// TArray<bool32,int32>: int32 -> first bundle where asset appears

	// same size of arrays
	TArray<FGears4Bundle> Bundles;
	TArray<byte> Array3;

	TArray<int32> Array1;		// -> Assets index
	TArray<int32> Array2;		// ? -> Assets index

	int64 p1;
	int64 p2;

	int Serialize(FArchive& Ar)
	{
		guard(FGears4Manifest::Serialize);

		// Header
		uint32 Magic;
		int32 Version;
		Ar << Magic << Version;
		if (Magic != 0x4C444E42)
			appError("Wrong Gears4 manifest magic");
		if (Version != 3)
			return 1;

		// Asset list
		uint32 Magic2;
		int32 Version2;
		Ar << Magic2 << Version2;
		if (Magic2 != 0xABAD1DEA)
			return 2;
		if (Version2 != 3)
			return 3;

		Ar << Assets;
		Ar << Array1;
		Ar << Array2;

		Ar << Bundles;

		if (Version >= 1)
		{
			Ar << Array3;
		}
		if (Version >= 2)
		{
			Ar << p1 << p2;
		}

		Array4.AddUninitialized(Assets.Num());
		for (int i = 0; i < Array4.Num(); i++)
		{
			Ar << Array4[i];
		}

		return 0;

		unguard;
	}
};


struct FGears4BundledInfo
{
	// File name. Could be obtained from linked CGameFileInfo, but this makes FindFile more complicated.
	const char* Name;
	// Info about bundle containing the file
	const CGameFileInfo* Container;
	// Position and size inside the bundle
	int32 Position;
	int32 Size;
	// Hashing for FindFile()
	FGears4BundledInfo*	HashNext;
};

class FGears4BundleFile : public FArchive
{
	DECLARE_ARCHIVE(FGears4BundleFile, FArchive);
public:
	FGears4BundleFile(const FGears4BundledInfo* info)
	:	Info(info)
	{
		guard(FGears4BundleFile::Constructor)
		Reader = Info->Container->CreateReader();
		assert(Reader);
		Reader->SetStopper(Info->Position + Info->Size);

		Reader->Seek(Info->Position);

		unguard;
	}

	virtual ~FGears4BundleFile()
	{
		if (Reader) delete Reader;
	}

	virtual void Serialize(void *data, int size)
	{
		guard(FGears4BundleFile::Serialize);
		if (ArStopper > 0 && ArPos + size > ArStopper)
			appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);

		Reader->Seek64(Info->Position + ArPos);
		Reader->Serialize(data, size);
		ArPos += size;

		unguard;
	}

	virtual void Seek(int Pos)
	{
		guard(FGears4BundleFile::Seek);
		assert(Pos >= 0 && Pos < Info->Size);
		ArPos = Pos;
		unguard;
	}

	virtual int GetFileSize() const
	{
		return (int)Info->Size;
	}

	virtual void Close()
	{
		Reader->Close();
	}

protected:
	const FGears4BundledInfo* Info;
	FArchive*	Reader;
};

// Dummy VFS container, just used to store file infos in some global list
class FGears4VFS : public FVirtualFileSystem
{
public:
	FGears4VFS()
	:	HashTable(NULL)
	{}

	virtual ~FGears4VFS()
	{
		if (HashTable) delete[] HashTable;
	}

	void ReserveFiles(int Count)
	{
		FileInfos.Empty(Count);
	}

	bool AddFile(const char* filename, const CGameFileInfo* bundleFile, int pos, int size)
	{
		guard(FGears4VFS::AddFile);

		// Do not register file duplicates (important for Gears4)
		if (FindFile(filename))
		{
			return false;
		}

		assert(FileInfos.Num() + 1 < FileInfos.Max()); // if we'll resize array, HashNext will be trashed
		FGears4BundledInfo* info = new (FileInfos) FGears4BundledInfo;

		info->Name = appStrdupPool(filename);
		info->Container = bundleFile;
		info->Position = pos;
		info->Size = size;

		AddFileToHash(info);

		// Register file in global FS
		CRegisterFileInfo reg;
		reg.Filename = filename;
		reg.Size = size;
		reg.IndexInArchive = FileInfos.Num() - 1;
		RegisterFile(reg);

		return true;

		unguard;
	}

	// Open the file from bundle
	virtual FArchive* CreateReader(int index)
	{
		guard(FGears4VFS::CreateReader);
		const FGears4BundledInfo& info = FileInfos[index];
		return new FGears4BundleFile(&info);
		unguard;
	}

	// Empty unneeded function from FVirtualFileSystem interface
	virtual bool AttachReader(FArchive* reader, FString& error)
	{
		assert(0);
		return false;
	}

protected:
	enum { HASH_SIZE = 16384 };
	enum { HASH_MASK = HASH_SIZE - 1 };

	TArray<FGears4BundledInfo> FileInfos;
	FGears4BundledInfo*	LastInfo;			// cached last accessed file info, simple optimization
	FGears4BundledInfo** HashTable;

	static uint16 GetHashForFileName(const char* FileName)
	{
		uint16 hash = 0;
		while (char c = *FileName++)
		{
			if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; // lowercase a character
			hash = ROL16(hash, 5) - hash + ((c << 4) + c ^ 0x13F);	// some crazy hash function
		}
		hash &= HASH_MASK;
		return hash;
	}

	void AddFileToHash(FGears4BundledInfo* File)
	{
		if (!HashTable)
		{
			HashTable = new FGears4BundledInfo* [HASH_SIZE];
			memset(HashTable, 0, sizeof(FGears4BundledInfo*) * HASH_SIZE);
		}
		uint16 hash = GetHashForFileName(File->Name);
		File->HashNext = HashTable[hash];
		HashTable[hash] = File;
	}

	const FGears4BundledInfo* FindFile(const char* name)
	{
		if (LastInfo && !stricmp(LastInfo->Name, name))
			return LastInfo;

		if (HashTable)
		{
			// Have a hash table, use it
			uint16 hash = GetHashForFileName(name);
			for (FGears4BundledInfo* info = HashTable[hash]; info; info = info->HashNext)
			{
				if (!stricmp(info->Name, name))
				{
					LastInfo = info;
					return info;
				}
			}
			return NULL;
		}

		// Linear search without a hash table
		for (int i = 0; i < FileInfos.Num(); i++)
		{
			FGears4BundledInfo* info = &FileInfos[i];
			if (!stricmp(info->Name, name))
			{
				LastInfo = info;
				return info;
			}
		}
		return NULL;
	}
};


void LoadGears4Manifest(const CGameFileInfo* info)
{
	guard(LoadGears4Manifest);

	appPrintf("Loading Gears4 manifest file %s\n", *info->GetRelativeName());

	FArchive* loader = info->CreateReader();
	assert(loader);
	loader->Game = GAME_Gears4;

	FGears4Manifest Manifest;
	int error = Manifest.Serialize(*loader);
	if (error != 0)
	{
		appError("Wrong Gears4 manifest format (error %d)", error);
	}

	delete loader;

	// Process manifest
	FGears4VFS* FileSystem = new FGears4VFS; // note: this pointer will not be stored anywhere, and not released
	FileSystem->ReserveFiles(Manifest.Assets.Num());

	int numMissingBundles = 0;
	int numBadBundles = 0;
	for (int bundleIndex = 0; bundleIndex < Manifest.Bundles.Num(); bundleIndex++)
	{
		const FGears4Bundle& Bundle = Manifest.Bundles[bundleIndex];

		// Find bundle
		char bundleFilename[MAX_PACKAGE_PATH];
		appSprintf(ARRAY_ARG(bundleFilename), "/Game/Bundles/%d.bundle", bundleIndex);
		const CGameFileInfo* bundleFile = CGameFileInfo::Find(bundleFilename);

		// Verify if we can use this bundle
		if (!bundleFile)
		{
			numMissingBundles++;
			continue;
		}
		if (bundleFile->Size != Bundle.GetBundleSize())
		{
			//?? TODO: probably scan bundle file instead of simply dropping it
			numBadBundles++;
			continue;
		}

		// Register bundled assets
		int32 pos = 0;
		FileSystem->Reserve(Bundle.Assets.Num());
		for (int assetIndex = 0; assetIndex < Bundle.Assets.Num(); assetIndex++)
		{
			const FGears4AssetEntry& Asset = Manifest.Assets[Bundle.Assets[assetIndex].AssetIndex];
			char buffer[MAX_PACKAGE_PATH];
			appSprintf(ARRAY_ARG(buffer), "%s.uasset", *Asset.AssetName);

			int size = Bundle.Assets[assetIndex].AssetSize;
			assert(size == Asset.AssetSize);
			FileSystem->AddFile(buffer, bundleFile, pos, size);
			pos += size;
		}
	}

	appPrintf("%d/%d bundles missing, %d bundles outdated\n", numMissingBundles, Manifest.Bundles.Num(), numBadBundles);

#if 0
	appPrintf("\n***\nAssets:%d A1:%d A2:%d Bundles:%d A3:%d A4:%d\n", Manifest.Assets.Num(),
		Manifest.Array1.Num(), Manifest.Array2.Num(), Manifest.Bundles.Num(), Manifest.Array3.Num(), Manifest.Array4.Num());

#define ANALYZE(arr,field) \
	{ \
		int vmin = 9999999, vmax = -9999999; \
		for (int i = 0; i < arr.Num(); i++) \
		{ \
			int n = arr[i] field; \
			if (n < vmin) vmin = n; \
			if (n > vmax) vmax = n; \
		} \
		appPrintf(STR(arr) STR(field)" = [%d,%d]\n", vmin, vmax); \
	}

#define ANALYZE2(arr,field1,field2) \
	{ \
		int vmin = 9999999, vmax = -9999999; \
		for (int i = 0; i < arr.Num(); i++) \
		{ \
			const auto& a2 = arr[i] field1; \
			for (int j = 0; j < a2.Num(); j++) \
			{ \
			    int n = a2[j] field2; \
				if (n < vmin) vmin = n; \
				if (n > vmax) vmax = n; \
			} \
		} \
		appPrintf(STR(arr) STR(field1) STR(field2) " = [%d,%d]\n", vmin, vmax); \
	}

//	ANALYZE(Manifest.Assets, .AssetSize);
	ANALYZE(Manifest.Assets, .p2);
	ANALYZE(Manifest.Assets, .p3);
	ANALYZE(Manifest.Assets, .p4);
	ANALYZE(Manifest.Array4, .X);
	ANALYZE(Manifest.Array4, .Y);
	ANALYZE(Manifest.Array1, );
	ANALYZE(Manifest.Array2, );
	ANALYZE(Manifest.Bundles, .SomeNum);
	ANALYZE2(Manifest.Bundles, .Assets, .AssetIndex);
	ANALYZE2(Manifest.Bundles, .Assets, .AssetSize);
	ANALYZE2(Manifest.Bundles, .p2, );

	for (int i = 0; i < Manifest.Bundles.Num(); i++)
	{
		const FGears4Bundle& E2 = Manifest.Bundles[i];
		for (int j = 0; j < E2.Assets.Num(); j++)
		{
			if (E2.Assets[j].AssetIndex == 55430)
			{
				appPrintf("Found in %d (size=%d)\n", i, E2.Assets[j].AssetSize);
			}
		}
	}

#define DIR_REF(label,index) appPrintf("%s[%d] = %s\n", label, index, *Manifest.Assets[index].AssetName)

	const char* find[] =
	{
//		"/game/effects/sparta/global/fire/materials/burningember",
//		"/game/design/mp/overhead_maps/gridlock_overhead_map",
		"/game/interface/options/options_button_backing",
//		"/game/characters/locust/brumak/gp01/brumakmesh_skeleton",
	};
	for (int ii = 0; ii < ARRAY_COUNT(find); ii++)
	for (int i = 0; i < Manifest.Assets.Num(); i++)
	{
		const FGears4AssetEntry& E = Manifest.Assets[i];
		if (!stricmp(*E.AssetName, find[ii]))
		{
			// Assets
			appPrintf("\n***\nFound %s at index %d\n", *E.AssetName, i);
			appPrintf("AssetSize=%d p2=%d p3=%d p4=%d\n", E.AssetSize, E.p2, E.p3, E.p4);
			appPrintf("Array4[%d] = { %d, %d }\n", i, Manifest.Array4[i].X, Manifest.Array4[i].Y);
			// Array1
			int Array1Index = E.p2;
			DIR_REF("Array1", Manifest.Array1[Array1Index]);
			// Entry
			int BundleIndex = Manifest.Array4[i].Y;
			const FGears4Bundle& Bundle = Manifest.Bundles[BundleIndex];
			appPrintf("Bundle[%d] = SomeNum=%d, Assets=%d, p2=%d\n", BundleIndex, Bundle.SomeNum, Bundle.Assets.Num(), Bundle.p2.Num());
			int32 pos = 0;
			for (int i2 = 0; i2 < Bundle.Assets.Num(); i2++)
			{
				const FGears4BundleItem& BI = Bundle.Assets[i2];
///				appPrintf("offs=%08X size=%08X %d: %s\n", pos, BI.AssetSize, i2, *Manifest.Assets[BI.AssetIndex].AssetName);
				pos += BI.AssetSize;
			}
			appPrintf("... bundle size = 0x%X (%d)\n", pos, pos);
//			ANALYZE(Bundle.Assets, .AssetSize);
			ANALYZE(Bundle.p2, );
			break;
		}
	}
#endif

	unguard;
}

#endif // GEARS4
