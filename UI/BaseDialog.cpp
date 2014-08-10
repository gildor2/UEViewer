#if _WIN32
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#include <windows.h>
#endif

#include "BaseDialog.h"


#if HAS_UI

/*-----------------------------------------------------------------------------
	UIElement
-----------------------------------------------------------------------------*/

UIElement::UIElement()
: X(-1)
, Y(-1)
, Width(-1)
, Height(-1)
, IsGroup(false)
, Parent(NULL)
, Wnd(0)
{}


UIElement::~UIElement()
{}


/*-----------------------------------------------------------------------------
	UIGroup
-----------------------------------------------------------------------------*/

UIGroup::UIGroup()
{}


UIGroup::~UIGroup()
{
	ReleaseControls();
}


void UIGroup::ReleaseControls()
{
	for (int i = 0; i < Children.Num(); i++)
		delete Children[i];
	Children.Empty();
}


/*-----------------------------------------------------------------------------
	UIBaseDialog
-----------------------------------------------------------------------------*/

UIBaseDialog::UIBaseDialog()
{
}


UIBaseDialog::~UIBaseDialog()
{
}


#define MAX_TITLE_LEN	256

static DLGTEMPLATE* MakeDialogTemplate(int width, int height, const wchar_t* title, const wchar_t* fontName, int fontSize)
{
	// This code uses volatile structure DLGTEMPLATEEX. It's described in MSDN, but
	// cannot be represented as a structure. We're declaring partial case of structure,
	// with empty strings, except font name.
	// This code works exactly like declaring dialog in resources.
	typedef struct {
		WORD      dlgVer;
		WORD      signature;
		DWORD     helpID;
		DWORD     exStyle;
		DWORD     style;
		WORD      cDlgItems;
		short     x;
		short     y;
		short     cx;
		short     cy;
		short     menu;				// sz_or_ord, dynamic size
		short     windowClass;		// sz_or_ord, dynamic size
		WCHAR     title[1];			// dynamic size
	} DLGTEMPLATEEX1;

	typedef struct {
		WORD      pointsize;
		WORD      weight;
		BYTE      italic;
		BYTE      charset;
		WCHAR     typeface[32];		// dynamic size
	} DLGTEMPLATEEX2;

	// 'static' is required, can't use stack variable, and don't want to allocate
	static byte buffer[sizeof(DLGTEMPLATEEX1) + sizeof(DLGTEMPLATEEX2) + MAX_TITLE_LEN * sizeof(wchar_t)];
	memset(buffer, 0, sizeof(buffer));

	DLGTEMPLATEEX1* dlg1 = (DLGTEMPLATEEX1*)buffer;
	dlg1->dlgVer = 1;
	dlg1->signature = 0xFFFF;
	dlg1->exStyle = 0;
	dlg1->style = WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT | DS_MODALFRAME;
	dlg1->cx = width;
	dlg1->cy = height;

	int titleLen = wcslen(title);
	assert(titleLen < MAX_TITLE_LEN);
	wcscpy(dlg1->title, title);

	DLGTEMPLATEEX2* dlg2 = (DLGTEMPLATEEX2*)buffer + sizeof(DLGTEMPLATEEX1) + titleLen * sizeof(wchar_t);
	dlg2->pointsize = fontSize;
	wcscpy(dlg2->typeface, fontName);

	return (DLGTEMPLATE*)buffer;
}


bool UIBaseDialog::ShowDialog(const char* title, int width, int height)
{
	wchar_t wTitle[MAX_TITLE_LEN];
	mbstowcs(wTitle, title, MAX_TITLE_LEN);
	DLGTEMPLATE* tmpl = MakeDialogTemplate(width, height, wTitle, L"MS Sans Serif", 8);

#if 1
	// modal
	int result = DialogBoxIndirectParam(
		GetModuleHandle(NULL),		// hInstance
		tmpl,						// lpTemplate
		0,							// hWndParent
		StaticWndProc,				// lpDialogFunc
		(LPARAM)this				// lParamInit
	);
	appPrintf("dialog result: %d, error: %d\n", result, GetLastError());
#else
	// modeless
	HWND dialog = CreateDialogIndirectParam(
		GetModuleHandle(NULL),		// hInstance
		tmpl,						// lpTemplate
		0,							// hWndParent
		StaticWndProc,				// lpDialogFunc
		(LPARAM)this				// lParamInit
	);

	if (dialog)
	{
		// implement a message loop
		appPrintf("Dialog created");
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))	//!! no exit condition here
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		appPrintf("Dialog closed");
	}
#endif

	return true;
}


INT_PTR CALLBACK UIBaseDialog::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UIBaseDialog* dlg;

	if (msg == WM_INITDIALOG)
	{
		// remember pointer in window's user data
		dlg = (UIBaseDialog*)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
	}
	else
	{
		dlg = (UIBaseDialog*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}
	return dlg->WndProc(hWnd, msg, wParam, lParam);
}


INT_PTR UIBaseDialog::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// handle dialog initialization
	if (msg == WM_INITDIALOG)
	{
		Wnd = hWnd;

		// center window on screen
		RECT controlRect;
		GetClientRect(hWnd, &controlRect);
		int newX = (GetSystemMetrics(SM_CXSCREEN) - (controlRect.right - controlRect.left)) / 2;
		int newY = (GetSystemMetrics(SM_CYSCREEN) - (controlRect.bottom - controlRect.top)) / 2;
		SetWindowPos(hWnd, NULL, newX, newY, 0, 0, SWP_NOSIZE);

//!!		BeginDialogLayout(m_startOffset);
//!!		InitUI();
//!!		EndDialogLayout(m_extraHeight);
		return TRUE;
	}

	if (msg == WM_CLOSE)
	{
		EndDialog(Wnd, 1);	//!! 1 = code, "cancel"
		return TRUE;
	}

	if (msg == WM_DESTROY)
	{
//!!		RemoveRollout();
		return TRUE;
	}

	int cmd = -1;                     // some controls has cmd==0, only 'id' will be analyzed
	int id = 0;

	// retrieve pointer to our class from user data
	if (msg == WM_COMMAND)
	{
		id  = LOWORD(wParam);
		cmd = HIWORD(wParam);
	}

	if (cmd == -1)
		return FALSE;

//!!	if (id < FIRST_ITEM_ID || id >= m_nextDialogId)
//!!		return TRUE;                  // not any of our controls

	bool res = true; //!! HandleCommand(id, cmd, lParam);   // returns 'true' if command was processed

	return res ? TRUE : FALSE;
}


#endif // HAS_UI
