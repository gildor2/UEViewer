#include "../UI/BaseDialog.h"

class TestDialog : public UIBaseDialog
{
public:
	void Show()
	{
		value1 = false;
		value2 = true;
		value3 = 0;
		ShowModal("UI Test", 500, 200);
		printf("v1=%d v2=%d v3=%d\n", value1, value2, value3);
	}

	virtual void InitUI()
	{
		UIMenu* menu = new UIMenu;
		SetMenu(menu);

		(*menu)
		[
			NewSubmenu("File")
			[
				NewMenuItem("First item")
				+ NewMenuSeparator()
				+ NewMenuCheckbox("Not checked", &value1)
				+ NewMenuCheckbox("Checked", &value2)
				+ NewMenuSeparator()
				+ NewMenuRadioGroup(&value3)
				[
					NewMenuRadioButton("Value 0", 0)
					+ NewMenuRadioButton("Value 1", 1)
					+ NewMenuRadioButton("Value 2", 2)
				]
				+ NewMenuSeparator()
				+ NewMenuItem("Last Item")
			]
		];

		(*this)
		[
			NewControl(UIGroup, "Group 1")
			[
				NewControl(UIButton, "Button 1")
				+ NewControl(UIButton, "Button 2")
			]
		];
	}

	bool value1;
	bool value2;
	int  value3;
};

void main()
{
	TestDialog dialog;
	dialog.Show();
}
