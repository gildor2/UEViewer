#include "Core.h"
#include "UnCore.h"


int  GForceGame           = GAME_UNKNOWN;
int  GForcePackageVersion = 0;
byte GForcePlatform       = PLATFORM_UNKNOWN;

#if THREADING
#include "Parallel.h"
#endif

#if PROFILE

uint32 GNumSerialize = 0;
uint32 GSerializeBytes = 0;
static int ProfileStartTime = -1;

void appResetProfiler()
{
	GNumAllocs = GNumSerialize = GSerializeBytes = 0;
	ProfileStartTime = appMilliseconds();
}


void appPrintProfiler(const char* label)
{
	if (ProfileStartTime == -1) return;
	float timeDelta = (appMilliseconds() - ProfileStartTime) / 1000.0f;
	if (timeDelta < 0.001f && !GNumAllocs && !GSerializeBytes && !GNumSerialize)
		return;		// nothing to print (perhaps already printed?)
	appPrintf("%s in %.1f sec, %d allocs, %.2f MBytes serialized in %d calls.\n",
		label ? label : "Loaded",
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

void FArray::MoveData(FArray& Other, int elementSize)
{
	if (!Other.IsStatic())
	{
		// Can simply reassign allocated data
		DataPtr = Other.DataPtr;
		DataCount = Other.DataCount;
		MaxCount = Other.MaxCount;
		Other.DataPtr = NULL;
		Other.DataCount = 0;
		Other.MaxCount = 0;
	}
	else
	{
		// Working with "static" array, should copy data instead
		int dataSize = Other.DataCount * elementSize;
		DataPtr = appMallocNoInit(dataSize);
		DataCount = Other.DataCount;
		MaxCount = Other.DataCount;
		memcpy(DataPtr, Other.DataPtr, dataSize);
		Other.DataCount = 0;
	}
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
	//!! (i.e. when array is 16 items, and calling Empty(15) - don't reallocate it, unless
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
		DataPtr = appMallocNoInit(count * elementSize);
	}

	unguardf("%d x %d", count, elementSize);
}

// This method will grow array's MaxCount. No items will be allocated.
// The allocated memory is not initialized because items could be inserted
// and removed at any time - so initialization should be performed in
// upper level functions like Insert()
void FArray::GrowArray(int count, int elementSize)
{
	//todo: remove appError's later (added 6.07.2020)
	if (count <= 0)
		appError("FArray::GrowArray failed: count = %d", count);

	// check for available space
	int newCount = DataCount + count;

	if (newCount <= MaxCount)
		return;

	// Not enough space, resize ...
	// Allow small initial size of array
	const int minCount = 4;
	if (newCount > minCount)
	{
		if (DataCount > 64 && count == 1)
		{
			// Array is large enough, and still growing - do the larger step
			int increment = DataCount / 8 + 16;
			MaxCount = Align(DataCount + increment, 16);
		}
		else
		{
			MaxCount = Align(DataCount + count, 16) + 16;
		}
	}
	else
	{
		MaxCount = minCount;
	}
	// Align memory block to reduce fragmentation
	int dataSize = Align(MaxCount * elementSize, 16);
	// Recompute MaxCount in a case if alignment increases its capacity
	MaxCount = dataSize / elementSize;
	// Reallocate memory
	if (!IsStatic())
	{
		DataPtr = appRealloc(DataPtr, dataSize);
	}
	else
	{
		// "static" array becomes non-static
		void* oldData = DataPtr; // this is a static pointer
		DataPtr = appMallocNoInit(dataSize);
		memcpy(DataPtr, oldData, DataCount * elementSize);
	}
}

void FArray::InsertUninitialized(int index, int count, int elementSize)
{
	guard(FArray::InsertUninitialized);

	if (!count) return;

	if (DataCount + count > MaxCount)
		GrowArray(count, elementSize);

	// move data
	if (index != DataCount)
	{
		assert(index >= 0 && index <= DataCount);
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

	if (!count) return;
	assert(count > 0);

	assert(index >= 0 && index + count <= DataCount);
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

	if (!count) return;
	assert(count > 0);

	assert(index >= 0 && index + count <= DataCount);
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
	if (!IsValidIndex(index))
		appError("TArray: index %d is out of range (%d)", index, DataCount);
	return OffsetPointer(DataPtr, index * elementSize);
}


/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

FString::FString(const char* src)
{
	int len = src ? strlen(src) + 1 : 1;
	if (len > 1) // not a empty string
	{
		Data.AddUninitialized(len);
		memcpy(Data.GetData(), src, len);
	}
}

FString::FString(int count, const char* src)
{
	// A short version of AppendChars()
	if (count)
	{
		Data.AddUninitialized(count + 1);
		char* memory = Data.GetData();
		memcpy(memory, src, count);
		memory[count] = 0;
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
	int len = src ? strlen(src) + 1 : 1;
	if (len > 1) // not a empty string
	{
		Data.AddUninitialized(len);
		memcpy(Data.GetData(), src, len);
	}
	return *this;
}

FString& FString::operator+=(const char* text)
{
	int len = strlen(text);
	return AppendChars(text, len);
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

FString& FString::AppendChars(const char* s, int count)
{
	// Get the size of string, including null character
	int oldLen = Data.Num();
	if (oldLen)
	{
		// Already has something
		Data.AddUninitialized(count);
		char* memory = Data.GetData();
		// oldLen-1 -- cut existing null char
		memcpy(memory + oldLen - 1, s, count);
		memory[oldLen - 1 + count] = 0;
	}
	else
	{
		// Empty string
		Data.AddUninitialized(count+1);	// reserve space for null char
		char* memory = Data.GetData();
		memcpy(memory, s, count);
		memory[count] = 0;
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

bool FString::StartsWith(const char* Text) const
{
	if (!Text || !Text[0] || IsEmpty()) return false;
	return (strncmp(Data.GetData(), Text, strlen(Text)) == 0);
}

bool FString::EndsWith(const char* Text) const
{
	if (!Text || !Text[0] || IsEmpty()) return false;
	int len = strlen(Text);
	if (len > Data.Num()) return false;
	return (strncmp(Data.GetData() + Data.Num() - 1 - len, Text, len) == 0);
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

FString FString::TrimStart() const
{
	FString Copy(*this);
	Copy.TrimStartInline();
	return Copy;
}

FString FString::TrimEnd() const
{
	FString Copy(*this);
	Copy.TrimEndInline();
	return Copy;
}

FString FString::TrimStartAndEnd() const
{
	FString Copy(*this);
	Copy.TrimStartAndEndInline();
	return Copy;
}

void FString::TrimStartInline()
{
	const char* s = Data.GetData();
	int pos;
	for (pos = 0; pos < Len(); pos++, s++)
	{
		if (!isspace(*s))
			break;
	}
	if (pos > 0)
	{
		Data.RemoveAt(0, pos);
	}
}

void FString::TrimEndInline()
{
	int endPos = Len() - 1;
	const char* s = Data.GetData() + endPos;
	int pos;
	for (pos = endPos; pos >= 0; pos--, s--)
	{
		if (!isspace(*s))
			break;
	}
	if (pos < endPos)
	{
		Data.RemoveAt(pos + 1, endPos - pos);
	}
}

void FString::TrimStartAndEndInline()
{
	TrimStartInline();
	TrimEndInline();
}


/*-----------------------------------------------------------------------------
	FName (string) pool
-----------------------------------------------------------------------------*/

#define STRING_HASH_SIZE		(65536*4)		// 1Mb of 32-bit pointers

struct CStringPoolEntry
{
	CStringPoolEntry*	HashNext;
	uint16				Length;
	char				Str[1];
};

static CStringPoolEntry* StringHashTable[STRING_HASH_SIZE];
static CMemoryChain* StringPool;

#if THREADING
static CMutex GStrdupPoolMutex;
#endif

const char* appStrdupPool(const char* str)
{
	int len = strlen(str);
#if 0
	unsigned int hash = 0;
	for (int i = 0; i < len; i++)
	{
		char c = str[i];
		hash = ROL32(hash, 1) + c;
	}
#else
	// The FNV Non-Cryptographic Hash Algorithm
	// https://tools.ietf.org/html/draft-eastlake-fnv-16
	// It produces much better has collision distribution and smaller
	// peak collision chain lengths.
	#define FNV32prime 0x01000193
	#define FNV32basis 0x811C9DC5
	unsigned int hash = FNV32basis;
	for (const char* s = str, *e = str + len; s < e; s++)
	{
		hash = FNV32prime * (hash ^ *s);
	}

#endif
	hash &= (STRING_HASH_SIZE - 1);

#if THREADING
	// Make appStrdupPool thread-safe
	CMutex::ScopedLock Lock(GStrdupPoolMutex);
#endif

	// Find existing string in a pool
	CStringPoolEntry** prevPoint = &StringHashTable[hash];
	while (true)
	{
		CStringPoolEntry* current = *prevPoint;
		// Keep items sorted by string length - it is almost free, but will
		// allow faster rejection during search.
		if (!current || current->Length > len) break;
		if (current->Length == len && !memcmp(str, current->Str, len))
		{
			// Found a string
			return current->Str;
		}
		prevPoint = &current->HashNext;
	}

	if (!StringPool) StringPool = new CMemoryChain();

	// Allocate new string from pool
	CStringPoolEntry* n = (CStringPoolEntry*)StringPool->Alloc(sizeof(CStringPoolEntry) + len);	// note: null byte is taken into account in CStringPoolEntry
	n->Length = len;
	memcpy(n->Str, str, len+1);
	// Insert into the hash collision chain
	n->HashNext = *prevPoint;
	*prevPoint = n;

	return n->Str;
}

#if 0
void PrintStringHashDistribution()
{
	int totalCount = 0;
	uint32 memoryUsed = sizeof(StringHashTable);
	int hashCounts[1024];
	int* bucketSizes = new int[STRING_HASH_SIZE];
	memset(hashCounts, 0, sizeof(hashCounts));
	memset(bucketSizes, 0, sizeof(bucketSizes));

	// Collect statistics
	for (int hash = 0; hash < STRING_HASH_SIZE; hash++)
	{
		int count = 0;
		for (CStringPoolEntry* info = StringHashTable[hash]; info; info = info->HashNext)
		{
			count++;
			memoryUsed += sizeof(CStringPoolEntry) + info->Length;
		}
		assert(count < ARRAY_COUNT(hashCounts));
		hashCounts[count]++;
		bucketSizes[hash] = count;
		totalCount += count;
	}

	// Print statistics
	FILE* f = fopen("StringTableInfo.txt", "w");
	fprintf(f, "%d strings, %.2f Mb used\n", totalCount, memoryUsed / (1024.0f * 1024.0f));
	fprintf(f, "String hash distribution: collision count -> num chains\n");
	int totalCount2 = 0;
	for (int i = 0; i < ARRAY_COUNT(hashCounts); i++)
	{
		int count = hashCounts[i];
		if (count > 0)
		{
			totalCount2 += count * i;
			float percent = totalCount2 * 100.0f / totalCount;
			fprintf(f, "%d -> %d [%.1f%%]\n", i, count, percent);
		}
	}
	fclose(f);
	assert(totalCount == totalCount2);

	// Store string table to a file
	f = fopen("StringTable.txt", "w");
	// Sort by bucket sizes
	for (int i = ARRAY_COUNT(hashCounts) - 1; i > 0; i--)
	{
		if (hashCounts[i] == 0) continue; // no buckets of this size

		for (int hash = 0; hash < STRING_HASH_SIZE; hash++)
		{
			if (bucketSizes[hash] != i) continue;
			fprintf(f, "# bucket %d items\n", i);
			TStaticArray<const CStringPoolEntry*, 1024> Entries;
			for (CStringPoolEntry* info = StringHashTable[hash]; info; info = info->HashNext)
			{
				Entries.Add(info);
			}
			Entries.Sort([](const CStringPoolEntry* const& A, const CStringPoolEntry* const& B) -> int
				{
					int Diff = A->Length - B->Length;
					if (Diff) return Diff; // note: already have items sorted by string length
					return stricmp(A->Str, B->Str);
				});
			for (const CStringPoolEntry* info : Entries)
			{
				fprintf(f, "%s\n", info->Str);
			}
			fprintf(f, "\n");
		}
	}
	fclose(f);

	// Cleanup
	delete[] bucketSizes;
}
#endif


/*-----------------------------------------------------------------------------
	Other
-----------------------------------------------------------------------------*/

/*
 * Half = Float16
 * http://www.openexr.com/  source: ilmbase-*.tar.gz/Half/toFloat.cpp
 * http://en.wikipedia.org/wiki/Half_precision
 * Also look at GL_ARB_half_float_pixel
 */
float half2float(uint16 h)
{
	union
	{
		float		f;
		unsigned	df;
	} f;

	int sign = (h >> 15) & 0x00000001;
	int exp  = (h >> 10) & 0x0000001F;
	int mant =  h        & 0x000003FF;

	exp  = exp + (127 - 15);
	f.df = (sign << 31) | (exp << 23) | (mant << 13);
	return f.f;
}
