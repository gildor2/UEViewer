#include "Core.h"
#include "UnCore.h"


void ValidateMountPoint(FString& MountPoint, const FString& ContextFilename)
{
	bool badMountPoint = false;
	if (!MountPoint.RemoveFromStart("../../.."))
		badMountPoint = true;
	if (MountPoint[0] != '/' || ( (MountPoint.Len() > 1) && (MountPoint[1] == '.') ))
		badMountPoint = true;

	if (badMountPoint)
	{
		appPrintf("WARNING: \"%s\" has strange mount point \"%s\", mounting to root\n", *ContextFilename, *MountPoint);
		MountPoint = "/";
	}
}

void CompactFilePath(FString& Path)
{
	guard(CompactFilePath);

	if (Path.StartsWith("/Engine/Content"))	// -> /Engine
	{
		Path.RemoveAt(7, 8);
		return;
	}
	if (Path.StartsWith("/Engine/Plugins")) // -> /Plugins
	{
		Path.RemoveAt(0, 7);
		return;
	}

	if (Path[0] != '/')
		return;

	char* delim = strchr(&Path[1], '/');
	if (!delim)
		return;
	if (strncmp(delim, "/Content/", 9) != 0)
		return;

	int pos = delim - &Path[0];
	if (pos > 4)
	{
		// /GameName/Content -> /Game
		int toRemove = pos + 8 - 5;
		Path.RemoveAt(5, toRemove);
		memcpy(&Path[1], "Game", 4);
	}

	unguard;
}

int32 StringToCompressionMethod(const char* Name)
{
	if (!stricmp(Name, "zlib"))
	{
		return COMPRESS_ZLIB;
	}
	else if (!stricmp(Name, "oodle"))
	{
		return COMPRESS_OODLE;
	}
	else if (!stricmp(Name, "lz4"))
	{
		return COMPRESS_LZ4;
	}
	else if (Name[0])
	{
		appPrintf("Warning: unknown compression method name: %s\n", Name);
		return COMPRESS_FIND;
	}
	return 0;
}

#if UNREAL4

bool FileRequiresAesKey(bool fatal)
{
	if ((GAesKeys.Num() == 0) && !UE4EncryptedPak())
	{
		if (fatal)
			appErrorNoLog("AES key is required");
		return false;
	}
	return true;
}

#endif // UNREAL4
