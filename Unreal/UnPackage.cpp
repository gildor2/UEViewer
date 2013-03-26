#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


byte GForceCompMethod = 0;		// COMPRESS_...


//#define DEBUG_PACKAGE		1


/*-----------------------------------------------------------------------------
	Lineage2 file reader
-----------------------------------------------------------------------------*/

#if LINEAGE2 || EXTEEL

#define LINEAGE_HEADER_SIZE		28

class FFileReaderLineage : public FReaderWrapper
{
public:
	int			ArPosOffset;
	byte		XorKey;

	FFileReaderLineage(FArchive *File, int Key)
	:	FReaderWrapper(File, LINEAGE_HEADER_SIZE)
	,	XorKey(Key)
	{
		Seek(0);		// skip header
	}

	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);
		if (XorKey)
		{
			int i;
			byte *p;
			for (i = 0, p = (byte*)data; i < size; i++, p++)
				*p ^= XorKey;
		}
	}
};

#endif // LINEAGE2 || EXTEEL


/*-----------------------------------------------------------------------------
	Battle Territory Online
-----------------------------------------------------------------------------*/

#if BATTLE_TERR

class FFileReaderBattleTerr : public FReaderWrapper
{
public:
	FFileReaderBattleTerr(FArchive *File)
	:	FReaderWrapper(File)
	{}

	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);

		int i;
		byte *p;
		for (i = 0, p = (byte*)data; i < size; i++, p++)
		{
			byte b = *p;
			int shift;
			byte v;
			for (shift = 1, v = b & (b - 1); v; v = v & (v - 1))	// shift = number of identity bits in 'v' (but b=0 -> shift=1)
				shift++;
			b = ROL8(b, shift);
			*p = b;
		}
	}
};

#endif // BATTLE_TERR


/*-----------------------------------------------------------------------------
	Blade & Soul
-----------------------------------------------------------------------------*/

#if AA2

class FFileReaderAA2 : public FReaderWrapper
{
public:
	FFileReaderAA2(FArchive *File)
	:	FReaderWrapper(File)
	{}

	virtual void Serialize(void *data, int size)
	{
		int StartPos = Reader->Tell();
		Reader->Serialize(data, size);

		int i;
		byte *p;
		for (i = 0, p = (byte*)data; i < size; i++, p++)
		{
			byte b = *p;
		#if 0
			// used with ArraysAGPCount != 0
			int shift;
			byte v;
			for (shift = 1, v = b & (b - 1); v; v = v & (v - 1))	// shift = number of identity bits in 'v' (but b=0 -> shift=1)
				shift++;
			b = ROR8(b, shift);
		#else
			// used with ArraysAGPCount == 0
			int PosXor = StartPos + i;
			PosXor = (PosXor >> 8) ^ PosXor;
			b ^= (PosXor & 0xFF);
			if (PosXor & 2)
			{
				b = ROL8(b, 1);
			}
		#endif
			*p = b;
		}
	}
};

#endif // AA2


/*-----------------------------------------------------------------------------
	Blade & Soul
-----------------------------------------------------------------------------*/

#if BLADENSOUL

class FFileReaderBnS : public FReaderWrapper
{
public:
	FFileReaderBnS(FArchive *File)
	:	FReaderWrapper(File)
	{}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		int i;
		byte *p;
		static const char *key = "qiffjdlerdoqymvketdcl0er2subioxq";
		for (i = 0, p = (byte*)data; i < size; i++, p++, Pos++)
		{
			*p ^= key[Pos % 32];
		}
	}
};


static void DecodeBnSPointer(int &Value, unsigned Code1, unsigned Code2, int Index)
{
  unsigned tmp1 = ROR32(Value, (Index + Code2) & 0x1F);
  unsigned tmp2 = ROR32(Code1, Index % 32);
  Value = tmp2 ^ tmp1;
}


static void PatchBnSExports(FObjectExport *Exp, const FPackageFileSummary &Summary)
{
	unsigned Code1 = ((Summary.HeadersSize & 0xFF) << 24) |
					 ((Summary.NameCount   & 0xFF) << 16) |
					 ((Summary.NameOffset  & 0xFF) << 8)  |
					 ((Summary.ExportCount & 0xFF));
	unsigned Code2 = (Summary.ExportOffset + Summary.ImportCount + Summary.ImportOffset) & 0x1F;

	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		DecodeBnSPointer(Exp->SerialSize,   Code1, Code2, i);
		DecodeBnSPointer(Exp->SerialOffset, Code1, Code2, i);
	}
}

#endif // BLADENSOUL


/*-----------------------------------------------------------------------------
	Nurien
-----------------------------------------------------------------------------*/

#if NURIEN

class FFileReaderNurien : public FReaderWrapper
{
public:
	int			Threshold;

	FFileReaderNurien(FArchive *File)
	:	FReaderWrapper(File)
	,	Threshold(0x7FFFFFFF)
	{}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		// only first Threshold bytes are compressed (package headers)
		if (Pos >= Threshold) return;

		int i;
		byte *p;
		static const byte key[] = {
			0xFE, 0xF2, 0x35, 0x2E, 0x12, 0xFF, 0x47, 0x8A,
			0xE1, 0x2D, 0x53, 0xE2, 0x21, 0xA3, 0x74, 0xA8
		};
		for (i = 0, p = (byte*)data; i < size; i++, p++, Pos++)
		{
			if (Pos >= Threshold) return;
			*p ^= key[Pos & 0xF];
		}
	}
};

#endif // NURIEN


/*-----------------------------------------------------------------------------
	UE3 compressed file reader
-----------------------------------------------------------------------------*/

#if UNREAL3

class FUE3ArchiveReader : public FArchive
{
public:
	FArchive				*Reader;
	// compression data
	int						CompressionFlags;
	TArray<FCompressedChunk> CompressedChunks;
	// own file positions, overriding FArchive's one (because parent class is
	// used for compressed data)
	int						Stopper;
	int						Position;
	// decompression buffer
	byte					*Buffer;
	int						BufferSize;
	int						BufferStart;
	int						BufferEnd;
	// chunk
	const FCompressedChunk	*CurrentChunk;
	FCompressedChunkHeader	ChunkHeader;
	int						ChunkDataPos;

	FUE3ArchiveReader(FArchive *File, int Flags, const TArray<FCompressedChunk> &Chunks)
	:	Reader(File)
	,	CompressionFlags(Flags)
	,	Buffer(NULL)
	,	BufferSize(0)
	,	BufferStart(0)
	,	BufferEnd(0)
	,	CurrentChunk(NULL)
	{
		guard(FUE3ArchiveReader::FUE3ArchiveReader);
		CopyArray(CompressedChunks, Chunks);
		SetupFrom(*File);
		assert(CompressionFlags);
		assert(CompressedChunks.Num());
		unguard;
	}

	~FUE3ArchiveReader()
	{
		if (Buffer) delete Buffer;
		if (Reader) delete Reader;
	}

	virtual void Serialize(void *data, int size)
	{
		guard(FUE3ArchiveReader::Serialize);

		if (Stopper > 0 && Position + size > Stopper)
			appError("Serializing behind stopper (%d+%d > %d)", Position, size, Stopper);

		while (true)
		{
			// check for valid buffer
			if (Position >= BufferStart && Position < BufferEnd)
			{
				int ToCopy = BufferEnd - Position;						// available size
				if (ToCopy > size) ToCopy = size;						// shrink by required size
				memcpy(data, Buffer + Position - BufferStart, ToCopy);	// copy data
				// advance pointers/counters
				Position += ToCopy;
				size     -= ToCopy;
				data     = OffsetPointer(data, ToCopy);
				if (!size) return;										// copied enough
			}
			// here: data/size points outside of loaded Buffer
			PrepareBuffer(Position);
			assert(Position >= BufferStart && Position < BufferEnd);	// validate PrepareBuffer()
		}

		unguard;
	}

	void PrepareBuffer(int Pos)
	{
		guard(FUE3ArchiveReader::PrepareBuffer);
		// find compressed chunk
		const FCompressedChunk *Chunk = NULL;
		for (int ChunkIndex = 0; ChunkIndex < CompressedChunks.Num(); ChunkIndex++)
		{
			Chunk = &CompressedChunks[ChunkIndex];
			if (Pos < Chunk->UncompressedOffset + Chunk->UncompressedSize)
				break;
		}
		assert(Chunk); // should be at least 1 chunk in CompressedChunks

		// DC Universe has uncompressed package headers but compressed remaining package part
		if (Pos < Chunk->UncompressedOffset)
		{
			if (Buffer) delete Buffer;
			int Size = Chunk->CompressedOffset;
			Buffer      = new byte[Size];
			BufferSize  = Size;
			BufferStart = 0;
			BufferEnd   = Size;
			Reader->Seek(0);
			Reader->Serialize(Buffer, Size);
			return;
		}

		if (Chunk != CurrentChunk)
		{
			// serialize compressed chunk header
			Reader->Seek(Chunk->CompressedOffset);
#if BIOSHOCK
			if (Game == GAME_Bioshock)
			{
				// read block size
				int CompressedSize;
				*Reader << CompressedSize;
				// generate ChunkHeader
				ChunkHeader.Blocks.Empty(1);
				FCompressedChunkBlock *Block = new (ChunkHeader.Blocks) FCompressedChunkBlock;
				Block->UncompressedSize = 32768;
				if (ArLicenseeVer >= 57)		//?? Bioshock 2; no version code found
					*Reader << Block->UncompressedSize;
				Block->CompressedSize = CompressedSize;
			}
			else
#endif // BIOSHOCK
			{
				if (Chunk->CompressedSize != Chunk->UncompressedSize)
					*Reader << ChunkHeader;
				else
				{
					// have seen such block in Borderlands: chunk has CompressedSize==UncompressedSize
					// and has no compression
					//!! verify UE3 code for this !!
					ChunkHeader.BlockSize = -1;	// mark as uncompressed (checked below)
					ChunkHeader.CompressedSize = ChunkHeader.UncompressedSize = Chunk->UncompressedSize;
					ChunkHeader.Blocks.Empty(1);
					FCompressedChunkBlock *Block = new (ChunkHeader.Blocks) FCompressedChunkBlock;
					Block->UncompressedSize = Block->CompressedSize = Chunk->UncompressedSize;
				}
			}
			ChunkDataPos = Reader->Tell();
			CurrentChunk = Chunk;
		}
		// find block in ChunkHeader.Blocks
		int ChunkPosition = Chunk->UncompressedOffset;
		int ChunkData     = ChunkDataPos;
		int UncompSize = 0, CompSize = 0;
		assert(ChunkPosition <= Pos);
		const FCompressedChunkBlock *Block = NULL;
		for (int BlockIndex = 0; BlockIndex < ChunkHeader.Blocks.Num(); BlockIndex++)
		{
			Block = &ChunkHeader.Blocks[BlockIndex];
			if (ChunkPosition + Block->UncompressedSize > Pos)
				break;
			ChunkPosition += Block->UncompressedSize;
			ChunkData     += Block->CompressedSize;
		}
		assert(Block);
		// read compressed data
		//?? optimize? can share compressed buffer and decompressed buffer between packages
		byte *CompressedBlock = new byte[Block->CompressedSize];
		Reader->Seek(ChunkData);
		Reader->Serialize(CompressedBlock, Block->CompressedSize);
		// prepare buffer for decompression
		if (Block->UncompressedSize > BufferSize)
		{
			if (Buffer) delete Buffer;
			Buffer = new byte[Block->UncompressedSize];
			BufferSize = Block->UncompressedSize;
		}
		// decompress data
		guard(DecompressBlock);
		if (ChunkHeader.BlockSize != -1)	// my own mark
			appDecompress(CompressedBlock, Block->CompressedSize, Buffer, Block->UncompressedSize, CompressionFlags);
		else
		{
			// no compression
			assert(Block->CompressedSize == Block->UncompressedSize);
			memcpy(Buffer, CompressedBlock, Block->CompressedSize);
		}
		unguardf(("block=%X+%X", ChunkData, Block->CompressedSize));
		// setup BufferStart/BufferEnd
		BufferStart = ChunkPosition;
		BufferEnd   = ChunkPosition + Block->UncompressedSize;
		// cleanup
		delete CompressedBlock;
		unguard;
	}

	// position controller
	virtual void Seek(int Pos)
	{
		Position = Pos;
	}
	virtual int Tell() const
	{
		return Position;
	}
	virtual int GetFileSize() const
	{
		// this function is meaningful for the decompressor tool
		guard(FUE3ArchiveReader::GetFileSize);
		const FCompressedChunk &Chunk = CompressedChunks[CompressedChunks.Num() - 1];
#if BIOSHOCK
		if (Game == GAME_Bioshock && ArLicenseeVer >= 57)		//?? Bioshock 2; no version code found
		{
			// Bioshock 2 has added "UncompressedSize" for block, so we must read it
			int OldPos = Reader->Tell();
			int CompressedSize, UncompressedSize;
			Reader->Seek(Chunk.CompressedOffset);
			*Reader << CompressedSize << UncompressedSize;
			Reader->Seek(OldPos);	// go back
			return Chunk.UncompressedOffset + UncompressedSize;
		}
#endif // BIOSHOCK
		return Chunk.UncompressedOffset + Chunk.UncompressedSize;
		unguard;
	}
	virtual void SetStopper(int Pos)
	{
		Stopper = Pos;
	}
	virtual int  GetStopper() const
	{
		return Stopper;
	}
};

#endif // UNREAL3

/*-----------------------------------------------------------------------------
	Package loading (creation) / unloading
-----------------------------------------------------------------------------*/

UnPackage::UnPackage(const char *filename, FArchive *Ar)
:	Loader(NULL)
{
	guard(UnPackage::UnPackage);

	// setup FArchive
	Loader = (Ar) ? Ar : new FFileReader(filename);
	IsLoading = true;

	appStrncpyz(Filename, appSkipRootDir(filename), ARRAY_COUNT(Filename));

#if LINEAGE2 || EXTEEL || BATTLE_TERR || BLADENSOUL
	int checkDword;
	*this << checkDword;

	#if LINEAGE2 || EXTEEL
	if (checkDword == ('L' | ('i' << 16)))	// unicode string "Lineage2Ver111"
	{
		// this is a Lineage2 package
		Seek(LINEAGE_HEADER_SIZE);
		// here is a encrypted by 'xor' standard FPackageFileSummary
		// to get encryption key, can check 1st byte
		byte b;
		*this << b;
		// for Ver111 XorKey==0xAC for Lineage or ==0x42 for Exteel, for Ver121 computed from filename
		byte XorKey = b ^ (PACKAGE_FILE_TAG & 0xFF);
		Game = GAME_Lineage2;	// may be changed by DetectGame()
		// replace Loader
		Loader = new FFileReaderLineage(Loader, XorKey);
	}
	else
	#endif // LINEAGE2 || EXTEEL
	#if BATTLE_TERR
	if (checkDword == 0x342B9CFC)
	{
		Game = GAME_BattleTerr;
		// replace Loader
		Loader = new FFileReaderBattleTerr(Loader);
	}
	#endif // BATTLE_TERR
	#if NURIEN
	FFileReaderNurien *NurienReader = NULL;
	if (checkDword == 0xB01F713F)
	{
		// replace loader
		Loader = NurienReader = new FFileReaderNurien(Loader);
	}
	#endif // NURIEN
	#if BLADENSOUL
	if (checkDword == 0xF84CEAB0)
	{
		Game = GAME_BladeNSoul;
		if (!GForceGame) GForceGame = GAME_BladeNSoul;
		Loader = new FFileReaderBnS(Loader);
	}
	#endif // BLADENSOUL
	Seek(0);	// seek back to header
#endif // complex

#if UNREAL3
	// code for fully compressed packages support
	//!! rewrite this code, merge with game autodetection
	bool fullyCompressed = false;
	int checkDword1, checkDword2;
	*this << checkDword1;
	if (checkDword1 == PACKAGE_FILE_TAG_REV)
	{
		ReverseBytes = true;
		if (GForcePlatform == PLATFORM_UNKNOWN)
			Platform = PLATFORM_XBOX360;			// default platform for "ReverseBytes" mode is PLATFORM_XBOX360
	}
	*this << checkDword2;
	Loader->Seek(0);
	if (checkDword2 == PACKAGE_FILE_TAG || checkDword2 == 0x20000 || checkDword2 == 0x10000)	// seen 0x10000 in Enslaved PS3
	{
		//!! NOTES:
		//!! 1)	GOW1/X360 byte-order logic is failed with Core.u and Ungine.u: package header is little-endian,
		//!!	but internal structures (should be) big endian; crashed on decompression: should use LZX, but
		//!!	used ZLIB
		//!! 2)	MKvsDC/X360 Core.u and Engine.u uses LZO instead of LZX
		guard(ReadFullyCompressedHeader);
		// this is a fully compressed package
		FCompressedChunkHeader H;
		*this << H;
		TArray<FCompressedChunk> Chunks;
		FCompressedChunk *Chunk = new (Chunks) FCompressedChunk;
		Chunk->UncompressedOffset = 0;
		Chunk->UncompressedSize   = H.UncompressedSize;
		Chunk->CompressedOffset   = 0;
		Chunk->CompressedSize     = H.CompressedSize;
		Loader->SetupFrom(*this);				//?? low-level loader; possibly, do it in FUE3ArchiveReader()
		byte CompMethod = GForceCompMethod;
		if (!CompMethod)
			CompMethod = (Platform == PLATFORM_XBOX360) ? COMPRESS_LZX : COMPRESS_ZLIB;
		Loader = new FUE3ArchiveReader(Loader, CompMethod, Chunks);
		fullyCompressed = true;
		unguard;
	}
#endif // UNREAL3

	// read summary
	*this << Summary;
//	ArVer         = Summary.FileVersion; -- already set by FPackageFileSummary serializer; plus, it may be overrided by DetectGame()
//	ArLicenseeVer = Summary.LicenseeVersion;
	Loader->SetupFrom(*this);
	PKG_LOG(("Loading package: %s Ver: %d/%d ", Filename, Summary.FileVersion, Summary.LicenseeVersion));
#if UNREAL3
	if (Game >= GAME_UE3)
		PKG_LOG(("Engine: %d ", Summary.EngineVersion));
	if (fullyCompressed)
		PKG_LOG(("[FullComp] "));
#endif
	PKG_LOG(("Names: %d Exports: %d Imports: %d Game: %X\n", Summary.NameCount, Summary.ExportCount, Summary.ImportCount, Game));

#if BIOSHOCK
	if (Game == GAME_Bioshock)
	{
		// read compression info
		int NumChunks, i;
		TArray<FCompressedChunk> Chunks;
		*this << NumChunks;
		Chunks.Empty(NumChunks);
		int UncompOffset = Tell() - 4;				//?? there should be a flag signalling presence of compression structures, because of "Tell()-4"
		for (i = 0; i < NumChunks; i++)
		{
			int Offset;
			*this << Offset;
			FCompressedChunk *Chunk = new (Chunks) FCompressedChunk;
			Chunk->UncompressedOffset = UncompOffset;
			Chunk->UncompressedSize   = 32768;
			Chunk->CompressedOffset   = Offset;
			Chunk->CompressedSize     = 0;			//?? not used
			UncompOffset             += 32768;
		}
		// replace Loader for reading compressed Bioshock archives
		Loader = new FUE3ArchiveReader(Loader, COMPRESS_ZLIB, Chunks);
		Loader->SetupFrom(*this);
	}
#endif // BIOSHOCK

#if AA2
	if (Game == GAME_AA2)
	{
		// America's Army 2 has encryption after FPackageFileSummary
		if (ArLicenseeVer >= 19)
		{
			int IsEncrypted;
			*this << IsEncrypted;
			if (IsEncrypted) Loader = new FFileReaderAA2(Loader);
		}
	}
#endif // AA2

#if UNREAL3
	if (Game >= GAME_UE3 && Summary.CompressionFlags)
	{
		if (fullyCompressed) appError("Fully compressed package %s has additional compression table", filename);
		// replace Loader with special reader for compressed UE3 archives
		Loader = new FUE3ArchiveReader(Loader, Summary.CompressionFlags, Summary.CompressedChunks);
	}
	#if NURIEN
	if (NurienReader) NurienReader->Threshold = Summary.HeadersSize;
	#endif
#endif // UNREAL3

	// read name table
	guard(ReadNameTable);
	if (Summary.NameCount > 0)
	{
		Seek(Summary.NameOffset);
		NameTable = new char*[Summary.NameCount];
		for (int i = 0; i < Summary.NameCount; i++)
		{
			guard(Name);
			if (Summary.FileVersion < 64)
			{
				char buf[1024];
				int len;
				for (len = 0; len < ARRAY_COUNT(buf); len++)
				{
					char c;
					*this << c;
					buf[len] = c;
					if (!c) break;
				}
				assert(len < ARRAY_COUNT(buf));
				NameTable[i] = strdup(buf);
				// skip object flags
				int tmp;
				*this << tmp;
			}
#if UC1 || PARIAH
			else if (Game == GAME_UC1 && ArLicenseeVer >= 28)
			{
			uc1_name:
				// used word + char[] instead of FString
				word len;
				*this << len;
				NameTable[i] = new char[len+1];
				Serialize(NameTable[i], len+1);
				// skip object flags
				int tmp;
				*this << tmp;
			}
	#if PARIAH
			else if (Game == GAME_Pariah && ((ArLicenseeVer & 0x3F) >= 28)) goto uc1_name;
	#endif
#endif // UC1 || PARIAH
			else
			{
				FString name;

#if SPLINTER_CELL
				if (Game == GAME_SplinterCell && ArLicenseeVer >= 85)
				{
					byte len;
					int flags;
					*this << len;
					NameTable[i] = new char[len + 1];
					Serialize(NameTable[i], len + 1);
					*this << flags;
					goto done;
				}
#endif // SPLINTER_CELL
#if LEAD
				if (Game == GAME_SplinterCellConv && ArVer >= 68)
				{
					int len;
					*this << AR_INDEX(len);
					NameTable[i] = new char[len + 1];
					Serialize(NameTable[i], len);
					goto done;
				}
#endif // LEAD
	#if AA2
				if (Game == GAME_AA2)
				{
					guard(AA2_FName);
					int len;
					*this << AR_INDEX(len);
					// read as unicode string and decrypt
					assert(len <= 0);
					len = -len;
					char *d = NameTable[i] = new char[len];
					byte shift = 5;
					for (int j = 0; j < len; j++, d++)
					{
						word c;
						*this << c;
						word c2 = ROR16(c, shift);
						assert(c2 < 256);
						*d = c2 & 0xFF;
						shift = (c - 5) & 15;
					}
					int unk;
					*this << AR_INDEX(unk);
					unguard;
					goto dword_flags;
				}
	#endif // AA2
#if DCU_ONLINE
				if (Game == GAME_DCUniverse)		// no version checking
				{
					int len;
					*this << len;
					assert(len > 0 && len < 0x3FF);	// requires extra code
					NameTable[i] = new char[len + 1];
					Serialize(NameTable[i], len);
					goto qword_flags;
				}
#endif // DCU_ONLINE
#if R6VEGAS
				if (Game == GAME_R6Vegas2 && ArLicenseeVer >= 71)
				{
					byte len;
					*this << len;
					NameTable[i] = new char[len + 1];
					Serialize(NameTable[i], len);
					goto done;
				}
#endif // R6VEGAS
#if TRANSFORMERS
				if (Game == GAME_Transformers && ArLicenseeVer >= 181) // Transformers: Fall of Cybertron; no real version in code
				{
					int len;
					*this << len;
					NameTable[i] = new char[len + 1];
					Serialize(NameTable[i], len);
					goto qword_flags;
				}
#endif // TRANSFORMERS

				// Korean games sometimes uses Unicode strings ...
				*this << name;
	#if AVA
				if (Game == GAME_AVA)
				{
					// strange code - package contains some bytes:
					// V(0) = len ^ 0x3E
					// V(i) = V(i-1) + 0x48 ^ 0xE1
					// Number of bytes = (len ^ 7) & 0xF
					int skip = name.Num();
					if (skip > 0) skip--;				// Num() included null terminating char
					skip = (skip ^ 7) & 0xF;
					Seek(Tell() + skip);
				}
	#endif // AVA
	#if 0
				NameTable[i] = new char[name.Num()];
				strcpy(NameTable[i], *name);
	#else
				NameTable[i] = name.Detach();
	#endif

				// skip object flags
	#if UNREAL3
		#if BIOSHOCK
				if (Game == GAME_Bioshock) goto qword_flags;
		#endif
				if (Game >= GAME_UE3 && ArVer >= 195)
				{
		#if WHEELMAN
					if (Game == GAME_Wheelman) goto dword_flags;
		#endif
		#if MASSEFF
					if (Game >= GAME_MassEffect && Game <= GAME_MassEffect3)
					{
						if (ArLicenseeVer >= 142) goto done;			// ME3, no flags
						if (ArLicenseeVer >= 102) goto dword_flags;		// ME2
					}
		#endif // MASSEFF

				qword_flags:
					// object flags are 64-bit in UE3, skip additional 32 bits
					int64 flags64;
					*this << flags64;
					goto done;
				}
	#endif // UNREAL3

			dword_flags:
				int flags32;
				*this << flags32;
			}
		done: ;
#if DEBUG_PACKAGE
			PKG_LOG(("Name[%d]: \"%s\"\n", i, NameTable[i]));
#endif
			unguardf(("%d", i));
		}
	}
	unguard;

	// load import table
	guard(ReadImportTable);
	if (Summary.ImportCount > 0)
	{
		Seek(Summary.ImportOffset);
		FObjectImport *Imp = ImportTable = new FObjectImport[Summary.ImportCount];
		for (int i = 0; i < Summary.ImportCount; i++, Imp++)
		{
			*this << *Imp;
#if DEBUG_PACKAGE
			PKG_LOG(("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName));
#endif
		}
	}
	unguard;

	// load exports table
	guard(ReadExportTable);
	if (Summary.ExportCount > 0)
	{
		Seek(Summary.ExportOffset);
		FObjectExport *Exp = ExportTable = new FObjectExport[Summary.ExportCount];
		for (int i = 0; i < Summary.ExportCount; i++, Exp++)
		{
			*this << *Exp;
#if DEBUG_PACKAGE
			PKG_LOG(("Export[%d]: %s'%s' offs=%08X size=%08X flags=%08X:%08X, exp_f=%08X arch=%d\n", i, GetObjectName(Exp->ClassIndex),
				*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize, Exp->ObjectFlags2, Exp->ObjectFlags, Exp->ExportFlags, Exp->Archetype));
#endif
		}
	}
#if BLADENSOUL
	if (Game == GAME_BladeNSoul && (Summary.PackageFlags & 0x08000000))
		PatchBnSExports(ExportTable, Summary);
#endif
	unguard;

#if UNREAL3
	if (Game == GAME_DCUniverse) goto no_depends;				// has non-standard checks
	if (Summary.FileVersion >= 415 && Summary.DependsOffset)	// some games are patrially upgraded: ArVer >= 415, but no depends table
	{
		guard(ReadDependsTable);
		Seek(Summary.DependsOffset);
		FObjectDepends *Dep = DependsTable = new FObjectDepends[Summary.ExportCount];
		for (int i = 0; i < Summary.ExportCount; i++, Dep++)
		{
			*this << *Dep;
/*			if (Dep->Objects.Num())
			{
				const FObjectExport &Exp = ExportTable[i];
				appPrintf("Depends for %s'%s' = %d\n", GetObjectName(Exp.ClassIndex),
					*Exp.ObjectName, Dep->Objects[i]);
			} */
		}
		unguard;
	}
no_depends: ;
#endif // UNREAL3

	// add self to package map
	char buf[256];
	const char *s = strrchr(filename, '/');
	if (!s) s = strrchr(filename, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = filename;
	appStrncpyz(buf, s, ARRAY_COUNT(buf));
	char *s2 = strchr(buf, '.');
	if (s2) *s2 = 0;
	appStrncpyz(Name, buf, ARRAY_COUNT(Name));
	PackageMap.AddItem(this);

	unguardf(("%s, ver=%d/%d, game=%X", filename, ArVer, ArLicenseeVer, Game));
}


UnPackage::~UnPackage()
{
	guard(UnPackage::~UnPackage);
	// free resources
	if (Loader) delete Loader;
	int i;
	for (i = 0; i < Summary.NameCount; i++)
		free(NameTable[i]);
	delete NameTable;
	delete ImportTable;
	delete ExportTable;
#if UNREAL3
	if (DependsTable) delete DependsTable;
#endif
	// remove self from package table
	i = PackageMap.FindItem(this);
	assert(i != INDEX_NONE);
	PackageMap.Remove(i);
	unguard;
}


/*-----------------------------------------------------------------------------
	Loading particular import or export package entry
-----------------------------------------------------------------------------*/

int UnPackage::FindExport(const char *name, const char *className, int firstIndex) const
{
	for (int i = firstIndex; i < Summary.ExportCount; i++)
	{
		const FObjectExport &Exp = ExportTable[i];
		// compare object name
		if (stricmp(Exp.ObjectName, name) != 0)
			continue;
		// if class name specified - compare it too
		const char *foundClassName = GetObjectName(Exp.ClassIndex);
		if (className && stricmp(foundClassName, className) != 0)
			continue;
		return i;
	}
	return INDEX_NONE;
}


bool UnPackage::CompareObjectPaths(int PackageIndex, UnPackage *RefPackage, int RefPackageIndex) const
{
	guard(UnPackage::CompareObjectPaths);

/*	appPrintf("Compare %s.%s [%d] with %s.%s [%d]\n",
		Name, GetObjectName(PackageIndex), PackageIndex,
		RefPackage->Name, RefPackage->GetObjectName(RefPackageIndex), RefPackageIndex
	); */

	while (PackageIndex || RefPackageIndex)
	{
		const char *PackageName, *RefPackageName;

		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else if (PackageIndex > 0)
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
			PackageName  = Name;

		if (RefPackageIndex < 0)
		{
			const FObjectImport &Rec = RefPackage->GetImport(-RefPackageIndex-1);
			RefPackageIndex = Rec.PackageIndex;
			RefPackageName  = Rec.ObjectName;
		}
		else if (RefPackageIndex > 0)
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = RefPackage->GetExport(RefPackageIndex-1);
			RefPackageIndex = Rec.PackageIndex;
			RefPackageName  = Rec.ObjectName;
		}
		else
			RefPackageName  = RefPackage->Name;
//		appPrintf("%20s -- %20s\n", PackageName, RefPackageName);
		if (stricmp(RefPackageName, PackageName) != 0) return false;
	}

	return true;

	unguard;
}


int UnPackage::FindExportForImport(const char *ObjectName, const char *ClassName, UnPackage *ImporterPackage, int ImporterIndex)
{
	guard(FindExportForImport);

	int ObjIndex = -1;
	while (true)
	{
		// iterate all objects with the same name and class
		ObjIndex = FindExport(ObjectName, ClassName, ObjIndex + 1);
		if (ObjIndex == INDEX_NONE)
			break;				// not found
		// a few objects in package could have the same name and class but resides in different groups,
		// so compare full object paths for sure
		if (CompareObjectPaths(ObjIndex+1, ImporterPackage, -1-ImporterIndex))
			return ObjIndex;	// found
	}

	return INDEX_NONE;			// not found

	unguard;
}


UObject* UnPackage::CreateExport(int index)
{
	guard(UnPackage::CreateExport);

	// create empty object
	FObjectExport &Exp = GetExport(index);
	if (Exp.Object)
		return Exp.Object;

	const char *ClassName = GetObjectName(Exp.ClassIndex);
	UObject *Obj = Exp.Object = CreateClass(ClassName);
	if (!Obj)
	{
		appPrintf("WARNING: Unknown class \"%s\" for object \"%s\"\n", ClassName, *Exp.ObjectName);
		return NULL;
	}
#if UNREAL3
	if (Game >= GAME_UE3 && (Exp.ExportFlags & EF_ForcedExport)) // ExportFlags appeared in ArVer=247
	{
		// find outermost package
		if (Exp.PackageIndex)
		{
			int PackageIndex = Exp.PackageIndex - 1;			// subtract 1, because 0 = no parent
			while (true)
			{
				const FObjectExport &Exp2 = GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;
				PackageIndex = Exp2.PackageIndex - 1;			// subtract 1 ...
			}
/*!! -- should display this message during serialization in UObject::EndLoad()
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			assert(Exp2.ExportFlags & EF_ForcedExport);
			const char *PackageName = Exp2.ObjectName;
			appPrintf("Forced export (%s): %s'%s.%s'\n", Name, ClassName, PackageName, *Exp.ObjectName);
*/
		}
	}
#endif // UNREAL3
	UObject::BeginLoad();

	// setup constant object fields
	Obj->Package      = this;
	Obj->PackageIndex = index;
	Obj->Name         = Exp.ObjectName;
	// add object to GObjLoaded for later serialization
	if (strnicmp(Exp.ObjectName, "Default__", 9) != 0)	// default properties are not supported -- this is a clean UObject format
		UObject::GObjLoaded.AddItem(Obj);

	UObject::EndLoad();
	return Obj;

	unguardf(("%s:%d", Filename, index));
}


UObject* UnPackage::CreateImport(int index)
{
	guard(UnPackage::CreateImport);

	FObjectImport &Imp = GetImport(index);
	if (Imp.Missing) return NULL;	// error message already displayed for this entry

	// load package
	const char *PackageName = GetObjectPackageName(Imp.PackageIndex);
	UnPackage *Package = LoadPackage(PackageName);
	int ObjIndex = INDEX_NONE;

	if (Package)
	{
		// has package with exactly this name - so this is either UE < 3 or non-cooked UE3 export
		ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
		if (ObjIndex == INDEX_NONE)
		{
			appPrintf("WARNING: Import(%s) was not found in package %s\n", *Imp.ObjectName, PackageName);
			Imp.Missing = true;
			return NULL;
		}
	}
#if UNREAL3
	// try to find import in startup package
	else if (Engine() >= GAME_UE3)
	{
		// check startup package
		Package = LoadPackage(GStartupPackage);
		if (Package)
			ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
		if (ObjIndex == INDEX_NONE)
		{
			// look in other loaded packages
			UnPackage *SkipPackage = Package;	// Package = either startup package or NULL
			for (int i = 0; i < PackageMap.Num(); i++)
			{
				Package = PackageMap[i];
				appPrintf("check for %s in %s\n", *Imp.ObjectName, Package->Name);
				if (Package == this || Package == SkipPackage)
					continue;		// already checked
				ObjIndex = Package->FindExportForImport(Imp.ObjectName, Imp.ClassName, this, index);
				if (ObjIndex != INDEX_NONE)
					break;			// found
			}
		}
		if (ObjIndex == INDEX_NONE)
		{
			appPrintf("WARNING: Import(%s.%s) was not found\n", PackageName, *Imp.ObjectName);
			Imp.Missing = true;
			return NULL;
		}
	}
#endif // UNREAL3

	// at this point we have either Package == NULL (not found) or Package != NULL and ObjIndex is valid

	if (!Package)
	{
		appPrintf("WARNING: Import(%s'%s'): package %s was not found\n", *Imp.ClassName, *Imp.ObjectName, PackageName);
		Imp.Missing = true;
		return NULL;
	}

	// create object
	return Package->CreateExport(ObjIndex);

	unguardf(("%s:%d", Filename, index));
}


// get outermost package name
//?? this function is not correct, it is used in package exporter tool only
const char *UnPackage::GetObjectPackageName(int PackageIndex) const
{
	guard(UnPackage::GetObjectPackageName);

	const char *PackageName = NULL;
	while (PackageIndex)
	{
		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
	}
	return PackageName;

	unguard;
}


// get full object path in a form
// "OutermostPackage.Package1...PackageN.ObjectName"
void UnPackage::GetFullExportName(const FObjectExport &Exp, char *buf, int bufSize, bool IncludeObjectName, bool IncludeCookedPackageName) const
{
	guard(UnPackage::GetFullExportNameBase);

	const char *PackageNames[256];
	const char *PackageName;
	int NestLevel = 0;

	// get object name
	if (IncludeObjectName)
	{
		PackageName = Exp.ObjectName;
		PackageNames[NestLevel++] = PackageName;
	}

	// gather nested package names (object parents)
	int PackageIndex = Exp.PackageIndex;
	while (PackageIndex)
	{
		assert(NestLevel < ARRAY_COUNT(PackageNames));
		if (PackageIndex < 0)
		{
			const FObjectImport &Rec = GetImport(-PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
		}
		else
		{
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
#if UNREAL3
			if (PackageIndex == 0 && (Rec.ExportFlags && EF_ForcedExport) && !IncludeCookedPackageName)
				break;		// do not add cooked package name
#endif
		}
		PackageNames[NestLevel++] = PackageName;
	}
	// concatenate package names in reverse order (from root to object)
	*buf = 0;
	for (int i = NestLevel-1; i >= 0; i--)
	{
		const char *PackageName = PackageNames[i];
		char *dst = strchr(buf, 0);
		appSprintf(dst, bufSize - (dst - buf), "%s%s", PackageName, i > 0 ? "." : "");
	}

	unguard;
}


const char *UnPackage::GetUncookedPackageName(int PackageIndex) const
{
	guard(UnPackage::GetUncookedPackageName);

#if UNREAL3
	if (PackageIndex != INDEX_NONE)
	{
		const FObjectExport &Exp = GetExport(PackageIndex);
		if (Game >= GAME_UE3 && (Exp.ExportFlags & EF_ForcedExport))
		{
			// find outermost package
			while (true)
			{
				const FObjectExport &Exp2 = GetExport(PackageIndex);
				if (!Exp2.PackageIndex) break;			// get parent (UPackage)
				PackageIndex = Exp2.PackageIndex - 1;	// subtract 1 from package index
			}
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			return *Exp2.ObjectName;
		}
	}
#endif // UNREAL3
	return Name;

	unguard;
}


/*-----------------------------------------------------------------------------
	Searching for package and maintaining package list
-----------------------------------------------------------------------------*/

TArray<UnPackage*>	UnPackage::PackageMap;
TArray<char*>		MissingPackages;

UnPackage *UnPackage::LoadPackage(const char *Name)
{
	guard(UnPackage::LoadPackage);

	const char *LocalName = appSkipRootDir(Name);

	int i;
	// check in loaded packages list
	for (i = 0; i < PackageMap.Num(); i++)
		if (!stricmp(LocalName, PackageMap[i]->Name))
			return PackageMap[i];
	// check in missing package names
	// note: it is not much faster than appFindGameFile(), but at least
	// this check will allow to print "missing package" warning only once
	for (i = 0; i < MissingPackages.Num(); i++)
		if (!stricmp(LocalName, MissingPackages[i]))
			return NULL;

	if (/*??(LocalName == Name) &&*/ (strchr(LocalName, '/') || strchr(LocalName, '\\')))
	{
		//?? trying to load package outside of game directory
		// or package contains path - appFindGameFile() will fail
		return new UnPackage(Name);
	}

	const CGameFileInfo *info = appFindGameFile(LocalName);
	if (info && info->IsPackage)
	{
		// Check in loaded packages again, but use info->RelativeName to compare
		// (package.Filename is set from info->RelativeName, see below).
		// This is done to prevent loading the same package twice when this function
		// is called with a different filename qualifiers: "path/package.ext",
		// "package.ext", "package"
		for (i = 0; i < PackageMap.Num(); i++)
			if (!stricmp(info->RelativeName, PackageMap[i]->Filename))
				return PackageMap[i];
		// package is not found, load it
		return new UnPackage(info->RelativeName, appCreateFileReader(info));
	}
	// package is missing
	// do not print any warnings: missing package is a normal situation in UE3 cooked builds, so print warnings
	// when needed at upper level
//	appPrintf("WARNING: package %s was not found\n", Name);
	MissingPackages.AddItem(strdup(Name));
	return NULL;

	unguardf(("%s", Name));
}
