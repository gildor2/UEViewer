#include "BaseDialog.h"
#include "../../UmodelTool/res/resource.h"

// Simple 'printf' may work badly in VSCode debugger
#define printf appPrintf

class TestDialog : public UIBaseDialog
{
public:
	void Show()
	{
		SetResizeable();

		value1 = false;
		value2 = true;
		value3 = 0;
		tabIndex = 0;
//		ShowModal("UI Test", 0, 0);
		ShowModal("UI Test", 600, 500);
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
					NewControl(UIHyperLink, "Test link", "https://www.gildor.org/")
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
							.SetCallback(BIND_LAMBDA([] { appPrintf("Button 1 clicked\n"); }))
							+ NewControl(UIButton, "Button 2")
							.SetCallback(BIND_LAMBDA([] { appPrintf("Button 2 clicked\n"); }))
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
#if 0
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
#else
			+ NewControl(UITabControl)
			.SetHeight(EncodeWidth(1.0f))
#endif
			[
				// page 1
				NewControl(UIGroup, "Nested TabControl", GROUP_NO_BORDER)
				[
					NewControl(UILabel, "Just tab page with some controls ...")
					+ NewControl(UICheckbox, "Some ... checkbox", true)
					.SetCallback(BIND_LAMBDA([](UICheckbox*, bool value) {
						appPrintf("Checkbox clicked (%d)\n", value);
					}))
					+ NewControl(UIHorizontalLine)
					+ NewControl(UISpacer)
					+ NewControl(UITabControl)
					.SetHeight(EncodeWidth(1.0f))
					[
						NewControl(UIGroup, "SubPage1", GROUP_NO_BORDER)
						[
							NewControl(UICheckboxGroup, "CB-group", true)
							.SetHeight(EncodeWidth(1.0f))
							[
								NewControl(UITextEdit, &text)
								.SetMultiline()
								.SetHeight(EncodeWidth(1.0f))
							]
						]
						+ NewControl(UIGroup, "SubPage2", GROUP_NO_BORDER)
						[
							NewControl(UILabel, "Some label here ...")
							+ NewControl(UILabel, "Another label ...")
						]
					]
				]
				// page 2
				+ NewControl(UIGroup, "MulticolumnListBox", GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
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
				+ NewControl(UIGroup, "Resize Window", GROUP_NO_BORDER)
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
				+ NewControl(UILabel, "Label as a page!")
			]
		];

		list->AddItem("Initial item ...");
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
		SetResizeable();
		Bool = true;
		Text = "Some Text";
		ShowModal("UI Test", -1, -1);
	}

	virtual void InitUI()
	{
		(*this)
		[
			NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			[
				NewControl(UICheckbox, "Flat view", false)
				.SetCallback(BIND_LAMBDA([this](UICheckbox*, bool value) { pager->SetActivePage((int)value); } ))
				+ NewControl(UISpacer)
				+ NewControl(UILabel, "Filter:")
					.SetY(2)
					.SetAutoSize()
				+ NewControl(UITextEdit, "some text")
					.SetWidth(120)
			]
#if 1
			+ NewControl(UIPageControl)
				.Expose(pager)
#else
			+ NewControl(UIGroup, "Hehe")
#endif
				.SetHeight(EncodeWidth(1.0f))
			[
				// page 0: TreeView + ListBox
				NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
				.SetHeight(EncodeWidth(1.0f))
				[
					NewControl(UITreeView)
						.SetRootLabel("Game")
						.SetWidth(EncodeWidth(0.3f))
						.SetHeight(-1)
						.UseFolderIcons()
						.SetItemHeight(20)
					+ NewControl(UIMulticolumnListbox, 3)
				]
				// page 1: single ListBox
				+ NewControl(UIMulticolumnListbox, 3)
			]
		];
	}

	FString Text;
	bool Bool;
	UIPageControl* pager;
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
