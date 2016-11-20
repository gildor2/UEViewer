#ifndef __UNARCHIVE_OBB_H__
#define __UNARCHIVE_OBB_H__

#if SUPPORT_ANDROID


struct FObbEntry
{
	int64		Pos;
	int			Size;
	const char*	Name;
};


class FObbFile : public FArchive
{
	DECLARE_ARCHIVE(FObbFile, FArchive);
public:
	FObbFile(const FObbEntry* info, FArchive* reader)
	:	Info(info)
	,	Reader(reader)
	{}

	virtual void Serialize(void *data, int size)
	{
		guard(FObbFile::Serialize);
		if (ArStopper > 0 && ArPos + size > ArStopper)
			appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);
		// seek every time in a case if the same 'Reader' was used by different FObbFile
		// (this is a lightweight operation for buffered FArchive)
		Reader->Seek64(Info->Pos + ArPos);
		Reader->Serialize(data, size);
		ArPos += size;
		unguard;
	}

	virtual void Seek(int Pos)
	{
		guard(FObbFile::Seek);
		assert(Pos >= 0 && Pos < Info->Size);
		ArPos = Pos;
		unguard;
	}

	virtual int GetFileSize() const
	{
		return Info->Size;
	}

protected:
	const FObbEntry* Info;
	FArchive*	Reader;
};


class FObbVFS : public FVirtualFileSystem
{
public:
	FObbVFS(const char* InFilename)
	:	LastInfo(NULL)
	,	Reader(NULL)
	,	Filename(InFilename)
	{}

	virtual ~FObbVFS()
	{
		delete Reader;
	}

	virtual bool AttachReader(FArchive* reader)
	{
		guard(FObbVFS::ReadDirectory);

		char signature[13];
		reader->Serialize(signature, 13);
		if (memcmp(signature, "UE3AndroidOBB", 13) != 0) return false; // bad signature

		// this file looks correct, store 'reader'
		Reader = reader;

		int count;
		*Reader << count;
		FileInfos.AddZeroed(count);

		for (int i = 0; i < count; i++)
		{
			FObbEntry& E = FileInfos[i];
			// file name
			char FileName[512];
			int NameLen;
			*Reader << NameLen;
			assert(NameLen < ARRAY_COUNT(FileName));
			Reader->Serialize(FileName, NameLen);
			// process name
			for (char* s = FileName; *s; s++)
			{
				if (*s == '\\') *s = '/';
			}
			const char* s = FileName;
			if (s[0] == '.' && s[1] == '.' && s[2] == '/') s += 3;	// skip "../"
			E.Name = appStrdupPool(s);
			// other fields
			*Reader << E.Pos << E.Size;
		}

		return true;

		unguard;
	}

	virtual int GetFileSize(const char* name)
	{
		const FObbEntry* info = FindFile(name);
		return (info) ? info->Size : 0;
	}

	// iterating over all files
	virtual int NumFiles() const
	{
		return FileInfos.Num();
	}

	virtual const char* FileName(int i)
	{
		FObbEntry* info = &FileInfos[i];
		LastInfo = info;
		return info->Name;
	}

	virtual FArchive* CreateReader(const char* name)
	{
		const FObbEntry* info = FindFile(name);
		if (!info) return NULL;
		return new FObbFile(info, Reader);
	}

protected:
	FString				Filename;
	FArchive*			Reader;
	TArray<FObbEntry>	FileInfos;
	FObbEntry*			LastInfo;			// cached last accessed file info, simple optimization

	const FObbEntry* FindFile(const char* name)
	{
		if (LastInfo && !stricmp(LastInfo->Name, name))
			return LastInfo;

		for (int i = 0; i < FileInfos.Num(); i++)
		{
			FObbEntry* info = &FileInfos[i];
			if (!stricmp(info->Name, name))
			{
				LastInfo = info;
				return info;
			}
		}
		return NULL;
	}
};


#endif // SUPPORT_ANDROID

#endif // __UNARCHIVE_OBB_H__
