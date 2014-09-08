#ifndef __PROGRESS_DIALOG_H__
#define __PROGRESS_DIALOG_H__

//!! add progress bar - show ( number_of_loaded/exported_objects / number_of_objects_to_process )
//!! esc should close this dialog
class UIProgressDialog : public UIBaseDialog
{
public:
	void Show(const char* title)
	{
		CloseOnEsc();
		ShowDialog(title, 250, 200);
	}

	void SetDescription(const char* text)
	{
		DescriptionText = text;
	}

	bool Progress(const char* package, int index, int total)
	{
		char buffer[512];
		appSprintf(ARRAY_ARG(buffer), "%s %d/%d", DescriptionText, index+1, total);
		DescriptionLabel->SetText(buffer);

		PackageLabel->SetText(package);

		ProgressBar->SetValue((float)(index+1) / total);

		return Tick();
	}

	bool Tick()
	{
		char buffer[64];
		appSprintf(ARRAY_ARG(buffer), "%d MBytes", GTotalAllocationSize >> 20);
		MemoryLabel->SetText(buffer);
		appSprintf(ARRAY_ARG(buffer), "%d", UObject::GObjObjects.Num());
		ObjectsLabel->SetText(buffer);
		return PumpMessageLoop();
	}

protected:
	const char*	DescriptionText;
	UILabel*	DescriptionLabel;
	UILabel*	PackageLabel;
	UILabel*	MemoryLabel;
	UILabel*	ObjectsLabel;
	UIProgressBar* ProgressBar;

	virtual void InitUI()
	{
		(*this)
		[
			NewControl(UIGroup)
			[
				NewControl(UILabel, "")
				.Expose(DescriptionLabel)
				+ NewControl(UISpacer)
				+ NewControl(UILabel, "")
				.Expose(PackageLabel)
				+ NewControl(UIProgressBar)
				.Expose(ProgressBar)
			]
			+ NewControl(UIGroup)
			[
				NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
				[
					NewControl(UILabel, "Memory used:", TA_Right)
					+ NewControl(UISpacer)
					+ NewControl(UILabel, "")
					.Expose(MemoryLabel)
				]
				+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
				[
					NewControl(UILabel, "Objects loaded:", TA_Right)
					+ NewControl(UISpacer)
					+ NewControl(UILabel, "")
					.Expose(ObjectsLabel)
				]
			]
			+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			[
				NewControl(UISpacer, -1)
				+ NewControl(UIButton, "Cancel")
				.SetWidth(100)
				.SetCancel()
				+ NewControl(UISpacer, -1)
			]
		];
	}
};


#endif // __PROGRESS_DIALOG_H__
