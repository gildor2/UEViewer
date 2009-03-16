#include "Core.h"
#include "UnCore.h"

#if UNREAL3
#	include "lzo/lzo1x.h"

#	if XBOX360

#		if !USE_XDK

			extern "C"
			{
#			include "mspack/mspack.h"
#			include "mspack/lzx.h"
			}

#		else // USE_XDK

#			if _WIN32
#			pragma comment(lib, "xdecompress.lib")
			extern "C"
			{
				int  __stdcall XMemCreateDecompressionContext(int CodecType, const void* pCodecParams, unsigned Flags, void** pContext);
				void __stdcall XMemDestroyDecompressionContext(void* Context);
				int  __stdcall XMemDecompress(void* Context, void* pDestination, size_t* pDestSize, const void* pSource, size_t SrcSize);
			}
#			else  // _WIN32
#				error XDK build is not supported on this platform
#			endif // _WIN32
#		endif // USE_XDK

#	endif // XBOX360

#endif // UNREAL3


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

void FArchive::ByteOrderSerialize(void *data, int size)
{
	guard(FArchive::ByteOrderSerialize);

	Serialize(data, size);
	if (!ReverseBytes || size <= 1) return;

	assert(IsLoading);
	byte *p1 = (byte*)data;
	byte *p2 = p1 + size - 1;
	while (p1 < p2)
	{
		Exchange(*p1, *p2);
		p1++;
		p2--;
	}

	unguard;
}


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

#if !USE_XDK && XBOX360

struct mspack_file
{
	byte*		buf;
	int			bufSize;
	int			pos;
	int			rest;
};

static int mspack_read(mspack_file *file, void *buffer, int bytes)
{
	guard(mspack_read);

	if (!file->rest)
	{
		// read block header
		if (file->buf[file->pos] == 0xFF)
		{
			// [0]   = FF
			// [1,2] = uncompressed block size
			// [3,4] = compressed block size
			file->rest = (file->buf[file->pos+3] << 8) | file->buf[file->pos+4];
			file->pos += 5;
		}
		else
		{
			// [0,1] = compressed size
			file->rest = (file->buf[file->pos+0] << 8) | file->buf[file->pos+1];
			file->pos += 2;
		}
		if (file->rest > file->bufSize - file->pos)
			file->rest = file->bufSize - file->pos;
	}
	if (bytes > file->rest) bytes = file->rest;
	if (!bytes) return 0;

	// copy block data
	memcpy(buffer, file->buf + file->pos, bytes);
	file->pos  += bytes;
	file->rest -= bytes;

	return bytes;
	unguard;
}

static int mspack_write(mspack_file *file, void *buffer, int bytes)
{
	guard(mspack_write);
	assert(file->pos + bytes <= file->bufSize);
	memcpy(file->buf + file->pos, buffer, bytes);
	file->pos += bytes;
	return bytes;
	unguard;
}

static void *mspack_alloc(mspack_system *self, size_t bytes)
{
	return appMalloc(bytes);
}

static void mspack_free(void *ptr)
{
	appFree(ptr);
}

static void mspack_copy(void *src, void *dst, size_t bytes)
{
	memcpy(dst, src, bytes);
}

static struct mspack_system lzxSys =
{
	NULL,				// open
	NULL,				// close
	mspack_read,
	mspack_write,
	NULL,				// seek
	NULL,				// tell
	NULL,				// message
	mspack_alloc,
	mspack_free,
	mspack_copy
};

static void appDecompressLZX(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize)
{
	guard(appDecompressLZX);

	// setup streams
	mspack_file src, dst;
	src.buf     = CompressedBuffer;
	src.bufSize = CompressedSize;
	src.pos     = 0;
	src.rest    = 0;
	dst.buf     = UncompressedBuffer;
	dst.bufSize = UncompressedSize;
	dst.pos     = 0;
	// prepare decompressor
	lzxd_stream *lzxd = lzxd_init(&lzxSys, &src, &dst, 17, 0, 256*1024, UncompressedSize);
	assert(lzxd);
	// decompress
	int r = lzxd_decompress(lzxd, UncompressedSize);
	if (r != MSPACK_ERR_OK)
		appError("lzxd_decompress() returned %d", r);
	// free resources
	lzxd_free(lzxd);

	unguard;
}

#endif // USE_XDK

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
	else if (Flags == COMPRESS_ZLIB)
		appError("appDecompress: Zlib compression is not supported");
	else if (Flags == COMPRESS_LZX)
	{
#if XBOX360
#	if !USE_XDK
		appDecompressLZX(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
#	else
		void *context;
		int r;
		r = XMemCreateDecompressionContext(0, NULL, 0, &context);
		if (r < 0) appError("XMemCreateDecompressionContext failed");
		unsigned int newLen = UncompressedSize;
		r = XMemDecompress(context, UncompressedBuffer, &newLen, CompressedBuffer, CompressedSize);
		if (r < 0) appError("XMemDecompress failed");
		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize);
		XMemDestroyDecompressionContext(context);
#	endif // USE_XDK
#else  // XBOX360
		appError("appDecompress: LZX compression is not supported");
#endif // XBOX360
	}
	else
		appError("appDecompress: unknown compression flags: %d", Flags);

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
		guard(OldBulkFormat);
		// old bulk format - evolution of TLazyArray
		assert(Ar.IsLoading);

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
		unguard;
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

//	int savePos, saveStopper;
	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
//		printf("bulk in separate file (flags=%X, pos=%X+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
		return;
//		// seek to data block
//		savePos     = Ar.Tell();
//		saveStopper = Ar.GetStopper();
//		Ar.SetStopper(0);
//		Ar.Seek(BulkDataOffsetInFile);
	}

	SerializeChunk(Ar);

	unguard;
}


void FByteBulkData::SerializeChunk(FArchive &Ar)
{
	guard(FByteBulkData::SerializeChunk);

	assert(!(BulkDataFlags & BULKDATA_NoData));

	// allocate array
	if (BulkData) appFree(BulkData);
	BulkData = NULL;
	int DataSize = ElementCount * GetElementSize();
	if (!DataSize) return;		// nothing to serialize
	BulkData = (byte*)appMalloc(DataSize);

	// serialize data block
	Ar.Seek(BulkDataOffsetInFile);
	if (BulkDataFlags & (BULKDATA_CompressedLzo | BULKDATA_CompressedZlib | BULKDATA_CompressedLzx))
	{
		// compressed block
		int flags = 0;
		if (BulkDataFlags & BULKDATA_CompressedZlib) flags = COMPRESS_ZLIB;
		if (BulkDataFlags & BULKDATA_CompressedLzo)  flags = COMPRESS_LZO;
		if (BulkDataFlags & BULKDATA_CompressedLzx)  flags = COMPRESS_LZX;
		appReadCompressedChunk(Ar, BulkData, DataSize, flags);
	}
	else
	{
		// uncompressed block
		Ar.Serialize(BulkData, DataSize);
	}
	assert(BulkDataOffsetInFile + BulkDataSizeOnDisk == Ar.Tell());

	unguard;
}


#endif // UNREAL3
