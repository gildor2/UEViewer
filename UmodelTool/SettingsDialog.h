#ifndef __SETTINGS_DIALOG_H__
#define __SETTINGS_DIALOG_H__

#include "UmodelSettings.h"

class UISettingsDialog : public UIBaseDialog
{
public:
	enum OptionsKind
	{
		OPT_Full,
		OPT_Export,
		OPT_Save,
	};

	UISettingsDialog(CUmodelSettings& settings, OptionsKind kind = OPT_Full);

	bool Show();

	static bool ShowExportOptions(CUmodelSettings& settings)
	{
		if (!settings.bShowExportOptions)
			return true;
		UISettingsDialog dialog(settings, OPT_Export);
		return dialog.Show();
	}

	static bool ShowSavePackagesOptions(CUmodelSettings& settings)
	{
		if (!settings.bShowSaveOptions)
			return true;
		UISettingsDialog dialog(settings, OPT_Save);
		return dialog.Show();
	}

protected:
	OptionsKind			Kind;
	CUmodelSettings*	OptRef;
	CUmodelSettings		Opt;

	virtual void InitUI();

	UIElement& MakeExportOptions();
	UIElement& MakeSavePackagesOptions();
	UIElement& MakeUIOptions();
};

#endif // __SETTINGS_DIALOG_H__
