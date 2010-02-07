#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"


//#define WHOLE_PKG_COMPRESS_TYPE		COMPRESS_LZO		// for Midway's games ...

/*-----------------------------------------------------------------------------
	Lineage2 file reader
-----------------------------------------------------------------------------*/

#if LINEAGE2 || EXTEEL

#define LINEAGE_HEADER_SIZE		28

class FFileReaderLineage : public FArchive
{
public:
	FArchive	*Reader;
	int			ArPosOffset;
	byte		XorKey;

	FFileReaderLineage(FArchive *File, int Key)
	:	Reader(File)
	,	ArPosOffset(LINEAGE_HEADER_SIZE)
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
	virtual void Seek(int Pos)
	{
		Reader->Seek(Pos + ArPosOffset);
	}
	virtual int Tell() const
	{
		return Reader->Tell() - ArPosOffset;
	}
	virtual int GetFileSize() const
	{
		return Reader->GetFileSize() - ArPosOffset;
	}
	virtual void SetStopper(int Pos)
	{
		Reader->SetStopper(Pos + ArPosOffset);
	}
	virtual int  GetStopper() const
	{
		return Reader->GetStopper() - ArPosOffset;
	}
};

#endif // LINEAGE2 || EXTEEL


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
		ReverseBytes = File->ReverseBytes;
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
			appError("Serializing behind stopper");

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
			if (Pos >= Chunk->UncompressedOffset && Pos < Chunk->UncompressedOffset + Chunk->UncompressedSize)
				break;
		}
		assert(Chunk); // should be found

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
				Block->CompressedSize   = CompressedSize;
			}
			else
#endif // BIOSHOCK
			{
				*Reader << ChunkHeader;
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
		appDecompress(CompressedBlock, Block->CompressedSize, Buffer, Block->UncompressedSize, CompressionFlags);
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
		guard(FUE3ArchiveReader::GetFileSize);
		const FCompressedChunk &Chunk = CompressedChunks[CompressedChunks.Num() - 1];
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

#if LINEAGE2 || EXTEEL
	int checkDword;
	*this << checkDword;
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
	#if LINEAGE2 || EXTEEL
		Game = GAME_Lineage2;	// may be changed by DetectGame()
	#endif
		// replace Loader
		Loader = new FFileReaderLineage(Loader, XorKey);
	}
	else
		Seek(0);	// seek back to header
#endif // LINEAGE2 || EXTEEL

#if UNREAL3
	// code for fully compressed packages support
	//!! rewrite this code, merge with game autodetection
	bool fullyCompressed = false;
	int checkDword1, checkDword2;
	*this << checkDword1;
	if (checkDword1 == PACKAGE_FILE_TAG_REV)
		ReverseBytes = true;
	*this << checkDword2;
	Loader->Seek(0);
	if (checkDword2 == PACKAGE_FILE_TAG || checkDword2 == 0x20000)
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
		Loader->ReverseBytes = ReverseBytes;				//?? low-level loader; possibly, do it in FUE3ArchiveReader()
#ifdef WHOLE_PKG_COMPRESS_TYPE
		Loader = new FUE3ArchiveReader(Loader, WHOLE_PKG_COMPRESS_TYPE, Chunks);
#else
		Loader = new FUE3ArchiveReader(Loader, ReverseBytes ? COMPRESS_LZX : COMPRESS_ZLIB, Chunks);
#endif
		fullyCompressed = true;
		unguard;
	}
#endif // UNREAL3

	// read summary
	*this << Summary;
	Loader->ReverseBytes = ReverseBytes;					//!! should implement as virtual function
	ArVer         = Summary.FileVersion;
	ArLicenseeVer = Summary.LicenseeVersion;
	PKG_LOG(("Loading package: %s Ver: %d/%d ", Filename, Summary.FileVersion, Summary.LicenseeVersion));
#if UNREAL3
	if (Game >= GAME_UE3)
		PKG_LOG(("Engine: %d ", Summary.EngineVersion));
	if (fullyCompressed)
		PKG_LOG(("[FullComp] "));
#endif
	PKG_LOG(("Names: %d Exports: %d Imports: %d\n", Summary.NameCount, Summary.ExportCount, Summary.ImportCount));

#if BIOSHOCK
	if (Game == GAME_Bioshock)
	{
		// read compression info
		int NumChunks, i;
		TArray<FCompressedChunk> Chunks;
		*this << NumChunks;
		Chunks.Empty(NumChunks);
		int UncompOffset = Tell() - 4;		//?? test "-4"
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
		Loader->Game = Game;
	}
#endif // BIOSHOCK

#if UNREAL3
	if (Game >= GAME_UE3 && Summary.CompressionFlags)
	{
		if (fullyCompressed) appError("Fully compressed package %s has additional compression table", filename);
		// replace Loader with special reader for compressed UE3 archives
		Loader = new FUE3ArchiveReader(Loader, Summary.CompressionFlags, Summary.CompressedChunks);
	}
#endif

	// read name table
	guard(ReadNameTable);
	if (Summary.NameCount > 0)
	{
		Seek(Summary.NameOffset);
		NameTable = new char*[Summary.NameCount];
		for (int i = 0; i < Summary.NameCount; i++)
		{
			if (Summary.FileVersion < 64)
			{
				char buf[256];
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
#if PARIAH
			else if (Game == GAME_Pariah && ((ArLicenseeVer & 0x3F) >= 0x1C))
			{
				// used word + char[] instead of FString
				word len;
				*this << len;
				NameTable[i] = new char[len+1];
				Serialize(NameTable[i], len+1);
				// skip object flags
				int tmp;
				*this << tmp;
			}
#endif // PARIAH
			else
			{
#if 0
				// FString, but less allocations ...
				int len;
				*this << AR_INDEX(len);
				char *s = NameTable[i] = new char[len];
				while (len-- > 0)
					*this << *s++;
#else
				// Lineage sometimes uses Unicode strings ...
				FString name;
				*this << name;
				NameTable[i] = new char[name.Num()];
				strcpy(NameTable[i], *name);
#endif
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
	#endif
				// skip object flags
				int tmp;
				*this << tmp;
	#if UNREAL3
		#if BIOSHOCK
				if (Game == GAME_Bioshock) goto qword_flags;
		#endif
				if (Game >= GAME_UE3)
				{
		#if WHEELMAN
					if (Game == GAME_Wheelman) goto word_flags;
		#endif
		#if MASSEFF
					if (Game == GAME_MassEffect2 && ArLicenseeVer >= 102) goto word_flags;
		#endif
				qword_flags:
					// object flags are 64-bit in UE3, skip additional 32 bits
					int unk;
					*this << unk;
				word_flags: ;
				}
	#endif // UNREAL3
			}
//			PKG_LOG(("Name[%d]: \"%s\"\n", i, NameTable[i]));
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
//			PKG_LOG(("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName));
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
//			PKG_LOG(("Export[%d]: %s'%s' offs=%08X size=%08X\n", i, GetObjectName(Exp->ClassIndex),
//				*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize));
		}
	}
	unguard;

#if UNREAL3
	if (Summary.FileVersion >= 415) // PACKAGE_V3
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
				printf("Depends for %s'%s' = %d\n", GetObjectName(Exp.ClassIndex),
					*Exp.ObjectName, Dep->Objects[i]);
			} */
		}
		unguard;
	}
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

	unguardf(("%s, game=%X", filename, Game));
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
		printf("WARNING: Unknown class \"%s\" for object \"%s\"\n", ClassName, *Exp.ObjectName);
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
			const FObjectExport &Exp2 = GetExport(PackageIndex);
			assert(Exp2.ExportFlags & EF_ForcedExport);
			const char *PackageName = Exp2.ObjectName;
			printf("Forced export: %s'%s.%s'\n", ClassName, PackageName, *Exp.ObjectName);
		}
	}
#endif // UNREAL3
	UObject::BeginLoad();

	// setup constant object fields
	Obj->Package      = this;
	Obj->PackageIndex = index;
	Obj->Name         = Exp.ObjectName;
	// add object to GObjLoaded for later serialization
	if (strncmp(Exp.ObjectName, "Default__", 9) != 0)	// default properties are not supported -- this is a clean UObject format
		UObject::GObjLoaded.AddItem(Obj);

	UObject::EndLoad();
	return Obj;

	unguardf(("%s:%d", Filename, index));
}


UObject* UnPackage::CreateImport(int index)
{
	guard(UnPackage::CreateImport);

	const FObjectImport &Imp = GetImport(index);

	// find root package
	int PackageIndex = Imp.PackageIndex;
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
#if UNREAL3
			// possible for UE3 forced exports
			const FObjectExport &Rec = GetExport(PackageIndex-1);
			PackageIndex = Rec.PackageIndex;
			PackageName  = Rec.ObjectName;
#else
			appError("Wrong package index: %d", PackageIndex);
#endif // UNREAL3
		}
	}
	// load package
	UnPackage *Package = LoadPackage(PackageName);

	if (!Package)
	{
//		printf("WARNING: Import(%s): package %s was not found\n", *Imp.ObjectName, PackageName);
		return NULL;
	}
	//!! use full object path
	// find object in loaded package export table
	int NewIndex = Package->FindExport(Imp.ObjectName, Imp.ClassName);
	if (NewIndex == INDEX_NONE)
	{
		printf("WARNING: Import(%s) was not found in package %s\n", *Imp.ObjectName, PackageName);
		return NULL;
	}
	// create object
	return Package->CreateExport(NewIndex);

	unguardf(("%s:%d", Filename, index));
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

	if (const CGameFileInfo *info = appFindGameFile(LocalName))
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
	printf("WARNING: package %s was not found\n", Name);
	MissingPackages.AddItem(strdup(Name));
	return NULL;

	unguardf(("%s", Name));
}
