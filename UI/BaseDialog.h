#ifndef __BASE_DIALOG_H__
#define __BASE_DIALOG_H__


#include "Core.h"

//!! make this header windows.h independent
#include "UnCore.h"					// for TArray and FString


#if HAS_UI

// forwards
class UIBaseDialog;


class UIElement
{
	friend class UIGroup;
public:
	UIElement();
	virtual ~UIElement();

	virtual void Create(UIBaseDialog* dialog) = 0;

	void SetRect(int x, int y, int width, int height);
	FORCEINLINE void SetWidth(int width)     { Width = width;   }
	FORCEINLINE void SetHeight(int height)   { Height = height; }
	FORCEINLINE int  GetWidth() const        { return Width;    }
	FORCEINLINE int  GetHeight() const       { return Height;   }

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

protected:
	int			X;
	int			Y;
	int			Width;
	int			Height;
	bool		IsGroup;
	UIGroup*	Parent;

	HWND		Wnd;
};


class UILabel : public UIElement
{
};


class UIButton : public UIElement
{
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

	virtual void Create(UIBaseDialog* dialog);

protected:
	FString		Label;
	TArray<UIElement*> Children;
	int			TopBorder;
	bool		NoBorder;
	bool		NoAutoLayout;

	void CreateGroupControls(UIBaseDialog* dialog);
};


class UIBaseDialog : public UIGroup
{
public:
	UIBaseDialog();
	virtual ~UIBaseDialog();

	virtual void InitUI()
	{}

	bool ShowDialog(const char* title, int width, int height);

protected:
	// dialog procedure
	static INT_PTR CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};


#endif // HAS_UI

#endif // __BASE_DIALOG_H__
