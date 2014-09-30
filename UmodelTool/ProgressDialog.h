#ifndef __PROGRESS_DIALOG_H__
#define __PROGRESS_DIALOG_H__

class UIProgressDialog : public UIBaseDialog
{
public:
	void Show(const char* title);
	void SetDescription(const char* text);
	bool Progress(const char* package, int index, int total);
	bool Tick();

protected:
	const char*	DescriptionText;
	UILabel*	DescriptionLabel;
	UILabel*	PackageLabel;
	UILabel*	MemoryLabel;
	UILabel*	ObjectsLabel;
	UIProgressBar* ProgressBar;

	virtual void InitUI();
};


#endif // __PROGRESS_DIALOG_H__
