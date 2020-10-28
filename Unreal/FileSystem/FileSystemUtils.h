#ifndef __FILE_SYSTEM_UTILS_H__
#define __FILE_SYSTEM_UTILS_H__

void ValidateMountPoint(FString& MountPoint, const FString& ContextFilename);

void CompactFilePath(FString& Path);

int32 StringToCompressionMethod(const char* Name);

bool FileRequiresAesKey(bool fatal = true);

#endif // __FILE_SYSTEM_UTILS_H__
