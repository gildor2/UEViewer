#include "BaseDialog.h"

#if HAS_UI

#include "PackageDialog.h"
#include "PackageScanDialog.h"
#include "ProgressDialog.h"
#include "AboutDialog.h"

#include "UnPackage.h"

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
	bool Filter(const char* str) const
	{
		for (int i = 0; i < Values.Num(); i++)
		{
			if (!appStristr(str, *Values[i]))
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
		SetVirtualMode();		//!! TODO: use callbacks to retrieve item texts
		// Add columns
		//?? right-align text in numeric columns
		AddColumn("Package name");
		AddColumn("Skel", 35, TA_Right);
		AddColumn("Stat", 35, TA_Right);
		AddColumn("Anim", 35, TA_Right);
		AddColumn("Tex",  35, TA_Right);
		AddColumn("Size, Kb", 70, TA_Right);
	#if USE_FULLY_VIRTUAL_LIST
		SetOnGetItemCount(BIND_MEM_CB(&UIPackageList::GetItemCountHandler, this));
		SetOnGetItemText(BIND_MEM_CB(&UIPackageList::GetItemTextHandler, this));
	#endif
	}

	void FillPackageList(UIPackageDialog::PackageList& InPackages, const char* directory, const char* packageFilter)
	{
		LockUpdate();

		RemoveAllItems();
		Packages.Empty();

		CFilter filter(packageFilter);

		for (int i = 0; i < InPackages.Num(); i++)
		{
			const CGameFileInfo* package = InPackages[i];
			char buffer[MAX_PACKAGE_PATH];
			appStrncpyz(buffer, package->RelativeName, ARRAY_COUNT(buffer));
			char* s = strrchr(buffer, '/');
			if (s) *s++ = 0;
			if ((!s && !directory[0]) ||				// root directory
				(s && !strcmp(buffer, directory)))		// another directory
			{
				const char* packageName = s ? s : buffer;
				if (filter.Filter(packageName))
				{
					// this package is in selected directory
					AddPackage(package);
				}
			}
		}

		UnlockUpdate(); // this will call Repaint()
	}

	void FillFlatPackageList(UIPackageDialog::PackageList& InPackages, const char* packageFilter)
	{
		LockUpdate(); // HUGE performance gain. Warning: don't use "return" here without UnlockUpdate()!

		RemoveAllItems();
		Packages.Empty(InPackages.Num());

		CFilter filter(packageFilter);

#if !USE_FULLY_VIRTUAL_LIST
		ReserveItems(InPackages.Num());
#endif
		for (int i = 0; i < InPackages.Num(); i++)
		{
			const CGameFileInfo* package = InPackages[i];
			if (filter.Filter(package->RelativeName))
				AddPackage(package);
		}

		UnlockUpdate();
	}

	void AddPackage(const CGameFileInfo* package)
	{
		Packages.Add(package);

#if !USE_FULLY_VIRTUAL_LIST
		const char* s = package->RelativeName;
		if (StripPath)
		{
			const char* s2 = strrchr(s, '/');
			if (s2) s = s2 + 1;
		}
		int index = AddItem(s);
		char buf[32];
		// put object count information as subitems
		if (package->PackageScanned)
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

		static char buf[64]; // returning this value outside by pointer, so it is 'static'

		const CGameFileInfo* file = Packages[ItemIndex];
		if (SubItemIndex == COLUMN_Name)
		{
			OutText = StripPath ? file->ShortFilename : file->RelativeName;
		}
		else if (SubItemIndex == COLUMN_Size)
		{
			appSprintf(ARRAY_ARG(buf), "%d", file->SizeInKb + file->ExtraSizeInKb);
			OutText = buf;
		}
		else if (file->PackageScanned)
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
				appSprintf(ARRAY_ARG(buf), "%d", value);
				OutText = buf;
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
{}

UIPackageDialog::EResult UIPackageDialog::Show()
{
	ModalResult = OPEN;
	if (!ShowModal("Choose a package to open", 500, 200))
		return CANCEL;

	UpdateSelectedPackages();

	// controls are not released automatically when dialog closed to be able to
	// poll them for user selection; release the memory now
	ReleaseControls();

	return ModalResult;
}

void UIPackageDialog::SelectPackage(UnPackage* package)
{
	SelectedPackages.Empty();
	const CGameFileInfo* info = appFindGameFile(package->Filename);
	if (info)
	{
		SelectedPackages.Add(info);
		SelectDirFromFilename(package->Filename);
	}
}

static bool PackageListEnum(const CGameFileInfo *file, TArray<const CGameFileInfo*> &param)
{
	param.Add(file);
	return true;
}

void UIPackageDialog::InitUI()
{
	guard(UIPackageDialog::InitUI);

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
				+ CreatePackageListControl(true).Expose(PackageListbox)
			]
			// page 1: single ListBox
			+ CreatePackageListControl(false).Expose(FlatPackageList)
		]
	];

	if (!Packages.Num())
	{
		// not scanned yet
		appEnumGameFiles(PackageListEnum, Packages);
	}

	// add paths of all found packages to the directory tree
	if (SelectedPackages.Num()) DirectorySelected = true;
	char prevPath[MAX_PACKAGE_PATH], path[MAX_PACKAGE_PATH];
	prevPath[0] = 0;
	// Make a copy of package list sorted by name, to ensure directory tree is always sorted.
	// Using a copy to not affect package sorting used before.
	PackageList SortedPackages;
	CopyArray(SortedPackages, Packages);
	SortPackages(SortedPackages, UIPackageList::COLUMN_Name, false);
	for (int i = 0; i < Packages.Num(); i++)
	{
		appStrncpyz(path, SortedPackages[i]->RelativeName, ARRAY_COUNT(path));
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
			// find the first directory with packages, but don't select /Game/Engine subdirectories by default
			bool isUE4EnginePath = (strnicmp(path, "Engine/", 7) == 0) || (strnicmp(path, "/Engine/", 8) == 0);
			if (!isUE4EnginePath && (stricmp(path, *SelectedDir) < 0 || SelectedDir.IsEmpty()))
			{
				// set selection to the first directory
				SelectedDir = s ? path : "";
			}
		}
	}
	if (!SelectedDir.IsEmpty())
	{
		PackageTree->Expand(*SelectedDir);	//!! note: will not work at the moment because "Expand" works only after creation of UITreeView
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
		+ NewMenuItem("Save selected packages")
		.Enable(SelectedPackages.Num() > 0)
		.SetCallback(BIND_MEM_CB(&UIPackageDialog::SavePackages, this))
		.Expose(SavePackagesMenu)
		+ NewMenuSeparator()
		+ NewMenuItem("About UModel")
		.SetCallback(BIND_FREE_CB(&UIAboutDialog::Show))
	];

	UIMenu* openMenu = new UIMenu;
	(*openMenu)
	[
		NewMenuItem("Open (replace loaded set)")
		.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnOpenClicked, this))
		+ NewMenuItem("Append (add to loaded set)")
		.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnAppendClicked, this))
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
		+ NewControl(UIMenuButton, "Open")
			.SetWidth(80)
			.SetMenu(openMenu)
			.Enable(false)
			.Expose(OpenButton)
			.SetCallback(BIND_MEM_CB(&UIPackageDialog::OnOpenClicked, this))
//			.SetOK() -- this will not let menu to open
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

	SortPackages(); // will call RefreshPackageListbox()
//	RefreshPackageListbox();

	unguard;
}

UIPackageList& UIPackageDialog::CreatePackageListControl(bool StripPath)
{
	UIPackageList& List = NewControl(UIPackageList, StripPath);
	List.SetHeight(-1)
		.SetSelChangedCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageSelected, this))
		.SetDblClickCallback(BIND_MEM_CB(&UIPackageDialog::OnPackageDblClick, this))
		.SetOnColumnClick(BIND_MEM_CB(&UIPackageDialog::OnColumnClick, this));
	return List;
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
			SelectDirFromFilename(SelectedPackages[0]->RelativeName);
	}

	unguard;
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

static bool PackageSort_Reverse;
static int  PackageSort_Column;

static int PackageSortFunction(const PackageSortHelper* const pA, const PackageSortHelper* const pB)
{
	const CGameFileInfo* A = pA->File;
	const CGameFileInfo* B = pB->File;
	if (PackageSort_Reverse) Exchange(A, B);
	int code = 0;
	switch (PackageSort_Column)
	{
	case UIPackageList::COLUMN_Name:
		code = stricmp(A->RelativeName, B->RelativeName);
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

// Stable sort of packages
/*static*/ void UIPackageDialog::SortPackages(PackageList& List, int Column, bool Reverse)
{
	// prepare helper array
	TArray<PackageSortHelper> SortedArray;
	SortedArray.AddUninitialized(List.Num());
	for (int i = 0; i < List.Num(); i++)
	{
		PackageSortHelper& S = SortedArray[i];
		S.File = List[i];
		S.Index = i;
	}

	PackageSort_Reverse = Reverse;
	PackageSort_Column = Column;
	SortedArray.Sort(PackageSortFunction);

	// copy sorted data back to List
	for (int i = 0; i < List.Num(); i++)
	{
		List[i] = SortedArray[i].File;
	}
}

void UIPackageDialog::SortPackages()
{
	guard(UIPackageDialog::SortPackages);

	UpdateSelectedPackages();

	SortPackages(Packages, SortedColumn, ReverseSort);

	FlatPackageList->ShowSortArrow(SortedColumn, ReverseSort);
	PackageListbox->ShowSortArrow(SortedColumn, ReverseSort);
	RefreshPackageListbox();

	unguard;
}

/*-----------------------------------------------------------------------------
	Content tools
-----------------------------------------------------------------------------*/

static void ScanPackageExports(UnPackage* package, CGameFileInfo* file)
{
	for (int idx = 0; idx < package->Summary.ExportCount; idx++)
	{
		const char* ObjectClass = package->GetObjectName(package->GetExport(idx).ClassIndex);

		if (!stricmp(ObjectClass, "SkeletalMesh") || !stricmp(ObjectClass, "DestructibleMesh"))
			file->NumSkeletalMeshes++;
		else if (!stricmp(ObjectClass, "StaticMesh"))
			file->NumStaticMeshes++;
		else if (!stricmp(ObjectClass, "Animation") || !stricmp(ObjectClass, "MeshAnimation") || !stricmp(ObjectClass, "AnimSequence")) // whole AnimSet count for UE2 and number of sequences for UE3+
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
	int lastTick = appMilliseconds();
	for (int i = 0; i < Packages.Num(); i++)
	{
		CGameFileInfo* file = const_cast<CGameFileInfo*>(Packages[i]);		// we'll modify this structure here
		if (file->PackageScanned) continue;

		// Update progress dialog
		int tick = appMilliseconds();
		if (tick - lastTick > 50)				// do not update too often
		{
			if (!progress.Progress(file->RelativeName, i, GNumPackageFiles))
			{
				cancelled = true;
				break;
			}
			lastTick = tick;
		}

		UnPackage* package = UnPackage::LoadPackage(file->RelativeName, /*silent=*/ true);	// should always return non-NULL
		file->PackageScanned = true;
		if (!package) continue;		// should not happen

		ScanPackageExports(package, file);
	}

	progress.CloseDialog();
	if (cancelled) return;
	ContentScanned = true;

	SortPackages();

	// finished - no needs to perform scan again, disable button
	ScanContentMenu->Enable(false);

	// update package list with new data
	UpdateSelectedPackages();
	RefreshPackageListbox();
}


static void CopyStream(FArchive *Src, FILE *Dst, int Count)
{
	byte buffer[16384];

	while (Count > 0)
	{
		int Size = min(Count, sizeof(buffer));
		Src->Serialize(buffer, Size);
		if (fwrite(buffer, Size, 1, Dst) != 1) appError("Write failed");
		Count -= Size;
	}
}

//!! TODO: move to PackageUtils.cpp
void UIPackageDialog::SavePackages()
{
	guard(UIPackageDialog::SavePackages);

	//!! Possible options:
	//!! - save referenced packages (find better name - "imports", "links", "used packages", "referenced packages" ...)
	//!! - decompress packages
	//!! - preserve package paths
	//!! - destination directory

	// We are using selection, so update it.
	UpdateSelectedPackages();

	UIProgressDialog progress;
	progress.Show("Saving packages");
	progress.SetDescription("Saving package");

	for (int i = 0; i < SelectedPackages.Num(); i++)
	{
		const CGameFileInfo* file = SelectedPackages[i];

		assert(file);
		if (!progress.Progress(file->RelativeName, i, GNumPackageFiles))
			break;

		// Reference in UE4 code: FNetworkPlatformFile::IsAdditionalCookedFileExtension()
		//!! TODO: perhaps save ALL files with the same path and name but different extension
		static const char* additionalExtensions[] =
		{
			"",				// empty string for original extension
#if UNREAL4
			".ubulk",
			".uexp",
#endif // UNREAL4
		};

		for (int ext = 0; ext < ARRAY_COUNT(additionalExtensions); ext++)
		{
			char SrcFile[MAX_PACKAGE_PATH];
#if UNREAL4
			if (ext > 0)
			{
				appStrncpyz(SrcFile, SelectedPackages[i]->RelativeName, ARRAY_COUNT(SrcFile));
				char* s = strrchr(SrcFile, '.');
				if (s && !stricmp(s, ".uasset"))
				{
					// Find additional file by replacing .uasset extension
					strcpy(s, additionalExtensions[ext]);
					file = appFindGameFile(SrcFile);
					if (!file)
					{
						continue;
					}
				}
				else
				{
					// there's no needs to process this file anymore - main file was already exported, no other files will exist
					break;
				}
			}
#endif // UNREAL4

			FArchive *Ar = appCreateFileReader(file);
			if (Ar)
			{
				guard(SaveFile);
				// prepare destination file
				char OutFile[1024];
				appSprintf(ARRAY_ARG(OutFile), "UmodelSaved/%s", file->ShortFilename);
				appMakeDirectoryForFile(OutFile);
				FILE *out = fopen(OutFile, "wb");
				// copy data
				CopyStream(Ar, out, Ar->GetFileSize());
				// cleanup
				delete Ar;
				fclose(out);
				unguardf("%s", file->RelativeName);
			}
		}
	}

	progress.CloseDialog();

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
		CloseDialog();
}

void UIPackageDialog::OnColumnClick(UIMulticolumnListbox* sender, int column)
{
	if (SortedColumn == column)
	{
		// when the same column clickec again, change sort mode
		ReverseSort = !ReverseSort;
	}
	else
	{
		SortedColumn = column;
		ReverseSort = (column >= 1); // default sort mode for first (name) column is 'normal', for other columns - 'reverse'
	}
	SortPackages();
}

void UIPackageDialog::OnOpenClicked()
{
	ModalResult = OPEN;
	CloseDialog();
}

void UIPackageDialog::OnAppendClicked()
{
	ModalResult = APPEND;
	CloseDialog();
}

void UIPackageDialog::OnExportClicked()
{
	ModalResult = EXPORT;
	CloseDialog();
}


#endif // HAS_UI
