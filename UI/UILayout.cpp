// Simple UI library.
// Copyright (C) 2018 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#include "BaseDialog.h"

#if HAS_UI

#include "UIPrivate.h"

//#define DEBUG_LAYOUT				1

#if DEBUG_LAYOUT
#define DBG_LAYOUT(msg,...)				appPrintf("%s" msg "\n", GetDebugLayoutIndent(), __VA_ARGS__)
#else
#define DBG_LAYOUT(...)
#endif


#if DEBUG_LAYOUT

static int DebugLayoutDepth = 0;

static const char* GetDebugLayoutIndent()
{
#define MAX_INDENT 32
	static char indent[MAX_INDENT*4+1];
	if (!indent[0]) memset(indent, ' ', sizeof(indent)-1);
	return &indent[MAX_INDENT*4 - DebugLayoutDepth*4];
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
, ParentRect(InGroup->Rect)
{
	// Fix for wrong height. TODO: should reimplement layout code to work without it!
	ParentRect.Height = InGroup->Layout.Height;

	if (!(Flags & GROUP_NO_BORDER))
	{
		CursorX = GROUP_INDENT;
		CursorY = GROUP_MARGIN_TOP;
	}
	else
	{
		CursorX = CursorY = 0;
	}
	DBG_LAYOUT("%sNewLayout (%s): x=%d y=%d w=%d h=%d\n", GetDebugLayoutIndent(), GetDebugLabel(InGroup),
		ParentRect.X, ParentRect.Y, ParentRect.Width, ParentRect.Height);
}

void UILayoutHelper::AllocateSpace(const UIRect& src, UIRect& dst)
{
	guard(UILayoutHelper::AllocateSpace);

	int baseX = ParentRect.X + CursorX;
	int baseY = ParentRect.Y + CursorY;
	int parentWidth = ParentRect.Width;
	int rightMargin = ParentRect.X + ParentRect.Width;

	if (!(Flags & GROUP_NO_BORDER))
	{
		parentWidth -= GROUP_INDENT * 2;
		rightMargin -= GROUP_INDENT;
	}

	int x = src.X;
	int y = src.Y;
	int w = src.Width;
	int h = src.Height;

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
	if (h < 0 && ParentRect.Height > 0)
	{
		h = int(UIElement::DecodeWidth(h) * ParentRect.Height);
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
		x = baseX + x;					// treat 'x' as relative value
	}

	// Clamp width
	if (x + w > rightMargin)
		w = rightMargin - x;

	// Compute Y
	if (y == -1)
	{
		// Automatic Y
		y = baseY;
		//!! UseVerticalLayout()
		if ((Flags & (GROUP_NO_AUTO_LAYOUT|GROUP_HORIZONTAL_LAYOUT)) == 0)
			CursorY += h;
	}
	else
	{
		// Y is absolute value, wo don't support fractional values yer
		y = baseY + y;					// treat 'y' as relative value
		// don't change 'Height'
	}

//	h = unchanged; (i.e. do not clamp height)

	DBG_LAYOUT("OUT: (%d %d %d %d) Curs: %d,%d\n", x, y, w, h, CursorX, CursorY);

	dst.X = x;
	dst.Y = y;
	dst.Width = w;
	dst.Height = h;

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
		int saveHeight = Layout.Height;
		Layout.Height = 0;
		// Add control with zero height. This will compute all values
		// of group's position, but won't reserve vertical space.
		parentLayout->AddControl(this);
		Layout.Height = saveHeight;
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
		int parentWidth = Rect.Width;		// width of space for children controls
		if (!(Flags & GROUP_NO_BORDER))
			parentWidth -= GROUP_INDENT * 2;

		for (UIElement* control = FirstChild; control; control = control->NextChild)
		{
			numControls++;
			// get width of control
			int w = control->Layout.Width;
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

	int maxControlY = Rect.Y + Rect.Height;

	for (UIElement* control = FirstChild; control; control = control->NextChild)
	{
		// evenly space controls for horizontal layout, when requested
		if (horizontalSpacing > 0 && control != FirstChild)
			layout.AddHorzSpace(horizontalSpacing);

		DBG_LAYOUT("%s%s \"%s\": x=%d y=%d w=%d h=%d\n", GetDebugLayoutIndent(),
			control->ClassName(), GetDebugLabel(control), control->Layout.X, control->Layout.Y, control->Layout.Width, control->Layout.Height);

		control->UpdateLayout(&layout);

		int bottom = control->Rect.Y + control->Rect.Height;
		if (bottom > maxControlY)
			maxControlY = bottom;
	}

	Rect.Height = max(Rect.Height, maxControlY - Rect.Y);

	if (!(Flags & GROUP_NO_BORDER))
		Rect.Height += GROUP_MARGIN_BOTTOM;

	if (parentLayout && parentLayout->UseVerticalLayout())
	{
		// We've reserved no vertical space before (when called parentLayout->AddControl()), so
		// we should fix that now.
		parentLayout->AddVertSpace(Rect.Height);
	}
#if DEBUG_LAYOUT
	DebugLayoutDepth--;
#endif

	if (parentLayout)
		parentLayout->AddVertSpace();

	unguard;
}

int UIElement::ComputeWidth() const
{
	return Layout.Width;
}

int UIElement::ComputeHeight() const
{
	return Layout.Height;
}

void UIGroup::ComputeLayout()
{
	guard(UIGroup::ComputeLayout);

#if DEBUG_LAYOUT
	DBG_LAYOUT("group \"%s\" %s",
		(Flags & GROUP_NO_BORDER) ? "(no border)" : *Label,
		(Flags & GROUP_HORIZONTAL_LAYOUT) ? "[horz]" : "[vert]"
	);
	DBG_LAYOUT("{");
	DebugLayoutDepth++;
	DBG_LAYOUT("this.Layout: x(%d) y(%d) w(%d) h(%d) - Rect: x(%d) y(%d) w(%d) h(%d)",
		Layout.X, Layout.Y, Layout.Width, Layout.Height,
		Rect.X, Rect.Y, Rect.Width, Rect.Height);
#endif

	int groupBorderTop = 0;
	int groupBorderLeft = 0;
	int groupBorderWidth = 0;
	int groupBorderHeight = 0;
	if (!(Flags & GROUP_NO_BORDER))
	{
		groupBorderTop = GROUP_MARGIN_TOP;
		groupBorderLeft = GROUP_INDENT;
		groupBorderWidth = GROUP_INDENT * 2;
		groupBorderHeight = GROUP_MARGIN_TOP + GROUP_MARGIN_BOTTOM;
	}

	// Determine layout type
	bool bHorizontalLayout = (Flags & GROUP_HORIZONTAL_LAYOUT) != 0;
	bool bVerticalLayout = !bHorizontalLayout;
	if (Flags & GROUP_NO_AUTO_LAYOUT)
	{
		bHorizontalLayout = bVerticalLayout = false;
	}

	// Common layout code
	int TotalWidth = 0, TotalHeight = 0;
	int MaxWidth = 0, MaxHeight = 0;
	float TotalFracWidth = 0.0f, TotalFracHeight = 0.0f;
	int MarginsSize = 0;

	DBG_LAYOUT(">>> prepare children");
	for (UIElement* child = FirstChild; child; child = child->NextChild)
	{
		child->Rect = child->Layout;

		int w = child->Rect.Width;
		int h = child->Rect.Height;

		if (child->IsGroup)
		{
			bool bComputeWidth = (Rect.Width < 0 && w < 0);
			bool bComputeHeight = (Rect.Height < 0 && h < 0);
			bool bFitWidth = (bHorizontalLayout && w == -1);
			bool bFitHeight = (bVerticalLayout && h == -1);

			if (w < 0 || h < -0)
//			if (bComputeWidth || bComputeHeight || w == -1 || h == -1)
			{
				// We should compute size of the child group
				static_cast<UIGroup*>(child)->ComputeLayout();
//				if (bComputeWidth)
//				{
					w = child->Rect.Width;
//				}
//				else
//				{
//					child->Rect.Width = w;
//				}
//				if (bComputeHeight)
//				{
					h = child->Rect.Height;
//				}
//				else
//				{
					child->Rect.Height = h;
//				}
			}
		}

		if (h >= 0)
		{
			TotalHeight += h;
		}
		else
		{
			TotalFracHeight += DecodeWidth(h);
			h = child->MinHeight;
		}

		if (w >= 0)
		{
			TotalWidth += w;
		}
		else
		{
			TotalFracWidth += DecodeWidth(w);
			w = child->MinWidth;
		}

		MarginsSize += child->TopMargin + child->BottomMargin;
		MaxWidth = max(w, MaxWidth);
		MaxHeight = max(h, MaxHeight);
	}

	DBG_LAYOUT(">>> do layout: max_w(%d) max_h(%d) frac_w(%g) frac_h(%g)", MaxWidth, MaxHeight, TotalFracWidth, TotalFracHeight);
	if (bHorizontalLayout)
	{
		if (Rect.Height <= 0)
		{
			Rect.Height = MaxHeight + groupBorderHeight;
			DBG_LAYOUT(">>> set height: %d", Rect.Height);
		}

		if (Rect.Width <= 0)
		{
			// Determine size of group for horizontal layout
			float FracScale = 0;
			for (UIElement* child = FirstChild; child; child = child->NextChild)
			{
				int w = child->Rect.Width;
				if (w < 0)
				{
					float frac = DecodeWidth(w);
					int minWidth = child->IsGroup ? child->Rect.Width: child->MinWidth;
					float localFracScale = minWidth / frac;
					if (localFracScale > FracScale)
					{
						FracScale = localFracScale;
					}
				}
			}

			Rect.Width = TotalWidth + /* MarginsSize +*/ FracScale + groupBorderWidth;
			DBG_LAYOUT(">>> computed width: %d", Rect.Width);
		}
		else
		{
			// Perform horizontal layout
			int groupWidth = Rect.Width - groupBorderWidth;
			int groupHeight = Rect.Height - groupBorderHeight - MarginsSize;
			int SizeOfComputedControls = groupWidth - TotalWidth;
			float FracScale = (TotalFracWidth > 0) ? SizeOfComputedControls / TotalFracWidth : 0;

			// Place controls inside the group
			int x = Rect.X + groupBorderLeft;
			int y = Rect.Y + groupBorderTop;
			for (UIElement* child = FirstChild; child; child = child->NextChild)
			{
				// x += child->LeftMargin;

				int w = child->Rect.Width;
				int h = child->Rect.Height;

				if (w < 0)
				{
					w = DecodeWidth(w) * FracScale;
				}

				if (h < 0)
				{
					h = DecodeWidth(h) * groupHeight;
				}

				child->Rect.X = x;
				child->Rect.Y = y;
				child->Rect.Width = w;
				child->Rect.Height = h;

				// Perform layout for child group
				if (child->IsGroup)
				{
					static_cast<UIGroup*>(child)->ComputeLayout();
				}

				x += w; // + child->RightMargin;
			}
		}
	}
	else if (bVerticalLayout)
	{
		if (Rect.Width <= 0)
		{
			Rect.Width = MaxWidth + groupBorderWidth;
			DBG_LAYOUT(">>> set width: %d", Rect.Width);
		}

		if (Rect.Height <= 0)
		{
			// Determine group size for vertical layout
			float FracScale = 0;
			for (UIElement* child = FirstChild; child; child = child->NextChild)
			{
				int h = child->Rect.Height;
				if (h < 0)
				{
					float frac = DecodeWidth(h);
					int minHeight = child->IsGroup ? child->Rect.Height : child->MinHeight;
					float localFracScale = minHeight / frac;
					if (localFracScale > FracScale)
					{
						FracScale = localFracScale;
					}
				}
			}

			Rect.Height = TotalHeight + MarginsSize + FracScale + groupBorderHeight;
			DBG_LAYOUT(">>> computed height: %d", Rect.Height);
		}
		else
		{
			// Size is known, perform vertical layout
			int groupWidth = Rect.Width - groupBorderWidth;
			int groupHeight = Rect.Height - groupBorderHeight - MarginsSize;
			int SizeOfComputedControls = groupHeight - TotalHeight;
			float FracScale = (TotalFracHeight > 0) ? SizeOfComputedControls / TotalFracHeight : 0;

			// Place controls inside the group
			int x = Rect.X + groupBorderLeft;
			int y = Rect.Y + groupBorderTop;
			for (UIElement* child = FirstChild; child; child = child->NextChild)
			{
				y += child->TopMargin;

				int w = child->Rect.Width;
				int h = child->Rect.Height;

				if (w < 0)
				{
					w = DecodeWidth(w) * groupWidth;
				}

				if (h < 0)
				{
					h = DecodeWidth(h) * FracScale;
				}

				DBG_LAYOUT("... %s(\"%s\") Layout(%d %d %d %d) -> Rect(%d %d %d %d)",
					child->ClassName(), GetDebugLabel(child),
					child->Layout.X, child->Layout.Y, child->Layout.Width, child->Layout.Height,
					x, y, w, h);

				child->Rect.X = x;
				child->Rect.Y = y;
				child->Rect.Width = w;
				child->Rect.Height = h;

				// Perform layout for child group
				if (child->IsGroup)
				{
					static_cast<UIGroup*>(child)->ComputeLayout();
				}

				y += h + child->BottomMargin;
			}
		}
	}

#if DEBUG_LAYOUT
	DBG_LAYOUT(">>> END: Rect: x(%d) y(%d) w(%d) h(%d)", Rect.X, Rect.Y, Rect.Width, Rect.Height);
	DebugLayoutDepth--;
	DBG_LAYOUT("}");
#endif

	unguard;
}

#endif // HAS_UI
