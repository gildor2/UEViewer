#ifndef __UNCORE_H__
#define __UNCORE_H__


#if RENDERING
#	include "CoreGL.h"		//?? for materials only
#endif

#undef PLATFORM_UNKNOWN		// defined in windows headers


// forward declarations
template<typename T> class TArray;
class FArchive;
class UObject;
class UnPackage;

// empty guard macros, if not defined
#ifndef guard
#define guard(x)
#endif

#ifndef unguard
#define unguard
#endif

#ifndef unguardf
#define unguardf(...)
#endif


// field offset macros
// get offset of the field in struc
//#ifdef offsetof
#	define FIELD2OFS(struc, field)		(offsetof(struc, field))				// more compatible
//#else
//#	define FIELD2OFS(struc, field)		((unsigned) &((struc *)NULL)->field)	// just in case
//#endif
// get field of type by offset inside struc
#define OFS2FIELD(struc, ofs, type)	(*(type*) ((byte*)(struc) + ofs))


#define INDEX_NONE			-1


#define FVECTOR_ARG(v)		(v).X, (v).Y, (v).Z
#define FQUAT_ARG(v)		(v).X, (v).Y, (v).Z, (v).W
#define FROTATOR_ARG(r)		(r).Yaw, (r).Pitch, (r).Roll
#define FCOLOR_ARG(v)		(v).R, (v).G, (v).B, (v).A


// detect engine version by package
#define PACKAGE_V2			100
#define PACKAGE_V3			180


#define PACKAGE_FILE_TAG		0x9E2A83C1
#define PACKAGE_FILE_TAG_REV	0xC1832A9E

#if PROFILE
extern int GNumSerialize;
extern int GSerializeBytes;

void appResetProfiler();
void appPrintProfiler();

#define PROFILE_POINT(Label)	appPrintProfiler(); appPrintf("PROFILE: " #Label "\n");

#endif

#define MAX_PACKAGE_PATH		512

/*-----------------------------------------------------------------------------
	Game compatibility
-----------------------------------------------------------------------------*/

// BIOSHOCK requires UNREAL3
#if !UNREAL3
#undef BIOSHOCK
#endif

/*-----------------------------------------------------------------------------
	Game directory support
-----------------------------------------------------------------------------*/

void appSetRootDirectory(const char *dir, bool recurse = true);
void appSetRootDirectory2(const char *filename);
const char *appGetRootDirectory();

struct CGameFileInfo
{
	const char*	RelativeName;						// relative to RootDirectory
	const char*	ShortFilename;						// without path, points to filename part of RelativeName
	const char*	Extension;							// points to extension part (excluding '.') of RelativeName
	CGameFileInfo* HashNext;						// used for fast search; computed from ShortFilename excluding extension
	bool		IsPackage;
	bool		PackageScanned;
	int			SizeInKb;							// file size, in kilobytes
	int			ExtraSizeInKb;						// size of additional non-package files
	class FVirtualFileSystem* FileSystem;			// owning virtual file system (NULL for OS file system)
	UnPackage*	Package;
	// content information, valid when PackageScanned is true
	int			NumSkeletalMeshes;
	int			NumStaticMeshes;
	int			NumAnimations;
	int			NumTextures;
};

extern int GNumPackageFiles;
extern int GNumForeignFiles;

// Ext = NULL -> use any package extension
// Filename can contain extension, but should not contain path.
// This function is quite fast because it uses hash tables.
const CGameFileInfo *appFindGameFile(const char *Filename, const char *Ext = NULL);
// This function allows wildcard use in Filename. When wildcard is used, it iterates over all
// found files and could be relatively slow.
void appFindGameFiles(const char *Filename, TArray<const CGameFileInfo*>& Files);

const char *appSkipRootDir(const char *Filename);
FArchive *appCreateFileReader(const CGameFileInfo *info);

typedef bool (*EnumGameFilesCallback_t)(const CGameFileInfo*, void*);
void appEnumGameFilesWorker(EnumGameFilesCallback_t, const char *Ext = NULL, void *Param = NULL);

template<typename T>
FORCEINLINE void appEnumGameFiles(bool (*Callback)(const CGameFileInfo*, T&), const char* Ext, T& Param)
{
	appEnumGameFilesWorker((EnumGameFilesCallback_t)Callback, Ext, &Param);
}

template<typename T>
FORCEINLINE void appEnumGameFiles(bool (*Callback)(const CGameFileInfo*, T&), T& Param)
{
	appEnumGameFilesWorker((EnumGameFilesCallback_t)Callback, NULL, &Param);
}

FORCEINLINE void appEnumGameFiles(bool (*Callback)(const CGameFileInfo*), const char* Ext = NULL)
{
	appEnumGameFilesWorker((EnumGameFilesCallback_t)Callback, Ext, NULL);
}

#if UNREAL3
extern const char *GStartupPackage;
#endif


/*-----------------------------------------------------------------------------
	FName class
-----------------------------------------------------------------------------*/

const char* appStrdupPool(const char* str);

class FName
{
public:
	int			Index;
#if UNREAL3 || UNREAL4
	int			ExtraIndex;
#endif
	const char	*Str;

	FName()
	:	Index(0)
	,	Str("None")
#if UNREAL3 || UNREAL4
	,	ExtraIndex(0)
#endif
	{}

	inline FName& operator=(const FName &Other)
	{
		Index = Other.Index;
#if UNREAL3 || UNREAL4
		ExtraIndex = Other.ExtraIndex;
#endif // UNREAL3
		Str = Other.Str;
		return *this;
	}

	inline FName& operator=(const char* String)
	{
		Str = appStrdupPool(String);
		Index = 0;
#if UNREAL3 || UNREAL4
		ExtraIndex = 0;
#endif
		return *this;
	}

	inline bool operator==(const FName& Other) const
	{
		// we're using appStrdupPool for FName strings, so perhaps comparison of pointers is enough here
		return (Str == Other.Str) || (stricmp(Str, Other.Str) == 0);
	}

	inline bool operator==(const char* String) const
	{
		return (stricmp(Str, String) == 0);
	}

	FORCEINLINE const char *operator*() const
	{
		return Str;
	}
	FORCEINLINE operator const char*() const
	{
		return Str;
	}
};


/*-----------------------------------------------------------------------------
	FCompactIndex class for serializing objects in a compactly, mapping
	small values to fewer bytes.
-----------------------------------------------------------------------------*/

class FCompactIndex
{
public:
	int		Value;
	friend FArchive& operator<<(FArchive &Ar, FCompactIndex &I);
};

#define AR_INDEX(intref)	(*(FCompactIndex*)&(intref))


/*-----------------------------------------------------------------------------
	Some enums used to distinguish game, engine and platform
-----------------------------------------------------------------------------*/

#define GAME_UE4(x)				(GAME_UE4_BASE + (x << 4))
#define GAME_UE4_GET_MINOR(x)	((x - GAME_UE4_BASE) >> 4)	// reverse operation for GAME_UE4(x)

enum EGame
{
	GAME_UNKNOWN   = 0,			// should be 0

	GAME_UE1       = 0x0100000,
		GAME_Undying,

	GAME_UE2       = 0x0200000,
		GAME_UT2,
		GAME_Pariah,
		GAME_SplinterCell,
		GAME_SplinterCellConv,
		GAME_Lineage2,
		GAME_Exteel,
		GAME_Ragnarok2,
		GAME_RepCommando,
		GAME_Loco,
		GAME_BattleTerr,
		GAME_UC1,				// note: not UE2X
		GAME_XIII,
		GAME_Vanguard,
		GAME_AA2,
		GAME_EOS,

	GAME_VENGEANCE = 0x0210000,	// variant of UE2
		GAME_Tribes3,
		GAME_Swat4,				// not autodetected, overlaps with Tribes3
		GAME_Bioshock,

	GAME_LEAD      = 0x0220000,

	GAME_UE2X      = 0x0400000,
		GAME_UC2,

	GAME_UE3       = 0x0800000,
		GAME_EndWar,
		GAME_MassEffect,
		GAME_MassEffect2,
		GAME_MassEffect3,
		GAME_R6Vegas2,
		GAME_MirrorEdge,
		GAME_TLR,
		GAME_Huxley,
		GAME_Turok,
		GAME_Fury,
		GAME_XMen,
		GAME_MagnaCarta,
		GAME_ArmyOf2,
		GAME_CrimeCraft,
		GAME_50Cent,
		GAME_AVA,
		GAME_Frontlines,
		GAME_Batman,
		GAME_Batman2,
		GAME_Batman3,
		GAME_Batman4,
		GAME_Borderlands,
		GAME_AA3,
		GAME_DarkVoid,
		GAME_Legendary,
		GAME_Tera,
		GAME_BladeNSoul,
		GAME_APB,
		GAME_AlphaProtocol,
		GAME_Transformers,
		GAME_MortalOnline,
		GAME_Enslaved,
		GAME_MOHA,
		GAME_MOH2010,
		GAME_Berkanix,
		GAME_DOH,
		GAME_DCUniverse,
		GAME_Bulletstorm,
		GAME_Undertow,
		GAME_Singularity,
		GAME_Tron,
		GAME_Hunted,
		GAME_DND,
		GAME_ShadowsDamned,
		GAME_Argonauts,
		GAME_SpecialForce2,
		GAME_GunLegend,
		GAME_TaoYuan,
		GAME_Tribes4,
		GAME_Dishonored,
		GAME_Hawken,
		GAME_Fable,
		GAME_DmC,
		GAME_PLA,
		GAME_AliensCM,
		GAME_GoWJ,
		GAME_Bioshock3,
		GAME_RememberMe,
		GAME_MarvelHeroes,
		GAME_LostPlanet3,
		GAME_XcomB,
		GAME_Xcom2,
		GAME_Thief4,
		GAME_Murdered,
		GAME_SOV,
		GAME_VEC,
		GAME_Dust514,
		GAME_Guilty,
		GAME_Alice,
		GAME_DunDef,
		GAME_Gigantic,
		GAME_MetroConflict,
		GAME_Smite,
		GAME_DevilsThird,

	GAME_MIDWAY3   = 0x0810000,	// variant of UE3
		GAME_A51,
		GAME_Wheelman,
		GAME_MK,
		GAME_Strangle,
		GAME_TNA,

	GAME_UE4_BASE  = 0x1000000,
		// bytes: 01.00.0N.NX : 01=UE4, 00=masked by GAME_ENGINE, NN=UE4 subversion, X=game (4 bits, 0=base engine)
		// Add custom UE4 game engines here
		// 4.8
		GAME_HIT = GAME_UE4(8)+1,
		// 4.12
		GAME_Gears4 = GAME_UE4(12)+1,
		// 4.13
		GAME_Lawbreakers = GAME_UE4(13)+1,
		// 4.14
		GAME_Friday13 = GAME_UE4(14)+1,
		GAME_Tekken7 = GAME_UE4(14)+2,
		// 4.19
		GAME_Paragon = GAME_UE4(19)+1,

	GAME_ENGINE    = 0xFFF0000	// mask for game engine
};

#define LATEST_SUPPORTED_UE4_VERSION		19		// UE4.XX

enum EPlatform
{
	PLATFORM_UNKNOWN = 0,
	PLATFORM_PC,
	PLATFORM_XBOX360,
	PLATFORM_PS3,
	PLATFORM_IOS,
	PLATFORM_ANDROID,

	PLATFORM_COUNT,
};


/*-----------------------------------------------------------------------------
	FArchive class
-----------------------------------------------------------------------------*/

class FArchive
{
public:
	bool	IsLoading;
	int		ArVer;
	int		ArLicenseeVer;
	bool	ReverseBytes;

protected:
	int		ArPos;
	int		ArStopper;

public:
	// game-specific flags
	int		Game;				// EGame
	int		Platform;			// EPlatform

	FArchive()
	:	ArPos(0)
	,	ArStopper(0)
	,	ArVer(100000)			//?? something large
	,	ArLicenseeVer(0)
	,	ReverseBytes(false)
	,	Game(GAME_UNKNOWN)
	,	Platform(PLATFORM_PC)
	{}

	virtual ~FArchive()
	{}

	void SetupFrom(const FArchive &Other)
	{
		ArVer         = Other.ArVer;
		ArLicenseeVer = Other.ArLicenseeVer;
		ReverseBytes  = Other.ReverseBytes;
		Game          = Other.Game;
		Platform      = Other.Platform;
	}

	// Information aboit game and engine this archive belongs to.

	void DetectGame();
	void OverrideVersion();

	inline int Engine() const
	{
		return (Game & GAME_ENGINE);
	}

	virtual bool IsCompressed() const
	{
		return false;
	}

#if UNREAL4
	virtual bool ContainsEditorData() const
	{
		return false;
	}
#endif

	// Position and file size methods.

	virtual void Seek(int Pos) = 0;

	virtual int Tell() const
	{
		return ArPos;
	}

	virtual int GetFileSize() const
	{
		return 0;
	}

	virtual bool IsEof() const
	{
		return false;
	}

	// 64-bit position support.
	// Note: 64-bit position support is required for FFileArchive classes only,
	// so use 32-bit position everywhere except these classes.

	virtual void Seek64(int64 Pos)
	{
		if (Pos >= (1LL << 31)) appError("%s::Seek64(0x%llX) - not implemented", GetName(), Pos);
		Seek((int)Pos);
	}

	virtual int64 Tell64() const
	{
		return Tell();
	}

	virtual int64 GetFileSize64() const
	{
		return GetFileSize();
	}

	// Serialization functions.

	virtual void Serialize(void *data, int size) = 0;
	void ByteOrderSerialize(void *data, int size);

	// "Stopper" is used to check for overrun serialization.
	// Note: there's no 64-bit "stopper" - large files are used only as containers for smaller
	// files, so stopper validation is performed on upper level, with 32-bit values.

	virtual void SetStopper(int Pos)
	{
		ArStopper = Pos;
	}

	virtual int GetStopper() const
	{
		return ArStopper;
	}

	bool IsStopper()
	{
		int stopper = GetStopper();
		return (stopper != 0) && (Tell() == stopper);
	}

	// Open/close functions, used to save file handles when working with large number of game files.

	virtual bool IsOpen() const
	{
		return true;
	}
	virtual bool Open()
	{
		return true;
	}
	virtual void Close()
	{
	}

	// Dummy implementation of Unreal type serialization

	virtual FArchive& operator<<(FName &/*N*/)
	{
		return *this;
	}
	virtual FArchive& operator<<(UObject *&/*Obj*/)
	{
		return *this;
	}

	// Use FArchive as a text stream

	void Printf(const char *fmt, ...);

	// Typeinfo

	static const char* StaticGetName()
	{
		return "FArchive";
	}

	virtual const char* GetName() const
	{
		return StaticGetName();
	}

	virtual bool IsA(const char* type) const
	{
		return !strcmp(type, "FArchive");
	}

	template<typename T>
	T* CastTo()
	{
		if (IsA(T::StaticGetName()))
			return static_cast<T*>(this);
		else
			return NULL;
	}

	template<typename T>
	const T* CastTo() const
	{
		if (IsA(T::StaticGetName()))
			return static_cast<const T*>(this);
		else
			return NULL;
	}
};

#define DECLARE_ARCHIVE(Class,Base)		\
	typedef Class	ThisClass;			\
	typedef Base	Super;				\
public:									\
	static const char* StaticGetName()	\
	{									\
		return #Class;					\
	}									\
	virtual const char* GetName() const	\
	{									\
		return StaticGetName();			\
	}									\
	virtual bool IsA(const char* type) const \
	{									\
		return !strcmp(type, StaticGetName()) || Super::IsA(type); \
	}									\
private:


// Booleans in UE are serialized as int32
FORCEINLINE FArchive& operator<<(FArchive &Ar, bool &B)
{
	int32 b32 = B;
	Ar.Serialize(&b32, 4);
	if (Ar.IsLoading) B = (b32 != 0);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, char &B) // int8
{
	Ar.Serialize(&B, 1);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, byte &B) // uint8
{
	Ar.Serialize(&B, 1);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, int16 &B)
{
	Ar.ByteOrderSerialize(&B, 2);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, uint16 &B)
{
	Ar.ByteOrderSerialize(&B, 2);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, int32 &B)
{
	Ar.ByteOrderSerialize(&B, 4);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, uint32 &B)
{
	Ar.ByteOrderSerialize(&B, 4);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, int64 &B)
{
	Ar.ByteOrderSerialize(&B, 8);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, uint64 &B)
{
	Ar.ByteOrderSerialize(&B, 8);
	return Ar;
}
FORCEINLINE FArchive& operator<<(FArchive &Ar, float &B)
{
	Ar.ByteOrderSerialize(&B, 4);
	return Ar;
}


enum EFileReaderOptions
{
	FRO_NoOpenError = 1,
};


class FFileArchive : public FArchive
{
	DECLARE_ARCHIVE(FFileArchive, FArchive);
public:
	FFileArchive(const char *Filename, unsigned InOptions);
	virtual ~FFileArchive();

	virtual void Seek(int Pos);
	virtual void Seek64(int64 Pos);
	virtual int Tell() const;
	virtual int64 Tell64() const;
	virtual int GetFileSize() const;
//	virtual int64 GetFileSize64() const; -- implemented in derived classes

	virtual bool IsEof() const;
	virtual bool IsOpen() const;
	virtual void Close();

protected:
	FILE		*f;
	unsigned	Options;
	const char	*FullName;		// allocated with appStrdup
	const char	*ShortName;		// points to FullName[N]
	int64		FileSize;

	byte*		Buffer;
	int			BufferSize;
	int64		BufferPos;		// position of Buffer in file
	int64		ArPos64;
	int64		FilePos;		// where 'f' position points to (when reading, it usually equals to 'BufferPos + BufferSize')

	bool OpenFile(const char *Mode);
};


inline bool appFileExists(const char* filename)
{
	FILE* f = fopen(filename, "rb");
	if (f)
	{
		fclose(f);
		return true;
	}
	return false;
}


class FFileReader : public FFileArchive
{
	DECLARE_ARCHIVE(FFileReader, FFileArchive);
public:
	FFileReader(const char *Filename, unsigned InOptions = 0);
	virtual ~FFileReader();

	virtual void Serialize(void *data, int size);
	virtual bool Open();
	virtual int64 GetFileSize64() const;
};


class FFileWriter : public FFileArchive
{
	DECLARE_ARCHIVE(FFileWriter, FFileArchive);
public:
	FFileWriter(const char *Filename, unsigned Options = 0);
	virtual ~FFileWriter();

	virtual void Serialize(void *data, int size);
	virtual bool Open();
	virtual void Close();
	virtual int64 GetFileSize64() const;

	static void CleanupOnError();

protected:
	void FlushBuffer();
};


// NOTE: this class should work well as a writer too!
class FReaderWrapper : public FArchive
{
	DECLARE_ARCHIVE(FReaderWrapper, FArchive);
public:
	FArchive	*Reader;
	int			ArPosOffset;

	FReaderWrapper(FArchive *File, int Offset = 0)
	:	Reader(File)
	,	ArPosOffset(Offset)
	{}
	virtual ~FReaderWrapper()
	{
		delete Reader;
	}
	virtual bool IsCompressed() const
	{
		return Reader->IsCompressed();
	}
	virtual void Seek(int Pos)
	{
		Reader->Seek(Pos + ArPosOffset);
	}
	virtual int Tell() const
	{
		return Reader->Tell() - ArPosOffset;
	}
	virtual int GetFileSize() const
	{
		return Reader->GetFileSize() - ArPosOffset;
	}
	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);
	}
	virtual void SetStopper(int Pos)
	{
		Reader->SetStopper(Pos + ArPosOffset);
	}
	virtual int GetStopper() const
	{
		return Reader->GetStopper() - ArPosOffset;
	}
	virtual bool IsOpen() const
	{
		return Reader->IsOpen();
	}
	virtual bool Open()
	{
		return Reader->Open();
	}
	virtual void Close()
	{
		Reader->Close();
	}
};


class FMemReader : public FArchive
{
	DECLARE_ARCHIVE(FMemReader, FArchive);
public:
	FMemReader(const void *data, int size)
	:	DataPtr((const byte*)data)
	,	DataSize(size)
	{
		IsLoading = true;
		ArStopper = size;
	}

	virtual void Seek(int Pos)
	{
		guard(FMemReader::Seek);
		assert(Pos >= 0 && Pos <= DataSize);
		ArPos = Pos;
		unguard;
	}

	virtual bool IsEof() const
	{
		return ArPos >= DataSize;
	}

	virtual void Serialize(void *data, int size)
	{
		guard(FMemReader::Serialize);
		if (ArStopper > 0 && ArPos + size > ArStopper)
			appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);
		if (ArPos + size > DataSize)
			appError("Serializing behind end of buffer");
		memcpy(data, DataPtr + ArPos, size);
		ArPos += size;
		unguard;
	}

	virtual int GetFileSize() const
	{
		return DataSize;
	}

protected:
	const byte *DataPtr;
	int		DataSize;
};


// drop remaining object data (until stopper)
#define DROP_REMAINING_DATA(Ar)							\
	Ar.Seek(Ar.GetStopper());

// research helper
inline void DUMP_ARC_BYTES(FArchive &Ar, int NumBytes, const char* Label = NULL)
{
	if (Label) appPrintf("%s:", Label);
	int64 OldPos = Ar.Tell64();
	for (int i = 0; i < NumBytes; i++)
	{
		if (Ar.IsStopper() || Ar.IsEof()) break;
		if (!(i & 31)) appPrintf("\n%06X :", OldPos+i);
		if (!(i & 3)) appPrintf(" ");
		byte b;
		Ar << b;
		appPrintf(" %02X", b);
	}
	appPrintf("\n");
	Ar.Seek64(OldPos);
}

inline void DUMP_MEM_BYTES(const void* Data, int NumBytes)
{
	const byte* b = (byte*)Data;
	for (int i = 0; i < NumBytes; i++)
	{
		if (!(i & 31)) appPrintf("\n%08X :", b);
		if (!(i & 3)) appPrintf(" ");
		appPrintf(" %02X", *b++);
	}
	appPrintf("\n");
}


// Reverse byte order for data array, inplace
void appReverseBytes(void *Block, int NumItems, int ItemSize);


/*-----------------------------------------------------------------------------
	Math classes
-----------------------------------------------------------------------------*/

struct FVector
{
	float	X, Y, Z;

	void Set(float _X, float _Y, float _Z)
	{
		X = _X; Y = _Y; Z = _Z;
	}

	void Scale(float value)
	{
		X *= value; Y *= value; Z *= value;
	}

	friend FArchive& operator<<(FArchive &Ar, FVector &V)
	{
		Ar << V.X << V.Y << V.Z;
#if ENDWAR
		if (Ar.Game == GAME_EndWar) Ar.Seek(Ar.Tell() + 4);	// skip W, at ArVer >= 290
#endif
		return Ar;
	}
};

struct FVector4
{
	float	X, Y, Z, W;

	friend FArchive& operator<<(FArchive &Ar, FVector4 &V)
	{
		return Ar << V.X << V.Y << V.Z << V.W;
	}
};

#if 1

FORCEINLINE bool operator==(const FVector &V1, const FVector &V2)
{
	return V1.X == V2.X && V1.Y == V2.Y && V1.Z == V2.Z;
}

#else

FORCEINLINE bool operator==(const FVector &V1, const FVector &V2)
{
	const int* p1 = (int*)&V1;
	const int* p2 = (int*)&V2;
	return ( (p1[0] ^ p2[0]) | (p1[1] ^ p2[1]) | (p1[2] ^ p2[2]) ) == 0;
}

#endif


struct FRotator
{
	int		Pitch, Yaw, Roll;

	void Set(int _Yaw, int _Pitch, int _Roll)
	{
		Pitch = _Pitch; Yaw = _Yaw; Roll = _Roll;
	}

	friend FArchive& operator<<(FArchive &Ar, FRotator &R)
	{
		Ar << R.Pitch << R.Yaw << R.Roll;
#if TNA_IMPACT
		if (Ar.Game == GAME_TNA && Ar.ArVer >= 395)
		{
			// WWE All Stars: this game has strange FRotator values; found formulas below experimentally
			R.Pitch = R.Pitch / 65536;
			R.Yaw   = R.Yaw   / 65536 - 692;
			R.Roll  = R.Roll  / 65536 - 692;
		}
#endif // TNA_IMPACT
		return Ar;
	}
};


struct FQuat
{
	float	X, Y, Z, W;

	void Set(float _X, float _Y, float _Z, float _W)
	{
		X = _X; Y = _Y; Z = _Z; W = _W;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuat &F)
	{
		return Ar << F.X << F.Y << F.Z << F.W;
	}
};


struct FCoords
{
	FVector	Origin;
	FVector	XAxis;
	FVector	YAxis;
	FVector	ZAxis;

	friend FArchive& operator<<(FArchive &Ar, FCoords &F)
	{
		return Ar << F.Origin << F.XAxis << F.YAxis << F.ZAxis;
	}
};


struct FBox
{
	FVector	Min;
	FVector	Max;
	byte	IsValid;

	friend FArchive& operator<<(FArchive &Ar, FBox &Box)
	{
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 146)
			return Ar << Box.Min << Box.Max;
#endif // UC2
		return Ar << Box.Min << Box.Max << Box.IsValid;
	}
};


struct FSphere : public FVector
{
	float	R;

	friend FArchive& operator<<(FArchive &Ar, FSphere &S)
	{
		Ar << (FVector&)S;
		if (Ar.ArVer >= 61)
			Ar << S.R;
		return Ar;
	};
};


struct FPlane : public FVector
{
	float	W;

	friend FArchive& operator<<(FArchive &Ar, FPlane &S)
	{
		return Ar << (FVector&)S << S.W;
	};
};


struct FMatrix
{
	FPlane	XPlane;
	FPlane	YPlane;
	FPlane	ZPlane;
	FPlane	WPlane;

	friend FArchive& operator<<(FArchive &Ar, FMatrix &F)
	{
		return Ar << F.XPlane << F.YPlane << F.ZPlane << F.WPlane;
	}
};


class FScale
{
public:
	FVector	Scale;
	float	SheerRate;
	byte	SheerAxis;	// ESheerAxis

	// Serializer.
	friend FArchive& operator<<(FArchive &Ar, FScale &S)
	{
		return Ar << S.Scale << S.SheerRate << S.SheerAxis;
	}
};


struct FColor
{
	byte	R, G, B, A;

	FColor()
	{}
	FColor(byte r, byte g, byte b, byte a = 255)
	:	R(r), G(g), B(b), A(a)
	{}
	friend FArchive& operator<<(FArchive &Ar, FColor &C)
	{
		return Ar << C.R << C.G << C.B << C.A;
	}
};


// UNREAL3
struct FLinearColor
{
	float	R, G, B, A;

	void Set(float _R, float _G, float _B, float _A)
	{
		R = _R; G = _G; B = _B; A = _A;
	}

	friend FArchive& operator<<(FArchive &Ar, FLinearColor &C)
	{
		return Ar << C.R << C.G << C.B << C.A;
	}
};

struct FBoxSphereBounds
{
	FVector	Origin;
	FVector	BoxExtent;
	float	SphereRadius;

	friend FArchive& operator<<(FArchive &Ar, FBoxSphereBounds &B)
	{
		return Ar << B.Origin << B.BoxExtent << B.SphereRadius;
	}
};


struct FPackedNormal
{
	uint32	Data;

	friend FArchive& operator<<(FArchive &Ar, FPackedNormal &N)
	{
		return Ar << N.Data;
	}

	operator FVector() const
	{
		// "x / 127.5 - 1" comes from Common.usf, TangentBias() macro
		FVector r;
		r.X = ( Data        & 0xFF) / 127.5f - 1;
		r.Y = ((Data >> 8 ) & 0xFF) / 127.5f - 1;
		r.Z = ((Data >> 16) & 0xFF) / 127.5f - 1;
		return r;
	}

	FPackedNormal &operator=(const FVector &V)
	{
		Data = int((V.X + 1) * 127.5f)
			+ (int((V.Y + 1) * 127.5f) << 8)
			+ (int((V.Z + 1) * 127.5f) << 16);
		return *this;
	}

	float GetW() const
	{
		return (Data >> 24) / 127.5f - 1;
	}
};


#if UNREAL4

// Packed normal replacement, used since UE4.12 for high-precision reflections
struct FPackedRGBA16N
{
	uint16	X, Y, Z, W;

	FPackedNormal ToPackedNormal() const
	{
		FPackedNormal r;
		FVector v = *this;		// conversion
		r = v;					// conversion
		return r;
	}

	operator FVector() const
	{
		FVector r;
		r.X = (X - 32767.5f) / 32767.5f;
		r.Y = (Y - 32767.5f) / 32767.5f;
		r.Z = (Z - 32767.5f) / 32767.5f;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FPackedRGBA16N &V)
	{
		return Ar << V.X << V.Y << V.Z << V.W;
	}
};

struct FIntPoint
{
	int		X, Y;

	friend FArchive& operator<<(FArchive &Ar, FIntPoint &V)
	{
		return Ar << V.X << V.Y;
	}
};

struct FIntVector
{
	int		X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FIntVector &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}
};

struct FVector2D
{
	float	X, Y;

	friend FArchive& operator<<(FArchive &Ar, FVector2D &V)
	{
		return Ar << V.X << V.Y;
	}
};

struct FTransform
{
	FQuat	Rotation;
	FVector	Translation;
	FVector	Scale3D;

	friend FArchive& operator<<(FArchive &Ar, FTransform &T)
	{
		return Ar << T.Rotation << T.Translation << T.Scale3D;
	}
};

#endif // UNREAL4


/*-----------------------------------------------------------------------------
	Typeinfo for fast array serialization
-----------------------------------------------------------------------------*/

// Default typeinfo
template<typename T>
struct TTypeInfo
{
	enum { FieldSize = sizeof(T) };
	enum { NumFields = 1         };
	enum { IsSimpleType = 0      };		// type consists of NumFields fields of integral type, sizeof(type) == FieldSize
	enum { IsRawType = 0         };		// type's on-disk layout exactly matches in-memory layout
	enum { IsPod = IS_POD(T)     };		// type has no constructor/destructor
};


template<typename T1, typename T2>
struct TAreTypesEqual
{
	enum { Value = 0 };
};

template<typename T>
struct TAreTypesEqual<T,T>
{
	enum { Value = 1 };
};


// Declare type, consists from fields of the same type length
// (e.g. from ints and floats, or from chars and bytes etc), and
// which memory layout is the same as disk layout (if endian of
// package and platform is the same)
#define SIMPLE_TYPE(Type,BaseType)			\
template<> struct TTypeInfo<Type>			\
{											\
	enum { FieldSize = sizeof(BaseType) };	\
	enum { NumFields = sizeof(Type) / sizeof(BaseType) }; \
	enum { IsSimpleType = 1 };				\
	enum { IsRawType = 1 };					\
	enum { IsPod = 1 };						\
};


// Declare type, which memory layout is the same as disk layout
#define RAW_TYPE(Type)						\
template<> struct TTypeInfo<Type>			\
{											\
	enum { FieldSize = sizeof(Type) };		\
	enum { NumFields = 1 };					\
	enum { IsSimpleType = 0 };				\
	enum { IsRawType = 1 };					\
	enum { IsPod = 1 };						\
};


// Note: SIMPLE and RAW types does not need constructors for loading
//!! add special mode to check SIMPLE/RAW types (serialize twice and compare results)

#if 0
//!! testing
#undef  SIMPLE_TYPE
#undef  RAW_TYPE
#define SIMPLE_TYPE(x,y)
#define RAW_TYPE(x)
#endif


// Declare fundamental types
SIMPLE_TYPE(bool,     bool)
SIMPLE_TYPE(byte,     byte)
SIMPLE_TYPE(char,     char)
SIMPLE_TYPE(int16,    int16)
SIMPLE_TYPE(uint16,   uint16)
SIMPLE_TYPE(int32,    int32)
SIMPLE_TYPE(uint32,   uint32)
SIMPLE_TYPE(float,    float)
SIMPLE_TYPE(int64,    int64)
SIMPLE_TYPE(uint64,   uint64)

// Aggregates
SIMPLE_TYPE(FVector,  float)
SIMPLE_TYPE(FVector4, float)
SIMPLE_TYPE(FQuat,    float)
SIMPLE_TYPE(FCoords,  float)
SIMPLE_TYPE(FColor,   byte)
SIMPLE_TYPE(FPackedNormal, uint32)

#if UNREAL4

SIMPLE_TYPE(FPackedRGBA16N, uint16)
SIMPLE_TYPE(FIntPoint,  int)
SIMPLE_TYPE(FIntVector, int)
SIMPLE_TYPE(FVector2D,  float)
SIMPLE_TYPE(FTransform, float)

#endif // UNREAL4


/*-----------------------------------------------------------------------------
	TArray class
-----------------------------------------------------------------------------*/

/*
 * NOTES:
 *	- FArray/TArray should not contain objects with virtual tables (no
 *	  constructor/destructor support)
 *	- should not use new[] and delete[] here, because compiler will allocate
 *	  additional 'count' field to support correct delete[], but we use
 *	  appMalloc/appFree calls to allocate/release memory.
 */

class FArray
{
	friend struct CTypeInfo;
	friend class FString;
	template<int N> friend class FStaticString;

public:
	FORCEINLINE FArray()
	:	DataCount(0)
	,	MaxCount(0)
	,	DataPtr(NULL)
	{}
	~FArray();

	FORCEINLINE void *GetData()
	{
		return DataPtr;
	}
	FORCEINLINE const void *GetData() const
	{
		return DataPtr;
	}
	FORCEINLINE int Num() const
	{
		return DataCount;
	}
	FORCEINLINE int Max() const
	{
		return MaxCount;
	}
	FORCEINLINE bool IsValidIndex(int index) const
	{
		return index >= 0 && index < DataCount;
	}

	void RawCopy(const FArray &Src, int elementSize);

	// serializers
	FArchive& SerializeSimple(FArchive &Ar, int NumFields, int FieldSize);
	FArchive& SerializeRaw(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize);

protected:
	void	*DataPtr;
	int		DataCount;
	int		MaxCount;

	// helper for TStaticArray and FStaticString - these classes has data allocated
	// immediately after FArray structure
	FORCEINLINE bool IsStatic() const
	{
		return DataPtr == (void*)(this + 1);
	}

	// serializers
	FArchive& Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize);

	// clear array and resize to specific count
	void Empty(int count, int elementSize);
	// reserve space for 'count' items
	void GrowArray(int count, int elementSize);
	// insert 'count' items of size 'elementSize' at position 'index', memory will be zeroed
	void InsertZeroed(int index, int count, int elementSize);
	// insert 'count' items of size 'elementSize' at position 'index', memory will be uninitialized
	void InsertUninitialized(int index, int count, int elementSize);
	// remove items and then move next items to the position of removed items
	void Remove(int index, int count, int elementSize);
	// remove items and then fill the hole with items from array's end
	void RemoveAtSwap(int index, int count, int elementSize);

	void* GetItem(int index, int elementSize) const;
};

#if DECLARE_VIEWER_PROPS
#define ARRAY_COUNT_FIELD_OFFSET	( sizeof(void*) )	// offset of DataCount field inside TArray structure
#endif


FArchive& SerializeLazyArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*));
#if UNREAL3
FArchive& SerializeBulkArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*));
#endif


// NOTE: this container cannot hold objects, required constructor/destructor
// (at least, Add/Insert/Remove functions are not supported, but can serialize
// such data)
template<typename T>
class TArray : public FArray
{
public:
	TArray()
	:	FArray()
	{}
	~TArray()
	{
		// destruct all array items
		if (!TTypeInfo<T>::IsPod) Destruct(0, DataCount);
	}
	// data accessors

	FORCEINLINE T* GetData()
	{
		return (T*)DataPtr;
	}
	FORCEINLINE const T* GetData() const
	{
		return (const T*)DataPtr;
	}
#if !DO_ASSERT
	// version without verifications, very compact
	FORCEINLINE T& operator[](int index)
	{
		return *((T*)DataPtr + index);
	}
	FORCEINLINE const T& operator[](int index) const
	{
		return *((T*)DataPtr + index);
	}
#elif DO_GUARD_MAX
	// version with __FUNCSIG__
	T& operator[](int index)
	{
		if (!IsValidIndex(index)) appError("%s: index %d is out of range (%d)", __FUNCSIG__, index, DataCount);
		return *((T*)DataPtr + index);
	}
	const T& operator[](int index) const
	{
		if (!IsValidIndex(index)) appError("%s: index %d is out of range (%d)", __FUNCSIG__, index, DataCount);
		return *((T*)DataPtr + index);
	}
#else // DO_ASSERT && !DO_GUARD_MAX
	// common implementation for all types
	FORCEINLINE T& operator[](int index)
	{
		return *(T*)GetItem(index, sizeof(T));
	}
	FORCEINLINE const T& operator[](int index) const
	{
		return *(T*)GetItem(index, sizeof(T));
	}
#endif // DO_ASSERT && !DO_GUARD_MAX

	FORCEINLINE T& Last(int IndexFromEnd = 0)
	{
		return *(T*)GetItem(DataCount - IndexFromEnd - 1, sizeof(T));
	}

	FORCEINLINE const T& Last(int IndexFromEnd = 0) const
	{
		return *(T*)GetItem(DataCount - IndexFromEnd - 1, sizeof(T));
	}

	//!! Possible additions from UE4:
	//!! Emplace(...)       = new(...)
	//!! SetNum/SetNumUninitialized/SetNumZeroed

	FORCEINLINE void Init(const T& value, int count)
	{
		Empty(count);
		DataCount = count;
		for (int i = 0; i < count; i++)
			*((T*)DataPtr + i) = value;
	}

	FORCEINLINE int Add(const T& item)
	{
		int index = DataCount;
		FArray::InsertUninitialized(index, 1, sizeof(T));
		new ((T*)DataPtr + index) T(item);
		return index;
	}
	FORCEINLINE int AddZeroed(int count = 1)
	{
		int index = DataCount;
		FArray::InsertZeroed(index, count, sizeof(T));
		return index;
	}
	FORCEINLINE int AddDefaulted(int count = 1)
	{
		int index = DataCount;
		if (!TTypeInfo<T>::IsPod)
		{
			FArray::InsertUninitialized(index, count, sizeof(T));
			Construct(index, count);
		}
		else
		{
			FArray::InsertZeroed(index, count, sizeof(T));
		}
		return index;
	}
	FORCEINLINE int AddUninitialized(int count = 1)
	{
		int index = DataCount;
		FArray::InsertUninitialized(index, count, sizeof(T));
		return index;
	}
	FORCEINLINE int AddUnique(const T& item)
	{
		int index = FindItem(item);
		if (index >= 0) return index;
		return Add(item);
	}

	FORCEINLINE void Insert(const T& item, int index)
	{
		FArray::InsertUninitialized(index, 1, sizeof(T));
		new ((T*)DataPtr + index) T(item);
	}
	FORCEINLINE void InsertZeroed(int index, int count = 1)
	{
		FArray::InsertZeroed(index, count, sizeof(T));
	}
	FORCEINLINE void InsertDefaulted(int index, int count = 1)
	{
		if (!TTypeInfo<T>::IsPod)
		{
			FArray::InsertUninitialized(index, count, sizeof(T));
			Construct(index, count);
		}
		else
		{
			FArray::InsertZeroed(index, count, sizeof(T));
		}
	}
	FORCEINLINE void InsertUninitialized(int index, int count = 1)
	{
		FArray::InsertUninitialized(index, count, sizeof(T));
	}

	FORCEINLINE void RemoveAt(int index, int count = 1)
	{
		// destruct specified array items
		if (!TTypeInfo<T>::IsPod) Destruct(index, count);
		// remove items from array
		FArray::Remove(index, count, sizeof(T));
	}

	// Remove an item and copy last array's item(s) to the removed item position,
	// so no array shifting performed. Could be used when order of array elements
	// is not important.
	FORCEINLINE void RemoveAtSwap(int index, int count = 1)
	{
		// destruct specified array items
		if (!TTypeInfo<T>::IsPod) Destruct(index, count);
		// remove items from array
		FArray::RemoveAtSwap(index, count, sizeof(T));
	}

	FORCEINLINE void RemoveSingle(const T& item)
	{
		int index = FindItem(item);
		if (index >= 0)
			RemoveAt(index);
	}

	int FindItem(const T& item, int startIndex = 0) const
	{
#if 1
		const T *P;
		int i;
		for (i = startIndex, P = (const T*)DataPtr + startIndex; i < DataCount; i++, P++)
			if (*P == item)
				return i;
#else
		for (int i = startIndex; i < DataCount; i++)
			if (*((T*)DataPtr + i) == item)
				return i;
#endif
		return INDEX_NONE;
	}

	FORCEINLINE void Empty(int count = 0)
	{
		// destruct all array items
		if (!TTypeInfo<T>::IsPod) Destruct(0, DataCount);
		// remove data array (count=0) or preallocate memory (count>0)
		FArray::Empty(count, sizeof(T));
	}

	FORCEINLINE void ResizeTo(int count)
	{
		if (count > DataCount)
		{
			// grow array
			FArray::GrowArray(count - DataCount, sizeof(T));
		}
		else if (count < DataCount)
		{
			// shrink array
			RemoveAt(count, DataCount - count);
		}
	}

	// set new DataCount without reallocation if possible
	FORCEINLINE void Reset(int count = 0)
	{
		if (MaxCount < count)
		{
			Empty(count);
		}
		else
		{
			if (!TTypeInfo<T>::IsPod) Destruct(0, DataCount);
			DataCount = 0;
		}
	}

	FORCEINLINE void Sort(int (*cmpFunc)(const T*, const T*))
	{
		QSort<T>((T*)DataPtr, DataCount, cmpFunc);
	}

	// serializer
	friend FORCEINLINE FArchive& operator<<(FArchive &Ar, TArray &A)
	{
#if DO_GUARD_MAX
		guardfunc;
#endif
		// special case for SIMPLE_TYPE
		if (TTypeInfo<T>::IsSimpleType)
		{
			staticAssert(sizeof(T) == TTypeInfo<T>::NumFields * TTypeInfo<T>::FieldSize, Error_In_TypeInfo);
			return A.SerializeSimple(Ar, TTypeInfo<T>::NumFields, TTypeInfo<T>::FieldSize);
		}

		// special case for RAW_TYPE
		if (TTypeInfo<T>::IsRawType)
			return A.SerializeRaw(Ar, TArray<T>::SerializeItem, sizeof(T));

		// generic case
		// erase previous data before loading in a case of non-POD data
		if (!TTypeInfo<T>::IsPod && Ar.IsLoading)
			A.Destruct(0, A.Num());		// do not call Empty() - data will be freed anyway in FArray::Serialize()
		return A.Serialize(Ar, TArray<T>::SerializeItem, sizeof(T));
#if DO_GUARD_MAX
		unguard;
#endif
	}

#if UNREAL3
	// Serialize an array, which file contents exactly matches in-memory contents.
	// Whole array can be read using a single read call. Package engine version should
	// equals to game engine version, and endianness should match, otherwise per-element
	// reading will be performed (as usual in TArray). Implemented in UE3 and UE4.
	// Note: there is no reading optimization performed here (in umodel).
	FORCEINLINE void BulkSerialize(FArchive& Ar)
	{
	#if DO_GUARD_MAX
		guardfunc;
	#endif
		SerializeBulkArray(Ar, *this, SerializeArray);
	#if DO_GUARD_MAX
		unguard;
	#endif
	}
#endif // UNREAL3

	// serializer helper; used from 'operator<<(FArchive, TArray<>)' only
	static void SerializeItem(FArchive &Ar, void *item)
	{
		if (!TTypeInfo<T>::IsPod && Ar.IsLoading)
			new (item) T;		// construct item before reading
		Ar << *(T*)item;		// serialize item
	}

	// serializer which allows passing custom function: function should have
	// prototype 'void Func(FArchive& Ar, T& Obj)'
	typedef void (*SerializerFunc_t)(FArchive& Ar, T& Obj);

	template<SerializerFunc_t F>
	FArchive& Serialize2(FArchive& Ar)
	{
		if (!TTypeInfo<T>::IsPod && Ar.IsLoading)
			Destruct(0, Num());
		return FArray::Serialize(Ar, TArray<T>::SerializeItem2<F>, sizeof(T));
	}

	template<SerializerFunc_t F>
	static void SerializeItem2(FArchive& Ar, void* item)
	{
		if (!TTypeInfo<T>::IsPod && Ar.IsLoading)
			new (item) T;		// construct item before reading
		F(Ar, *(T*) item);
	}

protected:
	// disable array copying
	TArray(const TArray &Other)
	:	FArray()
	{}
	TArray& operator=(const TArray &Other)
	{
		return *this;
	}
	// Helper function to reduce TLazyArray etc operator<<'s code size.
	// Used as C-style wrapper around TArray<>::operator<<().
	static FArchive& SerializeArray(FArchive &Ar, void *Array)
	{
		return Ar << *(TArray<T>*)Array;
	}
	// fast version of operator[] without assertions (may be used in safe code)
	FORCEINLINE T& Item(int index)
	{
		return *((T*)DataPtr + index);
	}
	FORCEINLINE const T& Item(int index) const
	{
		return *((T*)DataPtr + index);
	}
	void Construct(int index, int count)
	{
		for (int i = 0; i < count; i++)
			new ((T*)DataPtr + index + i) T;
	}
	void Destruct(int index, int count)
	{
		for (int i = 0; i < count; i++)
			((T*)DataPtr + index + i)->~T();
	}
};

template<typename T>
inline void Exchange(TArray<T>& A, TArray<T>& B)
{
	const int size = sizeof(TArray<T>);
	byte buffer[size];
	memcpy(buffer, &A, size);
	memcpy(&A, &B, size);
	memcpy(&B, buffer, size);
}

// Binary-compatible array, but with inline allocation. FArray has helper function
// IsStatic() for this class. The array size is not limited to 'N' - if more items
// will be required, memory will be allocated.
template<typename T, int N>
class TStaticArray : public TArray<T>
{
	// We require "using TArray<T>::*" for gcc 3.4+ compilation
	// http://gcc.gnu.org/gcc-3.4/changes.html
	// - look for "unqualified names"
	// - "temp.dep/3" section of the C++ standard [ISO/IEC 14882:2003]
	using TArray<T>::DataPtr;
	using TArray<T>::MaxCount;
public:
	FORCEINLINE TStaticArray()
	{
		DataPtr = (void*)&StaticData[0];
		MaxCount = N;
	}

protected:
	T		StaticData[N];
};

#ifndef UMODEL_LIB_IN_NAMESPACE
// Do not compile operator new when building UnCore.h inside a namespace.
// More info: https://github.com/gildor2/UModel/pull/15/commits/3dc3096a6e81845a75024e060715b76bf345cd1b

template<typename T>
FORCEINLINE void* operator new(size_t size, TArray<T> &Array)
{
	guard(TArray::operator new);
	assert(size == sizeof(T)); // allocating wrong object? can't disallow allocating of "int" inside "TArray<FString>" at compile time ...
	int index = Array.AddUninitialized(1);
	return Array.GetData() + index;
	unguard;
}

#endif // UMODEL_LIB_IN_NAMESPACE


// Skip array of items of fixed size
void SkipFixedArray(FArchive &Ar, int ItemSize);


// TLazyArray implemented as simple wrapper around TArray with
// different serialization function
// Purpose in UE: array with can me loaded asynchronously (when serializing
// it 1st time only disk position is remembered, and later array can be
// read from file when needed)

template<typename T>
class TLazyArray : public TArray<T>
{
#if DO_GUARD_MAX
	friend FArchive& operator<<(FArchive &Ar, TLazyArray &A)
	{
		guardfunc;
		return SerializeLazyArray(Ar, A, TArray<T>::SerializeArray);
		unguard;
	}
#else
	friend FORCEINLINE FArchive& operator<<(FArchive &Ar, TLazyArray &A)
	{
		return SerializeLazyArray(Ar, A, TArray<T>::SerializeArray);
	}
#endif
};


void SkipLazyArray(FArchive &Ar);
#if UNREAL3
void SkipBulkArrayData(FArchive &Ar, int Size = -1);
#endif // UNREAL3

template<typename T1, typename T2>
inline void CopyArray(TArray<T1> &Dst, const TArray<T2> &Src)
{
	if (TAreTypesEqual<T1,T2>::Value && TTypeInfo<T1>::IsPod)
	{
		// fast version when copying POD type array
		Dst.RawCopy(Src, sizeof(T1));
		return;
	}

	// copying 2 different types, or non-POD type
	int Count = Src.Num();
	Dst.Empty(Count);
	if (Count)
	{
		Dst.AddUninitialized(Count);
		T1 *pDst = (T1*)Dst.GetData();
		T2 *pSrc = (T2*)Src.GetData();
		do		// Count is > 0 here - checked above, so "do ... while" is more suitable (and more compact)
		{
			*pDst++ = *pSrc++;
		} while (--Count);
	}
}


/*-----------------------------------------------------------------------------
	TMap template
-----------------------------------------------------------------------------*/

// Very simple class, required only for serialization
template<typename TK, typename TV>
struct TMapPair
{
	TK		Key;
	TV		Value;

	friend FArchive& operator<<(FArchive &Ar, TMapPair &V)
	{
		return Ar << V.Key << V.Value;
	}
};


template<typename TK, typename TV>
class TMap : public TArray<TMapPair<TK, TV> >
{
public:
	friend FORCEINLINE FArchive& operator<<(FArchive &Ar, TMap &Map)
	{
		return Ar << (TArray<TMapPair<TK, TV> >&)Map;
	}
};

template<typename TK, typename TV, int N>
class TStaticMap : public TStaticArray<TMapPair<TK, TV>, N>
{
public:
	friend FORCEINLINE FArchive& operator<<(FArchive &Ar, TStaticMap &Map)
	{
		return Ar << (TStaticArray<TMapPair<TK, TV>, N>&)Map;
	}
};


/*-----------------------------------------------------------------------------
	TArray of T[N] template
-----------------------------------------------------------------------------*/

template<typename T, int N>
struct TArrayOfArrayItem
{
	T	Data[N];

	friend FArchive& operator<<(FArchive &Ar, TArrayOfArrayItem &S)
	{
		for (int i = 0; i < N; i++)
			Ar << S.Data[i];
		return Ar;
	}
};

template<typename T, int N>
class TArrayOfArray : public TArray<TArrayOfArrayItem<T, N> >
{
};

/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

class FString
{
public:
	FString()
	{}
	FString(const char* src);
	FString(const FString& Other);

	FString& operator=(const char* src);
	FString& operator=(const FString& src);

	FString& operator+=(const char* text);

	//?? TODO: operate with arrays here
	FORCEINLINE FString& operator+=(const FString& Str)
	{
		return operator+=(*Str);
	}

	// use FString as allocated char*, FString became empty and will not free
	// detached string in destructor
	char* Detach();

	FORCEINLINE int Len() const
	{
		return Data.Num() <= 1 ? 0 : Data.Num() - 1;
	}

	FORCEINLINE TArray<char>& GetDataArray()
	{
		return Data;
	}

	FORCEINLINE const TArray<char>& GetDataArray() const
	{
		return Data;
	}

	FORCEINLINE void Empty(int count = 0)
	{
		Data.Empty(count);
	}

	FORCEINLINE bool IsEmpty() const
	{
		return Data.Num() <= 1;
	}

	bool StartsWith(const char* Text);
	bool EndsWith(const char* Text);
	bool RemoveFromStart(const char* Text);
	bool RemoveFromEnd(const char* Text);

	FString& AppendChar(char ch);

	FORCEINLINE void RemoveAt(int index, int count = 1)
	{
		Data.RemoveAt(index, count);
	}

	char& operator[](int index)
	{
		guard(FString::operator[]);
		assert(index >= 0 && index < Data.Num());
		return Data.GetData()[index];
		unguardf("%d/%d", index, Data.Num());
	}
	const char& operator[](int index) const
	{
		guard(FString::operator[]);
		assert(index >= 0 && index < Data.Num());
		return Data.GetData()[index];
		unguardf("%d/%d", index, Data.Num());
	}

	// convert string to char* - use "*Str"
	FORCEINLINE const char *operator*() const
	{
		return IsEmpty() ? "" : Data.GetData();
	}
//	FORCEINLINE operator const char*() const
//	{
//		return IsEmpty() ? "" : Data.GetData();
//	}
	// comparison
	friend FORCEINLINE bool operator==(const FString& A, const FString& B)
	{
		return !strcmp(*A, *B);
	}
	friend FORCEINLINE bool operator==(const char* A, const FString& B)
	{
		return !strcmp(A, *B);
	}
	friend FORCEINLINE bool operator==(const FString& A, const char* B)
	{
		return !strcmp(*A, B);
	}

	friend FArchive& operator<<(FArchive &Ar, FString &S);

protected:
	TArray<char>		Data;
};

// Binary-compatible string, but with no allocations inside
template<int N>
class FStaticString : public FString
{
public:
	FORCEINLINE FStaticString()
	{
		Data.DataPtr = (void*)&StaticData[0];
		Data.MaxCount = N;
	}

	// operators
	FORCEINLINE FStaticString& operator=(const char* src)
	{
		return (FStaticString&) FString::operator=(src);
	}
	FORCEINLINE FStaticString& operator=(const FString& src)
	{
		return (FStaticString&) FString::operator=(src);
	}

protected:
	char	StaticData[N];
};


/*-----------------------------------------------------------------------------
	Guid
-----------------------------------------------------------------------------*/

class FGuid
{
public:
	uint32		A, B, C, D;

	friend FArchive& operator<<(FArchive &Ar, FGuid &G)
	{
		Ar << G.A << G.B << G.C << G.D;
#if FURY
		if (Ar.Game == GAME_Fury && Ar.ArLicenseeVer >= 24)
		{
			int guid5;
			Ar << guid5;
		}
#endif // FURY
		return Ar;
	}

	FORCEINLINE bool operator==(const FGuid& Other) const
	{
		return memcmp(this, &Other, sizeof(FGuid)) == 0;
	}
};


#if UNREAL3

/*-----------------------------------------------------------------------------
	Support for UE3 compressed files
-----------------------------------------------------------------------------*/

struct FCompressedChunkBlock
{
	int			CompressedSize;
	int			UncompressedSize;

	friend FArchive& operator<<(FArchive &Ar, FCompressedChunkBlock &B);
};

struct FCompressedChunkHeader
{
	int			Tag;
	int			BlockSize;				// maximal size of uncompressed block
	FCompressedChunkBlock Sum;			// summary for the whole compressed block
	TArray<FCompressedChunkBlock> Blocks;

	friend FArchive& operator<<(FArchive &Ar, FCompressedChunkHeader &H);
};

void appReadCompressedChunk(FArchive &Ar, byte *Buffer, int Size, int CompressionFlags);


/*-----------------------------------------------------------------------------
	UE3/UE4 bulk data - replacement for TLazyArray
-----------------------------------------------------------------------------*/

// UE3
#define BULKDATA_StoreInSeparateFile	0x01		// bulk stored in different file
#define BULKDATA_CompressedZlib			0x02		// name = BULKDATA_SerializeCompressedZLIB (UE4) ?
#define BULKDATA_CompressedLzo			0x10		// unknown name
#define BULKDATA_Unused					0x20		// empty bulk block
#define BULKDATA_SeparateData			0x40		// unknown name - bulk stored in a different place in the same file
#define BULKDATA_CompressedLzx			0x80		// unknown name

#if BLADENSOUL
#define BULKDATA_CompressedLzoEncr		0x100		// encrypted LZO
#endif

// UE4

#if UNREAL4

#define BULKDATA_PayloadAtEndOfFile		0x0001		// bulk data stored at the end of this file, data offset added to global data offset in package
//#define BULKDATA_CompressedZlib		0x0002		// the same value as for UE3
//#define BULKDATA_Unused				0x0020		// the same value as for UE3
#define BULKDATA_ForceInlinePayload		0x0040		// bulk data stored immediately after header
#define BULKDATA_PayloadInSeperateFile	0x0100		// data stored in .ubulk file near the asset (UE4.12+)

#endif // UNREAL4

struct FByteBulkData //?? separate FUntypedBulkData
{
	int		BulkDataFlags;				// BULKDATA_...
	int		ElementCount;				// number of array elements
	int64	BulkDataOffsetInFile;		// position in file, points to BulkData; 32-bit in UE3, 64-bit in UE4
	int		BulkDataSizeOnDisk;			// size of bulk data on disk
//	int		SavedBulkDataFlags;
//	int		SavedElementCount;
//	int		SavedBulkDataOffsetInFile;
//	int		SavedBulkDataSizeOnDisk;
	byte	*BulkData;					// pointer to array data
//	int		LockStatus;
//	FArchive *AttachedAr;

	FByteBulkData()
	:	BulkData(NULL)
	,	BulkDataOffsetInFile(0)
	{}

	virtual ~FByteBulkData()
	{
		ReleaseData();
	}

	virtual int GetElementSize() const
	{
		return 1;
	}

	void ReleaseData()
	{
		if (BulkData) appFree(BulkData);
		BulkData = NULL;
	}

	// support functions
	void SerializeHeader(FArchive &Ar);
	void SerializeData(FArchive &Ar);
	// main functions
	void Serialize(FArchive &Ar);
	void Skip(FArchive &Ar);

protected:
	void SerializeDataChunk(FArchive &Ar);
};

struct FWordBulkData : public FByteBulkData
{
	virtual int GetElementSize() const
	{
		return 2;
	}
};

struct FIntBulkData : public FByteBulkData
{
	virtual int GetElementSize() const
	{
		return 4;
	}
};

#endif // UNREAL3


// UE3 compression flags; may be used for other engines, so keep it outside of #if UNREAL3 block
#define COMPRESS_ZLIB		1
#define COMPRESS_LZO		2
#define COMPRESS_LZX		4

#if BLADENSOUL
#define COMPRESS_LZO_ENC_BNS	8					// encrypted LZO
#endif

#if SMITE
#define COMPRESS_LZO_ENC_SMITE	514					// encrypted LZO
#endif

#if GEARS4
#define COMPRESS_LZ4		0xFE					// custom umodel's constant
#endif

#define COMPRESS_FIND		0xFF					// use this flag for appDecompress when exact compression method is not known

#define PKG_StoreCompressed	 0x02000000
#define PKG_FilterEditorOnly 0x80000000

int appDecompress(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize, int Flags);

// UE4 has built-in AES encryption

extern FString GAesKey;

void appDecryptAES(byte* Data, int Size);

// Callback called when encrypted pak file is attempted to load
bool UE4EncryptedPak();


/*-----------------------------------------------------------------------------
	UE4 support
-----------------------------------------------------------------------------*/

#if UNREAL4

// Unreal engine 4 versions, declared as enum to be able to see all revisions in a single place
enum
{
	// Pre-release UE4 file versions
	VER_UE4_ASSET_REGISTRY_TAGS = 112,
	VER_UE4_TEXTURE_DERIVED_DATA2 = 124,
	VER_UE4_ADD_COOKED_TO_TEXTURE2D = 125,
	VER_UE4_REMOVED_STRIP_DATA = 130,
	VER_UE4_REMOVE_EXTRA_SKELMESH_VERTEX_INFLUENCES = 134,
	VER_UE4_TEXTURE_SOURCE_ART_REFACTOR = 143,
	VER_UE4_ADD_SKELMESH_MESHTOIMPORTVERTEXMAP = 152,
	VER_UE4_REMOVE_ARCHETYPE_INDEX_FROM_LINKER_TABLES = 163,
	VER_UE4_REMOVE_NET_INDEX = 196,
	VER_UE4_BULKDATA_AT_LARGE_OFFSETS = 198,
	VER_UE4_SUMMARY_HAS_BULKDATA_OFFSET = 212,
	VER_UE4_STATIC_MESH_STORE_NAV_COLLISION = 216,
	VER_UE4_DEPRECATED_STATIC_MESH_THUMBNAIL_PROPERTIES_REMOVED = 242,
	VER_UE4_APEX_CLOTH = 254,
	VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX = 269,
	VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES = 277,
	VER_UE4_APEX_CLOTH_LOD = 280,
	VAR_UE4_ARRAY_PROPERTY_INNER_TAGS = 282, // note: here's a typo in UE4 code - "VAR_" instead of "VER_"
	VER_UE4_KEEP_SKEL_MESH_INDEX_DATA = 283,
	VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING = 302,
	VER_UE4_REFERENCE_SKELETON_REFACTOR = 310,
	VER_UE4_FIXUP_ROOTBONE_PARENT = 312,
	VER_UE4_FIX_ANIMATIONBASEPOSE_SERIALIZATION = 331,
	VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES = 332,
	VER_UE4_SUPPORT_GPUSKINNING_8_BONE_INFLUENCES = 334,
	VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION = 335,
	VER_UE4_ENGINE_VERSION_OBJECT = 336,
	VER_UE4_SKELETON_GUID_SERIALIZATION = 338,
	// UE4.0 source code was released on GitHub. Note: if we don't have any VER_UE4_...
	// values between two VER_UE4_xx constants, for instance, between VER_UE4_0 and VER_UE4_1,
	// it doesn't matter for this framework which version will be serialized serialized -
	// 4.0 or 4.1, because 4.1 has nothing new for supported object formats compared to 4.0.
	VER_UE4_0 = 342,
	VER_UE4_1 = 352,
	VER_UE4_2 = 363,
		VER_UE4_LOAD_FOR_EDITOR_GAME = 365,
		VER_UE4_FTEXT_HISTORY = 368,					// used for UStaticMesh versioning
		VER_UE4_STORE_BONE_EXPORT_NAMES = 370,
	VER_UE4_3 = 382,
		VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP = 384,
	VER_UE4_4 = 385,
		VER_UE4_SKELETON_ADD_SMARTNAMES = 388,
		VER_UE4_SOUND_COMPRESSION_TYPE_ADDED = 392,
		VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN = 394,	// used for UStaticMesh versioning
		VER_UE4_DEPRECATE_UMG_STYLE_ASSETS = 397,		// used for UStaticMesh versioning
	VER_UE4_5 = 401,
	VER_UE4_6 = 413,
		VER_UE4_RENAME_WIDGET_VISIBILITY = 416,			// used for UStaticMesh versioning
		VER_UE4_ANIMATION_ADD_TRACKCURVES = 417,
	VER_UE4_7 = 434,
		VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG = 441,
		VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION = 444,
	VER_UE4_8 = 451,
		VER_UE4_SERIALIZE_TEXT_IN_PACKAGES = 459,
	VER_UE4_9 = 482,
	VER_UE4_10 = VER_UE4_9,								// exactly the same file version for 4.9 and 4.10
		VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT = 485,
		VER_UE4_SOUND_CONCURRENCY_PACKAGE = 489,		// used for UStaticMesh versioning
	VER_UE4_11 = 498,
		VER_UE4_INNER_ARRAY_TAG_INFO = 500,
		VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG = 503,
		VER_UE4_NAME_HASHES_SERIALIZED = 504,
	VER_UE4_12 = 504,
	VER_UE4_13 = 505,
		VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS = 507,
		VER_UE4_TemplateIndex_IN_COOKED_EXPORTS = 508,
	VER_UE4_14 = 508,
		VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT = 509,
		VER_UE4_ADDED_SEARCHABLE_NAMES = 510,
	VER_UE4_15 = 510,
		VER_UE4_64BIT_EXPORTMAP_SERIALSIZES = 511,
	VER_UE4_16 = 513,
	VER_UE4_17 = 513,
	VER_UE4_18 = 514,
		VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID = 516,
	VER_UE4_19 = 516,
	// look for NEW_ENGINE_VERSION over the code to find places where version constants should be inserted.
	// LATEST_SUPPORTED_UE4_VERSION should be updated too.
};

int GetUE4CustomVersion(const FArchive& Ar, const FGuid& Guid);

struct FFrameworkObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		MoveCompressedAnimDataToTheDDC = 5,
		// UE4.12 = 6
		SmartNameRefactor = 7,
		// UE4.13 = 12
		RemoveSoundWaveCompressionName = 12,
		// UE4.14 = 17
		MoveCurveTypesToSkeleton = 15,
		CacheDestructibleOverlaps = 16,
		GeometryCacheMissingMaterials = 17,	// not needed now - for UGeometryCache
		// UE4.15 = 22
		// UE4.16 = 23
		// UE4.17 = 28
		// UE4.18 = 30

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xCFFC743F, 0x43B04480, 0x939114DF, 0x171D2073 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

#if FRIDAY13 || TEKKEN7
		if (Ar.Game == GAME_Friday13 || Ar.Game == GAME_Tekken7) return (Type)14;		// pre-UE4.14
#endif

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)6;
		if (Ar.Game < GAME_UE4(14))
			return RemoveSoundWaveCompressionName;
		if (Ar.Game < GAME_UE4(15))
			return GeometryCacheMissingMaterials;
		if (Ar.Game < GAME_UE4(16))
			return (Type)22;
		if (Ar.Game < GAME_UE4(17))
			return (Type)23;
		if (Ar.Game < GAME_UE4(18))
			return (Type)28;
		if (Ar.Game < GAME_UE4(19))
			return (Type)30;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FEditorObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.12 = 2
		// UE4.13 = 6
		// UE4.14 = 8
		RefactorMeshEditorMaterials = 8,
		// UE4.15 = 14
		UPropertryForMeshSection = 10,
		// UE4.16 = 17
		// UE4.17, UE4.18 = 20

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xE4B068ED, 0xF49442E9, 0xA231DA0B, 0x2E46BB41 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

#if FRIDAY13 || TEKKEN7
		if (Ar.Game == GAME_Friday13 || Ar.Game == GAME_Tekken7) return (Type)7;		// pre-UE4.14
#endif

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)2;
		if (Ar.Game < GAME_UE4(14))
			return (Type)6;
		if (Ar.Game < GAME_UE4(15))
			return RefactorMeshEditorMaterials;
		if (Ar.Game < GAME_UE4(16))
			return (Type)14;
		if (Ar.Game < GAME_UE4(17))
			return (Type)17;
		if (Ar.Game < GAME_UE4(19))		// UE4.17 and 4.18
			return (Type)20;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FSkeletalMeshCustomVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.13 = 4
		CombineSectionWithChunk = 1,
		CombineSoftAndRigidVerts = 2,
		RecalcMaxBoneInfluences = 3,
		SaveNumVertices = 4,
		// UE4.14 = 5
		UseSharedColorBufferFormat = 6,		// separate vertex stream for vertex influences
		UseSeparateSkinWeightBuffer = 7,	// use FColorVertexStream for both static and skeletal meshes
		// UE4.15 = 7
		NewClothingSystemAdded = 8,
		// UE4.16, UE4.17 = 9
		// UE4.18 = 10
		CompactClothVertexBuffer = 10,
		RemoveSourceData = 11,
		SplitModelAndRenderData = 12,
		RemoveTriangleSorting = 13,
		RemoveDuplicatedClothingSections = 14,
		DeprecateSectionDisabledFlag = 16,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xD78A4A00, 0xE8584697, 0xBAA819B5, 0x487D46B4 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
#if PARAGON
		if (Ar.Game == GAME_Paragon) return (Type)12;
#endif

		if (Ar.Game < GAME_UE4(13))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(14))
			return SaveNumVertices;
		if (Ar.Game < GAME_UE4(15))
			return (Type)5;
		if (Ar.Game < GAME_UE4(16))
			return UseSeparateSkinWeightBuffer;
		if (Ar.Game < GAME_UE4(18)) // 4.16 and 4.17
			return (Type)9;
		if (Ar.Game < GAME_UE4(19))
			return CompactClothVertexBuffer;
		if (Ar.Game < GAME_UE4(20))
			return DeprecateSectionDisabledFlag;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FRenderingObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.14
		TextureStreamingMeshUVChannelData = 10,
		// UE4.16 = 15
		// UE4.17 = 19
		// UE4.18 = 20

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x12F88B9F, 0x88754AFC, 0xA67CD90C, 0x383ABD29 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

#if FRIDAY13 || TEKKEN7
		if (Ar.Game == GAME_Friday13 || Ar.Game == GAME_Tekken7) return (Type)9;		// pre-UE4.14
#endif

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)2;
		if (Ar.Game < GAME_UE4(14))
			return (Type)4;
		if (Ar.Game < GAME_UE4(16))	// 4.14 and 4.15
			return (Type)12;
		if (Ar.Game < GAME_UE4(17))
			return (Type)15;
		if (Ar.Game < GAME_UE4(18))
			return (Type)19;
		if (Ar.Game < GAME_UE4(19))
			return (Type)20;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FAnimPhysObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.16 = 3
		RemoveUIDFromSmartNameSerialize = 5,
		// UE4.17 = 7
		SmartNameRefactorForDeterministicCooking = 10,
		// UE4.18 = 12
		AddLODToCurveMetaData = 12,
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x12F88B9F, 0x88754AFC, 0xA67CD90C, 0x383ABD29 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
		if (Ar.Game < GAME_UE4(16))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(17))
			return (Type)3;
		if (Ar.Game < GAME_UE4(18))
			return (Type)7;
		if (Ar.Game < GAME_UE4(19))
			return AddLODToCurveMetaData;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

class FStripDataFlags
{
public:
	FStripDataFlags(FArchive& Ar, int MinVersion = VER_UE4_REMOVED_STRIP_DATA)
	{
		if (Ar.ArVer >= MinVersion)
		{
			Ar << GlobalStripFlags << ClassStripFlags;
		}
		else
		{
			GlobalStripFlags = ClassStripFlags = 0;
		}
	}

	FORCEINLINE bool IsEditorDataStripped() const
	{
		return (GlobalStripFlags & 1) != 0;
	}

	FORCEINLINE bool IsDataStrippedForServer() const
	{
		return (GlobalStripFlags & 2) != 0;
	}

	FORCEINLINE bool IsClassDataStripped(byte Flag) const
	{
		return (ClassStripFlags & Flag) != 0;
	}

protected:
	byte	GlobalStripFlags;
	byte	ClassStripFlags;
};

#endif // UNREAL4

/*-----------------------------------------------------------------------------
	Global variables
-----------------------------------------------------------------------------*/

extern FArchive *GDummySave;
extern int       GForceGame;
extern int       GForcePackageVersion;
extern byte      GForcePlatform;
extern byte      GForceCompMethod;


/*-----------------------------------------------------------------------------
	Miscellaneous game support
-----------------------------------------------------------------------------*/

#if TRIBES3
// macro to skip Tribes3 FHeader structure
// check==3 -- Tribes3
// check==4 -- Bioshock
#define TRIBES_HDR(Ar,Ver)							\
	int t3_hdrV = 0, t3_hdrSV = 0;					\
	if (Ar.Engine() == GAME_VENGEANCE && Ar.ArLicenseeVer >= Ver) \
	{												\
		int check;									\
		Ar << check;								\
		if (check == 3)								\
			Ar << t3_hdrV << t3_hdrSV;				\
		else if (check == 4)						\
			Ar << t3_hdrSV;							\
		else										\
			appError("T3:check=%X (Pos=%X)", check, Ar.Tell()); \
	}
#endif // TRIBES3


#endif // __UNCORE_H__
