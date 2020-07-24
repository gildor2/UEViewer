#ifndef __UNARCHIVE_PAK_H__
#define __UNARCHIVE_PAK_H__

#if UNREAL4

// Pak file versions
enum
{
	PakFile_Version_Initial = 1,
	PakFile_Version_NoTimestamps = 2,
	PakFile_Version_CompressionEncryption = 3,		// UE4.3+
	PakFile_Version_IndexEncryption = 4,			// UE4.17+ - encrypts only pak file index data leaving file content as is
	PakFile_Version_RelativeChunkOffsets = 5,		// UE4.20+
	PakFile_Version_DeleteRecords = 6,				// UE4.21+ - this constant is not used in UE4 code
	PakFile_Version_EncryptionKeyGuid = 7,			// ... allows to use multiple encryption keys over the single project
	PakFile_Version_FNameBasedCompressionMethod = 8, // UE4.22+ - use string instead of enum for compression method
	PakFile_Version_FrozenIndex = 9,				// UE4.25 - used only in 4.25, removed with 4.26
	PakFile_Version_PathHashIndex = 10,				// UE4.26+ - all file paths are encrypted (stored as 64-bit hash values)

	PakFile_Version_Last,
	PakFile_Version_Latest = PakFile_Version_Last - 1
};

// Hack: use ArLicenseeVer to not pass FPakInfo.Version to serializer.
// Note: UE4.22 and UE4.23 are both using version 8, however pak format differs. Due to that, we're adding "PakSubver".
// Some details are here: https://udn.unrealengine.com/questions/518568/view.html

#define PakVer							(Ar.ArLicenseeVer >> 4)
#define PakSubver						(Ar.ArLicenseeVer & 15)
#define MakePakVer(MainVer, SubVer)		(((MainVer) << 4) | (SubVer))

struct FPakInfo
{
	int32		Magic;
	int32		Version;
	int64		IndexOffset;
	int64		IndexSize;
	byte		IndexHash[20];
	// When new fields are added to FPakInfo, they're serialized before 'Magic' to keep compatibility
	// with older pak file versions. At the same time, structure size grows.
	byte		bEncryptedIndex;
	FGuid		EncryptionKeyGuid;
	int32		CompressionMethods[4];

	enum
	{
		Size = sizeof(int32) * 2 + sizeof(int64) * 2 + 20 + /* new fields */ 1 + sizeof(FGuid),
		Size8 = Size + 32*4,				// added size of CompressionMethods as char[32]
		Size8a = Size8 + 32,				// UE4.23 - also has version 8 (like 4.22) but different pak file structure
		Size9 = Size8a + 1,					// UE4.25
		// Size10 = Size8a
	};

	friend FArchive& operator<<(FArchive& Ar, FPakInfo& P);
};

struct FPakCompressedBlock
{
	int64		CompressedStart;
	int64		CompressedEnd;

	friend FArchive& operator<<(FArchive& Ar, FPakCompressedBlock& B)
	{
		return Ar << B.CompressedStart << B.CompressedEnd;
	}
};

struct FPakEntry
{
	int64		Pos;
	int64		Size;
	int64		UncompressedSize;
	int32		CompressionMethod;
	int32		CompressionBlockSize;
	TArray<FPakCompressedBlock> CompressionBlocks;
	byte		bEncrypted;					// replaced with 'Flags' in UE4.21

	uint16		StructSize;					// computed value: size of FPakEntry prepended to each file
	FPakEntry*	HashNext;					// computed value: used for fast name lookup

	CGameFileInfo* FileInfo;

	void Serialize(FArchive& Ar);

	void CopyFrom(const FPakEntry& Other)
	{
		memcpy(this, &Other, sizeof(*this));
	}

	void DecodeFrom(const uint8* Data);

	friend FArchive& operator<<(FArchive& Ar, FPakEntry& E)
	{
		E.Serialize(Ar);
		return Ar;
	}
};

inline bool PakRequireAesKey(bool fatal = true)
{
	if ((GAesKey.Len() == 0) && !UE4EncryptedPak())
	{
		if (fatal)
			appErrorNoLog("AES key is required");
		return false;
	}
	return true;
}

class FPakFile : public FArchive
{
	DECLARE_ARCHIVE(FPakFile, FArchive);
public:
	FPakFile(const FPakEntry* info, FArchive* reader)
	:	Info(info)
	,	Reader(reader)
	,	UncompressedBuffer(NULL)
	{}

	virtual ~FPakFile()
	{
		if (UncompressedBuffer)
			appFree(UncompressedBuffer);
	}

	virtual void Serialize(void *data, int size);

	virtual void Seek(int Pos)
	{
		guard(FPakFile::Seek);
		assert(Pos >= 0 && Pos < Info->UncompressedSize);
		ArPos = Pos;
		unguardf("file=%s", *Info->FileInfo->GetRelativeName());
	}

	virtual int GetFileSize() const
	{
		return (int)Info->UncompressedSize;
	}

	virtual bool IsOpen() const
	{
		// Not really "open state", but rather indicate that there's something to clean up in Close()
		return UncompressedBuffer != NULL;
	}

	virtual void Close()
	{
		if (UncompressedBuffer)
		{
			appFree(UncompressedBuffer);
			UncompressedBuffer = NULL;
		}
	}

	enum { EncryptionAlign = 16 }; // AES-specific constant
	enum { EncryptedBufferSize = 256 }; //?? TODO: check - may be value 16 will be better for performance

protected:
	const FPakEntry* Info;
	FArchive*	Reader;
	byte*		UncompressedBuffer;
	int			UncompressedBufferPos;
};


class FPakVFS : public FVirtualFileSystem
{
public:
	FPakVFS(const char* InFilename)
	:	Filename(InFilename)
	,	Reader(NULL)
//	,	HashTable(NULL)
	,	NumEncryptedFiles(0)
	{}

	virtual ~FPakVFS()
	{
		delete Reader;
//		if (HashTable) delete[] HashTable;
	}

	void CompactFilePath(FString& Path);

	virtual bool AttachReader(FArchive* reader, FString& error);

	virtual FArchive* CreateReader(int index)
	{
		guard(FPakVFS::CreateReader);
		const FPakEntry& info = FileInfos[index];
		return new FPakFile(&info, Reader);
		unguard;
	}

protected:
	FString				Filename;
	FArchive*			Reader;
	TArray<FPakEntry>	FileInfos;
	FStaticString<MAX_PACKAGE_PATH> MountPoint;
	int					NumEncryptedFiles;

	void ValidateMountPoint(FString& MountPoint);

	// UE4.24 and older
	bool LoadPakIndexLegacy(FArchive* reader, const FPakInfo& info, FString& error);
	// UE4.25 and newer
	bool LoadPakIndex(FArchive* reader, const FPakInfo& info, FString& error);

#if 0
	enum { HASH_SIZE = 1024 };
	enum { HASH_MASK = HASH_SIZE - 1 };
	enum { MIN_PAK_SIZE_FOR_HASHING = 256 };

	FPakEntry**			HashTable;

	static uint16 GetHashForFileName(const char* FileName)
	{
		uint16 hash = 0;
		while (char c = *FileName++)
		{
			if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; // lowercase a character
			hash = ROL16(hash, 5) - hash + ((c << 4) + c ^ 0x13F);	// some crazy hash function
		}
		hash &= HASH_MASK;
		return hash;
	}

	void AddFileToHash(FPakEntry* File)
	{
		if (!HashTable)
		{
			HashTable = new FPakEntry* [HASH_SIZE];
			memset(HashTable, 0, sizeof(FPakEntry*) * HASH_SIZE);
		}
		uint16 hash = GetHashForFileName(File->Name);
		File->HashNext = HashTable[hash];
		HashTable[hash] = File;
	}
#endif
};


#endif // UNREAL4

#endif // __UNARCHIVE_PAK_H__
