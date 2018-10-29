#ifndef __PACKAGE_DIALOG_H__
#define __PACKAGE_DIALOG_H__

#if HAS_UI

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
	FStaticString<64>  PackageFilter;
	FStaticString<256> SelectedDir;

	int				SortedColumn;
	bool			ReverseSort;

	PackageList		Packages;

	void CloseDialog(EResult Result);

	void OnBeforeListMenuPopup();
	void OnTreeItemSelected(UITreeView* sender, const char* text);
	void OnPackageSelected(UIMulticolumnListbox* sender);
	void OnFlatViewChanged(UICheckbox* sender, bool value);
	void OnPackageDblClick(UIMulticolumnListbox* sender, int value);
	void OnColumnClick(UIMulticolumnListbox* sender, int column);
	void OnExportFolderClicked();
	void OnFilterTextChanged(UITextEdit* sender, const char* text);

	void ScanContent();
	void SavePackages();
	void SaveFolderPackages();

	void UpdateSelectedPackages();
	void GetPackagesForSelectedFolder(PackageList& OutPackages);
	void SelectDirFromFilename(const char* filename);
	void RefreshPackageListbox();
	void SortPackages();
	static void SortPackages(PackageList& List, int Column, bool Reverse);

	UIPackageList& CreatePackageListControl(bool StripPath);

	virtual void InitUI();
};

#endif // HAS_UI

#endif // __PACKAGE_DIALOG_H__
