#include "Core.h"
#include "UnCore.h"
#if UNREAL3
#include "lzo/lzo1x.h"
#endif


static char RootDirectory[256];

void appSetRootDirectory(const char *dir)
{
	appStrncpyz(RootDirectory, dir, ARRAY_COUNT(RootDirectory));
}

const char *appGetRootDirectory()
{
	return RootDirectory[0] ? RootDirectory : NULL;
}


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
#if UNREAL3
	if (Ar.ArVer >= 145) // PACKAGE_V3; UC2 ??
		Ar << DataCount;
	else
#endif
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
#if UNREAL3
	if (Ar.ArVer >= PACKAGE_V3)
		appError("FCompactIndex is missing in UE3");
#endif
	if (Ar.IsLoading)
	{
		byte b;
		Ar << b;
		int sign  = b & 0x80;	// sign bit
		int shift = 6;
		int r     = b & 0x3F;
		if (b & 0x40)			// has 2nd byte
		{
			do
			{
				Ar << b;
				r |= (b & 0x7F) << shift;
				shift += 7;
			} while (b & 0x80);	// has more bytes
		}
		I.Value = sign ? -r : r;
	}
	else
	{
		int v = I.Value;
		byte b = 0;
		if (v < 0)
		{
			v = -v;
			b |= 0x80;			// sign
		}
		b |= v & 0x3F;
		if (v <= 0x3F)
		{
			Ar << b;
		}
		else
		{
			b |= 0x40;			// has 2nd byte
			v >>= 6;
			Ar << b;
			assert(v);
			while (v)
			{
				b = v & 0x7F;
				v >>= 7;
				if (v)
					b |= 0x80;	// has more bytes
				Ar << b;
			}
		}
	}
	return Ar;
}


FArchive& operator<<(FArchive &Ar, FString &S)
{
	guard(FString<<);

	if (!Ar.IsLoading)
	{
		Ar << (TArray<char>&)S;
		return Ar;
	}
	// loading
	int len, i;
#if UNREAL3
	if (Ar.ArVer >= 145)		// PACKAGE_V3 ? found exact version in UC2
		Ar << len;
	else
#endif
		Ar << AR_INDEX(len);
	S.Empty((len >= 0) ? len : -len);
	if (len >= 0)
	{
		// ANSI string
		for (i = 0; i < len; i++)
		{
			char c;
			Ar << c;
			S.AddItem(c);
		}
	}
	else
	{
		// UNICODE string
		for (i = 0; i < -len; i++)
		{
			short c;
			Ar << c;
			S.AddItem(c & 255);		//!! incorrect ...
		}
	}
	return Ar;

	unguard;
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
	virtual void Serialize(void *data, int size)
	{}
};


static CDummyArchive DummyArchive;
FArchive *GDummySave = &DummyArchive;


#if UNREAL3

/*-----------------------------------------------------------------------------
	Reading UE3 compressed chunks
-----------------------------------------------------------------------------*/

void appDecompress(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize, int Flags)
{
	guard(appDecompress);
	if (Flags == COMPRESS_LZO)
	{
		int r;
		r = lzo_init();
		if (r != LZO_E_OK) appError("lzo_init() returned %d", r);
		unsigned long newLen = UncompressedSize;
		r = lzo1x_decompress_safe(CompressedBuffer, CompressedSize, UncompressedBuffer, &newLen, NULL);
		if (r != LZO_E_OK) appError("lzo_decompress() returned %d", r);
		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize);
	}
	else
	{
		appError("appDecompress: unknown compression flags: %d", Flags);
	}
	unguard;
}


// code is similar to FUE3ArchiveReader::PrepareBuffer()
void appReadCompressedChunk(FArchive &Ar, byte *Buffer, int Size, int CompressionFlags)
{
	guard(appReadCompressedChunk);

	// read header
	FCompressedChunkHeader ChunkHeader;
	Ar << ChunkHeader;
	// prepare buffer for reading compressed data
	byte *ReadBuffer = (byte*)appMalloc(ChunkHeader.BlockSize * 2);	// BlockSize is size of uncompressed data
	// read and decompress data
	for (int BlockIndex = 0; BlockIndex < ChunkHeader.Blocks.Num(); BlockIndex++)
	{
		const FCompressedChunkBlock *Block = &ChunkHeader.Blocks[BlockIndex];
		assert(ChunkHeader.BlockSize * 2 >= Block->CompressedSize);
		assert(Block->UncompressedSize <= Size);
		Ar.Serialize(ReadBuffer, Block->CompressedSize);
		appDecompress(ReadBuffer, Block->CompressedSize, Buffer, Block->UncompressedSize, CompressionFlags);
		Size   -= Block->UncompressedSize;
		Buffer += Block->UncompressedSize;
	}
	// finalize
	assert(Size == 0);			// should be comletely read
	delete ReadBuffer;
	unguard;
}


void FByteBulkData::Serialize(FArchive &Ar)
{
	guard(FByteBulkData::Serialize);

	if (Ar.ArVer < 266)
	{
		// old bulk format
		assert(Ar.IsLoading);
		//!! GET FROM SerializeByteBulkData_OLD() (WarGame)
		//?? TLazyArray -> FBulkData migration
		assert(0);

		BulkDataFlags = 4;					// unknown
		BulkDataSizeOnDisk = INDEX_NONE;
		int EndPosition;
		Ar << EndPosition;
		if (Ar.ArVer >= 254)
			Ar << BulkDataSizeOnDisk;
		if (Ar.ArVer >= 251)
		{
			int LazyLoaderFlags;
			Ar << LazyLoaderFlags;
			assert((LazyLoaderFlags & 1) == 0);	// LLF_PayloadInSeparateFile
			if (LazyLoaderFlags & 2)
				BulkDataFlags |= BULKDATA_CompressedZlib;
		}
		if (Ar.ArVer >= 260)
		{
			FName unk;
			Ar << unk;
		}
		Ar << ElementCount;
		if (BulkDataSizeOnDisk == INDEX_NONE)
			BulkDataSizeOnDisk = ElementCount * GetElementSize();
		BulkDataOffsetInFile = Ar.Tell();
		BulkDataSizeOnDisk   = EndPosition - BulkDataOffsetInFile;
	}
	else
	{
		// current bulk format
		// read header
		Ar << BulkDataFlags << ElementCount;
		assert(Ar.IsLoading);
		Ar << BulkDataSizeOnDisk << BulkDataOffsetInFile;
		if (BulkDataFlags & BULKDATA_NoData)	// skip serializing
			return;								//?? what to do with BulkData ?
	}

	assert((BulkDataFlags & BULKDATA_StoreInSeparateFile) || (BulkDataOffsetInFile == Ar.Tell()));

	// allocate array
	if (BulkData) appFree(BulkData);
	BulkData = NULL;
	int DataSize = ElementCount * GetElementSize();
	if (!DataSize) return;		// nothing to serialize
	BulkData = (byte*)appMalloc(DataSize);

//	int savePos, saveStopper;
	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
		printf("bulk in separate file (flags=%X, pos=%X+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
		return;		//????
//		// seek to data block
//		savePos     = Ar.Tell();
//		saveStopper = Ar.GetStopper();
//		Ar.SetStopper(0);
//		Ar.Seek(BulkDataOffsetInFile);
	}

	// serialize data block
	if (BulkDataFlags & (BULKDATA_Compressed | BULKDATA_CompressedZlib))
	{
		// compressed block
		appReadCompressedChunk(
			Ar, BulkData, DataSize,
			(BulkDataFlags & BULKDATA_CompressedZlib) ? COMPRESS_ZLIB : COMPRESS_LZO
		);
	}
	else
	{
		// uncompressed block
		Ar.Serialize(BulkData, DataSize);
	}
	assert(BulkDataOffsetInFile + BulkDataSizeOnDisk == Ar.Tell());

//	// restore stream position
//	if (isSeparateBlock)		//????
//	{
//		Ar.Seek(savePos);
//		Ar.SetStopper(saveStopper);
//	}

	unguard;
}


#endif // UNREAL3
