#include "BaseDialog.h"

#if HAS_UI

#include "ProgressDialog.h"
#include "PackageUtils.h"

class UIPackageScanDialog : public UIBaseDialog
{
public:
	void Show()
	{
		UIProgressDialog progress;
		progress.Show("Scanning packages");
		progress.SetDescription("Scanning package");

		bool done = ScanPackageVersions(PkgInfo, &progress);
		progress.CloseDialog();

		if (done)
			ShowModal("Package version report", 400, 300);
	}

	virtual void InitUI()
	{
		UIMulticolumnListbox* listbox;

		// Create controls
		(*this)
		[
			NewControl(UIMulticolumnListbox, 4)
				.SetHeight(300)
				.Expose(listbox)
				.AddColumn("Ver", 60)
				.AddColumn("Lic", 60)
				.AddColumn("Count", 60)
				.AddColumn("Path")
			+ NewControl(UIGroup, GROUP_HORIZONTAL_LAYOUT|GROUP_NO_BORDER)
			[
				NewControl(UISpacer, -1)
				+ NewControl(UIButton, "Copy")
				.SetWidth(80)
				.SetCallback(BIND_MEMBER(&UIPackageScanDialog::CopyToClipboard, this))
				+ NewControl(UISpacer)
				+ NewControl(UIButton, "Close")
				.SetWidth(80)
				.SetOK()
				+ NewControl(UISpacer, -1)
			]
		];

		// Fill package information
		for (int i = 0; i < PkgInfo.Num(); i++)
		{
			const FileInfo &Info = PkgInfo[i];
			char buf[128];

			appSprintf(ARRAY_ARG(buf), "%d (%X)", Info.Ver, Info.Ver);
			int index = listbox->AddItem(buf);

			appSprintf(ARRAY_ARG(buf), "%d (%X)", Info.LicVer, Info.LicVer);
			listbox->AddSubItem(index, 1, buf);

			appSprintf(ARRAY_ARG(buf), "%d", Info.Count);
			listbox->AddSubItem(index, 2, buf);

			appSprintf(ARRAY_ARG(buf), "%s%s", Info.FileName, (Info.Count > 1) ? "..." : "");
			listbox->AddSubItem(index, 3, buf);
		}
	}

protected:
	void CopyToClipboard()
	{
		FStaticString<1024> Report;
		for (int i = 0; i < PkgInfo.Num(); i++)
		{
			char buf[256];
			const FileInfo &Info = PkgInfo[i];
			appSprintf(ARRAY_ARG(buf), "%3d (%3X)  %3d (%3X)  %4d    %s%s\n",
				Info.Ver, Info.Ver, Info.LicVer, Info.LicVer, Info.Count, Info.FileName,
				Info.Count > 1 && Info.FileName[0] ? "..." : "");
			Report += buf;
		}
		appCopyTextToClipboard(*Report);
	}

	TArray<FileInfo>	PkgInfo;
};


void ShowPackageScanDialog()
{
	UIPackageScanDialog dialog;
	dialog.Show();
}

#endif // HAS_UI
