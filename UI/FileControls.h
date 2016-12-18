#ifndef __FILE_CONTROLS_H__
#define __FILE_CONTROLS_H__

#if HAS_UI

class UIFilePathEditor : public UIGroup
{
	DECLARE_UI_CLASS(UIFilePathEditor, UIGroup);
public:
	UIFilePathEditor(FString* path);

	FORCEINLINE UIFilePathEditor& SetTitle(const char* InTitle)
	{
		Title = InTitle;
		return *this;
	}

protected:
	UITextEdit*		Editor;
	FString*		Path;
	FString			Title;
	HWND			DlgWnd;

	virtual void Create(UIBaseDialog* dialog);
	virtual void AddCustomControls();
	void OnBrowseClicked(UIButton* sender);
};


class UIFileNameEditor : public UIGroup
{
	DECLARE_UI_CLASS(UIFileNameEditor, UIGroup);
public:
	UIFileNameEditor(FString* path);

	FORCEINLINE UIFileNameEditor& SetTitle(const char* InTitle)
	{
		Title = InTitle;
		return *this;
	}

	FORCEINLINE UIFileNameEditor& SetInitialDirectory(const char* Directory)
	{
		InitialDirectory = Directory;
		return *this;
	}

	FORCEINLINE UIFileNameEditor& UseLoadDialog()
	{
		bIsSaveDialog = false;
		return *this;
	}

	FORCEINLINE UIFileNameEditor& UseSaveDialog()
	{
		bIsSaveDialog = true;
		return *this;
	}

	FORCEINLINE UIFileNameEditor& AddFilter(const char* Filter)
	{
		Filters.Add(Filter);
		return *this;
	}

protected:
	UITextEdit*		Editor;
	FString*		Path;
	HWND			DlgWnd;
	bool			bIsSaveDialog;
	FString			Title;
	FString			InitialDirectory;
	TArray<FString>	Filters;

	virtual void Create(UIBaseDialog* dialog);
	virtual void AddCustomControls();
	void OnBrowseClicked(UIButton* sender);
};


#endif // HAS_UI

#endif // __FILE_CONTROLS_H__
