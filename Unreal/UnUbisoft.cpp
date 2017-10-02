#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"


/*-----------------------------------------------------------------------------
	LEAD Engine archive file reader
-----------------------------------------------------------------------------*/

#if LEAD

#define LEAD_FILE_TAG				0x4B435045		// 'EPCK'

// Archive flags
#define LEAD_PKG_COMPRESS_ZLIB		0x01			// use ZLib compression
#define LEAD_PKG_COMPRESS_XMEM		0x02			// use XBox 360 LZX compression
#define LEAD_PKG_CHECK_CRC			0x30			// really this is 2 separate flags
#define LEAD_PKG_COMPRESS_TABLES	0x80			// archive page table is compressed


struct FLeadArcPage
{
	int						CompressedPos;
	int						CompressedSize;
	byte					Flags;					// LEAD_PKG_COMPRESS_...
	unsigned				CompressedCrc;
	unsigned				UncompressedCrc;
};


class FLeadArchiveReader : public FArchive
{
public:
	FArchive				*Reader;
	// compression parameters
	int						BufferSize;
	int						FileSize;
	int						DirectoryOffset;
	byte					Flags;					// LEAD_PKG_...
	TArray<FLeadArcPage>	Pages;
	// own file positions, overriding FArchive's one (because parent class is
	// used for compressed data)
	int						Stopper;
	int						Position;
	// decompression buffer
	byte					*Buffer;
	int						BufferStart;
	int						BufferEnd;

	FLeadArchiveReader(FArchive *File)
	:	Reader(File)
	,	Buffer(NULL)
	,	BufferStart(0)
	,	BufferEnd(0)
	{
		guard(FLeadArchiveReader::FLeadArchiveReader);
		SetupFrom(*File);
		ReadArchiveHeaders();
		ArVer = 128;		// something UE2
		IsLoading = true;
		unguard;
	}

	~FLeadArchiveReader()
	{
		if (Buffer) delete Buffer;
		if (Reader) delete Reader;
	}

	void ReadArchiveHeaders()
	{
		guard(FLeadArchiveReader::ReadArchiveHeaders);
		*Reader << BufferSize << FileSize << DirectoryOffset << Flags;
		int DataStart = Reader->Tell();
		appPrintf("buf=%X file=%X dir=%X F=%X\n", BufferSize, FileSize, DirectoryOffset, Flags);
		if (Flags & LEAD_PKG_COMPRESS_TABLES)
		{
			int DataSkip, TableSizeUncompr, TableSizeCompr;
			*Reader << DataSkip << TableSizeUncompr << TableSizeCompr;
			byte* PageTableUncompr = new byte[TableSizeUncompr];
			appPrintf("compr tables: skip=%X uncompTblSize=%X compSize=%X\n", DataSkip, TableSizeUncompr, TableSizeCompr);
			if (TableSizeCompr)
			{
				byte* PageTableCompr = new byte[TableSizeCompr];
				Reader->Serialize(PageTableCompr, TableSizeCompr);
				int len = appDecompress(PageTableCompr, TableSizeCompr, PageTableUncompr, TableSizeUncompr, COMPRESS_ZLIB);
				assert(len == TableSizeUncompr);
				delete PageTableCompr;
			}
			else
			{
				Reader->Serialize(PageTableUncompr, TableSizeUncompr);
			}
			FMemReader Mem(PageTableUncompr, TableSizeUncompr);
			Mem.SetupFrom(*Reader);
			ReadPageTable(Mem, DataStart + DataSkip + 12, (Flags & LEAD_PKG_CHECK_CRC));
			assert(Mem.Tell() == TableSizeUncompr);
			delete PageTableUncompr;
		}
		else
		{
			appNotify("#1: LEAD package with uncompressed tables");	//!! UNTESTED
			Reader->Seek(DirectoryOffset - 4);
			ReadPageTable(*Reader, DataStart, (Flags & LEAD_PKG_CHECK_CRC));
		}
		unguard;
	}

	void ReadPageTable(FArchive& Ar, int DataOffset, bool checkCrc)
	{
		guard(FLeadArchiveReader::ReadPageTable);
		int NumPages = (FileSize + BufferSize - 1) / BufferSize;
		Pages.AddZeroed(NumPages);
		int Remaining = FileSize;
		int NumPages2;
		Ar << AR_INDEX(NumPages2);	// unused
		assert(NumPages == NumPages2);
		for (int i = 0; i < NumPages; i++)
		{
			FLeadArcPage& P = Pages[i];
			P.CompressedPos = DataOffset;
			Ar << AR_INDEX(P.CompressedSize) << P.Flags;
			if (checkCrc)
			{
				Ar << P.CompressedCrc << P.UncompressedCrc;
			}
			if (!P.CompressedSize)
				P.CompressedSize = min(Remaining, BufferSize);	// size of uncompressed page
			// advance pointers
			DataOffset += P.CompressedSize;
			Remaining  -= BufferSize;
		}
		unguard;
	}

	// this function is taken from FUE3ArchiveReader
	virtual void Serialize(void *data, int size)
	{
		guard(FLeadArchiveReader::Serialize);

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
		guard(FLeadArchiveReader::PrepareBuffer);

		int Page = Pos / BufferSize;
		assert(Page >= 0 && Page < Pages.Num());
		const FLeadArcPage& P = Pages[Page];
		if (!Buffer) Buffer = new byte[BufferSize];

		// read page
		Reader->Seek(P.CompressedPos);
		int DstSize = (Page < Pages.Num() - 1) ? BufferSize : FileSize % BufferSize;
		int SrcSize = P.CompressedSize;
		if (!SrcSize) SrcSize = DstSize;

		byte* CompressedBuffer = new byte[SrcSize];
		Reader->Serialize(CompressedBuffer, SrcSize);

		if (!P.Flags)
		{
			assert(SrcSize <= BufferSize);
			memcpy(Buffer, CompressedBuffer, SrcSize);
		}
		else
		{
			assert(SrcSize == P.CompressedSize);
			assert(P.Flags & (LEAD_PKG_COMPRESS_ZLIB | LEAD_PKG_COMPRESS_XMEM));
			appDecompress(
				CompressedBuffer, SrcSize, Buffer, DstSize,
				(P.Flags & LEAD_PKG_COMPRESS_ZLIB) ? COMPRESS_ZLIB : COMPRESS_LZX
			);
		}

		delete CompressedBuffer;

		BufferStart = Page * BufferSize;
		BufferEnd   = BufferStart + BufferSize;

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
		return FileSize;
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


struct FLeadDirEntry
{
	int						id;						// file identifier (hash of filename?)
	// the following data are originally packed as separate structure
	FString					ShortFilename;			// short filename
	FString					Filename;				// full filename with path
	int						FileSize;

	friend FArchive& operator<<(FArchive &Ar, FLeadDirEntry &D)
	{
		return Ar << D.id << D.ShortFilename << D.Filename << AR_INDEX(D.FileSize);
	}
};


struct FLeadArcChunk								// file chunk
{
	int						OriginalOffset;			// offset in original file
	int						PackedOffset;			// position in uncompressed UMD (excluding aligned archive directory size!! align=0x20000)
	int						ChunkSize;				// length of the data

	friend FArchive& operator<<(FArchive &Ar, FLeadArcChunk &C)
	{
		return Ar << AR_INDEX(C.OriginalOffset) << AR_INDEX(C.PackedOffset) << AR_INDEX(C.ChunkSize);
	}
};


struct FLeadArcChunkList
{
	int						id;						// file identifier
	TArray<FLeadArcChunk>	chunks;

	friend FArchive& operator<<(FArchive &Ar, FLeadArcChunkList& L)
	{
		return Ar << L.id << L.chunks;
	}
};


struct FLeadArcHdr
{
	FString					id;						// package tag
	TArray<FLeadDirEntry>	dir;					// file list
	TArray<FLeadArcChunkList> chunkList;			// file chunk list

	friend FArchive& operator<<(FArchive &Ar, FLeadArcHdr &H)
	{
		return Ar << H.id << H.dir << H.chunkList;
	}
};


class FLeadUmdFile : public FLeadArchiveReader
{
public:
	FLeadArcHdr				Hdr;
	int						DataStart;

	FLeadUmdFile(FArchive *File)
	:	FLeadArchiveReader(File)
	{
		// load embedded data
		Seek(0);
		*(FArchive*)this << Hdr;
		DataStart = Align(Tell(), 0x20000);		//?? align with BlockSize?
	}

	void PrintHeaders()
	{
		//!! dump embedded data
		appPrintf("DataStart: %08X\n", DataStart);
		int i;
		appPrintf("ID: %s\n", *Hdr.id);
		for (i = 0; i < Hdr.dir.Num(); i++)
		{
			const FLeadDirEntry& Dir = Hdr.dir[i];
			appPrintf("[%d] %08X - (%s) %s / %d (%08X)\n", i, Dir.id, *Dir.ShortFilename, *Dir.Filename, Dir.FileSize, Dir.FileSize);
		}
		for (i = 0; i < Hdr.chunkList.Num(); i++)
		{
			const FLeadArcChunkList& List = Hdr.chunkList[i];
			appPrintf("[%d] %08X (%d)\n", i, List.id, List.chunks.Num());
			for (int j = 0; j < List.chunks.Num(); j++)
			{
				const FLeadArcChunk &Chunk = List.chunks[j];
				appPrintf("       %08X  %08X  %08X\n", Chunk.OriginalOffset, Chunk.PackedOffset, Chunk.ChunkSize);
			}
		}
	}

	// test function
	void SaveEmbeddedFile(const char *FileName)
	{
		appPrintf("Extracting ...\n");
		byte* mem = new byte[FileSize];
		Seek(0);
		Serialize(mem, FileSize);
		for (int i = DataStart; i < FileSize; i++)	// data XOR-ed from DataStart offset
			mem[i] ^= 0xB7;
		appMakeDirectoryForFile(FileName);
		FArchive *OutAr = new FFileWriter(FileName);
		OutAr->Serialize(mem, FileSize);
		delete mem;
		delete OutAr;
		appPrintf("Extraction done.\n");
	}


	bool Extract(const char *OutDir)
	{
		guard(FLeadUmdFile::Extract);

		// extra bytes to allow oversize (bug in LEAD engine?)
#define ALLOW_EXTRA_BYTES	1
//		PrintHeaders();

		// precache all file contents
		byte *Data = new byte[FileSize + ALLOW_EXTRA_BYTES];
		guard(Precache);
		Seek(DataStart);
		Serialize(Data, FileSize - DataStart);
		for (int i = 0; i < FileSize - DataStart; i++)	// data XOR-ed from DataStart offset
			Data[i] ^= 0xB7;
		unguard;

		int i, j;
		for (i = 0; i < Hdr.chunkList.Num(); i++)
		{
			const FLeadArcChunkList& List = Hdr.chunkList[i];

			// find file name for this chunk list
			const char *FileName = NULL;
			guard(GetFilename);
			for (j = 0; j < Hdr.dir.Num(); j++)
			{
				const FLeadDirEntry& Dir = Hdr.dir[j];
				if (Dir.id == List.id)
				{
					FileName = *Dir.Filename;
					break;
				}
			}
			unguard;
			if (!FileName)
			{
				// should be found
//				PrintHeaders();
				appError("Chunk #%d: unable to find file for id %08X", i, List.id);
			}
			appPrintf("... %s\n", FileName);

			// open a file
			char FullFileName[512];
			appSprintf(ARRAY_ARG(FullFileName), "%s/%s", OutDir, FileName);
			appMakeDirectoryForFile(FullFileName);
			FILE *f = fopen(FullFileName, "rb+");				// open for update
			if (!f) f = fopen(FullFileName, "wb");				// open for writing
			if (!f)
			{
				appPrintf("ERROR: unable to open file \"%s\"\n", FullFileName);
				continue;
			}

			// extract all chunks in a list
			guard(ExtractChunks);
			for (j = 0; j < List.chunks.Num(); j++)
			{
				const FLeadArcChunk &Chunk = List.chunks[j];
				fseek(f, Chunk.OriginalOffset, SEEK_SET);
//				appPrintf("PackedOffset=%08X  ChunkSize=%08X  FileSize=%08X  DataStart=%08X\n", Chunk.PackedOffset, Chunk.ChunkSize, FileSize, DataStart);
				assert(Chunk.PackedOffset + Chunk.ChunkSize <= FileSize - DataStart + ALLOW_EXTRA_BYTES);
				fwrite(Data + Chunk.PackedOffset, Chunk.ChunkSize, 1, f);
			}
			unguardf("list %d/%d, chunk %d/%d", i, Hdr.chunkList.Num(), j, List.chunks.Num());

			fclose(f);
		}

		delete Data;

		return true;

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	External interface functions (CHANGE!! MOVE FLeadUmdFile to .h)
-----------------------------------------------------------------------------*/

FArchive* CreateUMDReader(FArchive *File)
{
	guard(CreateUMDReader);

	int Tag;
	*File << Tag;
	if (Tag != LEAD_FILE_TAG) return NULL;

	FLeadUmdFile *Ar = new FLeadUmdFile(File);	// note: File points to offset 4
//	Ar->PrintHeaders();
	return Ar;

	unguard;
}


bool ExtractUMDArchive(FArchive *UmdFile, const char *OutDir)
{
	guard(ExtractUMDArchive);

	FLeadUmdFile *File = (FLeadUmdFile*)UmdFile;
	return File->Extract(OutDir);

	unguard;
}


void SaveUMDArchive(FArchive *UmdFile, const char *OutName)
{
	guard(SaveUMDArchive);

	FLeadUmdFile *File = (FLeadUmdFile*)UmdFile;
	return File->SaveEmbeddedFile(OutName);

	unguard;
}


#endif // LEAD
