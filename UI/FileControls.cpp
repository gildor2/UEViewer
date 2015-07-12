#if _WIN32

#include <ObjBase.h>		// CoInitialize()
#include <Shlwapi.h>		// SH* functions
#include <Shlobj.h>			// SHBrowseForFolder

#undef PLATFORM_UNKNOWN

#endif //  _WIN32

#include "BaseDialog.h"
#include "FileControls.h"

#if HAS_UI

#if _WIN32

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

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

#define BROWSE_BUTTON_WIDTH		70

UIFilePathEditor::UIFilePathEditor(FString* path)
:	UIGroup(GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
,	Path(path)
{
#if _WIN32
	for (int i = 0; i < Path->Len(); i++)
		if ((*Path)[i] == '/') (*Path)[i] = '\\';
#endif
}

void UIFilePathEditor::Create(UIBaseDialog* dialog)
{
	Super::Create(dialog);
	InitializeOLE();
	SHAutoComplete(Editor->GetWnd(), SHACF_FILESYS_DIRS);
	DlgWnd = dialog->GetWnd();
}

void UIFilePathEditor::AddCustomControls()
{
	(*this)
	[
		NewControl(UITextEdit, Path)
		.Expose(Editor)
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Browse ...")
		.SetWidth(BROWSE_BUTTON_WIDTH)
		.SetCallback(BIND_MEM_CB(&UIFilePathEditor::OnBrowseClicked, this))
	];
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
	BROWSEINFO bi;
	memset(&bi, 0, sizeof(bi));

	char szDisplayName[MAX_PATH];	// will not be used
	szDisplayName[0] = 0;

	bi.hwndOwner      = DlgWnd;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = szDisplayName;
	bi.lpszTitle      = "Please select a directory:";	//!! customize
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


#endif // HAS_UI
