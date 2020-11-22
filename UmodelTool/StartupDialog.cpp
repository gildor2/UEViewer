#if HAS_UI

#include "BaseDialog.h"
#include "StartupDialog.h"
#include "FileControls.h"

#include "GameDatabase.h"
#include "MiscStrings.h"
#include "AboutDialog.h"

UIStartupDialog::UIStartupDialog(CStartupSettings& settings)
:	Opt(settings)
{}

bool UIStartupDialog::Show()
{
	if (!ShowModal("UE Viewer Startup Options", 500, -1))
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
		+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UISpacer, -1)
			+ NewControl(UIHyperLink, "Game compatibility", "https://www.gildor.org/projects/umodel/compat")
			.SetAutoSize()
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

	static const int DefaultIndent = 20;

	NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
	.SetParent(this)
	.SetWidth(EncodeWidth(1.0f))
	[
		NewControl(UIGroup, "View / export object types")
		.SetWidth(EncodeWidth(0.6f))
		[
			NewControl(UIGroup, GROUP_NO_BORDER|GROUP_HORIZONTAL_LAYOUT)
			[
				NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(EncodeWidth(1.0f))
				[
					NewControl(UICheckbox, "Skeletal mesh", &Opt.UseSkeletalMesh)
					+ NewControl(UICheckbox, "Morph target", &Opt.UseMorphTarget)
					  .SetX(DefaultIndent)
					+ NewControl(UICheckbox, "Animation", &Opt.UseAnimation)
					+ NewControl(UICheckbox, "Static mesh", &Opt.UseStaticMesh)
				]
				+ NewControl(UIGroup, GROUP_NO_BORDER)
				.SetWidth(EncodeWidth(1.0f))
				[
					NewControl(UICheckbox, "Texture", &Opt.UseTexture)
					+ NewControl(UICheckbox, "Lightmap", &Opt.UseLightmapTexture)
					  .SetX(DefaultIndent)
					+ NewControl(UICheckbox, "Vertex mesh", &Opt.UseVertMesh)
				]
			]
		]
		+ NewControl(UIGroup, "Export-only types")
		.SetWidth(EncodeWidth(0.4f))
		[
			NewControl(UICheckbox, "Sound", &Opt.UseSound)
			+ NewControl(UICheckbox, "ScaleForm (UE3)", &Opt.UseScaleForm)
			+ NewControl(UICheckbox, "FaceFX (UE3)", &Opt.UseFaceFx)
		]
	];

	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UIGroup, "UE3 package compression", GROUP_HORIZONTAL_LAYOUT|GROUP_HORIZONTAL_SPACING)
			.SetWidth(EncodeWidth(1.0f))
			.SetRadioVariable(&Opt.PackageCompression)
			[
				NewControl(UIRadioButton, "Auto", 0)
				+ NewControl(UIRadioButton, "LZO", COMPRESS_LZO)
				+ NewControl(UIRadioButton, "zlib", COMPRESS_ZLIB)
				+ NewControl(UIRadioButton, "LZX", COMPRESS_LZX)
			]
			+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT)
			[
				NewControl(UILabel, "Platform:")
					.SetAutoSize()
					.SetY(4)
				+ NewControl(UICombobox, &Opt.Platform)
					.SetWidth(150)
					.AddItem("Auto", PLATFORM_UNKNOWN)
					.AddItem("PC", PLATFORM_PC)
					.AddItem("XBox360", PLATFORM_XBOX360)
					.AddItem("PS3", PLATFORM_PS3)
					.AddItem("PS4", PLATFORM_PS4)
					.AddItem("Nintendo Switch", PLATFORM_SWITCH)
					.AddItem("iOS", PLATFORM_IOS)
					.AddItem("Android", PLATFORM_ANDROID)
			]
		]
	];

	// dialog buttons
	NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
	.SetParent(this)
	[
		NewControl(UIHyperLink, GBuildString)
		.SetAutoSize()
		.SetY(3)
		.SetCallback(BIND_STATIC(&UIAboutDialog::Show))
		+ NewControl(UISpacer, -1)
		+ NewControl(UIButton, "OK")
		.SetWidth(80)
		.SetOK()
		+ NewControl(UIButton, "Cancel")
		.SetWidth(80)
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
