#ifndef __IOSTORE_FILE_SYSTEM_H__
#define __IOSTORE_FILE_SYSTEM_H__

#if UNREAL4

class FIOStoreFileSystem : public FVirtualFileSystem
{
public:
	FIOStoreFileSystem(const char* InFilename)
	:	Filename(InFilename)
	,	Reader(NULL)
	{}

	~FIOStoreFileSystem()
	{
		delete Reader;
	}

	virtual bool AttachReader(FArchive* reader, FString& error);

	virtual FArchive* CreateReader(int index);

protected:
	void WalkDirectoryTreeRecursive(struct FIoDirectoryIndexResource& IndexResource, int DirectoryIndex, const FString& ParentDirectory);

	FString				Filename;
	FArchive*			Reader;
};

#endif // UNREAL4

#endif // __IOSTORE_FILE_SYSTEM_H__
