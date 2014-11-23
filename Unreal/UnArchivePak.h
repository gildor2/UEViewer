#ifndef __UNARCHIVEPAK_H__
#define __UNARCHIVEPAK_H__

#if UNREAL4


// NOTE: this implementation has a lot of common things with FObbFile. If there'll be another
// one virtual file system with similar implementation, it's worth to make some parent class
// for all of them which will differs perhaps only with AttachReader() method.

#define PAK_FILE_MAGIC		0x5A6F12E1

// Pak file versions
enum
{
	PAK_INITIAL = 1,
	PAK_NO_TIMESTAMPS,
	PAK_COMPRESSION_ENCRYPTION,
};

// hack: use ArLicenseeVer to not pass FPakInfo.Version to serializer
#define PakVer		ArLicenseeVer

struct FPakInfo
{
	int			Magic;
	int			Version;
	int64		IndexOffset;
	int64		IndexSize;
	byte		IndexHash[20];

	enum { Size = sizeof(int) * 2 + sizeof(int64) * 2 + 20 };

	friend FArchive& operator<<(FArchive& Ar, FPakInfo& P)
	{
		Ar << P.Magic << P.Version << P.IndexOffset << P.IndexSize;
		Ar.Serialize(ARRAY_ARG(P.IndexHash));
		return Ar;
	}
};

struct FPakEntry
{
	const char*	Name;
	int64		Pos;
	int64		Size;
	int64		UncompressedSize;
	int			CompressionMethod;
	byte		Hash[20];
	byte		bEncrypted;
	int			CompressionBlockSize;

	int			StructSize;					// computed value

	friend FArchive& operator<<(FArchive& Ar, FPakEntry& P)
	{
		guard(FPakEntry<<);

		// FPakEntry is duplicated before each stored file, without a filename. So,
		// remember the serialized size of this structure to avoid recomputation later.
		int64 StartOffset = Ar.Tell64();

		Ar << P.Pos << P.Size << P.UncompressedSize << P.CompressionMethod;

		if (Ar.PakVer < PAK_NO_TIMESTAMPS)
		{
			int64	timestamp;
			Ar << timestamp;
		}

		Ar.Serialize(ARRAY_ARG(P.Hash));

		if (Ar.PakVer >= PAK_COMPRESSION_ENCRYPTION)
		{
			if (P.CompressionMethod != 0)
			{
				appError("Compressed PAKs are not supported");
				// Ar << P.CompressionBlocks;
			}
			Ar << P.bEncrypted << P.CompressionBlockSize;
			if (P.bEncrypted) appError("Encrypted PAKs are not supported");
		}

		P.StructSize = Ar.Tell64() - StartOffset;

		return Ar;

		unguard;
	}
};

class FPakFile : public FArchive
{
	DECLARE_ARCHIVE(FPakFile, FArchive);
public:
	FPakFile(const FPakEntry* info, FArchive* reader)
	:	Info(info)
	,	Reader(reader)
	{}

	virtual void Serialize(void *data, int size)
	{
		guard(FPakFile::Serialize);
		if (ArStopper > 0 && ArPos + size > ArStopper)
			appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);
		// seek every time in a case if the same 'Reader' was used by different FPakFile
		// (this is a lightweight operation for buffered FArchive)
		// Note:
		Reader->Seek64(Info->Pos + Info->StructSize + ArPos);
		Reader->Serialize(data, size);
		ArPos += size;
		unguard;
	}

	virtual void Seek(int Pos)
	{
		guard(FPakFile::Seek);
		assert(Pos >= 0 && Pos < Info->UncompressedSize);
		ArPos = Pos;
		unguard;
	}

	virtual int GetFileSize() const
	{
		return Info->UncompressedSize;
	}

protected:
	const FPakEntry* Info;
	FArchive*	Reader;
};


class FPakVFS : public FVirtualFileSystem
{
public:
	FPakVFS()
	:	LastInfo(NULL)
	{}

	virtual ~FPakVFS()
	{
		delete Reader;
	}

	virtual bool AttachReader(FArchive* reader)
	{
		guard(FPakVFS::ReadDirectory);

		Reader = reader;

		// Read pak header
		FPakInfo info;
		Reader->Seek64(Reader->GetFileSize64() - FPakInfo::Size);
		*Reader << info;
		if (info.Magic != PAK_FILE_MAGIC)		// no endian checking here
			return false;

		// Read pak index

		Reader->ArLicenseeVer = info.Version;

		Reader->Seek64(info.IndexOffset);

		FString MountPoint;
		*Reader << MountPoint;

		int count;
		*Reader << count;
		FileInfos.Add(count);

		for (int i = 0; i < count; i++)
		{
			FPakEntry& E = FileInfos[i];
			// serialize name
			FStaticString<512> Filename;
			*Reader << Filename;
			E.Name = appStrdupPool(Filename);
			// serialize other fields
			*Reader << E;
		}

		return true;

		unguard;
	}

	virtual int GetFileSize(const char* name)
	{
		const FPakEntry* info = FindFile(name);
		return (info) ? info->Size : 0;
	}

	// iterating over all files
	virtual int NumFiles() const
	{
		return FileInfos.Num();
	}

	virtual const char* FileName(int i)
	{
		FPakEntry* info = &FileInfos[i];
		LastInfo = info;
		return info->Name;
	}

	virtual FArchive* CreateReader(const char* name)
	{
		const FPakEntry* info = FindFile(name);
		if (!info) return NULL;
		return new FPakFile(info, Reader);
	}

protected:
	FArchive*			Reader;
	TArray<FPakEntry>	FileInfos;
	FPakEntry*			LastInfo;			// cached last accessed file info, simple optimization

	const FPakEntry* FindFile(const char* name)
	{
		if (LastInfo && !stricmp(LastInfo->Name, name))
			return LastInfo;

		for (int i = 0; i < FileInfos.Num(); i++)
		{
			FPakEntry* info = &FileInfos[i];
			if (!stricmp(info->Name, name))
			{
				LastInfo = info;
				return info;
			}
		}
		return NULL;
	}
};


#endif // UNREAL4

#endif // __UNARCHIVEPAK_H__
