#include "BaseDialog.h"
#include "PackageDialog.h"

/*!! TODO, remaining:
! bug: -path=data => in directory tree will be selected a wrong item
- doubleclick in listbox -> open
*/

#if HAS_UI

FString UIPackageDialog::SelectedPackage;

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
			.Expose(PackageListbox)
			.AddColumn("Package name", EncodeWidth(0.7f))
			.AddColumn("Size, Kb")
		]
	];

	//!! make static for accessing it later
	pPackageList = &Packages;
	appEnumGameFiles(PackageListEnum);

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
	}

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UISpacer, -1)
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


#endif // HAS_UI
