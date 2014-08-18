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
	return ShowDialog("Umodel Startup Options", 300, 180);
}

void UIStartupDialog::InitUI()
{
	guard(UIStartupDialog::InitUI);

	NewControl(UICheckboxGroup, "Override game detection", false)
	.SetParent(this)
	[
		NewControl(UIGroup, GROUP_CUSTOM_LAYOUT)
		[
			NewControl(UICombobox)
			.Expose(OverrideEngineCombo)
			.SetCallback(BIND_MEM_CB(&UIStartupDialog::OnEngineChanged, this))
			.SetRect(0, 0, EncodeWidth(0.4f), -1)
			+ NewControl(UICombobox)
			.Expose(OverrideGameCombo)
			.SetRect(EncodeWidth(0.42f), -1, -1, -1)
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

	Add(new UILabel("Path to game files:"));
	UIFilePathEditor* pathEditor = new UIFilePathEditor();
	Add(pathEditor);

	UIGroup* classList = new UIGroup("Engine classed to load");
	Add(classList);
	(*classList)
	[
		NewControl(UIGroup, GROUP_NO_BORDER)
		.SetRect(0, 0, EncodeWidth(0.5f), -1)
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
		.SetRect(EncodeWidth(0.5f), 0, -1, -1)
		[
			NewControl(UILabel, "Export-only classes:")
			.SetHeight(20)
			+ NewControl(UICheckbox, "Sound",     &UseSound)
			+ NewControl(UICheckbox, "ScaleForm", &UseScaleForm)
			+ NewControl(UICheckbox, "FaceFX",    &UseFaceFX)
		]
	];

	//!! platform
	//!! compression method
	//!! save log to file (-log=...)

#if 0 // test code
	UIGroup* testGroup1 = new UIGroup("Group #1");
	UIGroup* testGroup2 = new UIGroup("Group #2");
	UIGroup* testGroup3 = new UIGroup("Group #3");
	Add(testGroup1);
	testGroup1->Add(testGroup2);
	Add(testGroup3);

	UILabel* label1 = new UILabel("Test label 1", TA_Left);
	testGroup1->Add(label1);

	UIButton* button1 = new UIButton("Button 1");
//	button1->SetCallback(BIND_FREE_CB(&PrintVersionInfoCB));
	testGroup1->Add(button1);

	UICheckbox* checkbox1 = new UICheckbox("Checkbox 1", true);
	testGroup1->Add(checkbox1);

	UILabel* label2 = new UILabel("Test label 2", TA_Right);
	testGroup2->Add(label2);

	UILabel* label3 = new UILabel("Test label 3", TA_Center);
	testGroup3->Add(label3);

	static FString str;
	str = "test text";
	UITextEdit* textEdit = new UITextEdit(&str);
	testGroup3->Add(textEdit);
#endif

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
