#include "BaseDialog.h"
#include "SettingsDialog.h"
#include "FileControls.h"

#if HAS_UI

UISettingsDialog::UISettingsDialog(CUmodelSettings& settings)
:	Opt(settings)
,	OptRef(&settings)
{}

//!! TODO: replace functions with arrays

namespace SkelMeshFmt
{
	static const char* Names[] = { "ActorX (psk)", "md5mesh", NULL };

	int EnumToIndex(EExportMeshFormat Fmt)
	{
		switch (Fmt)
		{
		case EExportMeshFormat::psk:
			return 0;
		case EExportMeshFormat::md5:
			return 1;
		}
		return -1;
	}

	EExportMeshFormat IndexToEnum(int Index)
	{
		switch (Index)
		{
		case 0:
		default:
			return EExportMeshFormat::psk;
		case 1:
			return EExportMeshFormat::md5;
		}
	}
}

namespace StatMeshFmt
{
	static const char* Names[] = { "ActorX (pskx)", "glTF 2.0", NULL };

	int EnumToIndex(EExportMeshFormat Fmt)
	{
		switch (Fmt)
		{
		case EExportMeshFormat::psk:
			return 0;
		case EExportMeshFormat::gltf:
			return 1;
		}
		return -1;
	}

	EExportMeshFormat IndexToEnum(int Index)
	{
		switch (Index)
		{
		case 0:
		default:
			return EExportMeshFormat::psk;
		case 1:
			return EExportMeshFormat::gltf;
		}
	}
}

bool UISettingsDialog::Show()
{
	if (!ShowModal("Options", 480, -1))
		return false;

	Opt.Export.SkeletalMeshFormat = SkelMeshFmt::IndexToEnum(SkelMeshFormatCombo->GetSelectionIndex());
	Opt.Export.StaticMeshFormat = StatMeshFmt::IndexToEnum(StatMeshFormatCombo->GetSelectionIndex());

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
				+ NewControl(UICombobox)
					.AddItems(SkelMeshFmt::Names)
					.SelectItem(SkelMeshFmt::EnumToIndex(Opt.Export.SkeletalMeshFormat))
					.Expose(SkelMeshFormatCombo)
				+ NewControl(UISpacer)
				+ NewControl(UILabel, "Static Mesh:").SetY(4).SetAutoSize()
				+ NewControl(UICombobox)
					.AddItems(StatMeshFmt::Names)
					.SelectItem(SkelMeshFmt::EnumToIndex(Opt.Export.StaticMeshFormat))
					.Expose(StatMeshFormatCombo)
			]
			+ NewControl(UICheckbox, "Export LODs", &Opt.Export.ExportMeshLods)
		]
		+ NewControl(UICheckbox, "Export compressed textures to dds format", &Opt.Export.ExportDdsTexture)
		;
}

#endif // HAS_UI
