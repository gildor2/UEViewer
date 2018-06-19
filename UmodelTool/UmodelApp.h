#ifndef __UMODEL_APP_H__
#define __UMODEL_APP_H__

#include "Viewers/ObjectViewer.h"
#include "UmodelSettings.h"

class UIMenu;
class UIMenuItem;

//?? remove this define - we have 3 defines here!
#if _WIN32 && HAS_UI && RENDERING
#define HAS_MENU		1
#else
#define HAS_MENU		0
#endif

//!! TODO: verify: perhaps RENDERING=0 would keep some SDL stuff now (too much things has been changed)
class CUmodelApp : public CApplication
{
public:
	CUmodelApp();
	virtual ~CUmodelApp();

	virtual void WindowCreated();
	virtual void Draw3D(float TimeDelta);
	virtual void DrawTexts();
	virtual void BeforeSwap();
	virtual void ProcessKey(int key, bool isDown);

#if RENDERING
	// Create visualizer for object. If 'test' is true, then visualizer will not be created, but
	// possibility of creation will be validated, and result of the function will be 'true' if
	// object can be viewed, or 'false' otherwise.
	bool CreateVisualizer(UObject *Obj, bool test = false);
	// dir = 1 - forward direction for search, dir = -1 - backward.
	// When forceVisualizer is true, dummy visualizer will be created if no supported object found
	bool FindObjectAndCreateVisualizer(int dir, bool forceVisualizer = false, bool newPackage = false);

	FORCEINLINE bool ObjectSupported(UObject *Obj)
	{
		return CreateVisualizer(Obj, true);
	}
#else // RENDERING
	// pure command line tool
	FORCEINLINE bool ObjectSupported(UObject *Obj)
	{
		return true;
	}
#endif // RENDERING

#if HAS_UI
	bool ShowStartupDialog(CStartupSettings& settings);
	bool ShowPackageUI();
	void SetPackage(UnPackage* package);
	void ShowErrorDialog();
	#if UNREAL4
	int ShowUE4UnversionedPackageDialog(int verMin, int verMax);
	FString ShowUE4AesKeyDialog();
	#endif
#endif // HAS_UI

protected:
#if HAS_MENU
	UIMenu*		MainMenu;
	UIMenuItem*	ObjectMenu;
	void CreateMenu();
	void UpdateObjectMenu();
	virtual void WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
#endif

	int			ObjIndex;			// index of the current object in UObject::GObjObjects array
	int			DoScreenshot;

public:
	bool		GuiShown;
#if RENDERING
	CObjectViewer *Viewer;			// used from GlWindow callbacks
	bool		ShowMeshes;
	bool		ShowMaterials;
#endif
};

extern CUmodelApp GApplication;

// Main.cpp functions
void InitClassAndExportSystems(int Game);
bool ExportObjects(const TArray<UObject*> *Objects = NULL, IProgressCallback* progress = NULL);
void DisplayPackageStats(const TArray<UnPackage*> &Packages);


#endif // __UMODEL_APP_H__
