// Simple UI library.
// Copyright (C) 2022 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#if _WIN32

#define _CRT_SECURE_NO_WARNINGS
#undef UNICODE

#include <ObjBase.h>		// CoInitialize()
#include <Shlwapi.h>		// SH* functions
#include <VersionHelpers.h>

// prevent "warning C4091: 'typedef ': ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared" with Win7.1 SDK
#pragma warning(push)
#pragma warning(disable:4091)

#include <Shlobj.h>			// SHBrowseForFolder
// Restore warnings
#pragma warning(pop)

#undef PLATFORM_UNKNOWN

#endif //  _WIN32

#include "BaseDialog.h"
#include "FileControls.h"
#include "UIPrivate.h"

#if _WIN32

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

static void InitializeOLE()
{
	static bool initialized = false;
	if (initialized) return;
	initialized = true;
	CoInitialize(NULL);
}

#endif // _WIN32

/*-----------------------------------------------------------------------------
	UIFilePathEditor
-----------------------------------------------------------------------------*/

#define USE_ELLIPSIS_CHARACTER	1
#define BROWSE_BUTTON_WIDTH		70

UIFilePathEditor::UIFilePathEditor(FString* path)
:	UIGroup(GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
,	Path(path)
,	Title("Please select a directory:")
{
#if _WIN32
	for (int i = 0; i < Path->Len(); i++)
		if ((*Path)[i] == '/') (*Path)[i] = '\\';
#endif
}

void UIFilePathEditor::Create(UICreateContext& ctx)
{
	Super::Create(ctx);
	InitializeOLE();
	SHAutoComplete(Editor->GetWnd(), SHACF_FILESYS_DIRS);
	DlgWnd = ctx.dialog->GetWnd();
}

void UIFilePathEditor::AddCustomControls()
{
	(*this)
	[
		NewControl(UITextEdit, Path)
		.Expose(Editor)
	#if !USE_ELLIPSIS_CHARACTER
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Browse ...")
		.SetWidth(BROWSE_BUTTON_WIDTH)
	#else
		+ NewControl(UIButton, "...")
		.SetWidth(20)
	#endif
		.SetCallback(BIND_MEMBER(&UIFilePathEditor::OnBrowseClicked, this))
	];
}

// Browse for folder using more modern UI - FileOpenDialog with selecting for folder instead of a file.
// It is available since Windows Vista, but it seems Vista has some problems, so we're using it since Windows 8.
static bool BrowseForFolderNew(const char* Title, HWND ParentWindow, FString* Path)
{
	IFileOpenDialog* pFileDialog;
	if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog))))
	{
		DWORD dwFlags = 0;
		pFileDialog->GetOptions(&dwFlags);
		pFileDialog->SetOptions(dwFlags | FOS_PICKFOLDERS);

		wchar_t wTitle[MAX_TITLE_LEN];
		mbstowcs(wTitle, Title, MAX_TITLE_LEN);

		// Set up common settings
		pFileDialog->SetTitle(wTitle);
		if (!Path->IsEmpty())
		{
			wchar_t wPath[MAX_PATH];
			mbstowcs(wPath, *(*Path), MAX_PATH);

			// SHCreateItemFromParsingName is not available in WinXP, try to maintain compatibility
			// at least with not using link-time binding.
			typedef HRESULT (WINAPI *SHCreateItemFromParsingName_t)(PCWSTR, IBindCtx*, REFIID, void**);
			static SHCreateItemFromParsingName_t pSHCreateItemFromParsingName = NULL;
			if (!pSHCreateItemFromParsingName)
			{
				pSHCreateItemFromParsingName = (SHCreateItemFromParsingName_t) GetProcAddress(
					GetModuleHandle("shell32.dll"), "SHCreateItemFromParsingName");
				assert(pSHCreateItemFromParsingName);
			}

			IShellItem* pDefaultPathItem;
			if (SUCCEEDED(pSHCreateItemFromParsingName(wPath, NULL, IID_PPV_ARGS(&pDefaultPathItem))))
			{
				pFileDialog->SetFolder(pDefaultPathItem);
				pDefaultPathItem->Release();
			}
		}

		// Show the picker
		bool Result = false;
		if (SUCCEEDED(pFileDialog->Show((HWND)ParentWindow)))
		{
			IShellItem* pResult;
			if (SUCCEEDED(pFileDialog->GetResult(&pResult)))
			{
				PWSTR pFilePath = NULL;
				if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath)))
				{
					// Convert Unicode path to Ansi
					Result = true;
					int len = wcslen(pFilePath);
					char* buf = new char[len+2];
					wcstombs(buf, pFilePath, len+1);
					*Path = buf;
					delete[] buf;
					::CoTaskMemFree(pFilePath);
				}
				pResult->Release();
			}
		}

		pFileDialog->Release();
		return Result;
	}

	return false;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	static LPARAM stored_lpData;

	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		if (lpData)
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		stored_lpData = lpData;
		break;
	case BFFM_SELCHANGED:
		if (stored_lpData)
		{
			// http://social.msdn.microsoft.com/Forums/vstudio/en-US/a22b664e-cb30-44f4-bf77-b7a385de49f3/shbrowseforfolder-bug-in-windows-7?forum=vcgeneral
			// send BFFM_SETSELECTION after popping up the dialog again - this will fix
			// out-of-view selection on Windows 7
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, stored_lpData);
			stored_lpData = NULL;
		}
		break;
	}

	return 0; // The function should always return 0.
}

void UIFilePathEditor::OnBrowseClicked(UIButton* sender)
{
	// For Windows 7 and later, use IFileOpenDialog. Note that GetVersion is deprecated, so
	// we're using newer approach for detecting which Windows version is used.
	if (IsWindows7OrGreater())
	{
		if (BrowseForFolderNew(*Title, DlgWnd, Path))
		{
			// 'Path' is already updated, just need to update UI
			Editor->SetText();
		}
	}
	else
	{
		BROWSEINFO bi;
		memset(&bi, 0, sizeof(bi));

		char szDisplayName[MAX_PATH];	// will not be used
		szDisplayName[0] = 0;

		bi.hwndOwner      = DlgWnd;
		bi.pidlRoot       = NULL;
		bi.pszDisplayName = szDisplayName;
		bi.lpszTitle      = *Title;
		bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
		bi.lParam         = (LPARAM)Editor->GetText();
		bi.iImage         = 0;
		bi.lpfn           = BrowseCallbackProc;

		LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
		char szPathName[1024];
		if (pidl)
		{
			BOOL bRet = SHGetPathFromIDList(pidl, szPathName);
			if (bRet)
			{
				*Path = szPathName;
				Editor->SetText();
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	UIFileNameEditor
-----------------------------------------------------------------------------*/

#define BROWSE_BUTTON_WIDTH		70

UIFileNameEditor::UIFileNameEditor(FString* path)
:	UIGroup(GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
,	Path(path)
,	bIsSaveDialog(false)
{
#if _WIN32
	for (int i = 0; i < Path->Len(); i++)
		if ((*Path)[i] == '/') (*Path)[i] = '\\';
#endif
}

void UIFileNameEditor::Create(UICreateContext& ctx)
{
	Super::Create(ctx);
	InitializeOLE();
	SHAutoComplete(Editor->GetWnd(), SHACF_FILESYS_ONLY);
	DlgWnd = ctx.dialog->GetWnd();
}

void UIFileNameEditor::AddCustomControls()
{
	(*this)
	[
		NewControl(UITextEdit, Path)
		.Expose(Editor)
	#if !USE_ELLIPSIS_CHARACTER
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Browse ...")
		.SetWidth(BROWSE_BUTTON_WIDTH)
	#else
		+ NewControl(UIButton, "...")
		.SetWidth(20)
	#endif
		.SetCallback(BIND_MEMBER(&UIFileNameEditor::OnBrowseClicked, this))
	];
}

FString ShowFileSelectionDialog(
	bool bIsSaveDialog,
	UIBaseDialog* ParentDialog,
	const FString& InitialFilename,
	const FString& InitialDirectory,
	const FString& Title,
	const TArray<FString>& Filters)
{
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));

	char szPathName[1024];
	appStrncpyz(szPathName, *InitialFilename, ARRAY_COUNT(szPathName));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner   = ParentDialog->GetWnd();
	ofn.lpstrFile   = szPathName;
	ofn.nMaxFile    = ARRAY_COUNT(szPathName);

	// Build filters string. It consists of null-terminated strings concatenated
	// into single buffer. Ends with extra null character.
	FStaticString<1024> filterBuf;
	for (int i = 0; i < Filters.Num(); i++)
	{
		const char* s = *Filters[i];
		const char* s2 = strchr(s, '|');
		if (s2)
		{
			int delimRSize = Filters[i].Len() - int(s2 - s);
			filterBuf += s;
			filterBuf[filterBuf.Len() - delimRSize] = 0;
			filterBuf.AppendChar(0);
		}
		else
		{
			filterBuf += s;
			filterBuf.AppendChar(0);
			filterBuf += s;
			filterBuf.AppendChar(0);
		}
	}
	// Add "all files" filter, and finish it with extra null character.
	filterBuf += "All Files (*.*)";
	filterBuf.AppendChar(0);
	filterBuf += "*.*";
	filterBuf.AppendChar(0);
	filterBuf.AppendChar(0);
	ofn.lpstrFilter = *filterBuf;

	if (!InitialDirectory.IsEmpty())
	{
		ofn.lpstrInitialDir = *InitialDirectory;
	}

	if (!Title.IsEmpty())
	{
		ofn.lpstrTitle = *Title;
	}

	ofn.Flags = OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER;
	if (bIsSaveDialog)
	{
		ofn.Flags |= OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_NOVALIDATE;
	}
	else
	{
		ofn.Flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	}

	int bSuccess = (bIsSaveDialog) ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);
	if (bSuccess)
	{
		return szPathName;
	}
	else
	{
		return "";
	}
}

void UIFileNameEditor::OnBrowseClicked(UIButton* sender)
{
	FString filename = ShowFileSelectionDialog(bIsSaveDialog, GetDialog(), *Path, InitialDirectory, Title, Filters);
	if (!filename.IsEmpty())
	{
		*Path = filename;
		Editor->SetText();
	}
}
