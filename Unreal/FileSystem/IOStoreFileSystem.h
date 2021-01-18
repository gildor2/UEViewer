#ifndef __IOSTORE_FILE_SYSTEM_H__
#define __IOSTORE_FILE_SYSTEM_H__

#if UNREAL4

class FIOStoreFileSystem;
enum class EIoChunkType : uint8;
struct FIoChunkId;
struct FIoOffsetAndLength;
struct FIoStoreTocCompressedBlockEntry;

typedef uint64 FPackageId;

class FIOStoreFile : public FArchive
{
	DECLARE_ARCHIVE(FIOStoreFile, FArchive);
public:
	FIOStoreFile(int InFileIndex, FIOStoreFileSystem* InParent);

	virtual ~FIOStoreFile();

	virtual void Serialize(void *data, int size);

	virtual void Seek(int Pos)
	{
		guard(FIOStoreFile::Seek);
		assert(Pos >= 0 && Pos <= UncompressedSize);
		ArPos = Pos;
//		unguardf("file=%s", *Info->FileInfo->GetRelativeName());
		unguardf("file=%d", FileIndex);
	}

	virtual int GetFileSize() const
	{
		return UncompressedSize;
	}

	virtual bool IsOpen() const
	{
		return IsFileOpen;
	}

	virtual void Close();

	enum { EncryptionAlign = 16 }; // AES-specific constant
	enum { EncryptedBufferSize = 256 }; //?? TODO: check - may be value 16 will be better for performance

protected:
	// File (chunk) info
	FIOStoreFileSystem* Parent;
	int32		FileIndex;

	// Cached file data
	uint64		UncompressedOffset;		// offset of the chunk inside whole container
	uint32		UncompressedSize;		// uncompressed size of the chunk

	// Data for decompression
	byte*		UncompressedBuffer;
	int			UncompressedBufferPos;	// buffer's position inside the chunk

	bool		IsFileOpen;
};

class FIOStoreFileSystem : public FVirtualFileSystem
{
	friend FIOStoreFile;
public:
	static const int MAX_COMPRESSION_METHODS = 8;

	FIOStoreFileSystem(const char* InFilename, bool InIsGlobalContainer = false);

	~FIOStoreFileSystem();

	virtual bool AttachReader(FArchive* reader, FString& error);

	virtual FArchive* CreateReader(int index);

	int FindChunkByType(EIoChunkType ChunkType);
	FArchive* CreateReaderForChunk(EIoChunkType ChunkType);

	static bool LoadGlobalContainer(const char* Filename);

	FString PakEncryptionKey;

protected:
	const FString& GetPakEncryptionKey() const;

	void DecryptDataBlock(byte* Data, int DataSize);

	void WalkDirectoryTreeRecursive(struct FIoDirectoryIndexResource& IndexResource, int DirectoryIndex, const FString& ParentDirectory);

	FString Filename;
	FArchive* Reader;

	// utoc/ucas information
	bool bIsGlobalContainer;
	int32 ContainerFlags;
	int32 CompressionBlockSize;
	TArray<FIoChunkId> ChunkIds;
	TArray<FIoOffsetAndLength> ChunkLocations;
	TArray<FIoStoreTocCompressedBlockEntry> CompressionBlocks;
	int NumCompressionMethods;
	int CompressionMethods[MAX_COMPRESSION_METHODS];
	uint64 PartitionSize;
	uint32 PartitionCount;
};

const CGameFileInfo* FindPackageById(FPackageId PackageId);

#endif // UNREAL4

#endif // __IOSTORE_FILE_SYSTEM_H__
