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
	if (!ShowModal("Options", 360, 200))
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
		[
/*			NewControl(UIGroup, "Display")
			+*/ NewControl(UIGroup, "Export")
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
			+ NewControl(UISpacer)
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
			NewControl(UICheckbox, "Export skeletal mesh and animation to md5mesh/md5anim", &Opt.Export.ExportMd5Mesh)
			+ NewControl(UICheckbox, "Export LODs", &Opt.Export.ExportMeshLods)
		]
		+ NewControl(UICheckbox, "Export compressed textures to dds format", &Opt.Export.ExportDdsTexture)
		;
}

#endif // HAS_UI
