#ifndef __PACKAGE_DIALOG_H__
#define __PACKAGE_DIALOG_H__

#if HAS_UI

class UIPackageDialog : public UIBaseDialog
{
public:
	UIPackageDialog();

	enum EResult
	{
		SHOW,
		EXPORT,
		CANCEL,
	};

	EResult Show();
	void SelectPackage(const char* name);

	TArray<FString>	SelectedPackages;

protected:
	UIPageControl*	FlatViewPager;
	UITreeView*		PackageTree;
	UIMulticolumnListbox* PackageListbox;
	UIMulticolumnListbox* FlatPackageList;
	UIButton*		OkButton;
	UIButton*		ExportButton;
	UIButton*		ScanContentButton;

	EResult			ModalResult;
	bool			UseFlatView;
	bool			DirectorySelected;
	FStaticString<64>  PackageFilter;
	FStaticString<256> SelectedDir;

	typedef TArray<const CGameFileInfo*> PackageList;
	PackageList		Packages;

	void OnTreeItemSelected(UITreeView* sender, const char* text);
	void OnPackageSelected(UIMulticolumnListbox* sender);
	void OnFlatViewChanged(UICheckbox* sender, bool value);
	void OnPackageDblClick(UIMulticolumnListbox* sender, int value);
	void OnExportClicked(UIButton* sender);
	void OnFilterTextChanged(UITextEdit* sender, const char* text);

	void ScanContent();

	void UpdateSelectedPackage();
	void SelectDirFromFilename(const char* filename);
	void UpdateFlatMode();

	void FillFlatPackageList();
	void AddPackageToList(UIMulticolumnListbox* listbox, const CGameFileInfo* package, bool stripPath);

	virtual void InitUI();
};

#endif // HAS_UI

#endif // __PACKAGE_DIALOG_H__
