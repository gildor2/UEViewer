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
#if UNREAL3
#	include "lzo/lzo1x.h"
#	include "zlib/zlib.h"

#	if XBOX360

#		if !USE_XDK

			extern "C"
			{
#			include "mspack/mspack.h"
#			include "mspack/lzx.h"
			}

#		else // USE_XDK

#			if _WIN32
#			pragma comment(lib, "xdecompress.lib")
			extern "C"
			{
				int  __stdcall XMemCreateDecompressionContext(int CodecType, const void* pCodecParams, unsigned Flags, void** pContext);
				void __stdcall XMemDestroyDecompressionContext(void* Context);
				int  __stdcall XMemDecompress(void* Context, void* pDestination, size_t* pDestSize, const void* pSource, size_t SrcSize);
			}
#			else  // _WIN32
#				error XDK build is not supported on this platform
#			endif // _WIN32
#		endif // USE_XDK

#	endif // XBOX360

#endif // UNREAL3


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
	printf("Loaded in %.2g sec, %d allocs, %.2f MBytes serialized in %d calls.\n",
		(appMilliseconds() - ProfileStartTime) / 1000.0f, GNumAllocs, GSerializeBytes / (1024.0f * 1024.0f), GNumSerialize);
	ProfileStartTime = -1;
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

#define MAX_GAME_FILES			8192
#define MAX_FOREIGN_FILES		16384

static char RootDirectory[256];


static const char *PackageExtensions[] =
{
	"u", "ut2", "utx", "uax", "usx", "ukx"
#if RUNE
	, "ums"
#endif
#if TRIBES3
	, "pkg"
#endif
#if BIOSHOCK
	, "bsm"
#endif
#if UNREAL3
	, "upk", "ut3", "xxx", "umap"
#endif
#if MASSEFF
	, "sfm"			// Mass Effect
	, "pcc"			// Mass Effect 2
#endif
#if TLR
	, "tlr"
#endif
	// other games with no special code
	, "lm"			// Landmass
	, "s8m"			// Section 8 map
	, "ccpk"		// Crime Craft character package
};

#if UNREAL3 || UC2
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
	"blk", "bdc"	// Bulk Content + Catalog
#	endif
};
#endif


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

	bool IsPackage = false;
	if (!FindExtension(FullName, ARRAY_ARG(PackageExtensions)))
	{
#if UNREAL3 || UC2
		if (!FindExtension(FullName, ARRAY_ARG(KnownExtensions)))
#endif
		{
			// unknown file type
			if (++NumForeignFiles >= MAX_FOREIGN_FILES)
				appError("Too much unknown files - bad root directory?");
			return true;
		}
	}
	else
		IsPackage = true;

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
	printf("Found %d game files (%d skipped)\n", NumGameFiles, NumForeignFiles);
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
	for (int i = 0; i < 4; i++)
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
		if (strstr(s, "Cooked") || strstr(s, "Content"))
		{
			s[-1] = '/';	// put it back
			detected = true;
			break;
		}
	}
	const char *root = (detected) ? buf : buf2;
	printf("Detected game root %s%s\n", root, (detected == false) ? " (no recurse)" : "");
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

	CGameFileInfo *info = NULL;
	int nameLen = strlen(buf);
	for (int i = 0; i < NumGameFiles; i++)
	{
		CGameFileInfo *info2 = GameFiles[i];
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
		info = info2;
		break;
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
	unguard;
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
		DataPtr = realloc(DataPtr, MaxCount * elementSize);	//?? appRealloc
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


FArchive& FArray::Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	guard(TArray::Serialize);

//-- if (Ar.IsLoading) Empty();	-- cleanup is done in TArray serializer (do not need
//								-- to pass array eraser/destructor to this function)
	// Here:
	// 1) when loading: 'this' array is empty (cleared from TArray's operator<<)
	// 2) when saving : data is not modified by this function

	// serialize data count
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) Ar << DataCount;
	else
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) Ar << DataCount;
	else
#endif
		Ar << AR_INDEX(DataCount);

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		// read data count
		// allocate space for data
		DataPtr  = (DataCount) ? appMalloc(elementSize * DataCount) : NULL;
		MaxCount = DataCount;
	}
	// perform serialization itself
	int i;
	void *ptr;
	for (i = 0, ptr = DataPtr; i < DataCount; i++, ptr = OffsetPointer(ptr, elementSize))
		Serializer(Ar, ptr);
	return Ar;

	unguard;
}


static void ReverseBytes(void *Block, int NumItems, int ItemSize)
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
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) Ar << DataCount;
	else
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) Ar << DataCount;
	else
#endif
		Ar << AR_INDEX(DataCount);

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		// read data count
		// allocate space for data
		DataPtr  = (DataCount) ? appMalloc(elementSize * DataCount) : NULL;
		MaxCount = DataCount;
	}
	if (!DataCount) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * DataCount);
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
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) Ar << DataCount;
	else
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) Ar << DataCount;
	else
#endif
		Ar << AR_INDEX(DataCount);

	int elementSize = NumFields * FieldSize;
	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		// read data count
		// allocate space for data
		DataPtr  = (DataCount) ? appMalloc(elementSize * DataCount) : NULL;
		MaxCount = DataCount;
	}
	if (!DataCount) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * DataCount);
	// reverse bytes when needed
	if (FieldSize > 1 && Ar.ReverseBytes)
	{
		assert(Ar.IsLoading);
		ReverseBytes(DataPtr, DataCount * NumFields, FieldSize);
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
	if (Ar.ArVer >= 453)
	{
	new_ver:
		int ElementSize;
		Ar << ElementSize;
		int SavePos = Ar.Tell();
		Serializer(Ar, &Array);
#if 0
		printf("savePos=%d count=%d elemSize=%d (real=%g) tell=%d\n", SavePos + 4, Array.Num(), ElementSize,
				Array.Num() ? float(Ar.Tell() - SavePos - 4) / Array.Num() : 0,
				Ar.Tell());
#endif
		assert(Ar.Tell() == SavePos + 4 + Array.Num() * ElementSize);	// check position
		return Ar;
	}
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
	if (Ar.ArVer >= PACKAGE_V3) appError("FCompactIndex is missing in UE3");
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


FArchive& operator<<(FArchive &Ar, FString &S)
{
	guard(FString<<);

	if (!Ar.IsLoading)
	{
		Ar << (TArray<char>&)S;
		return Ar;
	}
	// loading
	int len, i;
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) Ar << len;
	else
#endif
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		Ar << AR_INDEX(len);	// Bioshock serialized positive number, but it's string is always unicode
		len = -len;
	}
	else
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) Ar << len;
	else
#endif
		Ar << AR_INDEX(len);
	S.Empty((len >= 0) ? len : -len);
	if (!len)
		return Ar;
	if (len > 0)
	{
		// ANSI string
		S.Add(len);
		Ar.Serialize(S.GetData(), len);
	}
	else
	{
		// UNICODE string
		for (i = 0; i < -len; i++)
		{
			short c;
			Ar << c;
			S.AddItem(c & 255);		//!! incorrect ...
		}
	}
	if (S[abs(len)-1] != 0)
		appError("Serialized FString is not null-terminated");
	return Ar;

	unguard;
}


void FArchive::DetectGame()
{
	// different game platforms autodetection
	//?? should change this, if will implement command line switch to force mode
	//?? code moved here, check code of other structs loaded below for ability to use Ar.IsGameName...

	//?? remove #if ... #endif guards - detect game even when its support is disabled

	staticAssert(GAME_UNKNOWN == 0, GAME_UNKNOWN_value_not_0);	// needed in order "Game += GAME_Name" to work

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
#if UT2
	if ( ((ArVer >= 117 && ArVer <= 120) && (ArLicenseeVer >= 0x19 && ArLicenseeVer <= 0x1C)) ||
		 ((ArVer >= 121 && ArVer <= 128) && ArLicenseeVer == 0x1D) )
		SET(GAME_UT2);
#endif
#if LOCO
	if ((ArVer >= 131 && ArVer <= 134) && ArLicenseeVer == 29)
		SET(GAME_Loco);
#endif
#if PARIAH
	if (ArVer == 119 && ArLicenseeVer == 0x9127)
		SET(GAME_Pariah);
#endif
#if SPLINTER_CELL
	if ( (ArVer == 100 && (ArLicenseeVer >= 0x09 && ArLicenseeVer <= 0x11)) ||
		 (ArVer == 102 && (ArLicenseeVer >= 0x14 && ArLicenseeVer <= 0x1C)) )
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
	if (ArVer == 141 && (ArLicenseeVer == 56 || ArLicenseeVer == 57)) //?? Bioshock and Bioshock 2
		SET(GAME_Bioshock);
#endif
#if UC2
	if (ArVer == 151 && (ArLicenseeVer == 0 || ArLicenseeVer == 1))
		SET(GAME_UC2);
#endif

	/*-----------------------------------------------------------------------
	 * UE3 games
	 *-----------------------------------------------------------------------*/
	// most UE3 games has single version for all packages
	// here is a list of such games, sorted by version
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
#if MKVSDC
	if (ArVer == 402 && ArLicenseeVer == 30)	SET(GAME_MK);		//!! has extra tag
#endif
#if FRONTLINES
	if (ArVer == 433 && ArLicenseeVer == 52)	SET(GAME_Frontlines);
#endif
#if MCARTA
	if (ArVer == 446 && ArLicenseeVer == 25)	SET(GAME_MagnaCarta);
#endif
#if AVA
	if (ArVer == 451 && (ArLicenseeVer >= 52 || ArLicenseeVer <= 53)) SET(GAME_AVA);
#endif
#if MASSEFF
	if (ArVer == 491 && ArLicenseeVer == 0x3F0)	SET(GAME_MassEffect);
	if (ArVer == 512 && ArLicenseeVer == 130)	SET(GAME_MassEffect2);
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
#if BORDERLANDS
	if (ArVer == 584 && (ArLicenseeVer == 57 || ArLicenseeVer == 58)) SET(GAME_Borderlands); // release and update
#endif

	// UE3 games with the various versions of files
#if TUROK
	if ( (ArVer == 374 && ArLicenseeVer == 16) ||
		 (ArVer == 375 && ArLicenseeVer == 19) ||
		 (ArVer == 392 && ArLicenseeVer == 23) ||
		 (ArVer == 393 && (ArLicenseeVer >= 27 && ArLicenseeVer <= 61)) )
		SET(GAME_Turok);
#endif
#if HUXLEY
	if ( (ArVer == 402 && (ArLicenseeVer == 0  || ArLicenseeVer == 10)) ||	//!! has extra tag
		 (ArVer == 491 && (ArLicenseeVer >= 13 && ArLicenseeVer <= 16)) ||
		 (ArVer == 496 && (ArLicenseeVer >= 16 && ArLicenseeVer <= 22)) )
		SET(GAME_Huxley);
#endif
#if ARMYOF2
	if ( (ArVer == 445 && ArLicenseeVer == 79) ||		// Army of Two
		 (ArVer == 482 && ArLicenseeVer == 222) )		// Army of Two: the 40th Day
		SET(GAME_ArmyOf2);
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


#if UNREAL3

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
	else if (Flags == COMPRESS_ZLIB)
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
	else if (Flags == COMPRESS_LZX)
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
	else
		appError("appDecompress: unknown compression flags: %d", Flags);
	return 0;

	unguard;
}


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
		assert(Ar.IsLoading);

		BulkDataFlags = 4;					// unknown
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
		if (BulkDataFlags & BULKDATA_NoData)	// skip serializing
			return;								//?? what to do with BulkData ?
	}

#if MCARTA
	if (Ar.Game == GAME_MagnaCarta && (BulkDataFlags & 0x40))
	{
		// this game has different compression flags
		BulkDataFlags &= ~0x40;
		BulkDataFlags |= BULKDATA_CompressedLzx;
	}
#endif // MCARTA
//	printf("pos: %X bulk %X*%d elements (flags=%X, pos=%X+%X)\n", Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);

	unguard;
}


void FByteBulkData::Serialize(FArchive &Ar)
{
	guard(FByteBulkData::Serialize);

	SerializeHeader(Ar);

	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
//		printf("bulk in separate file (flags=%X, pos=%X+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
		return;
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
	else
	{
		// uncompressed block
		Ar.Serialize(BulkData, DataSize);
	}
	assert(BulkDataOffsetInFile + BulkDataSizeOnDisk == Ar.Tell());

	unguard;
}


#endif // UNREAL3
