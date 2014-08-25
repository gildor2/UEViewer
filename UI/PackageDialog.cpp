#include "BaseDialog.h"
#include "PackageDialog.h"

/*!! TODO, remaining:
- doubleclick in listbox -> open
*/

#if HAS_UI

FString UIPackageDialog::SelectedPackage;

bool UIPackageDialog::Show()
{
	if (!ShowDialog("Choose a package to open", 400, 200))
		return false;
	const char* pkgInDir = PackageListbox->GetSelectionText();
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
			+ NewControl(UIListbox)
			.SetHeight(-1)
			.Expose(PackageListbox)
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
//!!		PackageListbox->AddItem(packageList[i]->RelativeName);
	}

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UISpacer, -1)
		+ NewControl(UIButton, "OK")
		.SetWidth(EncodeWidth(0.2f))
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
		char buffer[512];
		appStrncpyz(buffer, Packages[i]->RelativeName, ARRAY_COUNT(buffer));
		char* s = strrchr(buffer, '/');
		if (s) *s++ = 0;
		if ((!s && !text[0]) ||					// root directory
			(s && !strcmp(buffer, text)))		// other directory
		{
			// this package is in selected directory
			PackageListbox->AddItem(s ? s : buffer);
		}
	}
}


#endif // HAS_UI
