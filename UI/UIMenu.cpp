// Simple UI library.
// Copyright (C) 2022 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#if _WIN32

#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windows headers
#define _CRT_SECURE_NO_WARNINGS
#undef UNICODE

#include <windows.h>
#include <ShellAPI.h>				// for ShellExecute

#endif // _WIN32

#include "BaseDialog.h"
#include "UIPrivate.h"

/*-----------------------------------------------------------------------------
	UIMenuItem
-----------------------------------------------------------------------------*/

// Hyperlink
UIMenuItem::UIMenuItem(const char* text, const char* link)
{
	Init(MI_HyperLink, text);
	Link = link;
}

// Checkbox
UIMenuItem::UIMenuItem(const char* text, bool checked)
:	bValue(checked)
,	pValue(&bValue)
,	iValue(0) // indicates that pValue is bool and not a mask
{
	Init(MI_Checkbox, text);
}

// Checkbox
UIMenuItem::UIMenuItem(const char* text, bool* checked)
:	pValue(checked)
//,	bValue(value) - uninitialized
,	iValue(0) // indicates that pValue is bool and not a mask
{
	Init(MI_Checkbox, text);
}

// Checkbox
UIMenuItem::UIMenuItem(const char* text, int* value, int mask)
:	pValue(value)
,	iValue(mask) // indicates that pValue is a bitfield with iValue mask
//,	bValue(value) - uninitialized
{
	assert((mask & (mask-1)) == 0 && mask != 0); // should be non-zero value with a single bit set
	Init(MI_Checkbox, text);
}

// RadioGroup
UIMenuItem::UIMenuItem(int value)
:	iValue(value)
,	pValue(&iValue)
{
	Init(MI_RadioGroup, NULL);
}

// RadioGroup
UIMenuItem::UIMenuItem(int* value)
:	pValue(value)
//,	iValue(value) - uninitialized
{
	Init(MI_RadioGroup, NULL);
}

// RadioButton
UIMenuItem::UIMenuItem(const char* text, int value)
:	iValue(value)
{
	Init(MI_RadioButton, text);
}

// Common part of constructors
void UIMenuItem::Init(EType type, const char* label)
{
	Type = type;
	Label = label ? label : "";
	Link = NULL;
	Id = 0;
	hMenu = NULL;
	Parent = NextChild = FirstChild = NULL;
	Enabled = true;
}

// Destructor: release all child items
UIMenuItem::~UIMenuItem()
{
	DestroyChildren();
}

void UIMenuItem::DestroyChildren()
{
	UIMenuItem* next;
	for (UIMenuItem* curr = FirstChild; curr; curr = next)
	{
		next = curr->NextChild;
		delete curr;		// may be recurse here
	}
	FirstChild = NULL;
}

// Append a new menu item to chain. This code is very similar to
// UIElement::operator+().
UIMenuItem& operator+(UIMenuItem& item, UIMenuItem& next)
{
	guard(operator+(UIMenuItem));

	UIMenuItem* e = &item;
	while (true)
	{
		UIMenuItem* n = e->NextChild;
		if (!n)
		{
			e->NextChild = &next;
			break;
		}
		e = n;
	}
	return item;

	unguard;
}

UIMenuItem& UIMenuItem::Enable(bool enable)
{
	if (Type == MI_Submenu)
	{
		// Propagate to children
		for (UIMenuItem* curr = FirstChild; curr; curr = curr->NextChild)
		{
			curr->Enable(enable);
		}
	}

	Enabled = enable;
	if (Parent && Parent->hMenu)
	{
		EnableMenuItem(Parent->hMenu, GetItemIndex(), MF_BYPOSITION | (enable ? MF_ENABLED : MF_DISABLED));
		UIMenu* Menu = GetOwner();
		if (Parent == Menu)
		{
			Menu->Redraw();
		}
	}
	return *this;
}

UIMenuItem& UIMenuItem::SetName(const char* newName)
{
	if (Label != newName)
	{
		Label = newName;
		if (Parent && Parent->hMenu)
		{
			MENUITEMINFO mii;
			memset(&mii, 0, sizeof(mii));
			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_STRING;
			mii.dwTypeData = (LPSTR)*Label;
			SetMenuItemInfo(Parent->hMenu, GetItemIndex(), TRUE, &mii);
			// If Parent is UIMenu, we're working with top-level menu item, so we should refresh menu line
			UIMenu* Menu = GetOwner();
			if (Parent == Menu)
			{
				Menu->Redraw();
			}
		}
	}
	return *this;
}

// Add new submenu. This code is very similar to UIGroup::Add().
void UIMenuItem::Add(UIMenuItem* item)
{
	guard(UIMenuItem::Add);

	assert(Type == MI_Submenu || Type == MI_RadioGroup);

	if (!FirstChild)
	{
		FirstChild = item;
	}
	else
	{
		// find last child
		UIMenuItem* prev = NULL;
		for (UIMenuItem* curr = FirstChild; curr; prev = curr, curr = curr->NextChild)
		{ /* empty */ }
		// add item(s)
		prev->NextChild = item;
	}

	// set parent for all items in chain
	for ( /* empty */; item; item = item->NextChild)
	{
		assert(item->Parent == NULL);
		item->Parent = this;
	}

	unguard;
}

// Get index of 'this' menu item in parent's children list
int UIMenuItem::GetItemIndex() const
{
	if (!Parent) return -1;
	int index = 0;
	for (UIMenuItem* item = Parent->FirstChild; item; item = item->NextChild)
	{
		if (item == this) return index;
		// todo: can skip invisible items, if we'll support those
		index++;
	}
	return -1;
}

// Recursive function for menu creation
void UIMenuItem::FillMenuItems(HMENU parentMenu, int& nextId, int& position)
{
	guard(UIMenuItem::FillMenuItems);

	assert(Type == MI_Submenu || Type == MI_RadioGroup);

	for (UIMenuItem* item = FirstChild; item; item = item->NextChild, position++)
	{
		switch (item->Type)
		{
		case MI_Text:
		case MI_HyperLink:
		case MI_Checkbox:
		case MI_RadioButton:
			{
				assert(item->Id == 0);
				item->Id = nextId++;

				MENUITEMINFO mii;
				memset(&mii, 0, sizeof(mii));

				UINT fType = MFT_STRING;
				UINT fState = 0;
				if (!item->Enabled) fState |= MFS_DISABLED;
				if (item->Type == MI_Checkbox && item->GetCheckboxValue())
				{
					// checked checkbox
					fState |= MFS_CHECKED;
				}
				else if (Type == MI_RadioGroup && item->Type == MI_RadioButton)
				{
					// radio button will work as needed only
					fType |= MFT_RADIOCHECK;
					if (*(int*)pValue == item->iValue)
						fState |= MFS_CHECKED;
				}

				mii.cbSize     = sizeof(mii);
				mii.fMask      = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
				mii.fType      = fType;
				mii.fState     = fState;
				mii.wID        = item->Id;
				mii.dwTypeData = const_cast<char*>(*item->Label);
				mii.cch        = (UINT)strlen(mii.dwTypeData);

				InsertMenuItem(parentMenu, position, TRUE, &mii);
			}
			break;

		case MI_Separator:
			AppendMenu(parentMenu, MF_SEPARATOR, 0, NULL);
			break;

		case MI_Submenu:
			{
				assert(!item->hMenu);
				item->hMenu = CreatePopupMenu();
				AppendMenu(parentMenu, MF_POPUP, (UINT_PTR)item->hMenu, *item->Label);
				int submenuPosition = 0;
				item->FillMenuItems(item->hMenu, nextId, submenuPosition);
			}
			break;

		case MI_RadioGroup:
			// just add all children to current menu
			item->FillMenuItems(parentMenu, nextId, position);
			break;

		default:
			appError("Unknown item type: %d (label=%s)", item->Type, *item->Label);
		}
	}

	unguard;
}

bool UIMenuItem::HandleCommand(int id)
{
	guard(UIMenuItem::HandleCommand);

	HMENU hMenu = GetMenuHandle();
	if (!hMenu) return false; // should not happen - HandleCommand executed when menu is active, so it exists

	for (UIMenuItem* item = FirstChild; item; item = item->NextChild)
	{
		if (item->Id == id)
		{
			// this item was clicked
			switch (item->Type)
			{
			case MI_Text:
				if (item->Callback)
					item->Callback(item);
				break;

			case MI_HyperLink:
				ShellExecute(NULL, "open", item->Link, NULL, NULL, SW_SHOW);
				break;

			case MI_Checkbox:
				{
					// change value
					bool value = !item->GetCheckboxValue();
					item->SetCheckboxValue(value);
					// update menu
					CheckMenuItem(hMenu, item->Id, MF_BYCOMMAND | (value ? MF_CHECKED : 0));
					// callbacks
					if (item->Callback)
						item->Callback(item);
					if (item->CheckboxCallback)
						item->CheckboxCallback(item, value);
				}
				break;

			default:
				appError("Unknown item type: %d (label=%s)", item->Type, *item->Label);
			}
			return true;
		}
		// id is different, verify container items
		switch (item->Type)
		{
		case MI_Submenu:
			// recurse to children
			if (item->HandleCommand(id))
				return true;
			break;

		case MI_RadioGroup:
			{
				// check whether this id belongs to radio group
				// 'item' is group here
				UIMenuItem* clickedButton = NULL;
				int newValue = 0;
				for (UIMenuItem* button = item->FirstChild; button; button = button->NextChild)
					if (button->Id == id)
					{
						clickedButton = button;
						newValue = button->iValue;
						break;
					}
				if (!clickedButton) continue;	// not in this group
				// it's ours, process the button
				int oldValue = *(int*)item->pValue;
				for (UIMenuItem* button = item->FirstChild; button; button = button->NextChild)
				{
					assert(button->Type == MI_RadioButton);
					bool checked = (button == clickedButton);
					if (button->iValue == oldValue || checked)
						CheckMenuItem(hMenu, button->Id, MF_BYCOMMAND | (checked ? MF_CHECKED : 0));
				}
				// update value
				*(int*)item->pValue = newValue;
				// callbacks
				if (clickedButton->Callback)
					clickedButton->Callback(item);
				if (item->RadioCallback)
					item->RadioCallback(item, newValue);
			}
			break;
		}
	}

	// the command was not processed
	return false;

	unguard;
}

UIMenu* UIMenuItem::GetOwner()
{
	UIMenuItem* item = this;
	while (item->Parent)
		item = item->Parent;
	return static_cast<UIMenu*>(item);
}

HMENU UIMenuItem::GetMenuHandle()
{
	return GetOwner()->GetHandle(false);
}

int UIMenuItem::GetMaxItemIdRecursive()
{
	int maxId = Id;
	for (UIMenuItem* item = FirstChild; item; item = item->NextChild)
	{
		int maxChildId = item->GetMaxItemIdRecursive();
		if (maxChildId > maxId)
			maxId = maxChildId;
	}
	return maxId;
}

void UIMenuItem::Update()
{
	guard(UIMenuItem::Update);

	HMENU hMenu = GetMenuHandle();
	if (!hMenu) return;

	switch (Type)
	{
	case MI_Checkbox:
		{
			bool value = GetCheckboxValue();
			CheckMenuItem(hMenu, Id, MF_BYCOMMAND | (value ? MF_CHECKED : 0));
		}
		break;
	case MI_RadioGroup:
		for (UIMenuItem* button = FirstChild; button; button = button->NextChild)
		{
			guard(RadioGroup);
			bool checked = (button->iValue == *(int*)pValue);
			CheckMenuItem(hMenu, button->Id, MF_BYCOMMAND | (checked ? MF_CHECKED : 0));
			unguardf("\"%s\"", *button->Label);
		}
		break;
	case MI_Submenu:
		for (UIMenuItem* item = FirstChild; item; item = item->NextChild)
		{
			item->Update();
		}
		break;
	}

	unguardf("\"%s\"", *Label);
}

void UIMenuItem::ReplaceWith(UIMenuItem* other)
{
	guard(UIMenuItem::ReplaceWith);

	// Both should be submenus
	assert(Type == MI_Submenu && other->Type == MI_Submenu);
	// This should be created, other - not
	assert(hMenu != NULL && Parent != NULL);
	assert(other->hMenu == NULL && other->Parent == NULL);

	// Destroy C++ objects
	DestroyChildren();

	// Destroy all menu items
	for (int i = GetMenuItemCount(hMenu) - 1; i >= 0; i--)
	{
		DeleteMenu(hMenu, i, MF_BYPOSITION);
	}

	// Now, move other's content into this menu
	FirstChild = other->FirstChild;
	other->FirstChild = NULL;
	for (UIMenuItem* item = FirstChild; item; item = item->NextChild)
	{
		item->Parent = this;
	}

	int nextId = GetOwner()->GetNextItemId();
	int submenuPosition = 0;
	FillMenuItems(hMenu, nextId, submenuPosition);

	if (!other->Label.IsEmpty())
	{
		SetName(*other->Label);
	}

	// Cleanup
	delete other;

	unguard;
}

bool UIMenuItem::GetCheckboxValue() const
{
	assert(Type == MI_Checkbox);
	if (iValue)
	{
		// bitfield
		return ( *(int*)pValue & iValue ) != 0;
	}
	else
	{
		// simple bool
		return *(bool*)pValue;
	}
}

void UIMenuItem::SetCheckboxValue(bool newValue)
{
	assert(Type == MI_Checkbox);
	if (iValue)
	{
		// bitfield
		int fullValue = *(int*)pValue;
		fullValue = fullValue & ~iValue | (newValue ? iValue : 0);
		*(int*)pValue = fullValue;
	}
	else
	{
		// simple bool
		*(bool*)pValue = newValue;
	}
}


/*-----------------------------------------------------------------------------
	UIMenu
-----------------------------------------------------------------------------*/

UIMenu::UIMenu()
:	UIMenuItem(MI_Submenu)
,	ReferenceCount(0)
,	MenuOwner(NULL)
,	MenuObject(NULL)
{}

UIMenu::~UIMenu()
{
	if (MenuObject) DestroyMenu(MenuObject);
}

void UIMenu::AttachTo(HWND Wnd, bool updateRefCount)
{
	assert(MenuOwner == NULL);
	MenuOwner = Wnd;
	if (updateRefCount)
	{
		ReferenceCount++;
	}
	SetMenu(MenuOwner, GetHandle(false, true));
	Redraw();
}

void UIMenu::Detach()
{
	if (MenuOwner)
	{
		SetMenu(MenuOwner, NULL);
		Redraw();
		MenuOwner = NULL;
	}
	if (--ReferenceCount == 0) delete this;
}

void UIMenu::Redraw()
{
	if (MenuOwner) DrawMenuBar(MenuOwner);
}

HMENU UIMenu::GetHandle(bool popup, bool forceCreate)
{
	if (!hMenu && forceCreate) Create(popup);
	return hMenu;
}

void UIMenu::Create(bool popup)
{
	guard(UIMenu::Create);

	assert(!hMenu);
	int nextId = FIRST_MENU_ID, position = 0;

	if (popup)
	{
		// TrackPopupMenu can't work with main menu, it requires a submenu handle.
		// Create dummy submenu to host all menu items. MenuObject will be a menu
		// owner here, and hMenu will be a menu itself.
		MenuObject = CreateMenu();
		hMenu = CreatePopupMenu();
		AppendMenu(MenuObject, MF_POPUP, (UINT_PTR)hMenu, "");
	}
	else
	{
		hMenu = MenuObject = CreateMenu();
	}

	FillMenuItems(hMenu, nextId, position);

	unguard;
}

int UIMenu::GetNextItemId()
{
	return max(GetMaxItemIdRecursive() + 1, FIRST_MENU_ID);
}

void UIMenu::Popup(UIElement* Owner, int x, int y)
{
	guard(UIMenu::Popup);

	if (BeforePopup)
	{
		BeforePopup(this, Owner);
	}

	// Create menu
	GetHandle(true, true);

	TPMPARAMS tpmParams;
	memset(&tpmParams, 0, sizeof(TPMPARAMS));
	tpmParams.cbSize = sizeof(TPMPARAMS);
//??	tpmParams.rcExclude = rectButton; - this will let some control to not be covered by menu (used for UIMenuButton before)

	int cmd = TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, x, y, Owner ? Owner->GetWnd() : NULL, &tpmParams);
	if (cmd)
	{
		HandleCommand(cmd);
	}

	unguard;
}
