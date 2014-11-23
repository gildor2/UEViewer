#ifndef __GAME_FILE_SYSTEM_H__
#define __GAME_FILE_SYSTEM_H__

class FVirtualFileSystem
{
public:
	virtual ~FVirtualFileSystem()
	{}

	// Attach FArchive which will be used for reading VFS content. This function should
	// scan VFS directory.
	virtual bool AttachReader(FArchive* reader) = 0;
	// Open a file from VFS.
	virtual FArchive* CreateReader(const char* name) = 0;

	// Functions for iteration over all stored files

	virtual int NumFiles() const = 0;
	virtual const char* FileName(int i) = 0;
	virtual int GetFileSize(const char* name) = 0;
};


#endif // __GAME_FILE_SYSTEM_H__
