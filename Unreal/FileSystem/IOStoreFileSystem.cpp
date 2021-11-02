#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"
#include "FileSystemUtils.h"
#include "UnrealPackage/UnPackage.h"

#include "IOStoreFileSystem.h"

#if UNREAL4

// Print file-chunk mapping for better understanding container structure
//#define PRINT_CHUNKS 1

enum class EIoStoreTocVersion : uint8
{
	Invalid = 0,
	Initial,
	DirectoryIndex,
	PartitionSize,		// UE4.27, allows to use multi-volume (partitioned) ucas files
	LatestPlusOne,
	Latest = LatestPlusOne - 1
};

enum class EIoContainerFlags : uint8
{
	None = 0,
	Compressed = 1,
	Encrypted = 2,
	Signed = 4,
	Indexed = 8,
};

enum class EIoChunkType : uint8
{
	Invalid,
	InstallManifest,
	ExportBundleData,
	BulkData,
	OptionalBulkData,
	MemoryMappedBulkData,
	LoaderGlobalMeta,
	LoaderInitialLoadMeta,
	LoaderGlobalNames,
	LoaderGlobalNameHashes,
	ContainerHeader
};

struct FIoChunkId
{
	uint8 Data[12];

	EIoChunkType GetType() const
	{
		return (EIoChunkType)Data[11];
	}

	FPackageId GetPackageId() const
	{
		return *(FPackageId*)Data;
	}
};

struct FIoOffsetAndLength
{
	uint64 GetOffset() const
	{
		// 5 byte big-endian value
		return (uint64(Data[0]) << 32) | (uint64(Data[1]) << 24) | (uint64(Data[2]) << 16) | (uint64(Data[3]) << 8) | uint64(Data[4]);
	}
	uint64 GetLength() const
	{
		// 5 byte big-endian value
		return (uint64(Data[5]) << 32) | (uint64(Data[6]) << 24) | (uint64(Data[7]) << 16) | (uint64(Data[8]) << 8) | uint64(Data[9]);
	}
protected:
	byte Data[10];
};

#define TOC_MAGIC "-==--==--==--==-"

struct FIoStoreTocCompressedBlockEntry
{
	// Packed data:
	// 5 bytes offset
	// 3 bytes compressed size + 3 bytes uncompressed size
	// 1 byte compression method

	uint64 GetOffset() const
	{
		uint64 Result = *(uint64*)Data;
		return Result & 0xFFFFFFFFFF; // mask 5 bytes
	}

	uint32 GetCompressedSize() const
	{
		uint32 Result = *(uint32*)(Data + 5); // skip "offset" bytes
		return Result & 0xFFFFFF;
	}

	uint32 GetUncompressedSize() const
	{
		uint32 Result = *(uint32*)(Data + 8); // skip offset and compressed size bytes
		return Result & 0xFFFFFF;
	}

	uint32 GetCompressionMethodIndex() const
	{
		return Data[11];
	}

protected:
	uint8 Data[12];
};

// The layout of this structure is the same on disk and in memory
struct FIoStoreTocHeader
{
	char Magic[16];
	uint32 Version;			// uint8 + padding
	uint32 TocHeaderSize;
	uint32 TocEntryCount;
	uint32 TocCompressedBlockEntryCount;
	uint32 TocCompressedBlockEntrySize;
	uint32 CompressionMethodNameCount;
	uint32 CompressionMethodNameLength;
	uint32 CompressionBlockSize;
	uint32 DirectoryIndexSize;
	uint32 PartitionCount;
	uint64 ContainerId;		// FIoContainerId
	FGuid EncryptionKeyGuid;
	uint8 ContainerFlags;	// EIoContainerFlags (uint8) + padding
	uint8 Pad1[7];			// pad to uint64
	uint64 PartitionSize;
	uint8 Pad[48];

	bool Read(FArchive& Ar)
	{
		// Read the header as a single memory block
		Ar.Serialize(this, sizeof(*this));
		// Verify the header
		if (memcmp(Magic, TOC_MAGIC, sizeof(Magic)) != 0)
			return false;
		if (TocHeaderSize != sizeof(*this))
			return false;
		if (TocCompressedBlockEntrySize != sizeof(FIoStoreTocCompressedBlockEntry))
			return false;
		if (Version < (int)EIoStoreTocVersion::PartitionSize)
		{
			PartitionCount = 1;
			PartitionSize = (uint64)-1;
		}
		return true;
	}
};

struct FIoDirectoryIndexEntry
{
	uint32 Name;
	uint32 FirstChildEntry;
	uint32 NextSiblingEntry;
	uint32 FirstFileEntry;

	friend FArchive& operator<<(FArchive& Ar, FIoDirectoryIndexEntry& E)
	{
		return Ar << E.Name << E.FirstChildEntry << E.NextSiblingEntry << E.FirstFileEntry;
	}
};

SIMPLE_TYPE(FIoDirectoryIndexEntry, uint32);

struct FIoFileIndexEntry
{
	uint32 Name;
	uint32 NextFileEntry;
	uint32 UserData;		// index in ChunkIds and ChunkOffsetLengths

	friend FArchive& operator<<(FArchive& Ar, FIoFileIndexEntry& E)
	{
		return Ar << E.Name << E.NextFileEntry << E.UserData;
	}
};

SIMPLE_TYPE(FIoFileIndexEntry, uint32);

// Notes:
// - DirectoryEntries[0] seems invalid data, all items are (-1)
// - FileEntries are sorted by UserData, what equals to index in ChunkIds array (they're filled at the
//   same time as filling ChunkIds array, see FIoStoreWriterImpl::ProcessChunksThread()).
// - CompressionBlocks are not in sync with any chunk data, it is sequential compression of the whole
//   .ucas file. To compute compression block index, file offset is divided by CompressionBlockSize.

struct FIoDirectoryIndexResource
{
	FString MountPoint;
	TArray<FIoDirectoryIndexEntry> DirectoryEntries;
	TArray<FIoFileIndexEntry> FileEntries;
	TArray<FString> StringTable;

	void Serialize(FArchive& Ar)
	{
		guard(FIoDirectoryIndexResource::Serialize);
		Ar << MountPoint;
		Ar << DirectoryEntries;
		Ar << FileEntries;
		Ar << StringTable;
		unguard;
	}
};

struct FIoStoreTocResource
{
	FIoStoreTocHeader Header;
	TArray<FIoChunkId> ChunkIds;
	TArray<FIoOffsetAndLength> ChunkOffsetLengths; // index here corresponds to ChunkIds
	TArray<FIoStoreTocCompressedBlockEntry> CompressionBlocks;
	int CompressionMethods[FIOStoreFileSystem::MAX_COMPRESSION_METHODS];
	FIoDirectoryIndexResource DirectoryIndex;

	FIoStoreTocResource()
	{}

	~FIoStoreTocResource()
	{}

	//todo Passing 'PakEncryptionKey' as non-const reference for possibility to detect/change the key here
	bool Read(FArchive& Ar, FString& PakEncryptionKey)
	{
		guard(FIoStoreTocResource::Read);

		// Read the header
		if (!Header.Read(Ar))
			return false;

		if (Header.Version > (int)EIoStoreTocVersion::Latest)
		{
			appPrintf("Warning: utok has version %d\n", Header.Version);
		}

		// Read remaining data
		int BufferSize = Ar.GetFileSize() - Ar.Tell();
		byte* Buffer = new byte[BufferSize];
		Ar.Serialize(Buffer, BufferSize);

		const byte* DataPtr = Buffer;

		CopyArrayView(ChunkIds, Buffer, Header.TocEntryCount);
		DataPtr += sizeof(FIoChunkId) * Header.TocEntryCount;

		CopyArrayView(ChunkOffsetLengths, DataPtr, Header.TocEntryCount);
		DataPtr += sizeof(FIoOffsetAndLength) * Header.TocEntryCount;

		CopyArrayView(CompressionBlocks, DataPtr, Header.TocCompressedBlockEntryCount);
		DataPtr += sizeof(FIoStoreTocCompressedBlockEntry) * Header.TocCompressedBlockEntryCount;

		// Compression methods
		CompressionMethods[0] = 0; // no compression
		assert(Header.CompressionMethodNameCount < FIOStoreFileSystem::MAX_COMPRESSION_METHODS);
		for (int i = 0; i < Header.CompressionMethodNameCount; i++)
		{
			CompressionMethods[i + 1] = StringToCompressionMethod((const char*)DataPtr);
			DataPtr += Header.CompressionMethodNameLength;
		}

		// Skip signatures if needed
		if (Header.ContainerFlags & (int)EIoContainerFlags::Signed)
		{
			int32 HashSize = *(int32*)DataPtr;
			DataPtr += sizeof(HashSize);
			DataPtr += HashSize * 2; // tok and block signatures
			DataPtr += Header.TocCompressedBlockEntryCount * 20; // SHA hash for every compressed block
		}

		// Directory index data
		if (Header.ContainerFlags & (int)EIoContainerFlags::Indexed)
		{
			assert(DataPtr + Header.DirectoryIndexSize <= Buffer + BufferSize);
			if ((Header.ContainerFlags & (int)EIoContainerFlags::Encrypted) != 0)
			{
				appDecryptAES(const_cast<byte*>(DataPtr), Header.DirectoryIndexSize, &PakEncryptionKey[0], PakEncryptionKey.Len());
			}
			FMemReader IndexReader(DataPtr, Header.DirectoryIndexSize);
			IndexReader.SetupFrom(Ar);
			DirectoryIndex.Serialize(IndexReader);
		}

		// The next is only Meta left to read

		delete[] Buffer;

		return true;

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	FIOStoreFile implementation
-----------------------------------------------------------------------------*/

FIOStoreFile::FIOStoreFile(int InFileIndex, FIOStoreFileSystem* InParent)
:	Parent(InParent)
,	FileIndex(InFileIndex)
,	UncompressedBuffer(NULL)
,	IsFileOpen(true)
{
	const FIoOffsetAndLength& OffsetAndLength = Parent->ChunkLocations[FileIndex];
	UncompressedOffset = OffsetAndLength.GetOffset();
	UncompressedSize = OffsetAndLength.GetLength();
}

FIOStoreFile::~FIOStoreFile()
{
	// Can't call virtual 'Close' from destructor, so use fully qualified name
	FIOStoreFile::Close();
}

void FIOStoreFile::Close()
{
	if (UncompressedBuffer)
	{
		appFree(UncompressedBuffer);
		UncompressedBuffer = NULL;
	}
	if (IsFileOpen)
	{
//		Parent->FileClosed();
		IsFileOpen = false;
	}
}

void FIOStoreFile::Serialize(void *data, int size)
{
	PROFILE_IF(size >= 1024);
	guard(FIOStoreFile::Serialize);
	if (ArStopper > 0 && ArPos + size > ArStopper)
		appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);

	// TODO: partitions. When PartitionCount is greater than 1, then we should read
	// data from a different file. Multiple files has suffices in their names: name_s2.ucas,
	// name_c3.ucas (the very first file has no name suffix) ... They behave like a single file.
	// Each file should have size equals to PartitionSize, number of files equals to PartitionCount.
	// The compressed offset should be divided by PartitionSize: the quetent is index of archive,
	// the remainder is a compressed data position in a file. Requests may pass through file
	// boundaries, so it's worth adding the checks inside a loop "while (size > 0)". File handles
	// should be precached at FS startup time.
	// Can't implement this logic right now as there's no games using it.
	assert(Parent->PartitionCount == 1);

	// (Re-)open pak file if needed
	if (!IsFileOpen)
	{
//		Parent->FileOpened();
		IsFileOpen = true;
	}

	FArchive* Reader = Parent->Reader;

	// References:
	// - FIoStoreReaderImpl::Read() - simpler implementation
	// - FFileIoStore::ReadBlocks() - more complex asynchronous reading, doing the same
	while (size > 0)
	{
		if ((UncompressedBuffer == NULL) || (ArPos < UncompressedBufferPos) || (ArPos >= UncompressedBufferPos + Parent->CompressionBlockSize))
		{
			// buffer is not ready
			if (UncompressedBuffer == NULL)
			{
				UncompressedBuffer = (byte*)appMallocNoInit(Parent->CompressionBlockSize); // size of uncompressed block
			}
			// prepare buffer
			int BlockIndex = int((UncompressedOffset + ArPos) / Parent->CompressionBlockSize);
			UncompressedBufferPos = int(int64(Parent->CompressionBlockSize) * BlockIndex - UncompressedOffset);

			const FIoStoreTocCompressedBlockEntry& Block = Parent->CompressionBlocks[BlockIndex];
			int CompressedBlockSize = Block.GetCompressedSize();
			int UncompressedBlockSize = Block.GetUncompressedSize();
			byte* CompressedData;
			if (!(Parent->ContainerFlags & int(EIoContainerFlags::Encrypted)))
			{
				CompressedData = (byte*)appMallocNoInit(CompressedBlockSize);
				Reader->Seek64(Block.GetOffset());
				Reader->Serialize(CompressedData, CompressedBlockSize);
			}
			else
			{
				int EncryptedSize = Align(CompressedBlockSize, EncryptionAlign);
				CompressedData = (byte*)appMallocNoInit(EncryptedSize);
				Reader->Seek64(Block.GetOffset());
				Reader->Serialize(CompressedData, EncryptedSize);
				FileRequiresAesKey();
				Parent->DecryptDataBlock(CompressedData, EncryptedSize);
			}
			uint32 CompressionMethodIndex = Block.GetCompressionMethodIndex();
			if (CompressionMethodIndex)
			{
				// Compressed data
				assert(CompressionMethodIndex <= Parent->NumCompressionMethods); // 0 = None is not counted, so "<=" is used here
				int CompressionFlags = Parent->CompressionMethods[CompressionMethodIndex];
				appDecompress(CompressedData, CompressedBlockSize, UncompressedBuffer, UncompressedBlockSize, CompressionFlags);
				appFree(CompressedData);
			}
			else
			{
				// Uncompressed data
				//todo: don't allocate 'CompressedData' and don't 'memcpy', read directly to 'UncompressedBuffer'
				assert(CompressedBlockSize == UncompressedBlockSize);
				memcpy(UncompressedBuffer, CompressedData, UncompressedBlockSize);
				appFree(CompressedData);
			}
		}

		// data is in buffer, copy it
		int BytesToCopy = UncompressedBufferPos + Parent->CompressionBlockSize - ArPos; // number of bytes until end of the buffer
		if (BytesToCopy > size) BytesToCopy = size;
		assert(BytesToCopy > 0);

		// copy uncompressed data
		int OffsetInBuffer = ArPos - UncompressedBufferPos;
		memcpy(data, UncompressedBuffer + OffsetInBuffer, BytesToCopy);

		// advance pointers
		ArPos += BytesToCopy;
		size  -= BytesToCopy;
		data  = OffsetPointer(data, BytesToCopy);
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	FPackageId to CGameFileInfo map
-----------------------------------------------------------------------------*/

struct PackageHashEntry
{
	FPackageId Id;
	const CGameFileInfo* File;
	PackageHashEntry* Next;
};

#define PACKAGE_HASH_BITS	12
#define PACKAGE_HASH_MASK	((1 << PACKAGE_HASH_BITS)-1)

static PackageHashEntry* PackageHashHeads[1 << PACKAGE_HASH_BITS];
static PackageHashEntry* PackageHashStore = NULL;
static CMemoryChain* PackageHashMemory = NULL;

FORCEINLINE int PackageIdToHash(FPackageId PackageId)
{
	return PackageId & PACKAGE_HASH_MASK;
}

const CGameFileInfo* FindPackageById(FPackageId PackageId)
{
	int Hash = PackageIdToHash(PackageId);
	for (const PackageHashEntry* Entry = PackageHashHeads[Hash]; Entry; Entry = Entry->Next)
	{
		if (Entry->Id == PackageId)
		{
			return Entry->File;
		}
	}
	return NULL;
}

void RegisterPackageId(FPackageId PackageId, const CGameFileInfo* File)
{
	int Hash = PackageIdToHash(PackageId);
	for (PackageHashEntry* Entry = PackageHashHeads[Hash]; Entry; Entry = Entry->Next)
	{
		if (Entry->Id == PackageId)
		{
			// The file could be overridden in patches
			Entry->File = File;
			return;
		}
	}

	if (!PackageHashMemory) PackageHashMemory = new CMemoryChain();
	PackageHashEntry* Entry = (PackageHashEntry*)PackageHashMemory->Alloc(sizeof(PackageHashEntry));
	Entry->Next = PackageHashHeads[Hash];
	PackageHashHeads[Hash] = Entry;

	Entry->Id = PackageId;
	Entry->File = File;
}


/*-----------------------------------------------------------------------------
	FIOStoreFileSystem implementation
-----------------------------------------------------------------------------*/

FIOStoreFileSystem::FIOStoreFileSystem(const char* InFilename, bool InIsGlobalContainer)
:	Filename(InFilename)
,	Reader(NULL)
,	bIsGlobalContainer(InIsGlobalContainer)
{}

FIOStoreFileSystem::~FIOStoreFileSystem()
{
	delete Reader;
}

#if PRINT_CHUNKS
static TArray<CGameFileInfo*> ChunkInfos;
#endif

const FString& FIOStoreFileSystem::GetPakEncryptionKey() const
{
	if (!PakEncryptionKey.IsEmpty())
		return PakEncryptionKey;

	// No encrypted index, so pick the first available key
	if (GAesKeys.Num())
		return GAesKeys[0];

	static FString EmptyString;
	return EmptyString;
}

void FIOStoreFileSystem::DecryptDataBlock(byte* Data, int DataSize)
{
	guard(FIOStoreFileSystem::DecryptDataBlock);

	const FString& Key = GetPakEncryptionKey();
	appDecryptAES(Data, DataSize, &Key[0], Key.Len());

	unguard;
}

bool FIOStoreFileSystem::AttachReader(FArchive* reader, FString& error)
{
	guard(FIOStoreFileSystem::AttachReader);

	// Find the container file first
	char ContainerFileName[MAX_PACKAGE_PATH];
	appStrncpyz(ContainerFileName, *Filename, ARRAY_COUNT(ContainerFileName));
	char* ext = strrchr(ContainerFileName, '.') + 1;
	strcpy(ext, "ucas");
	FArchive* ContainerFile = new FFileReader(ContainerFileName, EFileArchiveOptions::NoOpenError);
	if (!ContainerFile->IsOpen())
	{
		delete ContainerFile;
		error = ContainerFileName;
		error += " not found";
		return false;
	}

	FIoStoreTocResource Resource;
	if (!Resource.Read(*reader, PakEncryptionKey))
	{
		delete ContainerFile;
		error = ContainerFileName;
		error += " has unsupported format";
		return false;
	}

	bool bIsIndexed = Resource.Header.ContainerFlags & (int)EIoContainerFlags::Indexed;
	if (!bIsIndexed && !bIsGlobalContainer)
	{
		delete ContainerFile;
		error = ContainerFileName;
		error += " has no index";
		return false;
	}

	// Store relevant data in FIOStoreFileSystem
	Exchange(ChunkLocations, Resource.ChunkOffsetLengths);
	Exchange(CompressionBlocks, Resource.CompressionBlocks);
	ContainerFlags = Resource.Header.ContainerFlags;
	CompressionBlockSize = Resource.Header.CompressionBlockSize;
	NumCompressionMethods = Resource.Header.CompressionMethodNameCount;
	PartitionCount = Resource.Header.PartitionCount;
	PartitionSize = Resource.Header.PartitionSize;
	memcpy(CompressionMethods, Resource.CompressionMethods, sizeof(CompressionMethods));
	Exchange(ChunkIds, Resource.ChunkIds);

#if PRINT_CHUNKS
	ChunkInfos.Empty(ChunkIds.Num());
	ChunkInfos.AddZeroed(ChunkIds.Num());
#endif

	if (bIsIndexed)
	{
		// Process DirectoryIndex
		guard(ProcessIndex);
		ValidateMountPoint(Resource.DirectoryIndex.MountPoint, ContainerFileName);
		WalkDirectoryTreeRecursive(Resource.DirectoryIndex, 0, Resource.DirectoryIndex.MountPoint);
		unguard;
	}

#if PRINT_CHUNKS
	appPrintf("\n%d chunks\n\n", ChunkIds.Num());
	for (int i = 0; i < ChunkIds.Num(); i++)
	{
		const uint8* Id = ChunkIds[i].Data;
		const CGameFileInfo* File = ChunkInfos[i];
		appPrintf("%08X%08X [%d] #%d: %s\n", *(uint32*)Id, *(uint32*)(Id+4),
			*(uint32*)(Id+8) & 0xFFFFFF, Id[11], File ? *File->GetRelativeName() : "--"
		);
	}
#endif // PRINT_CHUNKS

	if (!bIsGlobalContainer)
	{
		// We don't need ChunkIds here
		ChunkIds.Empty();
	}

	delete reader;
	Reader = ContainerFile;
	return true;

	unguard;
}

#define FLATTEN_RECURSE 1

void FIOStoreFileSystem::WalkDirectoryTreeRecursive(struct FIoDirectoryIndexResource& IndexResource, int DirectoryIndex, const FString& ParentDirectory)
{
#if FLATTEN_RECURSE
	struct StackEntry
	{
		FString ParentDirectory;
		int DirectoryIndex;
	};
	TArray<StackEntry> Stack;
	Stack.Empty(32);
	FStaticString<MAX_PACKAGE_PATH> CurrentParentDirectory = ParentDirectory;
#else
	const FString& CurrentParentDirectory = ParentDirectory;
#endif

	while (DirectoryIndex != -1)
	{
		const FIoDirectoryIndexEntry& Directory = IndexResource.DirectoryEntries[DirectoryIndex];
		FStaticString<MAX_PACKAGE_PATH> DirectoryPath;
		DirectoryPath = CurrentParentDirectory;
		if (Directory.Name != -1)
		{
			if (DirectoryPath.Len() && DirectoryPath[DirectoryPath.Len()-1] != '/')
			{
				DirectoryPath += "/";
			}
			DirectoryPath += IndexResource.StringTable[Directory.Name];
		}
		CompactFilePath(DirectoryPath);

		// Process files
		int FileIndex = Directory.FirstFileEntry;
		if (FileIndex != -1)
		{
			// Register the content folder
			int FolderIndex = RegisterGameFolder(*DirectoryPath);

			while (FileIndex != -1)
			{
				const FIoFileIndexEntry& File = IndexResource.FileEntries[FileIndex];
				// Register the file
				CRegisterFileInfo reg;
				reg.Filename = *IndexResource.StringTable[File.Name];
				reg.FolderIndex = FolderIndex;
				reg.Size = ChunkLocations[File.UserData].GetLength();
				reg.Flags = CGameFileInfo::GFI_IOStoreFile;
				reg.IndexInArchive = File.UserData;
				CGameFileInfo* file = RegisterFile(reg);
#if PRINT_CHUNKS
				ChunkInfos[File.UserData] = file;
#endif

				FileIndex = File.NextFileEntry;
				if (file->IsPackage())
				{
					RegisterPackageId(ChunkIds[File.UserData].GetPackageId(), file);
				}
			}
		}

		// Recurse to child folders
		if (Directory.FirstChildEntry != -1)
		{
#if FLATTEN_RECURSE
			// Store current state
			if (Directory.NextSiblingEntry != -1)
			{
				StackEntry* S = new (Stack) StackEntry;
				S->DirectoryIndex = Directory.NextSiblingEntry;
				S->ParentDirectory = CurrentParentDirectory;
			}
			// Recurse
			DirectoryIndex = Directory.FirstChildEntry;
			CurrentParentDirectory = DirectoryPath;
			continue;
#else
			WalkDirectoryTreeRecursive(IndexResource, Directory.FirstChildEntry, DirectoryPath);
#endif
		}

		// Proceed to the sibling directory
		DirectoryIndex = Directory.NextSiblingEntry;

#if FLATTEN_RECURSE
		if (DirectoryIndex == -1 && Stack.Num())
		{
			StackEntry& S = Stack[Stack.Num() - 1];
			DirectoryIndex = S.DirectoryIndex;
			CurrentParentDirectory = MoveTemp(S.ParentDirectory);
			Stack.RemoveAt(Stack.Num() - 1);
		}
#endif
	}
}

FArchive* FIOStoreFileSystem::CreateReader(int index)
{
	guard(FIOStoreFileSystem::CreateReader);

	return new FIOStoreFile(index, this);;

	unguard;
}

int FIOStoreFileSystem::FindChunkByType(EIoChunkType ChunkType)
{
	for (int index = 0; index < ChunkIds.Num(); index++)
	{
		if (ChunkIds[index].GetType() == ChunkType)
			return index;
	}
	return INDEX_NONE;
}

FArchive* FIOStoreFileSystem::CreateReaderForChunk(EIoChunkType ChunkType)
{
	guard(FIOStoreFileSystem::CreateReaderForChunk);
	int ChunkIndex = FindChunkByType(ChunkType);
	if (ChunkIndex < 0)
		appError("Unable to locate chunk of type %d", ChunkType);
	return new FIOStoreFile(ChunkIndex, this);;
	unguard;
}

/*static*/ bool FIOStoreFileSystem::LoadGlobalContainer(const char* Filename)
{
	guard(FIOStoreFileSystem::LoadGlobalContainer);

	FArchive* tocReader = new FFileReader(Filename, EFileArchiveOptions::NoOpenError);
	if (!tocReader->IsOpen())
	{
		delete tocReader;
		appPrintf("%s not found\n", Filename);
		return false;
	}

	FIOStoreFileSystem* globalContainer = new FIOStoreFileSystem(Filename, true);
	tocReader->Game = GAME_UE4_BASE;
	FString Error;
	if (!globalContainer->AttachReader(tocReader, Error))
	{
		appPrintf("Error in %s: %s\n", Filename, *Error);
		delete tocReader;
		delete globalContainer;
	}

	// Find name hashes to compute number of global names
	int NameHashesChunk = globalContainer->FindChunkByType(EIoChunkType::LoaderGlobalNameHashes);
	if (NameHashesChunk < 0)
		appError("Missing LoaderGlobalNameHashes chunk");
	int NameCount = globalContainer->ChunkLocations[NameHashesChunk].GetLength() / sizeof(uint64) - 1;

	// Pass global container data to UnPackage system
	FArchive* GlobalNameReader = globalContainer->CreateReaderForChunk(EIoChunkType::LoaderGlobalNames);
	FArchive* ScriptObjectsReader = globalContainer->CreateReaderForChunk(EIoChunkType::LoaderInitialLoadMeta);
	UnPackage::LoadGlobalData4(GlobalNameReader, ScriptObjectsReader, NameCount);
	delete GlobalNameReader;
	delete ScriptObjectsReader;

	return true;

	unguard;
}

#endif // UNREAL4
