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
		ShowModal("UI Test", 350, 200);
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
				+ NewMenuItem("Last Item")
			]
			+ NewMenuItem("Empty")
		];

		UIMenu* popup = new UIMenu;
		(*popup)
		[
			NewMenuItem("Popup item 1")
			.SetCallback(BIND_MEM_CB(&TestDialog::OnMenuItem, this))
			+ NewMenuItem("Popup item 2")
			.SetCallback(BIND_MEM_CB(&TestDialog::OnMenuItem, this))
			+ NewMenuItem("Popup item 3")
			.SetCallback(BIND_MEM_CB(&TestDialog::OnMenuItem, this))
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
				+ NewControl(UISpacer, 8)
				+ NewControl(UIVerticalLine)
				.SetHeight(100)
				+ NewControl(UISpacer, 8)
				+ NewControl(UIGroup, GROUP_NO_BORDER)
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
					+ NewControl(UISpacer, 8)
					+ NewControl(UIHorizontalLine)
					+ NewControl(UISpacer, 8)
					+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
					[
						NewControl(UISpacer, -1)
						+ NewControl(UIMenuButton, "Menu")
						.SetWidth(100)
						.SetMenu(popup)
						+ NewControl(UISpacer)
						+ NewControl(UIButton, "Close")
						.SetWidth(100)
						.SetOK()
					]
				]
			]
			+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			.SetRadioCallback(BIND_MEM_CB(&TestDialog::OnPageChanged, this))
			.SetRadioVariable(&tabIndex)
			[
				NewControl(UIRadioButton, "page 1")
				+ NewControl(UIRadioButton, "page 2")
			]
			+ NewControl(UIPageControl)
			.SetHeight(150)
			.Expose(pager)
			[
				// page 1
				NewControl(UITextEdit, &text)
				.SetHeight(-1)
				.SetMultiline()
				// page 2
				+ NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
				[
					NewControl(UIMulticolumnListbox, 3)
					.Expose(list)
					.SetHeight(150)
					.AddColumn("Column 1", 100)
					.AddColumn("Column 2", 50)
					.AddColumn("Column 3")
					.AllowMultiselect()
					+ NewControl(UISpacer)
					+ NewControl(UIGroup, GROUP_NO_BORDER)
					.SetWidth(100)
					[
						NewControl(UIButton, "Add")
						.SetCallback(BIND_MEM_CB(&TestDialog::OnAddItems, this))
						+ NewControl(UIButton, "Remove")
						.SetCallback(BIND_MEM_CB(&TestDialog::OnRemoveItems, this))
						+ NewControl(UIButton, "Unselect")
						.SetCallback(BIND_MEM_CB(&TestDialog::OnUnselectItems, this))
					]
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

	void OnUnselectItems()
	{
		list->UnselectAllItems();
	}

	void OnEnableItem1(UICheckbox* sender, bool value)
	{
		item1->Enable(value);
	}

	void OnPageChanged()
	{
		pager->SetActivePage(tabIndex);
	}

	UIMenuItem*		item1;
	UIPageControl*	pager;
	UIMulticolumnListbox* list;
	bool			value1;
	bool			value2;
	int				value3;
	int				tabIndex;
	FString			text;
};

void main()
{
	TestDialog dialog;
	dialog.Show();
}
