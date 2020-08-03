#if HAS_UI

#include "BaseDialog.h"
#include "PackageDialog.h"
#include "PackageScanDialog.h"
#include "ProgressDialog.h"
#include "SettingsDialog.h"
#include "AboutDialog.h"

#include "UnPackage.h"
#include "UmodelCommands.h"

#include "Parallel.h"

#define USE_FULLY_VIRTUAL_LIST		1		// disable only for testing, to compare UIMulticolumnListbox behavior in virtual modes

/*-----------------------------------------------------------------------------
	UIPackageList
-----------------------------------------------------------------------------*/

class CFilter
{
public:
	CFilter(const char* value)
	{
		if (value)
		{
			char buffer[1024];
			appStrncpyz(buffer, value, ARRAY_COUNT(buffer));
			char* start = buffer;
			bool shouldBreak = false;
			while (!shouldBreak)
			{
				char* end = start;
				while ((*end != ' ') && (*end != 0))
				{
					end++;
				}
				shouldBreak = (*end == 0);
				*end = 0;
				if (*start != 0)
				{
					Values.Add(start);
				}
				start = end + 1;
			}
		}
	}
	inline bool IsEmpty() const
	{
		return Values.Num() == 0;
	}
	bool Filter(const char* str) const
	{
		for (const FString& Value : Values)
		{
			if (!appStristr(str, *Value))
				return false;
		}
		return true;
	}

private:
	TArray<FString>		Values;
};


class UIPackageList : public UIMulticolumnListbox
{
   	DECLARE_UI_CLASS(UIPackageList, UIMulticolumnListbox);
public:
	bool				StripPath;
	UIPackageDialog::PackageList Packages;

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

	UIPackageList(bool InStripPath)
	:	UIMulticolumnListbox(COLUMN_Count)
	,	StripPath(InStripPath)
	{
		AllowMultiselect();
		SetVirtualMode();
		// Add columns
		//?? right-align text in numeric columns
		AddColumn("Package name");
		AddColumn("Skel", 35, TA_Right);
		AddColumn("Stat", 35, TA_Right);
		AddColumn("Anim", 35, TA_Right);
		AddColumn("Tex",  35, TA_Right);
		AddColumn("Size, Kb", 70, TA_Right);
	#if USE_FULLY_VIRTUAL_LIST
		SetOnGetItemCount(BIND_MEMBER(&UIPackageList::GetItemCountHandler, this));
		SetOnGetItemText(BIND_MEMBER(&UIPackageList::GetItemTextHandler, this));
	#endif
	}

	// Fill package list for selected folder
	void FillPackageList(UIPackageDialog::PackageList& InPackages, const char* directory, const char* packageFilter)
	{
		guard(FillPackageList);

		LockUpdate();

		RemoveAllItems();
		Packages.Empty(256);

		int folderIndex = appGetGameFolderIndex(directory);
		if (folderIndex <= 0)
		{
			UnlockUpdate();
			return;		// there's no files in this folder
		}

		CFilter filter(packageFilter);
		for (const CGameFileInfo* package : InPackages)
		{
			if (package->FolderIndex == folderIndex)
			{
				// This package is in selected directory
				if (filter.IsEmpty())
				{
					AddPackage(package);
				}
				else
				{
					FStaticString<MAX_PACKAGE_PATH> PackageName;
					package->GetCleanName(PackageName);
					if (filter.Filter(*PackageName))
					{
						AddPackage(package);
					}
				}
			}
		}

		UnlockUpdate(); // this will call Repaint()

		unguard;
	}

	// Fill package list for all folders ("flat" mode)
	void FillFlatPackageList(UIPackageDialog::PackageList& InPackages, const char* packageFilter)
	{
		guard(FillFlatPackageList);

		LockUpdate(); // HUGE performance gain. Warning: don't use "return" here without UnlockUpdate()!

		RemoveAllItems();
		Packages.Empty(InPackages.Num());

		CFilter filter(packageFilter);

#if !USE_FULLY_VIRTUAL_LIST
		ReserveItems(InPackages.Num());
#endif
		for (const CGameFileInfo* package : InPackages)
		{
			if (filter.IsEmpty())
			{
				AddPackage(package);
			}
			else
			{
				FStaticString<MAX_PACKAGE_PATH> RelativeName;
				package->GetRelativeName(RelativeName);
				if (filter.Filter(*RelativeName))
					AddPackage(package);
			}
		}

		UnlockUpdate();

		unguard;
	}

	void AddPackage(const CGameFileInfo* package)
	{
		Packages.Add(package);

#if !USE_FULLY_VIRTUAL_LIST
		FStaticString<MAX_PACKAGE_PATH> Buffer;
		if (StripPath)
			package->GetCleanName(Buffer);
		else
			package->GetRelativeName(Buffer);
		int index = AddItem(*Buffer);
		char buf[32];
		// put object count information as subitems
		if (package->IsPackageScanned)
		{
#define ADD_COLUMN(ColumnEnum, Value)		\
			if (Value)						\
			{								\
				appSprintf(ARRAY_ARG(buf), "%d", Value); \
				AddSubItem(index, ColumnEnum, buf); \
			}
			ADD_COLUMN(COLUMN_NumSkel, package->NumSkeletalMeshes);
			ADD_COLUMN(COLUMN_NumStat, package->NumStaticMeshes);
			ADD_COLUMN(COLUMN_NumAnim, package->NumAnimations);
			ADD_COLUMN(COLUMN_NumTex,  package->NumTextures);
#undef ADD_COLUMN
		}
		// size
		appSprintf(ARRAY_ARG(buf), "%d", package->SizeInKb + package->ExtraSizeInKb);
		AddSubItem(index, COLUMN_Size, buf);
#endif // USE_FULLY_VIRTUAL_LIST
	}

	void SelectPackages(UIPackageDialog::PackageList& SelectedPackages)
	{
		UnselectAllItems();
		for (int i = 0; i < SelectedPackages.Num(); i++)
		{
			int index = Packages.FindItem(SelectedPackages[i]);
			if (index >= 0)
				SelectItem(index, true);
		}
	}

	void GetSelectedPackages(UIPackageDialog::PackageList& OutPackageList)
	{
		OutPackageList.Reset();
		OutPackageList.AddZeroed(GetSelectionCount());
		for (int selIndex = 0; selIndex < GetSelectionCount(); selIndex++)
		{
			OutPackageList[selIndex] = Packages[GetSelectionIndex(selIndex)];
		}
	}

private:
	// Virtual list mode: get list item count
	void GetItemCountHandler(UIMulticolumnListbox* Sender, int& OutCount)
	{
		OutCount = Packages.Num();
	}

	// Virtual list mode: show package information in list
	void GetItemTextHandler(UIMulticolumnListbox* Sender, const char*& OutText, int ItemIndex, int SubItemIndex)
	{
		guard(UIPackageList::GetItemTextHandler);

		static FStaticString<MAX_PACKAGE_PATH> Buffer; // returning this value outside by pointer, so it is 'static'

		const CGameFileInfo* file = Packages[ItemIndex];
		if (SubItemIndex == COLUMN_Name)
		{
			if (StripPath)
				file->GetCleanName(Buffer);
			else
				file->GetRelativeName(Buffer);
			OutText = *Buffer;
		}
		else if (SubItemIndex == COLUMN_Size)
		{
			appSprintf(&Buffer[0], 63, "%d", file->SizeInKb + file->ExtraSizeInKb);
			OutText = *Buffer;
		}
		else if (file->IsPackageScanned)
		{
			int value = 0;
			switch (SubItemIndex)
			{
			case COLUMN_NumSkel: value = file->NumSkeletalMeshes; break;
			case COLUMN_NumStat: value = file->NumStaticMeshes; break;
			case COLUMN_NumAnim: value = file->NumAnimations; break;
			case COLUMN_NumTex:  value = file->NumTextures; break;
			}
			if (value != 0)
			{
				// don't show zero counts
				appSprintf(&Buffer[0], 63, "%d", value);
				OutText = *Buffer;
			}
			else
			{
				OutText = "";
			}
		}

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	Main UI code
-----------------------------------------------------------------------------*/

UIPackageDialog::UIPackageDialog()
:	DirectorySelected(false)
,	ContentScanned(false)
,	UseFlatView(false)
,	SortedColumn(UIPackageList::COLUMN_Name)
,	ReverseSort(false)
{
	CloseOnEsc();
	HideOnClose();
	SetResizeable();
}

UIPackageDialog::EResult UIPackageDialog::Show()
{
	ModalResult = CANCEL;
	DontGetSelectedPackages = false;

	ShowModal("Choose a package to open", 750, 550);

	if (ModalResult != CANCEL && !DontGetSelectedPackages)
	{
		UpdateSelectedPackages();
	}
	DontGetSelectedPackages = false;

	return ModalResult;
}

void UIPackageDialog::SelectPackage(UnPackage* package)
{
	SelectedPackages.Empty();
	const CGameFileInfo* info = package->FileInfo;
	if (info)
	{
		SelectedPackages.Add(info);
		SelectedDir = info->GetPath();
	}
}

void UIPackageDialog::InitUI()
{
	guard(UIPackageDialog::InitUI);

	TreeMenu = new UIMenu;
	(*TreeMenu)
	[
		NewMenuItem("Open folder content")
		.SetCallback(BIND_MEMBER(&UIPackageDialog::OnOpenFolderClicked, this))
		+ NewMenuItem("Open folder content (append)")
		.SetCallback(BIND_MEMBER(&UIPackageDialog::OnOpenAppendFolderClicked, this))
		+ NewMenuItem("Export folder content")
		.SetCallback(BIND_MEMBER(&UIPackageDialog::OnExportFolderClicked, this))
		+ NewMenuItem("Save folder packages")
		.SetCallback(BIND_MEMBER(&UIPackageDialog::SaveFolderPackages, this))
		+ NewMenuSeparator()
		+ NewMenuItem("Scan folder content")
		.SetCallback(BIND_LAMBDA([this]() {
				PackageList list;
				GetPackagesForSelectedFolder(list);
				ScanContent(list);
			}))
	];

	ListMenu = new UIMenu;
	(*ListMenu)
	.SetBeforePopup(BIND_MEMBER(&UIPackageDialog::OnBeforeListMenuPopup, this))
	[
		NewMenuItem("Open")
		.SetCallback(BIND_LAMBDA([this]() { CloseDialog(OPEN); }))
		+ NewMenuItem("Open (add to loaded set)")
		.SetCallback(BIND_LAMBDA([this]() { CloseDialog(APPEND); }))
		+ NewMenuItem("Export")
		.SetCallback(BIND_LAMBDA([this]() { CloseDialog(EXPORT); }))
		+ NewMenuSeparator()
		+ NewMenuItem("Save packages")
		.SetCallback(BIND_MEMBER(&UIPackageDialog::SavePackages, this))
		+ NewMenuSeparator()
		+ NewMenuItem("Copy package path")
		.SetCallback(BIND_MEMBER(&UIPackageDialog::CopyPackagePaths, this))
	];

	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UICheckbox, "Flat view", &UseFlatView)
				.SetCallback(BIND_MEMBER(&UIPackageDialog::OnFlatViewChanged, this))
			+ NewControl(UISpacer)
			+ NewControl(UILabel, "Filter:")
				.SetY(2)
				.SetAutoSize()
			+ NewControl(UITextEdit, &PackageFilter)
				.SetWidth(120)
				.SetCallback(BIND_MEMBER(&UIPackageDialog::OnFilterTextChanged, this))
		]
		+ NewControl(UIPageControl)
			.Expose(FlatViewPager)
			.SetHeight(EncodeWidth(1.0f))
		[
			// page 0: TreeView + ListBox
			NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			.SetHeight(EncodeWidth(1.0f))
			[
				NewControl(UITreeView)
					.SetRootLabel("All packages")
					.SetWidth(EncodeWidth(0.3f))
					.SetHeight(-1)
					.SetCallback(BIND_MEMBER(&UIPackageDialog::OnTreeItemSelected, this))
					.UseFolderIcons()
					.SetItemHeight(20)
					.Expose(PackageTree)
					.SetMenu(TreeMenu)
				+ CreatePackageListControl(true).Expose(PackageListbox)
				.SetMenu(ListMenu)
			]
			// page 1: single ListBox
			+ CreatePackageListControl(false).Expose(FlatPackageList)
			.SetMenu(ListMenu)
		]
	];

	CSemaphore packageSortComplete;
	if (!Packages.Num())
	{
		guard(FetchPackageList);

		// Package list was not filled yet
		// Count packages first for more efficient Packages.Add() when number of packages is very large
		int NumPackages = 0;
		appEnumGameFiles<int&>(
			[](const CGameFileInfo* file, int& param) -> bool
			{
				param++;
				return true;
			}, NumPackages);
		Packages.Empty(NumPackages);
		// Fill Packages list
		appEnumGameFiles<TArray<const CGameFileInfo*> >(
			[](const CGameFileInfo* file, TArray<const CGameFileInfo*>& param) -> bool
			{
				param.Add(file);
				return true;
			}, Packages);

		ThreadPool::TryExecuteInThread([this]()
			{
				// Perform sort - 1st part of UIPackageDialog::SortPackages()
				SortPackages(Packages, SortedColumn, ReverseSort);
			}, &packageSortComplete);

		unguard;
	}

	// Add paths of all found packages to the directory tree

	if (SelectedPackages.Num()) DirectorySelected = true;

	guard(FillFolderTree);

	TArray<const FString*> Folders;
	Folders.Empty(4096); //todo
	appEnumGameFolders<TArray<const FString*> >(
		[](const FString& Folder, int NumFiles, TArray<const FString*>& Folders) -> bool
		{
			Folders.Add(&Folder);
			return true;
		}, Folders);
	Folders.Sort([](const FString* const& A, const FString* const& B) -> int
		{
			return stricmp(**A, **B);
		});

	// bool isUE4 = false; //todo: not really used
	for (const FString* Folder : Folders)
	{
		// Add a directory to TreeView
		const FString& Path = *Folder;
		PackageTree->AddItem(*Path);

		if (!DirectorySelected)
		{
			// find the first directory with packages, but don't select /Engine subdirectories by default
			bool isUE4EnginePath = (strnicmp(*Path, "Engine", 6) == 0) || (strnicmp(*Path, "/Engine", 7) == 0) || strstr(*Path, "/Plugins") != NULL;
			if (!isUE4EnginePath && (stricmp(*Path, *SelectedDir) < 0 || SelectedDir.IsEmpty()))
			{
				// set selection to the first directory
				SelectedDir = Path;
			}
		}
		// if (!isUE4 && !Path.IsEmpty() && !strnicmp(*Path, "/Game", 5))
		//	isUE4 = true;
	}

	unguard;

	if (!SelectedDir.IsEmpty())
	{
		PackageTree->Expand(*SelectedDir);	//!! note: will not work at the moment because "Expand" works only after creation of UITreeView
	}

/*	if (isUE4)
	{
		// UE4 may have multiple root nodes for better layout
//		PackageTree->HasRootNode(false);
//??		PackageTree->Expand("/Game"); -- doesn't work unless TreeView is already created
	} */

	// "Tools" menu
	UIMenu* toolsMenu = new UIMenu;
	(*toolsMenu)
	[
		NewMenuItem("Scan content")
		.Enable(!ContentScanned)
		.Expose(ScanContentMenu)
		.SetCallback(BIND_LAMBDA([this]() {
				if (ScanContent(Packages))
				{
					// finished - no needs to perform scan again, disable button
					ContentScanned = true;
					ScanContentMenu->Enable(false);
				}
			}))
		+ NewMenuItem("Scan versions")
		.SetCallback(BIND_STATIC(&ShowPackageScanDialog))
		+ NewMenuSeparator()
		+ NewMenuItem("Save selected packages")
		.Enable(SelectedPackages.Num() > 0)
		.SetCallback(BIND_MEMBER(&UIPackageDialog::SavePackages, this))
		.Expose(SavePackagesMenu)
		+ NewMenuSeparator()
		+ NewMenuItem("Options")
		.SetCallback(BIND_LAMBDA([]() { UISettingsDialog dialog(GSettings); dialog.Show(); }))
		+ NewMenuSeparator()
		+ NewMenuItem("About UModel")
		.SetCallback(BIND_STATIC(&UIAboutDialog::Show))
	];

	UIMenu* openMenu = new UIMenu;
	(*openMenu)
	[
		NewMenuItem("Open (replace loaded set)")
		.SetCallback(BIND_LAMBDA([this]() { CloseDialog(OPEN); }))
		+ NewMenuItem("Append (add to loaded set)")
		.SetCallback(BIND_LAMBDA([this]() { CloseDialog(APPEND); }))
	];

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UILabel, "Hint: you may open this dialog at any time by pressing \"O\"")
		+ NewControl(UIMenuButton, "Tools")
		.SetWidth(80)
		.SetMenu(toolsMenu)
		+ NewControl(UIMenuButton, "Open")
			.SetWidth(80)
			.SetMenu(openMenu)
			.Enable(false)
			.Expose(OpenButton)
			.SetCallback(BIND_LAMBDA([this]() { CloseDialog(OPEN); }))
//			.SetOK() -- this will not let menu to open
		+ NewControl(UIButton, "Export")
			.SetWidth(80)
			.Enable(false)
			.Expose(ExportButton)
			.SetCallback(BIND_LAMBDA([this]()
			{
				if (UISettingsDialog::ShowExportOptions(GSettings))
				{
					CloseDialog(EXPORT);
				}
			}))
		+ NewControl(UIButton, "Cancel")
			.SetWidth(80)
			.SetCallback(BIND_LAMBDA([this]() { CloseDialog(CANCEL); }))
	];

	// Finish sorting of packages - 2nd part of UIPackageDialog::SortPackages()
	packageSortComplete.Wait();
	UpdateUIAfterSort();

	unguard;
}

UIPackageList& UIPackageDialog::CreatePackageListControl(bool StripPath)
{
	UIPackageList& List = NewControl(UIPackageList, StripPath);
	List.SetHeight(-1)
		.SetSelChangedCallback(BIND_MEMBER(&UIPackageDialog::OnPackageSelected, this))
		.SetDblClickCallback(BIND_MEMBER(&UIPackageDialog::OnPackageDblClick, this))
		.SetOnColumnClick(BIND_MEMBER(&UIPackageDialog::OnColumnClick, this));
	return List;
}

void UIPackageDialog::CloseDialog(EResult Result, bool bDontGetSelectedPackages)
{
	ModalResult = Result;
	DontGetSelectedPackages = bDontGetSelectedPackages;
	UIBaseDialog::CloseDialog(false);
}


/*-----------------------------------------------------------------------------
	Support for tree and flat package lists
-----------------------------------------------------------------------------*/

// Retrieve list of selected packages from currently active UIPackageList
void UIPackageDialog::UpdateSelectedPackages()
{
	guard(UIPackageDialog::UpdateSelectedPackages);

	if (!IsDialogConstructed) return;	// nothing to read from controls yet, don't damage selection array

	if (!UseFlatView)
	{
		PackageListbox->GetSelectedPackages(SelectedPackages);
	}
	else
	{
		FlatPackageList->GetSelectedPackages(SelectedPackages);
		// Update currently selected directory in tree
		if (SelectedPackages.Num())
			SelectedDir = SelectedPackages[0]->GetPath();
	}

	unguard;
}

void UIPackageDialog::GetPackagesForSelectedFolder(PackageList& OutPackages)
{
	const char* folder = PackageTree->GetSelectedItem();
	if (!folder) return;

	int folderLen = strlen(folder);
	OutPackages.Empty(1024);

	for (const CGameFileInfo* package : Packages)
	{
		// When root folder selected, "folder" is a empty string, we'll fill Packages with full list of packages
		if (folderLen > 0)
		{
			//todo: use folder index!
			const FString& Path = package->GetPath();
			if (!Path.StartsWith(folder) || (Path.Len() != folderLen && Path[folderLen] != '/'))
			{
				// Not in this folder
				continue;
			}
		}

		OutPackages.Add(package);
	}
}

void UIPackageDialog::OnTreeItemSelected(UITreeView* sender, const char* text)
{
	SelectedDir = text;
	DirectorySelected = true;
	PackageListbox->FillPackageList(Packages, text, *PackageFilter);
}

void UIPackageDialog::OnFlatViewChanged(UICheckbox* sender, bool value)
{
	// call UpdateSelectedPackages using previous UseFlatView value
	UseFlatView = !UseFlatView;
	UpdateSelectedPackages();
	UseFlatView = !UseFlatView;

	RefreshPackageListbox();
}

void UIPackageDialog::RefreshPackageListbox()
{
	// What this function does:
	// 1. clear currently unused list
	// 2. fill current UIPackageList with filtered list of packages
	// 3. update selection - preserve it when changing flat mode value, or when typing something in filter box
	// 4. update dialog button states according to selection state (OnPackageSelected)
	// 5. activate selected control (use FlatViewPager)
#if 0
	appPrintf("Selected packages:\n");
	for (int i = 0; i < SelectedPackages.Num(); i++) appPrintf("  %s\n", SelectedPackages[i]->RelativeName);
#endif
	if (UseFlatView)
	{
		// switching to flat list
		PackageListbox->RemoveAllItems();
		FlatPackageList->FillFlatPackageList(Packages, *PackageFilter);
		// select item which was active in tree+list
		FlatPackageList->SelectPackages(SelectedPackages);
		// update buttons enable state
		OnPackageSelected(FlatPackageList);
	}
	else
	{
		// switching to tree+list
		FlatPackageList->RemoveAllItems();
		PackageTree->SelectItem(*SelectedDir);
		OnTreeItemSelected(PackageTree, *SelectedDir); // fills package list
		// select directory and package
		PackageListbox->SelectPackages(SelectedPackages);
		// update buttons enable state
		OnPackageSelected(PackageListbox);
	}
	// switch control
	FlatViewPager->SetActivePage(UseFlatView ? 1 : 0);
}

/*-----------------------------------------------------------------------------
	Package list sorting code
-----------------------------------------------------------------------------*/

// We are working in global package list, no matter if we have all packages
// are filtered by directory name etc, it should work well in any case.

// We can't make qsort stable without storing original package index for comparison.
// This is because when qsort performs sorting iteration, it will split data into 2 parts,
// one with smaller sort value, another one with larger sort value. When value is the same,
// moved data could be reordered.

struct PackageSortHelper
{
	const CGameFileInfo* File;
	int Index;
};

#define USE_FAST_SORTER 0

#if !USE_FAST_SORTER

static bool PackageSort_Reverse;
static int  PackageSort_Column;

// do not use 'switch' inside sorter function
static int PackageSortFunction(const PackageSortHelper* pA, const PackageSortHelper* pB)
{
	const CGameFileInfo* A = pA->File;
	const CGameFileInfo* B = pB->File;
	if (PackageSort_Reverse) Exchange(A, B);
	int code = 0;
	switch (PackageSort_Column)
	{
	case UIPackageList::COLUMN_Name:
		code = CGameFileInfo::CompareNames(*A, *B);
		break;
	case UIPackageList::COLUMN_Size:
		code = (A->SizeInKb - B->SizeInKb) + (A->ExtraSizeInKb - B->ExtraSizeInKb);
		break;
	case UIPackageList::COLUMN_NumSkel:
		code = A->NumSkeletalMeshes - B->NumSkeletalMeshes;
		break;
	case UIPackageList::COLUMN_NumStat:
		code = A->NumStaticMeshes - B->NumStaticMeshes;
		break;
	case UIPackageList::COLUMN_NumAnim:
		code = A->NumAnimations - B->NumAnimations;
		break;
	case UIPackageList::COLUMN_NumTex:
		code = A->NumTextures - B->NumTextures;
		break;
	}
	// make sort stable
	if (code == 0)
		code = pA->Index - pB->Index;

	return code;
}

#else

// This approach minimizes overhead of sort type selection in PackageSortFunction. Interestingly,
// it doesn't provide enough speedup, but keeping the code just for possible future reference.

#define SORTER0(argA, argB, ...) \
	[](const PackageSortHelper* pA, const PackageSortHelper* pB) -> int \
	{ \
		const CGameFileInfo* A = argA->File; \
		const CGameFileInfo* B = argB->File; \
		int code = __VA_ARGS__; \
		/* make sort stable */ \
		if (code == 0) \
			code = pA->Index - pB->Index; \
		return code; \
	}

#define SORTER(...) \
	{ SORTER0(pA, pB, __VA_ARGS__), SORTER0(pB, pA, __VA_ARGS__) }

typedef int (*PackageSortFunction_t)(const PackageSortHelper*, const PackageSortHelper*);

static const PackageSortFunction_t PackageSorter[][2] = {
	SORTER(CGameFileInfo::CompareNames(*A, *B)),			// COLUMN_Name
	SORTER(A->NumSkeletalMeshes - B->NumSkeletalMeshes),	// COLUMN_NumSkel
	SORTER(A->NumStaticMeshes - B->NumStaticMeshes),		// COLUMN_NumStat,
	SORTER(A->NumAnimations - B->NumAnimations),			// COLUMN_NumAnim
	SORTER(A->NumTextures - B->NumTextures),				// COLUMN_NumTex
	SORTER((A->SizeInKb - B->SizeInKb) + (A->ExtraSizeInKb - B->ExtraSizeInKb)), // COLUMN_Size
};

static_assert(ARRAY_COUNT(PackageSorter) == UIPackageList::COLUMN_Count, "Review PackageSorter");

#endif // USE_FAST_SORTER

// Stable sort of packages
/*static*/ void UIPackageDialog::SortPackages(PackageList& List, int Column, bool Reverse)
{
	guard(SortPackagesInternal);

	// prepare helper array
	TArray<PackageSortHelper> SortedArray;
	SortedArray.AddUninitialized(List.Num());
	for (int i = 0; i < List.Num(); i++)
	{
		PackageSortHelper& S = SortedArray[i];
		S.File = List[i];
		S.Index = i;
	}

#if !USE_FAST_SORTER
	PackageSort_Reverse = Reverse;
	PackageSort_Column = Column;
	SortedArray.Sort(PackageSortFunction);
#else
	SortedArray.Sort(PackageSorter[Column][int(Reverse)]);
#endif

	// copy sorted data back to List
	for (int i = 0; i < List.Num(); i++)
	{
		List[i] = SortedArray[i].File;
	}

	unguard;
}

void UIPackageDialog::SortPackages()
{
	guard(UIPackageDialog::SortPackages);
	UpdateSelectedPackages();
	SortPackages(Packages, SortedColumn, ReverseSort);
	UpdateUIAfterSort();
	unguard;
}

void UIPackageDialog::UpdateUIAfterSort()
{
	guard(UIPackageDialog::UpdateUIAfterSort);
	FlatPackageList->ShowSortArrow(SortedColumn, ReverseSort);
	PackageListbox->ShowSortArrow(SortedColumn, ReverseSort);
	RefreshPackageListbox();
	unguard;
}

/*-----------------------------------------------------------------------------
	Content tools
-----------------------------------------------------------------------------*/

bool UIPackageDialog::ScanContent(const PackageList& packageList)
{
	UIProgressDialog progress;
	progress.Show("Scanning packages");
	progress.SetDescription("Scanning package");

	// perform scan
	bool done = ::ScanContent(packageList, &progress);

	progress.CloseDialog();

	// Refresh package list anyway, even for partially scanned content
	SortPackages();

	// update package list with new data
	UpdateSelectedPackages();
	RefreshPackageListbox();

	return done;
}


void UIPackageDialog::SavePackages()
{
	guard(UIPackageDialog::SavePackages);

	if (!UISettingsDialog::ShowSavePackagesOptions(GSettings))
		return;

	// We are using selection, so update it.
	UpdateSelectedPackages();

	UIProgressDialog progress;
	progress.Show("Saving packages");
	progress.SetDescription("Saving package");

	::SavePackages(SelectedPackages, &progress);

	unguard;
}


void UIPackageDialog::SaveFolderPackages()
{
	guard(UIPackageDialog::SaveFolderPackages);

	if (!UISettingsDialog::ShowSavePackagesOptions(GSettings))
		return;

	PackageList PackagesToSave;
	GetPackagesForSelectedFolder(PackagesToSave);

	UIProgressDialog progress;
	progress.Show("Saving packages");
	progress.SetDescription("Saving package");

	::SavePackages(PackagesToSave, &progress);

	unguard;
}


/*-----------------------------------------------------------------------------
	Miscellaneous UI callbacks
-----------------------------------------------------------------------------*/

void UIPackageDialog::OnFilterTextChanged(UITextEdit* sender, const char* text)
{
	// re-filter lists
	UpdateSelectedPackages();
	RefreshPackageListbox();
}

void UIPackageDialog::OnPackageSelected(UIMulticolumnListbox* sender)
{
	bool enableButtons = (sender->GetSelectionCount() > 0);
	OpenButton->Enable(enableButtons);
	ExportButton->Enable(enableButtons);
	SavePackagesMenu->Enable(enableButtons);
}

void UIPackageDialog::OnPackageDblClick(UIMulticolumnListbox* sender, int value)
{
	if (value != -1)
	{
		CloseDialog(OPEN);
	}
}

void UIPackageDialog::OnColumnClick(UIMulticolumnListbox* sender, int column)
{
	if (SortedColumn == column)
	{
		// when the same column clicked again, change sort mode
		ReverseSort = !ReverseSort;
	}
	else
	{
		SortedColumn = column;
		ReverseSort = (column >= 1); // default sort mode for first (name) column is 'normal', for other columns - 'reverse'
	}
	SortPackages();
}

void UIPackageDialog::OnOpenFolderClicked()
{
	SelectedPackages.Empty();
	GetPackagesForSelectedFolder(SelectedPackages);
	CloseDialog(OPEN, true);
}

void UIPackageDialog::OnOpenAppendFolderClicked()
{
	SelectedPackages.Empty();
	GetPackagesForSelectedFolder(SelectedPackages);
	CloseDialog(APPEND, true);
}

void UIPackageDialog::OnExportFolderClicked()
{
	if (!UISettingsDialog::ShowExportOptions(GSettings))
		return;

	PackageList PackagesToExport;
	GetPackagesForSelectedFolder(PackagesToExport);

	UIProgressDialog progress;
	progress.Show("Exporting packages");
	bool cancelled = false;

	// Load all packages corresponding to PackagesToExport list
	// NOTE: same code exists in CUmodelApp
	progress.SetDescription("Scanning package");
	TArray<UnPackage*> UnrealPackages;
	UnrealPackages.Empty(PackagesToExport.Num());

	for (int i = 0; i < PackagesToExport.Num(); i++)
	{
		FStaticString<MAX_PACKAGE_PATH> RelativeName;
		PackagesToExport[i]->GetRelativeName(RelativeName);
		if (!progress.Progress(*RelativeName, i, PackagesToExport.Num()))
		{
			cancelled = true;
			break;
		}
		UnPackage* package = UnPackage::LoadPackage(PackagesToExport[i], true);	// should always return non-NULL
		if (package) UnrealPackages.Add(package);
	}
	if (cancelled || !UnrealPackages.Num())
	{
		return;
	}

	progress.SetDescription("Exporting package");
	ExportPackages(UnrealPackages, &progress);
}

void UIPackageDialog::OnBeforeListMenuPopup()
{
	// Enable or disable menu items according to current selection
	if (!UseFlatView)
	{
		ListMenu->Enable(PackageListbox->GetSelectionCount() > 0);
	}
	else
	{
		ListMenu->Enable(FlatPackageList->GetSelectionCount() > 0);
	}
}

void UIPackageDialog::CopyPackagePaths()
{
	FStaticString<MAX_PACKAGE_PATH> ToCopy;

	// We are using selection, so update it.
	UpdateSelectedPackages();
	for (const CGameFileInfo* package : SelectedPackages)
	{
		FStaticString<MAX_PACKAGE_PATH> Path;
		package->GetRelativeName(Path);
		if (!ToCopy.IsEmpty())
		{
			// Multiple file names
			ToCopy += "\n";
		}
		ToCopy += Path;
	}
	appCopyTextToClipboard(*ToCopy);
}

#endif // HAS_UI
