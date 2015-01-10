#include "Core.h"
#include "UnCore.h"


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
	float timeDelta = (appMilliseconds() - ProfileStartTime) / 1000.0f;
	if (timeDelta < 0.01f && !GNumAllocs && !GSerializeBytes && !GNumSerialize)
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
		DataPtr  = NULL;
		MaxCount = 0;
	}
	DataCount = 0;
}

void FArray::Empty(int count, int elementSize)
{
	guard(FArray::Empty);

	if (IsStatic())
	{
		if (count > MaxCount) goto allocate; // "static" array becomes non-static
		DataCount = 0;
		memset(DataPtr, 0, count * elementSize);
		return;
	}

	if (DataPtr)
		appFree(DataPtr);

allocate:
	DataPtr   = NULL;
	DataCount = 0;
	MaxCount  = count;
	if (count)
	{
		DataPtr = appMalloc(count * elementSize);
		memset(DataPtr, 0, count * elementSize);
	}
	unguardf("%d x %d", count, elementSize);
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


void FArray::FastRemove(int index, int count, int elementSize)
{
	guard(FArray::FastRemove);
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
	guard(operator[]);
	assert(index >= 0 && index < DataCount);
	return OffsetPointer(DataPtr, index * elementSize);
	unguardf("%d/%d", index, DataCount);
}


/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

FString::FString(const char* src)
{
	if (!src)
	{
		Add(1);					// null char
	}
	else
	{
		int len = strlen(src) + 1;
		Add(len);
		memcpy(DataPtr, src, len);
	}
}


FString& FString::operator=(const char* src)
{
	if (src == DataPtr)
		return *this; // assigning to self

	Empty();
	if (!src)
	{
		Add(1);					// null char
	}
	else
	{
		int len = strlen(src) + 1;
		Add(len);
		memcpy(DataPtr, src, len);
	}
	return *this;
}


FString& FString::operator+=(const char* text)
{
	int len = strlen(text);
	int oldLen = Num();
	if (oldLen)
	{
		Add(len);
		// oldLen-1 -- cut null char, len+1 -- append null char
		memcpy(OffsetPointer(DataPtr, oldLen-1), text, len+1);
	}
	else
	{
		Add(len+1);	// reserve space for null char
		memcpy(DataPtr, text, len+1);
	}
	return *this;
}


char* FString::Detach()
{
	char* data;
	if (IsStatic())
	{
		data = appStrdup((char*)DataPtr);
		// clear string
		DataCount = 0;
		*(char*)DataPtr = 0;
		return data;
	}
	data = (char*)DataPtr;
	DataPtr   = NULL;
	DataCount = 0;
	MaxCount  = 0;
	return data;
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
