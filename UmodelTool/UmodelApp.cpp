#include <SDL2/SDL_syswm.h>			// for SDL_SysWMinfo
#undef UnregisterClass

#include "Core.h"
#include "UnCore.h"
#include "UnrealClasses.h"
#include "PackageUtils.h"
#include "UnPackage.h"

#include "UnMesh2.h"
#include "UnMesh3.h"

#include "UmodelApp.h"

#include "Exporters/Exporters.h"	// for WriteTGA

#if HAS_UI
#include "BaseDialog.h"
#include "PackageDialog.h"
#include "StartupDialog.h"
#include "ProgressDialog.h"
#include "AboutDialog.h"
#include "ErrorDialog.h"
#include "PackageScanDialog.h"
#endif


CUmodelApp GApplication;

/*-----------------------------------------------------------------------------
	Object visualizer support
-----------------------------------------------------------------------------*/

#if RENDERING

bool CUmodelApp::FindObjectAndCreateVisualizer(int dir, bool forceVisualizer, bool newPackage)
{
	if (newPackage)
	{
		assert(dir > 0); // just in case
		ObjIndex = -1;
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
				appPrintf("\nThe specified package(s) has no supported objects.\n\n");
				DisplayPackageStats(GFullyLoadedPackages);
				return true;
			}
			return false;
		}
		Obj = UObject::GObjObjects[ObjIndex];
		if (ObjectSupported(Obj))
			break;
	}
	// change visualizer
	CreateVisualizer(Obj);
	return true;
}


#if HAS_UI

bool CUmodelApp::ShowStartupDialog(UmodelSettings& settings)
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
	// When we're doing export, then switching back to GUI, then pressing "Esc",
	// we can't return to the visualizer which was used before doing export because
	// all object was unloaded. In this case, code will set 'packagesChanged' flag
	// to true, causing re-initialization of browser list.
	bool packagesChanged = false;
	GuiShown = true;

	while (true)
	{
		UIPackageDialog::EResult mode = GPackageDialog.Show();
		if (mode == UIPackageDialog::CANCEL)
		{
			if (packagesChanged)
				FindObjectAndCreateVisualizer(1, true, true);
			return false;
		}

		UIProgressDialog progress;
		progress.Show(mode == UIPackageDialog::EXPORT ? "Exporting packages" : "Loading packages");
		bool cancelled = false;

		progress.SetDescription("Scanning package");
		TStaticArray<UnPackage*, 256> Packages;
		for (int i = 0; i < GPackageDialog.SelectedPackages.Num(); i++)
		{
			const char* pkgName = *GPackageDialog.SelectedPackages[i];
			if (!progress.Progress(pkgName, i, GPackageDialog.SelectedPackages.Num()))
			{
				cancelled = true;
				break;
			}
			UnPackage* package = UnPackage::LoadPackage(pkgName);	// should always return non-NULL
			if (package) Packages.AddItem(package);
		}
		if (cancelled)
		{
			progress.CloseDialog();
			continue;
		}

		if (!Packages.Num()) break;			// should not happen

		// register exporters and classes (will be performed only once); use any package
		// to detect an engine version
		InitClassAndExportSystems(Packages[0]->Game);

		// here we're in visualize mode

		// check whether we need to perform package unloading
		bool needReload = false;
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

		if (needReload || mode == UIPackageDialog::EXPORT)
		{
			// destroy a viewer before releasing packages
			CSkelMeshViewer::UntagAllMeshes();
			delete Viewer;
			Viewer = NULL;

			packagesChanged = true;
			ReleaseAllObjects();
		}

		if (mode == UIPackageDialog::EXPORT)
		{
#if PROFILE
//			appResetProfiler(); -- there's nested appResetProfiler/appPrintProfiler calls, which are not supported
#endif
			progress.SetDescription("Exporting package");
			// for each package: load a package, export, then release
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
				if (!ExportObjects(NULL, &progress))
				{
					cancelled = true;
					break;
				}
				ReleaseAllObjects();
			}
			// cleanup
			ResetExportedList();
#if PROFILE
//			appPrintProfiler();
#endif
			if (cancelled)
			{
				ReleaseAllObjects();
				//!! message box
				appPrintf("Operation interrupted by user.\n");
			}
			progress.CloseDialog();
			continue;		// after export, show the dialog again
		}

		// fully load all selected packages
		progress.SetDescription("Loading package");
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

		if (cancelled)
		{
			//!! message box
			appPrintf("Operation interrupted by user.\n");
		}

		progress.CloseDialog();

		if (packagesChanged || !Viewer)
		{
			FindObjectAndCreateVisualizer(1, true, true);
			packagesChanged = false;
		}
		break;
	}

	return true;
}

void CUmodelApp::SetPackageName(const char* name)
{
	GPackageDialog.SelectPackage(name);
}

void CUmodelApp::ShowAboutDialog()
{
	UIAboutDialog dialog;
	dialog.Show();
}

void CUmodelApp::ShowErrorDialog()
{
	UIErrorDialog errorDialog;
	errorDialog.Show();
}

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
		appSetNotifyHeader("%s:  %s'%s'", Obj->Package->Filename, Obj->GetClassName(), Obj->Name);
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
	bool showAll = !(ShowMeshes || ShowMaterials);
	if (ShowMeshes || showAll)
	{
		CLASS_VIEWER(UVertMesh,       CVertMeshViewer, true);
		MESH_VIEWER (USkeletalMesh,   CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh,     CStatMeshViewer      );
#if UNREAL3
		MESH_VIEWER (USkeletalMesh3,  CSkelMeshViewer      );
		MESH_VIEWER (UStaticMesh3,    CStatMeshViewer      );
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
	appSprintf(ARRAY_ARG(filename), "Screenshots/%s.tga", ObjectName);
	int retry = 1;
	while (true)
	{
		FILE *f = fopen(filename, "r");
		if (!f) break;
		fclose(f);
		// if file exists, append index
		retry++;
		appSprintf(ARRAY_ARG(filename), "Screenshots/%s_%02d.tga", ObjectName, retry);
	}
	appPrintf("Writting screenshot %s\n", filename);
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


static int GDoScreenshot = 0;

CUmodelApp::CUmodelApp()
:	GuiShown(false)
#if RENDERING
,	Viewer(NULL)
,	ShowMeshes(false)
,	ShowMaterials(false)
,	ObjIndex(0)
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
	UObject *Obj = (ObjIndex < UObject::GObjObjects.Num()) ? UObject::GObjObjects[ObjIndex] : NULL;

	guard(CUmodelApp::Draw3D);

	bool AlphaBgShot = GDoScreenshot >= 2;
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
		GDoScreenshot = 0;
	}

	unguardf("Obj=%s'%s'", Obj ? Obj->GetClassName() : "None", Obj ? Obj->Name : "None");
}


void CUmodelApp::BeforeSwap()
{
	guard(CUmodelApp::BeforeSwap);

	if (GDoScreenshot)
	{
		// take regular screenshot
		UObject *Obj = UObject::GObjObjects[ObjIndex];
		TakeScreenshot(Obj->Name, false);
		GDoScreenshot = 0;
	}

	unguard;
}


void CUmodelApp::ProcessKey(int key, bool isDown)
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
	case SPEC_KEY(PAGEDOWN):
	case SPEC_KEY(PAGEUP):
		FindObjectAndCreateVisualizer((key == SPEC_KEY(PAGEDOWN)) ? 1 : -1);
		break;
	case 's'|KEY_CTRL:
		GDoScreenshot = 1;
		break;
	case 's'|KEY_ALT:
		GDoScreenshot = 2;
		break;
#if HAS_UI
	case 'o':
		ShowPackageUI();
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

	unguard;
}


void CUmodelApp::DrawTexts()
{
	guard(CUmodelApp::DrawTexts);
	CApplication::DrawTexts();
	if (IsHelpVisible)
	{
		DrawKeyHelp("PgUp/PgDn", "browse objects");
#if HAS_UI
		DrawKeyHelp("O",         "open package");
#endif
		DrawKeyHelp("Ctrl+S",    "take screenshot");
		Viewer->ShowHelp();
		DrawTextLeft("-----\n");		// divider
	}
	Viewer->Draw2D();
	unguard;
}

#if MAX_DEBUG

static void DumpMemory()
{
	appPrintf("Memory: allocated %d bytes in %d blocks\n", GTotalAllocationSize, GTotalAllocationCount);
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
#endif // HAS_UI

#if HAS_MENU
	MainMenu = new UIMenu();
	(*MainMenu)
	[
		NewSubmenu("File")
		[
			NewMenuItem("Open package ...\tO")
			.SetCallback(BIND_MEM_CB(&CUmodelApp::ShowPackageUI, this))
			+ NewMenuSeparator()
			+ NewMenuItem("Exit\tEsc")
			.SetCallback(BIND_MEM_CB(&CUmodelApp::Exit, this))
		]
		+ NewSubmenu("View")
		[
			NewMenuCheckbox("Include meshes", &ShowMeshes)
			+ NewMenuCheckbox("Include materials", &ShowMaterials)
			+ NewMenuSeparator()
			+ NewMenuCheckbox("Show debug information\tCtrl+Q", &GShowDebugInfo)
		]
		+ NewSubmenu("Tools")
		[
			NewMenuItem("Scan package versions")
			.SetCallback(BIND_FREE_CB(&ShowPackageScanDialog))
		]
#if MAX_DEBUG
		+ NewSubmenu("Debug")
		[
			NewMenuItem("Dump memory")
			.SetCallback(BIND_FREE_CB(&DumpMemory))
		]
#endif
		+ NewSubmenu("Help")
		[
			NewMenuCheckbox("Keyboard shortcuts\tH", &IsHelpVisible)
			+ NewMenuHyperLink("View readme", "readme.txt")	//?? add directory here
			+ NewMenuSeparator()
			+ NewMenuHyperLink("UModel website", GUmodelHomepage)
			+ NewMenuHyperLink("UModel FAQ", "http://www.gildor.org/projects/umodel/faq")
			+ NewMenuHyperLink("Compatibility information", "http://www.gildor.org/projects/umodel/compat")
			+ NewMenuHyperLink("UModel forums", "http://www.gildor.org/smf/")
			+ NewMenuHyperLink("Donate", "http://www.gildor.org/en/donate")
			+ NewMenuSeparator()
			+ NewMenuItem("About UModel")
			.SetCallback(BIND_MEM_CB(&CUmodelApp::ShowAboutDialog, this))
		]
	];
	// attach menu to the SDL window
	SetMenu(wnd, MainMenu->GetHandle(true));
	// menu has been attached, resize the window
	ResizeWindow();
#endif // HAS_MENU
}

#if HAS_MENU
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
