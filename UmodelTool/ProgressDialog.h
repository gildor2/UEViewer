#ifndef __PROGRESS_DIALOG_H__
#define __PROGRESS_DIALOG_H__

#include "UnrealPackage/PackageUtils.h"	// for IProgressCallback

class UIProgressDialog : public UIBaseDialog, public IProgressCallback
{
public:
	void Show(const char* title);
	void SetDescription(const char* text);
	// IProgressCallback
	virtual bool Progress(const char* package, int index, int total);
	virtual bool Tick();

protected:
	const char*	DescriptionText;
	UILabel*	DescriptionLabel;
	UILabel*	PackageLabel;
	UILabel*	MemoryLabel;
	UILabel*	ObjectsLabel;
	UIProgressBar* ProgressBar;

	int			lastTick;

	virtual void InitUI();
};


#endif // __PROGRESS_DIALOG_H__
