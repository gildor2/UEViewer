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
{}

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
		.SetY(2)
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Browse ...")
		.SetWidth(BROWSE_BUTTON_WIDTH)
		.SetCallback(BIND_MEM_CB(&UIFilePathEditor::OnBrowseClicked, this))
	];
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	// If the BFFM_INITIALIZED message is received
	// set the path to the start path.
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
		{
			if (lpData)
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		}
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
	bi.lpszTitle      = "Select a directory:";	//!! customize
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
