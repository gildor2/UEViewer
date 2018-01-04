// Simple UI library.
// Copyright (C) 2018 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

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

	FORCEINLINE const FString& GetInitialDirectory() const
	{
		return InitialDirectory;
	}

	FORCEINLINE const FString& GetPath() const
	{
		return *Path;
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

FString ShowFileSelectionDialog(
	bool bIsSaveDialog,
	UIBaseDialog* ParentDialog,
	const FString& InitialFilename,
	const FString& InitialDirectory,
	const FString& Title,
	const TArray<FString>& Filters);


#endif // HAS_UI

#endif // __FILE_CONTROLS_H__
