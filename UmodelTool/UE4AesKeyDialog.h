#ifndef __UE4_AES_KEY_DIALOG_H__
#define __UE4_AES_KEY_DIALOG_H__

#if UNREAL4

class UIUE4AesKeyDialog : public UIBaseDialog
{
public:
	bool Show(TArray<FString>& Values)
	{
		SetResizeable();
		if (!ShowModal("Please enter AES encryption key", 530, 200))
			return "";

		if (Value.IsEmpty())
		{
			return false;
		}

		// Separate multiple AES keys to FString's
		const char* Begin = &Value[0];
		int Len = Value.Len() + 1; // include null character for simpler loop below
		for (int i = 0; i < Len; i++)
		{
			const char* s = &Value[i];
			char c = *s;
			if (c == '\n' || c == 0)
			{
				int len = s - Begin;
				if (len)
				{
					FStaticString<256> Code(len, Begin);
					Code.TrimStartAndEndInline();
					if (Code.Len()) Values.Add(Code);
				}
				Begin = s + 1;
			}
		}

		return true;
	}

	void InitUI()
	{
		(*this)
		[
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			.SetWidth(EncodeWidth(1.0f))
			.SetHeight(EncodeWidth(1.0f)) // allow vertical resize of contents
			[
				NewControl(UIBitmap)
				.SetWidth(48)
				.SetHeight(48)
				.SetResourceIcon(UIBitmap::BI_Warning)
				+ NewControl(UISpacer, 8)
				+NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(EncodeWidth(1.0f))
				[
					NewControl(UILabel, "UE Viewer has found an encrypted UE4 pak file. In order to work correctly").SetAutoSize()
					+NewControl(UILabel, "please specify an AES encryption key which is used for this game.").SetAutoSize()
					+NewControl(UILabel, "Multiple AES keys could be entered, each key on the new line").SetAutoSize()
					+NewControl(UISpacer)
					+NewControl(UITextEdit, &Value)
					.SetHeight(-1)
					.SetMultiline(true)
					+NewControl(UISpacer)
					+NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
					[
						NewControl(UISpacer, -1)
						+NewControl(UIButton, "Ok")
						.SetWidth(80)
						.SetOK()
						+NewControl(UIButton, "Cancel")
						.SetWidth(80)
						.SetCancel()
					]
				]
			]
		];
	}

	FString		Value;
//	UIButton*	OkButton;
};

#endif // UNREAL4

#endif // __UE4_AES_KEY_DIALOG_H__
