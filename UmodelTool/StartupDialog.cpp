#include "BaseDialog.h"
#include "StartupDialog.h"
#include "FileControls.h"

#include "GameDatabase.h"

#if HAS_UI

UIStartupDialog::UIStartupDialog(UmodelSettings& settings)
:	Opt(settings)
{}

bool UIStartupDialog::Show()
{
	if (!ShowModal("Umodel Startup Options", 320, 200))
		return false;

	// process some options

	// GameOverride
	Opt.GameOverride = GAME_UNKNOWN;
	if (OverrideGameGroup->IsChecked() && (OverrideGameCombo->GetSelectionIndex() >= 0))
	{
		const char* gameName = OverrideGameCombo->GetSelectionText();
		for (int i = 0; /* empty */; i++)
		{
			const GameInfo &info = GListOfGames[i];
			if (!info.Name) break;
			if (!strcmp(info.Name, gameName))
			{
				Opt.GameOverride = info.Enum;
				break;
			}
		}
	}

	return true;
}

void UIStartupDialog::InitUI()
{
	guard(UIStartupDialog::InitUI);

	(*this)
	[
		NewControl(UILabel, "Path to game files:")
		+ NewControl(UIFilePathEditor, &Opt.GamePath)
		//!! could check file extensions in target directory and at least
		//!! set "engine" part of "game override", if it is not set yet
	];

	NewControl(UICheckboxGroup, "Override game detection", Opt.GameOverride != GAME_UNKNOWN)
	.SetParent(this)
	.Expose(OverrideGameGroup)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UICombobox)
			.Expose(OverrideEngineCombo)
			.SetCallback(BIND_MEM_CB(&UIStartupDialog::OnEngineChanged, this))
			.SetWidth(EncodeWidth(0.4f))
			+ NewControl(UISpacer)
			+ NewControl(UICombobox)
			.Expose(OverrideGameCombo)
		]
	];

	// fill engines list
	int i;
	const char* lastEngine = NULL;
	const GameInfo* selectedGame = NULL;
	for (i = 0; /* empty */; i++)
	{
		const GameInfo &info = GListOfGames[i];
		if (!info.Name) break;
		if (info.Enum == Opt.GameOverride) selectedGame = &info;
		const char* engine = GetEngineName(info.Enum);
		if (engine != lastEngine)
		{
			lastEngine = engine;
			OverrideEngineCombo->AddItem(engine);
		}
	}

	// select a game passed through the command line
	if (selectedGame)
	{
		OverrideEngineCombo->SelectItem(GetEngineName(selectedGame->Enum));
		FillGameList();
		OverrideGameCombo->SelectItem(selectedGame->Name);
	}

#if 0
	OverrideEngineCombo->SelectItem("Unreal engine 3"); // this engine has most number of game titles
	FillGameList();
#endif

	NewControl(UIGroup, "Engine classes to load", GROUP_HORIZONTAL_LAYOUT)
	.SetParent(this)
	[
		NewControl(UIGroup, GROUP_NO_BORDER)
		[
			NewControl(UILabel, "Common classes:")
			+ NewControl(UICheckbox, "Skeletal mesh", &Opt.UseSkeletalMesh)
			+ NewControl(UICheckbox, "Static mesh",   &Opt.UseStaticMesh)
			+ NewControl(UICheckbox, "Animation",     &Opt.UseAnimation)
			+ NewControl(UICheckbox, "Textures",      &Opt.UseTexture)
			+ NewControl(UICheckbox, "Lightmaps",     &Opt.UseLightmapTexture)
		]
		+ NewControl(UIGroup, GROUP_NO_BORDER)
		[
			NewControl(UILabel, "Export-only classes:")
			+ NewControl(UICheckbox, "Sound",     &Opt.UseSound)
			+ NewControl(UICheckbox, "ScaleForm", &Opt.UseScaleForm)
			+ NewControl(UICheckbox, "FaceFX",    &Opt.UseFaceFx)
		]
	];

	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UIGroup, "Package compression", GROUP_HORIZONTAL_LAYOUT|GROUP_HORIZONTAL_SPACING)
			.SetWidth(EncodeWidth(0.4f))
			.SetRadioVariable(&Opt.PackageCompression)
			[
				NewControl(UIRadioButton, "Auto", 0)
				+ NewControl(UIRadioButton, "LZO", COMPRESS_LZO)
				+ NewControl(UIRadioButton, "zlib", COMPRESS_ZLIB)
				+ NewControl(UIRadioButton, "LZX", COMPRESS_LZX)
			]
			+ NewControl(UISpacer)
			+ NewControl(UIGroup, "Platform", GROUP_HORIZONTAL_LAYOUT|GROUP_HORIZONTAL_SPACING)
			.SetRadioVariable(&Opt.Platform)
			[
				NewControl(UIRadioButton, "Auto", PLATFORM_UNKNOWN)
				+ NewControl(UIRadioButton, "PC", PLATFORM_PC)
				+ NewControl(UIRadioButton, "XBox360", PLATFORM_XBOX360)
				+ NewControl(UIRadioButton, "PS3", PLATFORM_PS3)
				+ NewControl(UIRadioButton, "iOS", PLATFORM_IOS)
			]
		]
	];

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UISpacer, -1)
		+ NewControl(UIButton, "OK")
		.SetWidth(EncodeWidth(0.2f))
		.SetOK()
		+ NewControl(UISpacer)
		+ NewControl(UIButton, "Cancel")
		.SetWidth(EncodeWidth(0.2f))
		.SetCancel()
	];

	//!! - possibility to select a file to open, setup game path from it,
	//!!   set file mask from known file extensions
	//!! - save log to file (-log=...)
	//!! - about/help

	unguard;
}

static int CompareStrings(const char** a, const char** b)
{
	return stricmp(*a, *b);
}

// Fill list of game titles made with selected engine
void UIStartupDialog::FillGameList()
{
	OverrideGameCombo->RemoveAllItems();
	const char* selectedEngine = OverrideEngineCombo->GetSelectionText();

	TArray<const char*> gameNames;
	int numEngineEntries = 0;
	int i;

	for (i = 0; /* empty */; i++)
	{
		const GameInfo &info = GListOfGames[i];
		if (!info.Name) break;
		const char* engine = GetEngineName(info.Enum);
		if (!strcmp(engine, selectedEngine))
		{
			gameNames.AddItem(info.Name);
			if (!strnicmp(info.Name, "Unreal engine ", 14))
				numEngineEntries++;
		}
	}
	if (gameNames.Num() > numEngineEntries + 1)
	{
		// sort items, keep 1st numEngineEntries items (engine name) in place
		QSort(&gameNames[numEngineEntries], gameNames.Num() - numEngineEntries, CompareStrings);
	}
	for (i = 0; i < gameNames.Num(); i++)
		OverrideGameCombo->AddItem(gameNames[i]);

	// select engine item
	OverrideGameCombo->SelectItem(0);
}

void UIStartupDialog::OnEngineChanged(UICombobox* sender, int value, const char* text)
{
	FillGameList();
}


#endif // HAS_UI
