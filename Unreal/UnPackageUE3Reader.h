#ifndef __UNPACKAGEUE3READER_H__
#define __UNPACKAGEUE3READER_H__

/*-----------------------------------------------------------------------------
	UE3/UE4 compressed file reader
-----------------------------------------------------------------------------*/

#if UNREAL3

class FUE3ArchiveReader : public FArchive
{
	DECLARE_ARCHIVE(FUE3ArchiveReader, FArchive);
public:
	FArchive				*Reader;

	bool					IsFullyCompressed;

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

	int						PositionOffset;

	FUE3ArchiveReader(FArchive *File, int Flags, const TArray<FCompressedChunk> &Chunks)
	:	Reader(File)
	,	IsFullyCompressed(false)
	,	CompressionFlags(Flags)
	,	Buffer(NULL)
	,	BufferSize(0)
	,	BufferStart(0)
	,	BufferEnd(0)
	,	CurrentChunk(NULL)
	,	PositionOffset(0)
	{
		guard(FUE3ArchiveReader::FUE3ArchiveReader);
		CopyArray(CompressedChunks, Chunks);
		SetupFrom(*File);
		assert(CompressionFlags);
		assert(CompressedChunks.Num());
		unguard;
	}

	virtual ~FUE3ArchiveReader()
	{
		if (Buffer) delete[] Buffer;
		if (Reader) delete Reader;
	}

	virtual bool IsCompressed() const
	{
		return true;
	}

	virtual void Serialize(void *data, int size)
	{
		guard(FUE3ArchiveReader::Serialize);

		if (Stopper > 0 && Position + size > Stopper)
			appError("Serializing behind stopper (%X+%X > %X)", Position, size, Stopper);

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
			if (Buffer) delete[] Buffer;
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
					// and has no compression; no such code in original engine
					ChunkHeader.BlockSize = -1;	// mark as uncompressed (checked below)
					ChunkHeader.Sum.CompressedSize = ChunkHeader.Sum.UncompressedSize = Chunk->UncompressedSize;
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
			if (Buffer) delete[] Buffer;
			Buffer = new byte[Block->UncompressedSize];
			BufferSize = Block->UncompressedSize;
		}
		// decompress data
		guard(DecompressBlock);
		if (ChunkHeader.BlockSize != -1)	// my own mark
		{
			// Decompress block
			int UsedCompressionFlags = CompressionFlags;
#if BATMAN
			if (Game == GAME_Batman4 && CompressionFlags == 8) UsedCompressionFlags = COMPRESS_LZ4;
#endif
			appDecompress(CompressedBlock, Block->CompressedSize, Buffer, Block->UncompressedSize, UsedCompressionFlags);
		}
		else
		{
			// No compression
			assert(Block->CompressedSize == Block->UncompressedSize);
			memcpy(Buffer, CompressedBlock, Block->CompressedSize);
		}
		unguardf("block=%X+%X", ChunkData, Block->CompressedSize);
		// setup BufferStart/BufferEnd
		BufferStart = ChunkPosition;
		BufferEnd   = ChunkPosition + Block->UncompressedSize;
		// cleanup
		delete[] CompressedBlock;
		unguard;
	}

	// position controller
	virtual void Seek(int Pos)
	{
		Position = Pos - PositionOffset;
	}
	virtual int Tell() const
	{
		return Position + PositionOffset;
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
		return Chunk.UncompressedOffset + Chunk.UncompressedSize + PositionOffset;
		unguard;
	}
	virtual void SetStopper(int Pos)
	{
		Stopper = Pos;
	}
	virtual int GetStopper() const
	{
		return Stopper;
	}
	virtual bool IsOpen() const
	{
		return Reader->IsOpen();
	}
	virtual bool Open()
	{
		return Reader->Open();
	}

	virtual void Close()
	{
		guard(FUE3ArchiveReader::Close);
		Reader->Close();
		if (Buffer)
		{
			delete[] Buffer;
			Buffer = NULL;
			BufferStart = BufferEnd = BufferSize = 0;
		}
		CurrentChunk = NULL;
		unguard;
	}

	void ReplaceLoaderWithOffset(FArchive* file, int offset)
	{
		if (Reader) delete Reader;
		Reader = file;
		PositionOffset = offset;
	}
};

#endif // UNREAL3

#endif // __UNPACKAGEUE3READER_H__
