#include "BaseDialog.h"

class TestDialog : public UIBaseDialog
{
public:
	void Show()
	{
		value1 = false;
		value2 = true;
		value3 = 0;
		ShowModal("UI Test", 300, 200);
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
				NewMenuItem("First item\tCtrl+T")
				+ NewMenuSeparator()
				+ NewMenuCheckbox("Item #1", &value1)
					.Expose(item1)
				+ NewMenuCheckbox("Item #2", &value2)
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
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			[
				NewControl(UIGroup, "Menu control")
				[
					NewControl(UICheckbox, "Enable item #1", true)
					.SetCallback(BIND_MEM_CB(&TestDialog::OnEnableItem1, this))
					+ NewControl(UICheckbox, "Item #1", &value1)
					+ NewControl(UICheckbox, "Item #2", &value2)
				]
				+ NewControl(UISpacer)
				+ NewControl(UIGroup, "Group 1")
				[
					NewControl(UIButton, "Button 1")
					+ NewControl(UIButton, "Button 2")
				]
				+ NewControl(UISpacer)
				+ NewControl(UIGroup, "Group 2")
				.SetRadioVariable(&value3)
				[
					NewControl(UIRadioButton, "Value 0", 0)
					+ NewControl(UIRadioButton, "Value 1", 1)
					+ NewControl(UIRadioButton, "Value 2", 2)
				]
			]
		];
	}

	void OnEnableItem1(UICheckbox* sender, bool value)
	{
		item1->Enable(value);
	}

	UIMenuItem*		item1;
	bool			value1;
	bool			value2;
	int				value3;
};

void main()
{
	TestDialog dialog;
	dialog.Show();
}
