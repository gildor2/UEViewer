#ifndef __GAME_FILE_SYSTEM_H__
#define __GAME_FILE_SYSTEM_H__

class FVirtualFileSystem
{
public:
	virtual ~FVirtualFileSystem()
	{}

	// Attach FArchive which will be used for reading VFS content. This function should scan
	// VFS directory. If function failed, it should return false and optionally fill error string.
	virtual bool AttachReader(FArchive* reader, FString& error) = 0;
	// Open a file from VFS.
	virtual FArchive* CreateReader(const char* name) = 0;

	// Functions for iteration over all stored files

	virtual int NumFiles() const = 0;
	virtual const char* FileName(int i) = 0;
	virtual int GetFileSize(const char* name) = 0;
};

void appRegisterGameFile(const char *FullName, FVirtualFileSystem* parentVfs = NULL);


#endif // __GAME_FILE_SYSTEM_H__
