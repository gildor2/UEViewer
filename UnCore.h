#ifndef __UNCORE_H__
#define __UNCORE_H__


typedef unsigned char		byte;
typedef unsigned short		word;

class FArchive;
class UObject;
class UnPackage;


/*-----------------------------------------------------------------------------
	FName class
-----------------------------------------------------------------------------*/

class FName
{
public:
	int			Index;
	const char	*Str;

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
		if (!f)
			appError("Unable to open file %s", Filename);
		IsLoading = true;
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
			appError("Serailizing behind stopper");
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

	friend FArchive& operator<<(FArchive &Ar, FVector &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}
};


struct FRotator
{
	int		Yaw, Pitch, Roll;

	friend FArchive& operator<<(FArchive &Ar, FRotator &R)
	{
		return Ar << R.Yaw << R.Pitch << R.Roll;
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


/*-----------------------------------------------------------------------------
	TArray/TLazyArray templates
-----------------------------------------------------------------------------*/

template<class T> class TArray
{
public:
	TArray()
	:	DataCount(0)
	,	DataPtr(NULL)
	{}
	~TArray()
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
	}

	T& operator[](int index)
	{
		assert(index >= 0 && index < DataCount);
		return DataPtr[index];
	}
	const T& operator[](int index) const
	{
		assert(index >= 0 && index < DataCount);
		return DataPtr[index];
	}

	friend FArchive& operator<<(FArchive &Ar, TArray &A)
	{
		assert(Ar.IsLoading);	//?? savig requires more code
		A.Empty();
		int Count;
		Ar << AR_INDEX(Count);
		T* Ptr = new T[Count];
		A.DataPtr   = Ptr;
		A.DataCount = Count;
		for (int i = 0; i < Count; i++)
			Ar << *Ptr++;
		return Ar;
	}

protected:
	T		*DataPtr;
	int		DataCount;
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


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

class UObject
{
public:
	// internal storage
	UnPackage	*Package;
	int			PackageIndex;	// index in package export table
	const char	*Name;

//	unsigned	ObjectFlags;

	virtual ~UObject()
	{}

	virtual const char *GetClassName()
	{
		return "Object";
	}

	virtual void Serialize(FArchive &Ar)
	{
		// stack frame
//		assert(!(ObjectFlags & RF_HasStack));
		// property list
		int tmp;				// really, FName
		Ar << AR_INDEX(tmp);
//		assert(tmp == 0);		// index of "None"
	}
};


/*-----------------------------------------------------------------------------
	Simple RTTI
-----------------------------------------------------------------------------*/

// NOTE: DECLARE_CLASS and REGISTER_CLASS will skip 1st class name char

#define DECLARE_CLASS(Class)					\
	public:										\
		typedef Class ThisClass;				\
		static ThisClass* StaticConstructor()	\
		{ return new ThisClass; }				\
		virtual const char* GetClassName()		\
		{ return #Class+1; }					\
	private:									\
		friend FArchive& operator<<(FArchive &Ar, Class *&Res) \
		{										\
			return Ar << *(UObject**)&Res;		\
		}


struct CClassInfo
{
	const char *Name;
	UObject* (*Constructor)();
};


void RegisterClasses(CClassInfo *Table, int Count);
UObject *CreateClass(const char *Name);


#define BEGIN_CLASS_TABLE						\
	{											\
		static CClassInfo Table[] = {
#define REGISTER_CLASS(Class)					\
			{ #Class+1, (UObject* (*) ())Class::StaticConstructor },
#define END_CLASS_TABLE							\
		};										\
		RegisterClasses(ARRAY_ARG(Table));		\
	}


#endif // __UNCORE_H__
