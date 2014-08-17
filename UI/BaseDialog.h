#ifndef __BASE_DIALOG_H__
#define __BASE_DIALOG_H__


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


class UIElement
{
	friend class UIGroup;
public:
	UIElement();
	virtual ~UIElement();

	void Enable(bool enable);
	FORCEINLINE bool IsEnabled() const       { return Enabled;  }

	// Layout functions

	void SetRect(int x, int y, int width, int height);
	FORCEINLINE void SetWidth(int width)     { Width = width;   }
	FORCEINLINE void SetHeight(int height)   { Height = height; }
	FORCEINLINE int  GetWidth() const        { return Width;    }
	FORCEINLINE int  GetHeight() const       { return Height;   }

	FORCEINLINE HWND GetWnd() const          { return Wnd;      }

	static FORCEINLINE int EncodeWidth(float w)
	{
		if (w > 1) w = 1;
		if (w < 0) w = 0;
		int iw = w * 255.0f;
		return 0xFFFF0000 | iw;
	}

	static FORCEINLINE int DecodeWidth(int w)
	{
		assert((w & 0xFFFFFF00) == 0xFFFF0000 || w == -1);
		return (w & 0xFF) / 255.0f;      // w=-1 -> 1.0f
	}

	void MeasureTextSize(const char* text, int* width, int* height = NULL, HWND wnd = 0);

protected:
	int			X;
	int			Y;
	int			Width;
	int			Height;
	bool		IsGroup;
	bool		Enabled;
	UIGroup*	Parent;

	HWND		Wnd;
	int			Id;

	HWND Window(const char* className, const char* text, DWORD style, DWORD exstyle, UIBaseDialog* dialog,
		int id = -1, int x = -1, int y = -1, int w = -1, int h = -1);

	virtual void Create(UIBaseDialog* dialog) = 0;
	// Process WM_COMMAND message. 'id' is useless in most cases, useful for
	// groups only.
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam)
	{
		return false;
	}
	virtual void DialogClosed(bool cancel)
	{}
	virtual void UpdateEnabled();
};


class UILabel : public UIElement
{
public:
	UILabel(const char* text, ETextAlign align = TA_Left);

protected:
	FString		Label;
	ETextAlign	Align;

	virtual void Create(UIBaseDialog* dialog);
};


class UIButton : public UIElement
{
public:
	UIButton(const char* text);

	typedef util::Callback<void (UIButton*)> Callback_t;

	FORCEINLINE void SetCallback(const Callback_t& cb)
	{
		Callback = cb;
	}

protected:
	FString		Label;
	Callback_t	Callback;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UICheckbox : public UIElement
{
public:
	UICheckbox(const char* text, bool value);
	UICheckbox(const char* text, bool* value);

	typedef util::Callback<void (UICheckbox*, bool)> Callback_t;

	FORCEINLINE void SetCallback(const Callback_t& cb)
	{
		Callback = cb;
	}

protected:
	FString		Label;
	Callback_t	Callback;
	bool		bValue;			// local bool value
	bool*		pValue;			// pointer to editable value

	HWND		DlgWnd;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UITextEdit : public UIElement
{
public:
	UITextEdit(const char* text);
	UITextEdit(FString* text);

	typedef util::Callback<void (UITextEdit*, const char* text)> Callback_t;

	FORCEINLINE void SetCallback(const Callback_t& cb)
	{
		Callback = cb;
	}

protected:
	Callback_t	Callback;
	FString		sValue;
	FString*	pValue;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
	// request edited text only when dialog is closed
	virtual void DialogClosed(bool cancel);
};


class UICombobox : public UIElement
{
public:
	UICombobox();

	void AddItem(const char* item);
	void AddItems(const char** items);
	void RemoveAllItems();

	typedef util::Callback<void (UICombobox*, int, const char*)> Callback_t;

	FORCEINLINE void SetCallback(const Callback_t& cb)
	{
		Callback = cb;
	}

	void SelectItem(int index);
	void SelectItem(const char* item);

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
	Callback_t	Callback;
	TArray<FString> Items;
	int			Value;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UIGroup : public UIElement
{
public:
	UIGroup(const char* label = NULL);
	virtual ~UIGroup();

	void Add(UIElement* item);
	void Remove(UIElement* item);
	void ReleaseControls();

	void AllocateUISpace(int& x, int& y, int& w, int& h);
	void AddVerticalSpace(int height = -1);

	virtual void AddCustomControls()
	{}

	void EnableAllControls(bool enabled);

protected:
	FString		Label;
	TArray<UIElement*> Children;
	int			TopBorder;
	bool		NoBorder;
	bool		NoAutoLayout;

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
	virtual void DialogClosed(bool cancel);
	virtual void UpdateEnabled();
	void CreateGroupControls(UIBaseDialog* dialog);
};


// Mix of UICheckbox and UIGroup: this control has a checkbox instead
// of simple title. When it is not checked, all controls are disabled.
class UICheckboxGroup : public UIGroup
{
public:
	UICheckboxGroup(const char* label, bool value);

	typedef util::Callback<void (UICheckboxGroup*, bool)> Callback_t;

	FORCEINLINE void SetCallback(const Callback_t& cb)
	{
		Callback = cb;
	}

protected:
	FString		Label;			// overrides Label of parent
	bool		Value;
	HWND		CheckboxWnd;	// checkbox window
	HWND		DlgWnd;
	Callback_t	Callback;		// called when checkbox clicked

	virtual void Create(UIBaseDialog* dialog);
	virtual bool HandleCommand(int id, int cmd, LPARAM lParam);
};


class UIBaseDialog : public UIGroup
{
public:
	UIBaseDialog();
	virtual ~UIBaseDialog();

	virtual void InitUI()
	{}

	int GenerateDialogId()
	{
		return NextDialogId++;
	}

	bool ShowDialog(const char* title, int width, int height);
	void CloseDialog(bool cancel = false);

protected:
	int			NextDialogId;

	// dialog procedure
	static INT_PTR CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};


#endif // HAS_UI

#endif // __BASE_DIALOG_H__
