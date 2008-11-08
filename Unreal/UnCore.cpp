#include "Core.h"
#include "UnCore.h"


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


FArchive& FArray::Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	guard(TArray::Serialize);

//-- if (Ar.IsLoading) Empty();	-- cleanup is done in TArray serializer (do not need
//								-- to pass array eraser/destructor to this function)
	// Here:
	// 1) when loading: 'this' array is empty (cleared from TArray's operator<<)
	// 2) when saving : data is not modified by this function

	// serialize data count
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

/*-----------------------------------------------------------------------------
	FArchive methods
-----------------------------------------------------------------------------*/

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
	if (Ar.IsLoading)
	{
		byte b;
		Ar << b;
		int sign  = b & 0x80;
		int shift = 6;
		int r     = b & 0x3F;
		if (b & 0x40)
		{
			do
			{
				Ar << b;
				r |= (b & 0x7F) << shift;
				shift += 7;
			} while (b & 0x80);
		}
		I.Value = sign ? -r : r;
	}
	else
	{
		appError("write AR_INDEX is not implemented");
	}
	return Ar;
}


void SerializeChars(FArchive &Ar, char *buf, int length)
{
	for (int i = 0; i < length; i++)
		Ar << *buf++;
}


/*-----------------------------------------------------------------------------
	Dummy archive class
-----------------------------------------------------------------------------*/

class CDummyArchive : public FArchive
{
public:
	virtual void Seek(int Pos)
	{}
	virtual bool IsEof()
	{
		return true;
	}
	virtual void Serialize(void *data, int size)
	{}
	virtual FArchive& operator<<(FName &N)
	{
		return *this;
	}
	virtual FArchive& operator<<(UObject *&Obj)
	{
		return *this;
	}
};


static CDummyArchive DummyArchive;
FArchive *GDummySave = &DummyArchive;
