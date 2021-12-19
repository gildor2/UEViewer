#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#include "UnArchiveObb.h"
#include "UnArchivePak.h"
#include "IOStoreFileSystem.h"

#include "Parallel.h"

// includes for file enumeration
#if _WIN32
#	include <io.h>					// for findfirst() set
#else
#	include <dirent.h>				// for opendir() etc
#	include <sys/stat.h>			// for stat()
#endif


/*-----------------------------------------------------------------------------
	Game file system
-----------------------------------------------------------------------------*/

#define MAX_FOREIGN_FILES		32768

char GRootDirectory[MAX_PACKAGE_PATH];


#define UE4_PACKAGE_EXTENSIONS	"uasset", "umap",

static const char *PackageExtensions[] =
{
	"u", "ut2", "utx", "uax", "usx", "ukx",
#if UNREAL3
	"upk", "ut3", "xxx", "umap", "udk", "map",
#endif
#if UNREAL4
	UE4_PACKAGE_EXTENSIONS
#endif
#if RUNE
	"ums",
#endif
#if BATTLE_TERR
	"bsx", "btx", "bkx",				// older version
	"ebsx", "ebtx", "ebkx", "ebax",		// newer version, with encryption
#endif
#if TRIBES3
	"pkg",
#endif
#if BIOSHOCK
	"bsm",
#endif
#if VANGUARD
	"uea", "uem",
#endif
#if LEAD
	"ass", "umd",
#endif
#if MASSEFF
	"sfm",			// Mass Effect
	"pcc",			// Mass Effect 2
#endif
#if TLR
	"tlr",
#endif
#if LEGENDARY
	"ppk", "pda",	// Legendary: Pandora's Box
#endif
#if R6VEGAS
	"uppc", "rmpc",	// Rainbow 6 Vegas 2
#endif
#if TERA
	"gpk",			// TERA: Exiled Realms of Arborea
#endif
#if APB
	"apb",			// All Points Bulletin
#endif
#if TRIBES4
	"fmap",			// Tribes: Ascend
#endif
#if HUNTED			// Hunted: The Demon's Forge
	"lvl",
#endif
	// other games with no special code
	"lm",			// Landmass
	"s8m",			// Section 8 map
	"ccpk",			// Crime Craft character package
};

#if UNREAL3 || UC2
#define HAS_SUPPORT_FILES 1
// secondary (non-package) files
static const char *KnownExtensions[] =
{
#	if UNREAL3
	"tfc",			// Texture File Cache
	"bin",			// just in case
#	endif
#	if UC2
	"xpr",			// XBox texture container
#	endif
#	if BIOSHOCK
	"blk", "bdc",	// Bulk Content + Catalog
#	endif
#	if TRIBES4
	"rtc",
#	endif
#	if UNREAL4
	"ubulk",		// separately stored UE4 bulk data
	"uexp",			// object's data cut from UE4 package when Event Driven Loader is used
	"uptnl",		// optional ubulk
#	endif
#if GEARS4
	"bundle",
#endif
};
#endif

// By default umodel extracts data to the current directory. Working with a huge amount of files
// could result to get "too many unknown files" error. We can ignore types of files which are
// extracted by umodel to reduce chance to get such error.
static const char *SkipExtensions[] =
{
	"tga", "png", "dds", "bmp", "mat", "txt",	// textures, materials
	"psk", "pskx", "psa", "config", "gltf",		// meshes, animations
	"ogg", "wav", "fsb", "xma", "unk",			// sounds
	"gfx", "fxa",								// 3rd party
	"md5mesh", "md5anim",						// md5 mesh
	"uc", "3d",									// vertex mesh
	"wem",										// WwiseAudio files
};

#if UNREAL4

static bool GIsUE4Pak = false;

static const char* UE4PackageExtensions[] =
{
	UE4_PACKAGE_EXTENSIONS
};

#endif // UNREAL4

static bool FindExtension(const char *Ext, const char **ExtensionsList, int NumExtensions)
{
	for (int i = 0; i < NumExtensions; i++)
		if (!stricmp(Ext, ExtensionsList[i])) return true;
	return false;
}


static TArray<CGameFileInfo*> GameFiles;
int GNumPackageFiles = 0;
int GNumForeignFiles = 0;

#define GAME_FILE_HASH_SIZE		32768

//#define PRINT_HASH_DISTRIBUTION	1
//#define DEBUG_HASH				1
//#define DEBUG_HASH_NAME			"21680"

static CGameFileInfo* GameFileHash[GAME_FILE_HASH_SIZE];

#define GAME_FOLDER_HASH_SIZE	1024

struct CGameFolderInfo
{
	FString Name;
	int		HashNext;		// index in GameFileHash array
	int		NumFiles;		// number of files located in this folder

	CGameFolderInfo()
	: HashNext(0)
	, NumFiles(0)
	{}
};

static TArray<CGameFolderInfo> GameFolders;
static int GameFoldersHash[GAME_FOLDER_HASH_SIZE];


#if UNREAL3
const char*           GStartupPackage = "startup_xxx";	// this is just a magic constant pointer, content is meaningless
static CGameFileInfo* GStartupPackageInfo = NULL;
static int            GStartupPackageInfoWeight = 0;
#endif


void FVirtualFileSystem::Reserve(int count)
{
	guard(FVirtualFileSystem::Reserve);
	GameFiles.Reserve(GameFiles.Num() + count);
	unguard;
}

FORCEINLINE uint32 GetHashInternal(const char* s, int len)
{
	uint16 hash = 0;
	for (int i = 0; i < len; i++)
	{
		char c = *s++ & 0xDF;				// uppercase the character with "& 0xDF"
//		hash = ROL16(hash, 5) - hash + ((c << 4) + c ^ 0x13F);	// some crazy hash function
		hash = ROL16(hash, 1) + c;			// note: if we'll use more than 16-bit GAME_FILE_HASH_SIZE value, should use ROL32 here
	}
	return hash;
}

// Compute hash for filename, with skipping file extension. Name should not have path.
template<bool MayHaveExtension>
static int GetHashForFileName(const char* FileName)
{
	// Locate the end of string or extension
	const char* s = FileName;
	int len = -1;
	while (true)
	{
		char c = *s;
		if (MayHaveExtension)
		{
			if (c == '.') len = s - FileName;
			if (c == 0)
			{
				// End of string
				if (len < 0) len = s - FileName; // preserve strrchr('.') logic
				break;
			}
		}
		else
		{
			if (c == 0)
			{
				// End of string
				len = s - FileName;
				break;
			}
		}
		s++;
	}

	uint32 hash = GetHashInternal(FileName, len) & (GAME_FILE_HASH_SIZE - 1);
#ifdef DEBUG_HASH_NAME
	if (strstr(FileName, DEBUG_HASH_NAME))
		appPrintf("-> hash[%s] (%s,%d) -> %X\n", FileName, s1, len, hash);
#endif
	return hash;
}

inline int GetHashForFolderName(const char* FolderName)
{
	return GetHashInternal(FolderName, strlen(FolderName)) & (GAME_FOLDER_HASH_SIZE - 1);
}

#if PRINT_HASH_DISTRIBUTION

static void PrintHashDistribution()
{
	int hashCounts[1024];
	int totalCount = 0;
	memset(hashCounts, 0, sizeof(hashCounts));
	for (int hash = 0; hash < GAME_FILE_HASH_SIZE; hash++)
	{
		int count = 0;
		for (CGameFileInfo* info = GameFileHash[hash]; info; info = info->HashNext)
			count++;
		assert(count < ARRAY_COUNT(hashCounts));
		hashCounts[count]++;
		totalCount += count;
	}
	appPrintf("Filename hash distribution: collision count -> num chains\n");
	int totalCount2 = 0;
	for (int i = 0; i < ARRAY_COUNT(hashCounts); i++)
	{
		int count = hashCounts[i];
		if (count > 0)
		{
			totalCount2 += count * i;
			float percent = totalCount2 * 100.0f / totalCount;
			appPrintf("%d -> %d [%.1f%%]\n", i, count, percent);
		}
	}
	assert(totalCount == totalCount2);
}

#endif // PRINT_HASH_DISTRIBUTION

int appGetGameFolderIndex(const char* FolderName)
{
	int hash = GetHashForFolderName(FolderName);
	for (int index = GameFoldersHash[hash]; index; /* empty */)
	{
		CGameFolderInfo& info = GameFolders[index];
		if (!stricmp(*info.Name, FolderName))
		{
			// Found
			return index;
		}
		index = info.HashNext;
	}
	return -1;
}

int appGetGameFolderCount()
{
	return GameFolders.Num();
}

int RegisterGameFolder(const char* FolderName)
{
	guard(RegisterGameFolder);

	// Find existing folder entry. Copy-pasted of appGetGameFolderIndex, but with passing 'hash'
	// to the code below.
	int hash = GetHashForFolderName(FolderName);
	for (int index = GameFoldersHash[hash]; index; /* empty */)
	{
		CGameFolderInfo& info = GameFolders[index];
		if (!stricmp(*info.Name, FolderName))
		{
			// Found
			return index;
		}
		index = info.HashNext;
	}

	// Add new entry

	// Initialize GameFolders if needed. Entry with zero index is invalid, so explicitly skip it.
	if (GameFolders.Num() == 0)
	{
		GameFolders.AddDefaulted();
	}

	if (GameFolders.Num() + 1 >= GameFolders.Max())
	{
		// Resize GameFolders array with large steps
		GameFolders.Reserve(GameFolders.Num() + 256);
	}

	int newIndex = GameFolders.AddDefaulted();
	CGameFolderInfo& info = GameFolders[newIndex];
	info.Name = FolderName; // there's no much need to pass folder name through appStrdupPool, so keep it as FString
	info.HashNext = GameFoldersHash[hash];
	GameFoldersHash[hash] = newIndex;

	return newIndex;

	unguard;
}


//!! add define USE_VFS = SUPPORT_ANDROID || UNREAL4, perhaps || SUPPORT_IOS

static void RegisterGameFile(const char* FullName, int64 FileSize = -1)
{
	guard(RegisterGameFile);

//	printf("..file %s\n", FullName);

	const char* ext = strrchr(FullName, '.');
	if (ext == NULL) return;

	ext++;

	// Find if this file in an archive with VFS inside
	FVirtualFileSystem* vfs = NULL;
	FArchive* reader = NULL;

#if SUPPORT_ANDROID
	if (!stricmp(ext, "obb"))
	{
		GForcePlatform = PLATFORM_ANDROID;
		reader = new FFileReader(FullName);
		if (!reader) return;
		reader->Game = GAME_UE3;
		vfs = new FObbVFS(FullName);
	}
#endif // SUPPORT_ANDROID
#if UNREAL4
	FPakVFS* PakVfs = NULL;
	if (!stricmp(ext, "pak"))
	{
		reader = new FFileReader(FullName);
		if (!reader) return;
		reader->Game = GAME_UE4_BASE;
		PakVfs = new FPakVFS(FullName);
		vfs = PakVfs;
		GIsUE4Pak = true; // ignore non-UE4 extensions for speedup file registration
	}
	else if (!stricmp(ext, "utok") || !stricmp(ext, "ucas"))
	{
		// Processed with .pak file, so ignore these files
	}
#endif // UNREAL4

	// Note: VFS pointer is not stored in any global list, and not released at program exit
	if (vfs)
	{
		assert(reader);
		// read VFS directory
		FString error;
		if (!vfs->AttachReader(reader, error))
		{
#if UNREAL4
			// Reset GIsUE4Pak back in a case if .pak file appeared in directory
			// by accident.
			GIsUE4Pak = false;
#endif
			// something goes wrong
			if (error.Len())
			{
				appPrintf("%s\n", *error);
			}
			else
			{
				appPrintf("File %s has an unknown format\n", FullName);
			}
			delete vfs;
			delete reader;
			return;
		}
#if UNREAL4
		if (GIsUE4Pak)
		{
			// Get the pak's encryption key, which is only assigned after vfs->AttachReader()
			assert(PakVfs);
			FString PakEncryptionKey = PakVfs->GetPakEncryptionKey();

			// Check for presence of IOStore file system for this pak
			guard(TokArchive);
			char Path[MAX_PACKAGE_PATH];
			appStrncpyz(Path, FullName, ARRAY_COUNT(Path));
			char* PathExt = Path + (ext - FullName);
			strcpy(PathExt, "utoc");
			FILE* tocFile = fopen(Path, "rb");
			if (tocFile)
			{
				fclose(tocFile);

				static bool bGlobalChecked = false;
				if (!bGlobalChecked)
				{
					bGlobalChecked = true;
					char GlobalPath[MAX_PACKAGE_PATH];
					strcpy(GlobalPath, Path);
					char* s = strrchr(GlobalPath, '/');
					if (s)
						s++;
					else
						s = GlobalPath;
					// The name of global.utoc file is hardcoded in UE4 code in FPakPlatformFile::Initialize, see
					// IoStoreGlobalEnvironment.InitializeFileEnvironment() function call.
					strcpy(s, "global.utoc");
					FIOStoreFileSystem::LoadGlobalContainer(GlobalPath);
				}

				FIOStoreFileSystem* iosVfs = new FIOStoreFileSystem(Path);
				iosVfs->PakEncryptionKey = PakEncryptionKey;
				FArchive* tocReader = new FFileReader(Path);
				tocReader->Game = GAME_UE4_BASE;

				// Scan contents of IOStore container
				FString error;
				if (!iosVfs->AttachReader(tocReader, error))
				{
					if (error.Len())
					{
						appPrintf("%s\n", *error);
					}
					delete iosVfs;
					delete tocReader;
				}
			}
			unguard;

			// Reset GIsUE4Pak
			GIsUE4Pak = false;
		}
#endif
	}
	else
	{
		// Register OS file
		CRegisterFileInfo info;

		if (FileSize < 0)
		{
			// Get file size
			FILE* f = fopen(FullName, "rb");
			if (f)
			{
				fseek(f, 0, SEEK_END);
				info.Size = ftell(f);
				fclose(f);
			}
			else
			{
				// File is not accessible
				return;
			}
		}
		else
		{
			info.Size = FileSize;
		}

		// Cut GRootDirectory from filename
		const char *s = FullName + strlen(GRootDirectory) + 1;
		assert(s[-1] == '/');
		info.Filename = s;

		CGameFileInfo::Register(NULL, info);
	}

	unguardf("%s", FullName);
}

// Allocator for CGameFileInfo. Use CMemoryChain because:
// - we don't need to delete CGameFileInfo items (unless doing deallocation once and picking it up again next time)
// - there's no malloc overhead
// - file items has good cache locality when enumerating all files

static CMemoryChain* FileInfoChain;
static CGameFileInfo* DeallocatedFileInfo = NULL;

FORCEINLINE CGameFileInfo* AllocFileInfo()
{
	if (DeallocatedFileInfo)
	{
		CGameFileInfo* result = DeallocatedFileInfo;
		DeallocatedFileInfo = NULL;
		return result;
	}
	if (FileInfoChain == NULL)
	{
		FileInfoChain = new CMemoryChain();
	}
	return (CGameFileInfo*) FileInfoChain->Alloc(sizeof(CGameFileInfo));
}

FORCEINLINE void DeallocFileInfo(CGameFileInfo* info)
{
	assert(DeallocatedFileInfo == NULL);
	DeallocatedFileInfo = info;
}

CGameFileInfo* CGameFileInfo::Register(FVirtualFileSystem* parentVfs, const CRegisterFileInfo& RegisterInfo)
{
	guard(CGameFileInfo::Register);

	// Verify file extension
	const char *ext = strrchr(RegisterInfo.Filename, '.');
	if (!ext) return NULL; // unknown type
	ext++;

	// to know if file is a package or not. Note: this will also make pak loading a but faster.
	bool IsPackage = false;
#if UNREAL4
	if (GIsUE4Pak)
	{
		// Faster case for UE4 files - it has small list of extensions
		IsPackage = FindExtension(ext, ARRAY_ARG(UE4PackageExtensions));
	}
	else
#endif
	{
		// Longer list for games older than UE4. Processed slower, however we never have such a long list of files as for UE4.
		IsPackage = FindExtension(ext, ARRAY_ARG(PackageExtensions));
	}

	if (!parentVfs && !IsPackage)
	{
#if HAS_SUPPORT_FILES
		// Check for suppressed extensions
		if (!FindExtension(ext, ARRAY_ARG(KnownExtensions)))
#endif
		{
			// Unknown extension. Check if we should count it or not.
			// ignore unknown files inside "cooked" or "content" directories
			if (appStristr(RegisterInfo.Filename, "cooked") || appStristr(RegisterInfo.Filename, "content")) return NULL;
			// perhaps this file was exported by our tool - skip it
			if (!FindExtension(ext, ARRAY_ARG(SkipExtensions)))
			{
				// Unknown file type
				if (++GNumForeignFiles >= MAX_FOREIGN_FILES)
					appErrorNoLog("Too many unknown files - bad root directory (%s)?", GRootDirectory);
			}
			return NULL;
		}
	}

	// A known file type is here.

	// Use RegisterInfo.Path if not empty
	const char* ShortFilename = RegisterInfo.Filename;
	int FolderIndex = 0;

	if (RegisterInfo.FolderIndex)
	{
		FolderIndex = RegisterInfo.FolderIndex;
	}
	else if (!RegisterInfo.Path)
	{
		// Split file name and register/find folder
		FStaticString<MAX_PACKAGE_PATH> Folder;
		if (const char* s = strrchr(RegisterInfo.Filename, '/'))
		{
			// Have a path part inside a filename
			Folder = RegisterInfo.Filename;
			// Cut path at '/' location
			Folder[s - RegisterInfo.Filename] = 0;
			ShortFilename = s + 1;
		}
		FolderIndex = RegisterGameFolder(*Folder);
	}
	else
	{
		FolderIndex = RegisterGameFolder(RegisterInfo.Path);
	}

	int extOffset = ext - ShortFilename;
	assert(extOffset < 256); // restriction of CGameFileInfo::ExtensionOffset

	// Create CGameFileInfo entry
	CGameFileInfo* info = AllocFileInfo();
	info->Flags = RegisterInfo.Flags | (IsPackage ? CGameFileInfo::GFI_Package : 0);
	info->FileSystem = parentVfs;
	info->IndexInVfs = RegisterInfo.IndexInArchive;
	info->ShortFilename = appStrdupPool(ShortFilename);
	info->ExtensionOffset = extOffset;
	info->FolderIndex = FolderIndex;
	info->Size = RegisterInfo.Size;
	info->SizeInKb = (info->Size + 512) / 1024;

	if (info->Size < 16) info->Flags &= ~CGameFileInfo::GFI_Package;

#if UNREAL3
	if (info->IsPackage() && (strnicmp(info->ShortFilename, "startup", 7) == 0))
	{
		// Register a startup package
		// possible name variants:
		// - startup
		// - startup_int
		// - startup_*
		int startupWeight = 0;
		if (info->ShortFilename[7] == '.')
			startupWeight = 30;							// "startup.upk"
		else if (strnicmp(info->ShortFilename+7, "_int.", 5) == 0)
			startupWeight = 20;							// "startup_int.upk"
		else if (strnicmp(info->ShortFilename+7, "_loc_int.", 9) == 0)
			startupWeight = 20;							// "startup_int.upk"
		else if (info->ShortFilename[7] == '_')
			startupWeight = 1;							// non-int locale, lower priority - use if when other is not detected
		if (startupWeight > GStartupPackageInfoWeight)
		{
			GStartupPackageInfoWeight = startupWeight;
			GStartupPackageInfo = info;
		}
	}
#endif // UNREAL3

	int hash = GetHashForFileName<true>(info->ShortFilename);

	// find if we have previously registered file with the same name
	FastNameComparer FilenameCmp(info->ShortFilename);
	for (CGameFileInfo* prevInfo = GameFileHash[hash]; prevInfo; prevInfo = prevInfo->HashNext)
	{
		if ((prevInfo->FolderIndex == FolderIndex) && FilenameCmp(prevInfo->ShortFilename))
		{
			// this is a duplicate of the file (patch), use new information
			prevInfo->UpdateFrom(info);
			// return allocated info back to pool, so it will be reused next time
			DeallocFileInfo(info);
#if DEBUG_HASH
			appPrintf("--> dup(%s) pkg=%d hash=%X\n", prevInfo->ShortFilename, prevInfo->IsPackage, hash);
#endif
			return prevInfo;
		}
	}

	// Insert new CGameFileInfo into hash table
	if (GameFiles.Num() + 1 >= GameFiles.Max())
	{
		// Resize GameFiles array with large steps
		GameFiles.Reserve(GameFiles.Num() + 1024);
	}
	GameFiles.Add(info);
	if (IsPackage) GNumPackageFiles++;
	GameFolders[FolderIndex].NumFiles++;

	info->HashNext = GameFileHash[hash];
	GameFileHash[hash] = info;

#if DEBUG_HASH
	appPrintf("--> add(%s) pkg=%d hash=%X\n", info->ShortFilename, info->IsPackage, hash);
#endif

	return info;

	unguardf("%s", RegisterInfo.Filename);
}

static bool ScanGameDirectory(const char *dir, bool recurse)
{
	guard(ScanGameDirectory);

	char Path[MAX_PACKAGE_PATH];
	bool res = true;
//	printf("Scan %s\n", dir);

	struct FileInfo
	{
		FStaticString<256> Filename;	// short file name
		int64 Size;						// file size
	};

	//todo: check - there's TArray<FStaticString> what's unsafe
	TArray<FileInfo> Files;
	Files.Empty(1024);

	guard(ReadDir);

#if _WIN32
	appSprintf(ARRAY_ARG(Path), "%s/*.*", dir);
	_finddatai64_t found;
	intptr_t hFind = _findfirsti64(Path, &found);
	if (hFind == -1) return true;
	do
	{
		if (found.name[0] == '.') continue;			// "." or ".."
		// directory -> recurse
		if (found.attrib & _A_SUBDIR)
		{
			if (recurse)
			{
				appSprintf(ARRAY_ARG(Path), "%s/%s", dir, found.name);
				res = ScanGameDirectory(Path, recurse);
			}
		}
		else
		{
			Files.Add( { found.name, found.size } );
		}
	} while (res && _findnexti64(hFind, &found) != -1);
	_findclose(hFind);
#else
	DIR *find = opendir(dir);
	if (!find) return true;
	struct dirent *ent;
	while (/*res &&*/ (ent = readdir(find)))
	{
		if (ent->d_name[0] == '.') continue;			// "." or ".."
		appSprintf(ARRAY_ARG(Path), "%s/%s", dir, ent->d_name);
		// directory -> recurse
		// note: using 'stat64' here because 'stat' ignores large files
		struct stat64 buf;
		if (stat64(Path, &buf) < 0) continue;			// or break?
		if (S_ISDIR(buf.st_mode))
		{
			if (recurse)
				res = ScanGameDirectory(Path, recurse);
		}
		else
		{
			Files.Add( { ent->d_name, buf.st_size } );
		}
	}
	closedir(find);
#endif

	unguard;

	// Register files in sorted order - should be done for pak files, so patches will work.
	Files.Sort([](const FileInfo& p1, const FileInfo& p2) -> int
		{
			return stricmp(*p1.Filename, *p2.Filename);
		});

	for (const FileInfo& File : Files)
	{
		appSprintf(ARRAY_ARG(Path), "%s/%s", dir, *File.Filename);
		RegisterGameFile(Path, File.Size);
	}

	return res;

	unguard;
}


void LoadGears4Manifest(const CGameFileInfo* info);

void appSetRootDirectory(const char *dir, bool recurse)
{
	guard(appSetRootDirectory);

#if PROFILE
	appResetProfiler();
#endif

	if (dir[0] == 0) dir = ".";	// using dir="" will cause scanning of "/dir1", "/dir2" etc (i.e. drive root)
	appStrncpyz(GRootDirectory, dir, ARRAY_COUNT(GRootDirectory));
	ScanGameDirectory(GRootDirectory, recurse);

#if GEARS4
	if (GForceGame == GAME_Gears4)
	{
		const CGameFileInfo* manifest = CGameFileInfo::Find("BundleManifest.bin");
		if (manifest)
		{
			LoadGears4Manifest(manifest);
		}
		else
		{
			appErrorNoLog("Gears of War 4: missing BundleManifest.bin file.");
		}
	}
#endif // GEARS4

	appPrintf("Found %d game files (%d skipped) in %d folders at path \"%s\"\n", GameFiles.Num(), GNumForeignFiles, GameFolders.Num() ? GameFolders.Num()-1 : 0, dir);

#if UNREAL4
	// Count sizes of additional files. Should process .uexp and .ubulk files, register their information for .uasset.
	ParallelFor(GameFiles.Num(), [](int index)
		{
			CGameFileInfo* info = GameFiles[index];
			if (info->IsPackage())
			{
				// Find all files with the same path/name but different extension
				TStaticArray<const CGameFileInfo*, 32> otherFiles;
				info->FindOtherFiles(otherFiles);
				for (const CGameFileInfo* other : otherFiles)
				{
					info->ExtraSizeInKb += other->SizeInKb;
				}
			}
		});
#endif // UNREAL4

#if PROFILE
	appPrintProfiler("Scanned game directory");
#endif

#if PRINT_HASH_DISTRIBUTION
	PrintHashDistribution();
#endif
	unguardf("dir=%s", dir);
}


const char *appGetRootDirectory()
{
	return GRootDirectory[0] ? GRootDirectory : NULL;
}


// UE2 has simple directory hierarchy with directory depth 1
static const char *KnownDirs2[] =
{
	"Animations",
	"Maps",
	"Sounds",
	"StaticMeshes",
	"System",
#if LINEAGE2
	"Systextures",
#endif
#if UC2
	"XboxTextures",
	"XboxAnimations",
#endif
	"Textures"
};


void appSetRootDirectory2(const char *filename)
{
	guard(appSetRootDirectory2);

	char buf[MAX_PACKAGE_PATH];
	appStrncpyz(buf, filename, ARRAY_COUNT(buf));
	char *s;
	// replace slashes
	for (s = buf; *s; s++)
		if (*s == '\\') *s = '/';
	// cut filename
	s = strrchr(buf, '/');
	if (s)
	{
		*s = 0;
		// make a copy for fallback
	}
	else
	{
		strcpy(buf, ".");
	}

	FString root;
	int detected = 0;				// weight; 0 = not detected
	root = buf;

	// analyze path
	const char *pCooked = NULL;
	for (int i = 0; i < 8; i++)
	{
		// find deepest directory name
		s = strrchr(buf, '/');
		if (!s) break;
		*s++ = 0;
		bool found = false;
		if (i == 0)
		{
			for (int j = 0; j < ARRAY_COUNT(KnownDirs2); j++)
				if (!stricmp(KnownDirs2[j], s))
				{
					found = true;
					break;
				}
		}
		if (found)
		{
			if (detected < 1)
			{
				detected = 1;
				root = buf;
			}
		}
		pCooked = appStristr(s, "Cooked");
		if (pCooked || appStristr(s, "Content"))
		{
			s[-1] = '/';			// put it back
			if (detected < 2)
			{
				detected = 2;
				root = buf;
				break;
			}
		}
	}
	appPrintf("Detected game root %s%s", *root, (detected == false) ? " (no recurse)" : "");

	// detect platform by cooked folder name
	if (GForcePlatform == PLATFORM_UNKNOWN && pCooked)
	{
		pCooked += 6;	// skip "Cooked" string
		if (!strnicmp(pCooked, "PS3", 3))
			GForcePlatform = PLATFORM_PS3;
		else if (!strnicmp(pCooked, "Xenon", 5))
			GForcePlatform = PLATFORM_XBOX360;
		else if (!strnicmp(pCooked, "IPhone", 6))
			GForcePlatform = PLATFORM_IOS;
		if (GForcePlatform != PLATFORM_UNKNOWN)
		{
			static const char *PlatformNames[] =
			{
				"",
				"PC",
				"XBox360",
				"PS3",
				"PS4",
				"Switch",
				"iPhone",
				"Android",
			};
			static_assert(ARRAY_COUNT(PlatformNames) == PLATFORM_COUNT, "PlatformNames has wrong item count");
			appPrintf("; platform %s", PlatformNames[GForcePlatform]);
		}
	}

	// scan root directory
	appPrintf("\n");
	appSetRootDirectory(*root, detected != 0);

	unguardf("%s", filename);
}


const CGameFileInfo* CGameFileInfo::Find(const char *Filename, int GameFolder)
{
	guard(CGameFileInfo::Find);

#if UNREAL3
	bool findStartupPackage = (strcmp(Filename, GStartupPackage) == 0);
	if (findStartupPackage)
		return GStartupPackageInfo;
#endif

	char buf[MAX_PACKAGE_PATH];
	appStrncpyz(buf, Filename, ARRAY_COUNT(buf));

	// Replace backslashes and analyze the string
	const char* ShortFilename = buf;
	const char* Extension = NULL;
	char* s;
	for (s = buf; *s; s++)
	{
		char c = *s;
		if (c == '\\')
		{
			*s = '/';
			ShortFilename = s + 1;
		}
		else if (c == '/')
		{
			ShortFilename = s + 1;
		}
		else if (c == '.')
		{
			Extension = s + 1;
		}
	}
	// 's' points to the end of string
	int shortFilenameLen = s - ShortFilename;
	if (Extension < ShortFilename) // before the last path separator
		Extension = NULL;

	// Get path of the file
	FStaticString<MAX_PACKAGE_PATH> FindPath;
	if (ShortFilename > buf)
	{
		(char&)(ShortFilename[-1]) = 0; // removing 'const' here ...
		FindPath = buf;
	}
	// else - FindPath will be empty

	// Get hash before stripping extension (could be required for files with double extension, like .hdr.rtc for games with Redux textures).
	// If 'Ext' has been provided, ShortFilename has NO extension, and we're going to append Ext to the filename later, so there's nothing to
	// cut in this case.
	int hash = GetHashForFileName<true>(ShortFilename);
#if DEBUG_HASH
	appPrintf("--> find(%s) hash=%X\n", ShortFilename, hash);
#endif

	// check for extension in filename
	int nameLenNoExt = Extension ? Extension - ShortFilename - 1 : shortFilenameLen;
	assert(nameLenNoExt < 256); // restriction of CGameFileInfo::ExtensionOffset

	// Here:
	// 'buf' contains provided filename path (stripped file name)
	// 'ShortFilename' points to filename with extension
	// 'Extension' points to extension, or NULL if not supplied (therefore looking for package file)

#if defined(DEBUG_HASH_NAME) || DEBUG_HASH
	appPrintf("--> Loading %s (%s, len=%d, hash=%X)\n", buf, ShortFilename, nameLenNoExt, hash);
#endif

	CGameFileInfo* bestMatch = NULL;
	int bestMatchWeight = -1;
	uint8 extOffsetPattern = nameLenNoExt + 1;	// include '.' to length, just put outside the loop for optimization

	FastNameComparer nameCmp(ShortFilename, nameLenNoExt);
	FastNameComparer extCmp(Extension ? Extension : "");

	for (CGameFileInfo* info = GameFileHash[hash]; info; info = info->HashNext)
	{
#if defined(DEBUG_HASH_NAME) || DEBUG_HASH
		appPrintf("----> verify %s\n", *info->GetRelativeName());
#endif
		// Check if info's filename length matches required one
		if (info->ExtensionOffset != extOffsetPattern)
		{
			// Different filename length
//			printf("-----> wrong length %d\n", info->ExtensionOffset);
			continue;
		}
		if (GameFolder > 0 && info->FolderIndex != GameFolder)
		{
			// We've got explicit folder where to find
			continue;
		}

		// Compare extension
		if (Extension)
		{
			if (!extCmp(info->GetExtension())) continue;
		}
		else
		{
			// No extension has been provided, so we're looking only for package files
			if (!info->IsPackage()) continue;
		}

		// Verify the filename without extension
		if (!nameCmp(info->ShortFilename))
			continue;

		if (GameFolder > 0)
		{
			// Everything has been verified, avoid any fuzzy search over multiple folders
			return info;
		}

		// Short filename matched, now compare path before the filename. We're using fuzzy search here because
		// "saved" files may not match the exact file path.

		// Assume 'ShortFilename' is part of 'buf' and 'info->ShortFilename' is part of 'info->RelativeName'.
		const FString& InfoPath = info->GetPath();
		if (FindPath.IsEmpty() || InfoPath.IsEmpty())
		{
			// There's no path in input filename or in found file
			return info;
		}

		if (FindPath == InfoPath)
		{
			// Paths are exactly matching
			return info;
		}

		if (FindPath.Len() > InfoPath.Len())
		{
			// Can't match these paths - not enough space in InfoPath
			if (FindPath[0] == '/')
			{
				// It seems Find() is called for UE4 package with full name, e.g. when finding package import.
				// In the case uasset files were unpacked, and "game directory" points at subset of game files,
				// matching FindPath might be longer than InfoPath.
			}
			else
			{
				continue;
			}
		}

		int matchWeight = 0;
		const char *s = *InfoPath + InfoPath.Len() - 1;
		const char *d = *FindPath + FindPath.Len() - 1;
		int maxCheck = min(InfoPath.Len(), FindPath.Len());
		// Check if FindPath matches end of InfoPath, case-insensitively
		while (tolower(*s) == tolower(*d) && maxCheck > 0)
		{
			matchWeight++;
			maxCheck--;
			// don't include pointer decrements into the loop condition, so both pointers will always be decremented
			s--;
			d--;
		}
		if (maxCheck == 0)
		{
			// Fully matched
			return info;
		}
//		printf("--> matched: %s (weight=%d)\n", info->RelativeName, matchWeight);
		if (matchWeight > bestMatchWeight)
		{
//			printf("---> better match\n");
			bestMatch = info;
			bestMatchWeight = matchWeight;
		}
	}
	return bestMatch;

	unguardf("name=%s", Filename);
}


// Find all files with the same base file name (ignoring extension) and same directory as for provided file
void CGameFileInfo::FindOtherFiles(TArray<const CGameFileInfo*>& files) const
{
	guard(CGameFileInfo::FindOtherFiles);

	// Get file name to compute hash
	FStaticString<MAX_PACKAGE_PATH> Name;
	GetCleanName(Name);
	// Cut extension
	char* s = strrchr(&Name[0], '.');
	if (!s) return;
	*s = 0;

	int hash = GetHashForFileName<false>(*Name);

	// Restore point at extension part, for comparing "name."
	*s = '.';
	FastNameComparer FilenameCmp(*Name, s - *Name + 1);

	int folderIndex = FolderIndex;
	for (CGameFileInfo* otherFile = GameFileHash[hash]; otherFile; otherFile = otherFile->HashNext)
	{
		if (otherFile->FolderIndex != folderIndex || otherFile == this)
			continue;
		if (FilenameCmp(otherFile->ShortFilename))
		{
			files.Add(otherFile);
		}
	}

	unguard;
}

struct FindPackageWildcardData
{
	TArray<const CGameFileInfo*> FoundFiles;
	bool			WildcardContainsPath;
	FString			Wildcard;
};

static bool FindPackageWildcardCallback(const CGameFileInfo *file, FindPackageWildcardData &data)
{
	FStaticString<MAX_PACKAGE_PATH> Name;

	bool useThisPackage = false;
	if (data.WildcardContainsPath)
	{
		file->GetRelativeName(Name);
		useThisPackage = appMatchWildcard(*Name, *data.Wildcard, true);
	}
	else
	{
		file->GetCleanName(Name);
		useThisPackage = appMatchWildcard(*Name, *data.Wildcard, true);
	}
	if (useThisPackage)
	{
		data.FoundFiles.Add(file);
	}
	return true;
}


void appFindGameFiles(const char *Filename, TArray<const CGameFileInfo*>& Files)
{
	guard(appFindGameFiles);

	if (!appContainsWildcard(Filename))
	{
		const CGameFileInfo* File = CGameFileInfo::Find(Filename);
		if (File)
			Files.Add(File);
		return;
	}

	// here we're working with wildcard and should iterate over all files

	char buf[MAX_PACKAGE_PATH];
	appStrncpyz(buf, Filename, ARRAY_COUNT(buf));
	// replace backslashes
	bool containsPath = false;
	for (char* s = buf; *s; s++)
	{
		char c = *s;
		if (c == '\\')
		{
			*s = '/';
			containsPath = true;
		}
		else if (c == '/')
		{
			containsPath = true;
		}
	}

	FindPackageWildcardData findData;
	findData.WildcardContainsPath = containsPath;
	findData.Wildcard = buf;
	appEnumGameFiles(FindPackageWildcardCallback, findData);

	CopyArray(Files, findData.FoundFiles);

	unguardf("wildcard=%s", Filename);
}


const char *appSkipRootDir(const char *Filename)
{
	if (!GRootDirectory[0]) return Filename;

	const char *str1 = GRootDirectory;
	const char *str2 = Filename;
	while (true)
	{
		char c1 = *str1++;
		char c2 = *str2++;
		// normalize names for easier checking
		if (c1 == '\\') c1 = '/';
		if (c2 == '\\') c2 = '/';
		if (!c1)
		{
			// root directory name is fully scanned
			if (c2 == '/') return str2;
			// else - something like this: root="dirname/name2", file="dirname/name2extrachars"
			return Filename;			// not in root
		}
		if (!c2) return Filename;		// Filename is shorter than GRootDirectory
		if (c1 != c2) return Filename;	// different names
	}
}


FArchive* CGameFileInfo::CreateReader(bool bDontCrash) const
{
	if (!FileSystem)
	{
		// regular file
		FStaticString<MAX_PACKAGE_PATH> RelativeName;
		GetRelativeName(RelativeName);
		char buf[MAX_PACKAGE_PATH];
		appSprintf(ARRAY_ARG(buf), "%s/%s", GRootDirectory, *RelativeName);
		if (!bDontCrash)
		{
			return new FFileReader(buf);
		}
		else
		{
			// "Safe" mode
			FArchive* Reader = new FFileReader(buf, EFileArchiveOptions::OpenWarning);
			if (!Reader->IsOpen())
			{
				delete Reader;
				Reader = NULL;
			}
			return Reader;
		}
	}
	else
	{
		// file from virtual file system
		return FileSystem->CreateReader(IndexInVfs);
	}
}


void CGameFileInfo::GetRelativeName(FString& OutName) const
{
	const FString& Folder = GetPath();
	if (Folder.IsEmpty())
	{
		OutName = ShortFilename;
	}
	else
	{
		// Combine text inside FString instead of using appSptring and assignment
		OutName = Folder;
		OutName += "/";
		OutName += ShortFilename;
	}
}

FString CGameFileInfo::GetRelativeName() const
{
	FString Result;
	GetRelativeName(Result);
	return Result;
}

void CGameFileInfo::GetCleanName(FString& OutName) const
{
	OutName = ShortFilename;
}

/*static*/ const FString& CGameFileInfo::GetPathByIndex(int index)
{
	assert(GameFolders.IsValidIndex(index));
	return GameFolders.GetData()[index].Name; // using GetData to avoid another one index check
}

/*static*/ int CGameFileInfo::CompareNames(const CGameFileInfo& A, const CGameFileInfo& B)
{
	const FString& PathA = A.GetPath();
	const FString& PathB = B.GetPath();
	// Compare pointers to strings - if path is same, pointers are also same
	if (&PathA == &PathB)
	{
		return stricmp(A.ShortFilename, B.ShortFilename);
	}
	//todo: compare indices if path will be sorted
	return stricmp(*PathA, *PathB);
}

void appEnumGameFilesWorker(EnumGameFilesCallback_t Callback, const char *Ext, void *Param)
{
	guard(appEnumGameFilesWorker);
	for (const CGameFileInfo *info : GameFiles)
	{
		if (!Ext)
		{
			// enumerate packages
			if (!info->IsPackage()) continue;
		}
		else
		{
			// check extension
			if (stricmp(info->GetExtension(), Ext) != 0) continue;
		}
		if (!Callback(info, Param)) break;
	}
	unguard;
}

void appEnumGameFoldersWorker(EnumGameFoldersCallback_t Callback, void *Param)
{
	guard(appEnumGameFoldersWorker);
	for (const CGameFolderInfo& info : GameFolders)
	{
		if (!Callback(info.Name, info.NumFiles, Param)) break;
	}
	unguard;
}
