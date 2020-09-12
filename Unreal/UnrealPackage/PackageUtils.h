#ifndef __PACKAGE_UTILS_H__
#define __PACKAGE_UTILS_H__


class UnPackage;
extern TArray<UnPackage*> GFullyLoadedPackages;

// Virtual interface which could be used for progress indication.
class IProgressCallback
{
public:
	// Stop operating when these functions returns 'false'.
	virtual bool Progress(const char* package, int index, int total)
	{
		return true;
	}
	virtual bool Tick()
	{
		return true;
	}
};


bool LoadWholePackage(UnPackage* Package, IProgressCallback* progress = NULL);
void ReleaseAllObjects();


// Package scanner

struct FileInfo
{
	int		Ver;
	int		LicVer;
	int		Count;
	char	FileName[512];
};

bool ScanPackageVersions(TArray<FileInfo>& info, IProgressCallback* progress = NULL);

bool ScanContent(const TArray<const CGameFileInfo*>& Packages, IProgressCallback* Progress = NULL);


// Class statistics

struct ClassStats
{
	const char*	Name;
	int			Count;

	ClassStats()
	{}

	ClassStats(const char* name)
	:	Name(name)
	,	Count(0)
	{}
};

void CollectPackageStats(const TArray<UnPackage*> &Packages, TArray<ClassStats>& Stats);


#endif // __PACKAGE_UTILS_H__
