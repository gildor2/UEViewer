// Simple UI library.
// Copyright (C) 2022 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#ifndef __BASE_DIALOG_H__
#define __BASE_DIALOG_H__

#include "Core.h"

#include "Win32Types.h"
#include "UnCore.h"					// for TArray and FString

#include "callback.h"

// forwards
class UIMenu;
class UIElement;
class UIBaseDialog;


enum ETextAlign
{
	TA_Left,
	TA_Right,
	TA_Center,
};

struct UIRect
{
	int			X;
	int			Y;
	int			Width;
	int			Height;

	UIRect(int inX, int inY, int inWidth, int inHeight)
	: X(inX)
	, Y(inY)
	, Width(inWidth)
	, Height(inHeight)
	{}

	UIRect(const UIRect& other)
	{
		memcpy(this, &other, sizeof(UIRect));
	}

	inline void Set(int InX, int InY, int InWidth, int InHeight)
	{
		X = InX;
		Y = InY;
		Width = InWidth;
		Height = InHeight;
	}
};


/*-----------------------------------------------------------------------------
	UIElement
-----------------------------------------------------------------------------*/

struct UICreateContext
{
	UICreateContext(UIBaseDialog* pDialog);

	HWND MakeWindow(UIElement* control, const char* className, const char* text, DWORD style, DWORD exStyle, const UIRect* customRect = NULL);
	// Unicode version of Window()
	HWND MakeWindow(UIElement* control, const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle, const UIRect* customRect = NULL);

	// Dialog window which owns everything
	UIBaseDialog* dialog;
	// Owner of created windows - either dialog or group
	UIElement* owner;
	// Font used for created control
	HANDLE hDialogFont;
};

class UIElement
{
	friend UICreateContext;
	friend class UIGroup;
	friend class UIPageControl;
	friend class UITabControl;
public:
	UIElement();
	virtual ~UIElement();

	virtual const char* ClassName() const;

	// Disable/enable window updates. Guaranteed for only one locked window at a time (WinAPI restriction)
	void LockUpdate();
	virtual void UnlockUpdate();

	UIElement& Enable(bool enable);
	FORCEINLINE bool IsEnabled() const   { return Enabled; }

	UIElement& Show(bool visible);
	FORCEINLINE bool IsVisible() const   { return Visible; }

	UIElement& SetParent(UIGroup* group);
	UIGroup* GetParent()                 { return Parent; }
	const UIGroup* GetParent() const     { return Parent; }
	FORCEINLINE HWND GetWnd() const      { return Wnd; }

	UIBaseDialog* GetDialog();

	virtual bool IsA(const char* type) const
	{
		return !strcmp("UIElement", type);
	}

	// Layout functions

	UIElement& SetRect(int x, int y, int width, int height);
	FORCEINLINE int GetWidth() const     { return Layout.Width; }
	FORCEINLINE int GetHeight() const    { return Layout.Height; }

	UIElement& SetMenu(UIMenu* menu);

	void Repaint();

	//!! add SetFontHeight(int)
	/*	Also support bold-italic. Update 'Height' when AutoSize is set.
		Requires updates in MeasureTextSize()
		Snippet:
		    HFONT hFont = (HFONT)m_ProgList.SendMessage(WM_GETFONT);
    		LOGFONT lf = {0};
	    	GetObject(hFont, sizeof(LOGFONT), &lf);
	    	lf.lfWeight = FW_BOLD;
		    m_boldFont = CreateFontIndirect(&lf);
	*/

	static FORCEINLINE int EncodeWidth(float w)
	{
		w = bound(w, 0, 1);
		int iw = (int)(w * 255.0f);		// float -> int
		return 0xFFFF0000 | iw;
	}

	static FORCEINLINE float DecodeWidth(int w)
	{
#if MAX_DEBUG
		assert((w & 0xFFFFFF00) == 0xFFFF0000 || w == -1);
#endif
		return (w & 0xFF) / 255.0f;		// w=-1 -> 1.0f
	}

	// Measure text size. There should be a single line of text.
	void MeasureTextSize(const char* text, int* width, int* height = NULL, HWND wnd = 0);
	// Measure text size. 'width' contains width limit for multiline text.
	void MeasureTextVSize(const char* text, int* width, int* height = NULL, HWND wnd = 0);

	friend UIElement& operator+(UIElement& elem, UIElement& next);

protected:
	// Layout settings for this control
	UIRect		Layout;
	short		MinWidth;
	short		MinHeight;
	short		TopMargin;
	short		BottomMargin;
	short		LeftMargin;
	short		RightMargin;
	// Computed control's position depending on Layout
	UIRect		Rect;

	bool		IsGroup:1;
	bool		IsRadioButton:1;
	bool		Enabled:1;
	bool		Visible:1;
	bool		IsUpdateLocked;
	UIGroup*	Parent;
	UIElement*	NextChild;
	UIMenu*		Menu;

	HWND		Wnd;
	int			Id;

	virtual int ComputeWidth() const;
	virtual int ComputeHeight() const;

	// Subclass the control, Windows will call virtual SubclassProc for message handling.
	// Note: we're using cast HWND <-> void* to allow linkage of the program without included windows headers
	// everywhere when UI library was used (otherwise HWND may be defined in a different way in Win32Types.h,
	// what makes virtual function signature different and linker will fail).
	void EnableSubclass();
	static LONG_PTR CALLBACK StaticSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, ULONG_PTR dwRefData);
	virtual LONG_PTR SubclassProc(void* hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);

	virtual void Create(UICreateContext& ctx) = 0;
	virtual void UpdateSize(UIBaseDialog* dialog)
	{}
	virtual void UpdateLayout();
	// Process WM_COMMAND message. 'id' is useless in most cases, useful for
	// groups only.
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam)
	{
		return false;
	}
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result)
	{
		// Default implementation: call "legacy" 3-param HandleCommand
		if (HandleCommand(id, cmd, lParam))
		{
			result = 1; // TRUE
			return true;
		}
		else
		{
			result = 0; // FALSE
			return false;
		}
	}
	virtual void DialogClosed(bool cancel)
	{}
	virtual void UpdateEnabled();
	virtual void UpdateVisible();
};

// Declare some functions which could be useful for UE4-like declarative syntax:
//	Expose(var)			save pointer to control in variable
//	SetParent(parent)	attach control to parent

// Some functions exists in UIElement but overridden here to be able to chain them
// without falling back to UIElement class: UIElement's functions can't return
// 'this' of derived type, so we're redeclaring functions here.

// To add controls to a UIGroup object use the following syntax:
//		NewControl(UIGroup)
//		.Expose(GroupVar)
//		.SomeGroupFunc()
//		[
//			NewControl(ControlType1, args1)
//			.SomeControl1Func1()
//			.SomeControl1Func2()
//			+ NewControl(ControlType2, args2)
//			.Expose(Control2Var)
//			...
//		]
// This code is identical to:
//		GroupVar = new UIGroup();				// make a group
//		GroupVar->SomeGroupFunc();
//		tmpControl1 = new ControlType1(args1);	// make a control 1
//		tmpControl1->SomeControl1Func1();
//		tmpControl1->SomeControl1Func2();
//		ControlVar2 = new ControlType2(args2);	// make a control 2
//		GroupVar->Add(tmpControl1);				// add controls to group
//		GroupVar->Add(ControlVar2);

// Note: DECLARE_UI_CLASS allows access to private members from GetDebugLabel(). Probably
// should change the way how label obtained, like - adding virtual function for debug build etc.

#define DECLARE_UI_CLASS(Class, Base)				\
	typedef Class ThisClass;						\
	typedef Base Super;								\
	friend const char* GetDebugLabel(const UIElement* ctl); \
public:												\
	virtual const char* ClassName() const override { return #Class; } \
	virtual bool IsA(const char* type) const override \
	{												\
		return !strcmp(#Class, type) || Super::IsA(type); \
	}												\
	FORCEINLINE ThisClass& SetRect(int x, int y, int width, int height) \
	{ return (ThisClass&) Super::SetRect(x, y, width, height); } \
	FORCEINLINE ThisClass& SetX(int x)               { Layout.X = x; return *this; } \
	FORCEINLINE ThisClass& SetY(int y)               { Layout.Y = y; return *this; } \
	FORCEINLINE ThisClass& SetWidth(int width)       { Layout.Width = width; return *this; } \
	FORCEINLINE ThisClass& SetHeight(int height)     { Layout.Height = height; return *this; } \
	FORCEINLINE ThisClass& Enable(bool enable)       { return (ThisClass&) Super::Enable(enable); } \
	FORCEINLINE ThisClass& Show(bool visible)        { return (ThisClass&) Super::Show(visible); } \
	template<class T>								\
	FORCEINLINE ThisClass& Expose(T*& var)           { var = this; return *this; } \
	FORCEINLINE ThisClass& SetParent(UIGroup* group) { return (ThisClass&) Super::SetParent(group); } \
	FORCEINLINE ThisClass& SetParent(UIGroup& group) { return (ThisClass&) Super::SetParent(&group); } \
	FORCEINLINE ThisClass& SetMenu(UIMenu* menu)     { return (ThisClass&) Super::SetMenu(menu); } \
private:

// Use this macro to declare a callback type, its variable and SetCallback function.
// Notes:
// - It will automatically add 'ThisClass' pointer as a first parameter of callback function
// - SetCallback function name depends on VarName
#define DECLARE_CALLBACK(VarName, ...)				\
public:												\
	typedef ::Callback<void(ThisClass*, __VA_ARGS__)> VarName##_t; \
	template<typename CB>							\
	FORCEINLINE ThisClass& Set##VarName(CB&& cb)	\
	{												\
		this->VarName = Detail::Forward<CB>(cb); return *this; \
	}												\
protected:											\
	VarName##_t		VarName;						\
private:


// Control creation helper.
// Use this to receive 'Type&' value instead of 'Type*' available with 'new Type' call
#define NewControl(type, ...)	(* new type(__VA_ARGS__))


/*-----------------------------------------------------------------------------
	Controls
-----------------------------------------------------------------------------*/

class UISpacer : public UIElement
{
	DECLARE_UI_CLASS(UISpacer, UIElement);
public:
	UISpacer(int size = 0);

protected:
	virtual void Create(UICreateContext& ctx) override
	{}
};


//?? could extend to show box
class UIHorizontalLine : public UIElement
{
	DECLARE_UI_CLASS(UIHorizontalLine, UIElement);
public:
	UIHorizontalLine();

protected:
	virtual void Create(UICreateContext& ctx) override;
};


class UIVerticalLine : public UIElement
{
	DECLARE_UI_CLASS(UIVerticalLine, UIElement);
public:
	UIVerticalLine();

protected:
	virtual void Create(UICreateContext& ctx) override;
};


class UIBitmap : public UIElement
{
	DECLARE_UI_CLASS(UIBitmap, UIElement);
public:
	UIBitmap();

	// System bitmaps
	enum
	{
		BI_Warning = -1,
		BI_Question = -2,
		BI_Error = -3,
		BI_Information = -4,
	};

	//!! win32 version; check Linux
	// If you want to specify custom resource size, call SetWidth/SetHeight for UIBitmap.
	// Otherwise, image size will be placed into Width and Height fields.
	// WARNING: SetResource... functions will ignore SetWidth/SetHeight AFTER this call.
	UIBitmap& SetResourceIcon(int resId);
	UIBitmap& SetResourceBitmap(int resId);

protected:
	HANDLE		hImage;
	bool		IsIcon;

	virtual void Create(UICreateContext& ctx) override;

	void LoadResourceImage(int id, UINT type, UINT fuLoad);
};


class UILabel : public UIElement
{
	DECLARE_UI_CLASS(UILabel, UIElement);
public:
	UILabel(const char* text, ETextAlign align = TA_Left);
	UILabel& SetAutoSize() { AutoSize = true; return *this; }

	void SetText(const char* text);

protected:
	FString		Label;
	ETextAlign	Align;
	bool		AutoSize;

	virtual void UpdateSize(UIBaseDialog* dialog) override;
	virtual void Create(UICreateContext& ctx) override;
};


class UIHyperLink : public UILabel
{
	DECLARE_UI_CLASS(UIHyperLink, UILabel);
	DECLARE_CALLBACK(Callback);
public:
	UIHyperLink(const char* text, const char* link = NULL /*, ETextAlign align = TA_Left*/); // align is not working
	UIHyperLink& SetAutoSize() { return (ThisClass&)Super::SetAutoSize(); }

protected:
	FString		Link;

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result) override;
};


class UIProgressBar : public UIElement
{
	DECLARE_UI_CLASS(UIProgressBar, UIElement);
public:
	UIProgressBar();

	void SetValue(float value);

protected:
	float		Value;

	virtual void Create(UICreateContext& ctx) override;
};


class UIButton : public UIElement
{
	DECLARE_UI_CLASS(UIButton, UIElement);
	DECLARE_CALLBACK(Callback);
public:
	UIButton(const char* text);

	UIButton& SetOK();
	UIButton& SetCancel();

protected:
	FString		Label;

	virtual void UpdateSize(UIBaseDialog* dialog) override;
	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
};


class UIMenuButton : public UIElement
{
	DECLARE_UI_CLASS(UIMenuButton, UIElement);
	DECLARE_CALLBACK(Callback);
public:
	UIMenuButton(const char* text);

protected:
	FString		Label;

	virtual void UpdateSize(UIBaseDialog* dialog) override;
	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
};


class UICheckbox : public UIElement
{
	DECLARE_UI_CLASS(UICheckbox, UIElement);
	DECLARE_CALLBACK(Callback, bool);
public:
	UICheckbox(const char* text, bool value, bool autoSize = true);
	UICheckbox(const char* text, bool* value, bool autoSize = true);
	UICheckbox& InvertValue() { bInvertValue = true; return *this; }

protected:
	FString		Label;
	bool		bInvertValue;	// when we want inverse meaning
	bool		bValue;			// local bool value
	bool*		pValue;			// pointer to editable value
	bool		AutoSize;

	HWND		ParentWnd;

	virtual void UpdateSize(UIBaseDialog* dialog) override;
	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
};


class UIRadioButton : public UIElement
{
	friend class UIGroup;
	DECLARE_UI_CLASS(UIRadioButton, UIElement);
	DECLARE_CALLBACK(Callback, bool);
public:
	// UIRadioButton with automatic value
	// Value will be assigned in UIGroup::InitializeRadioGroup()
	UIRadioButton(const char* text, bool autoSize = true);
	// UIRadioButton with explicit value
	UIRadioButton(const char* text, int value, bool autoSize = true);

protected:
	FString		Label;
	int			Value;
	bool		Checked;
	bool		AutoSize;

	void ButtonSelected(bool value);
	void SelectButton();

	virtual void UpdateSize(UIBaseDialog* dialog) override;
	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
};


class UITextEdit : public UIElement
{
	DECLARE_UI_CLASS(UITextEdit, UIElement);
	DECLARE_CALLBACK(Callback, const char*);
public:
	UITextEdit(const char* text);
	UITextEdit(FString* text);

	FORCEINLINE UITextEdit& SetMultiline(bool multiline = true)
	{
		IsMultiline = multiline;
		return *this;
	}
	FORCEINLINE UITextEdit& SetReadOnly(bool readOnly = true)
	{
		IsReadOnly = readOnly;
		return *this;
	}
	FORCEINLINE UITextEdit& SetWantFocus(bool focus = true)
	{
		IsWantFocus = focus;
		return *this;
	}

	// This function may be used for creating text output window
	void AppendText(const char* text);

	// Set text in editor field. Passing NULL as text will just refresh value display.
	void SetText(const char* text = NULL);
	const char* GetText();

protected:
	FString		sValue;
	FString*	pValue;

	bool		IsMultiline;
	bool		IsReadOnly;
	bool		IsWantFocus;
	bool		TextDirty;

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
	// request edited text from UI
	void UpdateText();
};


class UICombobox : public UIElement
{
	DECLARE_UI_CLASS(UICombobox, UIElement);
	DECLARE_CALLBACK(Callback, int, const char*);

	struct ComboboxItem
	{
		FString Text;
		int     Value;

		ComboboxItem()
		{}

		ComboboxItem(const char* text, int value)
		: Text(text)
		, Value(value)
		{}
	};

public:
	UICombobox();

	UICombobox(int* value)
	: UICombobox()
	{
		pValue = value;
	}

	template<typename T, typename = typename Detail::TEnableIf<__is_enum(T)>::Type>
	UICombobox(T* value)
	: UICombobox()
	{
		static_assert(sizeof(T) == sizeof(int), "T should be castable to integer");
		pValue = (int*)value;
	}

	UICombobox& AddItem(const char* item, int value = -1);

	template<typename T, typename = typename Detail::TEnableIf<__is_enum(T)>::Type>
	UICombobox& AddItem(const char* item, T value)
	{
		AddItem(item, int(value));
		return *this;
	}
	UICombobox& AddItems(const char** items);
	void RemoveAllItems();

	UICombobox& SelectItem(int index);
	UICombobox& SelectItem(const char* item);

	FORCEINLINE const char* GetItem(int index) const
	{
		return *Items[index].Text;
	}
	FORCEINLINE int GetSelectionIndex() const
	{
		return Selection;
	}
	FORCEINLINE const char* GetSelectionText() const
	{
		return (Selection >= 0) ? *Items[Selection].Text : NULL;
	}

protected:
	TArray<ComboboxItem> Items;
	int			Selection;
	int			Value;
	int*		pValue;

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
};


// This class internally is very similar to UICombobox
//!! add "int* pValue" like for other controls
class UIListbox : public UIElement
{
	DECLARE_UI_CLASS(UIListbox, UIElement);
	DECLARE_CALLBACK(Callback, int, const char*);
	DECLARE_CALLBACK(DblClickCallback, int, const char*);
public:
	UIListbox();

	UIListbox& ReserveItems(int count);

	UIListbox& AddItem(const char* item);
	UIListbox& AddItems(const char** items);
	void RemoveAllItems();

	UIListbox& SelectItem(int index);
	UIListbox& SelectItem(const char* item);

	FORCEINLINE const char* GetItem(int index) const
	{
		return *Items[index];
	}
	FORCEINLINE int GetSelectionIndex() const
	{
		return Value;
	}
	FORCEINLINE const char* GetSelectionText() const
	{
		return (Value >= 0) ? *Items[Value] : NULL;
	}

protected:
	TArray<FString> Items;
	int			Value;

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
};


//?? Probably rename to ListView? But we're supporting only "report" style, so it generally looks
//?? like a Listbox with columns.
// Ways of using UIMulticolumnListbox:
// 1. Normal mode: create, add/remove items, display.
//    Items are stored inside UIMulticolumnListbox. Win32 object contains placeholders - empty items.
// 2. Virtual mode: create, SetVirtualMode, add/remove items, display.
//    Items are stored inside UIMulticolumnListbox. Win32 object contains just number of items. Application
//    works with UIMulticolumnListbox in this mode in exactly the same was as in "normal" mode, however
//    work with control performed much faster.
// 3. "True" virtual mode with callbacks: create, SetVirtualMode, set callbacks. Set number of items, display.
//    Items are stored on the side which created this control. UIMulticolumnListbox and Win32 objects
//    both holds only items count. This is the fastest mode, with lowest possible memory requirement.
class UIMulticolumnListbox : public UIElement
{
	DECLARE_UI_CLASS(UIMulticolumnListbox, UIElement);
	DECLARE_CALLBACK(Callback, int);							// when single item selected (not for multiselect)
	DECLARE_CALLBACK(SelChangedCallback);						// when selection changed
	DECLARE_CALLBACK(DblClickCallback, int);					// when double-clicked an item
	DECLARE_CALLBACK(OnGetItemCount, int&);						// true virtual mode (#3): (int& OutItemCount)
	DECLARE_CALLBACK(OnGetItemText, const char*&, int, int);	// true virtual mode (#3): (char*& OutText, int ItemIndex, int SubItemIndex)
	DECLARE_CALLBACK(OnColumnClick, int);						// column clicked
public:
	static const int MAX_COLUMNS = 16;

	UIMulticolumnListbox(int numColumns);

	UIMulticolumnListbox& AddColumn(const char* title, int width = -1, ETextAlign align = TA_Left);
	UIMulticolumnListbox& AllowMultiselect() { Multiselect = true; return *this; }
	UIMulticolumnListbox& SetVirtualMode() { IsVirtualMode = true; return *this; }
	UIMulticolumnListbox& ShowSortArrow(int columnIndex, bool reverseSort);

	UIMulticolumnListbox& ReserveItems(int count);					// not suitable for "true" virtual mode
	int AddItem(const char* item);									// not suitable for "true" virtual mode
	void AddSubItem(int itemIndex, int column, const char* text);	// not suitable for "true" virtual mode
	void RemoveItem(int itemIndex);									// not suitable for "true" virtual mode
	void RemoveAllItems();

	// select an item; if index==-1, unselect all items; if add==true - extend current selection
	// with this item (only when multiselect is enabled)
	UIMulticolumnListbox& SelectItem(int index, bool add = false);
	UIMulticolumnListbox& SelectItem(const char* item, bool add = false); // not suitable for "true" virtual mode
	// unselect an item
	UIMulticolumnListbox& UnselectItem(int index);
	UIMulticolumnListbox& UnselectItem(const char* item);			// not suitable for "true" virtual mode
	UIMulticolumnListbox& UnselectAllItems();

	int GetItemCount() const;
	FORCEINLINE const char* GetItem(int itemIndex) const { return GetSubItem(itemIndex, 0); }
	const char* GetSubItem(int itemIndex, int column) const;

	FORCEINLINE bool IsTrueVirtualMode() const { return (OnGetItemCount != NULL) && (OnGetItemText != NULL); }

	int GetSelectionIndex(int i = 0) const;							// returns -1 when no items selected
	int GetSelectionCount() const { return SelectedItems.Num(); }	// returns 0 when no items selected

	// UIElement functions
	virtual void UnlockUpdate() override;

protected:
	bool		IsVirtualMode;
	int			NumColumns;
	int			ColumnSizes[MAX_COLUMNS];
	ETextAlign	ColumnAlign[MAX_COLUMNS];
	bool		Multiselect;
	int			SortColumn;
	bool		SortMode;

	TArray<FString> Items;		// first NumColumns items - column headers, next NumColumns - 1st line, 2nd line, ...
	TStaticArray<int, 32> SelectedItems;

	void SetItemSelection(int index, bool select);
	void UpdateListViewHeaderSort();

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result) override;
};


struct TreeViewItem;

class UITreeView : public UIElement
{
	DECLARE_UI_CLASS(UITreeView, UIElement);
	DECLARE_CALLBACK(Callback, const char*);
public:
	UITreeView();
	virtual ~UITreeView() override;

	FORCEINLINE UITreeView& SetRootLabel(const char* root)
	{
		RootLabel = root;
		return *this;
	}

	UITreeView& AddItem(const char* item);
	void RemoveAllItems();

	UITreeView& SelectItem(const char* item);
	const char* GetSelectedItem();

	UITreeView& UseFolderIcons()          { bUseFolderIcons = true; return *this; }
	UITreeView& UseCheckboxes()           { bUseCheckboxes = true; return *this;  }
	UITreeView& SetItemHeight(int value)  { ItemHeight = value; return *this;     }
	UITreeView& HasRootNode(bool value)   { bHasRootNode = value; return *this;   }

	// Checkbox management
	void SetChecked(const char* item, bool checked = true);
	bool GetChecked(const char* item);

	void Expand(const char* item);
	void CollapseAll();
	void ExpandCheckedNodes();

protected:
	TArray<TreeViewItem*> Items;
	FString		RootLabel;
	bool		bHasRootNode;
	TreeViewItem* SelectedItem;
	int			ItemHeight;
	bool		bUseFolderIcons;
	bool		bUseCheckboxes;

	TreeViewItem** HashTable;
	static int GetHash(const char* text);

	FORCEINLINE TreeViewItem* GetRoot() { return Items[0]; }

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam) override;
	void CreateItem(TreeViewItem& item);
	TreeViewItem* FindItem(const char* item);
	TreeViewItem* FindItem(void* hItem);
	void UpdateCheckedStates();
	virtual void DialogClosed(bool cancel) override;
};


/*-----------------------------------------------------------------------------
	UIMenu
-----------------------------------------------------------------------------*/

class UIMenuItem
{
	typedef UIMenuItem ThisClass;
	DECLARE_CALLBACK(Callback);					// this callback is executed for any clicked menu item
	DECLARE_CALLBACK(CheckboxCallback, bool);	// this callback is executed for checkboxes, in addition to main 'Callback'
	DECLARE_CALLBACK(RadioCallback, int);		// this callback is executed for RadioGroup when one of its children clicked
public:
	enum EType
	{
		MI_Text,
		MI_HyperLink,
		MI_Checkbox,
		MI_RadioButton,
		MI_RadioGroup,		// container for radio buttons
		MI_Separator,
		MI_Submenu,
	};

	// Normal menu item
	UIMenuItem(const char* text)
	{
		Init(MI_Text, text);
	}
	// Hyperlink
	UIMenuItem(const char* text, const char* link);
	// Checkbox
	UIMenuItem(const char* text, bool checked);
	UIMenuItem(const char* text, bool* checked);
	UIMenuItem(const char* text, int* value, int mask);
	// RadioGroup
	UIMenuItem(int value);
	UIMenuItem(int* value);
	// RadioButton
	UIMenuItem(const char* text, int value);
	// other types
	UIMenuItem(EType type, const char* text = NULL)
	{
		Init(type, text);
	}

	~UIMenuItem();

	// Making a list of menu items
	friend UIMenuItem& operator+(UIMenuItem& item, UIMenuItem& next);

	UIMenuItem& Expose(UIMenuItem*& var) { var = this; return *this; }

	UIMenuItem& Enable(bool enable);
	UIMenuItem& SetName(const char* newName);

	const char* GetText() const { return *Label; }

	// Replace submenu content. 'other' will be destroyed after this function call.
	void ReplaceWith(UIMenuItem* other);

	// Update checkboxes and radio groups according to attached variables
	void Update();

	// Submenu methods
	void Add(UIMenuItem* item);
	FORCEINLINE void Add(UIMenuItem& item)
	{
		Add(&item);
	}

	// Function for adding children in declarative syntax
	FORCEINLINE UIMenuItem& operator[](UIMenuItem& item)
	{
		Add(item); return *this;
	}

	int GetItemIndex() const;

protected:
	FStaticString<32> Label;
	const char*	Link;			// web page link
	int			Id;
	HMENU		hMenu;			// valid for MI_Submenu

	EType		Type;
	bool		Enabled;

	// Hierarchy
	UIMenuItem*	Parent;
	UIMenuItem*	FirstChild;		// child submenu for MI_Submenu and for MI_RadioGroup
	UIMenuItem*	NextChild;		// next item belongs to the same parent
	// Checkbox and radio group
	bool		bValue;			// local bool value
	int			iValue;			// local int value; used as radio constant for MI_RadioButton
	void*		pValue;			// pointer to editable value (bool for MI_Checkbox and int for MI_RadioGroup)

	void Init(EType type, const char* label);
	void DestroyChildren();
	UIMenu* GetOwner();

	void FillMenuItems(HMENU parentMenu, int& nextId, int& position);

	HMENU GetMenuHandle();
	bool HandleCommand(int id);

	bool GetCheckboxValue() const;
	void SetCheckboxValue(bool newValue);

	int GetMaxItemIdRecursive();
};

class UIMenu : public UIMenuItem // note: we're not using virtual functions in menu classes now
{
	typedef UIMenu ThisClass;
	DECLARE_CALLBACK(BeforePopup, UIElement*);		// this callback is executed when menu is about to pop up
public:
	UIMenu();
	~UIMenu();

	HMENU GetHandle(bool popup, bool forceCreate = false);

	FORCEINLINE void Attach()
	{
		ReferenceCount++;
	}
	void AttachTo(HWND Wnd, bool updateRefCount = true);
	void Detach();

	void Redraw();

	// make HandleCommand public for UIMenu
	FORCEINLINE bool HandleCommand(int id)
	{
		return UIMenuItem::HandleCommand(id);
	}

	FORCEINLINE bool IsMainMenu() const
	{
		return MenuOwner != NULL;
	}

	int GetNextItemId();

	void Popup(UIElement* Owner, int x, int y);

protected:
	int			ReferenceCount;
	HWND		MenuOwner;
	HMENU		MenuObject;		// don't use UIMenuItem's hMenu here, see UIMenu::Create() for details

	void Create(bool popup);
};

FORCEINLINE UIMenuItem& NewMenuItem(const char* label)
{
	return *new UIMenuItem(label);
}

FORCEINLINE UIMenuItem& NewMenuHyperLink(const char* label, const char* link)
{
	return *new UIMenuItem(label, link);
}

FORCEINLINE UIMenuItem& NewSubmenu(const char* label)
{
	return *new UIMenuItem(UIMenuItem::MI_Submenu, label);
}

FORCEINLINE UIMenuItem& NewMenuSeparator()
{
	return *new UIMenuItem(UIMenuItem::MI_Separator);
}

// Create a checkbox

// Checkbox which tracks provided value
FORCEINLINE UIMenuItem& NewMenuCheckbox(const char* label, bool* value)
{
	return *new UIMenuItem(label, value);
}

// Checkbox with not automatic tracking
FORCEINLINE UIMenuItem& NewMenuCheckbox(const char* label, bool value)
{
	return *new UIMenuItem(label, value);
}

FORCEINLINE UIMenuItem& NewMenuCheckbox(const char* label, int* value, int mask)
{
	return *new UIMenuItem(label, value, mask);
}

FORCEINLINE UIMenuItem& NewMenuCheckbox(const char* label, unsigned int* value, int mask)
{
	return *new UIMenuItem(label, (int*)value, mask);
}

// Create a radio group: RadioGroup holds a number of RadioItems

FORCEINLINE UIMenuItem& NewMenuRadioGroup(int* value)
{
	return *new UIMenuItem(value);
}

FORCEINLINE UIMenuItem& NewMenuRadioGroup(int value)
{
	return *new UIMenuItem(value);
}

FORCEINLINE UIMenuItem& NewMenuRadioButton(const char* label, int value)
{
	return *new UIMenuItem(label, value);
}


/*-----------------------------------------------------------------------------
	UI containers
-----------------------------------------------------------------------------*/

// Constants for setting some group properties (Flags)
#define GROUP_NO_BORDER				1
#define GROUP_NO_AUTO_LAYOUT		2
#define GROUP_HORIZONTAL_LAYOUT		4
#define GROUP_HORIZONTAL_SPACING	8	// for GROUP_HORIZONTAL_LAYOUT, evenly distribute controls

#define GROUP_CUSTOM_LAYOUT			(GROUP_NO_BORDER|GROUP_NO_AUTO_LAYOUT)

class UIGroup : public UIElement
{
	friend class UIPageControl;
	friend class UITabControl;
	DECLARE_UI_CLASS(UIGroup, UIElement);
	DECLARE_CALLBACK(RadioCallback, int);
public:
	UIGroup(const char* label, unsigned flags = 0);
	UIGroup(unsigned flags = 0);
	virtual ~UIGroup() override;

	void Add(UIElement* item);
	FORCEINLINE void Add(UIElement& item)
	{
		Add(&item);
	}
	void Remove(UIElement* item);

	// Manually destroy all controls owned by UIGroup
	void ReleaseControls();

	// Functions used with UIRadioButton - UIGroup works like a "radio group"

	FORCEINLINE UIGroup& SetRadioVariable(int* var)
	{
		assert(Wnd == 0);
		pRadioValue = var;
		return *this;
	}
	FORCEINLINE UIGroup& SetRadioValue(int var)
	{
		assert(Wnd == 0);
		*pRadioValue = var;
		return *this;
	}
	FORCEINLINE int GetRadioValue() const { return *pRadioValue; }
	// callback for UIRadioButton
	void RadioButtonClicked(UIRadioButton* sender);

	// Function for adding children in declarative syntax
	FORCEINLINE UIGroup& operator[](UIElement& item)
	{
		Add(item); return *this;
	}

protected:
	FString		Label;
	UIElement*	FirstChild;
	unsigned	Flags;			// combination of GROUP_... flags
	bool		OwnsControls;	// when true, all children will be created under this window

	// support for children UIRadioButton
	int			RadioValue;
	int*		pRadioValue;
	UIRadioButton* SelectedRadioButton;

	virtual void Create(UICreateContext& ctx) override;
	virtual void UpdateSize(UIBaseDialog* dialog) override;
	virtual void UpdateLayout() override;
	virtual void ComputeLayout();

	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result) override;
	virtual void DialogClosed(bool cancel) override;
	virtual void UpdateEnabled() override;
	virtual void UpdateVisible() override;

	void CreateGroupControls(UICreateContext& ctx);
	bool HandleChildMessages(int uMsg, WPARAM wParam, LPARAM lParam, int& result);
	void InitializeRadioGroup();
	void EnableAllControls(bool enabled);
	void ShowAllControls(bool show);

	virtual void AddCustomControls()
	{}
};

// Use DECLARE_UIGROUP_CLASS for declaration of group-derived class
#define DECLARE_UIGROUP_CLASS(Class, Base)			\
	DECLARE_UI_CLASS(Class, Base);					\
public:												\
	FORCEINLINE ThisClass& SetRadioVariable(int* var)  { return (ThisClass&) Super::SetRadioVariable(var); } \
	FORCEINLINE ThisClass& SetRadioValue(int var)      { return (ThisClass&) Super::SetRadioValue(var); } \
	FORCEINLINE ThisClass& operator[](UIElement& item) { return (ThisClass&) Super::operator[](item); } \
private:

// Mix of UICheckbox and UIGroup: this control has a checkbox instead
// of simple title. When it is not checked, all controls are disabled.
class UICheckboxGroup : public UIGroup
{
	DECLARE_UIGROUP_CLASS(UICheckboxGroup, UIGroup);
	DECLARE_CALLBACK(Callback, bool);
public:
	UICheckboxGroup(const char* label, bool value, unsigned flags = 0);
	UICheckboxGroup(const char* label, bool* value, unsigned flags = 0);

	bool IsChecked() const
	{
		return *pValue;
	}

protected:
	bool		bValue;			// local bool value
	bool*		pValue;			// pointer to editable value
	HWND		CheckboxWnd;	// checkbox window
	HWND		ParentWnd;

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result) override;
	virtual void UpdateVisible() override;
	virtual void UpdateLayout() override;
};


// This control has any number of children (like UIGroup), but only one children could be visible at time.
// Index of visible children is specified in ActivePage.
class UIPageControl : public UIGroup
{
	DECLARE_UIGROUP_CLASS(UIPageControl, UIGroup);
public:
	UIPageControl();

	UIPageControl& SetActivePage(int index);
	UIPageControl& SetActivePage(UIElement* child);

protected:
	int			ActivePage;

	virtual void Create(UICreateContext& ctx) override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result) override;
	virtual void UpdateVisible() override;
	virtual void ComputeLayout() override;
	void ComputeLayoutWithBorders(int borderLeft, int borderRight, int borderTop, int borderBottom);
};


class UITabControl : public UIPageControl
{
	DECLARE_UIGROUP_CLASS(UITabControl, UIPageControl);
public:
	UITabControl();

protected:
	virtual LONG_PTR SubclassProc(void* hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	virtual void Create(UICreateContext& ctx) override;
	virtual void UpdateVisible() override;
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam, int& result) override;
	virtual void ComputeLayout() override;
};

class UIBaseDialog : public UIGroup
{
	DECLARE_UIGROUP_CLASS(UIBaseDialog, UIGroup);
public:
	UIBaseDialog();
	virtual ~UIBaseDialog() override;

	FORCEINLINE int GenerateDialogId()
	{
		return NextDialogId++;
	}

	// Show modal dialog. Returns false when dialog window cancelled.
	// TODO: Result value is valid only when HideOnClose() is not active for this dialog.
	FORCEINLINE bool ShowModal(const char* title, int width, int height)
	{
		return ShowDialog(true, title, width, height);
	}
	// Show non-modal window. Function will return immediately, code should periodically execute
	// PumpMessages(), otherwise dialog window will not work.
	FORCEINLINE void ShowDialog(const char* title, int width, int height)
	{
		ShowDialog(false, title, width, height);
	}

	// Control visibility of dialog window
	void ShowDialog();
	void HideDialog();

	FORCEINLINE bool IsDialogOpen() const
	{
		return Wnd != 0;
	}

	// Allow closing of non-modal dialog when Esc key pressed. In order to make
	// it working, we should call PumpMessages() to handle messages.
	// Details: 'Escape' key is processed in PumpMessages().
	FORCEINLINE UIBaseDialog& CloseOnEsc()
	{
		ShouldCloseOnEsc = true;
		return *this;
	}

	// When selected HideOnClose behavior, CloseDialog() will hide it instead of
	// destruction. This will allow to show this dialog again without full recreation.
	FORCEINLINE UIBaseDialog& HideOnClose()
	{
		ShouldHideOnClose = true;
		return *this;
	}

	FORCEINLINE UIBaseDialog& SetIconResId(int iconResId)
	{
		IconResId = iconResId;
		return *this;
	}

	FORCEINLINE UIBaseDialog& SetResizeable()
	{
		bResizeable = true;
		return *this;
	}

	void SetWindowSize(int width, int height);

	// Pumping a message loop, return 'false' when dialog was closed.
	// Should call this function for non-modal dialogs.
	// To popup a non-modal dialog and allow it working, the following construction
	// should be used:
	//	SomeDialogClass dialog;
	//	dialog.ShowDialog("Title", WIDTH, HEIGHT)
	//	while (dialog.PumpMessages()) { Sleep(0); } -- processing messages while window is visible
	//	-- at this point dialog window will be closed
	// If another message loop would be active, everything would work fine except window will
	// not react on 'Escape' key.
	bool PumpMessages();

	// Showing a non-modal dialog with custom message loop as modal window
	void BeginModal();
	void EndModal();

	void CloseDialog(bool cancel = false);

	static void SetMainWindow(HWND window);
	static void SetGlobalIconResId(int iconResId);

protected:
	int			NextDialogId;
	int			IconResId;
	bool		ShouldCloseOnEsc;
	bool		ShouldHideOnClose;
	bool		bResizeable;
	UIBaseDialog* ParentDialog;
	bool		IsDialogConstructed;	// true after InitUI() call
	HWND		DisabledOwnerWnd;		// non-null value when we're showing modal dialog with custom message loop

	int			ClosingDialog;

	bool ShowDialog(bool modal, const char* title, int width, int height);

	void CustomMessageLoop(bool modal);
	void DispatchWindowsMessage(void* msg);

	// dialog procedure
	static INT_PTR CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void WindowsSizeChanged();

	virtual void InitUI()
	{}

	// Virtual method which allows to prevent unwanted closing of dialog window. 'Cancel'
	// parameter indicates if dialog is closed with 'Esc', 'Cancel' button or with 'x'
	// (this is a parameter for CloseDialog() function passed here). This function should
	// return 'false' to deny dialog disappearing.
	virtual bool CanCloseDialog(bool cancel)
	{
		return true;
	}
};


/*-----------------------------------------------------------------------------
	Static stuff
-----------------------------------------------------------------------------*/

void UISetExceptionHandler(void (*Handler)());


#endif // __BASE_DIALOG_H__
