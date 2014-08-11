#if _WIN32
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#include <windows.h>
#include <CommCtrl.h>
#endif

#include "BaseDialog.h"

/*!! TODO
* ugly looking controls: try to embed manifest
  http://msdn.microsoft.com/en-us/library/bb773175.aspx
  http://www.transmissionzero.co.uk/computing/win32-apps-with-mingw/
*/


#if HAS_UI

#define FIRST_DIALOG_ID				4000

#define VERTICAL_SPACING			4
#define DEFAULT_VERT_BORDER			9
#define DEFAULT_HORZ_BORDER			7

#define DEFAULT_LABEL_HEIGHT		12
#define DEFAULT_BUTTON_HEIGHT		20
#define DEFAULT_CHECKBOX_HEIGHT		16

#define GROUP_INDENT				10
#define GROUP_MARGIN_TOP			16
#define GROUP_MARGIN_BOTTOM			7
#define GROUP_INDENT				10

#define MAX_TITLE_LEN				256


static HINSTANCE hInstance;

/*-----------------------------------------------------------------------------
	UIElement
-----------------------------------------------------------------------------*/

UIElement::UIElement()
:	X(-1)
,	Y(-1)
,	Width(-1)
,	Height(-1)
,	IsGroup(false)
,	Parent(NULL)
,	Wnd(0)
,	Id(0)
{}


UIElement::~UIElement()
{}


void UIElement::SetRect(int x, int y, int width, int height)
{
	if (x != -1)      X = x;
	if (y != -1)      Y = y;
	if (width != -1)  Width = width;
	if (height != -1) Height = height;
}


HWND UIElement::Window(const char* className, const char* text, DWORD style, UIBaseDialog* dialog,
	int id, int x, int y, int w, int h)
{
	if (x == -1) x = X;
	if (y == -1) y = Y;
	if (w == -1) w = Width;
	if (h == -1) h = Height;
	if (id == -1) id = Id;

	HWND dialogWnd = dialog->GetWnd();

	HWND wnd = ::CreateWindow(className, text, style, x, y, w, h,
		dialogWnd, (HMENU)id, hInstance, NULL);
	SendMessage(wnd, WM_SETFONT, SendMessage(dialogWnd, WM_GETFONT, 0, 0), MAKELPARAM(TRUE, 0));

	return wnd;
}


/*-----------------------------------------------------------------------------
	Utility functions
	Move to UIElement??
-----------------------------------------------------------------------------*/

static int ConvertTextAlign(ETextAlign align)
{
	if (align == TA_Left)
		return SS_LEFT;
	else if (align == TA_Right)
		return SS_RIGHT;
	else
		return SS_CENTER;
}


/*-----------------------------------------------------------------------------
	UILabel
-----------------------------------------------------------------------------*/

UILabel::UILabel(const char* text, ETextAlign align)
:	Label(text)
,	Align(align)
{
	Height = DEFAULT_LABEL_HEIGHT;
}


void UILabel::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(WC_STATIC, *Label, WS_CHILDWINDOW | ConvertTextAlign(Align) | WS_VISIBLE, dialog);
}


/*-----------------------------------------------------------------------------
	UIButton
-----------------------------------------------------------------------------*/

UIButton::UIButton(const char* text)
:	Label(text)
{
	Height = DEFAULT_BUTTON_HEIGHT;
}


void UIButton::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	Id = dialog->GenerateDialogId();

	//!! BS_DEFPUSHBUTTON - for default key
	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP | WS_CHILDWINDOW | WS_VISIBLE, dialog);
}


bool UIButton::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == BN_CLICKED && Callback)
		Callback(this);
	return true;
}


/*-----------------------------------------------------------------------------
	UICheckbox
-----------------------------------------------------------------------------*/

UICheckbox::UICheckbox(const char* text, bool value)
:	Label(text)
,	bValue(value)
,	pValue(&bValue)		// points to local variable
{
	Height = DEFAULT_CHECKBOX_HEIGHT;
}


UICheckbox::UICheckbox(const char* text, bool* value)
:	Label(text)
//,	bValue(value) - uninitialized
,	pValue(value)
{
	Height = DEFAULT_CHECKBOX_HEIGHT;
}


void UICheckbox::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Id = dialog->GenerateDialogId();

	DlgWnd = dialog->GetWnd();

	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP | WS_CHILDWINDOW | WS_VISIBLE | BS_AUTOCHECKBOX, dialog);

	CheckDlgButton(DlgWnd, Id, *pValue ? BST_CHECKED : BST_UNCHECKED);
}


bool UICheckbox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	bool checked = (IsDlgButtonChecked(DlgWnd, Id) != BST_UNCHECKED);
	if (*pValue != checked)
	{
		*pValue = checked;
		if (Callback)
			Callback(this, checked);
	}
	return true;
}


/*-----------------------------------------------------------------------------
	UIGroup
-----------------------------------------------------------------------------*/

UIGroup::UIGroup(const char* label)
:	Label(label)
,	TopBorder(0)
,	NoBorder(false)
,	NoAutoLayout(false)
{
	IsGroup = true;
}


UIGroup::~UIGroup()
{
	ReleaseControls();
}


void UIGroup::ReleaseControls()
{
	guard(UIGroup::ReleaseControls);

	for (int i = 0; i < Children.Num(); i++)
		delete Children[i];
	Children.Empty();

	unguard;
}


void UIGroup::Add(UIElement* item)
{
	guard(UIGroup::Add);

	if (item->Parent)
		item->Parent->Remove(item);
	Children.AddItem(item);
	item->Parent = this;

	unguard;
}


void UIGroup::Remove(UIElement* item)
{
	guard(UIGroup::Remove);

	item->Parent = NULL;
	for (int i = 0; i < Children.Num(); i++)
	{
		if (Children[i] == item)
		{
			Children.Remove(i);
			break;
		}
	}

	unguard;
}


void UIGroup::AllocateUISpace(int& x, int& y, int& w, int& h)
{
	guard(UIGroup::AllocateUISpace);

	int parentX = X;
	int parentWidth = Width;

	if (!NoBorder)
	{
		parentX += GROUP_INDENT;
		parentWidth -= GROUP_INDENT * 2;
	}

	if (x == -1)
		x = parentX;					// DecodeWidth(x) would return 1.0, should use 0.0 here
	else if (x < 0)
		x = parentX + DecodeWidth(x) * parentWidth;	// left border of parent control, 'x' is relative value
	else
		x = parentX + x;				// treat 'x' as relative value

	if (w < 0)
		w = DecodeWidth(w) * parentWidth;

	if (x + w > parentX + parentWidth)	// truncate width if too large
		w = parentX + parentWidth - x;

	if (y < 0)
	{
		y = Y + Height;					// next 'y' value
		if (!NoAutoLayout)
			Height += h;
	}
	else
	{
		y = Y + TopBorder + y;			// treat 'y' as relative value
		// don't change 'Height'
	}

//	h = unchanged;

	unguard;
}


void UIGroup::AddVerticalSpace(int height)
{
	if (NoAutoLayout) return;
	if (height < 0) height = VERTICAL_SPACING;
	Height += height;
}


bool UIGroup::HandleCommand(int id, int cmd, LPARAM lParam)
{
	for (int i = 0; i < Children.Num(); i++)
	{
		UIElement* ctl = Children[i];
		if (ctl->IsGroup || ctl->Id == id)
		{
			// pass command to control
			if (ctl->HandleCommand(id, cmd, lParam))
				return true;
		}
	}
	return false;
}


void UIGroup::Create(UIBaseDialog* dialog)
{
	CreateGroupControls(dialog);
}


void UIGroup::CreateGroupControls(UIBaseDialog* dialog)
{
	if (Parent)
	{
		Parent->AddVerticalSpace();
		// request x, y and width; height is not available yet
		int h = 0;
		Parent->AllocateUISpace(X, Y, Width, h);
	}
	if (!NoBorder)
	{
		Height = TopBorder = GROUP_MARGIN_TOP;
	}
	else
	{
		if (Height < 0)		// allow custom initial Height for vertical spacing
			Height = TopBorder = 0;
	}

	AddCustomControls();

	// call 'Create' for all children
	int maxControlY = Y + Height;
	for (int i = 0; i < Children.Num(); i++)
	{
		UIElement* control = Children[i];
		control->Create(dialog);
		int bottom = control->Y + control->Height;
		if (bottom > maxControlY)
			maxControlY = bottom;
	}
	Height = maxControlY - Y;

	if (!NoBorder)
		Height += GROUP_MARGIN_BOTTOM;

	if (Parent)
	{
		Parent->Height += Height;
	}

	if (!NoBorder)
	{
		// create a group window (border)
		Wnd = Window(WC_BUTTON, *Label, WS_CHILDWINDOW | BS_GROUPBOX | WS_GROUP | WS_VISIBLE, dialog);
	}

	if (Parent)
		Parent->AddVerticalSpace();
}


/*-----------------------------------------------------------------------------
	UIBaseDialog
-----------------------------------------------------------------------------*/

UIBaseDialog::UIBaseDialog()
:	NextDialogId(FIRST_DIALOG_ID)
{
	NoBorder = true;
}


UIBaseDialog::~UIBaseDialog()
{
}


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
	dlg1->style = WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT | DS_MODALFRAME | DS_CENTER;
	dlg1->cx = width;
	dlg1->cy = height;

	int titleLen = wcslen(title);
	assert(titleLen < MAX_TITLE_LEN);
	wcscpy(dlg1->title, title);

	DLGTEMPLATEEX2* dlg2 = (DLGTEMPLATEEX2*)(buffer + sizeof(DLGTEMPLATEEX1) + titleLen * sizeof(wchar_t));
	dlg2->pointsize = fontSize;
	wcscpy(dlg2->typeface, fontName);

	return (DLGTEMPLATE*)buffer;
}


bool UIBaseDialog::ShowDialog(const char* title, int width, int height)
{
	guard(UIBaseDialog::ShowDialog);

	if (!hInstance) hInstance = GetModuleHandle(NULL);

	// convert title to unicode
	wchar_t wTitle[MAX_TITLE_LEN];
	mbstowcs(wTitle, title, MAX_TITLE_LEN);
	DLGTEMPLATE* tmpl = MakeDialogTemplate(width, height, wTitle, L"MS Shell Dlg", 8);

#if 1
	// modal
	int result = DialogBoxIndirectParam(
		hInstance,					// hInstance
		tmpl,						// lpTemplate
		0,							// hWndParent
		StaticWndProc,				// lpDialogFunc
		(LPARAM)this				// lParamInit
	);
	appPrintf("dialog result: %d\n", result);
#else
	// modeless
	HWND dialog = CreateDialogIndirectParam(
		hInstance,					// hInstance
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

	unguard;
}


INT_PTR CALLBACK UIBaseDialog::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UIBaseDialog* dlg;

	if (msg == WM_INITDIALOG)
	{
		// remember pointer to UIBaseDialog in window's user data
		dlg = (UIBaseDialog*)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
	}
	else
	{
		// retrieve pointer to UIBaseDialog
		dlg = (UIBaseDialog*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}
	return dlg->WndProc(hWnd, msg, wParam, lParam);
}


INT_PTR UIBaseDialog::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	guard(UIBaseDialog::WndProc);

	// handle dialog initialization
	if (msg == WM_INITDIALOG)
	{
		Wnd = hWnd;

		// center window on screen
		RECT controlRect;
		GetClientRect(hWnd, &controlRect);
#if 0	// current implementation uses DS_CENTER
		int newX = (GetSystemMetrics(SM_CXSCREEN) - (controlRect.right - controlRect.left)) / 2;
		int newY = (GetSystemMetrics(SM_CYSCREEN) - (controlRect.bottom - controlRect.top)) / 2;
		SetWindowPos(hWnd, NULL, newX, newY, 0, 0, SWP_NOSIZE);
#endif

		// create controls
		X = DEFAULT_HORZ_BORDER;
		Y = 0;
		Width = controlRect.right - controlRect.left - DEFAULT_HORZ_BORDER * 2;
		Height = DEFAULT_VERT_BORDER;
		InitUI();
		CreateGroupControls(this);

		return TRUE;
	}

	if (msg == WM_CLOSE)
	{
		EndDialog(Wnd, IDCANCEL);
		return TRUE;
	}

	if (msg == WM_DESTROY)
	{
//!!		RemoveRollout();
		return TRUE;
	}

	int cmd = -1;
	int id = 0;

	// retrieve pointer to our class from user data
	if (msg == WM_COMMAND)
	{
		id  = LOWORD(wParam);
		cmd = HIWORD(wParam);
		appPrintf("WM_COMMAND cmd=%d id=%d\n", cmd, id);
		if (id == IDOK || id == IDCANCEL)
		{
			EndDialog(Wnd, id);
			return TRUE;
		}
	}

	if (cmd == -1)
		return FALSE;

	if (id < FIRST_DIALOG_ID || id >= NextDialogId)
		return TRUE;				// not any of our controls

	bool res = HandleCommand(id, cmd, lParam);   // returns 'true' if command was processed

	return res ? TRUE : FALSE;

	unguard;
}


#endif // HAS_UI
