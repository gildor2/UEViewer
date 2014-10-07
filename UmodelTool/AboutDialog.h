#ifndef __ABOUT_DIALOG_H__
#define __ABOUT_DIALOG_H__

#include "Version.h"

//!! move to common header
#define HOMEPAGE					"http://www.gildor.org/en/projects/umodel"

class UIAboutDialog : public UIBaseDialog
{
public:
	void Show()
	{
		ShowModal("About UModel", 300, 200);
	}

	//!! - Make larger font for 1st line -- should use AutoSize for label, and compute height too
	//!! - Add icon
	//!! - Add these lines:
	//!!   Distributed under the <license> license.
	virtual void InitUI()
	{
		(*this)
		[
			NewControl(UILabel, "UE viewer (UModel)")
			+ NewControl(UISpacer)
			+ NewControl(UILabel, "Compiled " __DATE__ " (git " STR(GIT_REVISION) ")")
			+ NewControl(UISpacer)
			+ NewControl(UILabel, "Copyright © 2007-2014 Konstantin Nosov (Gildor). All rights reserver")
			+ NewControl(UISpacer, 8)
			+ NewControl(UIHorizontalLine)
			+ NewControl(UISpacer, 8)
			+ NewControl(UIHyperLink, "Visit our website", HOMEPAGE)
			+ NewControl(UISpacer)
			+ NewControl(UIHyperLink, "and support the developers", "http://www.gildor.org/en/donate")
			+ NewControl(UISpacer)
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
