#include "Core.h"
#include "UnCore.h"


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
		Ar << BulkDataFlags << ElementCount;
		Ar << BulkDataSizeOnDisk;
		if (Ar.ArVer < VER_UE4_BULKDATA_AT_LARGE_OFFSETS)
		{
			Ar << BulkDataOffsetInFile;
			BulkDataOffsetInFile64 = BulkDataOffsetInFile;
		}
		else
		{
			Ar << BulkDataOffsetInFile64;
		}
		//!! add Summary.BulkDataStartOffset
#if DEBUG_BULK
		appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%I64X+%X)\n",
			Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile64, BulkDataSizeOnDisk);
#endif
		return;
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
		Ar << BulkDataSizeOnDisk << BulkDataOffsetInFile;
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
	appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%X+%X)\n",
		Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif

	unguard;
}


void FByteBulkData::Serialize(FArchive &Ar)
{
	guard(FByteBulkData::Serialize);

	SerializeHeader(Ar);

	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
#if DEBUG_BULK
		appPrintf("bulk in separate file (flags=%X, pos=%X+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif
		return;
	}

	if (BulkDataFlags & BULKDATA_NoData)	// skip serializing
	{
#if DEBUG_BULK
		appPrintf("bulk in separate file\n");
#endif
		return;								//?? what to do with BulkData ?
	}

	if (BulkDataFlags & BULKDATA_SeparateData)
	{
		// save archive position
		int savePos, saveStopper;
		savePos     = Ar.Tell();
		saveStopper = Ar.GetStopper();
		// seek to data block and read data
		Ar.SetStopper(0);
		Ar.Seek(BulkDataOffsetInFile);
		SerializeChunk(Ar);
		// restore archive position
		Ar.Seek(savePos);
		Ar.SetStopper(saveStopper);
		return;
	}

#if TRANSFORMERS
	// PS3 sounds in Transformers has alignment to 0x8000 with filling zeros
	if (Ar.Game == GAME_Transformers && Ar.Platform == PLATFORM_PS3)
		Ar.Seek(BulkDataOffsetInFile);
#endif
	assert(BulkDataOffsetInFile == Ar.Tell());
	SerializeChunk(Ar);

	unguard;
}


void FByteBulkData::Skip(FArchive &Ar)
{
	guard(FByteBulkData::Skip);

	// we cannot simply skip data, because:
	// 1) it may me compressed
	// 2) ElementSize is variable and not stored in archive

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
