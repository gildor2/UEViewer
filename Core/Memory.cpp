#include "Core.h"

#if DEBUG_MEMORY
#define MAX_STACK_TRACE			16
#define MAX_ALLOCATION_POINTS	8192
#endif // DEBUG_MEMORY


#if PROFILE
int GNumAllocs = 0;
#endif

size_t GTotalAllocationSize = 0;
int    GTotalAllocationCount = 0;

#define BLOCK_MAGIC				0xAE
#define FREE_BLOCK				0xFE

#define MAX_ALLOCATION_SIZE		(257<<20)		// upper limit for single allocation is 256+1 Mb

#if DEBUG_MEMORY

struct CStackTrace
{
	int				hash;
	address_t		stack[MAX_STACK_TRACE];

	CStackTrace()
	{
		memset(this, 0, sizeof(*this));
	}
	void UpdateHash()
	{
		int h = 0;
		for (int i = 0; i < MAX_STACK_TRACE; i++)
		{
			h += stack[i];
			h ^= 0x56789ABC;
		}
		hash = h;
	}

	bool operator==(const CStackTrace& other) const
	{
		if (hash != other.hash) return false;
		if (memcmp(stack, other.stack, sizeof(stack)) != 0) return false;
		return true;
	}

	void Dump() const
	{
		appDumpStackTrace(stack, MAX_STACK_TRACE);
	}
};

static CStackTrace GAllocationPoints[MAX_ALLOCATION_POINTS];
static int GNumAllocationPoints = 0;

#endif // DEBUG_MEMORY


struct CBlockHeader
{
	byte			magic;
	byte			offset;
	byte			align;
	int				blockSize;

#if DEBUG_MEMORY
	CBlockHeader*	prev;
	CBlockHeader*	next;
	CStackTrace*	stack;

	static CBlockHeader* first;

	inline void Link()
	{
		if (first) first->prev = this;
		next = first;
		prev = NULL;
		first = this;
	}
	inline void Unlink()
	{
		if (first == this) first = next;
		if (prev) prev->next = next;
		if (next) next->prev = prev;
	}
#endif // DEBUG_MEMORY
};

#if DEBUG_MEMORY
CBlockHeader* CBlockHeader::first = NULL;
#endif


/*-----------------------------------------------------------------------------
	Primary allocation functions
-----------------------------------------------------------------------------*/

void *appMalloc(int size, int alignment)
{
	guard(appMalloc);
	if (size < 0 || size >= MAX_ALLOCATION_SIZE)
		appError("Memory: bad allocation size %d bytes", size);
	assert(alignment > 1 && alignment <= 256 && ((alignment & (alignment - 1)) == 0));
	void *block = malloc(size + sizeof(CBlockHeader) + (alignment - 1));
	if (!block)
		appError("Out of memory: failed to allocate %d bytes", size);
	void *ptr = Align(OffsetPointer(block, sizeof(CBlockHeader)), alignment);
	if (size > 0)
		memset(ptr, 0, size);
	CBlockHeader *hdr = (CBlockHeader*)ptr - 1;
	byte offset = (byte*)ptr - (byte*)block;
	hdr->magic     = BLOCK_MAGIC;
	hdr->offset    = offset - 1;
	hdr->align     = alignment - 1;
	hdr->blockSize = size;

#if DEBUG_MEMORY
	hdr->Link();
	// collect a stack trace
	CStackTrace stack;
	appCaptureStackTrace(stack.stack, MAX_STACK_TRACE, 2);
	stack.UpdateHash();
	// find similar call stack
	CStackTrace* found = NULL;
	for (int i = 0; i < GNumAllocationPoints; i++)
	{
		if (stack == GAllocationPoints[i])
		{
			found = &GAllocationPoints[i];
			break;
		}
	}
	if (!found)
	{
		assert(GNumAllocationPoints < MAX_ALLOCATION_POINTS);
		found = &GAllocationPoints[GNumAllocationPoints++];
		*found = stack;
	}
	hdr->stack = found;
#endif // DEBUG_MEMORY

	// statistics
	GTotalAllocationSize += size;
	GTotalAllocationCount++;
#if PROFILE
	GNumAllocs++;
#endif

	return ptr;
	unguardf("size=%d (total=%d Mbytes)", size, (int)(GTotalAllocationSize >> 20));
}

void* appRealloc(void *ptr, int newSize)
{
	guard(appRealloc);

	// special case
	if (!ptr) return appMalloc(newSize);

	CBlockHeader *hdr = (CBlockHeader*)ptr - 1;

	int oldSize = hdr->blockSize;
	if (oldSize == newSize) return ptr;	// size not changed

	assert(hdr->magic == BLOCK_MAGIC);
	hdr->magic--;		// modify to any value
#if DEBUG_MEMORY
	hdr->Unlink();
#endif

	int alignment = hdr->align + 1;
	void *newData = appMalloc(newSize, alignment);

	memcpy(newData, ptr, min(newSize, oldSize));

	int offset = hdr->offset + 1;
	void *block = OffsetPointer(ptr, -offset);

#if DEBUG_MEMORY
	memset(ptr, FREE_BLOCK, oldSize);
#endif
	free(block);

	// statistics: we're allocating a new block with appMalloc, which counts statistics
	// for this allocation, so only eliminate statistics from old memory block here
	GTotalAllocationSize -= oldSize;
	GTotalAllocationCount--;

#if PROFILE
	GNumAllocs++;
#endif

	return newData;

	unguard;
}

void appFree(void *ptr)
{
	guard(appFree);
	assert(ptr);
	CBlockHeader *hdr = (CBlockHeader*)ptr - 1;

	int offset = hdr->offset + 1;
	void *block = OffsetPointer(ptr, -offset);

	assert(hdr->magic == BLOCK_MAGIC);
	hdr->magic--;		// modify to any value
#if DEBUG_MEMORY
	hdr->Unlink();
	memset(ptr, FREE_BLOCK, hdr->blockSize);
#endif

	// statistics
	GTotalAllocationSize -= hdr->blockSize;
	GTotalAllocationCount--;

	free(block);

	unguard;
}


/*-----------------------------------------------------------------------------
	CMemoryChain
-----------------------------------------------------------------------------*/

void* CMemoryChain::operator new(size_t size, int dataSize)
{
	guard(CMemoryChain::new);
	int alloc = Align(size + dataSize, MEM_CHUNK_SIZE);
	CMemoryChain *chain = (CMemoryChain *) appMalloc(alloc);	//!! allocate
	if (!chain)
		appError("Failed to allocate %d bytes", alloc);
	chain->size = alloc;
	chain->next = NULL;
	chain->data = (byte*) OffsetPointer(chain, size);
	chain->end  = (byte*) OffsetPointer(chain, alloc);

	memset(chain->data, 0, chain->end - chain->data);

	return chain;
	unguard;
}


void CMemoryChain::operator delete(void *ptr)
{
	guard(CMemoryChain::delete);
	CMemoryChain *curr, *next;
	for (curr = (CMemoryChain *)ptr; curr; curr = next)
	{
		// free memory block
		next = curr->next;
		free(curr);				//!! deallocate
	}
	unguard;
}


void *CMemoryChain::Alloc(size_t size, int alignment)
{
	guard(CMemoryChain::Alloc);
	if (!size) return NULL;

	// sequence of blocks (with using "next" field): 1(==this)->5(last)->4->3->2->NULL
	CMemoryChain *b = (next) ? next : this;			// block for allocation
	byte* start = Align(b->data, alignment);		// start of new allocation
	// check block free space
	if (start + size > b->end)
	{
		//?? may be, search in other blocks ...
		// allocate in the new block
		b = new (size + alignment - 1) CMemoryChain;
		// insert new block immediately after 1st block (==this)
		b->next = next;
		next = b;
		start = Align(b->data, alignment);
	}
	// update pointer to a free space
	b->data = start + size;

	return start;
	unguard;
}


int CMemoryChain::GetSize() const
{
	int n = 0;
	for (const CMemoryChain *c = this; c; c = c->next)
		n += c->size;
	return n;
}


/*-----------------------------------------------------------------------------
	Debugging information
-----------------------------------------------------------------------------*/

#if DEBUG_MEMORY

struct CAllocInfo
{
	int				totalBlocks;
	int				totalBytes;
	const CStackTrace* stack;
};

static int CompareAllocInfo(const CAllocInfo* p1, const CAllocInfo* p2)
{
	int cmp = p2->totalBytes - p1->totalBytes;
	if (cmp == 0)
		cmp = p2->totalBlocks - p1->totalBlocks;
	return cmp;
}

void appDumpMemoryAllocations()
{
	appPrintf(
		"Memory information:\n"
		FORMAT_SIZE("d")" bytes allocated in %d blocks from %d points\n\n", GTotalAllocationSize, GTotalAllocationCount, GNumAllocationPoints
	);

	// collect statistics
	CAllocInfo allocations[MAX_ALLOCATION_POINTS];
	memset(allocations, 0, sizeof(allocations));
	int numAllocations = 0;

	for (const CBlockHeader* hdr = CBlockHeader::first; hdr; hdr = hdr->next)
	{
		const CStackTrace* stack = hdr->stack;
		CAllocInfo* info = NULL;
		for (int i = 0; i < numAllocations; i++)
			if (allocations[i].stack == stack)
			{
				info = &allocations[i];
				break;
			}
		if (!info)
		{
			assert(numAllocations < MAX_ALLOCATION_POINTS);
			info = &allocations[numAllocations++];
			info->stack = stack;
		}
		info->totalBytes += hdr->blockSize;
		info->totalBlocks++;
	}

	// sort by allocation size
	QSort(allocations, numAllocations, CompareAllocInfo);

	// dump statistics
	for (int i = 0; i < numAllocations; i++)
	{
		const CAllocInfo* info = &allocations[i];
		appPrintf("%d blocks %d bytes\n", info->totalBlocks, info->totalBytes);
		info->stack->Dump();
		appPrintf("\n");
	}
}

#endif // DEBUG_MEMORY
