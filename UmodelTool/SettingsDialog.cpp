#include "BaseDialog.h"
#include "SettingsDialog.h"
#include "FileControls.h"

#if HAS_UI

UISettingsDialog::UISettingsDialog(CUmodelSettings& settings)
:	Opt(settings)
,	OptRef(&settings)
{}

bool UISettingsDialog::Show()
{
	if (!ShowModal("Options", 480, -1))
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

	(*this)
	[
		NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		.SetWidth(EncodeWidth(1.0f))
		[
/*			NewControl(UIGroup, "Display")
			+*/ NewControl(UIGroup, "Export")
			.SetWidth(EncodeWidth(1.0f))
			[
				MakeExportOptions()
			]
		]
		+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
		[
			NewControl(UISpacer, -1)
			+ NewControl(UIButton, "OK")
			.SetWidth(EncodeWidth(0.2f))
			.SetOK()
			+ NewControl(UIButton, "Cancel")
			.SetWidth(EncodeWidth(0.2f))
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
					.AddItem("ActorX (psk)", EExportMeshFormat::psk)
					.AddItem("glTF 2.0", EExportMeshFormat::gltf)
					.AddItem("md5mesh", EExportMeshFormat::md5)
				+ NewControl(UISpacer)
				+ NewControl(UILabel, "Static Mesh:").SetY(4).SetAutoSize()
				+ NewControl(UICombobox, &Opt.Export.StaticMeshFormat)
					.AddItem("ActorX (pskx)", EExportMeshFormat::psk)
					.AddItem("glTF 2.0", EExportMeshFormat::gltf)
			]
			+ NewControl(UICheckbox, "Export LODs", &Opt.Export.ExportMeshLods)
		]
		+ NewControl(UICheckbox, "Export compressed textures to dds format", &Opt.Export.ExportDdsTexture)
		;
}

#endif // HAS_UI
