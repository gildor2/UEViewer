#ifndef __UMODEL_COMMANDS_H__
#define __UMODEL_COMMANDS_H__

class UnPackage;
class IProgressCallback;

// Export all loaded objects.
bool ExportObjects(const TArray<UObject*> *Objects, IProgressCallback* progress = NULL);

// Export everything from provided package list.
bool ExportPackages(const TArray<UnPackage*>& Packages, IProgressCallback* Progress = NULL);

void DisplayPackageStats(const TArray<UnPackage*> &Packages);

void SavePackages(const TArray<const CGameFileInfo*>& Packages, IProgressCallback* Progress = NULL);

#endif // __UMODEL_COMMANDS_H__
