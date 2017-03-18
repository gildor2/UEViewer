#ifndef __UE4_VERSION_DIALOG_H__
#define __UE4_VERSION_DIALOG_H__

#if UNREAL4

class UIUE4VersionDialog : public UIBaseDialog
{
public:
	int Show(int verMin, int verMax)
	{
		VersionMin = verMin;
		VersionMax = verMax;

		if (!ShowModal("Unreal engine 4 version", 250, 200))
			return -1;

		int selection = SelectVersionCombo->GetSelectionIndex();
		return selection >= 0 ? selection + VersionMin : -1;
	}

	void InitUI()
	{
		(*this)
		[
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			[
				NewControl(UIBitmap)
				.SetWidth(48)
				.SetHeight(48)
				.SetResourceIcon(UIBitmap::BI_Question)
				+ NewControl(UISpacer, 8)
				+NewControl(UIGroup, GROUP_NO_BORDER)
				[
					NewControl(UILabel, "UModel has found an unversioned UE4 package. In order to")
					+NewControl(UILabel, "work correctly please specify which Unreal engine 4 version")
					+NewControl(UILabel, "is used for this game.")
					+NewControl(UISpacer)
					+NewControl(UICombobox)
					.SetCallback(BIND_MEM_CB(&UIUE4VersionDialog::EngineSelected, this))
					.Expose(SelectVersionCombo)
					+NewControl(UISpacer)
					+NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
					[
						NewControl(UISpacer, -1)
						+NewControl(UIButton, "Ok")
						.SetWidth(80)
						.SetOK()
						.Enable(false)
						.Expose(OkButton)
						+NewControl(UISpacer)
						+NewControl(UIButton, "Cancel")
						.SetWidth(80)
						.SetCancel()
					]
				]
			]
		];

		// Fill versions list
		for (int ver = VersionMin; ver <= VersionMax; ver++)
		{
			char buffer[64];
			appSprintf(ARRAY_ARG(buffer), "Unreal engine 4.%d", ver);
			SelectVersionCombo->AddItem(buffer);
		}
	}

	void EngineSelected()
	{
		OkButton->Enable(true);
	}

	int			VersionMin;
	int			VersionMax;

	UICombobox* SelectVersionCombo;
	UIButton*	OkButton;
};

#endif // UNREAL4

#endif // __UE4_VERSION_DIALOG_H__
