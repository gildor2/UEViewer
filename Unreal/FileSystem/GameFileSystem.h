#ifndef __GAME_FILE_SYSTEM_H__
#define __GAME_FILE_SYSTEM_H__

struct CRegisterFileInfo
{
	const char* Filename;
	const char* Path;
	int64 Size;
	int IndexInArchive;

	CRegisterFileInfo()
	: Filename(NULL)
	, Path(NULL)
	, Size(0)
	, IndexInArchive(-1)
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
		return CGameFileInfo::Register(this, info);
		unguard;
	}
};

#endif // __GAME_FILE_SYSTEM_H__
