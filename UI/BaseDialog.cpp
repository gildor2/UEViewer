// Simple UI library.
// Copyright (C) 2018 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#if _WIN32

#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windows headers
#define _CRT_SECURE_NO_WARNINGS
#undef UNICODE

#include <windows.h>
#include <CommCtrl.h>
#include <ShellAPI.h>				// for ShellExecute
#include <Shlwapi.h>				// for DllGetVersion stuff
#endif // _WIN32

#include "BaseDialog.h"


/* Useful links:

- SHBrowseForFolder replacement with GetOpenFileName:
  http://microsoft.public.win32.programmer.ui.narkive.com/YKMAHx5L/getopenfilename-to-select-a-folder
  http://stackoverflow.com/questions/31059/how-do-you-configure-an-openfiledialog-to-select-folders/510035#510035
  http://www.codeproject.com/Articles/16276/Customizing-OpenFileDialog-in-NET
- IFileDialog
  http://msdn.microsoft.com/en-us/library/windows/desktop/bb776913(v=vs.85).aspx
- creating dialogs as child windows
  http://blogs.msdn.com/b/oldnewthing/archive/2004/07/30/201988.aspx
- Enabling Visual Styles
  http://msdn.microsoft.com/en-us/library/windows/desktop/bb773175.aspx
- "Explorer" visual style for TreeView and ListView
  http://msdn.microsoft.com/ru-ru/library/windows/desktop/bb759827.aspx
*/

/* GTK+ notes
- UIMulticolumnListbox and UITreeView could be implemented with GtkTreeView
  https://developer.gnome.org/gtk3/stable/GtkTreeView.html
- LVN_GETDISPINFO doesn't have a direct analogue in GTK+, but workarounds are possible
  http://stackoverflow.com/questions/3164262/lazy-loaded-list-view-in-gtk
  http://stackoverflow.com/questions/23433819/creating-a-simple-file-browser-using-python-and-gtktreeview
- how to use GtkTreeView
  http://scentric.net/tutorial/treeview-tutorial.html
  https://developer.gnome.org/gtkmm-tutorial/3.9/sec-treeview-examples.html.en
  http://habrahabr.ru/post/116268/
- GTK+ C++ interface
  http://www.gtkmm.org/en/index.html
  (warning: possibly uses STL)
*/


#if HAS_UI

#define DEBUG_LAYOUT				0

#if DEBUG_LAYOUT
#define DBG_LAYOUT(...)				appPrintf(__VA_ARGS__)
#else
#define DBG_LAYOUT(...)
#endif

//#define DEBUG_WINDOWS_ERRORS		MAX_DEBUG

//#define DEBUG_MULTILIST_SEL			1
#define USE_EXPLORER_STYLE			1		// use modern control style whenever possible


#define FIRST_DIALOG_ID				4000
#define FIRST_MENU_ID				8000

#define VERTICAL_SPACING			4
#define HORIZONTAL_SPACING			8

#define DEFAULT_VERT_BORDER			13
#define DEFAULT_HORZ_BORDER			7

#define DEFAULT_LABEL_HEIGHT		14
#define DEFAULT_PROGRESS_BAR_HEIGHT	18
#define DEFAULT_BUTTON_HEIGHT		20
#define DEFAULT_CHECKBOX_HEIGHT		18
#define DEFAULT_EDIT_HEIGHT			20
#define DEFAULT_COMBOBOX_HEIGHT		20
#define DEFAULT_COMBOBOX_LIST_HEIGHT 200	// height of combobox dropdown list
#define DEFAULT_LISTBOX_HEIGHT		-1
#define DEFAULT_TREEVIEW_HEIGHT		-1
#define DEFAULT_TREE_ITEM_HEIGHT	0

#define GROUP_INDENT				10
#define GROUP_MARGIN_TOP			16
#define GROUP_MARGIN_BOTTOM			7
#define GROUP_INDENT				10

#define MAX_TITLE_LEN				256


#if _WIN32

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")	// for ShellExecute

static HINSTANCE hInstance;

#endif // _WIN32


/*-----------------------------------------------------------------------------
	UIElement
-----------------------------------------------------------------------------*/

UIElement::UIElement()
:	X(-1)
,	Y(-1)
,	Width(-1)
,	Height(-1)
,	IsGroup(false)
,	IsRadioButton(false)
,	Enabled(true)
,	Visible(true)
,	IsUpdateLocked(false)
,	Parent(NULL)
,	NextChild(NULL)
,	Menu(NULL)
,	Wnd(0)
,	Id(0)
{}

UIElement::~UIElement()
{
	if (Menu) Menu->Detach();
}

const char* UIElement::ClassName() const
{
	return "UIElement";
}

void UIElement::LockUpdate()
{
	LockWindowUpdate(Wnd);
	IsUpdateLocked = true;
}

void UIElement::UnlockUpdate()
{
	LockWindowUpdate(NULL);
	IsUpdateLocked = false;
}

UIElement& UIElement::Enable(bool enable)
{
	if (Enabled == enable) return *this;
	Enabled = enable;
	UpdateEnabled();
	return *this;
}

void UIElement::UpdateEnabled()
{
	if (!Wnd) return;
	EnableWindow(Wnd, Enabled ? TRUE : FALSE);
	InvalidateRect(Wnd, NULL, TRUE);
}

UIElement& UIElement::Show(bool visible)
{
	if (Visible == visible) return *this;
	Visible = visible;
	UpdateVisible();
	return *this;
}

void UIElement::UpdateVisible()
{
	if (!Wnd) return;
	ShowWindow(Wnd, Visible ? SW_SHOW : SW_HIDE);
}

UIElement& UIElement::SetRect(int x, int y, int width, int height)
{
	if (x != -1)      X = x;
	if (y != -1)      Y = y;
	if (width != -1)  Width = width;
	if (height != -1) Height = height;
	return *this;
}

UIElement& UIElement::SetMenu(UIMenu* menu)
{
	if (Menu) Menu->Detach();
	Menu = menu;
	if (Menu) Menu->Attach();
	return *this;
}

UIElement& UIElement::SetParent(UIGroup* group)
{
	group->Add(this);
	return *this;
}

UIBaseDialog* UIElement::GetDialog()
{
	UIElement* control;
	for (control = this; control->Parent != NULL; control = control->Parent)
	{
		// empty
	}
	assert(control->IsA("UIBaseDialog"));
	UIBaseDialog* dialog = static_cast<UIBaseDialog*>(control);
	return dialog;
}

void UIElement::MeasureTextSize(const char* text, int* width, int* height, HWND wnd)
{
	guard(UIElement::MeasureTextSize);
	if (!wnd) wnd = Wnd;
	assert(wnd);
	// set dialog's font for DC
	HDC dc = GetDC(wnd);
	HGDIOBJ oldFont = SelectObject(dc, (HFONT)SendMessage(wnd, WM_GETFONT, 0, 0));
	// measure text size
	SIZE size;
	GetTextExtentPoint32(dc, text, (int)strlen(text), &size);
	if (width) *width = size.cx;
	if (height) *height = size.cy;
	// restore font
	SelectObject(dc, oldFont);
	ReleaseDC(wnd, dc);
	unguard;
}

void UIElement::MeasureTextVSize(const char* text, int* width, int* height, HWND wnd)
{
	guard(UIElement::MeasureTextVSize);
	if (!wnd) wnd = Wnd;
	assert(wnd);

	// get control's width
	int w = *width;
	if (w < 0)
	{
		//!! see AllocateUISpace() for details, should separate common code in some way
//		if (w == -1 && (Parent->Flags & GROUP_HORIZONTAL_LAYOUT))
//			w = Parent->AutoWidth;
//		else
			w = int(DecodeWidth(w) * Parent->Width); //!! see parentWidth in AllocateUISpace()
	}

	// set dialog's font for DC
	HDC dc = GetDC(wnd);
	HGDIOBJ oldFont = SelectObject(dc, (HFONT)SendMessage(wnd, WM_GETFONT, 0, 0));
	// measure text size
	RECT rc;
	rc.bottom = rc.top = 0;
	rc.left = 0;
	rc.right = w;
	int h = DrawText(dc, text, -1, &rc, DT_CALCRECT | DT_WORDBREAK);
	if (width) *width = rc.right - rc.left;
	if (height) *height = h;
	// restore font
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

	if (Visible) style |= WS_VISIBLE;
	HWND wnd = CreateWindowEx(exstyle, className, text, style | WS_CHILDWINDOW, x, y, w, h,
		dialogWnd, (HMENU)(size_t)id, hInstance, NULL);		// convert int -> size_t -> HANDLE to avoid warnings on 64-bit platform
#if DEBUG_WINDOWS_ERRORS
	if (!wnd) appNotify("CreateWindow failed, GetLastError returned %d\n", GetLastError());
#endif
	SendMessage(wnd, WM_SETFONT, SendMessage(dialogWnd, WM_GETFONT, 0, 0), MAKELPARAM(TRUE, 0));

	return wnd;
}

HWND UIElement::Window(const wchar_t* className, const wchar_t* text, DWORD style, DWORD exstyle, UIBaseDialog* dialog,
	int id, int x, int y, int w, int h)
{
	if (x == -1) x = X;
	if (y == -1) y = Y;
	if (w == -1) w = Width;
	if (h == -1) h = Height;
	if (id == -1) id = Id;

	HWND dialogWnd = dialog->GetWnd();

	if (Visible) style |= WS_VISIBLE;
	HWND wnd = CreateWindowExW(exstyle, className, text, style | WS_CHILDWINDOW, x, y, w, h,
		dialogWnd, (HMENU)(size_t)id, hInstance, NULL);
#if DEBUG_WINDOWS_ERRORS
	if (!wnd) appNotify("CreateWindow failed, GetLastError returned %d\n", GetLastError());
#endif
	SendMessage(wnd, WM_SETFONT, SendMessage(dialogWnd, WM_GETFONT, 0, 0), MAKELPARAM(TRUE, 0));

	return wnd;
}

void UIElement::Repaint()
{
	InvalidateRect(Wnd, NULL, TRUE);
}

// This function will add another UIElement to chain for creation. As we're
// using NextChild for both holding a list of group's children and for
// this operation, there's no overhead here - adding a group of controls
// to children performed (in UIGroup::Add) as a simple list concatenation
// operation.
UIElement& operator+(UIElement& elem, UIElement& next)
{
	guard(operator+(UIElement));

	UIElement* e = &elem;
	while (true)
	{
		if (e->Parent) appError("Using UIElement::operator+ to join parented controls");
		UIElement* n = e->NextChild;
		if (!n)
		{
			e->NextChild = &next;
			break;
		}
		e = n;
	}
	return elem;

	unguard;
}

/*-----------------------------------------------------------------------------
	UISpacer
-----------------------------------------------------------------------------*/

UISpacer::UISpacer(int size)
{
	Width = (size != 0) ? size : HORIZONTAL_SPACING;	// use size==-1 for automatic width (for horizontal layout)
	Height = (size > 0) ? size : VERTICAL_SPACING;
}

void UISpacer::Create(UIBaseDialog* dialog)
{
	assert(Parent->UseAutomaticLayout());
	if (Parent->UseVerticalLayout())
	{
		Parent->AddVerticalSpace(Height);
	}
	else
	{
		if (Width > 0)
		{
			Parent->AddHorizontalSpace(Width);
		}
		else
		{
			Parent->AllocateUISpace(X, Y, Width, Height);
		}
	}
}


/*-----------------------------------------------------------------------------
	UIHorizontalLine
-----------------------------------------------------------------------------*/

UIHorizontalLine::UIHorizontalLine()
{
	Width = -1;
	Height = 2;
}

void UIHorizontalLine::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(WC_STATIC, "", SS_ETCHEDHORZ, 0, dialog);
}


/*-----------------------------------------------------------------------------
	UIVerticalLine
-----------------------------------------------------------------------------*/

UIVerticalLine::UIVerticalLine()
{
	Width = 2;
	Height = -1;
	//!! would be nice to get "Height = -1" working; probably would require 2-pass processing of UIGroup
	//!! layout: 1st pass would set Height, for example, to 1, plus remember control which requires height
	//!! change; 2nd pass will resize all remembered controls with the height of group
}

void UIVerticalLine::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(WC_STATIC, "", SS_ETCHEDVERT, 0, dialog);
}


/*-----------------------------------------------------------------------------
	UIBitmap
-----------------------------------------------------------------------------*/

// Check SS_REALSIZEIMAGE, SS_REALSIZECONTROL - may be useful.

UIBitmap::UIBitmap()
:	hImage(0)
{
	Width = Height = 0;		// use dimensions from resource
}

void UIBitmap::LoadResourceImage(int id, UINT type, UINT fuLoad)
{
	HINSTANCE inst = hInstance;
	LPCSTR rid = 0;
	if (id < 0)
	{
		inst = GetModuleHandle("user32.dll");
		switch (id)
		{
		case BI_Warning:
			rid = MAKEINTRESOURCE(101); // IDI_EXCLAMATION
			break;
		case BI_Question:
			rid = MAKEINTRESOURCE(102); // IDI_QUESTION
			break;
		case BI_Error:
			rid = MAKEINTRESOURCE(103); // IDI_ERROR
			break;
		case BI_Information:
			 rid = MAKEINTRESOURCE(104); // IDI_ASTERISK
			 break;
		}
	}
	else
	{
		rid = MAKEINTRESOURCE(id);
	}
	hImage = LoadImage(inst, rid, type, Width, Height, fuLoad);
}

UIBitmap& UIBitmap::SetResourceIcon(int resId)
{
	IsIcon = true;
	LoadResourceImage(resId, IMAGE_ICON, LR_SHARED);
#if DEBUG_WINDOWS_ERRORS
	if (!hImage)
		appPrintf("UIBitmap::SetResourceIcon: %d\n", GetLastError());
#endif
	// Note: can't get icon dimensione using GetObject() - this function would fail.
	if ((!Width || !Height) && hImage)
	{
		Width = GetSystemMetrics(SM_CXICON);
		Height = GetSystemMetrics(SM_CYICON);
	}
	return *this;
}

//!! WARNINGS:
//!! - This code (IsIcon==false) is not tested!
UIBitmap& UIBitmap::SetResourceBitmap(int resId)
{
	IsIcon = false;
	LoadResourceImage(resId, IMAGE_BITMAP, LR_SHARED);
#if DEBUG_WINDOWS_ERRORS
	if (!hImage)
		appPrintf("UIBitmap::SetResourceBitmap: %d\n", GetLastError());
#endif
	if ((!Width || !Height) && hImage)
	{
		BITMAP bm;
		memset(&bm, 0, sizeof(bm));
		GetObject(hImage, sizeof(bm), &bm);
		Width = bm.bmWidth;
		Height = bm.bmHeight;
	}
	return *this;
}

void UIBitmap::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(WC_STATIC, "", IsIcon ? SS_ICON : SS_BITMAP, 0, dialog);
	if (Wnd && hImage) SendMessage(Wnd, STM_SETIMAGE, IsIcon ? IMAGE_ICON : IMAGE_BITMAP, (LPARAM)hImage);
}


/*-----------------------------------------------------------------------------
	UILabel
-----------------------------------------------------------------------------*/

UILabel::UILabel(const char* text, ETextAlign align)
:	Label(text)
,	Align(align)
,	AutoSize(false)
{
	Height = DEFAULT_LABEL_HEIGHT;
}

void UILabel::SetText(const char* text)
{
	// Avoid flicker and memory reallocation when label is not changed (useful for
	// labels which are used as status text for some operation).
	if (strcmp(text, *Label) != 0)
	{
		Label = text;
		if (Wnd) SetWindowText(Wnd, *Label);
	}
}

void UILabel::UpdateSize(UIBaseDialog* dialog)
{
	if (AutoSize)
	{
		int labelWidth;
		MeasureTextSize(*Label, &labelWidth, NULL, dialog->GetWnd());
		Width = labelWidth;
	}
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
	if (Height == -1)
	{
		// Auto-size: compute label's height
		int labelWidth, labelHeight;
		labelWidth = Width;
		MeasureTextVSize(*Label, &labelWidth, &labelHeight, dialog->GetWnd());
		Height = labelHeight;
	}
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(WC_STATIC, *Label, ConvertTextAlign(Align), 0, dialog);
	UpdateEnabled();
}


/*-----------------------------------------------------------------------------
	UIHyperLink
-----------------------------------------------------------------------------*/

UIHyperLink::UIHyperLink(const char* text, const char* link /*, ETextAlign align*/)
:	UILabel(text /*, align*/)
,	Link(link)
{}

void UIHyperLink::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Id = dialog->GenerateDialogId();

#if 0
	// works without this code
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_LINK_CLASS;
	InitCommonControlsEx(&iccex);
#endif

#if 0
	// Unicode version (this control, as mentioned in SysLink documentaion, in Unicode-only)
	wchar_t buffer[MAX_TITLE_LEN];
	appSprintf(ARRAY_ARG(buffer), L"<a href=\"%S\">%S</a>", *Link, *Label);
	Wnd = Window(WC_LINK, buffer, ConvertTextAlign(Align), 0, dialog);
#else
	// ANSI version works too
	char buffer[MAX_TITLE_LEN];
	appSprintf(ARRAY_ARG(buffer), "<a href=\"%s\">%s</a>", *Link, *Label);
	Wnd = Window("SysLink", buffer, ConvertTextAlign(Align), 0, dialog);
#endif
	if (!Wnd)
	{
		// Fallback to ordinary label if SysLink was not created for some reason.
		// Could also change text color:
		// http://stackoverflow.com/questions/1525669/set-static-text-color-win32
		Wnd = Window(WC_STATIC, *Label, ConvertTextAlign(Align) | SS_NOTIFY, 0, dialog);
	}

	UpdateEnabled();
}

bool UIHyperLink::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == NM_CLICK || cmd == NM_RETURN || cmd == STN_CLICKED) // STN_CLICKED for WC_STATIC fallback
	{
		ShellExecute(NULL, "open", *Link, NULL, NULL, SW_SHOW);
	}
	return true;
}


/*-----------------------------------------------------------------------------
	UIProgressBar
-----------------------------------------------------------------------------*/

UIProgressBar::UIProgressBar()
:	Value(0)
{
	Height = DEFAULT_PROGRESS_BAR_HEIGHT;
}

void UIProgressBar::SetValue(float value)
{
	if (value == Value) return;
	Value = value;
	if (Wnd) SendMessage(Wnd, PBM_SETPOS, (int)(Value * 16384), 0);
}

void UIProgressBar::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Wnd = Window(PROGRESS_CLASS, "", 0, 0, dialog);
	SendMessage(Wnd, PBM_SETRANGE, 0, MAKELPARAM(0, 16384));
	if (Wnd) SendMessage(Wnd, PBM_SETPOS, (int)(Value * 16384), 0);
}


/*-----------------------------------------------------------------------------
	UIButton
-----------------------------------------------------------------------------*/

UIButton::UIButton(const char* text)
:	Label(text)
{
	Height = DEFAULT_BUTTON_HEIGHT;
}

UIButton& UIButton::SetOK()
{
	Id = IDOK;
	return *this;
}

UIButton& UIButton::SetCancel()
{
	Id = IDCANCEL;
	return *this;
}

void UIButton::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	if (Id == 0 || Id >= FIRST_DIALOG_ID)
		Id = dialog->GenerateDialogId();		// do not override Id which was set outside of Create()

	//!! BS_DEFPUSHBUTTON - for default key
	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP, 0, dialog);
	UpdateEnabled();
}

bool UIButton::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == BN_CLICKED && Callback)
		Callback(this);
	return true;
}


/*-----------------------------------------------------------------------------
	UIMenuButton
-----------------------------------------------------------------------------*/

static int GetComctl32Version()
{
	static DWORD dwVersion = 0;
	if (dwVersion) return dwVersion;	// already checked

	HINSTANCE hInstDll = GetModuleHandle("comctl32.dll");
	if (!hInstDll) return 0;

	DLLGETVERSIONPROC pDllGetVersion;
	pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hInstDll, "DllGetVersion");
	if (!pDllGetVersion) return 0;

	DLLVERSIONINFO dvi;
	memset(&dvi, 0, sizeof(dvi));
	dvi.cbSize = sizeof(dvi);

	HRESULT hr = (*pDllGetVersion)(&dvi);
	if (SUCCEEDED(hr))
		dwVersion = (dvi.dwMajorVersion << 8) | dvi.dwMinorVersion;
	return dwVersion;
}

UIMenuButton::UIMenuButton(const char* text)
:	Label(text)
{
	Height = DEFAULT_BUTTON_HEIGHT;
}

/*UIMenuButton& UIMenuButton::SetOK()
{
	Id = IDOK;
	return *this;
}

UIMenuButton& UIMenuButton::SetCancel()
{
	Id = IDCANCEL;
	return *this;
}*/

void UIMenuButton::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	if (Id == 0 || Id >= FIRST_DIALOG_ID)
		Id = dialog->GenerateDialogId();		// do not override Id which was set outside of Create()

	// should check Common Controls library version - if it's too low, control's window
	// will be created anyway, but will look completely wrong
	DWORD flags = WS_TABSTOP;
	if (GetComctl32Version() >= 0x600) flags |= BS_SPLITBUTTON; // not supported in comctl32.dll prior version 6.00
	Wnd = Window(WC_BUTTON, *Label, flags, 0, dialog);
	UpdateEnabled();
}

bool UIMenuButton::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == BCN_DROPDOWN || (cmd == BN_CLICKED && !Callback))
	{
		// reference: MFC, CSplitButton::OnDropDown()
		// create menu or get its handle
		HMENU hMenu = Menu->GetHandle(true, true);
		// get rect of button for menu positioning
		RECT rectButton;
		GetWindowRect(Wnd, &rectButton);
		TPMPARAMS tpmParams;
		tpmParams.cbSize = sizeof(TPMPARAMS);
		tpmParams.rcExclude = rectButton;
		int cmd = TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
			rectButton.left, rectButton.bottom, Wnd, &tpmParams);
		if (cmd)
			Menu->HandleCommand(cmd);
	}
	else if (cmd == BN_CLICKED && Callback)
		Callback(this);
	return true;
}


/*-----------------------------------------------------------------------------
	UICheckbox
-----------------------------------------------------------------------------*/

UICheckbox::UICheckbox(const char* text, bool value, bool autoSize)
:	Label(text)
,	bValue(value)
,	pValue(&bValue)		// points to local variable
,	AutoSize(autoSize)
{
	Height = DEFAULT_CHECKBOX_HEIGHT;
}

UICheckbox::UICheckbox(const char* text, bool* value, bool autoSize)
:	Label(text)
//,	bValue(value) - uninitialized, unused
,	pValue(value)
,	AutoSize(autoSize)
{
	Height = DEFAULT_CHECKBOX_HEIGHT;
}

void UICheckbox::UpdateSize(UIBaseDialog* dialog)
{
	if (AutoSize)
	{
		int checkboxWidth;
		MeasureTextSize(*Label, &checkboxWidth, NULL, dialog->GetWnd());
		Width = checkboxWidth + DEFAULT_CHECKBOX_HEIGHT;
	}
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
	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, dialog,
		Id, X, Y, min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Width));

	CheckDlgButton(DlgWnd, Id, *pValue ? BST_CHECKED : BST_UNCHECKED);
	UpdateEnabled();
}

bool UICheckbox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd != BN_CLICKED) return false;
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
	UIRadioButton
-----------------------------------------------------------------------------*/

UIRadioButton::UIRadioButton(const char* text, bool autoSize)
:	Label(text)
,	Checked(false)
,	Value(0)
,	AutoSize(autoSize)
{
	IsRadioButton = true;
	Height = DEFAULT_CHECKBOX_HEIGHT;
}

UIRadioButton::UIRadioButton(const char* text, int value, bool autoSize)
:	Label(text)
,	Checked(false)
,	Value(value)
,	AutoSize(autoSize)
{
	IsRadioButton = true;
	Height = DEFAULT_CHECKBOX_HEIGHT;
}

void UIRadioButton::UpdateSize(UIBaseDialog* dialog)
{
	if (AutoSize)
	{
		int radioWidth;
		MeasureTextSize(*Label, &radioWidth, NULL, dialog->GetWnd());
		Width = radioWidth + DEFAULT_CHECKBOX_HEIGHT;
	}
}

void UIRadioButton::Create(UIBaseDialog* dialog)
{
	Parent->AllocateUISpace(X, Y, Width, Height);
	Id = dialog->GenerateDialogId();

	HWND DlgWnd = dialog->GetWnd();

	// compute width of checkbox, otherwise it would react on whole parent's width area
	int radioWidth;
	MeasureTextSize(*Label, &radioWidth, NULL, DlgWnd);

	// add DEFAULT_CHECKBOX_HEIGHT to 'Width' to include checkbox rect
	Wnd = Window(WC_BUTTON, *Label, WS_TABSTOP | BS_AUTORADIOBUTTON, 0, dialog,
		Id, X, Y, min(radioWidth + DEFAULT_CHECKBOX_HEIGHT, Width));

//	CheckDlgButton(DlgWnd, Id, *pValue ? BST_CHECKED : BST_UNCHECKED);
	UpdateEnabled();
}

void UIRadioButton::ButtonSelected(bool value)
{
	if (value == Checked) return;
	Checked = value;
	if (Callback)
		Callback(this, value);
}

void UIRadioButton::SelectButton()
{
	SendMessage(Wnd, BM_SETCHECK, BST_CHECKED, 0);
}

bool UIRadioButton::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == BN_CLICKED)
	{
		Parent->RadioButtonClicked(this);
		ButtonSelected(true);
	}
	return true;
}


/*-----------------------------------------------------------------------------
	UITextEdit
-----------------------------------------------------------------------------*/

UITextEdit::UITextEdit(const char* value)
:	pValue(&sValue)		// points to local variable
,	sValue(value)
,	IsMultiline(false)
,	IsReadOnly(false)
,	IsWantFocus(true)
,	TextDirty(false)
{
	Height = DEFAULT_EDIT_HEIGHT;
}

UITextEdit::UITextEdit(FString* value)
:	pValue(value)
//,	sValue(value) - uninitialized, unused
,	IsMultiline(false)
,	IsReadOnly(false)
,	IsWantFocus(true)
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

	int style = (IsWantFocus) ? WS_TABSTOP : 0;
	if (IsMultiline)
	{
		style |= WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL;
	}
	else
	{
		style |= ES_AUTOHSCROLL;
	}
	if (IsReadOnly)  style |= ES_READONLY;

	Wnd = Window(WC_EDIT, "", style, WS_EX_CLIENTEDGE, dialog);
	SetWindowText(Wnd, *(*pValue));
	UpdateEnabled();

	// Remove limit of 30k characters
	SendMessage(Wnd, EM_SETLIMITTEXT, 0x100000, 0);
}

bool UITextEdit::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == EN_CHANGE)
	{
		// don't update pValue every time when text changed unless
		// we need to execute callback
		TextDirty = true;
		if (Callback)
		{
			UpdateText();
			Callback(this, **pValue);
		}
	}
	else if (cmd == EN_KILLFOCUS)
	{
		// this callback is called when control looses focus or when dialog window is closed
		UpdateText();
	}
	return true;
}

void UITextEdit::UpdateText()
{
	if (!Wnd) return;
	if (!TextDirty) return;
	TextDirty = true;

	int len = GetWindowTextLength(Wnd);
	char* buf = new char[len+1];
	GetWindowText(Wnd, buf, len + 1);

	*pValue = buf;
	delete[] buf;
}

void UITextEdit::AppendText(const char* text)
{
	int currLength = GetWindowTextLength(Wnd);
	SendMessage(Wnd, EM_SETSEL, currLength, currLength);
	SendMessage(Wnd, EM_REPLACESEL, FALSE, (LPARAM)text);
	SendMessage(Wnd, EM_SCROLLCARET, 0, 0);
}


/*-----------------------------------------------------------------------------
	UICombobox
-----------------------------------------------------------------------------*/

UICombobox::UICombobox()
:	Value(-1)
{
	Height = DEFAULT_COMBOBOX_HEIGHT;
}

UICombobox& UICombobox::AddItem(const char* item)
{
	new (Items) FString(item);
	if (Wnd) SendMessage(Wnd, CB_ADDSTRING, 0, (LPARAM)item);
	return *this;
}

UICombobox& UICombobox::AddItems(const char** items)
{
	while (*items) AddItem(*items++);
	return *this;
}

void UICombobox::RemoveAllItems()
{
	Items.Empty();
	if (Wnd) SendMessage(Wnd, CB_RESETCONTENT, 0, 0);
	Value = -1;
}

UICombobox& UICombobox::SelectItem(int index)
{
	if (Value == index) return *this;
	Value = index;
	if (Wnd) SendMessage(Wnd, CB_SETCURSEL, Value, 0);
	return *this;
}

UICombobox& UICombobox::SelectItem(const char* item)
{
	int index = -1;
	for (int i = 0; i < Items.Num(); i++)
	{
		if (!stricmp(*Items[i], item))
		{
			index = i;
			break;
		}
	}
	SelectItem(index);
	return *this;
}

void UICombobox::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	Id = dialog->GenerateDialogId();

	// Note: we're sending DEFAULT_COMBOBOX_LIST_HEIGHT instead of control's Height here, otherwise
	// dropdown list could appear empty (with zero height) or systems not supporting visual styles
	// (pre-XP Windows) or when dropdown list is empty
	Wnd = Window(WC_COMBOBOX, "",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE, dialog,
		Id, -1, -1, -1, DEFAULT_COMBOBOX_LIST_HEIGHT);
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
		int v = (int)SendMessage(Wnd, CB_GETCURSEL, 0, 0);
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
	UIListbox
-----------------------------------------------------------------------------*/

UIListbox::UIListbox()
:	Value(-1)
{
	Height = DEFAULT_LISTBOX_HEIGHT;
}

UIListbox& UIListbox::ReserveItems(int count)
{
	Items.ResizeTo(Items.Num() + count);
	return *this;
}

UIListbox& UIListbox::AddItem(const char* item)
{
	new (Items) FString(item);
	if (Wnd) SendMessage(Wnd, LB_ADDSTRING, 0, (LPARAM)item);
	return *this;
}

UIListbox& UIListbox::AddItems(const char** items)
{
	while (*items) AddItem(*items++);
	return *this;
}

void UIListbox::RemoveAllItems()
{
	Items.Empty();
	if (Wnd) SendMessage(Wnd, LB_RESETCONTENT, 0, 0);
	Value = -1;
}

UIListbox& UIListbox::SelectItem(int index)
{
	if (Value == index) return *this;
	Value = index;
	if (Wnd) SendMessage(Wnd, LB_SETCURSEL, Value, 0);
	return *this;
}

UIListbox& UIListbox::SelectItem(const char* item)
{
	int index = -1;
	for (int i = 0; i < Items.Num(); i++)
	{
		if (!stricmp(*Items[i], item))
		{
			index = i;
			break;
		}
	}
	SelectItem(index);
	return *this;
}

void UIListbox::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	Id = dialog->GenerateDialogId();

	Wnd = Window(WC_LISTBOX, "",
		LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE, dialog);
	// add items
	for (int i = 0; i < Items.Num(); i++)
		SendMessage(Wnd, LB_ADDSTRING, 0, (LPARAM)(*Items[i]));
	// set selection
	SendMessage(Wnd, LB_SETCURSEL, Value, 0);
	UpdateEnabled();
}

bool UIListbox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == LBN_SELCHANGE || cmd == LBN_DBLCLK)
	{
		int v = (int)SendMessage(Wnd, LB_GETCURSEL, 0, 0);
		if (v != Value)
		{
			Value = v;
			if ((cmd == LBN_SELCHANGE) && Callback)
				Callback(this, Value, GetSelectionText());
		}
		if ((cmd == LBN_DBLCLK) && DblClickCallback)
			DblClickCallback(this, Value, GetSelectionText());
		return true;
	}
	return false;
}


/*-----------------------------------------------------------------------------
	UIMulticolumnListbox
-----------------------------------------------------------------------------*/

#if USE_EXPLORER_STYLE

static void SetExplorerTheme(HWND Wnd)
{
	static bool loaded = false;
	static HRESULT (WINAPI *SetWindowTheme)(HWND, LPCWSTR, LPCWSTR) = NULL;

	if (!loaded)
	{
		loaded = true;
		HMODULE hDll = LoadLibrary("uxtheme.dll");
		if (hDll == NULL) return;
		SetWindowTheme = (HRESULT (WINAPI *)(HWND, LPCWSTR, LPCWSTR))GetProcAddress(hDll, "SetWindowTheme");
	}

	if (SetWindowTheme != NULL)
	{
		SetWindowTheme(Wnd, L"Explorer", NULL);
//??	SendMessage(Wnd, TVM_SETEXTENDEDSTYLE, 0, TVS_EX_DOUBLEBUFFER);
//??	SendMessage(Wnd, TV_FIRST + 44, 0x0004, 0);
	}
}

#endif // USE_EXPLORER_STYLE

UIMulticolumnListbox::UIMulticolumnListbox(int numColumns)
:	NumColumns(numColumns)
,	Multiselect(false)
,	IsVirtualMode(false)
,	SortColumn(-1)
,	SortMode(false)
{
	Height = DEFAULT_LISTBOX_HEIGHT;
	assert(NumColumns > 0 && NumColumns <= MAX_COLUMNS);
	Items.AddZeroed(numColumns);	// reserve place for header
}

UIMulticolumnListbox& UIMulticolumnListbox::AddColumn(const char* title, int width, ETextAlign align)
{
	// find first empty column
	for (int i = 0; i < NumColumns; i++)
	{
		if (Items[i].IsEmpty())
		{
			Items[i] = title;
			ColumnSizes[i] = width;
			ColumnAlign[i] = align;
			return *this;
		}
	}
	appError("UIMulticolumnListbox: too many columns");
	return *this;
}

void UIMulticolumnListbox::UpdateListViewHeaderSort()
{
	HWND hHeader = ListView_GetHeader(Wnd);
	if (hHeader)
	{
		for (int i = 0; i < NumColumns; i++)
		{
			HDITEM hItem;
			hItem.mask = HDI_FORMAT;
			if (Header_GetItem(hHeader, i, &hItem))
			{
				if (i == SortColumn)
				{
					// sorting with this column
					if (SortMode)
						hItem.fmt = (hItem.fmt & ~HDF_SORTUP) | HDF_SORTDOWN;
					else
						hItem.fmt = (hItem.fmt & ~HDF_SORTDOWN) | HDF_SORTUP;
				}
				else
				{
					// not sorting with this column
					hItem.fmt = hItem.fmt & ~(HDF_SORTDOWN|HDF_SORTUP);
				}
				Header_SetItem(hHeader, i, &hItem);
			}
		}
	}
}

UIMulticolumnListbox& UIMulticolumnListbox::ShowSortArrow(int columnIndex, bool reverseSort)
{
	if ((SortColumn != columnIndex) || (SortMode != reverseSort))
	{
		SortColumn = columnIndex;
		SortMode = reverseSort;
		if (Wnd)
		{
			UpdateListViewHeaderSort();
		}
	}
	return *this;
}

UIMulticolumnListbox& UIMulticolumnListbox::ReserveItems(int count)
{
	Items.ResizeTo((GetItemCount() + count + 1) * NumColumns);
	return *this;
}

int UIMulticolumnListbox::AddItem(const char* item)
{
	guard(UIMulticolumnListbox::AddItem);

	assert(Items.Num() % NumColumns == 0);
	int numItems = GetItemCount();
	int index = Items.AddZeroed(NumColumns);
	Items[index] = item;

	if (Wnd)
	{
		if (!IsVirtualMode)
		{
			LVITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.iSubItem = 0;
			lvi.iItem = numItems;
			lvi.lParam = numItems;
			ListView_InsertItem(Wnd, &lvi);
		}
		else if (!IsUpdateLocked)
		{
			ListView_SetItemCount(Wnd, GetItemCount());
		}
	}

	return numItems;

	unguard;
}

void UIMulticolumnListbox::UnlockUpdate()
{
	Super::UnlockUpdate();
	if (IsVirtualMode)
	{
		// for virtual mode, update items only once
		ListView_SetItemCount(Wnd, GetItemCount());
	}
}

void UIMulticolumnListbox::AddSubItem(int itemIndex, int column, const char* text)
{
	guard(UIMulticolumnListbox::AddSubItem);

	assert(column >= 1 && column < NumColumns);
	int index = (itemIndex + 1) * NumColumns + column;
	Items[index] = text;
	// note: not calling Win32 API here - Windows will request text with LVN_GETDISPINFO anyway

	unguard;
}

int UIMulticolumnListbox::GetItemCount() const
{
	if (!IsTrueVirtualMode())
		return Items.Num() / NumColumns - 1;
	int NumItems = 0;
	OnGetItemCount(const_cast<UIMulticolumnListbox*>(this), NumItems); // callbacks has non-const "this"
	return NumItems;
}

const char* UIMulticolumnListbox::GetSubItem(int itemIndex, int column) const
{
	guard(UIMulticolumnListbox::AddSubItem);

	assert(column >= 0 && column < NumColumns);
	int index = (itemIndex + 1) * NumColumns + column;
	return *Items[index];

	unguard;
}

static int CompareInts(const int* i1, const int* i2)
{
	return *i1 - *i2;
}

int UIMulticolumnListbox::GetSelectionIndex(int i) const
{
	if (i >= SelectedItems.Num()) return -1;

	if (SelectedItems.Num() >= 2 && i == 0)
	{
		// sort items when called for 1st item
		// ignore "const" modifier (don't want to use "mutable" for SelectedItems or non-const for this function)
		const_cast<UIMulticolumnListbox*>(this)->SelectedItems.Sort(CompareInts);
	}
	return SelectedItems[i];
}

void UIMulticolumnListbox::RemoveItem(int itemIndex)
{
	int numItems = GetItemCount();
	if (itemIndex < 0 || itemIndex >= numItems)
		return;									// out of range
	// remove from Items array
	int stringIndex = (itemIndex + 1) * NumColumns;
	Items.RemoveAt(stringIndex, NumColumns);	// remove 1 item
	// remove from ListView
	if (Wnd)
	{
		// remove item
		if (!IsVirtualMode)
		{
			ListView_DeleteItem(Wnd, itemIndex);
		}
		else
		{
			ListView_SetItemCount(Wnd, numItems - 1);
		}
	}
	// remove from selection
	// (note: when window exists, item will be removed from selection in ListView_DeleteItem -> HandleCommand chain)
	int pos = SelectedItems.FindItem(itemIndex);
	if (pos >= 0) SelectedItems.RemoveAtSwap(pos);
	// renumber selected items
	for (int i = 0; i < SelectedItems.Num(); i++)
	{
		int n = SelectedItems[i];
		if (n >= itemIndex)
			SelectedItems[i] = n - 1;
	}
}

void UIMulticolumnListbox::RemoveAllItems()
{
	// remove items from local storage and from control
	int numStrings = Items.Num();
	if (numStrings > NumColumns)
		Items.RemoveAt(NumColumns, numStrings - NumColumns);
	if (Wnd) ListView_DeleteAllItems(Wnd);

	// process selection
	int selCount = SelectedItems.Num();
	SelectedItems.Reset();
	if (selCount)
	{
		// execute a callback about selection change
		if (selCount == 1)
		{
			if (Callback)
				Callback(this, -1);
		}
		if (SelChangedCallback)
			SelChangedCallback(this);
	}
}

#if DEBUG_MULTILIST_SEL
static void PrintSelection(const char* title, const TArray<int> &selection)
{
	appPrintf("%s: Selection[%d] =", title, selection.Num());
	for (int i = 0; i < selection.Num(); i++) appPrintf(" %d", selection[i]);
	appPrintf("\n");
}
#endif

UIMulticolumnListbox& UIMulticolumnListbox::SelectItem(int index, bool add)
{
	assert(!add || Multiselect);

	if (index == -1 || (Multiselect && !add))
	{
		UnselectAllItems();
		return *this;
	}

	// put this item to SelectedItems array
	if (!Multiselect)
	{
		int value = GetSelectionIndex();
		if (index != value)
		{
			SelectedItems.Reset();
			SelectedItems.Add(index);
			// perform selection for control
			if (Wnd) SetItemSelection(index, true);
		}
	}
	else
	{
		int pos = SelectedItems.FindItem(index);
		if (pos < 0)
		{
			SelectedItems.Add(index);
			// perform selection for control
			if (Wnd) SetItemSelection(index, true);
		}
	}
#if DEBUG_MULTILIST_SEL
	PrintSelection("SelectItem", SelectedItems);
#endif
	return *this;
}

UIMulticolumnListbox& UIMulticolumnListbox::SelectItem(const char* item, bool add)
{
	int index = 0;
	for (int i = NumColumns; i < Items.Num(); i += NumColumns, index++)
	{
		if (!strcmp(*Items[i], item))
			return SelectItem(index, add);
	}
	return *this;
}

UIMulticolumnListbox& UIMulticolumnListbox::UnselectItem(int index)
{
	if (Wnd) SetItemSelection(index, false);
	int pos = SelectedItems.FindItem(index);
	if (pos >= 0) SelectedItems.RemoveAtSwap(pos);
	return *this;
}

UIMulticolumnListbox& UIMulticolumnListbox::UnselectItem(const char* item)
{
	int index = 0;
	for (int i = NumColumns; i < Items.Num(); i += NumColumns, index++)
	{
		if (!strcmp(*Items[i], item))
			return UnselectItem(index);
	}
	return *this;
}

UIMulticolumnListbox& UIMulticolumnListbox::UnselectAllItems()
{
	if (Wnd)
	{
		for (int i = SelectedItems.Num() - 1; i >= 0; i--) // SelectedItems will be altered in HandleCommand
			SetItemSelection(SelectedItems[i], false);
	}
	SelectedItems.Reset();
	return *this;
}

void UIMulticolumnListbox::SetItemSelection(int index, bool select)
{
	ListView_SetItemState(Wnd, index, select ? LVIS_SELECTED : 0, LVIS_SELECTED);
	if (select) ListView_EnsureVisible(Wnd, index, FALSE);
}

void UIMulticolumnListbox::Create(UIBaseDialog* dialog)
{
	int i;

	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	Id = dialog->GenerateDialogId();

	DWORD style = Multiselect ? 0 : LVS_SINGLESEL;
	if (IsVirtualMode) style |= LVS_OWNERDATA;

	Wnd = Window(WC_LISTVIEW, "",
		style | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE, dialog);
	ListView_SetExtendedListViewStyle(Wnd, LVS_EX_FLATSB | LVS_EX_LABELTIP);

#if USE_EXPLORER_STYLE
	SetExplorerTheme(Wnd);
#endif

	// compute automatic column width
	int clientWidth = Width - GetSystemMetrics(SM_CXVSCROLL) - 6; // exclude scrollbar and border areas
	int totalWidth = 0;
	int numAutoWidthColumns = 0;
	int autoColumnWidth = 0;
	for (i = 0; i < NumColumns; i++)
	{
		int w = ColumnSizes[i];
		if (w == -1)
			numAutoWidthColumns++;
		else if (w < 0)
			w = int(DecodeWidth(w) * clientWidth);
		totalWidth += w;
	}
	assert(totalWidth <= Width);
	if (numAutoWidthColumns)
		autoColumnWidth = (clientWidth - totalWidth) / numAutoWidthColumns;

	// create columns
	LVCOLUMN column;
	column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	for (i = 0; i < NumColumns; i++)
	{
		column.iSubItem = i;
		column.pszText = const_cast<char*>(*Items[i]);
		int w = ColumnSizes[i];
		if (w == -1)
			column.cx = autoColumnWidth;
		else if (w < 0)
			column.cx = int(DecodeWidth(w) * clientWidth);
		else
			column.cx = w;
		switch (ColumnAlign[i])
		{
		case TA_Right:  column.fmt = LVCFMT_RIGHT; break;
		case TA_Center: column.fmt = LVCFMT_CENTER; break;
//		case TA_Left:
		default:        column.fmt = LVCFMT_LEFT; break;
		}
		ListView_InsertColumn(Wnd, i, &column);
	}

	UpdateListViewHeaderSort();

	// add items
	int numItems = GetItemCount();
	if (!IsVirtualMode)
	{
		LVITEM lvi;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.pszText = LPSTR_TEXTCALLBACK;
		lvi.iSubItem = 0;
		for (i = 0; i < numItems; i++)
		{
			lvi.iItem = i;
			lvi.lParam = i;
			ListView_InsertItem(Wnd, &lvi);
		}
	}
	else
	{
		ListView_SetItemCount(Wnd, numItems);
	}

	// set selection
	for (i = 0; i < SelectedItems.Num(); i++)
		SetItemSelection(SelectedItems[i], true);

	UpdateEnabled();
}

bool UIMulticolumnListbox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	guard(UIMulticolumnListbox::HandleCommand);

	if (cmd == LVN_GETDISPINFO)
	{
		// Note: this callback is executed only when items is visualized, so we can
		// add items in "lazy" fashion.
		NMLVDISPINFO* plvdi = (NMLVDISPINFO*)lParam;
		int itemIndex    = plvdi->item.iItem;
		int subItemIndex = plvdi->item.iSubItem;
		if (!IsTrueVirtualMode())
		{
			plvdi->item.pszText = const_cast<char*>(*Items[(itemIndex + 1) * NumColumns + subItemIndex]);
		}
		else
		{
			const char* text = "";
			OnGetItemText(this, text, itemIndex, subItemIndex);
			plvdi->item.pszText = const_cast<char*>(text);
		}
		return true;
	}

	if (cmd == LVN_ITEMCHANGED)
	{
		NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
		if (nmlv->uChanged & LVIF_STATE)
		{
			if ((nmlv->uOldState ^ nmlv->uNewState) & LVIS_SELECTED)
			{
				int item = nmlv->iItem;
				if (nmlv->uNewState & LVIS_SELECTED)
				{
					// the item could be already in selection list when we're creating control
					// and setting selection for all SelectedItems
					int pos = SelectedItems.FindItem(item);
					if (pos < 0)
						SelectedItems.Add(item);
				}
				else
				{
					if (item != -1)
					{
						int pos = SelectedItems.FindItem(item);
						assert(pos >= 0);
						SelectedItems.RemoveAtSwap(pos);
					}
					else
					{
						// iItem == -1 means all items
						SelectedItems.Empty();
					}
				}
				// callbacks
				if (GetSelectionCount() <= 1)
				{
					// sending this callback only when no multiple items selected
					int value = GetSelectionIndex();
					if (Callback)
						Callback(this, value);
				}
				// just notify about selection changes
				if (SelChangedCallback)
					SelChangedCallback(this);
#if DEBUG_MULTILIST_SEL
				PrintSelection("HandleCommand", SelectedItems);
#endif
			}
		}
		return true;
	}

	if (cmd == LVN_ODSTATECHANGED && IsVirtualMode)
	{
		// special selection mode for selecting range of items with Shift key
		NMLVODSTATECHANGE* state = (NMLVODSTATECHANGE*)lParam;
		if ((state->uOldState ^ state->uNewState) & LVIS_SELECTED)
		{
			assert(state->uNewState & LVIS_SELECTED);	// unselection is made with LVN_ITEMCHANGED, with iItem=-1
			SelectedItems.Empty(state->iTo - state->iFrom + 1);
			for (int item = state->iFrom; item <= state->iTo; item++)
				SelectedItems.Add(item);
			// notify about selection changes (multiple values changed, so no per-item notifications)
			if (SelChangedCallback)
				SelChangedCallback(this);
#if DEBUG_MULTILIST_SEL
			PrintSelection("HandleCommand", SelectedItems);
#endif
		}
		return true;
	}

	if (cmd == LVN_COLUMNCLICK)
	{
		NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
		if (nmlv->iSubItem >= 0 && nmlv->iSubItem < NumColumns && OnColumnClick)
			OnColumnClick(this, nmlv->iSubItem);
	}

	if (cmd == LVN_ODFINDITEM && IsVirtualMode)
	{
		//!! TODO: search
		return false;
	}

	if (cmd == LVN_ITEMACTIVATE)
	{
		if (DblClickCallback)
			DblClickCallback(this, GetSelectionIndex());
		return true;
	}

	if (cmd == LVN_KEYDOWN)
	{
		NMLVKEYDOWN* pnkd = (NMLVKEYDOWN*)lParam;
		if (GetKeyState(VK_CONTROL) & 0x8000)	// can check other keys here too
		{
			// handle Ctrl+A
			if (pnkd->wVKey == 'A')
			{
				int numItems = GetItemCount();
				LockUpdate();
				for (int i = 0; i < numItems; i++)
					SetItemSelection(i, true);
				UnlockUpdate();
			}
		}

		return true;
	}

	return false;

	unguard;
}


/*-----------------------------------------------------------------------------
	UITreeView
-----------------------------------------------------------------------------*/

struct TreeViewItem
{
	FString			Label;
	TreeViewItem*	Parent;
	TreeViewItem*	HashNext;
	HTREEITEM		hItem;
	bool			Checked;

	TreeViewItem()
	:	Parent(NULL)
	,	hItem(NULL)
	,	HashNext(NULL)
	,	Checked(false)
	{}
};

#define TREE_HASH_SIZE		256
#define TREE_HASH_MASK		(TREE_HASH_SIZE-1)
#define TREE_SEPARATOR_CHAR	'/'

// NOTE: this function don't have to be a part of UITreeView - it is very generic
/*static*/ int UITreeView::GetHash(const char* text)
{
	int hash = 0;
	while (char c = *text++)
	{
		hash = (hash ^ 0x25) + (c & 0xDF);	// 0xDF mask will ignore character's case
	}
	return hash & TREE_HASH_MASK;
}

UITreeView::UITreeView()
:	SelectedItem(NULL)
,	RootLabel("Root")
,	bUseFolderIcons(false)
,	bUseCheckboxes(false)
,	ItemHeight(DEFAULT_TREE_ITEM_HEIGHT)
{
	Height = DEFAULT_TREEVIEW_HEIGHT;
	HashTable = new TreeViewItem*[TREE_HASH_SIZE]; // will be initialized in RemoveAllItems()
	// create a root item
	RemoveAllItems();
}

UITreeView::~UITreeView()
{
	for (int i = 0; i < Items.Num(); i++)
	{
		delete Items[i];
	}
	delete[] HashTable;
}

TreeViewItem* UITreeView::FindItem(const char* item)
{
	if (item[0] == 0)
	{
		// this is a root node
		return Items[0];
	}
	// lookup item using hash table - it is very useful when tree has lots of items
	int hash = GetHash(item);
	for (TreeViewItem* tvitem = HashTable[hash]; tvitem; tvitem = tvitem->HashNext)
	{
		if (stricmp(*tvitem->Label, item) == 0)
		{
			return tvitem;
		}
	}
	return NULL;
}

UITreeView& UITreeView::AddItem(const char* item)
{
	char buffer[1024];
	appStrncpyz(buffer, item, ARRAY_COUNT(buffer));

	// create full hierarchy if needed
	TreeViewItem* parent = GetRoot();
	char *s, *n;
	for (s = buffer; s && s[0]; s = n)
	{
		// split path: find a separator and replace it with null
		n = strchr(s, TREE_SEPARATOR_CHAR);
		if (n) *n = 0;
		// find this item
		TreeViewItem* curr = FindItem(buffer);
		if (!curr)
		{
			curr = new TreeViewItem;
			curr->Label = buffer;		// names consists of full path, i.e. they are "a", "a/b", "a/b/c" etc
			curr->Parent = parent;
			// insert into hash table
			int hash = GetHash(buffer);
			curr->HashNext = HashTable[hash];
			HashTable[hash] = curr;
			// finish creation
			Items.Add(curr);
			CreateItem(*curr);
		}
		// restore string (return separator back) and skip separator
		if (n) *n++ = TREE_SEPARATOR_CHAR;
		parent = curr;
	}

	return *this;
}

// Note: this function will create "root" item
void UITreeView::RemoveAllItems()
{
	// Remove currently allocated items
	for (int i = 0; i < Items.Num(); i++)
	{
		delete Items[i];
	}
	Items.Empty();
	memset(HashTable, 0, TREE_HASH_SIZE * sizeof(TreeViewItem*));

	if (Wnd) TreeView_DeleteAllItems(Wnd);
	// add root item, at index 0
	TreeViewItem* root = new TreeViewItem;
	root->Label = "";
	Items.Add(root);
	SelectedItem = root;
}

void UITreeView::SetChecked(const char* item, bool checked)
{
	TreeViewItem* foundItem = FindItem(item);
	if (foundItem && (foundItem->Checked != checked))
	{
		foundItem->Checked = checked;
		if (Wnd)
		{
			TV_ITEM tvi;
			tvi.mask = TVIF_HANDLE | TVIF_STATE;
			tvi.hItem = foundItem->hItem;
			tvi.stateMask = TVIS_STATEIMAGEMASK;
			tvi.state = checked ? 2 << 12 : 1 << 12;
			TreeView_SetItem(Wnd, &tvi);
		}
	}
}

bool UITreeView::GetChecked(const char* item)
{
	TreeViewItem* foundItem = FindItem(item);

	if (!foundItem)
		return false;

	if (Wnd == NULL)
		return foundItem->Checked;

	TV_ITEM tvi;
	tvi.mask = TVIF_HANDLE | TVIF_STATE;
	tvi.hItem = foundItem->hItem;
	TreeView_GetItem(Wnd, &tvi);
	return (tvi.state >> 12) > 1;
}

UITreeView& UITreeView::SelectItem(const char* item)
{
	TreeViewItem* foundItem = FindItem(item);
	if (foundItem && foundItem != SelectedItem)
	{
		SelectedItem = foundItem;
		if (Wnd)
			TreeView_SelectItem(Wnd, SelectedItem->hItem);
	}

	return *this;
}

// This image list is created once and shared between all possible UITreeView controls.
static HIMAGELIST GTreeFolderImages;

static void LoadFolderIcons()
{
	// the code is based on Microsoft's CppWindowsCommonControls demo
	// http://code.msdn.microsoft.com/windowsapps/CppWindowsCommonControls-9ea0de64
	if (GTreeFolderImages != NULL) return;

	GTreeFolderImages = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 1, 1);

	HINSTANCE hDll = LoadLibrary("shell32.dll");
	if (!hDll) return;

	for (int i = 4; i < 6; i++)	// load 2 icons
	{
		// Because the icons are loaded from system resources (i.e. they are
		// shared), it is not necessary to free resources with 'DestroyIcon'.
		HICON hIcon = (HICON)LoadImage(hDll, MAKEINTRESOURCE(i), IMAGE_ICON, 0, 0, LR_SHARED);
		ImageList_AddIcon(GTreeFolderImages, hIcon);
	}

	FreeLibrary(hDll);
}

void UITreeView::Create(UIBaseDialog* dialog)
{
	Parent->AddVerticalSpace();
	Parent->AllocateUISpace(X, Y, Width, Height);
	Parent->AddVerticalSpace();
	Id = dialog->GenerateDialogId();

	Wnd = Window(WC_TREEVIEW, "",
		TVS_HASLINES | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE, dialog);
	if (bUseCheckboxes)
	{
		// Can't set TVS_CHECKBOXES immediately - this will not allow to change "checked" state of items after creation.
		// The known workaround is to set this style separately.
		DWORD style = GetWindowLong(Wnd, GWL_STYLE);
		SetWindowLong(Wnd, GWL_STYLE, style | TVS_CHECKBOXES);
	}

#if USE_EXPLORER_STYLE
	SetExplorerTheme(Wnd);
#endif

	if (ItemHeight > 0)
		TreeView_SetItemHeight(Wnd, ItemHeight);

	if (bUseFolderIcons)
	{
		LoadFolderIcons();
		// Attach image lists to tree view common control
		TreeView_SetImageList(Wnd, GTreeFolderImages, TVSIL_NORMAL);
	}

	// add items
	for (int i = 0; i < Items.Num(); i++)
		CreateItem(*Items[i]);
	// set selection
	TreeView_SelectItem(Wnd, SelectedItem->hItem);

	UpdateEnabled();
}

bool UITreeView::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == TVN_SELCHANGED)
	{
		LPNMTREEVIEW data = (LPNMTREEVIEW)lParam;
		HTREEITEM hItem = data->itemNew.hItem;
		for (int i = 0; i < Items.Num(); i++)
		{
			TreeViewItem* item = Items[i];
			if (item->hItem == hItem)
			{
				if (SelectedItem != item)
				{
					SelectedItem = item;
					if (Callback)
						Callback(this, *item->Label);
				}
				break;
			}
		}
		return true;
	}
	return false;
}

void UITreeView::CreateItem(TreeViewItem& item)
{
	assert(item.hItem == NULL);
	if (!Wnd) return;

	// note: due to AddItem() nature, all item parents are placed before the item itself

	TVINSERTSTRUCT tvis;
	memset(&tvis, 0, sizeof(tvis));

	const char* text;
	if (!item.Parent)
	{
		text = *RootLabel;
	}
	else
	{
		text = *item.Label;
		const char* s = strrchr(*item.Label, TREE_SEPARATOR_CHAR);
		if (s) text = s+1;
	}

	tvis.hParent = item.Parent ? item.Parent->hItem : NULL;
	tvis.item.mask = TVIF_TEXT;
	tvis.item.pszText = const_cast<char*>(text);

	if (bUseFolderIcons)
	{
		tvis.item.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
//		tvis.item.iImage = 0;
		tvis.item.iSelectedImage = 1;
	}

	if (bUseCheckboxes)
	{
		tvis.item.mask |= TVIF_STATE;
		tvis.item.stateMask = TVIS_STATEIMAGEMASK;
		tvis.item.state = item.Checked ? 2 << 12 : 1 << 12;
	}

	item.hItem = TreeView_InsertItem(Wnd, &tvis);
	// expand root item
	if (item.Parent)
	{
		const TreeViewItem* root = GetRoot();
		if (item.Parent == root || item.Parent->Parent == root)
			TreeView_Expand(Wnd, item.Parent->hItem, TVE_EXPAND);
	}
}

void UITreeView::Expand(const char* item)
{
	TreeViewItem* foundItem = FindItem(item);
	if (foundItem)
	{
		TreeView_Expand(Wnd, foundItem->hItem, TVE_EXPAND);
	}
}

void UITreeView::CollapseAll()
{
	LockUpdate();
	for (int i = 0; i < Items.Num(); i++)
	{
		TreeView_Expand(Wnd, Items[i]->hItem, TVE_COLLAPSE);
	}
	UnlockUpdate();
}

void UITreeView::ExpandCheckedNodes()
{
	CollapseAll();
	UpdateCheckedStates();

	LockUpdate();

	for (int i = 0; i < Items.Num(); i++)
	{
		TreeViewItem* item = Items[i];
		if (item->Checked)
		{
			for (TreeViewItem* p = item->Parent; p; p = p->Parent)
			{
				TreeView_Expand(Wnd, p->hItem, TVE_EXPAND);
			}
		}
	}

	UnlockUpdate();
}

void UITreeView::UpdateCheckedStates()
{
	if (bUseCheckboxes && (Wnd != NULL))
	{
		// Update checkbox states
		for (int i = 0; i < Items.Num(); i++)
		{
			TreeViewItem* item = Items[i];
			TV_ITEM tvi;
			tvi.mask = TVIF_HANDLE | TVIF_STATE;
			tvi.hItem = item->hItem;
			TreeView_GetItem(Wnd, &tvi);
			item->Checked = (tvi.state >> 12) > 1;
		}
	}
}

void UITreeView::DialogClosed(bool cancel)
{
	UpdateCheckedStates();
	// Some marker indicating that window is no longer valid
	Wnd = NULL;
}


/*-----------------------------------------------------------------------------
	UIMenu
-----------------------------------------------------------------------------*/

// Hyperlink
UIMenuItem::UIMenuItem(const char* text, const char* link)
{
	Init(MI_HyperLink, text);
	Link = link;
}

// Checkbox
UIMenuItem::UIMenuItem(const char* text, bool checked)
:	bValue(checked)
,	pValue(&bValue)
{
	Init(MI_Checkbox, text);
}

// Checkbox
UIMenuItem::UIMenuItem(const char* text, bool* checked)
:	pValue(checked)
//,	bValue(value) - uninitialized
{
	Init(MI_Checkbox, text);
}

// RadioGroup
UIMenuItem::UIMenuItem(int value)
:	iValue(value)
,	pValue(&iValue)
{
	Init(MI_RadioGroup, NULL);
}

// RadioGroup
UIMenuItem::UIMenuItem(int* value)
:	pValue(value)
//,	iValue(value) - uninitialized
{
	Init(MI_RadioGroup, NULL);
}

// RadioButton
UIMenuItem::UIMenuItem(const char* text, int value)
:	iValue(value)
{
	Init(MI_RadioButton, text);
}

// Common part of constructors
void UIMenuItem::Init(EType type, const char* label)
{
	Type = type;
	Label = label ? label : "";
	Link = NULL;
	Id = 0;
	Parent = NextChild = FirstChild = NULL;
	Enabled = true;
	Checked = false;
}

// Destructor: release all child items
UIMenuItem::~UIMenuItem()
{
	UIMenuItem* next;
	for (UIMenuItem* curr = FirstChild; curr; curr = next)
	{
		next = curr->NextChild;
		delete curr;		// may be recurse here
	}
	FirstChild = NULL;
}

// Append a new menu item to chain. This code is very similar to
// UIElement::operator+().
UIMenuItem& operator+(UIMenuItem& item, UIMenuItem& next)
{
	guard(operator+(UIMenuItem));

	UIMenuItem* e = &item;
	while (true)
	{
		UIMenuItem* n = e->NextChild;
		if (!n)
		{
			e->NextChild = &next;
			break;
		}
		e = n;
	}
	return item;

	unguard;
}

UIMenuItem& UIMenuItem::Enable(bool enable)
{
	Enabled = enable;
	if (!Id) return *this;

	HMENU hMenu = GetMenuHandle();
	if (hMenu)
		EnableMenuItem(hMenu, Id, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_DISABLED));
	return *this;
}

// Add new submenu. This code is very similar to UIGroup::Add().
void UIMenuItem::Add(UIMenuItem* item)
{
	guard(UIMenuItem::Add);

	assert(Type == MI_Submenu || Type == MI_RadioGroup);

	if (!FirstChild)
	{
		FirstChild = item;
	}
	else
	{
		// find last child
		UIMenuItem* prev = NULL;
		for (UIMenuItem* curr = FirstChild; curr; prev = curr, curr = curr->NextChild)
		{ /* empty */ }
		// add item(s)
		prev->NextChild = item;
	}

	// set parent for all items in chain
	for ( /* empty */; item; item = item->NextChild)
	{
		assert(item->Parent == NULL);
		item->Parent = this;
	}

	unguard;
}

// Recursive function for menu creation
void UIMenuItem::FillMenuItems(HMENU parentMenu, int& nextId, int& position)
{
	guard(UIMenuItem::FillMenuItems);

	assert(Type == MI_Submenu || Type == MI_RadioGroup);

	for (UIMenuItem* item = FirstChild; item; item = item->NextChild, position++)
	{
		switch (item->Type)
		{
		case MI_Text:
		case MI_HyperLink:
		case MI_Checkbox:
		case MI_RadioButton:
			{
				assert(item->Id == 0);
				item->Id = nextId++;

				MENUITEMINFO mii;
				memset(&mii, 0, sizeof(mii));

				UINT fType = MFT_STRING;
				UINT fState = 0;
				if (!item->Enabled) fState |= MFS_DISABLED;
				if (item->Type == MI_Checkbox && *(bool*)item->pValue)
				{
					// checked checkbox
					fState |= MFS_CHECKED;
				}
				else if (Type == MI_RadioGroup && item->Type == MI_RadioButton)
				{
					// radio button will work as needed only
					fType |= MFT_RADIOCHECK;
					if (*(int*)pValue == item->iValue)
						fState |= MFS_CHECKED;
				}

				mii.cbSize     = sizeof(mii);
				mii.fMask      = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
				mii.fType      = fType;
				mii.fState     = fState;
				mii.wID        = item->Id;
				mii.dwTypeData = const_cast<char*>(*item->Label);
				mii.cch        = (UINT)strlen(mii.dwTypeData);

				InsertMenuItem(parentMenu, position, TRUE, &mii);
			}
			break;

		case MI_Separator:
			AppendMenu(parentMenu, MF_SEPARATOR, 0, NULL);
			break;

		case MI_Submenu:
			{
				HMENU hSubMenu = CreatePopupMenu();
				AppendMenu(parentMenu, MF_POPUP, (UINT_PTR)hSubMenu, *item->Label);
				int submenuPosition = 0;
				item->FillMenuItems(hSubMenu, nextId, submenuPosition);
			}
			break;

		case MI_RadioGroup:
			// just add all children to current menu
			item->FillMenuItems(parentMenu, nextId, position);
			break;

		default:
			appError("Unkwnown item type: %d (label=%s)", item->Type, *item->Label);
		}
	}

	unguard;
}

bool UIMenuItem::HandleCommand(int id)
{
	guard(UIMenuItem::HandleCommand);

	HMENU hMenu = GetMenuHandle();
	if (!hMenu) return false; // should not happen - HandleCommand executed when menu is active, so it exists

	for (UIMenuItem* item = FirstChild; item; item = item->NextChild)
	{
		if (item->Id == id)
		{
			// this item was clicked
			switch (item->Type)
			{
			case MI_Text:
				if (item->Callback)
					item->Callback(item);
				break;

			case MI_HyperLink:
				ShellExecute(NULL, "open", item->Link, NULL, NULL, SW_SHOW);
				break;

			case MI_Checkbox:
				{
					// change value
					bool value = !*(bool*)item->pValue;
					*(bool*)item->pValue = value;
					// update menu
					CheckMenuItem(hMenu, item->Id, MF_BYCOMMAND | (value ? MF_CHECKED : 0));
					// callbacks
					if (item->Callback)
						item->Callback(item);
					if (item->CheckboxCallback)
						item->CheckboxCallback(item, value);
				}
				break;

			default:
				appError("Unkwnown item type: %d (label=%s)", item->Type, *item->Label);
			}
			return true;
		}
		// id is different, verify container items
		switch (item->Type)
		{
		case MI_Submenu:
			// recurse to children
			if (item->HandleCommand(id))
				return true;
			break;

		case MI_RadioGroup:
			{
				// check whether this id belongs to radio group
				// 'item' is group here
				UIMenuItem* clickedButton = NULL;
				int newValue = 0;
				for (UIMenuItem* button = item->FirstChild; button; button = button->NextChild)
					if (button->Id == id)
					{
						clickedButton = button;
						newValue = button->iValue;
						break;
					}
				if (!clickedButton) continue;	// not in this group
				// it's ours, process the button
				int oldValue = *(int*)item->pValue;
				for (UIMenuItem* button = item->FirstChild; button; button = button->NextChild)
				{
					assert(button->Type == MI_RadioButton);
					bool checked = (button == clickedButton);
					if (button->iValue == oldValue || checked)
						CheckMenuItem(hMenu, button->Id, MF_BYCOMMAND | (checked ? MF_CHECKED : 0));
				}
				// update value
				*(int*)item->pValue = newValue;
				// callbacks
				if (clickedButton->Callback)
					clickedButton->Callback(item);
				if (item->RadioCallback)
					item->RadioCallback(item, newValue);
			}
			break;
		}
	}

	// the command was not processed
	return false;

	unguard;
}

HMENU UIMenuItem::GetMenuHandle()
{
	UIMenuItem* item = this;
	while (item->Parent)
		item = item->Parent;
	return static_cast<UIMenu*>(item)->GetHandle(false);
}

void UIMenuItem::Update()
{
	guard(UIMenuItem::Update);

	HMENU hMenu = GetMenuHandle();
	if (!hMenu) return;

	switch (Type)
	{
	case MI_Checkbox:
		CheckMenuItem(hMenu, Id, MF_BYCOMMAND | (*(bool*)pValue ? MF_CHECKED : 0));
		break;
	case MI_RadioGroup:
		for (UIMenuItem* button = FirstChild; button; button = button->NextChild)
		{
			bool checked = (button->iValue == *(int*)pValue);
			CheckMenuItem(hMenu, button->Id, MF_BYCOMMAND | (checked ? MF_CHECKED : 0));
		}
		break;
	case MI_Submenu:
		for (UIMenuItem* item = FirstChild; item; item = item->NextChild)
			item->Update();
		break;
	}

	unguard;
}

UIMenu::UIMenu()
:	UIMenuItem(MI_Submenu)
,	hMenu(0)
,	ReferenceCount(0)
{}

UIMenu::~UIMenu()
{
	if (hMenu) DestroyMenu(hMenu);
}

HMENU UIMenu::GetHandle(bool popup, bool forceCreate)
{
	if (!hMenu && forceCreate) Create(popup);
	return popup ? GetSubMenu(hMenu, 0) : hMenu;
}

void UIMenu::Create(bool popup)
{
	guard(UIMenu::Create);

	assert(!hMenu);
	int nextId = FIRST_MENU_ID, position = 0;

	if (popup)
	{
		// TrackPopupMenu can't work with main menu, it requires a submenu handle.
		// Create dummy submenu to host all menu items. Note: GetMenuHandle() will
		// return submenu at position 0 when requesting a popup memu handle.
		hMenu = CreateMenu();
		HMENU hSubMenu = CreatePopupMenu();
		AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, "");
		FillMenuItems(hSubMenu, nextId, position);
	}
	else
	{
		hMenu = CreateMenu();
		FillMenuItems(hMenu, nextId, position);
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	UIGroup
-----------------------------------------------------------------------------*/

UIGroup::UIGroup(const char* label, unsigned flags)
:	Label(label)
,	FirstChild(NULL)
,	Flags(flags)
,	RadioValue(0)
,	pRadioValue(&RadioValue)
{
	IsGroup = true;
}

UIGroup::UIGroup(unsigned flags)
:	FirstChild(NULL)
,	Flags(flags)
,	RadioValue(0)
,	pRadioValue(&RadioValue)
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

	UIElement* next;
	for (UIElement* curr = FirstChild; curr; curr = next)
	{
		next = curr->NextChild;
		delete curr;
	}
	FirstChild = NULL;

	unguard;
}

void UIGroup::Add(UIElement* item)
{
	guard(UIGroup::Add);

	// if control belongs to some group, detach it
	// note: this will break NextChild chain, so we'll attach only one control there
	if (item->Parent)
		item->Parent->Remove(item);

	if (!FirstChild)
	{
		FirstChild = item;
	}
	else
	{
		// find last child
		UIElement* prev = NULL;
		for (UIElement* curr = FirstChild; curr; prev = curr, curr = curr->NextChild)
		{ /* empty */ }
		// add item(s)
		prev->NextChild = item;
	}

	// set parent for all items in chain
	for ( /* empty */; item; item = item->NextChild)
	{
		assert(item->Parent == NULL);
		item->Parent = this;
	}

	unguard;
}

void UIGroup::Remove(UIElement* item)
{
	guard(UIGroup::Remove);

	assert(item->Parent == this);
	item->Parent = NULL;

	// remove control from child chain

	if (item == FirstChild)
	{
		FirstChild = item->NextChild;
		item->NextChild = NULL;
		return;
	}

	UIElement* prev = NULL;
	for (UIElement* curr = FirstChild; curr; prev = curr, curr = curr->NextChild)
	{
		if (curr == item)
		{
			assert(prev);
			prev->NextChild = curr->NextChild;
			curr->NextChild = NULL;
			return;
		}
	}
	assert(0);

	unguard;
}

#if DEBUG_LAYOUT

static int DebugLayoutDepth = 0;

const char* GetDebugLayoutIndent()
{
#define MAX_INDENT 32
	static char indent[MAX_INDENT*2+1];
	if (!indent[0]) memset(indent, ' ', sizeof(indent)-1);
	return &indent[MAX_INDENT*2 - DebugLayoutDepth*2];
}

#endif // DEBUG_LAYOUT

void UIGroup::AllocateUISpace(int& x, int& y, int& w, int& h)
{
	guard(UIGroup::AllocateUISpace);

	int baseX = X + CursorX;
	int parentWidth = Width;
	int rightMargin = X + Width;

	if (!(Flags & GROUP_NO_BORDER))
	{
		parentWidth -= GROUP_INDENT * 2;
		rightMargin -= GROUP_INDENT;
	}

	DBG_LAYOUT("%s... AllocSpace (%d %d %d %d) IN: Curs: %d,%d W: %d -- ", GetDebugLayoutIndent(), x, y, w, h, CursorX, CursorY, parentWidth);

	if (w < 0)
	{
		if (w == -1 && (Flags & GROUP_HORIZONTAL_LAYOUT))
			w = AutoWidth;
		else
			w = int(DecodeWidth(w) * parentWidth);
	}

	if (h < 0 && Height > 0)
	{
		h = int(DecodeWidth(h) * Height);
	}
	assert(h >= 0);

	if (x == -1)
	{
		x = baseX;
		if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == GROUP_HORIZONTAL_LAYOUT)
			CursorX += w;
	}
	else if (x < 0)
		x = baseX + int(DecodeWidth(x) * parentWidth);	// left border of parent control, 'x' is relative value
	else
		x = baseX + x;									// treat 'x' as relative value

	if (x + w > rightMargin)
		w = rightMargin - x;

	if (y < 0)
	{
		y = Y + CursorY;								// next 'y' value
		if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == 0)
			CursorY += h;
	}
	else
	{
		y = Y + CursorY + y;							// treat 'y' as relative value
		// don't change 'Height'
	}

//	h = unchanged;

	DBG_LAYOUT("OUT: (%d %d %d %d) Curs: %d,%d\n", x, y, w, h, CursorX, CursorY);

	unguard;
}

void UIGroup::AddVerticalSpace(int size)
{
	if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == 0)
		CursorY += (size >= 0 ? size : VERTICAL_SPACING);
}

void UIGroup::AddHorizontalSpace(int size)
{
	if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == GROUP_HORIZONTAL_LAYOUT)
		CursorX += (size >= 0 ? size : HORIZONTAL_SPACING);
}

bool UIGroup::HandleCommand(int id, int cmd, LPARAM lParam)
{
	for (UIElement* ctl = FirstChild; ctl; ctl = ctl->NextChild)
	{
		if (ctl->IsGroup || ctl->Id == id)
		{
			// pass command to control or all groups
			if (ctl->HandleCommand(id, cmd, lParam))
				return true;
			if (ctl->Id == id)
				return true;
		}
	}
	return false;
}

void UIGroup::DialogClosed(bool cancel)
{
	for (UIElement* ctl = FirstChild; ctl; ctl = ctl->NextChild)
		ctl->DialogClosed(cancel);
}

void UIGroup::EnableAllControls(bool enabled)
{
	for (UIElement* ctl = FirstChild; ctl; ctl = ctl->NextChild)
		ctl->Enable(enabled);
}

void UIGroup::ShowAllControls(bool show)
{
	for (UIElement* ctl = FirstChild; ctl; ctl = ctl->NextChild)
		ctl->Show(show);
}

void UIGroup::Create(UIBaseDialog* dialog)
{
	CreateGroupControls(dialog);
	// Disable all children controls if this UIGroup is disabled
	if (!Enabled)
	{
		EnableAllControls(Enabled);
	}
}

void UIGroup::UpdateEnabled()
{
	Super::UpdateEnabled();
	EnableAllControls(Enabled);
}

void UIGroup::UpdateVisible()
{
	Super::UpdateVisible();
	ShowAllControls(Visible);
}

void UIGroup::CreateGroupControls(UIBaseDialog* dialog)
{
	guard(UIGroup::CreateGroupControls);

	// save original positions for second AllocateUISpace call
	int origX = X, origY = Y, origW = Width, origH = Height;

	if (!(Flags & GROUP_NO_BORDER))
	{
		CursorX = GROUP_INDENT;
		CursorY = GROUP_MARGIN_TOP;
	}
	else
	{
		CursorX = CursorY = 0;
	}
	if (Parent)
	{
//		if (!(Flags & GROUP_NO_BORDER)) -- makes control layout looking awful
		Parent->AddVerticalSpace();
		// request x, y and width; height is not available yet
		int h = 0;
		Parent->AllocateUISpace(X, Y, Width, h);
	}
	else
	{
		// this is a dialog, add some space on top for better layout
		CursorY += VERTICAL_SPACING;
	}
#if DEBUG_LAYOUT
	DBG_LAYOUT("%sgroup \"%s\" cursor: %d %d\n", GetDebugLayoutIndent(), *Label, CursorX, CursorY);
	DebugLayoutDepth++;
#endif

	// allow UIGroup-based classes to add own controls
	AddCustomControls();

	// some controls could compite size depending on text
	for (UIElement* control = FirstChild; control; control = control->NextChild)
		control->UpdateSize(dialog);

	// determine default width of control in horizontal layout; this value will be used for
	// all controls which width was not specified (for Width==-1)
	AutoWidth = 0;
	int horizontalSpacing = 0;
	if (Flags & GROUP_HORIZONTAL_LAYOUT)
	{
		int totalWidth = 0;					// total width of controls with specified width
		int numAutoWidthControls = 0;		// number of controls with width set to -1
		int numControls = 0;
		int parentWidth = Width;			// width of space for children controls
		if (!(Flags & GROUP_NO_BORDER))
			parentWidth -= GROUP_INDENT * 2;

		for (UIElement* control = FirstChild; control; control = control->NextChild)
		{
			numControls++;
			// get width of control
			int w = control->Width;
			if (w == -1)
			{
				numAutoWidthControls++;
			}
			else if (w < 0)
			{
				w = int(DecodeWidth(w) * parentWidth);
				totalWidth += w;
			}
			else
			{
				totalWidth += w;
			}
		}
		if (totalWidth > parentWidth)
			appNotify("Group(%s) is not wide enough to store children controls: %d < %d", *Label, parentWidth, totalWidth);
		if (numAutoWidthControls)
			AutoWidth = (parentWidth - totalWidth) / numAutoWidthControls;
		if (Flags & GROUP_HORIZONTAL_SPACING)
		{
			if (numAutoWidthControls)
			{
				appNotify("Group(%s) has GROUP_HORIZONTAL_SPACING and auto-width controls", *Label);
			}
			else
			{
				if (numControls > 1)
					horizontalSpacing = (parentWidth - totalWidth) / (numControls - 1);
			}
		}
	}

	// call 'Create' for all children
	int maxControlY = Y + Height;
	bool isRadioGroup = false;
	int controlIndex = 0;
	for (UIElement* control = FirstChild; control; control = control->NextChild, controlIndex++)
	{
		// evenly space controls for horizontal layout, when requested
		if (horizontalSpacing > 0 && control != FirstChild)
			AddHorizontalSpace(horizontalSpacing);
		DBG_LAYOUT("%screate %s: x=%d y=%d w=%d h=%d\n", GetDebugLayoutIndent(),
			control->ClassName(), control->X, control->Y, control->Width, control->Height);

		guard(ControlCreate);
		control->Create(dialog);
		unguardf("index=%d,class=%s", controlIndex, control->ClassName());

		int bottom = control->Y + control->Height;
		if (bottom > maxControlY)
			maxControlY = bottom;
		if (control->IsRadioButton) isRadioGroup = true;
	}
	if (isRadioGroup) InitializeRadioGroup();

	Height = max(Height, maxControlY - Y);

	if (!(Flags & GROUP_NO_BORDER))
		Height += GROUP_MARGIN_BOTTOM;

	if (Parent && !(Parent->Flags & GROUP_HORIZONTAL_LAYOUT))
	{
		// for vertical layout we should call AllocateUISpace again to adjust parent's CursorY
		// (because height wasn't known when we called AllocateUISpace first time)
		origH = Height;
		Parent->AllocateUISpace(origX, origY, origW, origH);
	}
#if DEBUG_LAYOUT
	DebugLayoutDepth--;
#endif

	if (!(Flags & GROUP_NO_BORDER))
	{
		// create a group window (border)
		Wnd = Window(WC_BUTTON, *Label, BS_GROUPBOX | WS_GROUP, 0, dialog);
	}

	if (Parent)
		Parent->AddVerticalSpace();

	unguardf("%s", *Label);
}

void UIGroup::InitializeRadioGroup()
{
	guard(UIGroup::InitializeRadioGroup);

	int radioIndex = 0;
	int numAssignedButtons = 0;	//!! used for assert/validation only
	bool allZeros = true;
	for (UIElement* control = FirstChild; control; control = control->NextChild)
	{
		if (!control->IsRadioButton) continue;
		UIRadioButton* radio = (UIRadioButton*)control;

		if (radio->Value) allZeros = false;
		if (!radio->Value && allZeros)
		{
			// This button has no value assigned - auto-assign index here.
			// Note: if all radio values are zeros, we'll assign automatic index. If
			// just 1st value is zero, it will be reassigned to zero again, so value
			// will not be changed
			radio->Value = radioIndex;
			numAssignedButtons++;
		}
		radioIndex++;
		// find and mark active button
		if (*pRadioValue == radio->Value)
			radio->SelectButton();
	}
	// sanity check: we should either have all items to be zeros (will have auto-index now),
	// or only one zero item; there's no check performed for other value duplicates
	assert(numAssignedButtons <= 1 || numAssignedButtons == radioIndex);

	//!! check selected item

	unguard;
}

void UIGroup::RadioButtonClicked(UIRadioButton* sender)
{
	*pRadioValue = sender->Value;
	if (RadioCallback)
		RadioCallback(this, *pRadioValue);
	//!! notify previously selected UIRadioButton that it's unchecked
}


/*-----------------------------------------------------------------------------
	UICheckboxGroup
-----------------------------------------------------------------------------*/

UICheckboxGroup::UICheckboxGroup(const char* label, bool value, unsigned flags)
:	UIGroup(flags)
,	Label(label)
,	bValue(value)
,	pValue(&bValue)		// points to local variable
,	CheckboxWnd(0)
{}

UICheckboxGroup::UICheckboxGroup(const char* label, bool* value, unsigned flags)
:	UIGroup(flags)
,	Label(label)
//,	bValue(value) - uninitialized, unused
,	pValue(value)
,	CheckboxWnd(0)
{}

void UICheckboxGroup::Create(UIBaseDialog* dialog)
{
	UIGroup::Create(dialog);

	Id = dialog->GenerateDialogId();
	DlgWnd = dialog->GetWnd();

	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth);

	int checkboxOffset = (Flags & GROUP_NO_BORDER) ? 0 : GROUP_INDENT;

	CheckboxWnd = Window(WC_BUTTON, *Label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, dialog,
		Id, X + checkboxOffset, Y, min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Width - checkboxOffset), DEFAULT_CHECKBOX_HEIGHT);

	CheckDlgButton(DlgWnd, Id, *pValue ? BST_CHECKED : BST_UNCHECKED);
	EnableAllControls(*pValue);
}

bool UICheckboxGroup::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (id == Id)
	{
		// checkbox
		if (cmd != BN_CLICKED) return false;

		bool checked = (IsDlgButtonChecked(DlgWnd, Id) != BST_UNCHECKED);
		if (*pValue != checked)
		{
			*pValue = checked;
			EnableAllControls(*pValue);
			if (Callback)
				Callback(this, checked);
			return true;
		}
	}
	return Super::HandleCommand(id, cmd, lParam);
}


/*-----------------------------------------------------------------------------
	UIPageControl
-----------------------------------------------------------------------------*/

UIPageControl::UIPageControl()
:	UIGroup(GROUP_CUSTOM_LAYOUT)
,	ActivePage(0)
{}

UIPageControl& UIPageControl::SetActivePage(int index)
{
	if (index == ActivePage) return *this;
	ActivePage = index;

	int pageIndex = 0;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
		page->Show(pageIndex == index);
	return *this;
}

UIPageControl& UIPageControl::SetActivePage(UIElement* child)
{
	int pageIndex = 0;
	ActivePage = -1;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
	{
		bool show = (page == child);
		if (show) ActivePage = pageIndex;
		page->Show(show);
	}
	return *this;
}

void UIPageControl::Create(UIBaseDialog* dialog)
{
	guard(UIPageControl::Create);

	Parent->AllocateUISpace(X, Y, Width, Height);

	int pageIndex = 0;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
	{
		CursorX = CursorY = 0;
		page->Show(pageIndex == ActivePage);

		guard(PageCreate);
		page->Create(dialog);
		unguardf("index=%d,class=%s", pageIndex, page->ClassName());
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	UIBaseDialog
-----------------------------------------------------------------------------*/

UIBaseDialog::UIBaseDialog()
:	UIGroup(GROUP_NO_BORDER)
,	NextDialogId(FIRST_DIALOG_ID)
,	ShouldCloseOnEsc(false)
,	ParentDialog(NULL)
,	IconResId(0)
,	IsDialogConstructed(false)
{}

UIBaseDialog::~UIBaseDialog()
{
	CloseDialog(false);
}

static HWND GMainWindow = 0;
static UIBaseDialog* GCurrentDialog = NULL;
static int GGlobalIconResId = 0;

void UIBaseDialog::SetMainWindow(HWND window)
{
	GMainWindow = window;
}

void UIBaseDialog::SetGlobalIconResId(int iconResId)
{
	GGlobalIconResId = iconResId;
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
	dlg1->style = WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT | DS_MODALFRAME;
	dlg1->cx = width;
	dlg1->cy = height;

	int titleLen = (int)wcslen(title);
	assert(titleLen < MAX_TITLE_LEN);
	wcscpy(dlg1->title, title);

	DLGTEMPLATEEX2* dlg2 = (DLGTEMPLATEEX2*)(buffer + sizeof(DLGTEMPLATEEX1) + titleLen * sizeof(wchar_t));
	dlg2->pointsize = fontSize;
	wcscpy(dlg2->typeface, fontName);

	return (DLGTEMPLATE*)buffer;
}

bool UIBaseDialog::ShowDialog(bool modal, const char* title, int width, int height)
{
	guard(UIBaseDialog::ShowDialog);

	assert(Wnd == 0);

	if (!hInstance)
	{
		hInstance = GetModuleHandle(NULL);
		InitCommonControls();
	}

	NextDialogId = FIRST_DIALOG_ID;

	// convert title to unicode
	wchar_t wTitle[MAX_TITLE_LEN];
	mbstowcs(wTitle, title, MAX_TITLE_LEN);
	DLGTEMPLATE* tmpl = MakeDialogTemplate(width, height, wTitle, L"MS Shell Dlg", 8);

	HWND ParentWindow = GMainWindow;
	if (GCurrentDialog) ParentWindow = GCurrentDialog->GetWnd();

	if (modal)
	{
		// modal
		ParentDialog = GCurrentDialog;
		GCurrentDialog = this;
#if DO_GUARD
		TRY {
#endif
			INT_PTR result = DialogBoxIndirectParam(
				hInstance,				// hInstance
				tmpl,					// lpTemplate
				ParentWindow,			// hWndParent
				StaticWndProc,			// lpDialogFunc
				(LPARAM)this			// lParamInit
			);
			GCurrentDialog = ParentDialog;
			ParentDialog = NULL;
			return (result != IDCANCEL);
#if DO_GUARD
		} CATCH_CRASH {
			GCurrentDialog = ParentDialog;
			ParentDialog = NULL;
			THROW;
		}
#endif // DO_GUARD
	}
	else
	{
		// modeless
		HWND dialog = CreateDialogIndirectParam(
			hInstance,					// hInstance
			tmpl,						// lpTemplate
			ParentWindow,				// hWndParent
			StaticWndProc,				// lpDialogFunc
			(LPARAM)this				// lParamInit
		);

		assert(dialog);
		// process all messages to allow window to appear on screen
		PumpMessageLoop();
		return true;
	}

	unguardf("modal=%d, title=\"%s\"", modal, title);
}

bool UIBaseDialog::PumpMessageLoop()
{
	guard(UIBaseDialog::PumpMessageLoop);

	if (Wnd == 0) return false;

	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_KEYDOWN && ShouldCloseOnEsc && msg.wParam == VK_ESCAPE)
		{
			// Win32 dialog boxes doesn't receive keyboard messages. By the way, modal boxes receives IDOK
			// or IDCANCEL commands when user press 'Enter' or 'Escape'. In order to handle the 'Escape' key
			// in NON-modal boxes, we should have our own message loop. We have one for non-modal dialog
			// boxes here. We don't have access to the message loop of modal dialog box. Note: we are comparing
			// msg.hwnd with dialog's window, and also comparing msg.hwnd's parent with dialog too. No other
			// checks are performed because we have very simple hierarchy in our UI system: all children are
			// parented by single dialog window.
			// If we'll need nore robust way of processing messages (for example, when we need to process
			// keys for modal dialogs, or when message loop is processed by code which is not accessible for
			// modification) - we'll need to use SetWindowsHook. Another way is to subclass all controls
			// (because key messages are sent to the focused window only, and not to its parent), but it looks
			// more complicated.
			if (msg.hwnd == Wnd || GetParent(msg.hwnd) == Wnd)
				CloseDialog(true);
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (Wnd != 0);

	unguard;
}

void UIBaseDialog::CloseDialog(bool cancel)
{
	if (Wnd && CanCloseDialog(cancel))
	{
		DialogClosed(cancel);
		EndDialog(Wnd, cancel ? IDCANCEL : IDOK);
		Wnd = 0;
	}
}

static void (*GUIExceptionHandler)() = NULL;

void UISetExceptionHandler(void (*Handler)())
{
	GUIExceptionHandler = Handler;
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

#if !DO_GUARD
	return dlg->WndProc(hWnd, msg, wParam, lParam);
#else
	// windows will not allow us to pass SEH through the message handler, so
	// add a SEH guards here
	TRY {
		return dlg->WndProc(hWnd, msg, wParam, lParam);
	} CATCH_CRASH {
#if MAX_DEBUG
		// sometimes when working with debugger, exception inside UI could not be passed outside,
		// and program crashed bypassing out exception handler - always show error information in
		// MAX_DEBUG mode
		if (GErrorHistory[0])
		{
			appNotify("ERROR in WindowProc: %s\n", GErrorHistory);
		}
		else
		{
			appNotify("Unknown error in WindowProc\n");
		}
#endif // MAX_DEBUG
//		dlg->CloseDialog(true);
		if (GUIExceptionHandler)
			GUIExceptionHandler();
		THROW;
	}
#endif
}

INT_PTR UIBaseDialog::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	guard(UIBaseDialog::WndProc);

	// handle dialog initialization
	if (msg == WM_INITDIALOG)
	{
		Wnd = hWnd;

		// attach dialog's icon
		int icon = IconResId;
		if (!icon) icon = GGlobalIconResId;
		if (icon)
			SendMessage(hWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(icon)));

		// compute client area width
		RECT r;
		GetClientRect(Wnd, &r);
		int clientWidth = r.right - r.left;

		// prepare layout variabled
		X = DEFAULT_HORZ_BORDER;
		Y = 0;
		Width = clientWidth - DEFAULT_HORZ_BORDER * 2;
		Height = 0;

		// release old controls, if dialog is opened 2nd time
		ReleaseControls();
		// create controls
		IsDialogConstructed = false;
		InitUI();
		IsDialogConstructed = true;
		CreateGroupControls(this);

		if (Menu)
			::SetMenu(Wnd, Menu->GetHandle(false, true));

		// adjust window size taking into account desired client size and center window on screen
		r.left   = 0;
		r.top    = 0;
		r.right  = clientWidth;
		r.bottom = Height + VERTICAL_SPACING;

		int newX = (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2;
		int newY = (GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2;

		AdjustWindowRect(&r, GetWindowLong(Wnd, GWL_STYLE), (Menu != NULL) ? TRUE : FALSE);
		SetWindowPos(hWnd, NULL, newX, newY, r.right - r.left, r.bottom - r.top, 0);

		return TRUE;
	}

	if (msg == WM_CLOSE)
	{
		CloseDialog(true);
		return TRUE;
	}

	if (msg == WM_DESTROY)
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

		if (id >= FIRST_MENU_ID && Menu)
		{
			if (Menu->HandleCommand(id))
				return TRUE;
		}
	}

	if (msg == WM_INITMENU && Menu != NULL)
	{
		//!! process WM_INITMENUPOPUP for popup menus
		Menu->Update();
		return TRUE;
	}

	// handle WM_NOTIFY in a similar way
	if (msg == WM_NOTIFY)
	{
		id  = LOWORD(wParam);
		cmd = ((LPNMHDR)lParam)->code;
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
