#include "BaseDialog.h"
#include "StartupDialog.h"
#include "FileControls.h"

#include "../GameList.h"

#if HAS_UI

UIStartupDialog::UIStartupDialog()
:	GameOverride(-1)
,	UseSkeletalMesh(true)
,	UseStaticMesh(true)
,	UseAnimation(true)
,	UseTexture(true)
,	UseSound(false)
,	UseScaleForm(false)
,	UseFaceFX(false)
{}

bool UIStartupDialog::Show()
{
	return ShowDialog("Umodel Startup Options", 320, 200);
}

void UIStartupDialog::InitUI()
{
	guard(UIStartupDialog::InitUI);

	(*this)
	[
		NewControl(UILabel, "Path to game files:")
		+ NewControl(UIFilePathEditor)
	];

	NewControl(UICheckboxGroup, "Override game detection", false)
	.SetParent(this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT)
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

	int i;
	const char* lastEngine = NULL;
	for (i = 0; /* empty */; i++)
	{
		const GameInfo &info = GListOfGames[i];
		if (!info.Name) break;
		const char* engine = GetEngineName(info.Enum);
		if (engine != lastEngine)
		{
			lastEngine = engine;
			OverrideEngineCombo->AddItem(engine);
		}
	}
#if 0
	OverrideEngineCombo->SelectItem("Unreal engine 3"); // this engine has most number of game titles
	FillGameList();
#endif

	(*this)
	[
		NewControl(UIGroup, "Engine classed to load", GROUP_HORIZONTAL_LAYOUT)
		[
			NewControl(UIGroup, GROUP_NO_BORDER)
			[
				NewControl(UILabel, "Common classes:")
				.SetHeight(20)
				+ NewControl(UICheckbox, "Skeletal mesh", &UseSkeletalMesh)
				+ NewControl(UICheckbox, "Static mesh",   &UseStaticMesh)
				+ NewControl(UICheckbox, "Animation",     &UseAnimation)
				+ NewControl(UICheckbox, "Textures",      &UseTexture)
				//!! lightmap
			]
			+ NewControl(UIGroup, GROUP_NO_BORDER)
			[
				NewControl(UILabel, "Export-only classes:")
				.SetHeight(20)
				+ NewControl(UICheckbox, "Sound",     &UseSound)
				+ NewControl(UICheckbox, "ScaleForm", &UseScaleForm)
				+ NewControl(UICheckbox, "FaceFX",    &UseFaceFX)
			]
		]
	];

	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UIGroup, "Package compression", GROUP_HORIZONTAL_LAYOUT|GROUP_HORIZONTAL_SPACING)
			.SetWidth(EncodeWidth(0.4f))
			[
				NewControl(UIRadioButton, "Auto")
				+ NewControl(UIRadioButton, "LZO")
				+ NewControl(UIRadioButton, "zlib")
				+ NewControl(UIRadioButton, "LZX")
			]
			+ NewControl(UISpacer)
			+ NewControl(UIGroup, "Platform", GROUP_HORIZONTAL_LAYOUT|GROUP_HORIZONTAL_SPACING)
			[
				NewControl(UIRadioButton, "Auto")
				+ NewControl(UIRadioButton, "PC")
				+ NewControl(UIRadioButton, "XBox360")
				+ NewControl(UIRadioButton, "PS3")
				+ NewControl(UIRadioButton, "iOS")
			]
		]
	];

	//!! save log to file (-log=...)

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
		// sort items, keep 1st item (engine name) first
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
