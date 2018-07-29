#ifndef __SETTINGS_DIALOG_H__
#define __SETTINGS_DIALOG_H__

#if HAS_UI

#include "UmodelSettings.h"

class UISettingsDialog : public UIBaseDialog
{
public:
	UISettingsDialog(CUmodelSettings& settings);

	bool Show();

protected:
	CUmodelSettings*	OptRef;
	CUmodelSettings		Opt;

	virtual void InitUI();

	UIElement& MakeExportOptions();
};

#endif // HAS_UI

#endif // __SETTINGS_DIALOG_H__
