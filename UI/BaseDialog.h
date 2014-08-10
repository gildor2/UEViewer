#ifndef __BASE_DIALOG_H__
#define __BASE_DIALOG_H__


#include "Core.h"

//!! make this header windows.h independent
#include "UnCore.h"					// for TArray


#if HAS_UI

// forwards
class UIBaseDialog;


class UIElement
{
public:
	UIElement();
	virtual ~UIElement();

	virtual void Create(UIBaseDialog* dialog) = 0;

protected:
	int			X;
	int			Y;
	int			Width;
	int			Height;
	bool		IsGroup;
	UIElement*	Parent;

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
	UIGroup();
	virtual ~UIGroup();

	void Add(UIElement* item);
	void ReleaseControls();

protected:
	TArray<UIElement*> Children;
};


class UIBaseDialog : public UIGroup
{
public:
	UIBaseDialog();
	virtual ~UIBaseDialog();

	bool ShowDialog(const char* title, int width, int height);

	virtual void Create(UIBaseDialog* dialog)
	{
		//!! TODO
	}

protected:
	// dialog procedure
	static INT_PTR CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};


#endif // HAS_UI

#endif // __BASE_DIALOG_H__
