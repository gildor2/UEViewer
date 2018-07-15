#include "BaseDialog.h"
#include "../../UmodelTool/res/resource.h"

class TestDialog : public UIBaseDialog
{
public:
	void Show()
	{
		value1 = false;
		value2 = true;
		value3 = 0;
		tabIndex = 0;
//		ShowModal("UI Test", 0, 0);
		ShowModal("UI Test", 470, 350);
		printf("v1=%d v2=%d v3=%d\n", value1, value2, value3);
		printf("Text: [%s]\n", *text);
	}

	virtual void InitUI()
	{
		UIMenu* menu = new UIMenu;
		SetMenu(menu);

		(*menu)
		[
			NewSubmenu("Menu")
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
				+ NewSubmenu("Submenu")
				[
					NewMenuItem("Submenu 1")
					+ NewMenuItem("Submenu 2")
					+ NewMenuItem("Submenu 3")
				]
				+ NewMenuItem("Last Item")
			]
			+ NewMenuItem("Empty")
				.Expose(emptyItem)
		];

		UIMenu* popup = new UIMenu;
		(*popup)
		[
			NewMenuItem("Popup item 1")
			.SetCallback(BIND_MEMBER(&TestDialog::OnMenuItem, this))
			+ NewMenuItem("Popup item 2")
			.SetCallback(BIND_MEMBER(&TestDialog::OnMenuItem, this))
			+ NewMenuItem("Popup item 3")
			.SetCallback(BIND_MEMBER(&TestDialog::OnMenuItem, this))
		];

		(*this)
		[
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			[
				NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(70)
				[
					NewControl(UIHyperLink, "Test link", "http://www.gildor.org/")
					+ NewControl(UIBitmap)
						.SetWidth(64)
						.SetHeight(64)
						.SetResourceIcon(IDC_MAIN_ICON)
					+ NewControl(UIBitmap)
						.SetX(16)
						.SetWidth(32)
						.SetHeight(32)
						.SetResourceIcon(IDC_MAIN_ICON)
					+ NewControl(UIBitmap)
						.SetX(24)
						.SetWidth(16)
						.SetHeight(16)
						.SetResourceIcon(IDC_MAIN_ICON)
				]
				+ NewControl(UISpacer, 4)
				+ NewControl(UIVerticalLine)
				.SetHeight(-1)
				+ NewControl(UISpacer, 4)
				+  NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(EncodeWidth(1.0f))
				[
					NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
					[
						NewControl(UIGroup, "Menu control")
						.SetWidth(EncodeWidth(1.0f))
						[
							NewControl(UICheckbox, "Enable item #1", true)
							.SetCallback(BIND_LAMBDA([this](UICheckbox*, bool value) { item1->Enable(value); }))
							+ NewControl(UICheckbox, "Item #1", &value1)
							+ NewControl(UICheckbox, "Item #2", &value2)
							+ NewControl(UICheckbox, "Enable \"Empty\"", true)
							.SetCallback(BIND_LAMBDA([this](UICheckbox*, bool value) { emptyItem->Enable(value); }))
						]
						+ NewControl(UIGroup, "Group 1")
						.SetWidth(EncodeWidth(1.0f))
						[
							NewControl(UIButton, "Button 1")
							+ NewControl(UIButton, "Button 2")
						]
						+ NewControl(UICheckboxGroup, "Group 2", true)
						.SetWidth(EncodeWidth(1.0f))
						.SetRadioVariable(&value3)
						[
							NewControl(UIRadioButton, "Value 0", 0)
							+ NewControl(UIRadioButton, "Value 1", 1)
							+ NewControl(UIRadioButton, "Value 2", 2)
						]
					]
					+ NewControl(UISpacer, 4)
					+ NewControl(UIHorizontalLine)
					+ NewControl(UISpacer, 4)
					+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
					[
						NewControl(UISpacer, -1)
						+ NewControl(UIMenuButton, "Menu")
						.SetWidth(100)
						.SetMenu(popup)
						+ NewControl(UIButton, "Close")
						.SetWidth(100)
						.SetOK()
					]
				]
			]
			+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			.SetRadioCallback(BIND_LAMBDA([this]() { pager->SetActivePage(tabIndex); }))
			.SetRadioVariable(&tabIndex)
			[
				NewControl(UIRadioButton, "page 1")
				+ NewControl(UIRadioButton, "page 2")
				+ NewControl(UIRadioButton, "page 3")
			]
			+ NewControl(UIPageControl)
			.SetHeight(EncodeWidth(1.0f))
			.Expose(pager)
			[
				// page 1
				NewControl(UIGroup, GROUP_NO_BORDER)
				[
					NewControl(UILabel, "Some label for page 1")
					+ NewControl(UITextEdit, &text)
					.SetHeight(EncodeWidth(1.0f))
					.SetMultiline()
				]
				// page 2
				+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
				[
					NewControl(UIMulticolumnListbox, 3)
					.Expose(list)
					.SetHeight(-1)
					.AddColumn("Column 1", 100)
					.AddColumn("Column 2", 50)
					.AddColumn("Column 3")
					.AllowMultiselect()
//					.SetWidth(500)
					+ NewControl(UIGroup, GROUP_NO_BORDER)
					.SetWidth(100)
					[
						NewControl(UIButton, "Add")
						.SetCallback(BIND_MEMBER(&TestDialog::OnAddItems, this))
						+ NewControl(UIButton, "Remove")
						.SetCallback(BIND_MEMBER(&TestDialog::OnRemoveItems, this))
						+ NewControl(UIButton, "Unselect")
						.SetCallback(BIND_LAMBDA( [this]() { list->UnselectAllItems(); } ) )
						+ NewControl(UIButton, "HAHAHA")
						.SetHeight(50)
					]
				]
				// page 3
				+ NewControl(UIGroup, GROUP_NO_BORDER)
				[
					NewControl(UILabel, "Set window size:")
					+ NewControl(UISpacer)
					+ NewControl(UIButton, "450 x 400")
					.SetCallback(BIND_LAMBDA([this]() { SetWindowSize(450, 400); } ))
					+ NewControl(UIButton, "700 x 600")
					.SetCallback(BIND_LAMBDA([this]() { SetWindowSize(700, 600); } ))
					+ NewControl(UIButton, "1000 x 800")
					.SetCallback(BIND_LAMBDA([this]() { SetWindowSize(1000, 800); } ))
				]
			]
		];
	}

	void OnMenuItem(UIMenuItem* item)
	{
		printf("Menu button clicked: %s\n", item->GetText());
	}

	void OnAddItems()
	{
		static int nn = 0;
		char buf[32];
		static const char* texts[] =
		{
			"------",
			"this",
			"is",
			"UI",
			"framework",
			"test",
		};
		for (int i = 0; i < ARRAY_COUNT(texts); i++)
		{
			int n = list->AddItem(texts[i]);
			appSprintf(ARRAY_ARG(buf), "%d", n);
			list->AddSubItem(n, 1, buf);
			appSprintf(ARRAY_ARG(buf), "%d", ++nn);
			list->AddSubItem(n, 2, buf);
		}
	}

	void OnRemoveItems()
	{
		while (list->GetSelectionCount() > 0)
		{
			int index = list->GetSelectionIndex(0);
			list->RemoveItem(index);
		}
		printf("------\n");
		for (int i = 0; i < list->GetItemCount(); i++)
			printf("%d = %s\n", i, list->GetItem(i));
		printf("------\n");
	}

	UIMenuItem*		item1;
	UIMenuItem*		emptyItem;
	UIPageControl*	pager;
	UIMulticolumnListbox* list;
	bool			value1;
	bool			value2;
	int				value3;
	int				tabIndex;
	FString			text;
};

class TestDialog2 : public UIBaseDialog
{
public:
	void Show()
	{
		Bool = true;
		Text = "Some Text";
		ShowModal("UI Test", 200, 100);
	}

	virtual void InitUI()
	{
		(*this)
		[
			NewControl(UILabel, "Label")
#if 1
			+ NewControl(UIGroup, "Group")
			[
				NewControl(UITextEdit, &Text)
				.SetHeight(-1)
//				.SetHeight(10)
			]
#else
			+ NewControl(UITextEdit, &Text)
			.SetHeight(-1)
#endif
			+ NewControl(UICheckbox, "Checkbox", &Bool)
			+ NewControl(UIButton, "Button")
		];
	}

	FString Text;
	bool Bool;
};

int main()
{
#if 1
	TestDialog dialog;
#else
	TestDialog2 dialog;
#endif
	dialog.Show();
}
