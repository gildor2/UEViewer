#ifndef __PACKAGE_DIALOG_H__
#define __PACKAGE_DIALOG_H__

#if HAS_UI

class UIPackageDialog : public UIBaseDialog
{
public:
	bool Show();
	virtual void InitUI();

	UIListbox*		PackageListbox;
	static FString	SelectedPackage;

protected:
	void OnTreeItemSelected(UITreeView* sender, const char* text);

	FString			SelectedDir;

	typedef TArray<const CGameFileInfo*> PackageList;
	PackageList		Packages;
};

#endif // HAS_UI

#endif // __PACKAGE_DIALOG_H__
