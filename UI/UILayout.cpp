// Simple UI library.
// Copyright (C) 2022 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#include "BaseDialog.h"
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
	P(UIGroup, Label)

	return "";
#undef P
}

#define RECT_ARG(r)		r.X >= -1 ? (float)r.X : DecodeWidth(r.X),				\
						r.Y >= -1 ? (float)r.Y : DecodeWidth(r.Y),				\
						r.Width >= -1 ? (float)r.Width : DecodeWidth(r.Width),	\
						r.Height >= -1 ? (float)r.Height : DecodeWidth(r.Height)

#endif // DEBUG_LAYOUT


//todo: review, remove
int UIElement::ComputeWidth() const
{
	return Layout.Width;
}

int UIElement::ComputeHeight() const
{
	return Layout.Height;
}


/*-----------------------------------------------------------------------------
	UIGroup layout
-----------------------------------------------------------------------------*/

void UIGroup::ComputeLayout()
{
	guard(UIGroup::ComputeLayout);

#if DEBUG_LAYOUT
	DBG_LAYOUT("%s \"%s\" %s",
		ClassName(),
		(Flags & GROUP_NO_BORDER) ? "(no border)" : *Label,
		(Flags & GROUP_HORIZONTAL_LAYOUT) ? "[horz]" : "[vert]"
	);
	DBG_LAYOUT("{");
	DebugLayoutDepth++;
	DBG_LAYOUT("this: Layout(%g %g %g %g), Rect(%g %g %g %g)",
		RECT_ARG(Layout), RECT_ARG(Rect));
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
	int MarginsSizeX = 0, MarginsSizeY = 0;

	DBG_LAYOUT(">>> prepare children");
	for (UIElement* child = FirstChild; child; child = child->NextChild)
	{
		child->Rect = child->Layout;

		int x = child->Rect.X;
		int y = child->Rect.Y;
		int w = child->Rect.Width;
		int h = child->Rect.Height;
		// Variables for computing maximal width and height
		int spaceX = w;
		int spaceY = h;

		if (child->IsGroup && (w < 0 || h < 0))
		{
			// We should compute size of the child group
			static_cast<UIGroup*>(child)->ComputeLayout();

			spaceX = child->Rect.Width;
			spaceY = child->Rect.Height;

			// Store computed width and height as group's MinWidth/MinHeight
			child->MinWidth = child->Rect.Width;
			child->MinHeight = child->Rect.Height;

			// Restore child->Rect values
			bool bFitWidth = (bHorizontalLayout && w == -1);
			bool bFitHeight = (bVerticalLayout && h == -1);

			if (!bFitWidth)
			{
				child->Rect.Width = w;
			}
			else
			{
				w = child->Rect.Width;
			}
			if (!bFitHeight)
			{
				child->Rect.Height = h;
			}
			else
			{
				h = child->Rect.Height;
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

		if (x < -1)
		{
			TotalFracWidth += DecodeWidth(x);
		}
		else if (x > 0)
		{
			spaceX += x;
		}

		if (y < -1)
		{
			TotalFracHeight += DecodeWidth(y);
		}
		else if (y > 0)
		{
			spaceY += y;
		}

		// LeftMargin is used for all controls except first, RightMargin for all controls except last
		MarginsSizeX += ((child == FirstChild) ? 0 : child->LeftMargin) + ((child->NextChild) ? child->RightMargin : 0);
		MarginsSizeY += child->TopMargin + child->BottomMargin;
		MaxWidth = max(spaceX, MaxWidth);
		MaxHeight = max(spaceY, MaxHeight);
	}

	DBG_LAYOUT(">>> do layout: max_w(%d) max_h(%d) frac_w(%g) frac_h(%g)", MaxWidth, MaxHeight, TotalFracWidth, TotalFracHeight);

	// The code below for bHorizontalLayout and bVerticalLayout are almost identical, with difference that
	// in first place it use width and in second place - height (sometimes vice versa). So, when adding something
	// in one place we should consider adding similar code to another place.

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
					int minWidth = child->MinWidth;
					float localFracScale = minWidth / frac;
					if (localFracScale > FracScale)
					{
						FracScale = localFracScale;
					}
				}
			}

			Rect.Width = TotalWidth + MarginsSizeX + int(TotalFracWidth * FracScale) + groupBorderWidth;
			DBG_LAYOUT(">>> computed width: %d", Rect.Width);
		}
		else
		{
			// Perform horizontal layout
			int groupWidth = Rect.Width - groupBorderWidth - MarginsSizeX;
			int groupHeight = Rect.Height - groupBorderHeight;
			int SizeOfComputedControls = groupWidth - TotalWidth;
			float FracScale = (TotalFracWidth > 0) ? SizeOfComputedControls / TotalFracWidth : 0;

			// Place controls inside the group
			int x = Rect.X + groupBorderLeft;
			int y = Rect.Y + groupBorderTop;
			for (UIElement* child = FirstChild; child; child = child->NextChild)
			{
				x += (child == FirstChild) ? 0 : child->LeftMargin;

				int y0 = child->Rect.Y;
				int w = child->Rect.Width;
				int h = child->Rect.Height;

				if (y0 < -1)
				{
					y0 = int(DecodeWidth(y0) * FracScale);
				}
				else if (y0 == -1)
				{
					y0 = 0;
				}
				if (w < 0)
				{
					w = int(DecodeWidth(w) * FracScale);
				}

				if (h < 0)
				{
					h = int(DecodeWidth(h) * groupHeight);
				}

				child->Rect.Set(x, y + y0, w, h);

				DBG_LAYOUT("... %s(\"%s\") Layout(%g %g %g %g) -> Rect(%g %g %g %g)",
					child->ClassName(), GetDebugLabel(child),
					RECT_ARG(child->Layout), RECT_ARG(child->Rect));

				// Perform layout for child group
				if (child->IsGroup)
				{
					static_cast<UIGroup*>(child)->ComputeLayout();
				}

				x += w + ((child->NextChild) ? child->RightMargin : 0);
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
					int minHeight = child->MinHeight;
					float localFracScale = minHeight / frac;
					if (localFracScale > FracScale)
					{
						FracScale = localFracScale;
					}
				}
			}

			Rect.Height = TotalHeight + MarginsSizeY + int(TotalFracHeight * FracScale) + groupBorderHeight;
			DBG_LAYOUT(">>> computed height: %d", Rect.Height);
		}
		else
		{
			// Size is known, perform vertical layout
			int groupWidth = Rect.Width - groupBorderWidth;
			int groupHeight = Rect.Height - groupBorderHeight - MarginsSizeY;
			int SizeOfComputedControls = groupHeight - TotalHeight;
			float FracScale = (TotalFracHeight > 0) ? SizeOfComputedControls / TotalFracHeight : 0;

			// Place controls inside the group
			int x = Rect.X + groupBorderLeft;
			int y = Rect.Y + groupBorderTop;
			for (UIElement* child = FirstChild; child; child = child->NextChild)
			{
				y += child->TopMargin;

				int x0 = child->Rect.X;
				int w = child->Rect.Width;
				int h = child->Rect.Height;

				if (x0 < -1)
				{
					x0 = int(DecodeWidth(x0) * FracScale);
				}
				else if (x0 == -1)
				{
					x0 = 0;
				}
				if (w < 0)
				{
					w = int(DecodeWidth(w) * groupWidth);
				}

				if (h < 0)
				{
					h = int(DecodeWidth(h) * FracScale);
				}

				child->Rect.Set(x + x0, y, w, h);

				DBG_LAYOUT("... %s(\"%s\") Layout(%g %g %g %g) -> Rect(%g %g %g %g)",
					child->ClassName(), GetDebugLabel(child),
					RECT_ARG(child->Layout), RECT_ARG(child->Rect));

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

void UIPageControl::ComputeLayoutWithBorders(int borderLeft, int borderRight, int borderTop, int borderBottom)
{
	guard(UIPageControl::ComputeLayoutWithBorders);

#if DEBUG_LAYOUT
	DBG_LAYOUT("%s \"%s\" %s",
		ClassName(),
		(Flags & GROUP_NO_BORDER) ? "(no border)" : *Label,
		(Flags & GROUP_HORIZONTAL_LAYOUT) ? "[horz]" : "[vert]"
	);
	DBG_LAYOUT("{");
	DebugLayoutDepth++;
	DBG_LAYOUT("this.Layout: x(%g) y(%g) w(%g) h(%g) - Rect: x(%g) y(%g) w(%g) h(%g)",
		RECT_ARG(Layout), RECT_ARG(Rect));
#endif

	int MaxWidth = 0, MaxHeight = 0;

	// First pass: iterate over all children and find maximal size of the page
	DBG_LAYOUT(">>> prepare pages");
	for (UIElement* child = FirstChild; child; child = child->NextChild)
	{
		child->Rect = child->Layout;

		int w = child->Rect.Width;
		int h = child->Rect.Height;

		if (child->IsGroup)
		{
			if (w < 0 || h < 0)
			{
				// We should compute size of the child group
				static_cast<UIGroup*>(child)->ComputeLayout();

				// Store computed width and height as group's MinWidth/MinHeight
				w = child->MinWidth = child->Rect.Width;
				h = child->MinHeight = child->Rect.Height;
			}
		}

		if (w < 0)
		{
			w = child->MinWidth;
		}
		if (h < 0)
		{
			h = child->MinHeight;
		}

		MaxWidth = max(w, MaxWidth);
		MaxHeight = max(h, MaxHeight);
	}

	MaxWidth += borderLeft + borderRight;
	MaxHeight += borderTop + borderBottom;

	DBG_LAYOUT(">>> do page layout: max_w(%d) max_h(%d)", MaxWidth, MaxHeight);

	// Work with "fill maximal size" parameters
	if (Rect.Width < 0)
	{
		Rect.Width = MaxWidth;
	}
	if (Rect.Height < 0)
	{
		Rect.Height = MaxHeight;
	}

	UIRect childRect(0, 0, Rect.Width, Rect.Height);
	if (!OwnsControls)
	{
		childRect = Rect;
	}

	childRect.X += borderLeft;
	childRect.Y += borderTop;
	childRect.Width -= borderLeft + borderRight;
	childRect.Height -= borderTop + borderBottom;

	// Second pass: recompute group layouts with taking into account page control size
	for (UIElement* child = FirstChild; child; child = child->NextChild)
	{
		child->Rect = childRect;

		DBG_LAYOUT("... %s(\"%s\") Layout(%g %g %g %g) -> Rect(%g %g %g %g)",
			child->ClassName(), GetDebugLabel(child),
			RECT_ARG(child->Layout), RECT_ARG(child->Rect));

		// Perform layout for child page (group)
		if (child->IsGroup)
		{
			static_cast<UIGroup*>(child)->ComputeLayout();
		}
	}

#if DEBUG_LAYOUT
	DBG_LAYOUT(">>> END: Rect: x(%d) y(%d) w(%d) h(%d)", Rect.X, Rect.Y, Rect.Width, Rect.Height);
	DebugLayoutDepth--;
	DBG_LAYOUT("}");
#endif

	unguard;
}
