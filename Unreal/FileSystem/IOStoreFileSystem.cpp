#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#include "IOStoreFileSystem.h"

#if UNREAL4

#define MAX_COMPRESSION_METHODS 8

// extern
int32 StringToCompressionMethod(const char* Name);

enum class EIoStoreTocVersion : uint8
{
	Invalid = 0,
	Initial,
	DirectoryIndex,
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

using FIoChunkId = uint8[12];

struct FIoOffsetAndLength
{
	uint64 GetOffset() const
	{
		// 5 byte big-endian value
		return (uint64(Data[0]) << 32) | (Data[1] << 24) | (Data[2] << 16) | (Data[3] << 8) | Data[4];
	}
	uint64 GetLength() const
	{
		// 5 byte big-endian value
		return (uint64(Data[5]) << 32) | (Data[6] << 24) | (Data[7] << 16) | (Data[8] << 8) | Data[9];
	}
protected:
	byte Data[10];
};

#define TOC_MAGIC "-==--==--==--==-"

struct FIoStoreTocCompressedBlockEntry
{
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
	uint64 ContainerId;		// FIoContainerId
	FGuid EncryptionKeyGuid;
	uint32 ContainerFlags;	// EIoContainerFlags (uint8) + padding
	uint8 Pad[60];

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
		return true;
	}
};

template<typename T>
void CopyArrayView(TArray<T>& Destination, const void* Source, int Count)
{
	Destination.Empty(Count);
	Destination.AddUninitialized(Count);
	memcpy(Destination.GetData(), Source, Count * sizeof(T));
}

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
	uint32 UserData;

	friend FArchive& operator<<(FArchive& Ar, FIoFileIndexEntry& E)
	{
		return Ar << E.Name << E.NextFileEntry << E.UserData;
	}
};

SIMPLE_TYPE(FIoFileIndexEntry, uint32);

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
	TArray<FIoOffsetAndLength> ChunkOffsetLengths;
	TArray<FIoStoreTocCompressedBlockEntry> CompressionBlocks;
	int CompressionMethods[MAX_COMPRESSION_METHODS];
	FIoDirectoryIndexResource DirectoryIndex;

	FIoStoreTocResource()
	{}

	~FIoStoreTocResource()
	{}

	bool Read(FArchive& Ar)
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
		assert(Header.CompressionMethodNameCount < MAX_COMPRESSION_METHODS);
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
			assert((Header.ContainerFlags & (int)EIoContainerFlags::Encrypted) == 0); // not supported, not used
			assert(DataPtr + Header.DirectoryIndexSize <= Buffer + BufferSize);
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

bool FIOStoreFileSystem::AttachReader(FArchive* reader, FString& error)
{
	guard(FIOStoreFileSystem::AttachReader);

	// Find the container file first
	char ContainerFileName[MAX_PACKAGE_PATH];
	appStrncpyz(ContainerFileName, *Filename, ARRAY_COUNT(ContainerFileName));
	char* ext = strrchr(ContainerFileName, '.') + 1;
	strcpy(ext, "ucas");
	FArchive* ContainerFile = new FFileReader(ContainerFileName, FAO_NoOpenError);
	if (!ContainerFile->IsOpen())
	{
		delete ContainerFile;
		error = ContainerFileName;
		error += " not found";
		return false;
	}

	FIoStoreTocResource Resource;
	if (!Resource.Read(*reader))
	{
		delete ContainerFile;
		error = ContainerFileName;
		error += " has unsupported format";
		return false;
	}

	// todo: WHEN all good, ContainerFile should go to "Reader" field, "reader" should be deleted, then return 'true'
	delete ContainerFile;
	error = ".utoc Not implemented";
	return false;

	unguard;
}

FArchive* FIOStoreFileSystem::CreateReader(int index)
{
	guard(FIOStoreFileSystem::CreateReader);

	assert(0);
	return NULL;

	unguard;
}

#endif // UNREAL4
