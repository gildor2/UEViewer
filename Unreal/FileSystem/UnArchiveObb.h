#ifndef __UNARCHIVE_OBB_H__
#define __UNARCHIVE_OBB_H__

#if SUPPORT_ANDROID


struct FObbEntry
{
	int64		Pos;
	int32		Size;
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

	virtual bool AttachReader(FArchive* reader, FString& error)
	{
		guard(FObbVFS::ReadDirectory);

		char signature[13];
		reader->Serialize(signature, 13);
		if (memcmp(signature, "UE3AndroidOBB", 13) != 0) return false; // bad signature

		// this file looks correct, store 'reader'
		Reader = reader;

		int32 count;
		*Reader << count;
		FileInfos.AddZeroed(count);
		Reserve(count);

		for (int i = 0; i < count; i++)
		{
			FObbEntry& E = FileInfos[i];
			// file name
			FStaticString<MAX_PACKAGE_PATH> FileName;
			*Reader << FileName;
			// process name
			for (char& c : FileName.GetDataArray())
			{
				if (c == '\\') c = '/';
			}
			const char* name = *FileName;
			if (name[0] == '.' && name[1] == '.' && name[2] == '/') name += 3;	// skip "../"
			// other fields
			*Reader << E.Pos << E.Size;

			// Register file in global FS
			CRegisterFileInfo reg;
			reg.Filename = name;
			reg.Size = E.Size;
			reg.IndexInArchive = i;
			RegisterFile(reg);
		}

		return true;

		unguard;
	}

	virtual FArchive* CreateReader(int index)
	{
		guard(FObbVFS::CreateReader);
		const FObbEntry& info = FileInfos[index];
		return new FObbFile(&info, Reader);
		unguard;
	}

protected:
	FString				Filename;
	FArchive*			Reader;
	TArray<FObbEntry>	FileInfos;
	FObbEntry*			LastInfo;			// cached last accessed file info, simple optimization
};


#endif // SUPPORT_ANDROID

#endif // __UNARCHIVE_OBB_H__
