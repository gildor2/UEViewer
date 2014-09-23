#ifndef __PACKAGE_UTILS_H__
#define __PACKAGE_UTILS_H__

class UnPackage;
#if HAS_UI
class UIProgressDialog;
#endif

extern TArray<UnPackage*> GFullyLoadedPackages;

#if HAS_UI
bool LoadWholePackage(UnPackage* Package, UIProgressDialog* progress = NULL);
#else
bool LoadWholePackage(UnPackage* Package);
#endif
void ReleaseAllObjects();


// Package scanner

struct FileInfo
{
	int		Ver;
	int		LicVer;
	int		Count;
	char	FileName[512];
};

void ScanPackages(TArray<FileInfo>& info);


#endif // __PACKAGE_UTILS_H__
