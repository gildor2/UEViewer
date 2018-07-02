// Simple UI library.
// Copyright (C) 2018 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#include "BaseDialog.h"

#if HAS_UI

#include "UIPrivate.h"

#define DEBUG_LAYOUT				0

#if DEBUG_LAYOUT
#define DBG_LAYOUT(...)				appPrintf(__VA_ARGS__)
#else
#define DBG_LAYOUT(...)
#endif


#if DEBUG_LAYOUT

static int DebugLayoutDepth = 0;

static const char* GetDebugLayoutIndent()
{
#define MAX_INDENT 32
	static char indent[MAX_INDENT*2+1];
	if (!indent[0]) memset(indent, ' ', sizeof(indent)-1);
	return &indent[MAX_INDENT*2 - DebugLayoutDepth*2];
}

const char* GetDebugLabel(const UIElement* ctl)
{
#define P(type, field)		\
	if (ctl->IsA(#type)) return *static_cast<const type*>(ctl)->field;

	P(UIButton, Label)
	P(UIMenuButton, Label)
	P(UICheckbox, Label)
	P(UIRadioButton, Label)
	P(UILabel, Label)

	return "";
#undef P
}

#endif // DEBUG_LAYOUT


/*-----------------------------------------------------------------------------
	UILayoutHelper
-----------------------------------------------------------------------------*/

UILayoutHelper::UILayoutHelper(UIGroup* InGroup, int InFlags)
: Flags(InFlags)
, X(InGroup->X)
, Y(InGroup->Y)
, Width(InGroup->Width)
, Height(InGroup->Height)
{
	if (!(Flags & GROUP_NO_BORDER))
	{
		CursorX = GROUP_INDENT;
		CursorY = GROUP_MARGIN_TOP;
	}
	else
	{
		CursorX = CursorY = 0;
	}
}

void UILayoutHelper::AllocateSpace(int& x, int& y, int& w, int& h)
{
	guard(UILayoutHelper::AllocateSpace);

	int baseX = X + CursorX;
	int parentWidth = Width;
	int rightMargin = X + Width;

	if (!(Flags & GROUP_NO_BORDER))
	{
		parentWidth -= GROUP_INDENT * 2;
		rightMargin -= GROUP_INDENT;
	}

	DBG_LAYOUT("%s... AllocSpace (%d %d %d %d) IN: Curs: %d,%d W: %d -- ", GetDebugLayoutIndent(), x, y, w, h, CursorX, CursorY, parentWidth);

	// Compute width
	if (w < 0)
	{
		if (w == -1 && (Flags & GROUP_HORIZONTAL_LAYOUT))
			w = AutoWidth;
		else
			w = int(UIElement::DecodeWidth(w) * parentWidth);
	}

	// Compute height
	if (h < 0 && Height > 0)
	{
		h = int(UIElement::DecodeWidth(h) * Height);
	}
	assert(h >= 0);

	// Compute X
	if (x == -1)
	{
		// Automatic X
		x = baseX;
		//!! UseHorizontalLayout()
		if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == GROUP_HORIZONTAL_LAYOUT)
			CursorX += w;
	}
	else if (x < 0)
	{
		// X is fractional value
		x = baseX + int(UIElement::DecodeWidth(x) * parentWidth);	// left border of parent control, 'x' is relative value
	}
	else
	{
		// X is absolute value
		x = baseX + x;									// treat 'x' as relative value
	}

	// Clamp width
	if (x + w > rightMargin)
		w = rightMargin - x;

	// Compute Y
	if (y < 0)
	{
		// Automatic Y
		y = Y + CursorY;								// next 'y' value
		//!! UseVerticalLayout()
		if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == 0)
			CursorY += h;
	}
	else
	{
		// Y is absolute value, wo don't support fractional values yer
		y = Y + CursorY + y;							// treat 'y' as relative value
		// don't change 'Height'
	}

//	h = unchanged; (i.e. do not clamp height)

	DBG_LAYOUT("OUT: (%d %d %d %d) Curs: %d,%d\n", x, y, w, h, CursorX, CursorY);

	unguard;
}

void UILayoutHelper::AddVerticalSpace(int size)
{
	if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == 0)
		CursorY += (size >= 0 ? size : VERTICAL_SPACING);
}

void UILayoutHelper::AddHorizontalSpace(int size)
{
	if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == GROUP_HORIZONTAL_LAYOUT)
		CursorX += (size >= 0 ? size : HORIZONTAL_SPACING);
}


/*-----------------------------------------------------------------------------
	UIGroup layout
-----------------------------------------------------------------------------*/

void UIGroup::UpdateLayout(UILayoutHelper* parentLayout)
{
	guard(UIGroup::UpdateLayout);

	if (parentLayout)
	{
//		if (!(Flags & GROUP_NO_BORDER)) -- makes control layout looking awful
		parentLayout->AddVertSpace();
		// request x, y and width; height is not available yet
		int saveHeight = Height;
		Height = 0;
		// Add control with zero height. This will compute all values
		// of group's position, but won't reserve vertical space.
		parentLayout->AddControl(this);
		Height = saveHeight;
	}

	UILayoutHelper layout(this, Flags);
	if (!parentLayout)
	{
		// this is a dialog, add some space on top for better layout
		layout.AddVertSpace();
	}

#if DEBUG_LAYOUT
	DBG_LAYOUT("%sgroup \"%s\" cursor: %d %d\n", GetDebugLayoutIndent(), *Label, layout.CursorX, layout.CursorY);
	DebugLayoutDepth++;
#endif

	// determine default width of control in horizontal layout; this value will be used for
	// all controls which width was not specified (for Width==-1)
	layout.AutoWidth = 0;
	int horizontalSpacing = 0;
	if (layout.UseHorizontalLayout())
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
			layout.AutoWidth = (parentWidth - totalWidth) / numAutoWidthControls;
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

	int maxControlY = Y + Height;

	for (UIElement* control = FirstChild; control; control = control->NextChild)
	{
		// evenly space controls for horizontal layout, when requested
		if (horizontalSpacing > 0 && control != FirstChild)
			layout.AddHorzSpace(horizontalSpacing);

		DBG_LAYOUT("%s%s \"%s\": x=%d y=%d w=%d h=%d\n", GetDebugLayoutIndent(),
			control->ClassName(), GetDebugLabel(control), control->X, control->Y, control->Width, control->Height);

		control->UpdateLayout(&layout);

		int bottom = control->Y + control->Height;
		if (bottom > maxControlY)
			maxControlY = bottom;
	}

	Height = max(Height, maxControlY - Y);

	if (!(Flags & GROUP_NO_BORDER))
		Height += GROUP_MARGIN_BOTTOM;

	if (parentLayout && parentLayout->UseVerticalLayout())
	{
		// We've reserved no vertical space before (when called parentLayout->AddControl()), so
		// we should fix that now.
		parentLayout->AddVertSpace(Height);
	}
#if DEBUG_LAYOUT
	DebugLayoutDepth--;
#endif

	if (parentLayout)
		parentLayout->AddVertSpace();

	unguard;
}


#endif // HAS_UI
