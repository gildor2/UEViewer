#include "BaseDialog.h"
#include "PackageDialog.h"

/*!! TODO, remaining:
- doubleclick in listbox -> open
*/

#if HAS_UI

UIPackageDialog::UIPackageDialog()
:	DirectorySelected(false)
{}

bool UIPackageDialog::Show()
{
	if (!ShowDialog("Choose a package to open", 400, 200))
		return false;
	int selectedPackageIndex = PackageListbox->GetSelectionIndex();
	assert(selectedPackageIndex >= 0);

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
	return true;
}

static TArray<const CGameFileInfo*> *pPackageList;

static bool PackageListEnum(const CGameFileInfo *file)
{
	pPackageList->AddItem(file);
	return true;
}

void UIPackageDialog::InitUI()
{
	UITreeView* tree;
	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		.SetHeight(500)
		[
			NewControl(UITreeView)
			.SetRootLabel("Game root")
			.SetWidth(EncodeWidth(0.3f))
			.SetHeight(-1)
			.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnTreeItemSelected, this))
			.Expose(tree)
			+ NewControl(UISpacer)
			+ NewControl(UIMulticolumnListbox, 2)
			.SetHeight(-1)
			.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageSelected, this))
			.SetDblClickCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageDblClick, this))
			.Expose(PackageListbox)
			.AddColumn("Package name", EncodeWidth(0.7f))
			.AddColumn("Size, Kb")
		]
	];

	if (!Packages.Num())
	{
		// not scanned yet
		pPackageList = &Packages;
		appEnumGameFiles(PackageListEnum);
	}

	// add paths of all found packages
	for (int i = 0; i < Packages.Num(); i++)
	{
		char buffer[512];
		appStrncpyz(buffer, Packages[i]->RelativeName, ARRAY_COUNT(buffer));
		char* s = strrchr(buffer, '/');
		if (s)
		{
			*s = 0;
			tree->AddItem(buffer);
		}
		if (i == 0 && !DirectorySelected)
		{
			// set selection to the first directory
			SelectedDir = s ? buffer : "";
		}
	}

	// select directory and package
	tree->SelectItem(*SelectedDir);
	OnTreeItemSelected(tree, *SelectedDir);
	if (!SelectedPackage.IsEmpty())
	{
		const char *s = *SelectedPackage;
		const char* s2 = strrchr(s, '/');	// strip path
		if (s2) s = s2+1;
		PackageListbox->SelectItem(s);
	}

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UILabel, "Hint: you may open this dialog at any time by pressing \"O\"")
		+ NewControl(UIButton, "OK")
		.SetWidth(EncodeWidth(0.2f))
		.Enable(false)
		.Expose(OkButton)
		.SetOK()
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Cancel")
		.SetWidth(EncodeWidth(0.2f))
		.SetCancel()
	];
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
			int index = PackageListbox->AddItem(s ? s : buffer);
			char buf[32];
			appSprintf(ARRAY_ARG(buf), "%d", package->SizeInKb);
			PackageListbox->AddSubItem(index, 1, buf);
		}
	}
}

void UIPackageDialog::OnPackageSelected(UIMulticolumnListbox* sender, int value)
{
	if (value == -1)
	{
		OkButton->Enable(false);
		return;
	}
	OkButton->Enable(true);
}

void UIPackageDialog::OnPackageDblClick(UIMulticolumnListbox* sender, int value)
{
	if (value != -1)
		CloseDialog();
}


#endif // HAS_UI
