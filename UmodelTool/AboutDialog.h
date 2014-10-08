#ifndef __ABOUT_DIALOG_H__
#define __ABOUT_DIALOG_H__

#include "Version.h"
#include "res/resource.h"

//!! move to common header
#define HOMEPAGE					"http://www.gildor.org/en/projects/umodel"

class UIAboutDialog : public UIBaseDialog
{
public:
	void Show()
	{
		ShowModal("About UModel", 300, 200);
	}

	//!! - Add these lines:
	//!!   Distributed under the <license> license.
	virtual void InitUI()
	{
		(*this)
		[
			// top part
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			[
				// icon
				NewControl(UIBitmap)
					.SetWidth(64)
					.SetHeight(64)
					.SetResourceIcon(IDC_MAIN_ICON)
				+ NewControl(UISpacer, 8)
				// and text
				+ NewControl(UIGroup, GROUP_NO_BORDER)
				[
					NewControl(UISpacer, 8)
					+ NewControl(UILabel, "UE Viewer (UModel)")
					+ NewControl(UISpacer)
					+ NewControl(UILabel, "Compiled " __DATE__ " (git " STR(GIT_REVISION) ")")
					+ NewControl(UISpacer)
					+ NewControl(UILabel, "Copyright © 2007-2014 Konstantin Nosov (Gildor). All rights reserved")
				]
			]
			// bottom part
			+ NewControl(UISpacer, 8)
			+ NewControl(UIHorizontalLine)
			+ NewControl(UISpacer, 8)
			+ NewControl(UIHyperLink, "Visit our website", HOMEPAGE)
			+ NewControl(UISpacer)
			+ NewControl(UIHyperLink, "and support the developers", "http://www.gildor.org/en/donate")
			+ NewControl(UISpacer)
			// close button
			+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			[
				NewControl(UISpacer, -1)
				+ NewControl(UIButton, "Close")
				.SetWidth(80)
				.SetOK()
			]
		];
	}
};

#endif // __ABOUT_DIALOG_H__
