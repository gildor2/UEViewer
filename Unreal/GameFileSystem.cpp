#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#include "UnArchiveObb.h"
#include "UnArchivePak.h"

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

static char RootDirectory[MAX_PACKAGE_PATH];


static const char *PackageExtensions[] =
{
	"u", "ut2", "utx", "uax", "usx", "ukx",
#if UNREAL3
	"upk", "ut3", "xxx", "umap", "udk", "map",
#endif
#if UNREAL4
	"uasset",
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
#define HAS_SUPORT_FILES 1
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
#	endif
};
#endif

// By default umodel extracts data to the current directory. Working with a huge amount of files
// could result to get "too many unknown files" error. We can ignore types of files which are
// extracted by umodel to reduce chance to get such error.
static const char *SkipExtensions[] =
{
	"tga", "dds", "bmp", "mat",				// textures, materials
	"psk", "pskx", "psa", "config",			// meshes, animations
	"ogg", "wav", "fsb", "xma", "unk",		// sounds
	"gfx", "fxa",							// 3rd party
	"md5mesh", "md5anim",					// md5 mesh
	"uc", "3d",								// vertex mesh
	"wem",									// WwiseAudio files
};

static bool FindExtension(const char *Filename, const char **Extensions, int NumExtensions)
{
	const char *ext = strrchr(Filename, '.');
	if (!ext) return false;
	ext++;
	for (int i = 0; i < NumExtensions; i++)
		if (!stricmp(ext, Extensions[i])) return true;
	return false;
}


static TArray<CGameFileInfo*> GameFiles;
int GNumPackageFiles = 0;
int GNumForeignFiles = 0;

#define GAME_FILE_HASH_SIZE		4096
#define GAME_FILE_HASH_MASK		(GAME_FILE_HASH_SIZE-1)

//#define PRINT_HASH_DISTRIBUTION	1
//#define DEBUG_HASH				1
//#define DEBUG_HASH_NAME			"MiniMap"

static CGameFileInfo* GGameFileHash[GAME_FILE_HASH_SIZE];


#if UNREAL3
const char*           GStartupPackage = "startup_xxx";	// this is just a magic constant pointer, content is meaningless
static CGameFileInfo* GStartupPackageInfo = NULL;
static int            GStartupPackageInfoWeight = 0;
#endif


static int GetHashForFileName(const char* FileName, bool stripExtension)
{
	const char* s1 = strrchr(FileName, '/'); // assume path delimiters are normalized
	s1 = (s1 != NULL) ? s1 + 1 : FileName;
	const char* s2 = stripExtension ? strrchr(s1, '.') : NULL;
	int len = (s2 != NULL) ? s2 - s1 : strlen(s1);

	uint16 hash = 0;
	for (int i = 0; i < len; i++)
	{
		char c = s1[i];
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; // lowercase a character
		hash = ROL16(hash, 5) - hash + ((c << 4) + c ^ 0x13F);	// some crazy hash function
	}
//	hash += (len << 6) - len;
	hash &= GAME_FILE_HASH_MASK;
#ifdef DEBUG_HASH_NAME
	if (strstr(FileName, DEBUG_HASH_NAME))
		printf("-> hash[%s] (%s,%d) -> %X\n", FileName, s1, len, hash);
#endif
	return hash;
}

#if PRINT_HASH_DISTRIBUTION

static void PrintHashDistribution()
{
	int hashCounts[1024];
	memset(hashCounts, 0, sizeof(hashCounts));
	for (int hash = 0; hash < GAME_FILE_HASH_SIZE; hash++)
	{
		int count = 0;
		for (CGameFileInfo* info = GGameFileHash[hash]; info; info = info->HashNext)
			count++;
		assert(count < ARRAY_COUNT(hashCounts));
		hashCounts[count]++;
	}
	appPrintf("Filename hash distribution:\n");
	for (int i = 0; i < ARRAY_COUNT(hashCounts); i++)
		if (hashCounts[i] > 0)
			appPrintf("%d -> %d\n", i, hashCounts[i]);
}

#endif // PRINT_HASH_DISTRIBUTION


//!! add define USE_VFS = SUPPORT_ANDROID || UNREAL4, perhaps || SUPPORT_IOS

static TArray<FVirtualFileSystem*> GFileSystems;

static bool RegisterGameFile(const char *FullName, FVirtualFileSystem* parentVfs = NULL)
{
	guard(RegisterGameFile);

//	printf("..file %s\n", FullName);

	if (!parentVfs)		// no nested VFSs
	{
		const char* ext = strrchr(FullName, '.');
		if (ext)
		{
			guard(MountVFS);

			ext++;
			FVirtualFileSystem* vfs = NULL;
			FArchive* reader = NULL;

#if SUPPORT_ANDROID
			if (!stricmp(ext, "obb"))
			{
				GForcePlatform = PLATFORM_ANDROID;
				reader = new FFileReader(FullName);
				if (!reader) return true;
				reader->Game = GAME_UE3;
				vfs = new FObbVFS(FullName);
			}
#endif // SUPPORT_ANDROID
#if UNREAL4
			if (!stricmp(ext, "pak"))
			{
				reader = new FFileReader(FullName);
				if (!reader) return true;
				reader->Game = GAME_UE4_BASE;
				vfs = new FPakVFS(FullName);
				//!! detect game by file name
			}
#endif // UNREAL4
			//!! process other VFS types here
			if (vfs)
			{
				assert(reader);
				// read VF directory
				if (!vfs->AttachReader(reader))
				{
					// something goes wrong
					appPrintf("File %s has unknown format\n", FullName);
					delete vfs;
					delete reader;
					return true;
				}
				// add game files
				int NumVFSFiles = vfs->NumFiles();
				for (int i = 0; i < NumVFSFiles; i++)
				{
					if (!RegisterGameFile(vfs->FileName(i), vfs))
						return false;
				}
				return true;
			}

			unguard;
		}
	}

	bool IsPackage = false;
	if (FindExtension(FullName, ARRAY_ARG(PackageExtensions)))
	{
		IsPackage = true;
	}
	else
	{
#if HAS_SUPORT_FILES
		if (!FindExtension(FullName, ARRAY_ARG(KnownExtensions)))
#endif
		{
			// ignore any unknown files inside VFS
			if (parentVfs) return true;
			// ignore unknown files inside "cooked" or "content" directories
			if (appStristr(FullName, "cooked") || appStristr(FullName, "content")) return true;
			// perhaps this file was exported by our tool - skip it
			if (FindExtension(FullName, ARRAY_ARG(SkipExtensions)))
				return true;
			// unknown file type
			if (++GNumForeignFiles >= MAX_FOREIGN_FILES)
				appError("Too many unknown files - bad root directory (%s)?", RootDirectory);
			return true;
		}
	}

	// create entry
	CGameFileInfo *info = new CGameFileInfo;
	GameFiles.Add(info);
	info->IsPackage = IsPackage;
	info->FileSystem = parentVfs;
	if (IsPackage) GNumPackageFiles++;

	if (!parentVfs)
	{
		// regular file
		FILE* f = fopen(FullName, "rb");
		if (f)
		{
			fseek(f, 0, SEEK_END);
			info->SizeInKb = (ftell(f) + 512) / 1024;
			fclose(f);
		}
		else
		{
			info->SizeInKb = 0;
		}
		// cut RootDirectory from filename
		const char *s = FullName + strlen(RootDirectory) + 1;
		assert(s[-1] == '/');
		info->RelativeName = appStrdupPool(s);
	}
	else
	{
		// file in virtual file system
		info->SizeInKb = (parentVfs->GetFileSize(FullName) + 512) / 1024;
		info->RelativeName = appStrdupPool(FullName);
	}

	// find filename
	const char* s = strrchr(info->RelativeName, '/');
	if (s) s++; else s = info->RelativeName;
	info->ShortFilename = s;
	// find extension
	s = strrchr(info->ShortFilename, '.');
	if (s) s++;
	info->Extension = s;

#if UNREAL3
	if (info->IsPackage && (strnicmp(info->ShortFilename, "startup", 7) == 0))
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

	// insert CGameFileInfo into hash table
	int hash = GetHashForFileName(info->ShortFilename, true);
	info->HashNext = GGameFileHash[hash];
	GGameFileHash[hash] = info;
#if DEBUG_HASH
	printf("--> add(%s) pkg=%d hash=%X\n", info->ShortFilename, info->IsPackage, hash);
#endif

	return true;

	unguardf("%s", FullName);
}

static bool ScanGameDirectory(const char *dir, bool recurse)
{
	guard(ScanGameDirectory);

	char Path[MAX_PACKAGE_PATH];
	bool res = true;
//	printf("Scan %s\n", dir);
#if _WIN32
	appSprintf(ARRAY_ARG(Path), "%s/*.*", dir);
	_finddatai64_t found;
	intptr_t hFind = _findfirsti64(Path, &found);
	if (hFind == -1) return true;
	do
	{
		if (found.name[0] == '.') continue;			// "." or ".."
		appSprintf(ARRAY_ARG(Path), "%s/%s", dir, found.name);
		// directory -> recurse
		if (found.attrib & _A_SUBDIR)
		{
			if (recurse)
				res = ScanGameDirectory(Path, recurse);
			else
				res = true;
		}
		else
			res = RegisterGameFile(Path);
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
			else
				res = true;
		}
		else
			res = RegisterGameFile(Path);
	}
	closedir(find);
#endif
	return res;

	unguard;
}


void appSetRootDirectory(const char *dir, bool recurse)
{
	guard(appSetRootDirectory);
	if (dir[0] == 0) dir = ".";	// using dir="" will cause scanning of "/dir1", "/dir2" etc (i.e. drive root)
	appStrncpyz(RootDirectory, dir, ARRAY_COUNT(RootDirectory));
	ScanGameDirectory(RootDirectory, recurse);
	appPrintf("Found %d game files (%d skipped)\n", GameFiles.Num(), GNumForeignFiles);

#if UNREAL4
	// Should process .uexp and .ubulk files, register their information for .uasset
	for (int i = 0; i < GameFiles.Num(); i++)
	{
		CGameFileInfo *info = GameFiles[i];
		char SrcFile[MAX_PACKAGE_PATH];
		appStrncpyz(SrcFile, info->RelativeName, ARRAY_COUNT(SrcFile));
		char* s = strrchr(SrcFile, '.');
		if (s && !stricmp(s, ".uasset"))
		{
			static const char* additionalExtensions[] =
			{
				".ubulk",
				".uexp",
			};
			for (int ext = 0; ext < ARRAY_COUNT(additionalExtensions); ext++)
			{
				strcpy(s, additionalExtensions[ext]);
				const CGameFileInfo* file = appFindGameFile(SrcFile);
				if (file)
				{
					info->ExtraSizeInKb += file->SizeInKb;
				}
			}
		}
	}
#endif // UNREAL4

#if PRINT_HASH_DISTRIBUTION
	PrintHashDistribution();
#endif
	unguardf("dir=%s", dir);
}


const char *appGetRootDirectory()
{
	return RootDirectory[0] ? RootDirectory : NULL;
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
	char buf[MAX_PACKAGE_PATH], buf2[MAX_PACKAGE_PATH];
	appStrncpyz(buf, filename, ARRAY_COUNT(buf));
	char *s;
	// replace slashes
	for (s = buf; *s; s++)
		if (*s == '\\') *s = '/';
	// cut filename
	s = strrchr(buf, '/');
	*s = 0;
	// make a copy for fallback
	strcpy(buf2, buf);

	FString root;
	int detected = 0;				// weigth; 0 = not detected
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
	// detect platform
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
				"iPhone",
				"Android",
			};
			staticAssert(ARRAY_COUNT(PlatformNames) == PLATFORM_COUNT, Verify_PlatformNames);
			appPrintf("; platform %s", PlatformNames[GForcePlatform]);
		}
	}
	// scan root directory
	appPrintf("\n");
	appSetRootDirectory(*root, detected != 0);
}


const CGameFileInfo *appFindGameFile(const char *Filename, const char *Ext)
{
	guard(appFindGameFile);

#if UNREAL3
	bool findStartupPackage = (strcmp(Filename, GStartupPackage) == 0);
	if (findStartupPackage)
		return GStartupPackageInfo;
#endif

	char buf[MAX_PACKAGE_PATH];
	appStrncpyz(buf, Filename, ARRAY_COUNT(buf));

	// replace backslashes
	const char* ShortFilename = buf;
	for (char* s = buf; *s; s++)
	{
		char c = *s;
		if (c == '\\') *s = '/';
		if (*s == '/') ShortFilename = s + 1;
	}

	// Get hash before stripping extension (could be required for files with double extension, like .hdr.rtc for games with Redux textures)
	int hash = GetHashForFileName(buf, /* stripExtension = */ Ext == NULL);
#if DEBUG_HASH
	printf("--> find(%s) hash=%X\n", buf, hash);
#endif

	if (Ext)
	{
		// extension is provided
		//assert(!strchr(buf, '.')); -- don't assert because Dungeon Defenders (and perhaps other games) has TFC file names with dots
	}
	else
	{
		// check for extension in filename
		char *s = strrchr(buf, '.');
		if (s)
		{
			Ext = s + 1;	// remember extension
			*s = 0;			// cut extension
		}
	}

	int nameLen = strlen(ShortFilename);
#if defined(DEBUG_HASH_NAME) || DEBUG_HASH
	printf("--> Loading %s (%s, len=%d, hash=%X)\n", buf, ShortFilename, nameLen, hash);
#endif

	CGameFileInfo* bestMatch = NULL;
	int bestMatchWeight = -1;
	for (CGameFileInfo* info = GGameFileHash[hash]; info; info = info->HashNext)
	{
#if defined(DEBUG_HASH_NAME) || DEBUG_HASH
		printf("----> verify %s\n", info->RelativeName);
#endif
		if (info->Extension - 1 - info->ShortFilename != nameLen)	// info->Extension points to char after '.'
		{
//			printf("-----> wrong length %d\n", info->Extension - info->ShortFilename);
			continue;		// different filename length
		}

		// verify extension
		if (Ext)
		{
			if (stricmp(info->Extension, Ext) != 0) continue;
		}
		else
		{
			// Ext = NULL => should be any package extension
			if (!info->IsPackage) continue;
		}

		// verify a filename
		if (strnicmp(info->ShortFilename, ShortFilename, nameLen) != 0)
			continue;
//		if (info->ShortFilename[nameLen] != '.') -- verified before extension comparison
//			continue;

		// Short filename matched, now compare path before the filename.
		// Assume 'ShortFilename' is part of 'buf' and 'info->ShortFilename' is part of 'info->RelativeName'.
		int matchWeight = 0;
		const char *s = ShortFilename;
		const char *d = info->ShortFilename;
		while (--s >= buf && --d >= info->RelativeName)
		{
			if (*s != *d) break;
			matchWeight++;
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

	unguardf("name=%s ext=%s", Filename, Ext);
}


struct FindPackageWildcardData
{
	TArray<const CGameFileInfo*> FoundFiles;
	bool			WildcardContainsPath;
	FString			Wildcard;
};

static bool FindPackageWildcardCallback(const CGameFileInfo *file, FindPackageWildcardData &data)
{
	bool useThisPackage = false;
	if (data.WildcardContainsPath)
	{
		useThisPackage = appMatchWildcard(file->RelativeName, *data.Wildcard, true);
	}
	else
	{
		useThisPackage = appMatchWildcard(file->ShortFilename, *data.Wildcard, true);
	}
	if (useThisPackage)
	{
		data.FoundFiles.Add(file);
	}
	return true;
}


//?? TODO: may be pass "Ext" here
void appFindGameFiles(const char *Filename, TArray<const CGameFileInfo*>& Files)
{
	guard(appFindGameFiles);

	if (!appContainsWildcard(Filename))
	{
		const CGameFileInfo* File = appFindGameFile(Filename);
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
	if (!RootDirectory[0]) return Filename;

	const char *str1 = RootDirectory;
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
		if (!c2) return Filename;		// Filename is shorter than RootDirectory
		if (c1 != c2) return Filename;	// different names
	}
}


FArchive *appCreateFileReader(const CGameFileInfo *info)
{
	if (!info->FileSystem)
	{
		// regular file
		char buf[MAX_PACKAGE_PATH];
		appSprintf(ARRAY_ARG(buf), "%s/%s", RootDirectory, info->RelativeName);
		return new FFileReader(buf);
	}
	else
	{
		// file from virtual file system
		return info->FileSystem->CreateReader(info->RelativeName);
	}
}


void appEnumGameFilesWorker(bool (*Callback)(const CGameFileInfo*, void*), const char *Ext, void *Param)
{
	for (int i = 0; i < GameFiles.Num(); i++)
	{
		const CGameFileInfo *info = GameFiles[i];
		if (!Ext)
		{
			// enumerate packages
			if (!info->IsPackage) continue;
		}
		else
		{
			// check extension
			if (stricmp(info->Extension, Ext) != 0) continue;
		}
		if (!Callback(info, Param)) break;
	}
}
