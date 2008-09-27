#ifndef __UNCORE_H__
#define __UNCORE_H__


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


/*-----------------------------------------------------------------------------
	FName class
-----------------------------------------------------------------------------*/

class FName
{
public:
	int			Index;
	const char	*Str;

	FName()
	:	Index(0)
	,	Str(NULL)
	{}

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

class FArchive
{
public:
	bool	IsLoading;
	int		ArVer;
	int		ArLicenseeVer;
	int		ArPos;
	int		ArStopper;

	FArchive()
	:	ArStopper(0)
	,	ArVer(99999)			//?? something large
	,	ArLicenseeVer(0)
	{}

	virtual ~FArchive()
	{}

	virtual void Seek(int Pos) = 0;
	virtual bool IsEof() = 0;
	virtual void Serialize(void *data, int size) = 0;

	bool IsStopper()
	{
		return ArStopper == ArPos;
	}

	friend FArchive& operator<<(FArchive &Ar, char &B)
	{
		Ar.Serialize(&B, 1);
		return Ar;
	}
	friend FArchive& operator<<(FArchive &Ar, byte &B)
	{
		Ar.Serialize(&B, 1);
		return Ar;
	}
	friend FArchive& operator<<(FArchive &Ar, short &B)
	{
		Ar.Serialize(&B, 2);
		return Ar;
	}
	friend FArchive& operator<<(FArchive &Ar, word &B)
	{
		Ar.Serialize(&B, 2);
		return Ar;
	}
	friend FArchive& operator<<(FArchive &Ar, int &B)
	{
		Ar.Serialize(&B, 4);
		return Ar;
	}
	friend FArchive& operator<<(FArchive &Ar, unsigned &B)
	{
		Ar.Serialize(&B, 4);
		return Ar;
	}
	friend FArchive& operator<<(FArchive &Ar, float &B)
	{
		Ar.Serialize(&B, 4);
		return Ar;
	}

	virtual FArchive& operator<<(FName &N) = 0;
	virtual FArchive& operator<<(UObject *&Obj) = 0;

	// different game platforms autodetection
	//?? should change this, if will implement command line switch to force mode
#if SPLINTER_CELL
	bool IsSplinterCell()
	{
		return (ArVer == 100 && (ArLicenseeVer >= 0x09 && ArLicenseeVer <= 0x11)) ||
			   (ArVer == 102 && (ArLicenseeVer >= 0x14 && ArLicenseeVer <= 0x1C));
	}
#endif
};


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
	}

	FFileReader(const char *Filename, bool loading = true)
	:	f(fopen(Filename, loading ? "rb" : "wb"))
	{
		guard(FFileReader::FFileReader);
		if (!f)
			appError("Unable to open file %s", Filename);
		IsLoading = loading;
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
		fseek(f, Pos, SEEK_SET);
		ArPos = ftell(f);
		assert(Pos == ArPos);
	}

	virtual bool IsEof()
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

protected:
	FILE	*f;
	virtual void Serialize(void *data, int size)
	{
		int res;
		if (IsLoading)
			res = fread(data, size, 1, f);
		else
			res = fwrite(data, size, 1, f);
		ArPos += size;
		if (ArStopper > 0 && ArPos > ArStopper)
			appError("Serializing behind stopper");
		if (res != 1)
			appError("Unable to serialize data");
	}
};


void SerializeChars(FArchive &Ar, char *buf, int length);


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
		return Ar << V.X << V.Y << V.Z;
	}
};


struct FRotator
{
	int		Pitch, Yaw, Roll;

	friend FArchive& operator<<(FArchive &Ar, FRotator &R)
	{
		return Ar << R.Pitch << R.Yaw << R.Roll;
	}
};


struct FQuat
{
	float	X, Y, Z, W;

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

	int Num() const
	{
		return DataCount;
	}

protected:
	void	*DataPtr;
	int		DataCount;
	int		MaxCount;

	void Empty(int count, int elementSize);
	void Insert(int index, int count, int elementSize);
	void Remove(int index, int count, int elementSize);
	FArchive& Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize);
};

// NOTE: this container cannot hold objects, required constructor/destructor
// (at least, Add/Insert/Remove functions are not supported, but can serialize
// such data)
template<class T> class TArray : public FArray
{
public:
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
			((T*)DataPtr)->~T();
#endif
	}
	// data accessors
	T& operator[](int index)
	{
		assert(index >= 0 && index < DataCount);
		return *((T*)DataPtr + index);
	}
	const T& operator[](int index) const
	{
		assert(index >= 0 && index < DataCount);
		return *((T*)DataPtr + index);
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
			((T*)DataPtr)->~T();
#endif
		// remove items from array
		FArray::Remove(index, count, sizeof(T));
	}

	int AddItem(const T& item)
	{
		int index = Add();
		(*this)[index] = item;
		return index;
	}

	FORCEINLINE void Empty(int count = 0)
	{
		// destruct all array items
		for (int i = 0; i < DataCount; i++)
			((T*)DataPtr)->~T();
		// remove data array (count=0) or preallocate memory (count>0)
		FArray::Empty(count, sizeof(T));
	}

	// serializer
#if _MSC_VER == 1200			// VC6 bug
	friend FArchive& operator<<(FArchive &Ar, TArray &A);
#else
	template<class T2> friend FArchive& operator<<(FArchive &Ar, TArray<T2> &A);
#endif

protected:
	// serializer helper; used from 'operator<<(FArchive, TArray<>)' only
	static void SerializeItem(FArchive &Ar, void *item)
	{
		if (Ar.IsLoading)
			new (item) T;		// construct item before reading
		Ar << *(T*)item;		// serialize item
	}
};



// VC6 sometimes cannot instantiate this function, when declared inside
// template class
template<class T> FArchive& operator<<(FArchive &Ar, TArray<T> &A)
{
	if (Ar.IsLoading)
		A.Empty();				// erase previous data before loading
	return A.Serialize(Ar, TArray<T>::SerializeItem, sizeof(T));
}

// TLazyArray implemented as simple wrapper around TArray with
// different serialization function
template<class T> class TLazyArray : public TArray<T>
{
	friend FArchive& operator<<(FArchive &Ar, TLazyArray &A)
	{
		assert(Ar.IsLoading);
		if (Ar.ArVer > 61)
		{
			int SkipPos;		// ignored
			Ar << SkipPos;
		}
		return Ar << (TArray<T>&)A;
	}
};


/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

class FString : public TArray<char>
{
public:
	inline const char *operator*() const
	{
		return (char*)DataPtr;
	}
	inline operator const char*() const
	{
		return (char*)DataPtr;
	}
};


#endif // __UNCORE_H__
