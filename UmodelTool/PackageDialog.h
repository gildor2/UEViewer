#ifndef __PACKAGE_DIALOG_H__
#define __PACKAGE_DIALOG_H__

class UIPackageList;

class UIPackageDialog : public UIBaseDialog
{
public:
	UIPackageDialog();

	typedef TArray<const CGameFileInfo*> PackageList;

	enum EResult
	{
		OPEN,
		APPEND,
		EXPORT,
		CANCEL,
	};

	EResult Show();
	void SelectPackage(UnPackage* package);

	PackageList		SelectedPackages;

protected:
	UIPageControl*	FlatViewPager;
	UITreeView*		PackageTree;
	UIPackageList*	PackageListbox;
	UIPackageList*	FlatPackageList;
	UIMenuButton*	OpenButton;
	UIButton*		ExportButton;
	UIMenuItem*		ScanContentMenu;
	UIMenuItem*		SavePackagesMenu;
	UIMenu*			TreeMenu;
	UIMenu*			ListMenu;

	EResult			ModalResult;
	bool			UseFlatView;
	bool			DirectorySelected;
	bool			ContentScanned;
	// when true, CloseDialog will not reevaluate SelectedPackages
	bool			DontGetSelectedPackages;
	FStaticString<64>  PackageFilter;
	FStaticString<256> SelectedDir;

	int				SortedColumn;
	bool			ReverseSort;

	PackageList		Packages;

	void CloseDialog(EResult Result, bool bDontGetSelectedPackages = false);

	void OnBeforeListMenuPopup();
	void OnTreeItemSelected(UITreeView* sender, const char* text);
	void OnPackageSelected(UIMulticolumnListbox* sender);
	void OnFlatViewChanged(UICheckbox* sender, bool value);
	void OnPackageDblClick(UIMulticolumnListbox* sender, int value);
	void OnColumnClick(UIMulticolumnListbox* sender, int column);
	void OnOpenFolderClicked();
	void OnOpenAppendFolderClicked();
	void OnExportFolderClicked();
	void OnFilterTextChanged(UITextEdit* sender, const char* text);

	bool ScanContent(const PackageList& packageList);
	void SavePackages();
	void SaveFolderPackages();
	void CopyPackagePaths();

	void UpdateSelectedPackages();
	void GetPackagesForSelectedFolder(PackageList& OutPackages);
	void RefreshPackageListbox();

	// Package sort stuff
	void SortPackages();
	// Sort helpers
	static void SortPackages(PackageList& List, int Column, bool Reverse);
	void UpdateUIAfterSort();

	UIPackageList& CreatePackageListControl(bool StripPath);

	virtual void InitUI();
};

#endif // __PACKAGE_DIALOG_H__
