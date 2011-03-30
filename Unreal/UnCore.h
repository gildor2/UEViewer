#ifndef __UNCORE_H__
#define __UNCORE_H__


#if RENDERING
#	include "CoreGL.h"		//?? for materials only
#endif


// forward declarations
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
#define unguardf(x)
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

#define PROFILE_POINT(Label)	appPrintProfiler(); printf("PROFILE: " #Label "\n");

#endif

/*-----------------------------------------------------------------------------
	Game directory support
-----------------------------------------------------------------------------*/

void appSetRootDirectory(const char *dir, bool recurse = true);
void appSetRootDirectory2(const char *filename);
const char *appGetRootDirectory();

struct CGameFileInfo
{
	char		RelativeName[256];		// relative to RootDirectory
	const char *ShortFilename;			// without path
	const char *Extension;				// points to extension part (after '.')
	bool		IsPackage;
};

// Ext = NULL -> use any package extension
// Filename can contain extension, but should not contain path
const CGameFileInfo *appFindGameFile(const char *Filename, const char *Ext = NULL);
const char *appSkipRootDir(const char *Filename);
FArchive *appCreateFileReader(const CGameFileInfo *info);
void appEnumGameFiles(bool (*Callback)(const CGameFileInfo*), const char *Ext = NULL);


#if UNREAL3
extern const char *GStartupPackage;
#endif


/*-----------------------------------------------------------------------------
	FName class
-----------------------------------------------------------------------------*/

class FName
{
public:
	int			Index;
#if UNREAL3
	int			ExtraIndex;
#endif
	const char	*Str;
	bool		NameGenerated;

	FName()
	:	Index(0)
	,	Str(NULL)
#if UNREAL3
	,	ExtraIndex(0)
	,	NameGenerated(false)
#endif
	{}

	~FName()
	{
		if (NameGenerated) free((void*)Str);
	}

	void AppendIndex()
	{
		if (NameGenerated || !ExtraIndex) return;
		NameGenerated = true;
		Str = strdup(va("%s_%d", Str, ExtraIndex-1));
	}

#if BIOSHOCK
	void AppendIndexBio()
	{
		if (NameGenerated || !ExtraIndex) return;
		NameGenerated = true;
		Str = strdup(va("%s%d", Str, ExtraIndex-1));	// without "_" char
	}
#endif // BIOSHOCK

	inline const char *operator*() const
	{
		return Str;
	}
	inline operator const char*() const
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
	FArchive class
-----------------------------------------------------------------------------*/


enum EGame
{
	GAME_UNKNOWN   = 0,			// should be 0

	GAME_UE1       = 0x1000,
		GAME_Undying,

	GAME_UE2       = 0x2000,
		GAME_UT2,
		GAME_Pariah,
		GAME_SplinterCell,
		GAME_Lineage2,
		GAME_Exteel,
		GAME_Ragnarok2,
		GAME_RepCommando,
		GAME_Loco,
		GAME_BattleTerr,
		GAME_UC1,				// note: not UE2X

	GAME_VENGEANCE = 0x2100,	// variant of UE2
		GAME_Tribes3,
		GAME_Swat4,				// not autodetected, overlaps with Tribes3
		GAME_Bioshock,

	GAME_LEAD      = 0x2200,

	GAME_UE2X      = 0x4000,
		GAME_UC2,

	GAME_UE3       = 0x8000,
		GAME_EndWar,
		GAME_MassEffect,
		GAME_MassEffect2,
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
		GAME_Borderlands,
		GAME_AA3,
		GAME_DarkVoid,
		GAME_Legendary,
		GAME_Tera,
		GAME_APB,
		GAME_AlphaProtocol,
		GAME_Transformers,
		GAME_MortalOnline,
		GAME_Enslaved,
		GAME_MOH2010,
		GAME_Berkanix,
		GAME_DOH,
		GAME_DCUniverse,
		GAME_Bulletstorm,
		GAME_Undertow,
		GAME_Singularity,
		GAME_Tron,

	GAME_MIDWAY3   = 0x8100,	// variant of UE3
		GAME_A51,
		GAME_Wheelman,
		GAME_MK,
		GAME_Strangle,
		GAME_TNA,

	GAME_ENGINE    = 0xFF00		// mask for game engine
};

enum EPlatform
{
	PLATFORM_UNKNOWN = 0,
	PLATFORM_PC,
	PLATFORM_XBOX360,
	PLATFORM_PS3,
	PLATFORM_IOS,
};


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
	,	ArVer(99999)			//?? something large
	,	ArLicenseeVer(0)
	,	ReverseBytes(false)
	,	Game(GAME_UNKNOWN)
	,	Platform(PLATFORM_PC)
	{}

	virtual ~FArchive()
	{}

	void DetectGame();
	void OverrideVersion();

	inline int Engine() const
	{
		return (Game & GAME_ENGINE);
	}

	void SetupFrom(const FArchive &Other)
	{
		ArVer         = Other.ArVer;
		ArLicenseeVer = Other.ArLicenseeVer;
		ReverseBytes  = Other.ReverseBytes;
		Game          = Other.Game;
		Platform      = Other.Platform;
	}

	virtual void Seek(int Pos) = 0;
	virtual bool IsEof() const
	{
		return false;
	}

	virtual int Tell() const
	{
		return ArPos;
	}

	virtual void Serialize(void *data, int size) = 0;
	void ByteOrderSerialize(void *data, int size);

	void Printf(const char *fmt, ...);

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
		return Tell() == GetStopper();
	}

	virtual int GetFileSize() const
	{
		return 0;
	}

	virtual FArchive& operator<<(FName &N)
	{
		return *this;
	}
	virtual FArchive& operator<<(UObject *&Obj)
	{
		return *this;
	}
};


inline FArchive& operator<<(FArchive &Ar, bool &B)
{
	Ar.Serialize(&B, 1);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, char &B)
{
	Ar.Serialize(&B, 1);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, byte &B)
{
	Ar.ByteOrderSerialize(&B, 1);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, short &B)
{
	Ar.ByteOrderSerialize(&B, 2);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, word &B)
{
	Ar.ByteOrderSerialize(&B, 2);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, int &B)
{
	Ar.ByteOrderSerialize(&B, 4);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, int64 &B)
{
	Ar.ByteOrderSerialize(&B, 8);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, unsigned &B)
{
	Ar.ByteOrderSerialize(&B, 4);
	return Ar;
}
inline FArchive& operator<<(FArchive &Ar, float &B)
{
	Ar.ByteOrderSerialize(&B, 4);
	return Ar;
}


class FFileReader : public FArchive
{
public:
	FFileReader()
	:	f(NULL)
	{}

	FFileReader(FILE *InFile)
	:	f(InFile)
	{
		IsLoading = true;
		ArPos     = 0;
	}

	FFileReader(const char *Filename, bool loading = true)
	:	f(fopen(Filename, loading ? "rb" : "wb"))
	{
		guard(FFileReader::FFileReader);
		if (!f)
			appError("Unable to open file %s", Filename);
		IsLoading = loading;
		ArPos     = 0;
		const char *s = strrchr(Filename, '/');
		if (!s)     s = strrchr(Filename, '\\');
		if (s) s++; else s = Filename;
		appStrncpyz(ShortName, s, ARRAY_COUNT(ShortName));
		unguardf(("%s", Filename));
	}

	virtual ~FFileReader()
	{
		if (f) fclose(f);
	}

	void Setup(FILE *InFile, bool Loading)
	{
		f         = InFile;
		IsLoading = Loading;
	}

	virtual void Seek(int Pos)
	{
		guard(FFileReader::Seek);
		fseek(f, Pos, SEEK_SET);
		ArPos = ftell(f);
		assert(Pos == ArPos);
		unguardf(("File=%s, pos=%d", ShortName, Pos));
	}

	virtual bool IsEof() const
	{
		int pos  = ftell(f); fseek(f, 0, SEEK_END);
		int size = ftell(f); fseek(f, pos, SEEK_SET);
		return size == pos;
	}

	virtual FArchive& operator<<(FName &N)
	{
		*this << AR_INDEX(N.Index);
		return *this;
	}

	virtual FArchive& operator<<(UObject *&Obj)
	{
		int tmp;
		*this << AR_INDEX(tmp);
		printf("Object: %d\n", tmp);
		return *this;
	}

	virtual void Serialize(void *data, int size)
	{
		guard(FFileReader::Serialize);
		if (size == 0) return;
		if (ArStopper > 0 && ArPos + size > ArStopper)
			appError("Serializing behind stopper (%d+%d > %d)", ArPos, size, ArStopper);

		int res;
		if (IsLoading)
			res = fread(data, size, 1, f);
		else
			res = fwrite(data, size, 1, f);
		if (res != 1)
			appError("Unable to serialize %d bytes at pos=%d", size, ArPos);
		ArPos += size;
#if PROFILE
		GNumSerialize++;
		GSerializeBytes += size;
#endif
		unguardf(("File=%s", ShortName));
	}

	virtual int GetFileSize() const
	{
		int pos  = ftell(f); fseek(f, 0, SEEK_END);
		int size = ftell(f); fseek(f, pos, SEEK_SET);
		return size;
	}

protected:
	FILE	*f;
	char	ShortName[128];
};



class FMemReader : public FArchive
{
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
			appError("Serializing behind stopper (%d+%d > %d)", ArPos, size, ArStopper);
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
		return Ar << R.Pitch << R.Yaw << R.Roll;
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


/*-----------------------------------------------------------------------------
	Typeinfo for fast array serialization
-----------------------------------------------------------------------------*/

// Default typeinfo
template<class T> struct TTypeInfo
{
	//?? note: some fields were added for TArray serializer, but currently
	//?? we have made a specialized serializers for simple and raw types
	//?? in SIMPLE_TYPE and RAW_TYPE macros, so these fields are no more
	//?? used
	enum { FieldSize = sizeof(T) };
	enum { NumFields = 1         };
	enum { IsSimpleType = 0      };
	enum { IsRawType = 0         };
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
};											\
template<> inline void CopyArray<Type>(TArray<Type> &Dst, const TArray<Type> &Src) \
{											\
	Dst.RawCopy(Src, sizeof(Type));			\
}											\
inline FArchive& operator<<(FArchive &Ar, TArray<Type> &A) \
{											\
	staticAssert(sizeof(Type) == TTypeInfo<Type>::NumFields * TTypeInfo<Type>::FieldSize, \
		Error_In_TypeInfo);					\
	return A.SerializeSimple(Ar, TTypeInfo<Type>::NumFields, TTypeInfo<Type>::FieldSize); \
}


// Declare type, which memory layout is the same as disk layout
#define RAW_TYPE(Type)						\
template<> struct TTypeInfo<Type>			\
{											\
	enum { FieldSize = sizeof(Type) };		\
	enum { NumFields = 1 };					\
	enum { IsSimpleType = 0 };				\
	enum { IsRawType = 1 };					\
};											\
template<> inline void CopyArray<Type>(TArray<Type> &Dst, const TArray<Type> &Src) \
{											\
	Dst.RawCopy(Src, sizeof(Type));			\
}											\
inline FArchive& operator<<(FArchive &Ar, TArray<Type> &A) \
{											\
	return A.SerializeRaw(Ar, TArray<Type>::SerializeItem, sizeof(Type)); \
}


// Note: SIMPLE and RAW types does not need constructors for loading
//!! add special mode to check SIMPLE/RAW types (serialize twice and compare results)

#if 0
//!! testing
#undef  SIMPLE_TYPE
#undef  RAW_TYPE
#define SIMPLE_TYPE(x,y)
#define RAW_TYPE(x)
#endif


/*-----------------------------------------------------------------------------
	TArray/TLazyArray templates
-----------------------------------------------------------------------------*/

/*
 * NOTES:
 *	- FArray/TArray should not contain objects with virtual tables (no
 *	  constructor/destructor support)
 *	- should not use new[] and delete[] here, because compiler will alloc
 *	  additional 'count' field for correct delete[], but we uses appMalloc/
 *	  appFree calls.
 */

class FArray
{
public:
	FArray()
	:	DataCount(0)
	,	MaxCount(0)
	,	DataPtr(NULL)
	{}
	~FArray()
	{
		if (DataPtr)
			appFree(DataPtr);
		DataPtr   = NULL;
		DataCount = 0;
		MaxCount  = 0;
	}

	void *GetData()
	{
		return DataPtr;
	}
	const void *GetData() const
	{
		return DataPtr;
	}
	int Num() const
	{
		return DataCount;
	}

	void Empty (int count, int elementSize);
	void Add   (int count, int elementSize);
	void Insert(int index, int count, int elementSize);
	void Remove(int index, int count, int elementSize);

	void RawCopy(const FArray &Src, int elementSize);

	// serializers
	FArchive& SerializeSimple(FArchive &Ar, int NumFields, int FieldSize);
	FArchive& SerializeRaw(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize);

protected:
	void	*DataPtr;
	int		DataCount;
	int		MaxCount;

	// serializers
	FArchive& Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize);
};

FArchive& SerializeLazyArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*));
#if UNREAL3
FArchive& SerializeRawArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*));
#endif

// NOTE: this container cannot hold objects, required constructor/destructor
// (at least, Add/Insert/Remove functions are not supported, but can serialize
// such data)
template<class T> class TArray : public FArray
{
public:
	TArray()
	:	FArray()
	{}
	~TArray()
	{
		// destruct all array items
#if 0
		T *P, *P2;
		for (P = (T*)DataPtr, P2 = P + DataCount; P < P2; P++)
			P->~T();
#else
		// this code is more compact at least for VC6 when T has no destructor
		// (for code above, compiler will always compute P2, even when not needed)
		for (int i = 0; i < DataCount; i++)
			((T*)DataPtr + i)->~T();
#endif
	}
	// data accessors
	T& operator[](int index)
	{
#if DO_GUARD_MAX
		guardfunc;
#else
		guard(TArray[]);
#endif
		assert(index >= 0 && index < DataCount);
		return *((T*)DataPtr + index);
		unguardf(("%d/%d", index, DataCount));
	}
	const T& operator[](int index) const
	{
#if DO_GUARD_MAX
		guardfunc;
#else
		guard(TArray[]);
#endif
		assert(index >= 0 && index < DataCount);
		return *((T*)DataPtr + index);
		unguardf(("%d/%d", index, DataCount));
	}

	int Add(int count = 1)
	{
		int index = DataCount;
		FArray::Insert(index, count, sizeof(T));
		for (int i = 0; i < count; i++)
			new ((T*)DataPtr + index + i) T;
		return index;
	}

	void Insert(int index, int count = 1)
	{
		FArray::Insert(index, count, sizeof(T));
	}

	void Remove(int index, int count = 1)
	{
		// destruct specified array items
#if 0
		T *P, *P2;
		for (P = (T*)DataPtr + index, P2 = P + count; P < P2; P++)
			P->~T();
#else
		for (int i = index; i < index + count; i++)
			((T*)DataPtr + i)->~T();
#endif
		// remove items from array
		FArray::Remove(index, count, sizeof(T));
	}

	int AddItem(const T& item)
	{
		int index = Add();
		Item(index) = item;
		return index;
	}

	T& AddItem()
	{
		int index = Add();
		return Item(index);
	}

	int FindItem(const T& item, int startIndex = 0)
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
		for (int i = 0; i < DataCount; i++)
			((T*)DataPtr + i)->~T();
		// remove data array (count=0) or preallocate memory (count>0)
		FArray::Empty(count, sizeof(T));
	}

	void Sort(int (*cmpFunc)(const T*, const T*))
	{
		QSort<T>((T*)DataPtr, DataCount, cmpFunc);
	}

	// serializer
#if _MSC_VER == 1200			// VC6 bug
	friend FArchive& operator<<(FArchive &Ar, TArray &A);
#else
	template<class T2> friend FArchive& operator<<(FArchive &Ar, TArray<T2> &A);
#endif

//?? -- cannot compile with VC7: protected:
	// serializer helper; used from 'operator<<(FArchive, TArray<>)' only
	static void SerializeItem(FArchive &Ar, void *item)
	{
		if (Ar.IsLoading)
			new (item) T;		// construct item before reading
		Ar << *(T*)item;		// serialize item
	}

private:
	// disable array copying
	TArray(const TArray &Other)
	:	FArray()
	{}
	TArray& operator=(const TArray &Other)
	{
		return this;
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
};



// VC6 sometimes cannot instantiate this function, when declared inside
// template class
template<class T> FArchive& operator<<(FArchive &Ar, TArray<T> &A)
{
#if DO_GUARD_MAX
	guardfunc;
#endif
	// erase previous data before loading in a case of non-POD data
	if (Ar.IsLoading)
		A.Empty();
	return A.Serialize(Ar, TArray<T>::SerializeItem, sizeof(T));
#if DO_GUARD_MAX
	unguard;
#endif
}

template<class T> FORCEINLINE void* operator new(size_t size, TArray<T> &Array)
{
	guard(TArray::operator new);
	assert(size == sizeof(T));
	return &Array.AddItem();
	unguard;
}


// TLazyArray implemented as simple wrapper around TArray with
// different serialization function
// Purpose in UE: array with can me loaded asynchronously (when serializing
// it 1st time only disk position is remembered, and later array can be
// read from file when needed)

template<class T> class TLazyArray : public TArray<T>
{
	// Helper function to reduce TLazyArray<>::operator<<() code size.
	// Used as C-style wrapper around TArray<>::operator<<().
	static FArchive& SerializeArray(FArchive &Ar, void *Array)
	{
		return Ar << *(TArray<T>*)Array;
	}

	friend FArchive& operator<<(FArchive &Ar, TLazyArray &A)
	{
#if DO_GUARD_MAX
		guardfunc;
#endif
		return SerializeLazyArray(Ar, A, SerializeArray);
#if DO_GUARD_MAX
		unguard;
#endif
	}
};


inline void SkipLazyArray(FArchive &Ar)
{
	guard(SkipLazyArray);
	assert(Ar.IsLoading);
	int pos;
	Ar << pos;
	assert(Ar.Tell() < pos);
	Ar.Seek(pos);
	unguard;
}


#if UNREAL3

// NOTE: real class name is unknown; other suitable names: TCookedArray, TPodArray.
// Purpose in UE: array, which file contents exactly the same as in-memory
// contents. Whole array can be read using single read call. Package
// engine version should equals to game engine version, otherwise per-element
// reading will be performed (as usual in TArray)
// There is no reading optimization performed here (in umodel)
template<class T> class TRawArray : protected TArray<T>
{
public:
	// Helper function to reduce TRawArray<>::operator<<() code size.
	// Used as C-style wrapper around TArray<>::operator<<().
	static FArchive& SerializeArray(FArchive &Ar, void *Array)
	{
		return Ar << *(TArray<T>*)Array;
	}

	friend FArchive& operator<<(FArchive &Ar, TRawArray &A)
	{
#if DO_GUARD_MAX
		guardfunc;
#endif
		return SerializeRawArray(Ar, A, SerializeArray);
#if DO_GUARD_MAX
		unguard;
#endif
	}

protected:
	// disallow direct creation of TRawArray, this is a helper class with a
	// different serializer
	TRawArray()
	{}
};

inline void SkipRawArray(FArchive &Ar, int Size)
{
	guard(SkipRawArray);
	if (Ar.ArVer >= 453)
	{
		int ElementSize, Count;
		Ar << ElementSize << Count;
		assert(ElementSize == Size);
		Ar.Seek(Ar.Tell() + ElementSize * Count);
	}
	else
	{
		assert(Size > 0);
		int Count;
		Ar << Count;
		Ar.Seek(Ar.Tell() + Size * Count);
	}
	unguard;
}

// helper function for RAW_ARRAY macro
template<class T> inline TRawArray<T>& ToRawArray(TArray<T> &Arr)
{
	return (TRawArray<T>&)Arr;
}

#define RAW_ARRAY(Arr)		ToRawArray(Arr)

#endif // UNREAL3

template<typename T1, typename T2> void CopyArray(TArray<T1> &Dst, const TArray<T2> &Src)
{
	guard(CopyArray2);
	Dst.Empty(Src.Num());
	if (Src.Num())
	{
		Dst.Add(Src.Num());
		for (int i = 0; i < Src.Num(); i++)
			Dst[i] = Src[i];
	}
	unguard;
}


// Declare fundamental types
SIMPLE_TYPE(byte,     byte)
SIMPLE_TYPE(char,     char)
SIMPLE_TYPE(short,    short)
SIMPLE_TYPE(word,     word)
SIMPLE_TYPE(int,      int)
SIMPLE_TYPE(unsigned, unsigned)
SIMPLE_TYPE(float,    float)

// Aggregates
SIMPLE_TYPE(FVector, float)
SIMPLE_TYPE(FQuat,   float)
SIMPLE_TYPE(FCoords, float)
SIMPLE_TYPE(FColor,  byte)


/*-----------------------------------------------------------------------------
	TMap template
-----------------------------------------------------------------------------*/

template<class TK, class TV> struct TMapPair
{
	TK		Key;
	TV		Value;

#if _MSC_VER != 1200		// not for VC6
	friend FArchive& operator<<(FArchive &Ar, TMapPair &V)
	{
		return Ar << V.Key << V.Value;
	}
#endif
};

#if _MSC_VER == 1200		// for VC6
template<class TK, class TV> FArchive& operator<<(FArchive &Ar, TMapPair<TK, TV> &V)
{
	return Ar << V.Key << V.Value;
}
#endif

template<class TK, class TV> class TMap : public TArray<TMapPair<TK, TV> >
{
public:
	friend FArchive& operator<<(FArchive &Ar, TMap &Map)
	{
		return Ar << (TArray<TMapPair<TK, TV> >&)Map;
	}
};

/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

class FString : public TArray<char>
{
public:
	FString()
	{}
	FString(const char* src);

	char* Detach();		// use FString as allocated char*, FString became empty

	inline const char *operator*() const
	{
		return (char*)DataPtr;
	}
	inline operator const char*() const
	{
		return (char*)DataPtr;
	}

	friend FArchive& operator<<(FArchive &Ar, FString &S);
};


/*-----------------------------------------------------------------------------
	Guid
-----------------------------------------------------------------------------*/

class FGuid
{
//!!	DECLARE_STRUCT(FGuid) -- require include UnObject.h before this; possibly - separate typeinfo and UObject headers
public:
	unsigned	A, B, C, D;

/*	BEGIN_PROP_TABLE
		PROP_INT(A)
		PROP_INT(B)
		PROP_INT(C)
		PROP_INT(D)
	END_PROP_TABLE */

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
};


#if UNREAL3

/*-----------------------------------------------------------------------------
	Support for UE3 compressed files
-----------------------------------------------------------------------------*/

struct FCompressedChunkBlock
{
	int			CompressedSize;
	int			UncompressedSize;

	friend FArchive& operator<<(FArchive &Ar, FCompressedChunkBlock &B)
	{
		return Ar << B.CompressedSize << B.UncompressedSize;
	}
};

struct FCompressedChunkHeader
{
	int			Tag;
	int			BlockSize;				// maximal size of uncompressed block
	int			CompressedSize;
	int			UncompressedSize;
	TArray<FCompressedChunkBlock> Blocks;

	friend FArchive& operator<<(FArchive &Ar, FCompressedChunkHeader &H)
	{
		guard(FCompressedChunkHeader<<);
		int i;
		Ar << H.Tag;
		if (H.Tag == PACKAGE_FILE_TAG_REV)
			Ar.ReverseBytes = !Ar.ReverseBytes;
#if BERKANIX
		else if (Ar.Game == GAME_Berkanix && H.Tag == 0xF2BAC156)
		{
			// do nothing, correct tag
		}
#endif // BERKANIX
		else
			assert(H.Tag == PACKAGE_FILE_TAG);
		Ar << H.BlockSize << H.CompressedSize << H.UncompressedSize;
#if 0
		if (H.BlockSize == PACKAGE_FILE_TAG)
			H.BlockSize = (Ar.ArVer >= 369) ? 0x20000 : 0x8000;
		int BlockCount = (H.UncompressedSize + H.BlockSize - 1) / H.BlockSize;
		H.Blocks.Empty(BlockCount);
		H.Blocks.Add(BlockCount);
		for (i = 0; i < BlockCount; i++)
			Ar << H.Blocks[i];
#else
		H.BlockSize = 0x20000;
		H.Blocks.Empty((H.UncompressedSize + 0x20000 - 1) / 0x20000);	// optimized for block size 0x20000
		int CompSize = 0, UncompSize = 0;
		while (CompSize < H.CompressedSize && UncompSize < H.UncompressedSize)
		{
			FCompressedChunkBlock *Block = new (H.Blocks) FCompressedChunkBlock;
			Ar << *Block;
			CompSize   += Block->CompressedSize;
			UncompSize += Block->UncompressedSize;
		}
		// check header; seen one package where sum(Block.CompressedSize) < H.CompressedSize,
		// but UncompressedSize is exact
		assert(/*CompSize == H.CompressedSize &&*/ UncompSize == H.UncompressedSize);
		if (H.Blocks.Num() > 1)
			H.BlockSize = H.Blocks[0].UncompressedSize;
#endif
		return Ar;
		unguardf(("pos=%X", Ar.Tell()));
	}
};

void appReadCompressedChunk(FArchive &Ar, byte *Buffer, int Size, int CompressionFlags);


/*-----------------------------------------------------------------------------
	UE3 bulk data - replacement for TLazyArray
-----------------------------------------------------------------------------*/

#define BULKDATA_StoreInSeparateFile	0x01		// bulk stored in different file
#define BULKDATA_CompressedZlib			0x02		// unknown name
#define BULKDATA_CompressedLzo			0x10		// unknown name
#define BULKDATA_NoData					0x20		// unknown name - empty bulk block
#define BULKDATA_SeparateData			0x40		// unknown name - bulk stored in a different place in the same file
#define BULKDATA_CompressedLzx			0x80		// unknown name

struct FByteBulkData //?? separate FUntypedBulkData
{
	int		BulkDataFlags;				// BULKDATA_...
	int		ElementCount;				// number of array elements
	int		BulkDataOffsetInFile;		// position in file, points to BulkData
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
	{}

	virtual ~FByteBulkData()
	{
		if (BulkData) appFree(BulkData);
	}

	virtual int GetElementSize() const
	{
		return 1;
	}

	// support functions
	void SerializeHeader(FArchive &Ar);
	void SerializeChunk(FArchive &Ar);
	// main functions
	void Serialize(FArchive &Ar);
	void Skip(FArchive &Ar);
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

// UE3 compression flags
#define COMPRESS_ZLIB		1
#define COMPRESS_LZO		2
#define COMPRESS_LZX		4

int appDecompress(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize, int Flags);


#endif // UNREAL3


/*-----------------------------------------------------------------------------
	Global variables
-----------------------------------------------------------------------------*/

extern FArchive *GDummySave;
extern int       GForceGame;
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
