#if HAS_UI

#include "BaseDialog.h"
#include "SettingsDialog.h"
#include "FileControls.h"


UISettingsDialog::UISettingsDialog(CUmodelSettings& settings, OptionsKind kind)
:	Kind(kind)
,	Opt(settings)
,	OptRef(&settings)
{}

bool UISettingsDialog::Show()
{
	const char* title = "Options";
	if (Kind == OPT_Export)
		title = "Export options";
	else if (Kind == OPT_Save)
		title = "Save options";

	if (!ShowModal(title, -1, -1))
		return false;

	*OptRef = Opt;

	// Update export path (this call will handle relative and absolute paths)
	OptRef->Export.Apply();

	// Save settings
	OptRef->Save();

	return true;
}

void UISettingsDialog::InitUI()
{
	guard(UISettingsDialog::InitUI);

	// Show a single page if requested
	if (Kind != OPT_Full)
	{
		UIElement* controlList = NULL;
		bool* showWindowOpt = NULL;
		if (Kind == OPT_Export)
		{
			controlList = &MakeExportOptions();
			showWindowOpt = &Opt.bShowExportOptions;
		}
		else if (Kind == OPT_Save)
		{
			controlList = &MakeSavePackagesOptions();
			showWindowOpt = &Opt.bShowSaveOptions;
		}
		else
		{
			appError("UISettingsDialog: unknown 'kind'");
		}

		assert(*showWindowOpt);
		(*this)
		[
			*controlList
			+ NewControl(UISpacer)
				.SetHeight(10)
			+ NewControl(UIHorizontalLine)
			+ NewControl(UISpacer)
				.SetHeight(10)
			+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			[
				NewControl(UICheckbox, "Don't show this dialog window again", showWindowOpt)
					.InvertValue()
				+ NewControl(UISpacer, -1)
				+ NewControl(UIButton, "OK")
					.SetWidth(80)
					.SetOK()
				+ NewControl(UIButton, "Cancel")
					.SetWidth(80)
					.SetCancel()
			]
		];
		return;
	}

	// Show full options window
	(*this)
	[
		NewControl(UITabControl)
		.SetWidth(EncodeWidth(1.0f))
		[
/*			NewControl(UIGroup, "Display", GROUP_NO_BORDER)
			+*/
			NewControl(UIGroup, "Export", GROUP_NO_BORDER)
			.SetWidth(EncodeWidth(1.0f))
			[
				MakeExportOptions()
			]
			+ NewControl(UIGroup, "Save packages", GROUP_NO_BORDER)
			[
				MakeSavePackagesOptions()
			]
			+ NewControl(UIGroup, "Interface", GROUP_NO_BORDER)
			[
				MakeUIOptions()
			]
		]
		+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UISpacer, -1)
			+ NewControl(UIButton, "OK")
				.SetWidth(80)
				.SetOK()
			+ NewControl(UIButton, "Cancel")
				.SetWidth(80)
				.SetCancel()
		]
	];

	unguard;
}

UIElement& UISettingsDialog::MakeExportOptions()
{
	return
		NewControl(UIGroup, "File Layout")
		[
			NewControl(UILabel, "Export to this folder")
			+ NewControl(UIFilePathEditor, &Opt.Export.ExportPath)
			+ NewControl(UICheckbox, "Use object groups instead of types", &Opt.Export.SaveGroups)
			+ NewControl(UICheckbox, "Use uncooked UE3 package names", &Opt.Export.SaveUncooked)
		]
		+ NewControl(UIGroup, "Mesh Export")
		[
			NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			[
				NewControl(UILabel, "Skeletal Mesh:").SetY(4).SetAutoSize()
				+ NewControl(UICombobox, &Opt.Export.SkeletalMeshFormat)
					.SetWidth(100)
					.AddItem("ActorX (psk)", EExportMeshFormat::psk)
					.AddItem("glTF 2.0", EExportMeshFormat::gltf)
					.AddItem("md5mesh", EExportMeshFormat::md5)
				+ NewControl(UISpacer)
				+ NewControl(UILabel, "Static Mesh:").SetY(4).SetAutoSize()
				+ NewControl(UICombobox, &Opt.Export.StaticMeshFormat)
					.SetWidth(100)
					.AddItem("ActorX (pskx)", EExportMeshFormat::psk)
					.AddItem("glTF 2.0", EExportMeshFormat::gltf)
			]
			+ NewControl(UICheckbox, "Export LODs", &Opt.Export.ExportMeshLods)
		]
		+ NewControl(UIGroup, "Texture Export")
		[
			NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			[
				NewControl(UILabel, "Texture format:").SetY(4).SetAutoSize()
				+ NewControl(UICombobox, &Opt.Export.TextureFormat)
					.SetWidth(120)
					.AddItem("TGA", ETextureExportFormat::tga)
					.AddItem("TGA (uncompressed)", ETextureExportFormat::tga_uncomp)
					.AddItem("PNG", ETextureExportFormat::png)
			]
			+ NewControl(UICheckbox, "Export compressed textures to dds format", &Opt.Export.ExportDdsTexture)
		]
		+ NewControl(UICheckbox, "Don't overwrite already exported files", &Opt.Export.DontOverwriteFiles)
		;
}

UIElement& UISettingsDialog::MakeSavePackagesOptions()
{
	return NewControl(UIGroup, "File Layout")
		[
			NewControl(UILabel, "Save to this folder")
			+ NewControl(UIFilePathEditor, &Opt.SavePackages.SavePath)
			+ NewControl(UICheckbox, "Keep directory structure", &Opt.SavePackages.KeepDirectoryStructure)
		];
}

UIElement& UISettingsDialog::MakeUIOptions()
{
	return
		NewControl(UIGroup, "Confirmations")
		[
			NewControl(UICheckbox, "Popup options on export", &Opt.bShowExportOptions)
			+ NewControl(UICheckbox, "Popup options on save", &Opt.bShowSaveOptions)
		]
	;
}

#endif // HAS_UI
