typedef unsigned char		byte;
typedef unsigned short		word;


//-----------------------------------------------------------------------------
//	FArchive class
//-----------------------------------------------------------------------------

class FArchive
{
public:
	bool	IsLoading;
	int		ArVer;

	FArchive()
	:	f(NULL)
	{}

	FArchive(FILE *InFile)
	:	f(InFile)
	,	IsLoading(true)		//?? no writting now
	,	ArVer(9999)			//?? something large
	{}

	void Setup(FILE *InFile, bool Loading)
	{
		f         = InFile;
		IsLoading = Loading;
	}

	int GetPos()
	{
		return ftell(f);
	}

	bool IsEof()
	{
		int pos  = ftell(f); fseek(f, 0, SEEK_END);
		int size = ftell(f); fseek(f, pos, SEEK_SET);
		return size == pos;
	}

protected:
	FILE	*f;
	void Serialize(void *data, int size)
	{
		int res;
		if (IsLoading)
			res = fread(data, size, 1, f);
		else
			res = fwrite(data, size, 1, f);
		assert(res == 1);
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
};


//-----------------------------------------------------------------------------
//	FCompactIndex class for serializing objects in a compactly, mapping small values
//	to fewer bytes.
//-----------------------------------------------------------------------------

class FCompactIndex
{
public:
	int		Value;
	friend FArchive& operator<<(FArchive &Ar, FCompactIndex &I);
};

#define AR_INDEX(intref)	(*(FCompactIndex*)&(intref))


//-----------------------------------------------------------------------------
//	FName class
//-----------------------------------------------------------------------------

class FName
{
public:
	int		Index;

	friend FArchive& operator<<(FArchive &Ar, FName &Name)
	{
		return Ar << AR_INDEX(Name);
	}
};


//-----------------------------------------------------------------------------
//	Math classes
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	TArray/TLazyArray templates
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	UObject class
//-----------------------------------------------------------------------------

class UObject
{
public:
//	FName	Name;
//	uint32	ObjectFlags;
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

inline FArchive& operator<<(FArchive &Ar, UObject* &Obj)
{
	int index;
	Ar << AR_INDEX(index);
	printf("Object: %d\n", index);	//!! implement, use imports table
	return Ar;
}


#define DECLARE_CLASS(TClass) \
	friend FArchive& operator<<(FArchive &Ar, TClass *&Res) \
	{ \
		return Ar << *(UObject**)&Res; \
	}
