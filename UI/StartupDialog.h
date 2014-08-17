#ifndef __STARTUP_DIALOG_H__
#define __STARTUP_DIALOG_H__

#if HAS_UI

class UIStartupDialog : public UIBaseDialog
{
public:
	UIStartupDialog();

	bool Show();
	virtual void InitUI();

	int		GameOverride;

	bool			UseSkeletalMesh;
	bool			UseStaticMesh;
	bool			UseAnimation;
	bool			UseTexture;
	bool			UseSound;
	bool			UseScaleForm;
	bool			UseFaceFX;

protected:
	UICombobox*		OverrideEngineCombo;
	UICombobox*		OverrideGameCombo;

	void FillGameList();
	void OnEngineChanges(UICombobox* sender, int value, const char* text);
};

#endif // HAS_UI

#endif // __STARTUP_DIALOG_H__
