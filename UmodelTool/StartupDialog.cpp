#include "BaseDialog.h"
#include "StartupDialog.h"
#include "FileControls.h"

#include "GameDatabase.h"

#if HAS_UI

UIStartupDialog::UIStartupDialog(CStartupSettings& settings)
:	Opt(settings)
{}

bool UIStartupDialog::Show()
{
	if (!ShowModal("Umodel Startup Options", -1, -1))
		return false;

	// process some options

	// GameOverride
	int selectedGame = OverrideGameCombo->GetSelectionIndex();
	if (!OverrideGameGroup->IsChecked() || (selectedGame < 0))
	{
		Opt.GameOverride = GAME_UNKNOWN;
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
			.SetCallback(BIND_MEMBER(&UIStartupDialog::FillGameList, this))
			.SetWidth(EncodeWidth(0.4f))
			+ NewControl(UICombobox, &Opt.GameOverride)
			.Expose(OverrideGameCombo)
		]
	];

	// fill engines list
	int i;
	const char* lastEngine = NULL;
	for (i = 0; /* empty */; i++)
	{
		const GameInfo &info = GListOfGames[i];
		if (!info.Name) break;		// end of list marker
		const char* engine = GetEngineName(info.Enum);
		if (engine != lastEngine)
		{
			lastEngine = engine;
			OverrideEngineCombo->AddItem(engine);
		}
	}

	// select a game passed through the command line
	if (Opt.GameOverride != GAME_UNKNOWN)
	{
		OverrideEngineCombo->SelectItem(GetEngineName(Opt.GameOverride));
		FillGameList();
	}

	NewControl(UIGroup, "Engine classes to load", GROUP_HORIZONTAL_LAYOUT)
	.SetParent(this)
	[
		NewControl(UIGroup, GROUP_NO_BORDER)
		.SetWidth(EncodeWidth(0.5f))
		[
			NewControl(UILabel, "Common classes:")
			+ NewControl(UICheckbox, "Skeletal mesh", &Opt.UseSkeletalMesh)
			+ NewControl(UICheckbox, "Static mesh",   &Opt.UseStaticMesh)
			+ NewControl(UICheckbox, "Animation",     &Opt.UseAnimation)
			+ NewControl(UICheckbox, "Textures",      &Opt.UseTexture)
			+ NewControl(UICheckbox, "Lightmaps",     &Opt.UseLightmapTexture)
		]
		+ NewControl(UIGroup, GROUP_NO_BORDER)
		.SetWidth(EncodeWidth(0.5f))
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
			.SetRadioVariable(&Opt.PackageCompression)
			[
				NewControl(UIRadioButton, "Auto", 0)
				+ NewControl(UIRadioButton, "LZO", COMPRESS_LZO)
				+ NewControl(UIRadioButton, "zlib", COMPRESS_ZLIB)
				+ NewControl(UIRadioButton, "LZX", COMPRESS_LZX)
			]
			+ NewControl(UIGroup, "Platform", GROUP_HORIZONTAL_LAYOUT|GROUP_HORIZONTAL_SPACING)
			.SetRadioVariable(&Opt.Platform)
			[
				NewControl(UIRadioButton, "Auto", PLATFORM_UNKNOWN)
				+ NewControl(UIRadioButton, "PC", PLATFORM_PC)
				+ NewControl(UIRadioButton, "XBox360", PLATFORM_XBOX360)
				+ NewControl(UIRadioButton, "PS3", PLATFORM_PS3)
				+ NewControl(UIRadioButton, "PS4", PLATFORM_PS4)
				+ NewControl(UIRadioButton, "iOS", PLATFORM_IOS)
				+ NewControl(UIRadioButton, "Android", PLATFORM_ANDROID)
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

static int CompareGames(const GameInfo* const* a, const GameInfo* const* b)
{
	return stricmp((*a)->Name, (*b)->Name);
}

// Fill list of game titles made with selected engine
void UIStartupDialog::FillGameList()
{
	guard(FillGameList);

	OverrideGameCombo->RemoveAllItems();
	const char* selectedEngine = OverrideEngineCombo->GetSelectionText();

	TStaticArray<const GameInfo*, 256> SelectedGameInfos;

	int numEngineEntries = 0;
	int i;

	for (i = 0; /* empty */; i++)
	{
		const GameInfo &info = GListOfGames[i];
		if (!info.Name) break;
		const char* engine = GetEngineName(info.Enum);
		if (!strcmp(engine, selectedEngine))
		{
			if (!strnicmp(info.Name, "Unreal engine ", 14))
				numEngineEntries++;
			SelectedGameInfos.Add(&info);
		}
	}
	if (SelectedGameInfos.Num() > numEngineEntries + 1)
	{
		// sort items, keep 1st numEngineEntries items (engine name) in place
		QSort<const GameInfo*>(&SelectedGameInfos[numEngineEntries], SelectedGameInfos.Num() - numEngineEntries, CompareGames);
	}

	for (i = 0; i < SelectedGameInfos.Num(); i++)
	{
#if UNREAL4
		if (i == 0 && !stricmp(selectedEngine, "Unreal engine 4"))
		{
			// Special case for UE4 - we should generate engine names and enums
			for (int ue4ver = 0; ue4ver <= LATEST_SUPPORTED_UE4_VERSION; ue4ver++)
			{
				char buf[128];
				appSprintf(ARRAY_ARG(buf), "Unreal engine 4.%d", ue4ver);
				OverrideGameCombo->AddItem(buf, GAME_UE4(ue4ver));
			}
			continue;
		}
#endif // UNREAL4
		OverrideGameCombo->AddItem(SelectedGameInfos[i]->Name, SelectedGameInfos[i]->Enum);
	}

	// select engine item
	OverrideGameCombo->SelectItem(0);

	unguard;
}


#endif // HAS_UI
