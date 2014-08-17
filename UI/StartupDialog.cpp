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
	return ShowDialog("Umodel Options", 300, 200);
}

void UIStartupDialog::InitUI()
{
	guard(UIStartupDialog::InitUI);

	UICheckboxGroup* overrideGame = new UICheckboxGroup("Override game detection", false);
	Add(overrideGame);
		UIGroup* gameSelectGroup = new UIGroup(GROUP_CUSTOM_LAYOUT);
		overrideGame->Add(gameSelectGroup);
			OverrideEngineCombo = new UICombobox();
			OverrideEngineCombo->SetCallback(BIND_MEM_CB(&UIStartupDialog::OnEngineChanges, this));
			OverrideEngineCombo->SetRect(0, 0, EncodeWidth(0.4f), -1);
			gameSelectGroup->Add(OverrideEngineCombo);
			OverrideGameCombo = new UICombobox();
			OverrideGameCombo->SetRect(EncodeWidth(0.42f), -1, -1, -1);
			gameSelectGroup->Add(OverrideGameCombo);

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
		UIGroup* classList1 = new UIGroup(GROUP_NO_BORDER);
		classList->Add(classList1);
		classList1->SetRect(0, 0, EncodeWidth(0.5f), -1);
			classList1->Add(new UICheckbox("Skeletal mesh", &UseSkeletalMesh));
			classList1->Add(new UICheckbox("Static mesh",   &UseStaticMesh));
			classList1->Add(new UICheckbox("Animation",     &UseAnimation));
			classList1->Add(new UICheckbox("Textures",      &UseTexture));
		UIGroup* classList2 = new UIGroup(GROUP_NO_BORDER);
		classList->Add(classList2);
		classList2->SetRect(EncodeWidth(0.5f), 0, -1, -1);
			classList2->Add(new UICheckbox("Sound",     &UseSound));
			classList2->Add(new UICheckbox("ScaleForm", &UseScaleForm));
			classList2->Add(new UICheckbox("FaceFX",    &UseFaceFX));

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

void UIStartupDialog::OnEngineChanges(UICombobox* sender, int value, const char* text)
{
	FillGameList();
}


#endif // HAS_UI
