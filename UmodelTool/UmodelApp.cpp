#include "Core.h"

#if RENDERING

#include <SDL2/SDL_syswm.h>			// for SDL_SysWMinfo, does <windows.h> includes
#undef UnregisterClass
#undef GetClassName

#include "UnCore.h"
#include "UnObject.h"
#include "PackageUtils.h"
#include "UnPackage.h"

#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMesh/UnMesh2.h"
#include "UnrealMesh/UnMesh3.h"
#include "UnrealMesh/UnMesh4.h"

#include "UmodelApp.h"
#include "UmodelCommands.h"
#include "UmodelSettings.h"

#include "Exporters/Exporters.h"	// for WriteTGA

#if HAS_UI
#include "BaseDialog.h"
#include "PackageDialog.h"
#include "StartupDialog.h"
#include "ProgressDialog.h"
#include "AboutDialog.h"
#include "ErrorDialog.h"
#include "UE4VersionDialog.h"
#include "UE4AesKeyDialog.h"
#include "PackageScanDialog.h"
#include "SettingsDialog.h"
#endif


CUmodelApp GApplication;

#define SCREENSHOTS_DIR		"Screenshots"

#if MAX_DEBUG
extern bool GShowShaderParams;
#endif

/*-----------------------------------------------------------------------------
	Object visualizer support
-----------------------------------------------------------------------------*/

bool CUmodelApp::FindObjectAndCreateVisualizer(int dir, bool forceVisualizer, bool newPackage)
{
	guard(CUmodelApp::FindObjectAndCreateVisualizer);

	if (newPackage)
	{
		assert(dir > 0); // just in case
		ObjIndex = -1;
		BrowseHistory.Empty();
		CurrentHistoryItem = 0;
	}

	int looped = 0;
	UObject *Obj;
	while (true)
	{
		if (dir > 0)
		{
			ObjIndex++;
			if (ObjIndex >= UObject::GObjObjects.Num())
			{
				ObjIndex = 0;
				looped++;
			}
		}
		else
		{
			ObjIndex--;
			if (ObjIndex < 0)
			{
				ObjIndex = UObject::GObjObjects.Num()-1;
				looped++;
			}
		}
		if (looped > 1 || UObject::GObjObjects.Num() == 0)
		{
			if (forceVisualizer)
			{
				CreateVisualizer(NULL);
			#if HAS_MENU
				UpdateObjectMenu();
			#endif
				if (GFullyLoadedPackages.Num())
				{
					appPrintf("\nThe specified package(s) has no supported objects.\n\n");
					DisplayPackageStats(GFullyLoadedPackages);
				}
				return true;
			}
			return false;
		}
		Obj = UObject::GObjObjects[ObjIndex];
		if (ObjectSupported(Obj))
			break;
	}

	//todo: reuse VisualizeObject() function
	// change visualizer
	CreateVisualizer(Obj);
#if HAS_MENU
	UpdateObjectMenu();
#endif
	return true;

	unguard;
}


void CUmodelApp::VisualizeObject(int newIndex)
{
	guard(CUmodelApp::VisualizeObject);

	ObjIndex = newIndex;
	CreateVisualizer(UObject::GObjObjects[newIndex]);
#if HAS_MENU
	UpdateObjectMenu();
#endif

	unguard;
}


void CUmodelApp::ReleaseViewerAndObjects()
{
	// destroy a viewer before releasing packages
	CSkelMeshViewer::UntagAllMeshes();
	delete Viewer;
	Viewer = NULL;

	ReleaseAllObjects();
}


#if HAS_UI

bool CUmodelApp::ShowStartupDialog(CStartupSettings& settings)
{
	GuiShown = true;
	UIStartupDialog dialog(settings);
	return dialog.Show();
}


static UIPackageDialog GPackageDialog;

static HWND GetSDLWindowHandle(SDL_Window* window)
{
	if (!window) return 0;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(window, &info);
	return info.info.win.window;
}

// This function will return 'false' when dialog has popped up and cancelled. If
// user performs some action, and then pop up the dialog again - the function will
// always return true.
bool CUmodelApp::ShowPackageUI()
{
	guard(CUmodelApp::ShowPackageUI);

	GuiShown = true;

	while (true)
	{
		UIPackageDialog::EResult mode = GPackageDialog.Show();

		if (mode == UIPackageDialog::CANCEL)
		{
			if (UObject::GObjObjects.Num() == 0)
			{
				// When we're doing export, then switching back to GUI, then pressing "Esc",
				// we can't return to the visualizer which was used before doing export because
				// all object was unloaded. In this case, code will set 'packagesChanged' flag
				// to true, causing re-initialization of browser list.
				FindObjectAndCreateVisualizer(1, true, true);
			}
			return false;
		}

		UIProgressDialog progress;
		progress.Show(mode == UIPackageDialog::EXPORT ? "Exporting packages" : "Loading packages");
		bool cancelled = false;

		progress.SetDescription("Scanning package");
		TStaticArray<UnPackage*, 256> Packages;
		for (int i = 0; i < GPackageDialog.SelectedPackages.Num(); i++)
		{
			FStaticString<MAX_PACKAGE_PATH> RelativeName;
			GPackageDialog.SelectedPackages[i]->GetRelativeName(RelativeName);
			if (!progress.Progress(*RelativeName, i, GPackageDialog.SelectedPackages.Num()))
			{
				cancelled = true;
				break;
			}
			UnPackage* package = UnPackage::LoadPackage(GPackageDialog.SelectedPackages[i]);	// should always return non-NULL
			if (package) Packages.Add(package);
		}
		if (cancelled)
		{
			progress.CloseDialog();
			continue;
		}

		if (!Packages.Num())
		{
			// This will happen only if all selected packages has failed to load (wrong package tag etc).
			// Show the package UI again.
			continue;
		}

		// register exporters and classes (will be performed only once); use any package
		// to detect an engine version
		InitClassAndExportSystems(Packages[0]->Game);

		// here we're in visualize mode

		// check whether we need to perform package unloading
		bool needReload = false;
		if (mode != UIPackageDialog::APPEND)
		{
			for (int i = 0; i < GFullyLoadedPackages.Num(); i++)
			{
				if (Packages.FindItem(GFullyLoadedPackages[i]) < 0)
				{
					// One of currently loaded packages is not needed anymore. We can't safely
					// unload only one package because it could be linked by other loaded packages.
					// So, unload everything.
					needReload = true;
					break;
				}
			}
		}

		if (needReload || mode == UIPackageDialog::EXPORT)
		{
			// destroy a viewer before releasing packages
			ReleaseViewerAndObjects();
		}

		if (mode == UIPackageDialog::EXPORT)
		{
			progress.SetDescription("Exporting package");
			ExportPackages(Packages, &progress);
			progress.CloseDialog();
			continue;		// after export, show the dialog again
		}

		// fully load all selected packages
		progress.SetDescription("Loading package");
//		UObject::BeginLoad(); - added 27.20.2018 (#dfa0abf), disabled 18.09.2019 - causes UI to freeze when loading objects (LoadWholePackage already does Begin/EndLoad)
		for (int i = 0; i < Packages.Num(); i++)
		{
			UnPackage* package = Packages[i];
			if (!progress.Progress(package->Name, i, Packages.Num()))
			{
				cancelled = true;
				break;
			}
			if (!LoadWholePackage(package, &progress))
			{
				cancelled = true;
				break;
			}
		}
//		UObject::EndLoad();

		if (cancelled)
		{
			//!! message box
			appPrintf("Operation interrupted by user.\n");
		}

		progress.CloseDialog();

		// Viewer was released if we're releasing package which is currently used for viewing.
		if (!Viewer)
		{
			FindObjectAndCreateVisualizer(1, true, true);
		}
		break;
	}

	return true;

	unguard;
}

void CUmodelApp::SetPackage(UnPackage* package)
{
	GPackageDialog.SelectPackage(package);
}

void CUmodelApp::ShowOptionsDialog()
{
	UISettingsDialog dialog(GSettings);
	dialog.Show();
}

void CUmodelApp::ShowErrorDialog()
{
	UIErrorDialog errorDialog;
	errorDialog.Show();
}

#if UNREAL4
int CUmodelApp::ShowUE4UnversionedPackageDialog(int verMin, int verMax)
{
	UIUE4VersionDialog dialog;
	return dialog.Show(verMin, verMax);
}

FString CUmodelApp::ShowUE4AesKeyDialog()
{
	UIUE4AesKeyDialog dialog;
	return dialog.Show();
}

#endif // UNREAL4

#endif // HAS_UI

#endif // RENDERING


/*-----------------------------------------------------------------------------
	GlWindow callbacks
-----------------------------------------------------------------------------*/

#if RENDERING

bool CUmodelApp::CreateVisualizer(UObject *Obj, bool test)
{
	guard(CreateVisualizer);

	if (!test && Viewer)
	{
		if (Viewer->Object == Obj) return true;	// object is not changed
		delete Viewer;
		Viewer = NULL;
	}

	if (!Obj)
	{
		// dummy visualizer
		Viewer = new CObjectViewer(NULL, this);
		return true;
	}

	if (!test)
		appSetNotifyHeader("%s:  %s'%s'", *Obj->Package->GetFilename(), Obj->GetClassName(), Obj->Name);
	// create viewer class
#define CLASS_VIEWER(UClass, CViewer, extraCheck)	\
	if (Obj->IsA(#UClass + 1))						\
	{												\
		UClass *Obj2 = static_cast<UClass*>(Obj); 	\
		if (!(extraCheck)) return false;			\
		if (!test) Viewer = new CViewer(Obj2, this);\
		return true;								\
	}
#define MESH_VIEWER(UClass, CViewer)				\
	if (Obj->IsA(#UClass + 1))						\
	{												\
		if (!test)									\
		{											\
			UClass *Obj2 = static_cast<UClass*>(Obj); \
			if (!Obj2->ConvertedMesh)				\
				Viewer = new CObjectViewer(Obj, this); \
			else									\
				Viewer = new CViewer(Obj2->ConvertedMesh, this); \
		}											\
		return true;								\
	}
	// create viewer for known class
	// note: when 'test' is not set, we'll create a visualizer in any case
	bool showAll = !(ShowMeshes || ShowMaterials) || !test;
	if (ShowMeshes || showAll)
	{
		CLASS_VIEWER(UVertMesh,       CVertMeshViewer, true);
		MESH_VIEWER (USkeletalMesh,   CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh,     CStatMeshViewer      );
#if UNREAL3
		MESH_VIEWER (USkeletalMesh3,  CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh3,    CStatMeshViewer      );
#endif
#if UNREAL4
		MESH_VIEWER (USkeletalMesh4,  CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh4,    CStatMeshViewer      );
#endif
	}
	if (ShowMaterials || showAll)
	{
		CLASS_VIEWER(UUnrealMaterial, CMaterialViewer, !ShowMaterials || !Obj2->IsTexture());
	}
	// fallback for unknown class
	if (!test)
	{
		Viewer = new CObjectViewer(Obj, this);
	}
	return false;
#undef CLASS_VIEWER
	unguardf("%s'%s'", Obj->GetClassName(), Obj->Name);
}


static void TakeScreenshot(const char *ObjectName, bool CatchAlpha)
{
	char filename[256];
	appSprintf(ARRAY_ARG(filename), SCREENSHOTS_DIR "/%s.tga", ObjectName);
	int retry = 1;
	while (appFileExists(filename))
	{
		// if file exists, append an index
		appSprintf(ARRAY_ARG(filename), SCREENSHOTS_DIR "/%s_%02d.tga", ObjectName, ++retry);
	}
	appPrintf("Writing screenshot %s\n", filename);
	appMakeDirectoryForFile(filename);
	FFileWriter Ar(filename);
	int width, height;
	GApplication.GetWindowSize(width, height);

	byte *pic = new byte [width * height * 4];
	glFinish();
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pic);

	if (CatchAlpha)
	{
		/*
		NOTES:
		- this will work in GL1 mode only, GL2 has depth buffer somewhere in CFramebuffer;
		  we are copying depth to the main framebuffer in bloom shader
		- rendering using black background for better semi-transparency
		- processing semi-transparency with alpha channel in framebuffer will not work because
		  some parts of image could have different blending modes (blend, add etc)
		- some translucent parts could be painted with no depthwrite, this will produce black
		  areas! (example: bloom has no depthwrite, it is screen-space effect)
		*/
		float *picDepth = new float [width * height];
		glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, picDepth);
		for (int i = 0; i < width * height; i++)
		{
			float v = picDepth[i];
			pic[i * 4 + 3] = (v == 1) ? 0 : 255;
		}
		delete picDepth;
	}

	WriteTGA(Ar, width, height, pic);
	delete pic;
}


CUmodelApp::CUmodelApp()
:	GuiShown(false)
#if RENDERING
,	Viewer(NULL)
,	DoScreenshot(0)
,	ShowMeshes(false)
,	ShowMaterials(false)
,	ObjIndex(0)
#endif
#if HAS_MENU
,	MainMenu(NULL)
,	ObjectMenu(NULL)
#endif
{
#if HAS_UI
	UIBaseDialog::SetGlobalIconResId(IDC_MAIN_ICON);
#endif
}

CUmodelApp::~CUmodelApp()
{
#if RENDERING
	if (Viewer) delete Viewer;
#endif
}

void CUmodelApp::Draw3D(float TimeDelta)
{
	UObject *Obj = (UObject::GObjObjects.IsValidIndex(ObjIndex)) ? UObject::GObjObjects[ObjIndex] : NULL;

	guard(CUmodelApp::Draw3D);

	bool AlphaBgShot = DoScreenshot >= 2;
	if (AlphaBgShot)
	{
		// screenshot with transparent background
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// draw the frame
	Viewer->Draw3D(TimeDelta);

	if (AlphaBgShot)
	{
		// take screenshot without 2D texts and with transparency
		TakeScreenshot(Obj->Name, true);
		DoScreenshot = 0;
	}

	unguardf("Obj=%s'%s'", Obj ? Obj->GetClassName() : "None", Obj ? Obj->Name : "None");
}


void CUmodelApp::BeforeSwap()
{
	guard(CUmodelApp::BeforeSwap);

	if (DoScreenshot)
	{
		// take regular screenshot
		UObject *Obj = UObject::GObjObjects[ObjIndex];
		TakeScreenshot(Obj->Name, false);
		DoScreenshot = 0;
	}

	if (Viewer->JumpAfterFrame)
	{
		// note: without "const_cast" FindItem won't compile
		int NewObjectIndex = UObject::GObjObjects.FindItem(const_cast<UObject*>(Viewer->JumpAfterFrame));
		Viewer->JumpAfterFrame = NULL;
		if (NewObjectIndex >= 0)
		{
			// Remember current object
			BrowseHistory.RemoveAt(CurrentHistoryItem, BrowseHistory.Num() - CurrentHistoryItem);
			BrowseHistory.Add(ObjIndex);
			CurrentHistoryItem = BrowseHistory.Num();
			// Switch to object
			VisualizeObject(NewObjectIndex);
		}
	}

	unguard;
}


void CUmodelApp::GoBack()
{
	// Remember current item
	if (CurrentHistoryItem == BrowseHistory.Num())
		BrowseHistory.Add(ObjIndex);

	if (CurrentHistoryItem > 0)
	{
		VisualizeObject(BrowseHistory[--CurrentHistoryItem]);
	}
}


void CUmodelApp::GoForward()
{
	if (CurrentHistoryItem < BrowseHistory.Num() - 1)
	{
		VisualizeObject(BrowseHistory[++CurrentHistoryItem]);
	}
}


void CUmodelApp::ProcessKey(unsigned key, bool isDown)
{
	guard(CUmodelApp::ProcessKey);

	CApplication::ProcessKey(key, isDown);

	if (!isDown)
	{
		Viewer->ProcessKeyUp(key);
		return;
	}

	switch (key)
	{
	case SPEC_KEY(PAGEUP):
	case SDLK_KP_9:
		FindObjectAndCreateVisualizer(-1);
		break;
	case SPEC_KEY(PAGEDOWN):
	case SDLK_KP_3:
		FindObjectAndCreateVisualizer(1);
		break;
	case 's'|KEY_CTRL:
		DoScreenshot = 1;
		break;
	case 's'|KEY_ALT:
		DoScreenshot = 2;
		break;
	case SPEC_KEY(LEFT)|KEY_ALT:
		GoBack();
		break;
	case SPEC_KEY(RIGHT)|KEY_ALT:
		GoForward();
		break;
	case 'x'|KEY_CTRL:
#if HAS_UI
		if (Viewer && UISettingsDialog::ShowExportOptions(GSettings))
#else
		if (Viewer)
#endif
		{
			BeginExport();
			Viewer->Export();
			EndExport(true);
		}
		break;
#if HAS_UI
	case 'o':
		ShowPackageUI();
		break;
	case 'o'|KEY_CTRL:
		ShowOptionsDialog();
		break;
#endif // HAS_UI
	}

	Viewer->ProcessKey(key);

#if HAS_MENU
	// Processing keys most likely would change some settings. So we should update menu
	// in order to reflect these changes. Note: hooking WM_INITMENU in WndProc
	// doesn't help because WM_INITMENU dispatched to WndProc only when menu closed.
	MainMenu->Update();
#endif

	unguardf("key=%X, down=%d", key, isDown);
}


void CUmodelApp::DrawTexts()
{
#if 0
	DrawTextBottomRight("History: [%d] <- %d", BrowseHistory.Num(), CurrentHistoryItem);
	for (int i = 0; i < BrowseHistory.Num(); i++)
	{
		DrawTextBottomRight("[%d] = %s", i, UObject::GObjObjects[BrowseHistory[i]]->Name);
	}
#endif

	guard(CUmodelApp::DrawTexts);
	CApplication::DrawTexts();
	if (IsHelpVisible)
	{
		DrawKeyHelp("PgUp/PgDn", "browse objects");
#if HAS_UI
		DrawKeyHelp("O",         "open package");
		DrawKeyHelp("Ctrl+O",    "show options");
#endif
		DrawKeyHelp("Ctrl+X",	 "export object");
		DrawKeyHelp("Ctrl+S",    "take screenshot");
		DrawKeyHelp("Alt+Left/Right", "navigate history");
		Viewer->ShowHelp();
		DrawTextLeft("-----\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}

#if MAX_DEBUG

static void DumpMemory()
{
	appPrintf("Memory: allocated " FORMAT_SIZE("d") " bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
	appDumpMemoryAllocations();
}

#endif // MAX_DEBUG

void CUmodelApp::WindowCreated()
{
#if HAS_UI
	HWND wnd = GetSDLWindowHandle(GetWindow());
	// set window icon
	SendMessage(wnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_MAIN_ICON)));
	UIBaseDialog::SetMainWindow(wnd);
	#if HAS_MENU
	CreateMenu();
	#endif
#endif // HAS_UI
}

#if HAS_MENU

void CUmodelApp::CreateMenu()
{
	MainMenu = new UIMenu();
	(*MainMenu)
	[
		NewSubmenu("File")
		[
			NewMenuItem("Open package ...\tO")
			.SetCallback(BIND_LAMBDA([this] () { ShowPackageUI(); })) // binding of bool() to void(), so use lambda here
			+ NewMenuSeparator()
			+ NewMenuItem("Exit\tEsc")
			.SetCallback(BIND_MEMBER(&CUmodelApp::Exit, this))
		]
		+ NewSubmenu("View")
		[
			NewMenuItem("Screenshot\tCtrl+S")
			.SetCallback(BIND_LAMBDA([this]() { DoScreenshot = 1; }))
			+ NewMenuItem("Screenshot with alpha\tAlt+S")
			.SetCallback(BIND_LAMBDA([this]() { DoScreenshot = 2; }))
			+ NewMenuSeparator()
			+ NewMenuCheckbox("Show debug information\tCtrl+Q", &GShowDebugInfo)
		]
		+ NewSubmenu("Navigate")
		[
			NewMenuCheckbox("Include meshes", &ShowMeshes)
			+ NewMenuCheckbox("Include materials", &ShowMaterials)
			+ NewMenuSeparator()
			+ NewMenuItem("Previous\tPgUp")
			.SetCallback(BIND_LAMBDA([this]() { FindObjectAndCreateVisualizer(-1); }))
			+ NewMenuItem("Next\tPgDn")
			.SetCallback(BIND_LAMBDA([this]() { FindObjectAndCreateVisualizer(1); }))
			+ NewMenuSeparator()
			+ NewMenuItem("History back\tAlt+Left")
			.SetCallback(BIND_MEMBER(&CUmodelApp::GoBack, this))
			+ NewMenuItem("History forward\tAlt+Right")
			.SetCallback(BIND_MEMBER(&CUmodelApp::GoForward, this))
		]
		+ NewSubmenu("Object")
		.Enable(false)
		.Expose(ObjectMenu)
		+ NewSubmenu("Tools")
		[
			NewMenuItem("Export current object\tCtrl+X")
			.SetCallback(BIND_LAMBDA([this]()
				{
					if (Viewer && UISettingsDialog::ShowExportOptions(GSettings))
					{
						BeginExport();
						Viewer->Export();
						EndExport(true);
					}
				}))
			+ NewMenuSeparator()
			+ NewMenuHyperLink("Open export folder", *GSettings.Export.ExportPath)	//!! should update if directory will be changed from UI
			+ NewMenuHyperLink("Open screenshots folder", SCREENSHOTS_DIR)
			+ NewMenuSeparator()
			+ NewMenuItem("Scan package versions")
			.SetCallback(BIND_STATIC(&ShowPackageScanDialog))
			+ NewMenuSeparator()
			+ NewMenuItem("Options\tCtrl+O")
			.SetCallback(BIND_MEMBER(&CUmodelApp::ShowOptionsDialog, this))
		]
#if MAX_DEBUG
		+ NewSubmenu("Debug")
		[
			NewMenuItem("Dump memory")
			.SetCallback(BIND_STATIC(&DumpMemory))
			+ NewMenuCheckbox("Show shader parameters", &GShowShaderParams)
		]
#endif
		+ NewSubmenu("Help")
		[
			NewMenuCheckbox("Keyboard shortcuts\tH", &IsHelpVisible)
			+ NewMenuHyperLink("View readme", "readme.txt")	//?? add directory here
			+ NewMenuSeparator()
			+ NewMenuHyperLink("Tutorial videos", "https://www.youtube.com/playlist?list=PLJROJrENPVvK-V8PCTR9qBmY0Q7v4wCym")
			+ NewMenuSeparator()
			+ NewMenuHyperLink("UE Viewer website", GUmodelHomepage)
			+ NewMenuHyperLink("UE Viewer FAQ", "https://www.gildor.org/projects/umodel/faq")
			+ NewMenuHyperLink("Compatibility information", "https://www.gildor.org/projects/umodel/compat")
			+ NewMenuHyperLink("UE Viewer forums", "https://www.gildor.org/smf/")
			+ NewMenuHyperLink("Donate", "https://www.gildor.org/en/donate")
			+ NewMenuSeparator()
			+ NewMenuItem("About UModel")
			.SetCallback(BIND_STATIC(&UIAboutDialog::Show))
		]
	];

	// attach menu to the SDL window
	HWND wnd = GetSDLWindowHandle(GetWindow());
	MainMenu->AttachTo(wnd);
	// menu has been attached, resize the window
	ResizeWindow();

	UpdateObjectMenu();
}

void CUmodelApp::UpdateObjectMenu()
{
	guard(CUmodelApp::UpdateObjectMenu);
	if (!MainMenu || !Viewer)
	{
		// Window wasn't created yet, UpdateObjectMenu() will be called explicitly later
		return;
	}
	UIMenuItem* newObjMenu = Viewer->GetObjectMenu(NULL);
	if (!newObjMenu)
	{
		// We should replace old menu with a dummy one - we can't keep old menu because it may have
		// references to data inside objects which are no longer exists
		newObjMenu = &NewSubmenu("Object");
		ObjectMenu->Enable(false);
	}
	else
	{
		ObjectMenu->Enable(true);
	}
	ObjectMenu->ReplaceWith(newObjMenu);
	unguard;
}

void CUmodelApp::WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	guard(CUmodelApp::WndProc);
	if (msg == WM_COMMAND)
		MainMenu->HandleCommand(LOWORD(wParam));
//	if (msg == WM_INITMENU) MainMenu->Update(); -- this doesn't work, because this message dispatched to this WndProc only when menu closed
	unguard;
}

#endif // HAS_MENU

#endif // RENDERING
