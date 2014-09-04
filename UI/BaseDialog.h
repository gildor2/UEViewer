#ifndef __BASE_DIALOG_H__
#define __BASE_DIALOG_H__

/*!! TODO:
- TArray<FString>
  - make StringArray class - TArray<const char*>
    - will need a destructor
  - possible improvements:
    - add Reserve() for StringArray
*/

#include "Core.h"

#if HAS_UI	// defined in Build.h, included from Core.h

#include "UnCore.h"					// for TArray and FString

#include "callback.hpp"

// forwards
class UIBaseDialog;


enum ETextAlign
{
	TA_Left,
	TA_Right,
	TA_Center,
};


/*-----------------------------------------------------------------------------
	UIElement
-----------------------------------------------------------------------------*/

class UIElement
{
	friend class UIGroup;
public:
	UIElement();
	virtual ~UIElement();

	virtual const char* ClassName() const;

	UIElement& Enable(bool enable);
	FORCEINLINE bool IsEnabled() const   { return Enabled; }

	UIElement& Show(bool visible);
	FORCEINLINE bool IsVisible() const   { return Visible; }

	UIElement& SetParent(UIGroup* group);
	FORCEINLINE HWND GetWnd() const      { return Wnd; }

	// Layout functions

	UIElement& SetRect(int x, int y, int width, int height);
	FORCEINLINE int GetWidth() const     { return Width; }
	FORCEINLINE int GetHeight() const    { return Height; }

	static FORCEINLINE int EncodeWidth(float w)
	{
		w = bound(w, 0, 1);
		int iw = w * 255.0f;			// float -> int
		return 0xFFFF0000 | iw;
	}

	static FORCEINLINE float DecodeWidth(int w)
	{
#if MAX_DEBUG
		assert((w & 0xFFFFFF00) == 0xFFFF0000 || w == -1);
#endif
		return (w & 0xFF) / 255.0f;		// w=-1 -> 1.0f
	}

	void MeasureTextSize(const char* text, int* width, int* height = NULL, HWND wnd = 0);

	friend UIElement& operator+(UIElement& elem, UIElement& next);

protected:
	int			X;
	int			Y;
	int			Width;
	int			Height;
	bool		IsGroup:1;
	bool		IsRadioButton:1;
	bool		Enabled;
	bool		Visible;
	UIGroup*	Parent;
	UIElement*	NextChild;

	HWND		Wnd;
	int			Id;

	HWND Window(const char* className, const char* text, DWORD style, DWORD exstyle, UIBaseDialog* dialog,
		int id = -1, int x = -1, int y = -1, int w = -1, int h = -1);

	virtual void Create(UIBaseDialog* dialog) = 0;
	virtual void UpdateSize(UIBaseDialog* dialog)
	{}
	// Process WM_COMMAND message. 'id' is useless in most cases, useful for
	// groups only.
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam)
	{
		return false;
	}
	virtual void DialogClosed(bool cancel)
	{}
	virtual void UpdateEnabled();
	virtual void UpdateVisible();
};

// Declare some functions which could be useful for UE4-like declarative syntax:
//	Expose(var)			save pointer to control in variable
//	SetParent(parent)	attach control to parent

// Some functions exists in UIElement but overrided here to be able to chain them
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

#define DECLARE_UI_CLASS(Class, Base)				\
	typedef Class ThisClass;						\
	typedef Base Super;								\
public:												\
	virtual const char* ClassName() const { return #Class; } \
	FORCEINLINE ThisClass& SetRect(int x, int y, int width, int height) \
	{ return (ThisClass&) Super::SetRect(x, y, width, height); } \
	FORCEINLINE ThisClass& SetX(int x)               { X = x; return *this; } \
	FORCEINLINE ThisClass& SetY(int y)               { Y = y; return *this; } \
	FORCEINLINE ThisClass& SetWidth(int width)       { Width = width; return *this; } \
	FORCEINLINE ThisClass& SetHeight(int height)     { Height = height; return *this; } \
	FORCEINLINE ThisClass& Enable(bool enable)       { return (ThisClass&) Super::Enable(enable); } \
	FORCEINLINE ThisClass& Show(bool visible)        { return (ThisClass&) Super::Show(visible); } \
	FORCEINLINE ThisClass& Expose(ThisClass*& var)   { var = this; return *this; } \
	FORCEINLINE ThisClass& SetParent(UIGroup* group) { return (ThisClass&) Super::SetParent(group); } \
	FORCEINLINE ThisClass& SetParent(UIGroup& group) { return (ThisClass&) Super::SetParent(&group); } \
private:

// Use this macro to declare a callback type, its variable and SetCallback function.
// Notes:
// - It will automatically add 'ThisClass' pointer as a first parameter of callback function
// - SetCallback function name depends on VarName
#define DECLARE_CALLBACK(VarName, ...)				\
public:												\
	typedef util::Callback<void (ThisClass*, __VA_ARGS__)> VarName##_t; \
	FORCEINLINE ThisClass& Set##VarName(VarName##_t& cb) \
	{												\
		VarName = cb; return *this;					\
	}												\
protected:											\
	VarName##_t		VarName;						\
private:


// Control creation helpers.
// Use these to receive 'Type&' value instead of 'Type*' available with 'new Type' call

#define NewControl(type, ...)	_NewControl<type>(__VA_ARGS__)

// C++11 offers Variadic Templates (http://en.wikipedia.org/wiki/Variadic_template), unfortunately
// Visual Studio prior to 2013 doesn't support them. GCC 4.3 has variadic template support.
template<class T>
FORCEINLINE T& _NewControl()
{
	return *new T();
}

template<class T, class P1>
FORCEINLINE T& _NewControl(P1 p1)
{
	return *new T(p1);
}

template<class T, class P1, class P2>
FORCEINLINE T& _NewControl(P1 p1, P2 p2)
{
	return *new T(p1, p2);
}

template<class T, class P1, class P2, class P3>
FORCEINLINE T& _NewControl(P1 p1, P2 p2, P3 p3)
{
	return *new T(p1, p2, p3);
}

template<class T, class P1, class P2, class P3, class P4>
FORCEINLINE T& _NewControl(P1 p1, P2 p2, P3 p3, P4 p4)
{
	return *new T(p1, p2, p3, p4);
}


/*-----------------------------------------------------------------------------
	Controls
-----------------------------------------------------------------------------*/

class UISpacer : public UIElement
{
	DECLARE_UI_CLASS(UISpacer, UIElement);
public:
	UISpacer(int size = 0);

protected:
	virtual void Create(UIBaseDialog* dialog);
};


class UILabel : public UIElement
{
	DECLARE_UI_CLASS(UILabel, UIElement);
public:
	UILabel(const char* text, ETextAlign align = TA_Left);

protected:
	FString		Label;
	ETextAlign	Align;

	virtual void Create(UIBaseDialog* dialog);
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

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UICheckbox : public UIElement
{
	DECLARE_UI_CLASS(UICheckbox, UIElement);
	DECLARE_CALLBACK(Callback, bool);
public:
	UICheckbox(const char* text, bool value);
	UICheckbox(const char* text, bool* value);

protected:
	FString		Label;
	bool		bValue;			// local bool value
	bool*		pValue;			// pointer to editable value

	HWND		DlgWnd;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
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

	virtual void UpdateSize(UIBaseDialog* dialog);
	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UITextEdit : public UIElement
{
	DECLARE_UI_CLASS(UITextEdit, UIElement);
	DECLARE_CALLBACK(Callback, const char*);
public:
	UITextEdit(const char* text);
	UITextEdit(FString* text);

	void SetText(const char* text = NULL);
	const char* GetText();

protected:
	FString		sValue;
	FString*	pValue;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
	// request edited text from UI
	void UpdateText();
	// request edited text only when dialog is closed
	virtual void DialogClosed(bool cancel);
};


//!! add "int* pValue" like for other controls
class UICombobox : public UIElement
{
	DECLARE_UI_CLASS(UICombobox, UIElement);
	DECLARE_CALLBACK(Callback, int, const char*);
public:
	UICombobox();

	UICombobox& AddItem(const char* item);
	UICombobox& AddItems(const char** items);
	void RemoveAllItems();

	UICombobox& SelectItem(int index);
	UICombobox& SelectItem(const char* item);

	FORCEINLINE const char* GetItem(int index) const
	{
		return Items[index];
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

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
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

	UIListbox& AddItem(const char* item);
	UIListbox& AddItems(const char** items);
	void RemoveAllItems();

	UIListbox& SelectItem(int index);
	UIListbox& SelectItem(const char* item);

	FORCEINLINE const char* GetItem(int index) const
	{
		return Items[index];
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

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UIMulticolumnListbox : public UIElement
{
	DECLARE_UI_CLASS(UIMulticolumnListbox, UIElement);
	DECLARE_CALLBACK(Callback, int);
	DECLARE_CALLBACK(DblClickCallback, int);
public:
	static const int MAX_COLUMNS = 16;

	UIMulticolumnListbox(int numColumns);

	UIMulticolumnListbox& AddColumn(const char* title, int width = -1);

	int AddItem(const char* item);
	void AddSubItem(int itemIndex, int column, const char* text);
	void RemoveAllItems();

	UIMulticolumnListbox& SelectItem(int index);
	UIMulticolumnListbox& SelectItem(const char* item);

	FORCEINLINE const char* GetItem(int itemIndex) const { return GetSumItem(itemIndex, 0); }
	const char* GetSumItem(int itemIndex, int column) const;

	FORCEINLINE int GetSelectionIndex() const
	{
		return Value;
	}

protected:
	int			NumColumns;
	TArray<FString> Items;		// first NumColumns items - column headers, next NumColumns - 1st line, 2nd line, ...
	int			ColumnSizes[MAX_COLUMNS];
	int			Value;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


struct TreeViewItem;

class UITreeView : public UIElement
{
	DECLARE_UI_CLASS(UITreeView, UIElement);
	DECLARE_CALLBACK(Callback, const char*);
public:
	UITreeView();
	virtual ~UITreeView();

	FORCEINLINE UITreeView& SetRootLabel(const char* root)
	{
		RootLabel = root;
		return *this;
	}

	UITreeView& AddItem(const char* item);
	void RemoveAllItems();

	UITreeView& SelectItem(const char* item);

protected:
	TArray<TreeViewItem*> Items;
	FString		RootLabel;
	TreeViewItem* SelectedItem;

	FORCEINLINE TreeViewItem* GetRoot() { return Items[0]; }

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
	void CreateItem(TreeViewItem& item);
};


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
	DECLARE_UI_CLASS(UIGroup, UIElement);
	DECLARE_CALLBACK(RadioCallback, int);
public:
	UIGroup(const char* label, unsigned flags = 0);
	UIGroup(unsigned flags = 0);
	virtual ~UIGroup();

	void Add(UIElement* item);
	FORCEINLINE void Add(UIElement& item)
	{
		Add(&item);
	}
	void Remove(UIElement* item);
	void ReleaseControls();

	void AllocateUISpace(int& x, int& y, int& w, int& h);
	void AddVerticalSpace(int size = -1);
	void AddHorizontalSpace(int size = -1);

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

	FORCEINLINE bool UseAutomaticLayout() const
	{
		return (Flags & GROUP_NO_AUTO_LAYOUT) == 0;
	}

	FORCEINLINE bool UseVerticalLayout() const
	{
		return (Flags & GROUP_HORIZONTAL_LAYOUT) == 0;
	}

	FORCEINLINE bool UseHorizontalLayout() const
	{
		return (Flags & GROUP_HORIZONTAL_LAYOUT) != 0;
	}

protected:
	FString		Label;
	UIElement*	FirstChild;
	unsigned	Flags;			// combination of GROUP_... flags

	// transient variables used by layout code
	int			AutoWidth;		// used with GROUP_HORIZONTAL_LAYOUT, for controls with width set to -1
	int			CursorX;		// where to place next control in horizontal layout
	int			CursorY;		// ... for vertical layout

	// support for children UIRadioButton
	int			RadioValue;
	int*		pRadioValue;
	UIRadioButton* SelectedRadioButton;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
	virtual void DialogClosed(bool cancel);
	virtual void UpdateEnabled();
	virtual void UpdateVisible();

	void CreateGroupControls(UIBaseDialog* dialog);
	void InitializeRadioGroup();
	void EnableAllControls(bool enabled);
	void ShowAllControls(bool show);

	virtual void AddCustomControls()
	{}
};


// Mix of UICheckbox and UIGroup: this control has a checkbox instead
// of simple title. When it is not checked, all controls are disabled.
class UICheckboxGroup : public UIGroup
{
	DECLARE_UI_CLASS(UICheckboxGroup, UIGroup);
	DECLARE_CALLBACK(Callback, bool);
public:
	UICheckboxGroup(const char* label, bool value, unsigned flags = 0);

	bool IsChecked() const
	{
		return Value;
	}

protected:
	FString		Label;			// overrides Label of parent
	bool		Value;
	HWND		CheckboxWnd;	// checkbox window
	HWND		DlgWnd;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UIBaseDialog : public UIGroup
{
	DECLARE_UI_CLASS(UIBaseDialog, UIElement);
public:
	UIBaseDialog();
	virtual ~UIBaseDialog();

	virtual void InitUI()
	{}

	FORCEINLINE int GenerateDialogId()
	{
		return NextDialogId++;
	}

	bool ShowDialog(const char* title, int width, int height);
	void CloseDialog(bool cancel = false);

	//!! UIBaseDialog& AddOkCancelButtons(); - should be done in InitUI as last operation

protected:
	int			NextDialogId;

	// dialog procedure
	static INT_PTR CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};


#endif // HAS_UI

#endif // __BASE_DIALOG_H__
