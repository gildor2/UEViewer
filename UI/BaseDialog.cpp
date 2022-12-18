// Simple UI library.
// Copyright (C) 2022 Konstantin Nosov
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

/* Windows notes:
 - When you attach the version 6 manifest, the call to InitcommonControlsEx becomes unnecessary.
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


//#define DEBUG_WINDOWS_ERRORS		MAX_DEBUG
//#define DEBUG_MULTILIST_SEL			1

#include "UIPrivate.h"

#if _WIN32

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")	// for ShellExecute

static HINSTANCE hInstance;

#endif // _WIN32


/*-----------------------------------------------------------------------------
	uxtheme.dll stuff
-----------------------------------------------------------------------------*/

static HRESULT (WINAPI * SetWindowTheme)(HWND, LPCWSTR, LPCWSTR) = NULL;
//static HRESULT (WINAPI * EnableThemeDialogTexture)(HWND, DWORD) = NULL;
static BOOL    (WINAPI * IsAppThemed)() = NULL;			// pre-Win8: check if styles are disabled
static BOOL    (WINAPI * IsThemeActive)() = NULL;
static HRESULT (WINAPI * GetThemeColor)(HANDLE hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor) = NULL;
static HANDLE  (WINAPI * OpenThemeData)(HWND hwnd, LPCWSTR pszClassList) = NULL;
static void    (WINAPI * CloseThemeData)(HANDLE hTheme) = NULL;

static void InitUXTheme()
{
	static bool loaded = false;
	if (!loaded)
	{
		loaded = true;
		HMODULE hDll = LoadLibrary("uxtheme.dll");
		if (hDll == NULL) return;
	#define GET(func)			\
		{ void* fn = GetProcAddress(hDll, #func); *(void**)&func = fn; }
		GET(SetWindowTheme)
//		GET(EnableThemeDialogTexture)
		GET(IsAppThemed)
		GET(IsThemeActive)
		GET(GetThemeColor)
		GET(OpenThemeData)
		GET(CloseThemeData)
	#undef GET
	}
}


/*-----------------------------------------------------------------------------
	UICreateContext
-----------------------------------------------------------------------------*/

UICreateContext::UICreateContext(UIBaseDialog* pDialog)
:	dialog(pDialog)
,	owner(pDialog)
{
	// Retrieve dialog's font
	hDialogFont = (HANDLE)SendMessage(dialog->GetWnd(), WM_GETFONT, 0, 0);
}

HWND UICreateContext::MakeWindow(UIElement* control, const char* className, const char* text, DWORD style, DWORD exStyle, const UIRect* customRect)
{
	const UIRect* rect = customRect ? customRect : &control->Rect;

	int x = rect->X;
	int y = rect->Y;
	int w = rect->Width;
	int h = rect->Height;

	if (control->IsVisible())
	{
		style |= WS_VISIBLE;
	}

	HWND parentWnd = owner->GetWnd();
	assert(parentWnd);
	HWND wnd = CreateWindowEx(exStyle, className, text, style | WS_CHILDWINDOW, x, y, w, h,
		parentWnd, (HMENU)(size_t)control->Id, hInstance, NULL);		// convert int -> size_t -> HANDLE to avoid warnings on 64-bit platform
#if DEBUG_WINDOWS_ERRORS
	if (!wnd) appNotify("CreateWindow failed, GetLastError returned %d\n", GetLastError());
#endif
	SendMessage(wnd, WM_SETFONT, (WPARAM)hDialogFont, MAKELPARAM(TRUE, 0));

	return wnd;
}

HWND UICreateContext::MakeWindow(UIElement* control, const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle, const UIRect* customRect)
{
	const UIRect* rect = customRect ? customRect : &control->Rect;

	int x = rect->X;
	int y = rect->Y;
	int w = rect->Width;
	int h = rect->Height;

	if (control->IsVisible())
	{
		style |= WS_VISIBLE;
	}

	HWND parentWnd = owner->GetWnd();
	assert(parentWnd);
	HWND wnd = CreateWindowExW(exStyle, className, text, style | WS_CHILDWINDOW, x, y, w, h,
		parentWnd, (HMENU)(size_t)control->Id, hInstance, NULL);
#if DEBUG_WINDOWS_ERRORS
	if (!wnd) appNotify("CreateWindow failed, GetLastError returned %d\n", GetLastError());
#endif
	SendMessage(wnd, WM_SETFONT, (WPARAM)hDialogFont, MAKELPARAM(TRUE, 0));

	return wnd;
}


/*-----------------------------------------------------------------------------
	UIElement
-----------------------------------------------------------------------------*/

UIElement::UIElement()
:	Layout(-1, -1, -1, -1)
,	Rect(-1, -1, -1, -1)
,	MinWidth(0)
,	MinHeight(0)
,	TopMargin(0)
,	BottomMargin(0)
,	LeftMargin(0)
,	RightMargin(0)
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
	if (x != -1)      Layout.X = x;
	if (y != -1)      Layout.Y = y;
	if (width != -1)  Layout.Width = width;
	if (height != -1) Layout.Height = height;
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
		//!! NOTE: using Parent->Rect.Width, because Parent->Layout.Width may be set to non-absolute
		//!! value. So it is assumed that this code will be run after UpdateLayout called for parent
			w = int(DecodeWidth(w) * Parent->Rect.Width); //!! see parentWidth in AllocateUISpace()
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

void UIElement::UpdateLayout()
{
	if (Wnd)
	{
		MoveWindow(Wnd, Rect.X, Rect.Y, Rect.Width, Rect.Height, FALSE);
	}
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

void UIElement::EnableSubclass()
{
	guard(UIElement::EnableSubclass);
	assert(Wnd);
	SetWindowSubclass(Wnd, &UIElement::StaticSubclassProc, 0, (DWORD_PTR)this);
	unguard;
}

/*static*/ LONG_PTR CALLBACK UIElement::StaticSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, ULONG_PTR dwRefData)
{
	guard(UIElement::StaticSubclassProc);
	UIElement* ctl = (UIElement*)dwRefData;
	if (ctl)
	{
		return ctl->SubclassProc(hWnd, uMsg, wParam, lParam);
	}
	else
	{
		 return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
	unguard;
}

LONG_PTR UIElement::SubclassProc(void* hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefSubclassProc((HWND)hWnd, uMsg, wParam, lParam);
}


/*-----------------------------------------------------------------------------
	UISpacer
-----------------------------------------------------------------------------*/

UISpacer::UISpacer(int size)
{
	Layout.Width = (size != 0) ? size : HORIZONTAL_SPACING;	// use size==-1 for automatic width (for horizontal layout)
	Layout.Height = (size > 0) ? size : VERTICAL_SPACING;
}


/*-----------------------------------------------------------------------------
	UIHorizontalLine
-----------------------------------------------------------------------------*/

UIHorizontalLine::UIHorizontalLine()
{
	Layout.Width = -1;
	Layout.Height = 2;
}

void UIHorizontalLine::Create(UICreateContext& ctx)
{
	Wnd = ctx.MakeWindow(this, WC_STATIC, "", SS_ETCHEDHORZ, 0);
}


/*-----------------------------------------------------------------------------
	UIVerticalLine
-----------------------------------------------------------------------------*/

UIVerticalLine::UIVerticalLine()
{
	Layout.Width = 2;
	Layout.Height = -1;
	//!! would be nice to get "Height = -1" working; probably would require 2-pass processing of UIGroup
	//!! layout: 1st pass would set Height, for example, to 1, plus remember control which requires height
	//!! change; 2nd pass will resize all remembered controls with the height of group
}

void UIVerticalLine::Create(UICreateContext& ctx)
{
	Wnd = ctx.MakeWindow(this, WC_STATIC, "", SS_ETCHEDVERT, 0);
}


/*-----------------------------------------------------------------------------
	UIBitmap
-----------------------------------------------------------------------------*/

// Check SS_REALSIZEIMAGE, SS_REALSIZECONTROL - may be useful.

UIBitmap::UIBitmap()
:	hImage(0)
{
	Layout.Width = Layout.Height = 0;		// use dimensions from resource
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
	hImage = LoadImage(inst, rid, type, Layout.Width, Layout.Height, fuLoad);
}

UIBitmap& UIBitmap::SetResourceIcon(int resId)
{
	IsIcon = true;
	LoadResourceImage(resId, IMAGE_ICON, LR_SHARED);
#if DEBUG_WINDOWS_ERRORS
	if (!hImage)
		appPrintf("UIBitmap::SetResourceIcon: %d\n", GetLastError());
#endif
	// Note: can't get icon dimensions using GetObject() - this function would fail.
	if ((!Layout.Width || !Layout.Height) && hImage)
	{
		Layout.Width = GetSystemMetrics(SM_CXICON);
		Layout.Height = GetSystemMetrics(SM_CYICON);
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
	if ((!Layout.Width || !Layout.Height) && hImage)
	{
		BITMAP bm;
		memset(&bm, 0, sizeof(bm));
		GetObject(hImage, sizeof(bm), &bm);
		Layout.Width = bm.bmWidth;
		Layout.Height = bm.bmHeight;
	}
	return *this;
}

void UIBitmap::Create(UICreateContext& ctx)
{
	Wnd = ctx.MakeWindow(this, WC_STATIC, "", IsIcon ? SS_ICON : SS_BITMAP, 0);
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
	Layout.Height = DEFAULT_LABEL_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = DEFAULT_LABEL_HEIGHT;
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
	int labelWidth;
	MeasureTextSize(*Label, &labelWidth, NULL, dialog->GetWnd());

	if (AutoSize)
	{
		Layout.Width = labelWidth;
	}
	else if (MinWidth == 0)
	{
		MinWidth = labelWidth;
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

void UILabel::Create(UICreateContext& ctx)
{
	Wnd = ctx.MakeWindow(this, WC_STATIC, *Label, ConvertTextAlign(Align), 0);
	UpdateEnabled();
}


/*-----------------------------------------------------------------------------
	UIHyperLink
-----------------------------------------------------------------------------*/

UIHyperLink::UIHyperLink(const char* text, const char* link /*, ETextAlign align*/)
:	UILabel(text /*, align*/)
,	Link(link ? link : "")
{}

void UIHyperLink::Create(UICreateContext& ctx)
{
	Id = ctx.dialog->GenerateDialogId();

#if 0
	// works without this code
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_LINK_CLASS;
	InitCommonControlsEx(&iccex);
#endif

#if 0
	// Unicode version (this control, as mentioned in SysLink documentation, in Unicode-only)
	wchar_t buffer[MAX_TITLE_LEN];
	appSprintf(ARRAY_ARG(buffer), L"<a href=\"%S\">%S</a>", *Link, *Label);
	Wnd = ctx.MakeWindow(this, WC_LINK, buffer, ConvertTextAlign(Align), 0);
#else
	// ANSI version works too
	char buffer[MAX_TITLE_LEN];
	appSprintf(ARRAY_ARG(buffer), "<a href=\"%s\">%s</a>", *Link, *Label);
	Wnd = ctx.MakeWindow(this, "SysLink", buffer, ConvertTextAlign(Align), 0);
#endif
	if (!Wnd)
	{
		// Fallback to ordinary label if SysLink was not created for some reason.
		// Could also change text color:
		// http://stackoverflow.com/questions/1525669/set-static-text-color-win32
		Wnd = ctx.MakeWindow(this, WC_STATIC, *Label, ConvertTextAlign(Align) | SS_NOTIFY, 0);
	}

	UpdateEnabled();
}

bool UIHyperLink::HandleCommand(int id, int cmd, LPARAM lParam, int& result)
{
	// Note: disabled control will display blue hyper link anyway. To override that, should use extra code
	// https://social.msdn.microsoft.com/Forums/vstudio/en-US/bd54bd30-e21f-4dc7-a77f-88de02c63f72/changing-link-label-color-for-syslink?forum=vcgeneral

	if (cmd == NM_CLICK || cmd == NM_RETURN || cmd == STN_CLICKED) // STN_CLICKED for WC_STATIC fallback
	{
		if (Callback)
		{
			Callback(this);
		}
		else if (!Link.IsEmpty())
		{
			ShellExecute(NULL, "open", *Link, NULL, NULL, SW_SHOW);
		}
		result = 1;
	}
	else if (cmd == NM_CUSTOMDRAW)
	{
		// SysLink control doesn't change text color for disabled control. Do the change ourselves with use of NM_CUSTOMDRAW.
		// References:
		// - https://docs.microsoft.com/en-us/windows/win32/controls/using-custom-draw
		// - SWT UI library (Java), Link.java, wmNotifyChild() method.
		LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
		if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT)
		{
			if (!Enabled)
				result = CDRF_NOTIFYITEMDRAW;
		}
		else if (lplvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
		{
			if (!Enabled)
				SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_GRAYTEXT));
		}
	}
	return true;
}


/*-----------------------------------------------------------------------------
	UIProgressBar
-----------------------------------------------------------------------------*/

UIProgressBar::UIProgressBar()
:	Value(0)
{
	Layout.Height = DEFAULT_PROGRESS_BAR_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = DEFAULT_PROGRESS_BAR_HEIGHT;
	TopMargin = DEFAULT_MARGIN; //? review: may be remove
}

void UIProgressBar::SetValue(float value)
{
	if (value == Value) return;
	Value = value;
	if (Wnd) SendMessage(Wnd, PBM_SETPOS, (int)(Value * 16384), 0);
}

void UIProgressBar::Create(UICreateContext& ctx)
{
	Wnd = ctx.MakeWindow(this, PROGRESS_CLASS, "", 0, 0);
	SendMessage(Wnd, PBM_SETRANGE, 0, MAKELPARAM(0, 16384));
	if (Wnd) SendMessage(Wnd, PBM_SETPOS, (int)(Value * 16384), 0);
}


/*-----------------------------------------------------------------------------
	UIButton
-----------------------------------------------------------------------------*/

UIButton::UIButton(const char* text)
:	Label(text)
{
	Layout.Height = DEFAULT_BUTTON_HEIGHT;
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
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

void UIButton::UpdateSize(UIBaseDialog* dialog)
{
	if (MinWidth == 0)
	{
		int labelWidth;
		MeasureTextSize(*Label, &labelWidth, NULL, dialog->GetWnd());
		MinWidth = labelWidth + DEFAULT_BUTTON_HEIGHT - DEFAULT_LABEL_HEIGHT;
	}

	if (MinHeight == 0)
	{
		MinHeight = DEFAULT_BUTTON_HEIGHT;
	}
}

void UIButton::Create(UICreateContext& ctx)
{
	if (Id == 0 || Id >= FIRST_DIALOG_ID)
		Id = ctx.dialog->GenerateDialogId();		// do not override Id which was set outside of Create()

	//!! BS_DEFPUSHBUTTON - for default key
	Wnd = ctx.MakeWindow(this, WC_BUTTON, *Label, WS_TABSTOP, 0);
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
	Layout.Height = DEFAULT_BUTTON_HEIGHT;
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

void UIMenuButton::UpdateSize(UIBaseDialog* dialog)
{
	if (MinWidth == 0)
	{
		int labelWidth;
		MeasureTextSize(*Label, &labelWidth, NULL, dialog->GetWnd());
		MinWidth = labelWidth + DEFAULT_BUTTON_HEIGHT - DEFAULT_LABEL_HEIGHT + DEFAULT_CHECKBOX_HEIGHT;
	}

	if (MinHeight == 0)
	{
		MinHeight = DEFAULT_BUTTON_HEIGHT;
	}
}

void UIMenuButton::Create(UICreateContext& ctx)
{
	if (Id == 0 || Id >= FIRST_DIALOG_ID)
		Id = ctx.dialog->GenerateDialogId();		// do not override Id which was set outside of Create()

	// should check Common Controls library version - if it's too low, control's window
	// will be created anyway, but will look completely wrong
	DWORD flags = WS_TABSTOP;
	if (GetComctl32Version() >= 0x600) flags |= BS_SPLITBUTTON; // not supported in comctl32.dll prior version 6.00
	Wnd = ctx.MakeWindow(this, WC_BUTTON, *Label, flags, 0);
	UpdateEnabled();
}

bool UIMenuButton::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == BCN_DROPDOWN || (cmd == BN_CLICKED && !Callback))
	{
		// reference: MFC, CSplitButton::OnDropDown()
		// get rect of button for menu positioning
		RECT rectButton;
		GetWindowRect(Wnd, &rectButton);
		Menu->Popup(this, rectButton.left, rectButton.bottom);
	}
	else if (cmd == BN_CLICKED && Callback)
	{
		Callback(this);
	}
	return true;
}


/*-----------------------------------------------------------------------------
	UICheckbox
-----------------------------------------------------------------------------*/

UICheckbox::UICheckbox(const char* text, bool value, bool autoSize)
:	Label(text)
,	bInvertValue(false)
,	bValue(value)
,	pValue(&bValue)		// points to local variable
,	AutoSize(autoSize)
{
	Layout.Height = DEFAULT_CHECKBOX_HEIGHT;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

UICheckbox::UICheckbox(const char* text, bool* value, bool autoSize)
:	Label(text)
,	bInvertValue(false)
//,	bValue(value) - uninitialized, unused
,	pValue(value)
,	AutoSize(autoSize)
{
	Layout.Height = DEFAULT_CHECKBOX_HEIGHT;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

void UICheckbox::UpdateSize(UIBaseDialog* dialog)
{
	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth, NULL, dialog->GetWnd());
	checkboxWidth += DEFAULT_CHECKBOX_HEIGHT;

	if (AutoSize)
	{
		Layout.Width = checkboxWidth;
	}
	else if (MinWidth == 0)
	{
		MinWidth = checkboxWidth;
	}

	if (MinHeight == 0)
	{
		MinHeight = DEFAULT_CHECKBOX_HEIGHT;
	}
}

void UICheckbox::Create(UICreateContext& ctx)
{
	Id = ctx.dialog->GenerateDialogId();

	ParentWnd = ctx.owner->GetWnd();

	// compute width of checkbox, otherwise it would react on whole parent's width area
	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth, NULL, ctx.dialog->GetWnd());

	// add DEFAULT_CHECKBOX_HEIGHT to 'Width' to include checkbox rect
	UIRect ctlRect = Rect;
	ctlRect.Width = min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Rect.Width);

	Wnd = ctx.MakeWindow(this, WC_BUTTON, *Label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, &ctlRect);

	CheckDlgButton(ParentWnd, Id, (*pValue ^ bInvertValue) ? BST_CHECKED : BST_UNCHECKED);
	UpdateEnabled();
}

bool UICheckbox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd != BN_CLICKED) return false;
	bool checked = (IsDlgButtonChecked(ParentWnd, Id) != BST_UNCHECKED) ^ bInvertValue;
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
	Layout.Height = DEFAULT_CHECKBOX_HEIGHT;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

UIRadioButton::UIRadioButton(const char* text, int value, bool autoSize)
:	Label(text)
,	Checked(false)
,	Value(value)
,	AutoSize(autoSize)
{
	IsRadioButton = true;
	Layout.Height = DEFAULT_CHECKBOX_HEIGHT;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

void UIRadioButton::UpdateSize(UIBaseDialog* dialog)
{
	int radioWidth;
	MeasureTextSize(*Label, &radioWidth, NULL, dialog->GetWnd());
	radioWidth += DEFAULT_CHECKBOX_HEIGHT;

	if (AutoSize)
	{
		Layout.Width = radioWidth;
	}
	else if (MinWidth == 0)
	{
		MinWidth = radioWidth;
	}

	if (MinHeight == 0)
	{
		MinHeight = DEFAULT_CHECKBOX_HEIGHT;
	}
}

void UIRadioButton::Create(UICreateContext& ctx)
{
	Id = ctx.dialog->GenerateDialogId();

	HWND DlgWnd = ctx.dialog->GetWnd();

	// compute width of checkbox, otherwise it would react on whole parent's width area
	int radioWidth;
	MeasureTextSize(*Label, &radioWidth, NULL, DlgWnd);

	// add DEFAULT_CHECKBOX_HEIGHT to 'Width' to include checkbox rect
	UIRect ctlRect = Rect;
	ctlRect.Width = min(radioWidth + DEFAULT_CHECKBOX_HEIGHT, Rect.Width);

	Wnd = ctx.MakeWindow(this, WC_BUTTON, *Label, WS_TABSTOP | BS_AUTORADIOBUTTON, 0, &ctlRect);

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
	Layout.Height = DEFAULT_EDIT_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = DEFAULT_EDIT_HEIGHT;
}

UITextEdit::UITextEdit(FString* value)
:	pValue(value)
//,	sValue(value) - uninitialized, unused
,	IsMultiline(false)
,	IsReadOnly(false)
,	IsWantFocus(true)
{
	Layout.Height = DEFAULT_EDIT_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = DEFAULT_EDIT_HEIGHT;
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

void UITextEdit::Create(UICreateContext& ctx)
{
	Id = ctx.dialog->GenerateDialogId();

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

	Wnd = ctx.MakeWindow(this, WC_EDIT, "", style, WS_EX_CLIENTEDGE);
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
,	pValue(&Value)
,	Selection(-1)
{
	Layout.Height = DEFAULT_COMBOBOX_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = DEFAULT_COMBOBOX_HEIGHT;
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

UICombobox& UICombobox::AddItem(const char* item, int value)
{
	new (Items) ComboboxItem(item, value);
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
	if (Selection == index) return *this;
	Selection = index;
	if (Wnd) SendMessage(Wnd, CB_SETCURSEL, Selection, 0);
	return *this;
}

UICombobox& UICombobox::SelectItem(const char* item)
{
	int index = -1;
	for (int i = 0; i < Items.Num(); i++)
	{
		if (!stricmp(*Items[i].Text, item))
		{
			index = i;
			break;
		}
	}
	SelectItem(index);
	return *this;
}

void UICombobox::Create(UICreateContext& ctx)
{
	Id = ctx.dialog->GenerateDialogId();

	// Note: we're sending DEFAULT_COMBOBOX_LIST_HEIGHT instead of control's Height here, otherwise
	// dropdown list could appear empty (with zero height) on systems not supporting visual styles
	// (pre-XP Windows) or when dropdown list is empty
	UIRect ctlRect = Rect;
	ctlRect.Height = DEFAULT_COMBOBOX_LIST_HEIGHT;

	Wnd = ctx.MakeWindow(this, WC_COMBOBOX, "",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE, &ctlRect);
	// add items and find selected item
	int v = *pValue;
	if (Selection < 0)
	{
		// if all items will have uninitialized value, then we'll assume values 0,1,2...
		Selection = v;
	}
	for (int i = 0; i < Items.Num(); i++)
	{
		const ComboboxItem& item = Items[i];
		SendMessage(Wnd, CB_ADDSTRING, 0, (LPARAM)(*item.Text));
		if (item.Value >= 0 && item.Value == v)
		{
			Selection = i;
		}
	}
	// set selection
	SendMessage(Wnd, CB_SETCURSEL, Selection, 0);
	UpdateEnabled();
}

bool UICombobox::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == CBN_SELCHANGE)
	{
		int v = (int)SendMessage(Wnd, CB_GETCURSEL, 0, 0);
		if (v != Selection)
		{
			Selection = v;
			int m = Items[v].Value;
			*pValue = m;
			if (Callback)
				Callback(this, *pValue, GetSelectionText());
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
	Layout.Height = DEFAULT_LISTBOX_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = DEFAULT_LISTBOX_HEIGHT;
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
}

UIListbox& UIListbox::ReserveItems(int count)
{
	Items.Reserve(Items.Num() + count);
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

void UIListbox::Create(UICreateContext& ctx)
{
	Id = ctx.dialog->GenerateDialogId();

	Wnd = ctx.MakeWindow(this, WC_LISTBOX, "",
		LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE);
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
	InitUXTheme();
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
	Layout.Height = DEFAULT_LISTBOX_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = MIN_CONTROL_WIDTH;
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;

	assert(NumColumns > 0 && NumColumns <= MAX_COLUMNS);
	Items.AddZeroed(numColumns);	// reserve place for header
	for (int i = 0; i < numColumns; i++)
	{
		ColumnSizes[i] = 0;
		ColumnAlign[i] = TA_Left;
	}
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
	Items.Reserve((GetItemCount() + count + 1) * NumColumns);
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
	guard(UIMulticolumnListbox::RemoveAllItems);
	// remove items from local storage and from control
	int numStrings = Items.Num();
	if (numStrings > NumColumns)
		Items.RemoveAt(NumColumns, numStrings - NumColumns);

	if (Wnd)
	{
		if (!IsVirtualMode)
		{
			ListView_DeleteAllItems(Wnd);
		}
		else
		{
			ListView_SetItemCount(Wnd, 0);
		}
	}

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
	unguard;
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

void UIMulticolumnListbox::Create(UICreateContext& ctx)
{
	int i;

	Id = ctx.dialog->GenerateDialogId();

	DWORD style = Multiselect ? 0 : LVS_SINGLESEL;
	if (IsVirtualMode) style |= LVS_OWNERDATA;

	Wnd = ctx.MakeWindow(this, WC_LISTVIEW, "",
		style | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE);
	ListView_SetExtendedListViewStyle(Wnd, LVS_EX_FLATSB | LVS_EX_LABELTIP);

#if USE_EXPLORER_STYLE
	SetExplorerTheme(Wnd);
#endif

	// compute automatic column width
	int clientWidth = Rect.Width - GetSystemMetrics(SM_CXVSCROLL) - 6; // exclude scrollbar and border areas
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
	assert(totalWidth <= Rect.Width);
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

bool UIMulticolumnListbox::HandleCommand(int id, int cmd, LPARAM lParam, int& result)
{
	guard(UIMulticolumnListbox::HandleCommand);

	// Say "message has been processed" (will change to FALSE at the end when needed)
	result = TRUE;

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
		return true;
	}

	if (cmd == LVN_ODFINDITEM && IsVirtualMode)
	{
		// Search for item
		result = -1;
		NMLVFINDITEM* nmf = (NMLVFINDITEM*)lParam;
		if ((nmf->lvfi.flags & LVFI_STRING) == 0)
			return false;
		int start = nmf->iStart;
		int numItems = GetItemCount();
		if (start >= numItems)
			start = 0;
		int cmpLen = strlen(nmf->lvfi.psz);
		for (int itemIndex = start; itemIndex < numItems; itemIndex++)
		{
			const char* text = "";
			if (!IsTrueVirtualMode())
			{
				text = const_cast<char*>(*Items[(itemIndex + 1) * NumColumns /*+ subItemIndex*/]);
			}
			else
			{
				OnGetItemText(this, text, itemIndex, 0 /*subItemIndex*/);
			}

			if (strnicmp(text, nmf->lvfi.psz, cmpLen) == 0)
			{
				result = itemIndex;
				return true;
			}
		}
		return true;
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
				return true;
			}
		}
		// The key was not recognized, process it with default handler (pass 'result=FALSE' for that)
		result = FALSE;
		return true;
	}

	if (cmd == NM_RCLICK && Menu)
	{
		// Get mouse pointer location
		POINT pt;
		GetCursorPos(&pt);
		// Show menu
		Menu->Popup(this, pt.x, pt.y);
		return true;
	}

	// Say "message was NOT processed"
	result = FALSE;
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
,	bHasRootNode(true)
,	ItemHeight(DEFAULT_TREE_ITEM_HEIGHT)
{
	Layout.Height = DEFAULT_TREEVIEW_HEIGHT;
	MinWidth = MIN_CONTROL_WIDTH;
	MinHeight = MIN_CONTROL_WIDTH;
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;

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

TreeViewItem* UITreeView::FindItem(void* hItem)
{
	for (int i = 0; i < Items.Num(); i++)
	{
		TreeViewItem* item = Items[i];
		if (item->hItem == hItem)
		{
			return item;
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

const char* UITreeView::GetSelectedItem()
{
	return SelectedItem ? *SelectedItem->Label : NULL;
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

void UITreeView::Create(UICreateContext& ctx)
{
	guard(UITreeView::Create);

	Id = ctx.dialog->GenerateDialogId();

	Wnd = ctx.MakeWindow(this, WC_TREEVIEW, "",
		TVS_HASLINES | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | WS_VSCROLL | WS_TABSTOP,
		WS_EX_CLIENTEDGE);
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
	guard(CreateItems);
	for (int i = 0; i < Items.Num(); i++)
		CreateItem(*Items[i]);
	unguard;

	// set selection
	TreeView_SelectItem(Wnd, SelectedItem->hItem);

	UpdateEnabled();

	unguard;
}

bool UITreeView::HandleCommand(int id, int cmd, LPARAM lParam)
{
	if (cmd == TVN_SELCHANGED)
	{
		LPNMTREEVIEW data = (LPNMTREEVIEW)lParam;
		TreeViewItem* item = FindItem(data->itemNew.hItem);
		if (item && SelectedItem != item)
		{
			SelectedItem = item;
			if (Callback)
				Callback(this, *item->Label);
		}
		return true;
	}
	else if (cmd == NM_RCLICK && Menu)
	{
		// Get mouse pointer location
		POINT pt;
		GetCursorPos(&pt);

		// Get tree item under cursor
		TV_HITTESTINFO tvhip;
		tvhip.pt = pt;
		ScreenToClient(Wnd, &tvhip.pt);
		HTREEITEM hItem = (HTREEITEM)SendMessage(Wnd, TVM_HITTEST, 0, (LPARAM)&tvhip);
		TreeViewItem* item = FindItem(hItem);

		// Save selection: Win32 will temporarily highlight right-clicked item, and restore selection later.
		TreeViewItem* oldSelection = SelectedItem;
		// Set selection to new item
		SelectedItem = item;
		// Show menu
		Menu->Popup(this, pt.x, pt.y);
		// Restore selection
		SelectedItem = oldSelection;
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

	if (!item.Parent && !bHasRootNode)
	{
		return;
	}

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
	if (item.Parent && bHasRootNode)
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
	UIGroup
-----------------------------------------------------------------------------*/

UIGroup::UIGroup(const char* label, unsigned flags)
:	Label(label)
,	FirstChild(NULL)
,	Flags(flags)
,	RadioValue(0)
,	pRadioValue(&RadioValue)
{
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
	IsGroup = true;
}

UIGroup::UIGroup(unsigned flags)
:	FirstChild(NULL)
,	Flags(flags)
,	OwnsControls(false)
,	RadioValue(0)
,	pRadioValue(&RadioValue)
{
	TopMargin = DEFAULT_MARGIN;
	BottomMargin = DEFAULT_MARGIN;
	LeftMargin = DEFAULT_MARGIN;
	RightMargin = DEFAULT_MARGIN;
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

bool UIGroup::HandleChildMessages(int uMsg, WPARAM wParam, LPARAM lParam, int& result)
{
	int cmd = -1;		// control id
	int id = 0;			// passed command

	if (uMsg == WM_COMMAND)
	{
		id  = LOWORD(wParam);
		cmd = HIWORD(wParam);
		if (id == IDOK || id == IDCANCEL)
		{
			GetDialog()->CloseDialog(id != IDOK);
			result = TRUE;
			return true;
		}
	}
	else if (uMsg == WM_NOTIFY)
	{
		// handle WM_NOTIFY in a similar way
		id  = LOWORD(wParam);
		cmd = ((LPNMHDR)lParam)->code;
	}

	if (cmd != -1 && (id >= FIRST_DIALOG_ID) /* && id < NextDialogId */)
	{
		result = FALSE;
		HandleCommand(id, cmd, lParam, result); // ignore result
		return true;
	}

	return false;
}

bool UIGroup::HandleCommand(int id, int cmd, LPARAM lParam, int& result)
{
	for (UIElement* ctl = FirstChild; ctl; ctl = ctl->NextChild)
	{
		if (ctl->IsGroup || ctl->Id == id)
		{
			// pass command to control or all groups
			if (ctl->HandleCommand(id, cmd, lParam, result))
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

void UIGroup::Create(UICreateContext& ctx)
{
	CreateGroupControls(ctx);
	// Disable all children controls if this UIGroup is disabled
	if (!Enabled)
	{
		EnableAllControls(Enabled);
	}
}

void UIGroup::CreateGroupControls(UICreateContext& ctx)
{
	guard(UIGroup::CreateGroupControls);

/*	UIElement* saveOwner = ctx.owner;
	if (OwnsControls)
	{
		ctx.owner = this;
	} */

	if (!(Flags & GROUP_NO_BORDER))
	{
		// create a group window (border)
		Wnd = ctx.MakeWindow(this, WC_BUTTON, *Label, BS_GROUPBOX | WS_GROUP, 0);
	}
/*	else if (OwnsControls)
	{
		Wnd = ctx.MakeWindow(this, WC_BUTTON, *Label, BS_GROUPBOX | WS_GROUP, 0); -- todo: should create some host for child controls, "Button" or "Static" won't work
	} */

	// call 'Create' for all children
	bool isRadioGroup = false;
	int controlIndex = 0;
	for (UIElement* control = FirstChild; control; control = control->NextChild, controlIndex++)
	{
		guard(ControlCreate);
		control->Create(ctx);
		unguardf("index=%d,class=%s", controlIndex, control->ClassName());

		if (control->IsRadioButton) isRadioGroup = true;
	}
	if (isRadioGroup) InitializeRadioGroup();

//	ctx.owner = saveOwner;

	unguardf("%s", *Label);
}

void UIGroup::UpdateEnabled()
{
	Super::UpdateEnabled();
	EnableAllControls(Enabled);
}

void UIGroup::UpdateVisible()
{
	Super::UpdateVisible();
	if (!OwnsControls)
	{
		ShowAllControls(Visible);
	}
}

//?? todo: rename function because it does more than UpdateSize() call
void UIGroup::UpdateSize(UIBaseDialog* dialog)
{
	// allow UIGroup-based classes to add own controls
	AddCustomControls();

	// Call UpdateSize() for all child controls
	for (UIElement* control = FirstChild; control; control = control->NextChild)
	{
		control->UpdateSize(dialog);
	}

	// Estimate relative sizes for controls which has auto-size (width or height set to -1).
	float TotalFracSize = 0.0f;
	int NumAutoSizeControls = 0;
	int EncodedAutoWidth = 0;
	for (int pass = 0; pass < 2; pass++)
	{
		for (UIElement* control = FirstChild; control; control = control->NextChild)
		{
			// Get relevant size of control
			int Size = 0;
			if (Flags & GROUP_HORIZONTAL_LAYOUT)
			{
				Size = control->Layout.Width;
			}
			else
			{
				Size = control->Layout.Height;
			}
			// UIGroup has special meaning of -1 size, so skip such controls
			if (Size == -1 && control->IsGroup)
			{
				continue;
			}
			if (Size == -1)
			{
				// Auto-size control
				if (pass == 0)
				{
					NumAutoSizeControls++;
				}
				else
				{
					// Store computed size back to control
					if (Flags & GROUP_HORIZONTAL_LAYOUT)
					{
						control->Layout.Width = EncodedAutoWidth;
					}
					else
					{
						control->Layout.Height = EncodedAutoWidth;
					}
				}
			}
			else if (Size < 0)
			{
				// Control with fraction width
				TotalFracSize += DecodeWidth(Size);
			}
		}
		if (pass == 0)
		{
			// Compute automatic size at pass 0
			if (NumAutoSizeControls == 0 || TotalFracSize >= 1.0f)
			{
				break;
			}
			EncodedAutoWidth = EncodeWidth((1.0f - TotalFracSize) / NumAutoSizeControls);
		}
	}
}

void UIGroup::UpdateLayout()
{
	if (Parent != NULL)
	{
		// Do not call UpdateLayout for dialog
		Super::UpdateLayout();
	}

	for (UIElement* control = FirstChild; control; control = control->NextChild)
	{
		control->UpdateLayout();
	}
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
:	UIGroup(label, flags)
,	bValue(value)
,	pValue(&bValue)		// points to local variable
,	CheckboxWnd(0)
{}

UICheckboxGroup::UICheckboxGroup(const char* label, bool* value, unsigned flags)
:	UIGroup(label, flags)
//,	bValue(value) - uninitialized, unused
,	pValue(value)
,	CheckboxWnd(0)
{}

void UICheckboxGroup::Create(UICreateContext& ctx)
{
	// Call UIGroup::Create with hiding Label from this function. We'll make
	// own label using checkbox control
	FString tmpLabel(Detail::MoveTemp(Label));
	UIGroup::Create(ctx);
	Label = Detail::MoveTemp(tmpLabel);

	Id = ctx.dialog->GenerateDialogId();
	ParentWnd = ctx.owner->GetWnd();

	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth);

	int checkboxOffset = (Flags & GROUP_NO_BORDER) ? 0 : GROUP_INDENT;
	UIRect ctlRect(
		Rect.X + checkboxOffset,
		Rect.Y,
		min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Rect.Width - checkboxOffset),
		DEFAULT_CHECKBOX_HEIGHT);

	// Adjust checkbox Y
	ctlRect.Y -= 2;
	CheckboxWnd = ctx.MakeWindow(this, WC_BUTTON, *Label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, &ctlRect);

	CheckDlgButton(ParentWnd, Id, *pValue ? BST_CHECKED : BST_UNCHECKED);
	EnableAllControls(*pValue);
}

bool UICheckboxGroup::HandleCommand(int id, int cmd, LPARAM lParam, int& result)
{
	if (id == Id)
	{
		// checkbox
		if (cmd != BN_CLICKED) return false;

		bool checked = (IsDlgButtonChecked(ParentWnd, Id) != BST_UNCHECKED);
		if (*pValue != checked)
		{
			*pValue = checked;
			EnableAllControls(*pValue);
			if (Callback)
				Callback(this, checked);
			return true;
		}
	}
	return Super::HandleCommand(id, cmd, lParam, result);
}

void UICheckboxGroup::UpdateVisible()
{
	UIGroup::UpdateVisible();
	ShowWindow(CheckboxWnd, Visible ? SW_SHOW : SW_HIDE);
}

void UICheckboxGroup::UpdateLayout()
{
	Super::UpdateLayout();

	//TODO: this is a copy-paste of part of Create() method
	int checkboxWidth;
	MeasureTextSize(*Label, &checkboxWidth);

	int checkboxOffset = (Flags & GROUP_NO_BORDER) ? 0 : GROUP_INDENT;

	MoveWindow(CheckboxWnd, Rect.X + checkboxOffset, Rect.Y - 2, min(checkboxWidth + DEFAULT_CHECKBOX_HEIGHT, Rect.Width - checkboxOffset), DEFAULT_CHECKBOX_HEIGHT, FALSE);
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

bool UIPageControl::HandleCommand(int id, int cmd, LPARAM lParam, int& result)
{
	// Pass command to active page
	int pageIndex = 0;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
	{
		if (pageIndex == ActivePage)
		{
			if (page->HandleCommand(id, cmd, lParam, result))
				return true;
			break;
		}
	}
	return false;
}

void UIPageControl::UpdateVisible()
{
	// Update visibility only for active page, not calling parent method
	int pageIndex = 0;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
	{
		if (pageIndex == ActivePage)
		{
			page->Show(Visible);
			page->UpdateVisible();
			break;
		}
	}
}

void UIPageControl::Create(UICreateContext& ctx)
{
	guard(UIPageControl::Create);

	int pageIndex = 0;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
	{
		page->Show(pageIndex == ActivePage);

		guard(PageCreate);
		page->Create(ctx);
		unguardf("index=%d,class=%s", pageIndex, page->ClassName());
	}

	unguard;
}

void UIPageControl::ComputeLayout()
{
	ComputeLayoutWithBorders(0, 0, 0, 0);
}


/*-----------------------------------------------------------------------------
	UITabControl
-----------------------------------------------------------------------------*/

UITabControl::UITabControl()
{
	// Own child controls for better appearance
	OwnsControls = true;
}

/*
	There's a big issue with using win32 TabControl with correct background of child "static" controls.
	These controls appears with grey background which tab page may have white themed background. There's
	lots of discussions, and the most suitable way to achieve matching colors is to put all page controls
	into a nested dialog, and call function EnableThemeDialogTexture(PageDialogWnd, ETDT_ENABLETAB). This
	method will work ONLY if page controls are hosted by dialog. When not - this is a really tricky case.
	It is possible to disable TabControl theming at all - with use of

		SetWindowTheme(Wnd, L"", L"");

	call. This will make all pages grey. It is possible to create own "static" (label) with transparent
	background, or subclass original static to use transparent background. It is possible to specify
	background color in WM_CTLCOLORSTATIC, however there's no window where we could redirect this message
	to get correct background. So, we're going a way similar to used in wxWidgets: trying to get fixed
	color (textured background won't work).

	For reference, the most relevant discussion about this topic, which is made with Microsoft's technical
	stuff. Other discussions found in in Internet are not so competent, and usually based on guessing.
	https://microsoft.public.win32.programmer.ui.narkive.com/qRtjCoB9/control-background-color-on-systabcontrol32
 */

static HBRUSH GetTabBackgroundBrush(HWND Wnd)
{
	// Reference: wxWidgets, src/msw/notebook.cpp, wxNotebook::GetThemeBackgroundColour()
	InitUXTheme();

	// Note: caching color this way will not let
	// 1) have different color for different tab controls (e.g. if one of them has theme disabled)
	// 2) will not catch changing of theme during app run
	static HBRUSH backgroundBrush = 0;
	if (backgroundBrush != 0)
		return backgroundBrush;

	// Strange thing, but it works correctly compared to CreateSolidBrush(GetSysColor(COLOR_BTNFACE))
	backgroundBrush = (HBRUSH)(COLOR_BTNFACE+1);

	if (IsAppThemed() && IsThemeActive())
	{
		COLORREF backgroundColor = GetSysColor(COLOR_WINDOW);
#if 0
		// wxWidgets way, unfortunately didn't get fine appearance, "static" color is still grey.
		HANDLE hTheme = OpenThemeData(Wnd, L"TAB");
		appPrintf("theme -> %p\n", hTheme);
		if (hTheme)
		{
			COLORREF themeColor;
			if (GetThemeColor(hTheme, 10 /* TABP_BODY */, 1 /* NORMAL */, 3821 /* FILLCOLORHINT */, &themeColor) == S_OK)
			{
				if (themeColor == 1)
				{
					GetThemeColor(hTheme, 10 /* TABP_BODY */, 1 /* NORMAL */, 3802 /* FILLCOLOR */, &themeColor);
				}
			}
			backgroundColor = themeColor;
			CloseThemeData(hTheme);
		}
#endif
		// Make a brush. It is shared between TabControls, and if we'll need to change it - object should be released.
		backgroundBrush = CreateSolidBrush(backgroundColor);
	}
	return backgroundBrush;
}

LONG_PTR UITabControl::SubclassProc(void* hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CTLCOLORSTATIC)
	{
		return (LONG_PTR)GetTabBackgroundBrush(Wnd);
	}

	int result = -1;
	if (HandleChildMessages(uMsg, wParam, lParam, result))
	{
		if (result != 0)
		{
			// Do not call default proc only in a case if message was handled.
			// In this case, HandleChildMessages returns 'true' (what means control was found),
			// and 'result' will be TRUE, what means - message was processed.
			//todo: should review this, probably use subclass for critical elements instead
			//todo: of HandleCommand calls.
			// WHY this "if" was added: without it, TabControl holding ListView will not
			// let ListView to show its items - items will be there, but with empty text.
			return result;
		}
	}

	return DefSubclassProc((HWND)hWnd, uMsg, wParam, lParam);
}

void UITabControl::Create(UICreateContext& ctx)
{
	guard(UIPageControl::Create);

	// Works without this code
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&iccex);

	// Create a tab control window
	Id = ctx.dialog->GenerateDialogId();
	Wnd = ctx.MakeWindow(this, WC_TABCONTROL, *Label, WS_TABSTOP, 0);
	EnableSubclass();

#if 0 // can use this snippet to disable theme for this TabControl
	InitUXTheme();
	SetWindowTheme(Wnd, L"", L"");
#endif

	// Create children under TabControl's window
	UIElement* saveOwner = ctx.owner;
	ctx.owner = this;

	int pageIndex = 0;
	for (UIElement* page = FirstChild; page; page = page->NextChild, pageIndex++)
	{
		guard(PageCreate);

		// Create page controls
		page->Show(pageIndex == ActivePage);
		page->Create(ctx);

		// Create tab item
		TCITEM tie;
		tie.mask = TCIF_TEXT;
		tie.iImage = -1;
		tie.pszText = NULL;
		if (page->IsGroup)
		{
			// Pick group's label as tab name, despite group should have no-border style
			UIGroup* pageGroup = static_cast<UIGroup*>(page);
			if (!pageGroup->Label.IsEmpty())
			{
				tie.pszText = const_cast<char*>(*pageGroup->Label);
			}
		}
		if (tie.pszText == NULL)
		{
			// Should be a UIGroup, but still add something for other controls.
			// Also there could be UIGroup with no label. Both cases are mistakes.
			tie.pszText = const_cast<char*>(page->ClassName());
		}
		// Add tab
		TabCtrl_InsertItem(Wnd, pageIndex, &tie);

		unguardf("index=%d,class=%s", pageIndex, page->ClassName());
	}

	TabCtrl_SetCurSel(Wnd, ActivePage);

	ctx.owner = saveOwner;

	unguard;
}

bool UITabControl::HandleCommand(int id, int cmd, LPARAM lParam, int& result)
{
	if (id == Id)
	{
		// A message for us
		if (cmd == TCN_SELCHANGE)
		{
			int iPage = TabCtrl_GetCurSel(Wnd);
			SetActivePage(iPage);
		}
		return true;
	}
	return UIPageControl::HandleCommand(id, cmd, lParam, result);
}

void UITabControl::UpdateVisible()
{
	// Update tab control's visibility, not including pages
	UIElement::UpdateVisible();
	// Update page's visibility
	UIPageControl::UpdateVisible();
}

void UITabControl::ComputeLayout()
{
	ComputeLayoutWithBorders(TAB_MARGIN_OTHER, TAB_MARGIN_OTHER, TAB_MARGIN_TOP, TAB_MARGIN_OTHER);
}


/*-----------------------------------------------------------------------------
	UIBaseDialog
-----------------------------------------------------------------------------*/

UIBaseDialog::UIBaseDialog()
:	UIGroup(GROUP_NO_BORDER)
,	NextDialogId(FIRST_DIALOG_ID)
,	ShouldCloseOnEsc(false)
,	ShouldHideOnClose(false)
,	bResizeable(false)
,	ParentDialog(NULL)
,	IconResId(0)
,	IsDialogConstructed(false)
,	DisabledOwnerWnd(NULL)
,	ClosingDialog(false)
{}

UIBaseDialog::~UIBaseDialog()
{
	ShouldHideOnClose = false;
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

static DLGTEMPLATE* MakeDialogTemplate(int width, int height, const wchar_t* title, const wchar_t* fontName, int fontSize, bool resizeable)
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

	if (resizeable)
	{
		dlg1->style |= WS_THICKFRAME;
	}

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

	assert(Wnd == 0 || (modal && ShouldHideOnClose));

	if (!hInstance)
	{
		hInstance = GetModuleHandle(NULL);
		InitCommonControls();
	}

	if (Wnd == 0)
		NextDialogId = FIRST_DIALOG_ID;

	ClosingDialog = false;

	Layout.Width = width;
	Layout.Height = height;

	// convert title to unicode
	wchar_t wTitle[MAX_TITLE_LEN];
	mbstowcs(wTitle, title, MAX_TITLE_LEN);
	// use fixed constants for window size, will be changed after creation anyway
	DLGTEMPLATE* tmpl = MakeDialogTemplate(100, 50, wTitle, L"MS Shell Dlg", 8, bResizeable);

	HWND ParentWindow = GMainWindow;
	if (GCurrentDialog) ParentWindow = GCurrentDialog->GetWnd();

	if (modal && !ShouldHideOnClose)
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
	else if (modal) // && ShouldHideOnClose
	{
		ParentDialog = GCurrentDialog;
		GCurrentDialog = this;
		// modeless window, modal behavior
		if (Wnd == NULL)
		{
			HWND dialog = CreateDialogIndirectParam(
				hInstance,					// hInstance
				tmpl,						// lpTemplate
				ParentWindow,				// hWndParent
				StaticWndProc,				// lpDialogFunc
				(LPARAM)this				// lParamInit
			);
			assert(dialog);
			Wnd = dialog;
		}
		else
		{
			// We're not creating a window, so update its parent - it might be changed from last time call
			SetWindowLongPtr(Wnd, GWLP_HWNDPARENT, (LONG_PTR)ParentWindow);
			ShowDialog();
		}

#if DO_GUARD
		TRY {
#endif
			CustomMessageLoop(true);
			//?? Catch modal result here - not used now at all

			GCurrentDialog = ParentDialog;
			ParentDialog = NULL;
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
		// fully modeless window
		HWND dialog = CreateDialogIndirectParam(
			hInstance,					// hInstance
			tmpl,						// lpTemplate
			ParentWindow,				// hWndParent
			StaticWndProc,				// lpDialogFunc
			(LPARAM)this				// lParamInit
		);
		assert(dialog);
		// process all messages to allow window to appear on screen
		PumpMessages();
	}

	return true;

	unguardf("modal=%d, title=\"%s\"", modal, title);
}

void UIBaseDialog::ShowDialog()
{
	Visible = true;
	ShowWindow(Wnd, SW_SHOW);
}

void UIBaseDialog::HideDialog()
{
	Visible = false;
	ShowWindow(Wnd, SW_HIDE);
	// Don't call Show(false) because it will propagate visibility to all children
}

void UIBaseDialog::BeginModal()
{
	if (DisabledOwnerWnd == NULL)
	{
		// Disable parent window
		HWND Owner = (HWND)GetWindowLongPtr(Wnd, GWLP_HWNDPARENT);
		if (Owner != NULL)
		{
			BOOL OldEnabled = IsWindowEnabled(Owner);
			if (OldEnabled)
			{
				EnableWindow(Owner, FALSE);
				DisabledOwnerWnd = Owner;
			}
		}
	}
}

void UIBaseDialog::EndModal()
{
	if (DisabledOwnerWnd != NULL)
	{
		EnableWindow(DisabledOwnerWnd, TRUE);
		// When 'this' window is closed (hidden), owner window is disabled. This will cause wrong window to get
		// focus. Fix that be manually changing focus.
		SetForegroundWindow(DisabledOwnerWnd);
		DisabledOwnerWnd = NULL;
	}
}

void UIBaseDialog::DispatchWindowsMessage(void* pMsg)
{
	MSG& msg = *(MSG*)pMsg;

	if (msg.message == WM_KEYDOWN && ShouldCloseOnEsc && msg.wParam == VK_ESCAPE)
	{
		// Win32 dialog boxes doesn't receive keyboard messages. By the way, modal boxes receives IDOK
		// or IDCANCEL commands when user press 'Enter' or 'Escape'. In order to handle the 'Escape' key
		// in NON-modal boxes, we should have our own message loop. We have one for non-modal dialog
		// boxes here. We don't have access to the message loop of modal dialog box. Note: we are comparing
		// msg.hwnd with dialog's window, and also comparing msg.hwnd's parent with dialog too. No other
		// checks are performed because we have very simple hierarchy in our UI system: all children are
		// parented by single dialog window.
		// If we'll need more robust way of processing messages (for example, when we need to process
		// keys for modal dialogs, or when message loop is processed by code which is not accessible for
		// modification) - we'll need to use SetWindowsHook. Another way is to subclass all controls
		// (because key messages are sent to the focused window only, and not to its parent), but it looks
		// more complicated.
		if (msg.hwnd == Wnd || ::GetParent(msg.hwnd) == Wnd)
			CloseDialog(true);
	}

	TranslateMessage(&msg);
	DispatchMessage(&msg);
}

bool UIBaseDialog::PumpMessages()
{
	guard(UIBaseDialog::PumpMessages);

	if (Wnd == 0) return false;

	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		DispatchWindowsMessage(&msg);
	}
	// Return false if window has been closed
	return (Wnd != 0);

	unguard;
}

// Similar to PumpMessages, but using blocking GetMessage() call instead of PeekMessage(). Also it has
// 'modal' capabilities.
void UIBaseDialog::CustomMessageLoop(bool modal)
{
	guard(UIBaseDialog::CustomMessageLoop);

	if (Wnd == 0) return;

	if (modal)
	{
		BeginModal();
	}

	// Classic message loop, based on GetMessage() call. We're checking 'ClosingDialog' before GetMessage()
	// to exit modal message loop when window is hidden instead of being closed, otherwise application will
	// hang with waiting for a message for invisible window.
	MSG msg;
	while (!ClosingDialog && GetMessage(&msg, NULL, 0, 0))
	{
		DispatchWindowsMessage(&msg);
	}

	if (modal)
	{
		EndModal();
	}

	unguard;
}

void UIBaseDialog::CloseDialog(bool cancel)
{
	if (Wnd && CanCloseDialog(cancel))
	{
		EndModal();
		if (!ShouldHideOnClose)
		{
			DialogClosed(cancel);
			EndDialog(Wnd, cancel ? IDCANCEL : IDOK);
			Wnd = 0;
		}
		else if (IsVisible())
		{
			HideDialog();
			ClosingDialog = true;
		}
	}
}

void UIBaseDialog::SetWindowSize(int width, int height)
{
	if (!Wnd) return;

	SetWindowPos(Wnd, NULL, 0, 0, width, height, SWP_NOMOVE);
	WindowsSizeChanged();
}

void UIBaseDialog::WindowsSizeChanged()
{
	RECT r;
	GetClientRect(Wnd, &r);
	int clientWidth = r.right - r.left;
	int clientHeight = r.bottom - r.top;

	Layout.X = DEFAULT_HORZ_BORDER;
	Layout.Y = VERTICAL_SPACING;
	Layout.Width = clientWidth - DEFAULT_HORZ_BORDER*2;
	Layout.Height = clientHeight - VERTICAL_SPACING;

	Rect = Layout;

	ComputeLayout();

	UpdateLayout();
	Repaint();
}

static void (*GUIExceptionHandler)() = NULL;

void UISetExceptionHandler(void (*Handler)())
{
	GUIExceptionHandler = Handler;
}

/*static*/INT_PTR CALLBACK UIBaseDialog::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
		SetWindowLongPtr(hWnd, DWLP_MSGRESULT, 0);
		INT_PTR result = dlg->WndProc(hWnd, msg, wParam, lParam);
		if (result)
		{
			// For DlgProc we should store result in DWLP_MSGRESULT.
			// https://devblogs.microsoft.com/oldnewthing/?p=41923
#if 0 // tiny "spy++" analog code
			appPrintf("Msg: %X", msg);
			if (msg == WM_NOTIFY) appPrintf(" Id: %d N: %d R: %d", LOWORD(wParam), ((LPNMHDR)lParam)->code, result);
			appPrintf("\n");
#endif
			SetWindowLongPtr(hWnd, DWLP_MSGRESULT, result);
		}
		return result;
	} CATCH_CRASH {
#if MAX_DEBUG
		// sometimes when working with debugger, exception inside UI could not be passed outside,
		// and program crashed bypassing out exception handler - always show error information in
		// MAX_DEBUG mode
		if (GError.History[0])
		{
			appNotify("ERROR in WindowProc: %s\n", GError.History);
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

		// release old controls, if dialog is opened for the 2nd time
		ReleaseControls();

		// create controls (UIElement objects, not windows)
		IsDialogConstructed = false;
		InitUI();

		// prepare layout variables

		// adjust layout taking into account margins, so "client rect" of the window
		// will match original Layout.Width and Layout.Height
		Layout.X = DEFAULT_HORZ_BORDER;
		Layout.Y = VERTICAL_SPACING;
		if (Layout.Width > 0)
		{
			Layout.Width -= DEFAULT_HORZ_BORDER*2;
		}
		if (Layout.Height > 0)
		{
			Layout.Height -= VERTICAL_SPACING;
		}

		UpdateSize(this);

		Rect = Layout;

		// compute window's minimal size
		if (bResizeable)
		{
			// perform ComputeLayout() call with zero Rect.Width and Rect.Height to compute
			// minimal window size
			Rect.Width = Rect.Height = 0;
			ComputeLayout();
			MinWidth = Rect.Width + DEFAULT_HORZ_BORDER*2;
			MinHeight = Rect.Height + VERTICAL_SPACING;
		}

		if (Layout.Width <= 0 || Layout.Height <= 0)
		{
			// one of dimensions unknown
			if (!(Layout.Width <= 0 && Layout.Height <= 0 && bResizeable))
			{
				// we've already computed case with both dimensions unknown when bResizeable is true,
				// so call ComputeLayout() only for other cases
				Rect = Layout;
				ComputeLayout();
			}
		}
		else
		{
			if (bResizeable)
			{
				// for bResizeable case, we should restore Rect as it contains minimal size now
				Rect = Layout;
			}
		}

		// perform layout of controls, here we should have valid Rect.Width and Rect.Height.
		ComputeLayout();

		// create all controls (OS level)
		UICreateContext ctx(this);
		CreateGroupControls(ctx);

		if (Menu)
		{
			// we already had UIMenu::Attach() called before, so call AttachTo without updating reference count
			Menu->AttachTo(Wnd, false);
		}

		// adjust window size taking into account desired client size and center window on screen
		RECT r;
		r.left   = 0;
		r.top    = 0;
		r.right  = Rect.Width + DEFAULT_HORZ_BORDER*2;
		r.bottom = Rect.Y + Rect.Height;

		// position window at center of screen
		int newX = (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2;
		int newY = (GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2;

		// adjust size to take window borders into account, and set window's position
		AdjustWindowRect(&r, GetWindowLong(Wnd, GWL_STYLE), (Menu != NULL) ? TRUE : FALSE);
		SetWindowPos(hWnd, NULL, newX, newY, r.right - r.left, r.bottom - r.top, 0);

		// put IsDialogConstructed as the very last operation here
		IsDialogConstructed = true;

		return TRUE;
	}

	if (msg == WM_CLOSE)
	{
		CloseDialog(true);
		return TRUE;
	}

	if (msg == WM_DESTROY)
		return TRUE;

	// resize window handler
	if (msg == WM_SIZE && bResizeable && IsDialogConstructed)
	{
		WindowsSizeChanged();
		return TRUE;
	}

	// query minimal window size
	if (msg == WM_GETMINMAXINFO && bResizeable && IsDialogConstructed)
	{
		RECT r;
		r.left = 0;
		r.top = 0;
		r.right = MinWidth;
		r.bottom = MinHeight;
		AdjustWindowRect(&r, GetWindowLong(Wnd, GWL_STYLE), (Menu != NULL) ? TRUE : FALSE);

		MINMAXINFO* info = (MINMAXINFO*)lParam;
		info->ptMinTrackSize.x = r.right - r.left;
		info->ptMinTrackSize.y = r.bottom - r.top;

		return TRUE;
	}

	if (msg == WM_INITMENU && Menu != NULL)
	{
		Menu->Update();
		return TRUE;
	}

	// Menu commands
	if (msg == WM_COMMAND && Menu)
	{
		int id  = LOWORD(wParam);
		if (id >= FIRST_MENU_ID)
		{
			return (Menu->HandleCommand(id)) ? TRUE : FALSE;
		}
	}

	// Pass WM_COMMAND and WM_NOTIFY to children
	int result;
	if (HandleChildMessages(msg, wParam, lParam, result))
	{
		return result;
	}

	return FALSE;

	unguard;
}
