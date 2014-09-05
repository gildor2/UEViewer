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
	virtual void InitUI();

	FString			SelectedPackage;

protected:
	void OnTreeItemSelected(UITreeView* sender, const char* text);
	void OnPackageSelected(UIMulticolumnListbox* sender, int value);
	void OnFlatViewChanged(UICheckbox* sender, bool value);
	void OnPackageDblClick(UIMulticolumnListbox* sender, int value);
	void OnExportClicked(UIButton* sender);

	void UpdateSelectedPackage();
	void UpdateFlatMode();

	void FillFlatPackageList();
	void AddPackageToList(UIMulticolumnListbox* listbox, const CGameFileInfo* package, bool stripPath);

	UIPageControl*	FlatViewPager;
	UITreeView*		PackageTree;
	UIMulticolumnListbox* PackageListbox;
	UIMulticolumnListbox* FlatPackageList;
	UIButton*		OkButton;
	UIButton*		ExportButton;

	bool			UseFlatView;
	bool			DirectorySelected;
	FString			SelectedDir;
	EResult			ModalResult;

	typedef TArray<const CGameFileInfo*> PackageList;
	PackageList		Packages;
};

#endif // HAS_UI

#endif // __PACKAGE_DIALOG_H__
