#include "BaseDialog.h"

#if HAS_UI

#include "ProgressDialog.h"
#include "UnObject.h"


//!! Other possible statistics:
//!! - elapsed time

void UIProgressDialog::Show(const char* title)
{
	CloseOnEsc();
	ShowDialog(title, 250, 200);
}

void UIProgressDialog::SetDescription(const char* text)
{
	DescriptionText = text;
}

bool UIProgressDialog::Progress(const char* package, int index, int total)
{
	char buffer[512];
	appSprintf(ARRAY_ARG(buffer), "%s %d/%d", DescriptionText, index+1, total);
	DescriptionLabel->SetText(buffer);

	PackageLabel->SetText(package);

	ProgressBar->SetValue((float)(index+1) / total);

	return Tick();
}

bool UIProgressDialog::Tick()
{
	char buffer[64];
	appSprintf(ARRAY_ARG(buffer), "%d MBytes", (int)(GTotalAllocationSize >> 20));
	MemoryLabel->SetText(buffer);
	appSprintf(ARRAY_ARG(buffer), "%d", UObject::GObjObjects.Num());
	ObjectsLabel->SetText(buffer);
	return PumpMessageLoop();
}

void UIProgressDialog::InitUI()
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

#endif // HAS_UI
