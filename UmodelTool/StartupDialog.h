#ifndef __STARTUP_DIALOG_H__
#define __STARTUP_DIALOG_H__

#include "UmodelSettings.h"

class UIStartupDialog : public UIBaseDialog
{
public:
	UIStartupDialog(CStartupSettings& settings);

	bool Show();

protected:
	CStartupSettings&	Opt;

	UICheckboxGroup*	OverrideGameGroup;
	UICombobox*			OverrideEngineCombo;
	UICombobox*			OverrideGameCombo;

	void FillGameList();

	virtual void InitUI();
};

#endif // __STARTUP_DIALOG_H__
