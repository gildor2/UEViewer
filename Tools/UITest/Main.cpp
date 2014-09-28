#include "../UI/BaseDialog.h"

class TestDialog : public UIBaseDialog
{
public:
	void Show()
	{
		ShowModal("UI Test", 500, 200);
	}

	virtual void InitUI()
	{
		(*this)
		[
			NewControl(UIGroup, "Group 1")
			[
				NewControl(UIButton, "Button 1")
				+ NewControl(UIButton, "Button 2")
			]
		];
	}
};

void main()
{
	TestDialog dialog;
	dialog.Show();
}
