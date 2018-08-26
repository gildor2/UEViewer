#ifndef __ERROR_DIALOG_H__
#define __ERROR_DIALOG_H__

class UIErrorDialog : public UIBaseDialog
{
public:
	void Show()
	{
		ShowModal("Fatal Error", 475, -1);
	}

	void InitUI()
	{
#if DO_GUARD
		char message[256];
		char* s = strchr(GErrorHistory, '\n');
		const char* log;
		if (s)
		{
			appStrncpyz(message, GErrorHistory, min(s - GErrorHistory + 1, ARRAY_COUNT(message)));
			log = s + 1;
		}
		else
		{
			appStrncpyz(message, GErrorHistory, ARRAY_COUNT(message));
			log = "";
		}

		(*this)
		[
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			.SetWidth(EncodeWidth(1.0f))
			[
				NewControl(UIBitmap)
				.SetWidth(48)
				.SetHeight(48)
				.SetResourceIcon(UIBitmap::BI_Error)
				+ NewControl(UISpacer)
				+ NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(EncodeWidth(1.0f))
				[
					NewControl(UILabel, message)
					.SetHeight(-1)		//!! auto-size label height; use AutoVSize API?
					+ NewControl(UISpacer, 16)
					+ NewControl(UILabel, "Call stack:")
					+ NewControl(UISpacer)
					+ NewControl(UITextEdit, log)
					.SetHeight(100)		//?? try to auto-size it
					.SetMultiline()
					.SetReadOnly()
					.SetWantFocus(false)
				]
				+ NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(80)
				[
					NewControl(UIButton, "Close")
					.SetWidth(80)
					.SetOK()
					+ NewControl(UIButton, "Copy")
					.SetWidth(80)
					.SetCallback(BIND_STATIC(&UIErrorDialog::CopyToClipboard))
//					.SetCallback(BIND_STATIC([]() { appCopyTextToClipboard(GErrorHistory); })) - error in VS2013
				]
			]
		];
#endif // DO_GUARD
	}

	static void CopyToClipboard()
	{
#if DO_GUARD
		appCopyTextToClipboard(GErrorHistory);
#endif
	}
};

#endif // __ERROR_DIALOG_H__
