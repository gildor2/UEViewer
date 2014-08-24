#include "BaseDialog.h"
#include "PackageDialog.h"


#if HAS_UI

FString UIPackageDialog::SelectedPackage;

bool UIPackageDialog::Show()
{
	if (!ShowDialog("Choose a package to open", 400, 200))
		return false;
	SelectedPackage = PackageListbox->GetSelectionText();
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
	PackageListbox = new UIListbox();
	PackageListbox->SetHeight(500);
	Add(PackageListbox);

	//!! make static for accessing it later
	TArray<const CGameFileInfo*> packageList;
	pPackageList = &packageList;
	appEnumGameFiles(PackageListEnum);

	for (int i = 0; i < packageList.Num(); i++)
		PackageListbox->AddItem(packageList[i]->RelativeName);

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


#endif // HAS_UI
