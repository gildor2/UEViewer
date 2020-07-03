#ifndef __GAME_FILE_SYSTEM_H__
#define __GAME_FILE_SYSTEM_H__

// Register a file located in OS file system. File could be an archive (VFS container).
void appRegisterGameFile(const char *FullName);

struct CRegisterFileInfo
{
	const char* Filename;
	int64 Size;
	int IndexInArchive;

	CRegisterFileInfo()
	: Filename(NULL)
	, Size(0)
	, IndexInArchive(-1)
	{}
};

// Low-level function. Register a file inside VFS (parentVFS may be null). No VFS check is performed here.
CGameFileInfo* appRegisterGameFileInfo(FVirtualFileSystem* parentVfs, const CRegisterFileInfo& RegisterInfo);

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

//	// Functions for iteration over all stored files
//	virtual int NumFiles() const = 0;
//	virtual const char* FileName(int i) = 0;
//	virtual int GetFileSize(const char* name) = 0;

	// Reserve space for 'count' files
	void Reserve(int count);

	CGameFileInfo* RegisterFile(CRegisterFileInfo& info)
	{
		guard(FVirtualFileSystem::RegisterFile);
		assert(info.IndexInArchive >= 0);
		return appRegisterGameFileInfo(this, info);
		unguard;
	}
};

#endif // __GAME_FILE_SYSTEM_H__
