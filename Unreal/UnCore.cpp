#include "Core.h"
#include "UnCore.h"

// includes for file enumeration
#if _WIN32
#	include <io.h>					// for findfirst() set
#else
#	include <dirent.h>				// for opendir() etc
#	include <sys/stat.h>			// for stat()
#endif

// includes for package decompression
#include "lzo/lzo1x.h"
#include "zlib/zlib.h"

#if XBOX360

#	if !USE_XDK

#		include "mspack/mspack.h"
#		include "mspack/lzx.h"

#	else // USE_XDK

#		if _WIN32
#		pragma comment(lib, "xcompress.lib")
		extern "C"
		{
			int  __stdcall XMemCreateDecompressionContext(int CodecType, const void* pCodecParams, unsigned Flags, void** pContext);
			void __stdcall XMemDestroyDecompressionContext(void* Context);
			int  __stdcall XMemDecompress(void* Context, void* pDestination, size_t* pDestSize, const void* pSource, size_t SrcSize);
		}
#		else  // _WIN32
#			error XDK build is not supported on this platform
#		endif // _WIN32
#	endif // USE_XDK

#endif // XBOX360


//#define DEBUG_BULK			1
//#define DEBUG_RAW_ARRAY		1


int  GForceGame     = GAME_UNKNOWN;
byte GForcePlatform = PLATFORM_UNKNOWN;


#if PROFILE

int GNumSerialize = 0;
int GSerializeBytes = 0;
static int ProfileStartTime = -1;

void appResetProfiler()
{
	GNumAllocs = GNumSerialize = GSerializeBytes = 0;
	ProfileStartTime = appMilliseconds();
}


void appPrintProfiler()
{
	if (ProfileStartTime == -1) return;
	appPrintf("Loaded in %.2g sec, %d allocs, %.2f MBytes serialized in %d calls.\n",
		(appMilliseconds() - ProfileStartTime) / 1000.0f, GNumAllocs, GSerializeBytes / (1024.0f * 1024.0f), GNumSerialize);
	appResetProfiler();
}

#endif // PROFILE


/*-----------------------------------------------------------------------------
	ZLib support
-----------------------------------------------------------------------------*/

// using Core memory manager

extern "C" void *zcalloc(int opaque, int items, int size)
{
	return appMalloc(items * size);
}

extern "C" void zcfree(int opaque, void *ptr)
{
	appFree(ptr);
}


/*-----------------------------------------------------------------------------
	Game file system
-----------------------------------------------------------------------------*/

#define MAX_GAME_FILES			32768		// DC Universe Online has more than 20k upk files
#define MAX_FOREIGN_FILES		32768

static char RootDirectory[256];


static const char *PackageExtensions[] =
{
	"u", "ut2", "utx", "uax", "usx", "ukx",
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
#if UNREAL3
	"upk", "ut3", "xxx", "umap", "udk", "map",
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
};
#endif

// By default umodel extracts data to the current directory. Working with a huge amount of files
// could result to get "too much unknown files" error. We can ignore types of files which are
// extracted by umodel to reduce chance to get such error.
static const char *SkipExtensions[] =
{
	"tga", "dds", "bmp", "mat",				// textures, materials
	"psk", "pskx", "psa", "config",			// meshes, animations
	"ogg", "wav", "fsb", "xma", "unk",		// sounds
	"gfx", "fxa",							// 3rd party
	"md5mesh", "md5anim",					// md5 mesh
	"uc", "3d",								// vertex mesh
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


static CGameFileInfo *GameFiles[MAX_GAME_FILES];
static int           NumGameFiles = 0;
static int           NumForeignFiles = 0;


static bool RegisterGameFile(const char *FullName)
{
	guard(RegisterGameFile);

//	printf("..file %s\n", FullName);
	// return false when MAX_GAME_FILES
	if (NumGameFiles >= ARRAY_COUNT(GameFiles))
		return false;

	bool IsPackage;
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
			// perhaps this file was exported by our tool - skip it
			if (FindExtension(FullName, ARRAY_ARG(SkipExtensions)))
				return true;
			// unknown file type
			if (++NumForeignFiles >= MAX_FOREIGN_FILES)
				appError("Too much unknown files - bad root directory?");
			return true;
		}
		IsPackage = false;
	}

	// create entry
	CGameFileInfo *info = new CGameFileInfo;
	GameFiles[NumGameFiles++] = info;
	info->IsPackage = IsPackage;

	// cut RootDirectory from filename
	const char *s = FullName + strlen(RootDirectory) + 1;
	assert(s[-1] == '/');
	appStrncpyz(info->RelativeName, s, ARRAY_COUNT(info->RelativeName));
	// find filename
	s = strrchr(info->RelativeName, '/');
	if (s) s++; else s = info->RelativeName;
	info->ShortFilename = s;
	// find extension
	s = strrchr(info->ShortFilename, '.');
	if (s) s++;
	info->Extension = s;
//	printf("..  -> %s (pkg=%d)\n", info->ShortFilename, info->IsPackage);

	return true;

	unguardf(("%s", FullName));
}


static bool ScanGameDirectory(const char *dir, bool recurse)
{
	guard(ScanGameDirectory);

	char Path[256];
	bool res = true;
//	printf("Scan %s\n", dir);
#if _WIN32
	appSprintf(ARRAY_ARG(Path), "%s/*.*", dir);
	_finddatai64_t found;
	long hFind = _findfirsti64(Path, &found);
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
		struct stat buf;
		if (stat(Path, &buf) < 0) continue;			// or break?
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
	appStrncpyz(RootDirectory, dir, ARRAY_COUNT(RootDirectory));
	ScanGameDirectory(RootDirectory, recurse);
	appPrintf("Found %d game files (%d skipped)\n", NumGameFiles, NumForeignFiles);
	unguardf(("dir=%s", dir));
}


const char *appGetRootDirectory()
{
	return RootDirectory[0] ? RootDirectory : NULL;
}


static const char *KnownDirs[] =
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

#if UNREAL3
const char *GStartupPackage = "startup_xxx";
#endif


void appSetRootDirectory2(const char *filename)
{
	char buf[256], buf2[256];
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
	// analyze path
	bool detected = false;
	const char *pCooked = NULL;
	for (int i = 0; i < 8; i++)
	{
		// find deepest directory name
		s = strrchr(buf, '/');
		if (!s) break;
		*s++ = 0;
		if (i == 0)
		{
			for (int j = 0; j < ARRAY_COUNT(KnownDirs); j++)
				if (!stricmp(KnownDirs[j], s))
				{
					detected = true;
					break;
				}
		}
		if (detected) break;
		pCooked = appStristr(s, "Cooked");
		if (pCooked || appStristr(s, "Content"))
		{
			s[-1] = '/';	// put it back
			detected = true;
			break;
		}
	}
	const char *root = (detected) ? buf : buf2;
	appPrintf("Detected game root %s%s", root, (detected == false) ? " (no recurse)" : "");
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
			};
			staticAssert(ARRAY_COUNT(PlatformNames) == PLATFORM_COUNT, Verify_PlatformNames);
			appPrintf("; platform %s", PlatformNames[GForcePlatform]);
		}
	}
	// scan root directory
	appPrintf("\n");
	appSetRootDirectory(root, detected);
}


const CGameFileInfo *appFindGameFile(const char *Filename, const char *Ext)
{
	guard(appFindGameFile);

	char buf[256];
	appStrncpyz(buf, Filename, ARRAY_COUNT(buf));

	// validate name
	if (strchr(Filename, '/') || strchr(Filename, '\\'))
	{
		appError("appFindGameFile: file has path");
	}

	if (Ext)
	{
		// extension is provided
		assert(!strchr(buf, '.'));
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

#if UNREAL3
	bool findStartupPackage = (strcmp(Filename, GStartupPackage) == 0);
#endif

	int nameLen = strlen(buf);
	CGameFileInfo *info = NULL;
	for (int i = 0; i < NumGameFiles; i++)
	{
		CGameFileInfo *info2 = GameFiles[i];
#if UNREAL3
		// check for startup package
		// possible variants:
		// - startup
		if (findStartupPackage)
		{
			if (strnicmp(info2->ShortFilename, "startup", 7) != 0)
				continue;
			if (info2->ShortFilename[7] == '.')
				return info2;							// "startup.upk" (DCUO, may be others too)
			if (strnicmp(info2->ShortFilename+7, "_int.", 5) == 0)
				return info2;							// "startup_int.upk"
			if (info2->ShortFilename[7] == '_')
				info = info2;							// non-int locale, lower priority - use if when other is not detected
			continue;
		}
#endif // UNREAL3
		// verify filename
		if (strnicmp(info2->ShortFilename, buf, nameLen) != 0) continue;
		if (info2->ShortFilename[nameLen] != '.') continue;
		// verify extension
		if (Ext)
		{
			if (stricmp(info2->Extension, Ext) != 0) continue;
		}
		else
		{
			// Ext = NULL => should be any package extension
			if (!info2->IsPackage) continue;
		}
		// file was found
		return info2;
	}
	return info;

	unguardf(("name=%s ext=%s", Filename, Ext));
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
	char buf[256];
	appSprintf(ARRAY_ARG(buf), "%s/%s", RootDirectory, info->RelativeName);
	return new FFileReader(buf);
}


void appEnumGameFiles(bool (*Callback)(const CGameFileInfo*), const char *Ext)
{
	for (int i = 0; i < NumGameFiles; i++)
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
		if (!Callback(info)) break;
	}
}


/*-----------------------------------------------------------------------------
	FArray
-----------------------------------------------------------------------------*/

void FArray::Empty(int count, int elementSize)
{
	guard(FArray::Empty);
	if (DataPtr)
		appFree(DataPtr);
	DataPtr   = NULL;
	DataCount = 0;
	MaxCount  = count;
	if (count)
	{
		DataPtr = appMalloc(count * elementSize);
		memset(DataPtr, 0, count * elementSize);
	}
	unguardf(("%d x %d", count, elementSize));
}


void FArray::Add(int count, int elementSize)
{
	Insert(0, count, elementSize);
}


void FArray::Insert(int index, int count, int elementSize)
{
	guard(FArray::Insert);
	assert(index >= 0);
	assert(index <= DataCount);
	assert(count >= 0);
	if (!count) return;
	// check for available space
	if (DataCount + count > MaxCount)
	{
		// not enough space, resize ...
		int prevCount = MaxCount;
		MaxCount = ((DataCount + count + 7) / 8) * 8 + 8;
#if PROFILE
		GNumAllocs++;
#endif
		DataPtr = appRealloc(DataPtr, MaxCount * elementSize);
		// zero added memory
		memset(
			(byte*)DataPtr + prevCount * elementSize,
			0,
			(MaxCount - prevCount) * elementSize
		);
	}
	// move data
	memmove(
		(byte*)DataPtr + (index + count)     * elementSize,
		(byte*)DataPtr + index               * elementSize,
						 (DataCount - index) * elementSize
	);
	// last operation: advance counter
	DataCount += count;
	unguard;
}


void FArray::Remove(int index, int count, int elementSize)
{
	guard(FArray::Remove);
	assert(index >= 0);
	assert(count > 0);
	assert(index + count <= DataCount);
	// move data
	memcpy(
		(byte*)DataPtr + index                       * elementSize,
		(byte*)DataPtr + (index + count)             * elementSize,
						 (DataCount - index - count) * elementSize
	);
	// decrease counter
	DataCount -= count;
	unguard;
}


void FArray::RawCopy(const FArray &Src, int elementSize)
{
	guard(FArray::RawCopy);

	Empty(Src.DataCount, elementSize);
	if (!Src.DataCount) return;
	DataCount = Src.DataCount;
	memcpy(DataPtr, Src.DataPtr, Src.DataCount * elementSize);

	unguard;
}


void* FArray::GetItem(int index, int elementSize) const
{
	guard(operator[]);
	assert(index >= 0 && index < DataCount);
	return OffsetPointer(DataPtr, index * elementSize);
	unguardf(("%d/%d", index, DataCount));
}


static bool GameUsesFCompactIndex(FArchive &Ar)
{
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) return false;
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard && Ar.ArVer >= 128 && Ar.ArLicenseeVer >= 25) return false;
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) return false;
#endif
	return true;
}

FArchive& FArray::Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	int i = 0;

	guard(TArray::Serialize);

//-- if (Ar.IsLoading) Empty();	-- cleanup is done in TArray serializer (do not need
//								-- to pass array eraser/destructor to this function)
	// Here:
	// 1) when loading: 'this' array is empty (cleared from TArray's operator<<)
	// 2) when saving : data is not modified by this function

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	// perform serialization itself
	void *ptr;
	for (i = 0, ptr = DataPtr; i < Count; i++, ptr = OffsetPointer(ptr, elementSize))
		Serializer(Ar, ptr);
	return Ar;

	unguardf(("%d/%d", i, DataCount));
}


void appReverseBytes(void *Block, int NumItems, int ItemSize)
{
	byte *p1 = (byte*)Block;
	byte *p2 = p1 + ItemSize - 1;
	for (int i = 0; i < NumItems; i++, p1 += ItemSize, p2 += ItemSize)
	{
		byte *p1a = p1;
		byte *p2a = p2;
		while (p1a < p2a)
		{
			Exchange(*p1a, *p2a);
			p1a++;
			p2a--;
		}
	}
}


FArchive& FArray::SerializeRaw(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	guard(TArray::SerializeRaw);

	if (Ar.ReverseBytes)	// reverse bytes -> cannot use fast serializer
		return Serialize(Ar, Serializer, elementSize);

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	if (!Count) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * Count);
	return Ar;

	unguard;
}


FArchive& FArray::SerializeSimple(FArchive &Ar, int NumFields, int FieldSize)
{
	guard(TArray::SerializeSimple);

	//?? note: SerializeSimple() can reverse bytes on loading only, saving should
	//?? be done using generic serializer, or SerializeSimple should be
	//?? extended for this

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	int elementSize = NumFields * FieldSize;
	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	if (!Count) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * Count);
	// reverse bytes when needed
	if (FieldSize > 1 && Ar.ReverseBytes)
	{
		assert(Ar.IsLoading);
		appReverseBytes(DataPtr, Count * NumFields, FieldSize);
	}
	return Ar;

	unguard;
}


FArchive& SerializeLazyArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*))
{
	guard(TLazyArray<<);
	assert(Ar.IsLoading);
	int SkipPos = 0;								// ignored
	if (Ar.ArVer > 61)
		Ar << SkipPos;
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock && Ar.ArVer >= 131)
	{
		int f10, f8;
		Ar << f10 << f8;
//		printf("bio: pos=%08X skip=%08X f10=%08X f8=%08X\n", Ar.Tell(), SkipPos, f10, f8);
		if (SkipPos < Ar.Tell())
		{
//			appNotify("Bioshock: wrong SkipPos in array at %X", Ar.Tell());
			SkipPos = 0;		// have a few places with such bug ...
		}
	}
#endif // BIOSHOCK
	Serializer(Ar, &Array);
	return Ar;
	unguard;
}

#if UNREAL3

FArchive& SerializeRawArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*))
{
	guard(TRawArray<<);
	assert(Ar.IsLoading);
#if A51
	if (Ar.Game == GAME_A51 && Ar.ArVer >= 376) goto new_ver;	// partially upgraded old engine
#endif // A51
#if STRANGLE
	if (Ar.Game == GAME_Strangle) goto new_ver;					// also check package's MidwayTag ("WOO ") and MidwayVer (>= 369)
#endif
#if AVA
	if (Ar.Game == GAME_AVA && Ar.ArVer >= 436) goto new_ver;
#endif
#if DOH
	if (Ar.Game == GAME_DOH) goto old_ver;
#endif
#if MKVSDC
	if (Ar.Game == GAME_MK) goto old_ver;
#endif
	if (Ar.ArVer >= 453)
	{
	new_ver:
		int ElementSize;
		Ar << ElementSize;
		int SavePos = Ar.Tell();
		Serializer(Ar, &Array);
#if DEBUG_RAW_ARRAY
		appPrintf("savePos=%d count=%d elemSize=%d (real=%g) tell=%d\n", SavePos + 4, Array.Num(), ElementSize,
				Array.Num() ? float(Ar.Tell() - SavePos - 4) / Array.Num() : 0,
				Ar.Tell());
#endif
		if (Ar.Tell() != SavePos + 4 + Array.Num() * ElementSize)	// check position
			appError("RawArray item size mismatch: expected %d, got %d\n", ElementSize, (Ar.Tell() - SavePos) / Array.Num());
		return Ar;
	}
old_ver:
	// old version: no ElementSize property
	Serializer(Ar, &Array);
	return Ar;
	unguard;
}

#endif // UNREAL3

/*-----------------------------------------------------------------------------
	FArchive methods
-----------------------------------------------------------------------------*/

void FArchive::ByteOrderSerialize(void *data, int size)
{
	guard(FArchive::ByteOrderSerialize);

	Serialize(data, size);
	if (!ReverseBytes || size <= 1) return;

	assert(IsLoading);
	byte *p1 = (byte*)data;
	byte *p2 = p1 + size - 1;
	while (p1 < p2)
	{
		Exchange(*p1, *p2);
		p1++;
		p2--;
	}

	unguard;
}


void FArchive::Printf(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);
	Serialize(buf, len);
}


FArchive& operator<<(FArchive &Ar, FCompactIndex &I)
{
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) appError("FCompactIndex is missing in UE3");
#endif
	if (Ar.IsLoading)
	{
		byte b;
		Ar << b;
		int sign  = b & 0x80;	// sign bit
		int shift = 6;
		int r     = b & 0x3F;
		if (b & 0x40)			// has 2nd byte
		{
			do
			{
				Ar << b;
				r |= (b & 0x7F) << shift;
				shift += 7;
			} while (b & 0x80);	// has more bytes
		}
		I.Value = sign ? -r : r;
	}
	else
	{
		int v = I.Value;
		byte b = 0;
		if (v < 0)
		{
			v = -v;
			b |= 0x80;			// sign
		}
		b |= v & 0x3F;
		if (v <= 0x3F)
		{
			Ar << b;
		}
		else
		{
			b |= 0x40;			// has 2nd byte
			v >>= 6;
			Ar << b;
			assert(v);
			while (v)
			{
				b = v & 0x7F;
				v >>= 7;
				if (v)
					b |= 0x80;	// has more bytes
				Ar << b;
			}
		}
	}
	return Ar;
}


FString::FString(const char* src)
{
	int len = strlen(src) + 1;
	Add(len);
	memcpy(DataPtr, src, len);
}


char* FString::Detach()
{
	char* data = (char*)DataPtr;
	DataPtr   = NULL;
	DataCount = 0;
	MaxCount  = 0;
	return data;
}


FArchive& operator<<(FArchive &Ar, FString &S)
{
	guard(FString<<);

	if (!Ar.IsLoading)
	{
		Ar << (TArray<char>&)S;
		return Ar;
	}

	// loading

	// serialize character count
	int len;
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		Ar << AR_INDEX(len);		// Bioshock serialized positive number, but it's string is always unicode
		len = -len;
	}
	else
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard)	// this game uses int for arrays, but FCompactIndex for strings
		Ar << AR_INDEX(len);
	else
#endif
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(len);
	else
		Ar << len;

	S.Empty((len >= 0) ? len : -len);

	// resialize the string
	if (!len)
	{
		// empty FString
		// original UE has array count == 0 and special handling when converting FString
		// to char*
		S.AddItem(0);
		return Ar;
	}

	if (len > 0)
	{
		// ANSI string
		S.Add(len);
		Ar.Serialize(S.GetData(), len);
	}
	else
	{
		// UNICODE string
		for (int i = 0; i < -len; i++)
		{
			word c;
			Ar << c;
#if MASSEFF
			if (Ar.Game == GAME_MassEffect3 && Ar.ReverseBytes)	// uses little-endian strings for XBox360
				c = (c >> 8) | ((c & 0xFF) << 8);
#endif
			if (c & 0xFF00) c = '$';	//!! incorrect ...
			S.AddItem(c & 255);			//!! incorrect ...
		}
	}
/*	for (i = 0; i < S.Num(); i++) -- disabled 21.03.2012, Unicode chars are "fixed" before S.AddItem() above
	{
		char c = S[i];
		if (c >= 1 && c < ' ')
			S[i] = '$';
	} */
	if (S[abs(len)-1] != 0)
		appError("Serialized FString is not null-terminated");
	return Ar;

	unguard;
}


void FArchive::DetectGame()
{
	if (GForcePlatform != PLATFORM_UNKNOWN)
		Platform = GForcePlatform;

	if (GForceGame != GAME_UNKNOWN)
	{
		Game = GForceGame;
		return;
	}

	// different game platforms autodetection
	//?? should change this, if will implement command line switch to force mode
	//?? code moved here, check code of other structs loaded below for ability to use Ar.IsGameName...

	//?? remove #if ... #endif guards - detect game even when its support is disabled

	// check for already detected game
#if LINEAGE2 || EXTEEL
	if (Game == GAME_Lineage2)
	{
		if (ArLicenseeVer >= 1000)	// lineage LicenseeVer < 1000, exteel >= 1000
			Game = GAME_Exteel;
		return;
	}
#endif
	if (Game != GAME_UNKNOWN)		// may be GAME_Ragnarok2
		return;

	// here Game == GAME_UNKNOWN
	int check = 0;					// number of detected games; should be 0 or 1, otherwise autodetect is failed
#define SET(game)	{ Game = game; check++; }

	/*-----------------------------------------------------------------------
	 * UE2 games
	 *-----------------------------------------------------------------------*/
	// Digital Extremes games
#if UT2
	if ( ((ArVer >= 117 && ArVer <= 119) && (ArLicenseeVer >= 25 && ArLicenseeVer <= 27)) ||
		  (ArVer == 120 && (ArLicenseeVer == 27 || ArLicenseeVer == 28)) ||
		 ((ArVer >= 121 && ArVer <= 128) && ArLicenseeVer == 29) )
		SET(GAME_UT2);
#endif
#if PARIAH
	if (ArVer == 119 && ArLicenseeVer == 0x9127)
		SET(GAME_Pariah);
#endif
#if UC1
	if (ArVer == 119 && (ArLicenseeVer == 28 || ArLicenseeVer == 30))
		SET(GAME_UC1);
#endif
#if UC2
	if (ArVer == 151 && (ArLicenseeVer == 0 || ArLicenseeVer == 1))
		SET(GAME_UC2);
#endif

#if LOCO
	if ((ArVer >= 131 && ArVer <= 134) && ArLicenseeVer == 29)
		SET(GAME_Loco);
#endif
#if SPLINTER_CELL
	if ( (ArVer == 100 && (ArLicenseeVer >= 9 && ArLicenseeVer <= 17)) ||		// Splinter Cell 1
		 (ArVer == 102 && (ArLicenseeVer >= 29 && ArLicenseeVer <= 28)) )		// Splinter Cell 2
		SET(GAME_SplinterCell);
#endif
#if SWRC
	if ( ArLicenseeVer == 1 && (
		(ArVer >= 133 && ArVer <= 148) || (ArVer >= 154 && ArVer <= 159)
		) )
		SET(GAME_RepCommando);
#endif
#if TRIBES3
	if ( ((ArVer == 129 || ArVer == 130) && (ArLicenseeVer >= 0x17 && ArLicenseeVer <= 0x1B)) ||
		 ((ArVer == 123) && (ArLicenseeVer >= 3    && ArLicenseeVer <= 0xF )) ||
		 ((ArVer == 126) && (ArLicenseeVer >= 0x12 && ArLicenseeVer <= 0x17)) )
		SET(GAME_Tribes3);
#endif
#if BIOSHOCK
	if ( (ArVer == 141 && (ArLicenseeVer == 56 || ArLicenseeVer == 57)) || //?? Bioshock and Bioshock 2
		 (ArVer == 143 && ArLicenseeVer == 59) )					// Bioshock 2 multiplayer?
		SET(GAME_Bioshock);
#endif

	/*-----------------------------------------------------------------------
	 * UE3 games
	 *-----------------------------------------------------------------------*/
	// most UE3 games has single version for all packages
	// here is a list of such games, sorted by version
#if R6VEGAS
	if (ArVer == 241 && ArLicenseeVer == 71)	SET(GAME_R6Vegas2);
#endif
//#if ENDWAR
//	if (ArVer == 329 && ArLicenseeVer == 0)		SET(GAME_EndWar);	// LicenseeVer == 0
//#endif
#if STRANGLE
	if (ArVer == 375 && ArLicenseeVer == 25)	SET(GAME_Strangle);	//!! has extra tag
#endif
#if A51
	if (ArVer == 377 && ArLicenseeVer == 25)	SET(GAME_A51);		//!! has extra tag
#endif
#if TNA_IMPACT
	if (ArVer == 380 && ArLicenseeVer == 35)	SET(GAME_TNA);		//!! has extra tag
#endif
#if WHEELMAN
	if (ArVer == 390 && ArLicenseeVer == 32)	SET(GAME_Wheelman);	//!! has extra tag
#endif
#if FURY
	if (ArVer == 407 && (ArLicenseeVer == 26 || ArLicenseeVer == 36)) SET(GAME_Fury);
#endif
#if MOHA
	if (ArVer == 421 && ArLicenseeVer == 11)	SET(GAME_MOHA);
#endif
#if UNDERTOW
//	if (ArVer == 435 && ArLicenseeVer == 0)		SET(GAME_Undertow);	// LicenseeVer==0!
#endif
#if MCARTA
	if (ArVer == 446 && ArLicenseeVer == 25)	SET(GAME_MagnaCarta);
#endif
#if AVA
	if (ArVer == 451 && (ArLicenseeVer >= 52 || ArLicenseeVer <= 53)) SET(GAME_AVA);
#endif
#if DOH
	if (ArVer == 455 && ArLicenseeVer == 90)	SET(GAME_DOH);
#endif
#if TLR
	if (ArVer == 507 && ArLicenseeVer == 11)	SET(GAME_TLR);
#endif
#if MEDGE
	if (ArVer == 536 && ArLicenseeVer == 43)	SET(GAME_MirrorEdge);
#endif
#if BLOODONSAND
	if (ArVer == 538 && ArLicenseeVer == 73)	SET(GAME_50Cent);
#endif
#if ARGONAUTS
	if (ArVer == 539 && (ArLicenseeVer == 43 || ArLicenseeVer == 47)) SET(GAME_Argonauts);	// Rise of the Argonauts, Thor: God of Thunder
#endif
#if ALPHA_PR
	if (ArVer == 539 && ArLicenseeVer == 91)	SET(GAME_AlphaProtocol);
#endif
#if APB
	if (ArVer == 547 && (ArLicenseeVer == 31 || ArLicenseeVer == 32)) SET(GAME_APB);
#endif
#if LEGENDARY
	if (ArVer == 567 && ArLicenseeVer == 39)	SET(GAME_Legendary);
#endif
//#if AA3
//	if (ArVer == 568 && ArLicenseeVer == 0)		SET(GAME_AA3);	//!! LicenseeVer == 0 ! bad!
//#endif
#if XMEN
	if (ArVer == 568 && ArLicenseeVer == 101)	SET(GAME_XMen);
#endif
#if CRIMECRAFT
	if (ArVer == 576 && ArLicenseeVer == 5)		SET(GAME_CrimeCraft);
#endif
#if BATMAN
	if (ArVer == 576 && ArLicenseeVer == 21)	SET(GAME_Batman);
#endif
#if DARKVOID
	if (ArVer == 576 && (ArLicenseeVer == 61 || ArLicenseeVer == 66)) SET(GAME_DarkVoid); // demo and release
#endif
#if MOH2010
	if (ArVer == 581 && ArLicenseeVer == 58)	SET(GAME_MOH2010);
#endif
#if SINGULARITY
	if (ArVer == 584 && ArLicenseeVer == 126)	SET(GAME_Singularity);
#endif
#if TRON
	if (ArVer == 648 && ArLicenseeVer == 3)		SET(GAME_Tron);
#endif
#if DCU_ONLINE
	if (ArVer == 648 && ArLicenseeVer == 6405)	SET(GAME_DCUniverse);
#endif
#if ENSLAVED
	if (ArVer == 673 && ArLicenseeVer == 2)		SET(GAME_Enslaved);
#endif
#if MORTALONLINE
	if (ArVer == 678 && ArLicenseeVer == 32771) SET(GAME_MortalOnline);
#endif
#if SHADOWS_DAMNED
	if (ArVer == 706 && ArLicenseeVer == 28)	SET(GAME_ShadowsDamned);
#endif
#if BULLETSTORM
	if (ArVer == 742 && ArLicenseeVer == 29)	SET(GAME_Bulletstorm);
#endif
#if ALIENS_CM
	if (ArVer == 787 && ArLicenseeVer == 47)	SET(GAME_AliensCM);
#endif
#if DISHONORED
	if (ArVer == 801 && ArLicenseeVer == 30)	SET(GAME_Dishonored);
#endif
#if TRIBES4
	if (ArVer == 805 && ArLicenseeVer == 2)		SET(GAME_Tribes4);
#endif
#if BATMAN
	if (ArVer == 805 && ArLicenseeVer == 101)	SET(GAME_Batman2);
#endif
#if DMC
	if (ArVer == 845 && ArLicenseeVer == 4)		SET(GAME_DmC);
#endif
#if FABLE
	if (ArVer == 850 && ArLicenseeVer == 1017)	SET(GAME_Fable);
#endif
#if SPECIALFORCE2
	if (ArVer == 904 && (ArLicenseeVer == 9 || ArLicenseeVer == 14)) SET(GAME_SpecialForce2);
#endif

	// UE3 games with the various versions of files
#if TUROK
	if ( (ArVer == 374 && ArLicenseeVer == 16) ||
		 (ArVer == 375 && ArLicenseeVer == 19) ||
		 (ArVer == 392 && ArLicenseeVer == 23) ||
		 (ArVer == 393 && (ArLicenseeVer >= 27 && ArLicenseeVer <= 61)) )
		SET(GAME_Turok);
#endif
#if MASSEFF
	if ((ArVer == 391 && ArLicenseeVer == 92) ||		// XBox 360 version
		(ArVer == 491 && ArLicenseeVer == 1008))		// PC version
		SET(GAME_MassEffect);
	if (ArVer == 512 && ArLicenseeVer == 130)
		SET(GAME_MassEffect2);
	if (ArVer == 684 && (ArLicenseeVer == 185 || ArLicenseeVer == 194)) // 185 = demo, 194 = release
		SET(GAME_MassEffect3);
#endif
#if MKVSDC
	if ( (ArVer == 402 && ArLicenseeVer == 30) ||		//!! has extra tag
		 (ArVer == 472 && ArLicenseeVer == 46) )
		SET(GAME_MK);
#endif
#if HUXLEY
	if ( (ArVer == 402 && (ArLicenseeVer == 0  || ArLicenseeVer == 10)) ||	//!! has extra tag
		 (ArVer == 491 && (ArLicenseeVer >= 13 && ArLicenseeVer <= 16)) ||
		 (ArVer == 496 && (ArLicenseeVer >= 16 && ArLicenseeVer <= 23)) )
		SET(GAME_Huxley);
#endif
#if FRONTLINES
	if ( (ArVer == 433 && ArLicenseeVer == 52) ||		// Frontlines: Fuel of War
		 (ArVer == 576 && ArLicenseeVer == 100) )		// Homefront
		SET(GAME_Frontlines);
#endif
#if ARMYOF2
	if ( (ArVer == 445 && ArLicenseeVer == 79)  ||		// Army of Two
		 (ArVer == 482 && ArLicenseeVer == 222) ||		// Army of Two: the 40th Day
		 (ArVer == 483 && ArLicenseeVer == 4317) )		// ...
		SET(GAME_ArmyOf2);
#endif
#if TRANSFORMERS
	if ( (ArVer == 511 && ArLicenseeVer == 39 ) ||		// The Bourne Conspiracy
		 (ArVer == 511 && ArLicenseeVer == 145) ||		// Transformers: War for Cybertron (PC version)
		 (ArVer == 511 && ArLicenseeVer == 144) ||		// Transformers: War for Cybertron (PS3 and XBox 360 version)
		 (ArVer == 537 && ArLicenseeVer == 174) ||		// Transformers: Dark of the Moon
		 (ArVer == 846 && ArLicenseeVer == 181) )		// Transformers: Fall of Cybertron
		SET(GAME_Transformers);
#endif
#if BORDERLANDS
	if ( (ArVer == 512 && ArLicenseeVer == 35) ||		// Brothers in Arms: Hell's Highway
		 (ArVer == 584 && (ArLicenseeVer == 57 || ArLicenseeVer == 58)) || // Borderlands: release and update
		 (ArVer == 832 && ArLicenseeVer == 46) )		// Borderlands 2
		SET(GAME_Borderlands);
#endif
#if TERA
	if ((ArVer == 568 && (ArLicenseeVer >= 9 && ArLicenseeVer <= 10)) ||
		(ArVer == 610 && (ArLicenseeVer >= 13 && ArLicenseeVer <= 14)))
		SET(GAME_Tera);
#endif

	if (check > 1)
		appNotify("DetectGame detected a few titles (%d): Ver=%d, LicVer=%d", check, ArVer, ArLicenseeVer);

	if (Game == GAME_UNKNOWN)
	{
		// generic or unknown engine
		if (ArVer < PACKAGE_V2)
			Game = GAME_UE1;
		else if (ArVer < PACKAGE_V3)
			Game = GAME_UE2;
		else
			Game = GAME_UE3;
	}
#undef SET
}


#define OVERRIDE_ENDWAR_VER		224
#define OVERRIDE_TERA_VER		568
#define OVERRIDE_HUNTED_VER		708			// real version is 709, which is incorrect
#define OVERRIDE_DND_VER		673			// real version is 674
#define OVERRIDE_ME1_LVER		90			// real version is 1008, which is greater than LicenseeVersion of Mass Effect 2 and 3
#define OVERRIDE_TRANSFORMERS3	566			// real version is 846
#define OVERRIDE_SF2_VER		700
#define OVERRIDE_SF2_VER2		710
#define OVERRIDE_GOWJ			828			// real version is 846

void FArchive::OverrideVersion()
{
	int OldVer  = ArVer;
	int OldLVer = ArLicenseeVer;
#if ENDWAR
	if (Game == GAME_EndWar)	ArVer = OVERRIDE_ENDWAR_VER;
#endif
#if TERA
	if (Game == GAME_Tera)		ArVer = OVERRIDE_TERA_VER;
#endif
#if HUNTED
	if (Game == GAME_Hunted)	ArVer = OVERRIDE_HUNTED_VER;
#endif
#if DND
	if (Game == GAME_DND)		ArVer = OVERRIDE_DND_VER;
#endif
#if MASSEFF
	if (Game == GAME_MassEffect) ArLicenseeVer = OVERRIDE_ME1_LVER;
#endif
#if TRANSFORMERS
	if (Game == GAME_Transformers && ArLicenseeVer >= 181) ArVer = OVERRIDE_TRANSFORMERS3; // Transformers: Fall of Cybertron
#endif
#if SPECIALFORCE2
	if (Game == GAME_SpecialForce2)
	{
		// engine for this game is upgraded without changing ArVer, they have ArVer set too high and changind ArLicenseeVer only
		if (ArLicenseeVer >= 14)
			ArVer = OVERRIDE_SF2_VER2;
		else if (ArLicenseeVer == 9)
			ArVer = OVERRIDE_SF2_VER;
	}
#endif // SPECIALFORCE2
	if (Game == GAME_GoWJ)
	{
		ArVer = OVERRIDE_GOWJ;
	}
	if (ArVer != OldVer || ArLicenseeVer != OldLVer)
		appPrintf("Overrided version %d/%d -> %d/%d\n", OldVer, OldLVer, ArVer, ArLicenseeVer);
}


/*-----------------------------------------------------------------------------
	Dummy archive class
-----------------------------------------------------------------------------*/

class CDummyArchive : public FArchive
{
public:
	virtual void Seek(int Pos)
	{}
	virtual void Serialize(void *data, int size)
	{}
};


static CDummyArchive DummyArchive;
FArchive *GDummySave = &DummyArchive;


/*-----------------------------------------------------------------------------
	Reading UE3 compressed chunks
-----------------------------------------------------------------------------*/

#if !USE_XDK && XBOX360

struct mspack_file
{
	byte*		buf;
	int			bufSize;
	int			pos;
	int			rest;
};

static int mspack_read(mspack_file *file, void *buffer, int bytes)
{
	guard(mspack_read);

	if (!file->rest)
	{
		// read block header
		if (file->buf[file->pos] == 0xFF)
		{
			// [0]   = FF
			// [1,2] = uncompressed block size
			// [3,4] = compressed block size
			file->rest = (file->buf[file->pos+3] << 8) | file->buf[file->pos+4];
			file->pos += 5;
		}
		else
		{
			// [0,1] = compressed size
			file->rest = (file->buf[file->pos+0] << 8) | file->buf[file->pos+1];
			file->pos += 2;
		}
		if (file->rest > file->bufSize - file->pos)
			file->rest = file->bufSize - file->pos;
	}
	if (bytes > file->rest) bytes = file->rest;
	if (!bytes) return 0;

	// copy block data
	memcpy(buffer, file->buf + file->pos, bytes);
	file->pos  += bytes;
	file->rest -= bytes;

	return bytes;
	unguard;
}

static int mspack_write(mspack_file *file, void *buffer, int bytes)
{
	guard(mspack_write);
	assert(file->pos + bytes <= file->bufSize);
	memcpy(file->buf + file->pos, buffer, bytes);
	file->pos += bytes;
	return bytes;
	unguard;
}

static void *mspack_alloc(mspack_system *self, size_t bytes)
{
	return appMalloc(bytes);
}

static void mspack_free(void *ptr)
{
	appFree(ptr);
}

static void mspack_copy(void *src, void *dst, size_t bytes)
{
	memcpy(dst, src, bytes);
}

static struct mspack_system lzxSys =
{
	NULL,				// open
	NULL,				// close
	mspack_read,
	mspack_write,
	NULL,				// seek
	NULL,				// tell
	NULL,				// message
	mspack_alloc,
	mspack_free,
	mspack_copy
};

static void appDecompressLZX(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize)
{
	guard(appDecompressLZX);

	// setup streams
	mspack_file src, dst;
	src.buf     = CompressedBuffer;
	src.bufSize = CompressedSize;
	src.pos     = 0;
	src.rest    = 0;
	dst.buf     = UncompressedBuffer;
	dst.bufSize = UncompressedSize;
	dst.pos     = 0;
	// prepare decompressor
	lzxd_stream *lzxd = lzxd_init(&lzxSys, &src, &dst, 17, 0, 256*1024, UncompressedSize);
	assert(lzxd);
	// decompress
	int r = lzxd_decompress(lzxd, UncompressedSize);
	if (r != MSPACK_ERR_OK)
		appError("lzxd_decompress() returned %d", r);
	// free resources
	lzxd_free(lzxd);

	unguard;
}

#endif // USE_XDK

int appDecompress(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize, int Flags)
{
	guard(appDecompress);

#if BLADENSOUL
	if (GForceGame == GAME_BladeNSoul && Flags == COMPRESS_LZO_ENC)	// note: GForceGame is required (to not pass 'Game' here)
	{
		if (CompressedSize >= 32)
		{
			static const char *key = "qiffjdlerdoqymvketdcl0er2subioxq";
			for (int i = 0; i < CompressedSize; i++)
				CompressedBuffer[i] ^= key[i % 32];
		}
		// overide compression
		Flags = COMPRESS_LZO;
	}
#endif // BLADENSOUL

#if 0 // TAO_YUAN -- not working anyway, LZO returns error -6
	if (GForceGame == GAME_TaoYuan)	// note: GForceGame is required (to not pass 'Game' here);
	{
		static const byte key[] =
		{
			137, 35, 95, 142, 69, 136, 243, 119, 25, 35, 111, 94, 101, 136, 243, 204,
			243, 67, 95, 158, 69, 106, 107, 187, 237, 35, 103, 142, 72, 142, 243, 0
		};
		for (int i = 0; i < CompressedSize; i++)
			CompressedBuffer[i] ^= key[i % 32];
	}
#endif // TAO_YUAN

#if 1
		//?? Alice: Madness Returns only?
		if (CompressedSize == UncompressedSize)
		{
			// CompressedSize == UncompressedSize -> no compression
			memcpy(UncompressedBuffer, CompressedBuffer, UncompressedSize);
			return UncompressedSize;
		}
#endif

	if (Flags == COMPRESS_LZO)
	{
		int r;
		r = lzo_init();
		if (r != LZO_E_OK) appError("lzo_init() returned %d", r);
		unsigned long newLen = UncompressedSize;
		r = lzo1x_decompress_safe(CompressedBuffer, CompressedSize, UncompressedBuffer, &newLen, NULL);
		if (r != LZO_E_OK) appError("lzo_decompress() returned %d", r);
		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize);
		return newLen;
	}

	if (Flags == COMPRESS_ZLIB)
	{
#if 0
		appError("appDecompress: Zlib compression is not supported");
#else
		unsigned long newLen = UncompressedSize;
		int r = uncompress(UncompressedBuffer, &newLen, CompressedBuffer, CompressedSize);
		if (r != Z_OK) appError("zlib uncompress() returned %d", r);
//		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize); -- needed by Bioshock
		return newLen;
#endif
	}

	if (Flags == COMPRESS_LZX)
	{
#if XBOX360
#	if !USE_XDK
		appDecompressLZX(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
		return UncompressedSize;
#	else
		void *context;
		int r;
		r = XMemCreateDecompressionContext(0, NULL, 0, &context);
		if (r < 0) appError("XMemCreateDecompressionContext failed");
		unsigned int newLen = UncompressedSize;
		r = XMemDecompress(context, UncompressedBuffer, &newLen, CompressedBuffer, CompressedSize);
		if (r < 0) appError("XMemDecompress failed");
		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize);
		XMemDestroyDecompressionContext(context);
		return newLen;
#	endif // USE_XDK
#else  // XBOX360
		appError("appDecompress: LZX compression is not supported");
#endif // XBOX360
	}

	appError("appDecompress: unknown compression flags: %d", Flags);
	return 0;

	unguardf(("CompSize=%d UncompSize=%d Flags=0x%X", CompressedSize, UncompressedSize, Flags));
}


#if UNREAL3

// code is similar to FUE3ArchiveReader::PrepareBuffer()
void appReadCompressedChunk(FArchive &Ar, byte *Buffer, int Size, int CompressionFlags)
{
	guard(appReadCompressedChunk);

	// read header
	FCompressedChunkHeader ChunkHeader;
	Ar << ChunkHeader;
	// prepare buffer for reading compressed data
	int BufferSize = ChunkHeader.BlockSize * 16;
	byte *ReadBuffer = (byte*)appMalloc(BufferSize);	// BlockSize is size of uncompressed data
	// read and decompress data
	for (int BlockIndex = 0; BlockIndex < ChunkHeader.Blocks.Num(); BlockIndex++)
	{
		const FCompressedChunkBlock *Block = &ChunkHeader.Blocks[BlockIndex];
		assert(Block->CompressedSize <= BufferSize);
		assert(Block->UncompressedSize <= Size);
		Ar.Serialize(ReadBuffer, Block->CompressedSize);
		appDecompress(ReadBuffer, Block->CompressedSize, Buffer, Block->UncompressedSize, CompressionFlags);
		Size   -= Block->UncompressedSize;
		Buffer += Block->UncompressedSize;
	}
	// finalize
	assert(Size == 0);			// should be comletely read
	delete ReadBuffer;
	unguard;
}


void FByteBulkData::SerializeHeader(FArchive &Ar)
{
	guard(FByteBulkData::SerializeHeader);

	if (Ar.ArVer < 266)
	{
		guard(OldBulkFormat);
		// old bulk format - evolution of TLazyArray
		// very old version: serialized EndPosition and ElementCount - exactly as TLazyArray
		assert(Ar.IsLoading);

		BulkDataFlags = 4;						// unknown
		BulkDataSizeOnDisk = INDEX_NONE;
		int EndPosition;
		Ar << EndPosition;
		if (Ar.ArVer >= 254)
			Ar << BulkDataSizeOnDisk;
		if (Ar.ArVer >= 251)
		{
			int LazyLoaderFlags;
			Ar << LazyLoaderFlags;
			assert((LazyLoaderFlags & 1) == 0);	// LLF_PayloadInSeparateFile
			if (LazyLoaderFlags & 2)
				BulkDataFlags |= BULKDATA_CompressedZlib;
		}
		if (Ar.ArVer >= 260)
		{
			FName unk;
			Ar << unk;
		}
		Ar << ElementCount;
		if (BulkDataSizeOnDisk == INDEX_NONE)
			BulkDataSizeOnDisk = ElementCount * GetElementSize();
		BulkDataOffsetInFile = Ar.Tell();
		BulkDataSizeOnDisk   = EndPosition - BulkDataOffsetInFile;
		unguard;
	}
	else
	{
		// current bulk format
		// read header
		Ar << BulkDataFlags << ElementCount;
		assert(Ar.IsLoading);
		Ar << BulkDataSizeOnDisk << BulkDataOffsetInFile;
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 128)
		{
			int BulkDataKey;
			Ar << BulkDataKey;
		}
#endif // TRANSFORMERS
	}

#if MCARTA
	if (Ar.Game == GAME_MagnaCarta && (BulkDataFlags & 0x40))	// different flags
	{
		BulkDataFlags &= ~0x40;
		BulkDataFlags |= BULKDATA_CompressedLzx;
	}
#endif // MCARTA
#if APB
	if (Ar.Game == GAME_APB && (BulkDataFlags & 0x100))			// different flags
	{
		BulkDataFlags &= ~0x100;
		BulkDataFlags |= BULKDATA_SeparateData;
	}
#endif // APB
#if DEBUG_BULK
	appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%X+%X)\n",
		Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif

	unguard;
}


void FByteBulkData::Serialize(FArchive &Ar)
{
	guard(FByteBulkData::Serialize);

	SerializeHeader(Ar);

	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
#if DEBUG_BULK
		appPrintf("bulk in separate file (flags=%X, pos=%X+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif
		return;
	}

	if (BulkDataFlags & BULKDATA_NoData)	// skip serializing
	{
#if DEBUG_BULK
		appPrintf("bulk in separate file\n");
#endif
		return;								//?? what to do with BulkData ?
	}

	if (BulkDataFlags & BULKDATA_SeparateData)
	{
		// save archive position
		int savePos, saveStopper;
		savePos     = Ar.Tell();
		saveStopper = Ar.GetStopper();
		// seek to data block and read data
		Ar.SetStopper(0);
		Ar.Seek(BulkDataOffsetInFile);
		SerializeChunk(Ar);
		// restore archive position
		Ar.Seek(savePos);
		Ar.SetStopper(saveStopper);
		return;
	}

#if TRANSFORMERS
	// PS3 sounds in Transformers has alignment to 0x8000 with filling zeros
	if (Ar.Game == GAME_Transformers && Ar.Platform == PLATFORM_PS3)
		Ar.Seek(BulkDataOffsetInFile);
#endif
	assert(BulkDataOffsetInFile == Ar.Tell());
	SerializeChunk(Ar);

	unguard;
}


void FByteBulkData::Skip(FArchive &Ar)
{
	guard(FByteBulkData::Skip);

	// we cannot simply skip data, because:
	// 1) it may me compressed
	// 2) ElementSize is variable and not stored in archive

	SerializeHeader(Ar);
	if (BulkDataOffsetInFile == Ar.Tell())
	{
		// really should check flags here, but checking position is simpler
		Ar.Seek(Ar.Tell() + BulkDataSizeOnDisk);
	}

	unguard;
}


void FByteBulkData::SerializeChunk(FArchive &Ar)
{
	guard(FByteBulkData::SerializeChunk);

	assert(!(BulkDataFlags & BULKDATA_NoData));

	// allocate array
	if (BulkData) appFree(BulkData);
	BulkData = NULL;
	int DataSize = ElementCount * GetElementSize();
	if (!DataSize) return;		// nothing to serialize
	BulkData = (byte*)appMalloc(DataSize);

	// serialize data block
	Ar.Seek(BulkDataOffsetInFile);
	if (BulkDataFlags & (BULKDATA_CompressedLzo | BULKDATA_CompressedZlib | BULKDATA_CompressedLzx))
	{
		// compressed block
		int flags = 0;
		if (BulkDataFlags & BULKDATA_CompressedZlib) flags = COMPRESS_ZLIB;
		if (BulkDataFlags & BULKDATA_CompressedLzo)  flags = COMPRESS_LZO;
		if (BulkDataFlags & BULKDATA_CompressedLzx)  flags = COMPRESS_LZX;
		appReadCompressedChunk(Ar, BulkData, DataSize, flags);
	}
#if BLADENSOUL
	else if (Ar.Game == GAME_BladeNSoul && (BulkDataFlags & BULKDATA_CompressedLzoEncr))
	{
		appReadCompressedChunk(Ar, BulkData, DataSize, COMPRESS_LZO_ENC);
	}
#endif
	else
	{
		// uncompressed block
		Ar.Serialize(BulkData, DataSize);
	}
	assert(BulkDataOffsetInFile + BulkDataSizeOnDisk == Ar.Tell());

	unguard;
}


#endif // UNREAL3
