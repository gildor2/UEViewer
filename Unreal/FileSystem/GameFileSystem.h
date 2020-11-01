#ifndef __GAME_FILE_SYSTEM_H__
#define __GAME_FILE_SYSTEM_H__

struct CRegisterFileInfo
{
	const char* Filename;
	const char* Path;		//todo: probably using `FolderIndex`, so 'Path' could be removed?
	int64 Size;
	uint8 Flags;			// CGameFileInfo::GFI_...
	int IndexInArchive;
	int FolderIndex;

	CRegisterFileInfo()
	: Filename(NULL)
	, Path(NULL)
	, Size(0)
	, Flags(CGameFileInfo::GFI_None)
	, IndexInArchive(-1)
	, FolderIndex(0)
	{}
};

class FVirtualFileSystem
{
public:
	virtual ~FVirtualFileSystem()
	{}

	// Attach FArchive which will be used for reading VFS content. This function should scan
	// VFS directory. If function failed, it should return false and optionally fill error string.
	virtual bool AttachReader(FArchive* reader, FString& error) = 0;
	// Open a file from VFS.
	virtual FArchive* CreateReader(int index) = 0;

	// Reserve space for 'count' files
	void Reserve(int count);

	FORCEINLINE CGameFileInfo* RegisterFile(CRegisterFileInfo& info)
	{
		assert(info.IndexInArchive >= 0);
		return CGameFileInfo::Register(this, info);
	}
};

int RegisterGameFolder(const char* FolderName);

#endif // __GAME_FILE_SYSTEM_H__
