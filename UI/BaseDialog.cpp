#if _WIN32
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#include <windows.h>
#include <CommCtrl.h>
#endif

#include "BaseDialog.h"

/*!! TODO
- SHBrowseForFolder replacement with GetOpenFileName:
  http://microsoft.public.win32.programmer.ui.narkive.com/YKMAHx5L/getopenfilename-to-select-a-folder
  http://stackoverflow.com/questions/31059/how-do-you-configure-an-openfiledialog-to-select-folders/510035#510035
  http://www.codeproject.com/Articles/16276/Customizing-OpenFileDialog-in-NET
- IFileDialog
  http://msdn.microsoft.com/en-us/library/windows/desktop/bb776913(v=vs.85).aspx
*/

/* Useful links:
- http://blogs.msdn.com/b/oldnewthing/archive/2004/07/30/201988.aspx
  creating dialogs as child windows
- http://msdn.microsoft.com/en-us/library/windows/desktop/bb773175(v=vs.85).aspx
  Enabling Visual Styles
*/


#if HAS_UI

#define FIRST_DIALOG_ID				4000

#define VERTICAL_SPACING			4
#define DEFAULT_VERT_BORDER			9
#define DEFAULT_HORZ_BORDER			7

#define DEFAULT_LABEL_HEIGHT		12
#define DEFAULT_BUTTON_HEIGHT		20
#define DEFAULT_CHECKBOX_HEIGHT		16
#define DEFAULT_EDIT_HEIGHT			16
#define DEFAULT_COMBOBOX_HEIGHT		20

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
,	Enabled(true)
,	Parent(NULL)
,	Wnd(0)
,	Id(0)
,	CreateChain(NULL)
{}

UIElement::~UIElement()
{}

void UIElement::Enable(bool enable)
{
	if (Enabled == enable) return;
	Enabled = enable;
	UpdateEnabled();
}

void UIElement::UpdateEnabled()
{
	if (!Wnd) return;
	EnableWindow(Wnd, Enabled ? TRUE : FALSE);
	InvalidateRect(Wnd, NULL, TRUE);
}

UIElement& UIElement::SetRect(int x, int y, int width, int height)
{
	if (x != -1)      X = x;
	if (y != -1)      Y = y;
	if (width != -1)  Width = width;
	if (height != -1) Height = height;
	return *this;
}

UIElement& UIElement::SetParent(UIGroup* group)
{
	group->Add(this);
	return *this;
}

void UIElement::MeasureTextSize(const char* text, int* width, int* height, HWND wnd)
{
	guard(UIElement::MeasureTextSize);
	if (!wnd) wnd = Wnd;
	assert(wnd);
	HDC dc = GetDC(wnd);
	HGDIOBJ oldFont = SelectObject(dc, (HFONT)SendMessage(wnd, WM_GETFONT, 0, 0));
	SIZE size;
	//?? probably use DrawText() with DT_CALCRECT instead of GetTextExtentPoint32()
	GetTextExtentPoint32(dc, text, strlen(text), &size);
	if (width) *width = size.cx;
	if (height) *height = size.cy;
	SelectObject(dc, oldFont);
	ReleaseDC(wnd, dc);
	unguard;
}

HWND UIElement::Window(const char* className, const char* text, DWORD style, DWORD exstyle, UIBaseDialog* dialog,
	int id, int x, int y, int w, int h)
{
	if (x == -1) x = X;
	if (y == -1) y = Y;
	if (w == -1) w = Width;
	if (h == -1) h = Height;
	if (id == -1) id = Id;

	HWND dialogWnd = dialog->GetWnd();

	HWND wnd = CreateWindowEx(exstyle, className, text, style, x, y, w, h,
		dialogWnd, (HMENU)id, hInstance, NULL);
	SendMessage(wnd, WM_SETFONT, SendMessage(dialogWnd, WM_GETFONT, 0, 0), MAKELPARAM(TRUE, 0));

	return wnd;
}

// This function will add another UIElement to chain for creation. This function
// has a little overhead: it iterates over all already added elements to add a new
// one to the end of CreateChain. But it allows us to use UE4-like "declarative syntax"
// for creating controls.
UIElement& operator+(UIElement& elem, UIElement& next)
{
	UIElement* e = &elem;
	while (true)
	{
		UIElement* n = e->CreateChain;
		if (!n)
		{
			e->CreateChain = &next;
			break;
		}
		e = n;
	}
	return elem;
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

static int ConvertTextAlign(ETextAlign align)
{
	if (align == TA_Left)
		return SS_LEFT;
	else if (align == TA_Right)
		return SS_RIGHT;
	else
		return SS_CENTER;
}

void UILabel::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(WC_STATIC, *Label, WS_CHILDWINDOW | ConvertTextAlign(Align) | WS_VISIBLE, 0, dialog);
	UpdateEnabled();
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
	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP | WS_CHILDWINDOW | WS_VISIBLE, 0, dialog);
	UpdateEnabled();
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

	// compute width of checkbox, otherwise it would react on whole parent's width area
	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth, NULL, DlgWnd);

	// add DEFAULT_CHECKBOX_HEIGHT to 'Width' to include checkbox rect
	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP | WS_CHILDWINDOW | WS_VISIBLE | BS_AUTOCHECKBOX, 0, dialog,
		Id, X, Y, min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Width));

	CheckDlgButton(DlgWnd, Id, *pValue ? BST_CHECKED : BST_UNCHECKED);
	UpdateEnabled();
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
	UITextEdit
-----------------------------------------------------------------------------*/

UITextEdit::UITextEdit(const char* value)
:	pValue(&sValue)		// points to local variable
,	sValue(value)
{
	Height = DEFAULT_EDIT_HEIGHT;
}

UITextEdit::UITextEdit(FString* value)
:	pValue(value)
//,	sValue(value) - uninitialized
{
	Height = DEFAULT_EDIT_HEIGHT;
}

void UITextEdit::SetText(const char* text)
{
	if (text)
		*pValue = text;
	if (Wnd)
		SetWindowText(Wnd, *(*pValue));
}

const char* UITextEdit::GetText()
{
	UpdateText();
	return *(*pValue);
}

void UITextEdit::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Id = dialog->GenerateDialogId();

	Wnd = Window(WC_EDIT, "", WS_TABSTOP | WS_CHILDWINDOW | WS_VISIBLE, WS_EX_CLIENTEDGE, dialog);
	SetWindowText(Wnd, *(*pValue));
	UpdateEnabled();
}

bool UITextEdit::HandleCommand(int id, int cmd, LPARAM lParam)
{
	return true;
}

void UITextEdit::UpdateText()
{
	if (!Wnd) return;
	int len = GetWindowTextLength(Wnd) + 1;
	FString& S = *pValue;
	S.Empty(len+1);
	S.Add(len+1);
	GetWindowText(Wnd, &S[0], len + 1);
}

void UITextEdit::DialogClosed(bool cancel)
{
	if (cancel) return;
	UpdateText();
}


/*-----------------------------------------------------------------------------
	UICombobox
-----------------------------------------------------------------------------*/

UICombobox::UICombobox()
:	Value(-1)
{
	Height = DEFAULT_COMBOBOX_HEIGHT;
}

void UICombobox::AddItem(const char* item)
{
	new (Items) FString(item);
	if (Wnd) SendMessage(Wnd, CB_ADDSTRING, 0, (LPARAM)item);
}

void UICombobox::AddItems(const char** items)
{
	while (*items) AddItem(*items++);
}

void UICombobox::RemoveAllItems()
{
	Items.Empty();
	if (Wnd) SendMessage(Wnd, CB_RESETCONTENT, 0, 0);
	Value = -1;
}

void UICombobox::SelectItem(int index)
{
	if (Value == index) return;
	Value = index;
	if (Wnd) SendMessage(Wnd, CB_SETCURSEL, Value, 0);
}

void UICombobox::SelectItem(const char* item)
{
	int index = -1;
	for (int i = 0; i < Items.Num(); i++)
	{
		if (!stricmp(Items[i], item))
		{
			index = i;
			break;
		}
	}
	SelectItem(index);
}

void UICombobox::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	Id = dialog->GenerateDialogId();

	Wnd = Window(WC_COMBOBOX, "",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILDWINDOW | WS_VSCROLL | WS_TABSTOP | WS_VISIBLE,
		WS_EX_CLIENTEDGE, dialog);
	// add items
	for (int i = 0; i < Items.Num(); i++)
		SendMessage(Wnd, CB_ADDSTRING, 0, (LPARAM)(*Items[i]));
	// set selection
	SendMessage(Wnd, CB_SETCURSEL, Value, 0);
	UpdateEnabled();
}

bool UICombobox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == CBN_SELCHANGE)
	{
		int v = SendMessage(Wnd, CB_GETCURSEL, 0, 0);
		if (v != Value)
		{
			Value = v;
			if (Callback)
				Callback(this, Value, GetSelectionText());
		}
		return true;
	}
	return false;
}


/*-----------------------------------------------------------------------------
	UIGroup
-----------------------------------------------------------------------------*/

UIGroup::UIGroup(const char* label, unsigned flags)
:	Label(label)
,	TopBorder(0)
,	Flags(flags)
{
	IsGroup = true;
}

UIGroup::UIGroup(unsigned flags)
:	TopBorder(0)
,	Flags(flags)
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

	while (item)
	{
		if (item->Parent)
			item->Parent->Remove(item);
		Children.AddItem(item);
		item->Parent = this;
		item = item->CreateChain;
	}

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

	if (!(Flags & GROUP_NO_BORDER))
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
		if (!(Flags & GROUP_NO_AUTO_LAYOUT))
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
	if (Flags & GROUP_NO_AUTO_LAYOUT) return;
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

void UIGroup::DialogClosed(bool cancel)
{
	for (int i = 0; i < Children.Num(); i++)
		Children[i]->DialogClosed(cancel);
}

void UIGroup::EnableAllControls(bool enabled)
{
	for (int i = 0; i < Children.Num(); i++)
		Children[i]->Enable(enabled);
}

void UIGroup::Create(UIBaseDialog* dialog)
{
	CreateGroupControls(dialog);
}

void UIGroup::UpdateEnabled()
{
	EnableAllControls(Enabled);
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
	if (!(Flags & GROUP_NO_BORDER))
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

	if (!(Flags & GROUP_NO_BORDER))
		Height += GROUP_MARGIN_BOTTOM;

	if (Parent)
	{
		Parent->Height += Height;
	}

	if (!(Flags & GROUP_NO_BORDER))
	{
		// create a group window (border)
		Wnd = Window(WC_BUTTON, *Label, WS_CHILDWINDOW | BS_GROUPBOX | WS_GROUP | WS_VISIBLE, 0, dialog);
	}

	if (Parent)
		Parent->AddVerticalSpace();
}


/*-----------------------------------------------------------------------------
	UICheckboxGroup
-----------------------------------------------------------------------------*/

UICheckboxGroup::UICheckboxGroup(const char* label, bool value, unsigned flags)
:	UIGroup(flags)
,	Label(label)
,	Value(value)
,	CheckboxWnd(0)
{}

void UICheckboxGroup::Create(UIBaseDialog* dialog)
{
	UIGroup::Create(dialog);

	Id = dialog->GenerateDialogId();
	DlgWnd = dialog->GetWnd();

	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth);

	CheckboxWnd = Window(WC_BUTTON, *Label, WS_TABSTOP | WS_CHILDWINDOW | WS_VISIBLE | BS_AUTOCHECKBOX, 0, dialog,
		Id, X + GROUP_INDENT, Y, min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Width), DEFAULT_CHECKBOX_HEIGHT);

	CheckDlgButton(DlgWnd, Id, Value ? BST_CHECKED : BST_UNCHECKED);
	EnableAllControls(Value);
}

bool UICheckboxGroup::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (id == Id)
	{
		// checkbox
		bool checked = (IsDlgButtonChecked(DlgWnd, Id) != BST_UNCHECKED);
		if (Value != checked)
		{
			Value = checked;
			EnableAllControls(Value);
			if (Callback)
				Callback(this, checked);
			return true;
		}
	}
	return Super::HandleCommand(id, cmd, lParam);
}


/*-----------------------------------------------------------------------------
	UIBaseDialog
-----------------------------------------------------------------------------*/

UIBaseDialog::UIBaseDialog()
:	UIGroup(GROUP_NO_BORDER)
,	NextDialogId(FIRST_DIALOG_ID)
{}

UIBaseDialog::~UIBaseDialog()
{
}

static DLGTEMPLATE* MakeDialogTemplate(int width, int height, const wchar_t* title, const wchar_t* fontName, int fontSize)
{
	// This code uses volatile structure DLGTEMPLATEEX. It's described in MSDN, but
	// cannot be represented as a structure. We're building DLGTEMPLATEEX here.
	// This code works exactly like declaring dialog in resources.
	// More info:
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms644996%28v=vs.85%29.aspx
	// (has section "Creating a Template in Memory")
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

	if (!hInstance)
	{
		hInstance = GetModuleHandle(NULL);
//		InitCommonControls();
	}

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
#else
	// modeless
	//!! make as separate function
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
		while (GetMessage(&msg, NULL, 0, 0))	//!! there's no exit condition here
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		appPrintf("Dialog closed");
	}
#endif

	return (result != IDCANCEL);

	unguard;
}

void UIBaseDialog::CloseDialog(bool cancel)
{
	DialogClosed(cancel);
	EndDialog(Wnd, cancel ? IDCANCEL : IDOK);
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

		// show dialog's icon; 200 is resource id (we're not using resource.h here)
		SendMessage(hWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(200)));

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
		CloseDialog(true);
		return TRUE;
	}

	if (msg == WM_DESTROY)		//?? destroy local data here?
		return TRUE;

	int cmd = -1;
	int id = 0;

	// retrieve pointer to our class from user data
	if (msg == WM_COMMAND)
	{
		id  = LOWORD(wParam);
		cmd = HIWORD(wParam);
//		appPrintf("WM_COMMAND cmd=%d id=%d\n", cmd, id);
		if (id == IDOK || id == IDCANCEL)
		{
			CloseDialog(id != IDOK);
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
