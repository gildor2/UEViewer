#ifndef __ABOUT_DIALOG_H__
#define __ABOUT_DIALOG_H__

#include "MiscStrings.h"
#include "res/resource.h"

class UIAboutDialog : public UIBaseDialog
{
public:
	static void Show()
	{
		UIAboutDialog dialog;
		dialog.ShowModal("About UModel", 450, -1);
	}

	//!! - Add these lines:
	//!!   Distributed under the <license> license.
	virtual void InitUI()
	{
		(*this)
		[
			// top part
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			.SetWidth(EncodeWidth(1.0f))
			[
				// icon
				NewControl(UIBitmap)
					.SetWidth(64)
					.SetHeight(64)
					.SetResourceIcon(IDC_MAIN_ICON)
				+ NewControl(UISpacer, 8)
				// and text
				+ NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(EncodeWidth(1.0f))
				[
					NewControl(UISpacer, 8)
					+ NewControl(UILabel, "UE Viewer (UModel)")
					+ NewControl(UISpacer)
					+ NewControl(UILabel, GBuildString)
					+ NewControl(UISpacer)
					+ NewControl(UILabel, GCopyrightString)
					+ NewControl(UILabel, GLicenseString)
				]
			]
			// bottom part
			+ NewControl(UISpacer, 8)
			+ NewControl(UIHorizontalLine)
			+ NewControl(UISpacer, 8)
			+ NewControl(UIHyperLink, "Visit our website", GUmodelHomepage)
			+ NewControl(UISpacer)
			+ NewControl(UIHyperLink, "Donate", "https://www.gildor.org/en/donate")
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
