#include "BaseDialog.h"
#include "PackageDialog.h"


#if HAS_UI

/*-----------------------------------------------------------------------------
	Main UI code
-----------------------------------------------------------------------------*/

UIPackageDialog::UIPackageDialog()
:	DirectorySelected(false)
,	UseFlatView(false)
{}

UIPackageDialog::EResult UIPackageDialog::Show()
{
	ModalResult = SHOW;
	if (!ShowDialog("Choose a package to open", 400, 200))
		return CANCEL;

	UpdateSelectedPackage();

	// controls are not released automatically when dialog closed to be able to
	// poll them for user selection; release the memory now
	ReleaseControls();

	return ModalResult;
}

static TArray<const CGameFileInfo*> *pPackageList;

static bool PackageListEnum(const CGameFileInfo *file)
{
	pPackageList->AddItem(file);
	return true;
}

void UIPackageDialog::InitUI()
{
	(*this)
	[
		NewControl(UICheckbox, "Flat view", &UseFlatView)
			.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnFlatViewChanged, this))
		+ NewControl(UIPageControl)
			.Expose(FlatViewPager)
			.SetHeight(500)
		[
			// page 0: TreeView + ListBox
			NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			.SetHeight(500)
			[
				NewControl(UITreeView)
					.SetRootLabel("Game")
					.SetWidth(EncodeWidth(0.3f))
					.SetHeight(-1)
					.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnTreeItemSelected, this))
					.Expose(PackageTree)
				+ NewControl(UISpacer)
				+ NewControl(UIMulticolumnListbox, 2)
					.SetHeight(-1)
					.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageSelected, this))
					.SetDblClickCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageDblClick, this))
					.Expose(PackageListbox)
					.AddColumn("Package name")
					.AddColumn("Size, Kb", 70)		//?? right-align text in column
			]
			// page 1: lingle ListBox
			+ NewControl(UIMulticolumnListbox, 2)
				.SetHeight(-1)
				.Expose(FlatPackageList)
				.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageSelected, this))
				.SetDblClickCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageDblClick, this))
				.AddColumn("Package name")
				.AddColumn("Size, Kb", 70)			//?? right-align text in column
		]
	];

	if (!Packages.Num())
	{
		// not scanned yet
		pPackageList = &Packages;
		appEnumGameFiles(PackageListEnum);
	}

	// add paths of all found packages
	int selectedPathLen = 1024; // something large
	for (int i = 0; i < Packages.Num(); i++)
	{
		char buffer[512];
		appStrncpyz(buffer, Packages[i]->RelativeName, ARRAY_COUNT(buffer));
		char* s = strrchr(buffer, '/');
		if (s)
		{
			*s = 0;
			PackageTree->AddItem(buffer);
		}
		if (!DirectorySelected)
		{
			int pathLen = s ? s - buffer : 0;
			if (pathLen < selectedPathLen)
			{
				// set selection to the first directory
				SelectedDir = s ? buffer : "";
				selectedPathLen = pathLen;
			}
		}
	}

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UILabel, "Hint: you may open this dialog at any time by pressing \"O\"")
		+ NewControl(UIButton, "Open")
			.SetWidth(EncodeWidth(0.15f))
			.Enable(false)
			.Expose(OkButton)
			.SetOK()
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Export")
			.SetWidth(EncodeWidth(0.15f))
			.Enable(false)
			.Expose(ExportButton)
			.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnExportClicked, this))
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Cancel")
			.SetWidth(EncodeWidth(0.15f))
			.SetCancel()
	];

	UpdateFlatMode();
}

/*-----------------------------------------------------------------------------
	Support for tree and flat package lists
-----------------------------------------------------------------------------*/

void UIPackageDialog::UpdateSelectedPackage()
{
	if (!UseFlatView)
	{
		// get package name from directory + name
		int selectedPackageIndex = PackageListbox->GetSelectionIndex();
		if (selectedPackageIndex < 0)
		{
			SelectedPackage = "";
			return;
		}

		const char* pkgInDir = PackageListbox->GetItem(selectedPackageIndex);
		const char* dir = *SelectedDir;
		if (dir[0])
		{
			char buffer[512];
			appSprintf(ARRAY_ARG(buffer), "%s/%s", dir, pkgInDir);
			SelectedPackage = buffer;
		}
		else
		{
			SelectedPackage = pkgInDir;
		}
	}
	else
	{
		// use flat list, with relative filename (including path)
		int selectedPackageIndex = FlatPackageList->GetSelectionIndex();
		if (selectedPackageIndex < 0)
		{
			SelectedPackage = "";
			return;
		}
		SelectedPackage = FlatPackageList->GetItem(selectedPackageIndex);
		// split SelectedPackage to SelectedDir and SelectedPackage
		char buffer[512];
		appStrncpyz(buffer, *SelectedPackage, ARRAY_COUNT(buffer));
		char* s = strrchr(buffer, '/');
		if (s)
		{
			*s = 0;
			SelectedDir = buffer;
		}
		else
		{
			SelectedDir = "";
		}
	}
}

void UIPackageDialog::OnTreeItemSelected(UITreeView* sender, const char* text)
{
	PackageListbox->RemoveAllItems();
	SelectedDir = text;
	DirectorySelected = true;

	for (int i = 0; i < Packages.Num(); i++)
	{
		const CGameFileInfo* package = Packages[i];
		char buffer[512];
		appStrncpyz(buffer, package->RelativeName, ARRAY_COUNT(buffer));
		char* s = strrchr(buffer, '/');
		if (s) *s++ = 0;
		if ((!s && !text[0]) ||					// root directory
			(s && !strcmp(buffer, text)))		// other directory
		{
			// this package is in selected directory
			AddPackageToList(PackageListbox, package, true);
		}
	}
}

void UIPackageDialog::FillFlatPackageList()
{
	FlatPackageList->RemoveAllItems();
	for (int i = 0; i < Packages.Num(); i++)
		AddPackageToList(FlatPackageList, Packages[i], false);
}

void UIPackageDialog::AddPackageToList(UIMulticolumnListbox* listbox, const CGameFileInfo* package, bool stripPath)
{
	const char* s = package->RelativeName;
	if (stripPath)
	{
		const char* s2 = strrchr(s, '/');
		if (s2) s = s2 + 1;
	}
	int index = listbox->AddItem(s);
	char buf[32];
	appSprintf(ARRAY_ARG(buf), "%d", package->SizeInKb);
	listbox->AddSubItem(index, 1, buf);
}

void UIPackageDialog::OnFlatViewChanged(UICheckbox* sender, bool value)
{
	UseFlatView = !UseFlatView;
	UpdateSelectedPackage();
	UseFlatView = !UseFlatView;

	UpdateFlatMode();
}

void UIPackageDialog::UpdateFlatMode()
{
	if (UseFlatView)
	{
		PackageListbox->RemoveAllItems();
		// switching to flat list
		FillFlatPackageList();
		// select item which was active in tree+list
		if (!SelectedPackage.IsEmpty())
		{
			FlatPackageList->SelectItem(*SelectedPackage);
			// update buttons enable state
			OnPackageSelected(FlatPackageList, FlatPackageList->GetSelectionIndex());
		}
	}
	else
	{
		FlatPackageList->RemoveAllItems();
		// switching to tree+list
		// select directory and package
		PackageTree->SelectItem(*SelectedDir);
		OnTreeItemSelected(PackageTree, *SelectedDir);
		if (!SelectedPackage.IsEmpty())
		{
			const char *s = *SelectedPackage;
			const char* s2 = strrchr(s, '/');	// strip path
			if (s2) s = s2+1;
			PackageListbox->SelectItem(s);
			// update buttons enable state
			OnPackageSelected(PackageListbox, PackageListbox->GetSelectionIndex());
		}
	}
	FlatViewPager->SetActivePage(UseFlatView ? 1 : 0);
}

/*-----------------------------------------------------------------------------
	Miscellaneous UI callbacks
-----------------------------------------------------------------------------*/

void UIPackageDialog::OnPackageSelected(UIMulticolumnListbox* sender, int value)
{
	bool enableButtons = (value != -1);
	OkButton->Enable(enableButtons);
	ExportButton->Enable(enableButtons);
}

void UIPackageDialog::OnPackageDblClick(UIMulticolumnListbox* sender, int value)
{
	if (value != -1)
		CloseDialog();
}

void UIPackageDialog::OnExportClicked(UIButton* sender)
{
	ModalResult = EXPORT;
	CloseDialog();
}


#endif // HAS_UI
