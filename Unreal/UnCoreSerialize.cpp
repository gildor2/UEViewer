#include "Core.h"
#include "UnCore.h"

#if UNREAL4
#include "UnPackage.h"			// for accessing FPackageFileSummary from FByteBulkData
#endif


#define FILE_BUFFER_SIZE		4096


//#define DEBUG_BULK			1
//#define DEBUG_RAW_ARRAY		1

/*-----------------------------------------------------------------------------
	FCompactIndex
-----------------------------------------------------------------------------*/

static bool GameUsesFCompactIndex(FArchive &Ar)
{
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) return false;
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard && Ar.ArVer >= 128 && Ar.ArLicenseeVer >= 25) return false;
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) return false;
#endif
	return true;
}

FArchive& operator<<(FArchive &Ar, FCompactIndex &I)
{
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) appError("FCompactIndex is missing in UE3");
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


/*-----------------------------------------------------------------------------
	TArray
-----------------------------------------------------------------------------*/

FArchive& FArray::Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	int i = 0;

	guard(TArray::Serialize);

//-- if (Ar.IsLoading) Empty();	-- cleanup is done in TArray serializer (do not need
//								-- to pass array eraser/destructor to this function)
	// Here:
	// 1) when loading: 'this' array is empty (cleared from TArray's operator<<)
	// 2) when saving : data is not modified by this function

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	// perform serialization itself
	void *ptr;
	for (i = 0, ptr = DataPtr; i < Count; i++, ptr = OffsetPointer(ptr, elementSize))
		Serializer(Ar, ptr);
	return Ar;

	unguardf("%d/%d", i, DataCount);
}


struct DummyItem	// non-srializeable
{
	friend FArchive& operator<<(FArchive &Ar, DummyItem &Item)
	{
		return Ar;
	}
};

void SkipFixedArray(FArchive &Ar, int ItemSize)
{
	TArray<DummyItem> DummyArray;
	Ar << DummyArray;
	Ar.Seek(Ar.Tell() + DummyArray.Num() * ItemSize);
}

void SkipLazyArray(FArchive &Ar)
{
	guard(SkipLazyArray);
	assert(Ar.IsLoading);
	int pos;
	Ar << pos;
	assert(Ar.Tell() < pos);
	Ar.Seek(pos);
	unguard;
}

void appReverseBytes(void *Block, int NumItems, int ItemSize)
{
	byte *p1 = (byte*)Block;
	byte *p2 = p1 + ItemSize - 1;
	for (int i = 0; i < NumItems; i++, p1 += ItemSize, p2 += ItemSize)
	{
		byte *p1a = p1;
		byte *p2a = p2;
		while (p1a < p2a)
		{
			Exchange(*p1a, *p2a);
			p1a++;
			p2a--;
		}
	}
}


FArchive& FArray::SerializeRaw(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	guard(TArray::SerializeRaw);

	if (Ar.ReverseBytes)	// reverse bytes -> cannot use fast serializer
		return Serialize(Ar, Serializer, elementSize);

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	if (!Count) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * Count);
	return Ar;

	unguard;
}


FArchive& FArray::SerializeSimple(FArchive &Ar, int NumFields, int FieldSize)
{
	guard(TArray::SerializeSimple);

	//?? note: SerializeSimple() can reverse bytes on loading only, saving should
	//?? be done using generic serializer, or SerializeSimple should be
	//?? extended for this

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	int elementSize = NumFields * FieldSize;
	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	if (!Count) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * Count);
	// reverse bytes when needed
	if (FieldSize > 1 && Ar.ReverseBytes)
	{
		assert(Ar.IsLoading);
		appReverseBytes(DataPtr, Count * NumFields, FieldSize);
	}
	return Ar;

	unguard;
}


FArchive& SerializeLazyArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*))
{
	guard(TLazyArray<<);
	assert(Ar.IsLoading);
	int SkipPos = 0;								// ignored
	if (Ar.ArVer > 61)
		Ar << SkipPos;
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock && Ar.ArVer >= 131)
	{
		int f10, f8;
		Ar << f10 << f8;
//		printf("bio: pos=%08X skip=%08X f10=%08X f8=%08X\n", Ar.Tell(), SkipPos, f10, f8);
		if (SkipPos < Ar.Tell())
		{
//			appNotify("Bioshock: wrong SkipPos in array at %X", Ar.Tell());
			SkipPos = 0;		// have a few places with such bug ...
		}
	}
#endif // BIOSHOCK
	Serializer(Ar, &Array);
	return Ar;
	unguard;
}

#if UNREAL3

FArchive& SerializeRawArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*))
{
	guard(TRawArray<<);
	assert(Ar.IsLoading);
#if A51
	if (Ar.Game == GAME_A51 && Ar.ArVer >= 376) goto new_ver;	// partially upgraded old engine
#endif // A51
#if STRANGLE
	if (Ar.Game == GAME_Strangle) goto new_ver;					// also check package's MidwayTag ("WOO ") and MidwayVer (>= 369)
#endif
#if AVA
	if (Ar.Game == GAME_AVA && Ar.ArVer >= 436) goto new_ver;
#endif
#if DOH
	if (Ar.Game == GAME_DOH) goto old_ver;
#endif
#if MKVSDC
	if (Ar.Game == GAME_MK) goto old_ver;
#endif
	if (Ar.ArVer >= 453)
	{
	new_ver:
		int ElementSize;
		Ar << ElementSize;
		int SavePos = Ar.Tell();
		Serializer(Ar, &Array);
#if DEBUG_RAW_ARRAY
		appPrintf("savePos=%d count=%d elemSize=%d (real=%g) tell=%d\n", SavePos + 4, Array.Num(), ElementSize,
				Array.Num() ? float(Ar.Tell() - SavePos - 4) / Array.Num() : 0,
				Ar.Tell());
#endif
		if (Ar.Tell() != SavePos + 4 + Array.Num() * ElementSize)	// check position
			appError("RawArray item size mismatch: expected %d, got %d\n", ElementSize, (Ar.Tell() - SavePos) / Array.Num());
		return Ar;
	}
old_ver:
	// old version: no ElementSize property
	Serializer(Ar, &Array);
	return Ar;
	unguard;
}

void SkipRawArray(FArchive &Ar, int Size)
{
	guard(SkipRawArray);
	if (Ar.ArVer >= 453)
	{
		int ElementSize, Count;
		Ar << ElementSize << Count;
		assert(ElementSize == Size);
		Ar.Seek(Ar.Tell() + ElementSize * Count);
	}
	else
	{
		assert(Size > 0);
		int Count;
		Ar << Count;
		Ar.Seek(Ar.Tell() + Size * Count);
	}
	unguard;
}

#endif // UNREAL3


/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive &Ar, FString &S)
{
	guard(FString<<);

	if (!Ar.IsLoading)
	{
		Ar << (TArray<char>&)S;
		return Ar;
	}

	// loading

	// serialize character count
	int len;
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		Ar << AR_INDEX(len);		// Bioshock serialized positive number, but it's string is always unicode
		len = -len;
	}
	else
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard)	// this game uses int for arrays, but FCompactIndex for strings
		Ar << AR_INDEX(len);
	else
#endif
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(len);
	else
		Ar << len;

	S.Empty((len >= 0) ? len : -len);

	// resialize the string
	if (!len)
	{
		// empty FString
		// original UE has array count == 0 and special handling when converting FString
		// to char*
		S.AddItem(0);
		return Ar;
	}

	if (len > 0)
	{
		// ANSI string
		S.Add(len);
		Ar.Serialize(S.GetData(), len);
	}
	else
	{
		// UNICODE string
		for (int i = 0; i < -len; i++)
		{
			word c;
			Ar << c;
#if MASSEFF
			if (Ar.Game == GAME_MassEffect3 && Ar.ReverseBytes)	// uses little-endian strings for XBox360
				c = (c >> 8) | ((c & 0xFF) << 8);
#endif
			if (c & 0xFF00) c = '$';	//!! incorrect ...
			S.AddItem(c & 255);			//!! incorrect ...
		}
	}
/*	for (i = 0; i < S.Num(); i++) -- disabled 21.03.2012, Unicode chars are "fixed" before S.AddItem() above
	{
		char c = S[i];
		if (c >= 1 && c < ' ')
			S[i] = '$';
	} */
	if (S[abs(len)-1] != 0)
		appError("Serialized FString is not null-terminated");
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


/*-----------------------------------------------------------------------------
	FFileArchive classes
-----------------------------------------------------------------------------*/

FFileArchive::FFileArchive(const char *Filename, unsigned InOptions)
:	Options(InOptions)
,	f(NULL)
,	FileSize(-1)
,	Buffer(NULL)
,	BufferPos(0)
,	BufferSize(0)
{
	ArPos = 0;

	// process the filename
	FullName = appStrdup(Filename);
	const char *s = strrchr(FullName, '/');
	if (!s)     s = strrchr(FullName, '\\');
	if (s) s++; else s = FullName;
	ShortName = s;
}

FFileArchive::~FFileArchive()
{
	appFree(const_cast<char*>(FullName));
}

void FFileArchive::Seek(int Pos)
{
	ArPos = Pos;
}

bool FFileArchive::IsEof() const
{
	return ArPos < GetFileSize();
}

FArchive& FFileArchive::operator<<(FName &N)
{
	// do nothing
	return *this;
}

FArchive& FFileArchive::operator<<(UObject *&Obj)
{
	// do nothing
	return *this;
}

// this function is useful only for FRO_NoOpenError mode
bool FFileArchive::IsOpen() const
{
	return (f != NULL);
}

void FFileArchive::Close()
{
	if (IsOpen())
	{
		fclose(f);
		f = NULL;
		appFree(Buffer);
		Buffer = NULL;
	}
}

bool FFileArchive::OpenFile(const char *Mode)
{
	guard(FFileArchive::OpenFile);
	assert(!IsOpen());

	ArPos = FilePos = 0;
	Buffer = (byte*)appMalloc(FILE_BUFFER_SIZE);
	BufferPos = 0;
	BufferSize = 0;

	f = fopen(FullName, Mode);
	if (f) return true;			// success
	if (!(Options & FRO_NoOpenError))
		appError("Unable to open file %s", FullName);

	return false;
	unguard;
}

FFileReader::FFileReader(const char *Filename, unsigned InOptions)
:	FFileArchive(Filename, Options)
{
	guard(FFileReader::FFileReader);
	IsLoading = true;
	Open();
	unguardf("%s", Filename);
}

FFileReader::~FFileReader()
{
	Close();
}

void FFileReader::Serialize(void *data, int size)
{
	guard(FFileReader::Serialize);

	if (ArStopper > 0 && ArPos + size > ArStopper)
		appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);

	while (size > 0)
	{
		int LocalPos = ArPos - BufferPos;
		if (LocalPos < 0 || LocalPos >= BufferSize)
		{
			// seek to desired position if needed
			if (ArPos != FilePos)
			{
				fseek(f, ArPos, SEEK_SET);
				assert(ftell(f) == ArPos);
				FilePos = ArPos;
			}
			// the requested data is not in buffer
			if (size >= FILE_BUFFER_SIZE)
			{
				// large block, read directly from file
				int res = fread(data, size, 1, f);
				if (res != 1)
					appError("Unable to serialize %d bytes at pos=%d", size, FilePos);
			#if PROFILE
				GNumSerialize++;
				GSerializeBytes += size;
			#endif
				ArPos += size;
				FilePos += size;
				return;
			}
			// fill buffer
			int ReadBytes = fread(Buffer, 1, FILE_BUFFER_SIZE, f);
			if (ReadBytes == 0)
				appError("Unable to serialize %d bytes at pos=%d", 1, FilePos);
		#if PROFILE
			GNumSerialize++;
			GSerializeBytes += ReadBytes;
		#endif
			BufferPos = FilePos;
			BufferSize = ReadBytes;
			FilePos += ReadBytes;
			// update LocalPos
			LocalPos = ArPos - BufferPos;
			assert(LocalPos >= 0 && LocalPos < BufferSize);
		}

		// have something in buffer
		int CanCopy = BufferSize - LocalPos;
		if (CanCopy > size) CanCopy = size;
		memcpy(data, Buffer + LocalPos, CanCopy);
		data = OffsetPointer(data, CanCopy);
		size -= CanCopy;
		ArPos += CanCopy;
	}

	unguardf("File=%s", ShortName);
}

bool FFileReader::Open()
{
	return OpenFile("rb");
}

int FFileReader::GetFileSize() const
{
	// lazy file size computation
	if (FileSize < 0)
	{
		fseek(f, 0, SEEK_END);
		FFileReader* _this = const_cast<FFileReader*>(this);
		// don't rewind file back
		_this->FilePos = _this->FileSize = ftell(f);
	}
	return FileSize;
}

FFileWriter::FFileWriter(const char *Filename, unsigned Options)
:	FFileArchive(Filename, Options)
{
	guard(FFileWriter::FFileWriter);
	IsLoading = false;
	Open();
	unguardf("%s", Filename);
}

FFileWriter::~FFileWriter()
{
	Close();
}

void FFileWriter::Serialize(void *data, int size)
{
	guard(FFileWriter::Serialize);

	while (size > 0)
	{
		int LocalPos = ArPos - BufferPos;
		if (LocalPos < 0 || LocalPos >= FILE_BUFFER_SIZE || size >= FILE_BUFFER_SIZE)
		{
			// trying to write outside of buffer
			FlushBuffer();
			if (size >= FILE_BUFFER_SIZE)
			{
				// large block, write directly to file
				if (ArPos != FilePos)
				{
					fseek(f, ArPos, SEEK_SET);
					assert(ftell(f) == ArPos);
					FilePos = ArPos;
				}
				int res = fwrite(data, size, 1, f);
				if (res != 1)
					appError("Unable to serialize %d bytes at pos=%d", size, FilePos);
			#if PROFILE
				GNumSerialize++;
				GSerializeBytes += size;
			#endif
				ArPos += size;
				FilePos += size;
				return;
			}
			BufferPos = ArPos;
			BufferSize = 0;
			LocalPos = 0;
		}

		// have something for buffer
		int CanCopy = FILE_BUFFER_SIZE - LocalPos;
		if (CanCopy > size) CanCopy = size;
		memcpy(Buffer + LocalPos, data, CanCopy);
		data = OffsetPointer(data, CanCopy);
		size -= CanCopy;
		ArPos += CanCopy;
		BufferSize = max(BufferSize, LocalPos + CanCopy);
	}

	unguardf("File=%s", ShortName);
}

bool FFileWriter::Open()
{
	assert(!IsOpen());
	Buffer = (byte*)appMalloc(FILE_BUFFER_SIZE);
	BufferPos = 0;
	BufferSize = 0;
	return OpenFile("wb");
}

void FFileWriter::Close()
{
	FlushBuffer();
	Super::Close();
}

void FFileWriter::FlushBuffer()
{
	if (BufferSize > 0)
	{
		if (BufferPos != FilePos)
		{
			fseek(f, BufferPos, SEEK_SET);
			assert(ftell(f) == BufferPos);
			FilePos = BufferPos;
		}
		int res = fwrite(Buffer, BufferSize, 1, f);
		if (res != 1)
			appError("Unable to serialize %d bytes at pos=%d", BufferSize, FilePos);
#if PROFILE
		GNumSerialize++;
		GSerializeBytes += BufferSize;
#endif
		FilePos += BufferSize;
		BufferSize = 0;
		if (FilePos > FileSize) FileSize = FilePos;
	}
}

int FFileWriter::GetFileSize() const
{
	return max(FileSize, FilePos + BufferSize);
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


/*-----------------------------------------------------------------------------
	Reading UE3 bulk data and compressed chunks
-----------------------------------------------------------------------------*/

#if UNREAL3

FArchive& operator<<(FArchive &Ar, FCompressedChunkHeader &H)
{
	guard(FCompressedChunkHeader<<);
	int i;
	Ar << H.Tag;
	if (H.Tag == PACKAGE_FILE_TAG_REV)
		Ar.ReverseBytes = !Ar.ReverseBytes;
#if BERKANIX
	else if (Ar.Game == GAME_Berkanix && H.Tag == 0xF2BAC156) goto tag_ok;
#endif
#if HAWKEN
	else if (Ar.Game == GAME_Hawken && H.Tag == 0xEA31928C) goto tag_ok;
#endif
	else
		assert(H.Tag == PACKAGE_FILE_TAG);

#if UNREAL4
	if (Ar.Game >= GAME_UE4)
	{
		// Tag and BlockSize are really FCompressedChunkBlock, which has 64-bit integers here.
		int Pad;
		int64 BlockSize64;
		Ar << Pad << BlockSize64;
		assert((Pad == 0) && (BlockSize64 <= 0x7FFFFFFF));
		H.BlockSize = (int)BlockSize64;
		goto summary;
	}
#endif // UNREAL4

tag_ok:
	Ar << H.BlockSize;

summary:
	Ar << H.Sum;
#if 0
	if (H.BlockSize == PACKAGE_FILE_TAG)
		H.BlockSize = (Ar.ArVer >= 369) ? 0x20000 : 0x8000;
	int BlockCount = (H.Sum.UncompressedSize + H.BlockSize - 1) / H.BlockSize;
	H.Blocks.Empty(BlockCount);
	H.Blocks.Add(BlockCount);
	for (i = 0; i < BlockCount; i++)
		Ar << H.Blocks[i];
#else
	H.BlockSize = 0x20000;
	H.Blocks.Empty((H.Sum.UncompressedSize + 0x20000 - 1) / 0x20000);	// optimized for block size 0x20000
	int CompSize = 0, UncompSize = 0;
	while (CompSize < H.Sum.CompressedSize && UncompSize < H.Sum.UncompressedSize)
	{
		FCompressedChunkBlock *Block = new (H.Blocks) FCompressedChunkBlock;
		Ar << *Block;
		CompSize   += Block->CompressedSize;
		UncompSize += Block->UncompressedSize;
	}
	// check header; seen one package where sum(Block.CompressedSize) < H.CompressedSize,
	// but UncompressedSize is exact
	assert(/*CompSize == H.CompressedSize &&*/ UncompSize == H.Sum.UncompressedSize);
	if (H.Blocks.Num() > 1)
		H.BlockSize = H.Blocks[0].UncompressedSize;
#endif
	return Ar;
	unguardf("pos=%X", Ar.Tell());
}

// code is similar to FUE3ArchiveReader::PrepareBuffer()
void appReadCompressedChunk(FArchive &Ar, byte *Buffer, int Size, int CompressionFlags)
{
	guard(appReadCompressedChunk);

	// read header
	FCompressedChunkHeader ChunkHeader;
	Ar << ChunkHeader;
	// prepare buffer for reading compressed data
	int BufferSize = ChunkHeader.BlockSize * 16;
	byte *ReadBuffer = (byte*)appMalloc(BufferSize);	// BlockSize is size of uncompressed data
	// read and decompress data
	for (int BlockIndex = 0; BlockIndex < ChunkHeader.Blocks.Num(); BlockIndex++)
	{
		const FCompressedChunkBlock *Block = &ChunkHeader.Blocks[BlockIndex];
		assert(Block->CompressedSize <= BufferSize);
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


void FByteBulkData::SerializeHeader(FArchive &Ar)
{
	guard(FByteBulkData::SerializeHeader);

#if UNREAL4
	if (Ar.Game >= GAME_UE4)
	{
		guard(Bulk4);

		Ar << BulkDataFlags << ElementCount;
		Ar << BulkDataSizeOnDisk;
		if (Ar.ArVer < VER_UE4_BULKDATA_AT_LARGE_OFFSETS)
		{
			Ar << (int&)BulkDataOffsetInFile;		// 32-bit
		}
		else
		{
			Ar << BulkDataOffsetInFile;				// 64-bit
		}
		UnPackage* Package = Ar.CastTo<UnPackage>();
		assert(Package);
		BulkDataOffsetInFile += Package->Summary.BulkDataStartOffset;
	#if DEBUG_BULK
		appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%I64X+%X)\n",
			Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
	#endif
		return;

		unguard;
	}
#endif // UNREAL4

	if (Ar.ArVer < 266)
	{
		guard(OldBulkFormat);
		// old bulk format - evolution of TLazyArray
		// very old version: serialized EndPosition and ElementCount - exactly as TLazyArray
		assert(Ar.IsLoading);

		BulkDataFlags = 4;						// unknown
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
		int BulkDataOffsetInFile32;
		Ar << BulkDataSizeOnDisk << BulkDataOffsetInFile32;
		BulkDataOffsetInFile = BulkDataOffsetInFile32;			// sign extend to allow non-standard TFC systems which uses '-1' in this field
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 128)
		{
			int BulkDataKey;
			Ar << BulkDataKey;
		}
#endif // TRANSFORMERS
	}

#if MCARTA
	if (Ar.Game == GAME_MagnaCarta && (BulkDataFlags & 0x40))	// different flags
	{
		BulkDataFlags &= ~0x40;
		BulkDataFlags |= BULKDATA_CompressedLzx;
	}
#endif // MCARTA
#if APB
	if (Ar.Game == GAME_APB && (BulkDataFlags & 0x100))			// different flags
	{
		BulkDataFlags &= ~0x100;
		BulkDataFlags |= BULKDATA_SeparateData;
	}
#endif // APB

#if DEBUG_BULK
	appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%I64X+%X)\n",
		Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif

	unguard;
}


void FByteBulkData::Serialize(FArchive &Ar)
{
	guard(FByteBulkData::Serialize);

	SerializeHeader(Ar);

	if (BulkDataFlags & BULKDATA_Unused)	// skip serializing
	{
#if DEBUG_BULK
		appPrintf("bulk with no data\n");
#endif
		return;
	}

#if UNREAL4
	if ((Ar.Game >= GAME_UE4) && (BulkDataFlags & BULKDATA_PayloadAtEndOfFile))
	{
		// stored in the same file, but at different position
		// save archive position
		int savePos, saveStopper;
		savePos     = Ar.Tell();
		saveStopper = Ar.GetStopper();
		// seek to data block and read data
		Ar.SetStopper(0);
		SerializeChunk(Ar);
		// restore archive position
		Ar.Seek(savePos);
		Ar.SetStopper(saveStopper);
		return;
	}
#endif // UNREAL4

	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
		// stored in a different file (TFC)
#if DEBUG_BULK
		appPrintf("bulk in separate file (flags=%X, pos=%I64X+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif
		return;
	}

	if (BulkDataFlags & BULKDATA_SeparateData)
	{
		// stored in the same file, but at different position
		// save archive position
		int savePos, saveStopper;
		savePos     = Ar.Tell();
		saveStopper = Ar.GetStopper();
		// seek to data block and read data
		Ar.SetStopper(0);
		SerializeChunk(Ar);
		// restore archive position
		Ar.Seek(savePos);
		Ar.SetStopper(saveStopper);
		return;
	}

#if TRANSFORMERS
	// PS3 sounds in Transformers has alignment to 0x8000 with filling zeros
	if (Ar.Game == GAME_Transformers && Ar.Platform == PLATFORM_PS3)
		Ar.Seek64(BulkDataOffsetInFile);
#endif
	assert(BulkDataOffsetInFile == Ar.Tell());
	SerializeChunk(Ar);

	unguard;
}


// Serialize only header, and skip data block if it is inline
void FByteBulkData::Skip(FArchive &Ar)
{
	guard(FByteBulkData::Skip);

	SerializeHeader(Ar);
	if (BulkDataOffsetInFile == Ar.Tell())
	{
		// really should check flags here, but checking position is simpler
		Ar.Seek(Ar.Tell() + BulkDataSizeOnDisk);
	}

	unguard;
}


void FByteBulkData::SerializeChunk(FArchive &Ar)
{
	guard(FByteBulkData::SerializeChunk);

	assert(!(BulkDataFlags & BULKDATA_Unused));

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
#if BLADENSOUL
	else if (Ar.Game == GAME_BladeNSoul && (BulkDataFlags & BULKDATA_CompressedLzoEncr))
	{
		appReadCompressedChunk(Ar, BulkData, DataSize, COMPRESS_LZO_ENC);
	}
#endif
	else
	{
		// uncompressed block
		Ar.Serialize(BulkData, DataSize);
	}

	assert(BulkDataOffsetInFile + BulkDataSizeOnDisk == Ar.Tell());

	unguard;
}


#endif // UNREAL3
