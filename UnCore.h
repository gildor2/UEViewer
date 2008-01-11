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
#ifdef offsetof
#	define FIELD2OFS(struc, field)		(offsetof(struc, field))				// more compatible
#else
#	define FIELD2OFS(struc, field)		((unsigned) &((struc *)NULL)->field)	// just in case
#endif
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
	int		ArPos;
	int		ArStopper;

	FArchive()
	:	ArStopper(0)
	,	ArVer(9999)			//?? something large
	{}

	virtual ~FArchive()
	{}

	virtual void Seek(int Pos) = NULL;
	virtual bool IsEof() = NULL;
	virtual void Serialize(void *data, int size) = NULL;

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

	virtual FArchive& operator<<(FName &N) = NULL;
	virtual FArchive& operator<<(UObject *&Obj) = NULL;
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

	FFileReader(const char *Filename)
	:	f(fopen(Filename, "rb"))
	{
		guard(FFileReader::FFileReader);
		if (!f)
			appError("Unable to open file %s", Filename);
		IsLoading = true;
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
	FColor(byte r, byte g, byte b)
	:	R(r), G(g), B(b), A(255)
	{}
	FColor(byte r, byte g, byte b, byte a)
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
		Empty();
	}

	int Num() const
	{
		return DataCount;
	}

	void Empty()
	{
		if (DataPtr)
			delete[] DataPtr;
		DataPtr   = NULL;
		DataCount = 0;
		MaxCount  = 0;
	}

protected:
	void	*DataPtr;
	int		DataCount;
	int		MaxCount;

	void Insert(int index, int count, int elementSize)
	{
		assert(index >= 0);
		assert(index <= DataCount);
		assert(count > 0);
		// check for available space
		if (DataCount + count > MaxCount)
		{
			// not enough space, resize ...
			int prevCount = MaxCount;
			MaxCount = ((DataCount + count + 7) / 8) * 8 + 8;
			DataPtr = realloc(DataPtr, MaxCount * elementSize);
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
	}

	void Remove(int index, int count, int elementSize)
	{
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
	}
};

// NOTE: this container cannot hold objects, required constructor/destructor
// (at least, Add/Insert/Remove functions are not supported, but can serialize
// such data)
template<class T> class TArray : public FArray
{
public:
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
		return index;
	}

	void Insert(int index, int count = 1)
	{
		FArray::Insert(index, count, sizeof(T));
	}

	void Remove(int index, int count = 1)
	{
		FArray::Remove(index, count, sizeof(T));
	}

	int AddItem(const T& item)
	{
		int index = Add();
		(*this)[index] = item;
		return index;
	}

	// serializer
	friend FArchive& operator<<(FArchive &Ar, TArray &A)
	{
		guard(TArray<<);
		assert(Ar.IsLoading);	//?? saving requires more code
		A.Empty();
		int Count;
		Ar << AR_INDEX(Count);
		T* Ptr = new T[Count];
		A.DataPtr   = Ptr;
		A.DataCount = Count;
		A.MaxCount  = Count;
		for (int i = 0; i < Count; i++)
			Ar << *Ptr++;
		return Ar;
		unguard;
	}
};

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


#endif // __UNCORE_H__
