#ifndef __FILE_CONTROLS_H__
#define __FILE_CONTROLS_H__

#if HAS_UI

class UIFilePathEditor : public UIGroup
{
public:
	UIFilePathEditor();

protected:
	UITextEdit*		Editor;
	FString			Path;
	HWND			DlgWnd;

	virtual void Create(UIBaseDialog* dialog);
	virtual void AddCustomControls();
	void OnBrowseClicked(UIButton* sender);
};


#endif // HAS_UI

#endif // __FILE_CONTROLS_H__
