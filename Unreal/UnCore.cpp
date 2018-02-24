#include "Core.h"
#include "UnCore.h"


int  GForceGame           = GAME_UNKNOWN;
int  GForcePackageVersion = 0;
byte GForcePlatform       = PLATFORM_UNKNOWN;


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
	float timeDelta = (appMilliseconds() - ProfileStartTime) / 1000.0f;
	if (timeDelta < 0.001f && !GNumAllocs && !GSerializeBytes && !GNumSerialize)
		return;		// perhaps already printed?
	appPrintf("Loaded in %.2g sec, %d allocs, %.2f MBytes serialized in %d calls.\n",
		timeDelta, GNumAllocs, GSerializeBytes / (1024.0f * 1024.0f), GNumSerialize);
	appResetProfiler();
}

#endif // PROFILE


/*-----------------------------------------------------------------------------
	FArray
-----------------------------------------------------------------------------*/

FArray::~FArray()
{
	if (!IsStatic())
	{
		if (DataPtr)
			appFree(DataPtr);
	}
	DataCount = 0;
	DataPtr   = NULL;
	MaxCount  = 0;
}

void FArray::Empty(int count, int elementSize)
{
	guard(FArray::Empty);

	DataCount = 0;

	if (IsStatic())
	{
		if (count <= MaxCount)
			return;
		// "static" array becomes non-static, invalidate data pointer
		DataPtr = NULL;
	}

	//!! TODO: perhaps round up 'Max' to 16 bytes, allow comparison below to be 'softer'
	//!! (i.e. when array is 16 items, and calling Empty(15) - don't reallicate it, unless
	//!! item size is large
	if (DataPtr)
	{
		// check if we need to release old array
		if (count == MaxCount)
		{
			// the size was not changed
			return;
		}
		// delete old memory block
		appFree(DataPtr);
		DataPtr = NULL;
	}

	MaxCount = count;

	if (count)
	{
		DataPtr = appMalloc(count * elementSize);
	}

	unguardf("%d x %d", count, elementSize);
}

// This method will grow array's MaxCount. No items will be allocated.
// The allocated memory is not initialized because items could be inserted
// and removed at any time - so initialization should be performed in
// upper level functions like Insert()
void FArray::GrowArray(int count, int elementSize)
{
	guard(FArray::GrowArray);
	assert(count > 0);

	int prevCount = MaxCount;

	// check for available space
	if (DataCount + count > MaxCount)
	{
		// not enough space, resize ...
		MaxCount = ((DataCount + count + 15) / 16) * 16 + 16;
		if (!IsStatic())
		{
			DataPtr = appRealloc(DataPtr, MaxCount * elementSize);
		}
		else
		{
			// "static" array becomes non-static
			void* oldData = DataPtr; // this is a static pointer
			DataPtr = appMalloc(MaxCount * elementSize);
			memcpy(DataPtr, oldData, prevCount * elementSize);
		}
	}

	unguardf("%d x %d", count, elementSize);
}

void FArray::InsertUninitialized(int index, int count, int elementSize)
{
	guard(FArray::InsertUninitialized);
	assert(index >= 0);
	assert(index <= DataCount);
	assert(count >= 0);
	if (!count) return;
	GrowArray(count, elementSize);
	// move data
	if (index != DataCount)
	{
		memmove(
			(byte*)DataPtr + (index + count)     * elementSize,
			(byte*)DataPtr + index               * elementSize,
							 (DataCount - index) * elementSize
		);
	}
#if DEBUG_MEMORY
	// fill memory with some pattern for debugging
	memset((byte*)DataPtr + index * elementSize, 0xCC, count * elementSize);
#endif
	// last operation: advance counter
	DataCount += count;
	unguard;
}

void FArray::InsertZeroed(int index, int count, int elementSize)
{
	guard(FArray::InsertZeroed);
	if (!count) return;
	InsertUninitialized(index, count, elementSize);
	// zero memory which was inserted
	memset((byte*)DataPtr + index * elementSize, 0, count * elementSize);
	unguard;
}


void FArray::Remove(int index, int count, int elementSize)
{
	guard(FArray::Remove);
	assert(index >= 0);
	assert(count > 0);
	assert(index + count <= DataCount);
	// move data
	if (index + count < DataCount)
	{
		memmove(
			(byte*)DataPtr + index                       * elementSize,
			(byte*)DataPtr + (index + count)             * elementSize,	// all next items
							 (DataCount - index - count) * elementSize
		);
	}
	// decrease counter
	DataCount -= count;
	unguard;
}


void FArray::RemoveAtSwap(int index, int count, int elementSize)
{
	guard(FArray::RemoveAtSwap);
	assert(index >= 0);
	assert(count > 0);
	assert(index + count <= DataCount);
	// move data
	if (index + count < DataCount)
	{
		memmove(
			(byte*)DataPtr + index               * elementSize,
			(byte*)DataPtr + (DataCount - count) * elementSize,	// 'count' items from the end of array
							 count               * elementSize
		);
	}
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
	if (!IsValidIndex(index)) appError("TArray: index %d is out of range (%d)", index, DataCount);
	return OffsetPointer(DataPtr, index * elementSize);
}


/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

FString::FString(const char* src)
{
	if (!src)
	{
		Data.AddZeroed(1);		// null char
	}
	else
	{
		int len = strlen(src) + 1;
		Data.AddUninitialized(len);
		memcpy(Data.GetData(), src, len);
	}
}

FString::FString(const FString& Other)
{
	CopyArray(Data, Other.Data);
}

FString& FString::operator=(const FString& src)
{
	CopyArray(Data, src.Data);
	return *this;
}

FString& FString::operator=(const char* src)
{
	if (src == Data.GetData())
		return *this; // assigning to self

	Empty();
	if (!src)
	{
		Data.AddZeroed(1);		// null char
	}
	else
	{
		int len = strlen(src) + 1;
		Data.AddUninitialized(len);
		memcpy(Data.GetData(), src, len);
	}
	return *this;
}

FString& FString::operator+=(const char* text)
{
	int len = strlen(text);
	int oldLen = Data.Num();
	if (oldLen)
	{
		Data.AddUninitialized(len);
		// oldLen-1 -- cut null char, len+1 -- append null char
		memcpy(OffsetPointer(Data.GetData(), oldLen-1), text, len+1);
	}
	else
	{
		Data.AddUninitialized(len+1);	// reserve space for null char
		memcpy(Data.GetData(), text, len+1);
	}
	return *this;
}

FString& FString::AppendChar(char ch)
{
	if (Data.Num() == 0)
	{
		// empty array - should add a char and null byte
		Data.AddZeroed(2);
		Data[0] = ch;
	}
	else
	{
		// not a empty string, should add one null
		int nullCharIndex = Data.Num() - 1;
		Data[nullCharIndex] = ch;
		Data.AddZeroed(1);
	}
	return *this;
}

char* FString::Detach()
{
	char* RetData;
	if (Data.IsStatic())
	{
		RetData = appStrdup((char*)Data.DataPtr);
		// clear string
		Data.DataCount = 0;
		*(char*)Data.DataPtr = 0;
	}
	else
	{
		RetData = (char*)Data.DataPtr;
		Data.DataPtr   = NULL;
		Data.DataCount = 0;
		Data.MaxCount  = 0;
	}
	return RetData;
}

bool FString::StartsWith(const char* Text)
{
	if (!Text || !Text[0] || IsEmpty()) return false;
	return (strncmp(Data.GetData(), Text, strlen(Text)) == 0);
}

bool FString::EndsWith(const char* Text)
{
	if (!Text || !Text[0] || IsEmpty()) return false;
	int len = strlen(Text);
	if (len > Data.Num()) return false;
	return (strncmp(Data.GetData() + Data.Num() - len, Text, len) == 0);
}

bool FString::RemoveFromStart(const char* Text)
{
	if (StartsWith(Text))
	{
		RemoveAt(0, strlen(Text));
		return true;
	}
	return false;
}

bool FString::RemoveFromEnd(const char* Text)
{
	if (EndsWith(Text))
	{
		int len = strlen(Text);
		RemoveAt(Len() - len, len);
		return true;
	}
	return false;
}


/*-----------------------------------------------------------------------------
	FName (string) pool
-----------------------------------------------------------------------------*/

#define STRING_HASH_SIZE		16384

struct CStringPoolEntry
{
	CStringPoolEntry*	HashNext;
	int					Length;
	char				Str[1];
};

static CStringPoolEntry* StringHashTable[STRING_HASH_SIZE];
static CMemoryChain* StringPool;

const char* appStrdupPool(const char* str)
{
	int len = strlen(str);
	int hash = 0;
	for (int i = 0; i < len; i++)
	{
		char c = str[i];
#if 0
		hash = (hash + c) ^ 0xABCDEF;
#else
		hash = ROL16(hash, 5) - hash + ((c << 4) + c ^ 0x13F);	// some crazy hash function
#endif
	}
	hash &= (STRING_HASH_SIZE - 1);

	for (const CStringPoolEntry* s = StringHashTable[hash]; s; s = s->HashNext)
	{
		if (s->Length == len && !strcmp(str, s->Str))		// found a string
			return s->Str;
	}

	if (!StringPool) StringPool = new CMemoryChain();

	// allocate new string from pool
	CStringPoolEntry* n = (CStringPoolEntry*)StringPool->Alloc(sizeof(CStringPoolEntry) + len);	// note: null byte is taken into account in CStringPoolEntry
	n->HashNext = StringHashTable[hash];
	StringHashTable[hash] = n;
	n->Length = len;
	memcpy(n->Str, str, len+1);

	return n->Str;
}

#if 0
void PrintStringHashDistribution()
{
	int hashCounts[1024];
	memset(hashCounts, 0, sizeof(hashCounts));
	for (int hash = 0; hash < STRING_HASH_SIZE; hash++)
	{
		int count = 0;
		for (CStringPoolEntry* item = StringHashTable[hash]; item; item = item->HashNext)
			count++;
		assert(count < ARRAY_COUNT(hashCounts));
		hashCounts[count]++;
	}
	appPrintf("String hash distribution:\n");
	for (int i = 0; i < ARRAY_COUNT(hashCounts); i++)
		if (hashCounts[i] > 0)
			appPrintf("%d -> %d\n", i, hashCounts[i]);
}
#endif
