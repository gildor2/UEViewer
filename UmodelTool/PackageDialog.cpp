#include "BaseDialog.h"

#if HAS_UI

#include "PackageDialog.h"
#include "PackageScanDialog.h"
#include "ProgressDialog.h"
#include "AboutDialog.h"

#include "UnPackage.h"


/*-----------------------------------------------------------------------------
	Main UI code
-----------------------------------------------------------------------------*/

UIPackageDialog::UIPackageDialog()
:	DirectorySelected(false)
,	ContentScanned(false)
,	UseFlatView(false)
{}

UIPackageDialog::EResult UIPackageDialog::Show()
{
	ModalResult = SHOW;
	if (!ShowModal("Choose a package to open", 500, 200))
		return CANCEL;

	UpdateSelectedPackage();

	// controls are not released automatically when dialog closed to be able to
	// poll them for user selection; release the memory now
	ReleaseControls();

	return ModalResult;
}

void UIPackageDialog::SelectPackage(const char* name)
{
	SelectedPackages.Empty();
	SelectedPackages.AddItem(name);
	SelectDirFromFilename(name);
}

static bool PackageListEnum(const CGameFileInfo *file, TArray<const CGameFileInfo*> &param)
{
	param.AddItem(file);
	return true;
}

enum
{
	COLUMN_Name,
	COLUMN_NumSkel,
	COLUMN_NumStat,
	COLUMN_NumAnim,
	COLUMN_NumTex,
	COLUMN_Size,
	COLUMN_Count
};

void UIPackageDialog::InitUI()
{
	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UICheckbox, "Flat view", &UseFlatView)
				.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnFlatViewChanged, this))
			+ NewControl(UISpacer)
			+ NewControl(UILabel, "Filter:")
				.SetY(2)
				.SetAutoSize()
			+ NewControl(UITextEdit, &PackageFilter)
				.SetWidth(120)
				.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnFilterTextChanged, this))
		]
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
					.UseFolderIcons()
					.SetItemHeight(20)
					.Expose(PackageTree)
				+ NewControl(UISpacer)
				+ NewControl(UIMulticolumnListbox, COLUMN_Count)
					.SetHeight(-1)
					.SetSelChangedCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageSelected, this))
					.SetDblClickCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageDblClick, this))
					.Expose(PackageListbox)
					.AllowMultiselect()
					//?? right-align text in numeric columns
					.AddColumn("Package name")
					.AddColumn("Skel", 35, TA_Right)
					.AddColumn("Stat", 35, TA_Right)
					.AddColumn("Anim", 35, TA_Right)
					.AddColumn("Tex",  35, TA_Right)
					.AddColumn("Size, Kb", 70, TA_Right)
			]
			// page 1: single ListBox
			+ NewControl(UIMulticolumnListbox, COLUMN_Count)
				.SetHeight(-1)
				.SetSelChangedCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageSelected, this))
				.SetDblClickCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageDblClick, this))
				.Expose(FlatPackageList)
				.AllowMultiselect()
				//?? right-align text in numeric columns
				.AddColumn("Package name")
				.AddColumn("Skel", 35, TA_Right)
				.AddColumn("Stat", 35, TA_Right)
				.AddColumn("Anim", 35, TA_Right)
				.AddColumn("Tex",  35, TA_Right)
				.AddColumn("Size, Kb", 70, TA_Right)
		]
	];

	if (!Packages.Num())
	{
		// not scanned yet
		appEnumGameFiles(PackageListEnum, Packages);
	}

	// add paths of all found packages
	if (SelectedPackages.Num()) DirectorySelected = true;
	int selectedPathLen = 1024; // something large
	char prevPath[512], path[512];
	prevPath[0] = 0;
	for (int i = 0; i < Packages.Num(); i++)
	{
		appStrncpyz(path, Packages[i]->RelativeName, ARRAY_COUNT(path));
		char* s = strrchr(path, '/');
		if (s)
		{
			*s = 0;
			// simple optimization - avoid calling PackageTree->AddItem() too frequently (assume package list is sorted)
			if (!strcmp(prevPath, path)) continue;
			strcpy(prevPath, path);
			// add a directory to TreeView
			PackageTree->AddItem(path);
		}
		if (!DirectorySelected)
		{
			int pathLen = s ? s - path : 0;
			if (pathLen < selectedPathLen)
			{
				// set selection to the first directory
				SelectedDir = s ? path : "";
				selectedPathLen = pathLen;
			}
		}
	}

	// "Tools" menu
	UIMenu* toolsMenu = new UIMenu;
	(*toolsMenu)
	[
		NewMenuItem("Scan content")
		.Enable(!ContentScanned)
		.Expose(ScanContentMenu)
		.SetCallback(BIND_MEM_CB(&UIPackageDialog::ScanContent, this))
		+ NewMenuItem("Scan versions")
		.SetCallback(BIND_FREE_CB(&ShowPackageScanDialog))
		+ NewMenuSeparator()
		+ NewMenuItem("About UModel")
		.SetCallback(BIND_FREE_CB(&UIAboutDialog::Show))
	];

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UILabel, "Hint: you may open this dialog at any time by pressing \"O\"")
		+ NewControl(UIMenuButton, "Tools")
		.SetWidth(80)
		.SetMenu(toolsMenu)
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Open")
			.SetWidth(80)
			.Enable(false)
			.Expose(OkButton)
			.SetOK()
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Export")
			.SetWidth(80)
			.Enable(false)
			.Expose(ExportButton)
			.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnExportClicked, this))
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Cancel")
			.SetWidth(80)
			.SetCancel()
	];

	UpdateFlatMode();
}

/*-----------------------------------------------------------------------------
	Support for tree and flat package lists
-----------------------------------------------------------------------------*/

void UIPackageDialog::UpdateSelectedPackage()
{
	SelectedPackages.FastEmpty();

	if (!UseFlatView)
	{
		// get package name from directory + name
		for (int selIndex = 0; selIndex < PackageListbox->GetSelectionCount(); selIndex++)
		{
			const char* pkgInDir = PackageListbox->GetItem(PackageListbox->GetSelectionIndex(selIndex));
			const char* dir = *SelectedDir;
			FString* newPackageName = new (SelectedPackages) FString;
			if (dir[0])
			{
				char buffer[512];
				appSprintf(ARRAY_ARG(buffer), "%s/%s", dir, pkgInDir);
				*newPackageName = buffer;
			}
			else
			{
				*newPackageName = pkgInDir;
			}
		}
	}
	else
	{
		// use flat list, with relative filename (including path)
		for (int selIndex = 0; selIndex < FlatPackageList->GetSelectionCount(); selIndex++)
		{
			FString* newPackageName = new (SelectedPackages) FString;
			const char* text = FlatPackageList->GetItem(FlatPackageList->GetSelectionIndex(selIndex));
			*newPackageName = text;
			if (selIndex == 0)
				SelectDirFromFilename(text);
		}
	}
}

void UIPackageDialog::SelectDirFromFilename(const char* filename)
{
	// extract a directory name from 1st package name
	char buffer[512];
	appStrncpyz(buffer, filename, ARRAY_COUNT(buffer));
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

void UIPackageDialog::OnTreeItemSelected(UITreeView* sender, const char* text)
{
	PackageListbox->RemoveAllItems();
	SelectedDir = text;
	DirectorySelected = true;

	const char* filter = *PackageFilter;
	if (!filter[0]) filter = NULL;

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
			const char* packageName = s ? s : buffer;
			if (!filter || appStristr(packageName, filter))
			{
				// this package is in selected directory
				AddPackageToList(PackageListbox, package, true);
			}
		}
	}
}

void UIPackageDialog::FillFlatPackageList()
{
	FlatPackageList->RemoveAllItems();

	const char* filter = *PackageFilter;
	if (!filter[0]) filter = NULL;

	for (int i = 0; i < Packages.Num(); i++)
	{
		const CGameFileInfo* package = Packages[i];
		if (!filter || appStristr(package->RelativeName, filter))
			AddPackageToList(FlatPackageList, package, false);
	}
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
	// object counts
	if (package->PackageScanned)
	{
#define ADD_COLUMN(ColumnEnum, Value)	\
		if (Value)						\
		{								\
			appSprintf(ARRAY_ARG(buf), "%d", Value); \
			listbox->AddSubItem(index, ColumnEnum, buf); \
		}
		ADD_COLUMN(COLUMN_NumSkel, package->NumSkeletalMeshes);
		ADD_COLUMN(COLUMN_NumStat, package->NumStaticMeshes);
		ADD_COLUMN(COLUMN_NumAnim, package->NumAnimations);
		ADD_COLUMN(COLUMN_NumTex,  package->NumTextures);
#undef ADD_COLUMN
	}
	// size
	appSprintf(ARRAY_ARG(buf), "%d", package->SizeInKb);
	listbox->AddSubItem(index, COLUMN_Size, buf);
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
#if 0
	appPrintf("Selected packages:\n");
	for (int i = 0; i < SelectedPackages.Num(); i++) appPrintf("  %s\n", *SelectedPackages[i]);
#endif
	if (UseFlatView)
	{
		// switching to flat list
		PackageListbox->RemoveAllItems();
		FillFlatPackageList();
		// select item which was active in tree+list
		FlatPackageList->UnselectAllItems();
		for (int i = 0; i < SelectedPackages.Num(); i++)
			FlatPackageList->SelectItem(SelectedPackages[i], true);
		// update buttons enable state
		OnPackageSelected(FlatPackageList);
	}
	else
	{
		// switching to tree+list
		FlatPackageList->RemoveAllItems();
		PackageTree->SelectItem(*SelectedDir);
		OnTreeItemSelected(PackageTree, *SelectedDir);
		// select directory and package
		PackageListbox->UnselectAllItems();
		for (int i = 0; i < SelectedPackages.Num(); i++)
		{
			const char *s = SelectedPackages[i];
			const char* s2 = strrchr(s, '/');	// strip path
			if (s2) s = s2+1;
			//!! todo: compare string between [s,s2] with SelectedDir, add only when strings are equal
			PackageListbox->SelectItem(s, true);
		}
		// update buttons enable state
		OnPackageSelected(PackageListbox);
	}
	FlatViewPager->SetActivePage(UseFlatView ? 1 : 0);
}

void UIPackageDialog::OnFilterTextChanged(UITextEdit* sender, const char* text)
{
	// re-filter lists
	UpdateSelectedPackage();
	UpdateFlatMode();
}

/*-----------------------------------------------------------------------------
	Content tools
-----------------------------------------------------------------------------*/

static void ScanPackageExports(UnPackage* package, CGameFileInfo* file)
{
	for (int idx = 0; idx < package->Summary.ExportCount; idx++)
	{
		const char* ObjectClass = package->GetObjectName(package->GetExport(idx).ClassIndex);

		if (!stricmp(ObjectClass, "SkeletalMesh"))
			file->NumSkeletalMeshes++;
		else if (!stricmp(ObjectClass, "StaticMesh"))
			file->NumStaticMeshes++;
		else if (!stricmp(ObjectClass, "Animation") || !stricmp(ObjectClass, "AnimSequence")) // whole AnimSet for UE2 and number of sequences for UE3+
			file->NumAnimations++;
		else if (!strnicmp(ObjectClass, "Texture", 7))
			file->NumTextures++;
	}
}


void UIPackageDialog::ScanContent()
{
	UIProgressDialog progress;
	progress.Show("Scanning packages");
	progress.SetDescription("Scanning package");

	bool cancelled = false;
	for (int i = 0; i < Packages.Num(); i++)
	{
		CGameFileInfo* file = const_cast<CGameFileInfo*>(Packages[i]);		// we'll modify this structure here
		if (file->PackageScanned) continue;

		if (!progress.Progress(file->RelativeName, i, GNumPackageFiles))
		{
			cancelled = true;
			break;
		}
		UnPackage* package = UnPackage::LoadPackage(file->RelativeName);	// should always return non-NULL
		file->PackageScanned = true;
		if (!package) continue;		// should not happen

		ScanPackageExports(package, file);
	}

	progress.CloseDialog();
	if (cancelled) return;
	ContentScanned = true;

	// finished - no needs to perform scan again, disable button
	ScanContentMenu->Enable(false);

	// update package list with new data
	UpdateSelectedPackage();
	UpdateFlatMode();
}


/*-----------------------------------------------------------------------------
	Miscellaneous UI callbacks
-----------------------------------------------------------------------------*/

void UIPackageDialog::OnPackageSelected(UIMulticolumnListbox* sender)
{
	bool enableButtons = (sender->GetSelectionCount() > 0);
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
